// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 Koen De Vleeschauwer

// u8g2 module: u8g2 displays on SPI or I2C. monochrome displays, and color lcds
// with a 1bpp framebuffer expanded through a color lookup table.
// wiring and display type at runtime:
//   u8g2.init(spi, cs, dc, rst=None, bl=None, display=u8g2.ST7789_240X280)
//   u8g2.init(i2c=i2c, display=u8g2.SSD1306_I2C_128X64_NONAME)
// the displays in the firmware: tools/displays/displays.txt.
// framebuffer and tile clut map: allocated by init(), sized for the display.

#include <string.h>
#include "py/runtime.h"
#include "py/builtin.h"
#include "py/stream.h"
#include "py/mphal.h"
#include "extmod/modmachine.h"
#include "modu8g2.h"
#include "u8x8_clut_driver.h"

// backlight pwm
#ifndef MICROPY_HW_U8G2_BL_FREQ
#define MICROPY_HW_U8G2_BL_FREQ (1000)
#endif
#ifndef MICROPY_HW_U8G2_BL_INVERT
#define MICROPY_HW_U8G2_BL_INVERT (0)
#endif
#define U8G2_BL_DEFAULT (128)

// font after init()
#ifndef U8G2_MP_DEFAULT_FONT
#define U8G2_MP_DEFAULT_FONT u8g2_font_profont12_tf
#endif

u8g2_t mp_u8g2;

// one heap block: framebuffer (8 bytes per tile), then, for color displays, the
// tile clut map (two 4-bit clut indices per byte). allocated by init(), freed by soft reset.
MP_REGISTER_ROOT_POINTER(uint8_t *u8g2_mem);

static uint8_t *u8g2_clut_map;

extern const mp_obj_module_t u8g2_user_cmodule;

// set by init(). spi, pin and pwm objects are freed by a soft reset, but this
// flag survives it. a soft reset starts with an empty sys.modules, so init()
// also puts u8g2 in sys.modules, and ready means: flag set and u8g2 still there.
static bool u8g2_ready;

bool u8g2_is_ready(void) {
    if (!u8g2_ready) {
        return false;
    }
    mp_map_elem_t *elem = mp_map_lookup(&MP_STATE_VM(mp_loaded_modules_dict).map, MP_OBJ_NEW_QSTR(MP_QSTR_u8g2), MP_MAP_LOOKUP);
    return elem != NULL && elem->value == MP_OBJ_FROM_PTR(&u8g2_user_cmodule);
}

static void u8g2_check_ready(void) {
    if (!u8g2_is_ready()) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("call u8g2.init() first"));
    }
}

/******************************************************************************/
// displays: generated from tools/displays/displays.txt by tools/displays/Makefile,
// with "codebuild -t". D(name, display_cb, cad_cb, ll_hvline, tile_width, tile_height, com):
// the python name is the u8g2 setup name in upper case: u8g2_Setup_ssd1306_i2c_128x64_noname_f()
// is u8g2.SSD1306_I2C_128X64_NONAME. com is the codebuild interface mask.

// interfaces in the com mask: the COM_xxx values of tools/codebuild/codebuild.c
#define U8G2_MP_COM_4WSPI     (0x0001)
#define U8G2_MP_COM_I2C       (0x0010)
#define U8G2_MP_COM_ST7920SPI (0x0020) // 4-wire spi without dc

// bus of a display, display_info()
#define U8G2_MP_BUS_NONE      (0) // parallel or 3-wire spi only: not supported
#define U8G2_MP_BUS_SPI       (1) // 4-wire spi: sck, mosi, cs, dc (dc not for sharp memory lcds)
#define U8G2_MP_BUS_SPI_NO_DC (2) // spi without dc: st7920, max7219
#define U8G2_MP_BUS_I2C       (3)

typedef struct _u8g2_mp_display_t {
    u8x8_msg_cb display_cb;
    u8x8_msg_cb cad_cb;
    u8g2_draw_ll_hvline_cb ll_hvline;
    uint8_t tile_width;
    uint8_t tile_height;
    uint16_t com;
} u8g2_mp_display_t;

enum {
    #define D(name, display_cb, cad_cb, ll_hvline, tile_width, tile_height, com) DISPLAY_##name,
    #include "u8g2_displays_table.h"
    #undef D
    DISPLAY_COUNT
};

static const u8g2_mp_display_t u8g2_displays[DISPLAY_COUNT] = {
    #define D(name, display_cb, cad_cb, ll_hvline, tile_width, tile_height, com) \
    { display_cb, cad_cb, ll_hvline, tile_width, tile_height, com },
    #include "u8g2_displays_table.h"
    #undef D
};

// set by init(): the display has a color lookup table
static bool u8g2_is_color;

static uint8_t u8g2_display_bus(const u8g2_mp_display_t *disp) {
    if (disp->com & U8G2_MP_COM_I2C) {
        return U8G2_MP_BUS_I2C;
    }
    if (disp->com & U8G2_MP_COM_ST7920SPI) {
        return U8G2_MP_BUS_SPI_NO_DC;
    }
    if (disp->com & U8G2_MP_COM_4WSPI) {
        // no command/data distinction (max7219): dc is not used
        return disp->cad_cb == u8x8_cad_empty ? U8G2_MP_BUS_SPI_NO_DC : U8G2_MP_BUS_SPI;
    }
    return U8G2_MP_BUS_NONE;
}

static const u8g2_mp_display_t *u8g2_display_get(mp_obj_t display_in) {
    mp_int_t display = mp_obj_get_int(display_in);
    if (display < 0 || display >= DISPLAY_COUNT) {
        mp_raise_ValueError(MP_ERROR_TEXT("unknown display"));
    }
    return &u8g2_displays[display];
}

/******************************************************************************/
// backlight: a machine.PWM object, so the pwm channel comes from machine.PWM's
// own allocator. soft reset deinits all PWM; init() creates a new one.

MP_REGISTER_ROOT_POINTER(mp_obj_t u8g2_bl_pwm);

static uint8_t u8g2_bl_level;
static bool u8g2_power_save;

static void u8g2_bl_apply(void) {
    if (MP_STATE_VM(u8g2_bl_pwm) == MP_OBJ_NULL) {
        return;
    }
    mp_obj_t dest[3];
    mp_load_method(MP_STATE_VM(u8g2_bl_pwm), MP_QSTR_duty_u16, dest);
    dest[2] = MP_OBJ_NEW_SMALL_INT(u8g2_power_save ? 0 : u8g2_bl_level * 257);
    mp_call_method_n_kw(1, 0, dest);
}

static void u8g2_bl_init(mp_obj_t pin) {
    MP_STATE_VM(u8g2_bl_pwm) = MP_OBJ_NULL;
    u8g2_bl_level = U8G2_BL_DEFAULT;
    if (pin == mp_const_none) {
        return;
    }
    #if MICROPY_PY_MACHINE_PWM
    mp_obj_t args[] = {
        pin,
        MP_OBJ_NEW_QSTR(MP_QSTR_freq), MP_OBJ_NEW_SMALL_INT(MICROPY_HW_U8G2_BL_FREQ),
        MP_OBJ_NEW_QSTR(MP_QSTR_duty_u16), MP_OBJ_NEW_SMALL_INT(U8G2_BL_DEFAULT * 257),
        MP_OBJ_NEW_QSTR(MP_QSTR_invert), MP_OBJ_NEW_SMALL_INT(1),
    };
    size_t n_kw = MICROPY_HW_U8G2_BL_INVERT ? 3 : 2;
    MP_STATE_VM(u8g2_bl_pwm) = mp_call_function_n_kw(MP_OBJ_FROM_PTR(&machine_pwm_type), 1, n_kw, args);
    #else
    mp_raise_ValueError(MP_ERROR_TEXT("no machine.PWM for backlight"));
    #endif
}

/******************************************************************************/
// fonts

MP_DEFINE_CONST_OBJ_TYPE(
    u8g2_font_type,
    MP_QSTR_Font,
    MP_TYPE_FLAG_NONE
    );

#define FONT(pyname, arr) static const u8g2_font_obj_t pyname##_obj = {{&u8g2_font_type}, arr};
#include "u8g2_fonts_table.h"
#undef FONT

const uint8_t *u8g2_font_get(mp_obj_t font_in) {
    if (!mp_obj_is_type(font_in, &u8g2_font_type)) {
        mp_raise_TypeError(MP_ERROR_TEXT("expecting a font"));
    }
    return ((const u8g2_font_obj_t *)MP_OBJ_TO_PTR(font_in))->data;
}

/******************************************************************************/
// init

// rotation argument of init(): R0..R3, MIRROR
static const u8g2_cb_t *const u8g2_rotations[] = {
    U8G2_R0, U8G2_R1, U8G2_R2, U8G2_R3, U8G2_MIRROR,
};

// init(spi=None, cs=None, dc=None, rst=None, bl=None, *, display, i2c=None, addr=None, rotation=R0)
// spi displays: spi; cs, dc, rst and bl optional. most spi displays need dc to tell
// commands from data; st7920, max7219 and the sharp memory lcds have no dc.
// i2c displays: i2c; addr (7 bit, default: the display's, mostly 0x3c), rst and bl optional.
static mp_obj_t u8g2_init(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_spi, ARG_cs, ARG_dc, ARG_rst, ARG_bl, ARG_display, ARG_i2c, ARG_addr, ARG_rotation };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_spi, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_cs, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_dc, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_rst, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_bl, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_display, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_i2c, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_addr, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_rotation, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    const u8g2_mp_display_t *disp = u8g2_display_get(args[ARG_display].u_obj);
    uint8_t bus = u8g2_display_bus(disp);
    if (bus == U8G2_MP_BUS_NONE) {
        mp_raise_ValueError(MP_ERROR_TEXT("display has no spi or i2c interface"));
    }

    mp_int_t rotation = args[ARG_rotation].u_int;
    if (rotation < 0 || rotation >= (mp_int_t)MP_ARRAY_SIZE(u8g2_rotations)) {
        mp_raise_ValueError(MP_ERROR_TEXT("rotation: R0, R1, R2, R3 or MIRROR"));
    }

    mp_int_t addr = -1;
    if (args[ARG_addr].u_obj != mp_const_none) {
        if (bus != U8G2_MP_BUS_I2C) {
            mp_raise_ValueError(MP_ERROR_TEXT("addr is for i2c displays"));
        }
        addr = mp_obj_get_int(args[ARG_addr].u_obj);
        if (addr < 0 || addr > 127) {
            mp_raise_ValueError(MP_ERROR_TEXT("addr: 7 bit i2c address"));
        }
    }

    u8x8_msg_cb byte_cb;
    u8g2_ready = false;
    if (bus == U8G2_MP_BUS_I2C) {
        if (args[ARG_i2c].u_obj == mp_const_none) {
            mp_raise_ValueError(MP_ERROR_TEXT("i2c display: needs i2c="));
        }
        if (args[ARG_spi].u_obj != mp_const_none || args[ARG_cs].u_obj != mp_const_none || args[ARG_dc].u_obj != mp_const_none) {
            mp_raise_ValueError(MP_ERROR_TEXT("i2c display: no spi, cs or dc"));
        }
        u8g2_port_init_i2c(args[ARG_i2c].u_obj, args[ARG_rst].u_obj);
        byte_cb = u8g2_port_i2c_byte_cb;
    } else {
        if (args[ARG_spi].u_obj == mp_const_none) {
            mp_raise_ValueError(MP_ERROR_TEXT("spi display: needs spi="));
        }
        if (args[ARG_i2c].u_obj != mp_const_none) {
            mp_raise_ValueError(MP_ERROR_TEXT("spi display: no i2c"));
        }
        u8g2_port_init(args[ARG_spi].u_obj, args[ARG_cs].u_obj, args[ARG_dc].u_obj, args[ARG_rst].u_obj);
        byte_cb = u8g2_port_byte_cb;
    }

    // what u8g2_Setup_xxx_f() does, with our heap buffer.
    // color drivers set clut_native_bpp when they set up memory; monochrome drivers leave it 0
    mp_u8g2.u8x8.clut_native_bpp = 0;
    u8g2_SetupDisplay(&mp_u8g2, disp->display_cb, disp->cad_cb, byte_cb, u8g2_port_gpio_and_delay_cb);
    // some drivers use a software reset when there is no reset pin. the pin number is not used
    mp_u8g2.u8x8.pins[U8X8_PIN_RESET] = args[ARG_rst].u_obj == mp_const_none ? U8X8_PIN_NONE : 0;
    if (addr >= 0) {
        u8x8_SetI2CAddress(&mp_u8g2.u8x8, addr << 1); // u8x8 keeps the 8 bit write address
    }
    bool is_color = mp_u8g2.u8x8.clut_native_bpp != 0;
    size_t buf_size = disp->tile_width * 8 * disp->tile_height;
    size_t map_size = is_color ? (disp->tile_width * disp->tile_height + 1) / 2 : 0;
    MP_STATE_VM(u8g2_mem) = NULL; // the previous block, if any, can be collected
    uint8_t *mem = m_new(uint8_t, buf_size + map_size);
    memset(mem, 0, buf_size + map_size);
    MP_STATE_VM(u8g2_mem) = mem;
    u8g2_SetupBuffer(&mp_u8g2, mem, disp->tile_height, disp->ll_hvline, u8g2_rotations[rotation]);
    // monochrome display: no map. this also drops the map of a previous color display
    u8g2_clut_map = is_color ? mem + buf_size : NULL;
    u8g2_SetClutMap(&mp_u8g2, u8g2_clut_map);
    u8g2_is_color = is_color;
    u8g2_InitDisplay(&mp_u8g2);

    // known state: default palette (set by setup), clut index 0 everywhere, empty screen
    u8g2_SetMaxClipWindow(&mp_u8g2);
    u8g2_SetDrawColor(&mp_u8g2, 1);
    u8g2_SetFontMode(&mp_u8g2, 0);
    u8g2_SetBitmapMode(&mp_u8g2, 0);
    u8g2_SetFontDirection(&mp_u8g2, 0);
    u8g2_SetFontPosBaseline(&mp_u8g2);
    u8g2_SetFont(&mp_u8g2, U8G2_MP_DEFAULT_FONT);
    u8g2_ClearDisplay(&mp_u8g2);
    u8g2_SetPowerSave(&mp_u8g2, 0);
    u8g2_power_save = false;
    u8g2_bl_init(args[ARG_bl].u_obj);
    mp_obj_dict_store(MP_OBJ_FROM_PTR(&MP_STATE_VM(mp_loaded_modules_dict)),
        MP_OBJ_NEW_QSTR(MP_QSTR_u8g2), MP_OBJ_FROM_PTR(&u8g2_user_cmodule));
    u8g2_ready = true;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(u8g2_init_obj, 0, u8g2_init);

// display_info(display): (width, height, bus, spi_mode) of a display, without init().
// bus is BUS_SPI, BUS_SPI_NO_DC, BUS_I2C, or BUS_NONE: not supported. spi_mode: polarity = spi_mode >> 1, phase = spi_mode & 1.
static mp_obj_t u8g2_display_info(mp_obj_t display_in) {
    const u8g2_mp_display_t *disp = u8g2_display_get(display_in);
    u8x8_t tmp;
    memset(&tmp, 0, sizeof(tmp));
    disp->display_cb(&tmp, U8X8_MSG_DISPLAY_SETUP_MEMORY, 0, NULL);
    const u8x8_display_info_t *info = tmp.display_info;
    mp_obj_t items[] = {
        MP_OBJ_NEW_SMALL_INT(info->pixel_width),
        MP_OBJ_NEW_SMALL_INT(info->pixel_height),
        MP_OBJ_NEW_SMALL_INT(u8g2_display_bus(disp)),
        MP_OBJ_NEW_SMALL_INT(info->spi_mode),
    };
    return mp_obj_new_tuple(MP_ARRAY_SIZE(items), items);
}
static MP_DEFINE_CONST_FUN_OBJ_1(u8g2_display_info_obj, u8g2_display_info);

/******************************************************************************/
// helpers

#define ARG(i) mp_obj_get_int(args[i])

static mp_obj_t u8g2_ret_none_noarg(void (*f)(u8g2_t *)) {
    f(&mp_u8g2);
    return mp_const_none;
}

#define DEF_NOARG(pyname, cfunc) \
    static mp_obj_t u8g2_##pyname(void) { \
        u8g2_check_ready(); \
        return u8g2_ret_none_noarg(cfunc); \
    } \
    static MP_DEFINE_CONST_FUN_OBJ_0(u8g2_##pyname##_obj, u8g2_##pyname);

#define DEF_GET(pyname, expr) \
    static mp_obj_t u8g2_##pyname(void) { \
        u8g2_check_ready(); \
        return MP_OBJ_NEW_SMALL_INT(expr); \
    } \
    static MP_DEFINE_CONST_FUN_OBJ_0(u8g2_##pyname##_obj, u8g2_##pyname);

/******************************************************************************/
// buffer and display

static void u8g2_clear_display_c(u8g2_t *u8g2) {
    u8g2_ClearDisplay(u8g2);
}

DEF_NOARG(clear_buffer, u8g2_ClearBuffer)
DEF_NOARG(send_buffer, u8g2_SendBuffer)
DEF_NOARG(clear_display, u8g2_clear_display_c)
DEF_NOARG(update_display, u8g2_UpdateDisplay)
DEF_NOARG(set_max_clip_window, u8g2_SetMaxClipWindow)
DEF_NOARG(set_font_pos_baseline, u8g2_SetFontPosBaseline)
DEF_NOARG(set_font_pos_bottom, u8g2_SetFontPosBottom)
DEF_NOARG(set_font_pos_top, u8g2_SetFontPosTop)
DEF_NOARG(set_font_pos_center, u8g2_SetFontPosCenter)
DEF_NOARG(set_font_ref_height_text, u8g2_SetFontRefHeightText)
DEF_NOARG(set_font_ref_height_extended_text, u8g2_SetFontRefHeightExtendedText)
DEF_NOARG(set_font_ref_height_all, u8g2_SetFontRefHeightAll)

DEF_GET(get_display_width, u8g2_GetDisplayWidth(&mp_u8g2))
DEF_GET(get_display_height, u8g2_GetDisplayHeight(&mp_u8g2))
DEF_GET(get_cols, u8x8_GetCols(u8g2_GetU8x8(&mp_u8g2)))
DEF_GET(get_rows, u8x8_GetRows(u8g2_GetU8x8(&mp_u8g2)))
DEF_GET(get_draw_color, u8g2_GetDrawColor(&mp_u8g2))
DEF_GET(get_ascent, u8g2_GetAscent(&mp_u8g2))
DEF_GET(get_descent, u8g2_GetDescent(&mp_u8g2))
DEF_GET(get_max_char_width, u8g2_GetMaxCharWidth(&mp_u8g2))
DEF_GET(get_max_char_height, u8g2_GetMaxCharHeight(&mp_u8g2))
DEF_GET(get_font_ascent, u8g2_GetFontAscent(&mp_u8g2))
DEF_GET(get_font_descent, u8g2_GetFontDescent(&mp_u8g2))
DEF_GET(get_buffer_tile_width, u8g2_GetBufferTileWidth(&mp_u8g2))
DEF_GET(get_buffer_tile_height, u8g2_GetBufferTileHeight(&mp_u8g2))

static mp_obj_t u8g2_update_display_area(size_t n_args, const mp_obj_t *args) {
    u8g2_check_ready();
    u8g2_UpdateDisplayArea(&mp_u8g2, ARG(0), ARG(1), ARG(2), ARG(3));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(u8g2_update_display_area_obj, 4, 4, u8g2_update_display_area);

// set_power_save(on): display sleep, backlight off. off: wake, backlight restored.
static mp_obj_t u8g2_set_power_save(mp_obj_t on) {
    u8g2_check_ready();
    u8g2_power_save = mp_obj_is_true(on);
    u8g2_SetPowerSave(&mp_u8g2, u8g2_power_save);
    u8g2_bl_apply();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(u8g2_set_power_save_obj, u8g2_set_power_save);

// set_flip_mode(mode): 0 normal, 1 rotated by 180 degrees. takes effect on the
// next send_buffer(); the buffer and the clut map are not changed.
static mp_obj_t u8g2_set_flip_mode(mp_obj_t mode) {
    u8g2_check_ready();
    u8g2_SetFlipMode(&mp_u8g2, mp_obj_get_int(mode));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(u8g2_set_flip_mode_obj, u8g2_set_flip_mode);

// backlight(): level 0..255. backlight(level): set level.
static mp_obj_t u8g2_backlight(size_t n_args, const mp_obj_t *args) {
    u8g2_check_ready();
    if (n_args == 0) {
        return MP_OBJ_NEW_SMALL_INT(u8g2_bl_level);
    }
    mp_int_t level = mp_obj_get_int(args[0]);
    if (level < 0 || level > 255) {
        mp_raise_ValueError(MP_ERROR_TEXT("backlight 0..255"));
    }
    if (MP_STATE_VM(u8g2_bl_pwm) == MP_OBJ_NULL) {
        mp_raise_ValueError(MP_ERROR_TEXT("no backlight pin, see init(bl=...)"));
    }
    u8g2_bl_level = level;
    u8g2_bl_apply();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(u8g2_backlight_obj, 0, 1, u8g2_backlight);

static mp_obj_t u8g2_set_clip_window(size_t n_args, const mp_obj_t *args) {
    u8g2_check_ready();
    u8g2_SetClipWindow(&mp_u8g2, ARG(0), ARG(1), ARG(2), ARG(3));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(u8g2_set_clip_window_obj, 4, 4, u8g2_set_clip_window);

/******************************************************************************/
// drawing state

static mp_obj_t u8g2_set_draw_color(mp_obj_t c) {
    u8g2_check_ready();
    u8g2_SetDrawColor(&mp_u8g2, mp_obj_get_int(c));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(u8g2_set_draw_color_obj, u8g2_set_draw_color);

static mp_obj_t u8g2_set_bitmap_mode(mp_obj_t m) {
    u8g2_check_ready();
    u8g2_SetBitmapMode(&mp_u8g2, mp_obj_get_int(m));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(u8g2_set_bitmap_mode_obj, u8g2_set_bitmap_mode);

static mp_obj_t u8g2_set_font(mp_obj_t font) {
    u8g2_check_ready();
    u8g2_SetFont(&mp_u8g2, u8g2_font_get(font));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(u8g2_set_font_obj, u8g2_set_font);

static mp_obj_t u8g2_set_font_mode(mp_obj_t m) {
    u8g2_check_ready();
    u8g2_SetFontMode(&mp_u8g2, mp_obj_get_int(m));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(u8g2_set_font_mode_obj, u8g2_set_font_mode);

static mp_obj_t u8g2_set_font_direction(mp_obj_t d) {
    u8g2_check_ready();
    u8g2_SetFontDirection(&mp_u8g2, mp_obj_get_int(d));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(u8g2_set_font_direction_obj, u8g2_set_font_direction);

/******************************************************************************/
// text

static mp_obj_t u8g2_draw_str(mp_obj_t x, mp_obj_t y, mp_obj_t s) {
    u8g2_check_ready();
    return MP_OBJ_NEW_SMALL_INT(u8g2_DrawStr(&mp_u8g2, mp_obj_get_int(x), mp_obj_get_int(y), mp_obj_str_get_str(s)));
}
static MP_DEFINE_CONST_FUN_OBJ_3(u8g2_draw_str_obj, u8g2_draw_str);

static mp_obj_t u8g2_draw_utf8(mp_obj_t x, mp_obj_t y, mp_obj_t s) {
    u8g2_check_ready();
    return MP_OBJ_NEW_SMALL_INT(u8g2_DrawUTF8(&mp_u8g2, mp_obj_get_int(x), mp_obj_get_int(y), mp_obj_str_get_str(s)));
}
static MP_DEFINE_CONST_FUN_OBJ_3(u8g2_draw_utf8_obj, u8g2_draw_utf8);

static mp_obj_t u8g2_draw_glyph(mp_obj_t x, mp_obj_t y, mp_obj_t enc) {
    u8g2_check_ready();
    return MP_OBJ_NEW_SMALL_INT(u8g2_DrawGlyph(&mp_u8g2, mp_obj_get_int(x), mp_obj_get_int(y), mp_obj_get_int(enc)));
}
static MP_DEFINE_CONST_FUN_OBJ_3(u8g2_draw_glyph_obj, u8g2_draw_glyph);

static mp_obj_t u8g2_get_str_width(mp_obj_t s) {
    u8g2_check_ready();
    return MP_OBJ_NEW_SMALL_INT(u8g2_GetStrWidth(&mp_u8g2, mp_obj_str_get_str(s)));
}
static MP_DEFINE_CONST_FUN_OBJ_1(u8g2_get_str_width_obj, u8g2_get_str_width);

static mp_obj_t u8g2_get_utf8_width(mp_obj_t s) {
    u8g2_check_ready();
    return MP_OBJ_NEW_SMALL_INT(u8g2_GetUTF8Width(&mp_u8g2, mp_obj_str_get_str(s)));
}
static MP_DEFINE_CONST_FUN_OBJ_1(u8g2_get_utf8_width_obj, u8g2_get_utf8_width);

// is_all_valid_utf8(s): True if the current font has a glyph for every code in s
static mp_obj_t u8g2_is_all_valid_utf8(mp_obj_t s) {
    u8g2_check_ready();
    return mp_obj_new_bool(u8g2_IsAllValidUTF8(&mp_u8g2, mp_obj_str_get_str(s)));
}
static MP_DEFINE_CONST_FUN_OBJ_1(u8g2_is_all_valid_utf8_obj, u8g2_is_all_valid_utf8);

// draw_button_utf8(x, y, flags, width, padding_h, padding_v, text)
static mp_obj_t u8g2_draw_button_utf8(size_t n_args, const mp_obj_t *args) {
    u8g2_check_ready();
    u8g2_DrawButtonUTF8(&mp_u8g2, ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), mp_obj_str_get_str(args[6]));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(u8g2_draw_button_utf8_obj, 7, 7, u8g2_draw_button_utf8);

/******************************************************************************/
// graphics

static mp_obj_t u8g2_draw_pixel(mp_obj_t x, mp_obj_t y) {
    u8g2_check_ready();
    u8g2_DrawPixel(&mp_u8g2, mp_obj_get_int(x), mp_obj_get_int(y));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(u8g2_draw_pixel_obj, u8g2_draw_pixel);

static mp_obj_t u8g2_draw_hline(mp_obj_t x, mp_obj_t y, mp_obj_t w) {
    u8g2_check_ready();
    u8g2_DrawHLine(&mp_u8g2, mp_obj_get_int(x), mp_obj_get_int(y), mp_obj_get_int(w));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(u8g2_draw_hline_obj, u8g2_draw_hline);

static mp_obj_t u8g2_draw_vline(mp_obj_t x, mp_obj_t y, mp_obj_t h) {
    u8g2_check_ready();
    u8g2_DrawVLine(&mp_u8g2, mp_obj_get_int(x), mp_obj_get_int(y), mp_obj_get_int(h));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(u8g2_draw_vline_obj, u8g2_draw_vline);

#define DEF_DRAW4(pyname, cfunc) \
    static mp_obj_t u8g2_##pyname(size_t n_args, const mp_obj_t *args) { \
        u8g2_check_ready(); \
        cfunc(&mp_u8g2, ARG(0), ARG(1), ARG(2), ARG(3)); \
        return mp_const_none; \
    } \
    static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(u8g2_##pyname##_obj, 4, 4, u8g2_##pyname);

DEF_DRAW4(draw_line, u8g2_DrawLine)
DEF_DRAW4(draw_box, u8g2_DrawBox)
DEF_DRAW4(draw_frame, u8g2_DrawFrame)

#define DEF_DRAW5(pyname, cfunc) \
    static mp_obj_t u8g2_##pyname(size_t n_args, const mp_obj_t *args) { \
        u8g2_check_ready(); \
        cfunc(&mp_u8g2, ARG(0), ARG(1), ARG(2), ARG(3), ARG(4)); \
        return mp_const_none; \
    } \
    static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(u8g2_##pyname##_obj, 5, 5, u8g2_##pyname);

DEF_DRAW5(draw_rbox, u8g2_DrawRBox)
DEF_DRAW5(draw_rframe, u8g2_DrawRFrame)
DEF_DRAW5(draw_arc, u8g2_DrawArc)

// draw_circle(x, y, r, opt=DRAW_ALL), draw_disc(...)
#define DEF_CIRCLE(pyname, cfunc) \
    static mp_obj_t u8g2_##pyname(size_t n_args, const mp_obj_t *args) { \
        u8g2_check_ready(); \
        cfunc(&mp_u8g2, ARG(0), ARG(1), ARG(2), n_args > 3 ? ARG(3) : U8G2_DRAW_ALL); \
        return mp_const_none; \
    } \
    static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(u8g2_##pyname##_obj, 3, 4, u8g2_##pyname);

DEF_CIRCLE(draw_circle, u8g2_DrawCircle)
DEF_CIRCLE(draw_disc, u8g2_DrawDisc)

// draw_ellipse(x, y, rx, ry, opt=DRAW_ALL), draw_filled_ellipse(...)
#define DEF_ELLIPSE(pyname, cfunc) \
    static mp_obj_t u8g2_##pyname(size_t n_args, const mp_obj_t *args) { \
        u8g2_check_ready(); \
        cfunc(&mp_u8g2, ARG(0), ARG(1), ARG(2), ARG(3), n_args > 4 ? ARG(4) : U8G2_DRAW_ALL); \
        return mp_const_none; \
    } \
    static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(u8g2_##pyname##_obj, 4, 5, u8g2_##pyname);

DEF_ELLIPSE(draw_ellipse, u8g2_DrawEllipse)
DEF_ELLIPSE(draw_filled_ellipse, u8g2_DrawFilledEllipse)

static mp_obj_t u8g2_draw_triangle(size_t n_args, const mp_obj_t *args) {
    u8g2_check_ready();
    u8g2_DrawTriangle(&mp_u8g2, ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(u8g2_draw_triangle_obj, 6, 6, u8g2_draw_triangle);

// draw_xbm(x, y, w, h, buf)
static mp_obj_t u8g2_draw_xbm(size_t n_args, const mp_obj_t *args) {
    u8g2_check_ready();
    mp_int_t w = ARG(2), h = ARG(3);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[4], &bufinfo, MP_BUFFER_READ);
    if (w < 0 || h < 0 || bufinfo.len < (size_t)((w + 7) / 8 * h)) {
        mp_raise_ValueError(MP_ERROR_TEXT("buffer too small"));
    }
    u8g2_DrawXBM(&mp_u8g2, ARG(0), ARG(1), w, h, bufinfo.buf);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(u8g2_draw_xbm_obj, 5, 5, u8g2_draw_xbm);

/******************************************************************************/
// more text and graphics

// draw_glyph_x2(x, y, encoding), draw_str_x2(x, y, s), draw_utf8_x2(x, y, s):
// double size. return the width.
static mp_obj_t u8g2_draw_glyph_x2(mp_obj_t x, mp_obj_t y, mp_obj_t enc) {
    u8g2_check_ready();
    return MP_OBJ_NEW_SMALL_INT(u8g2_DrawGlyphX2(&mp_u8g2, mp_obj_get_int(x), mp_obj_get_int(y), mp_obj_get_int(enc)));
}
static MP_DEFINE_CONST_FUN_OBJ_3(u8g2_draw_glyph_x2_obj, u8g2_draw_glyph_x2);

static mp_obj_t u8g2_draw_str_x2(mp_obj_t x, mp_obj_t y, mp_obj_t str) {
    u8g2_check_ready();
    return MP_OBJ_NEW_SMALL_INT(u8g2_DrawStrX2(&mp_u8g2, mp_obj_get_int(x), mp_obj_get_int(y), mp_obj_str_get_str(str)));
}
static MP_DEFINE_CONST_FUN_OBJ_3(u8g2_draw_str_x2_obj, u8g2_draw_str_x2);

static mp_obj_t u8g2_draw_utf8_x2(mp_obj_t x, mp_obj_t y, mp_obj_t str) {
    u8g2_check_ready();
    return MP_OBJ_NEW_SMALL_INT(u8g2_DrawUTF8X2(&mp_u8g2, mp_obj_get_int(x), mp_obj_get_int(y), mp_obj_str_get_str(str)));
}
static MP_DEFINE_CONST_FUN_OBJ_3(u8g2_draw_utf8_x2_obj, u8g2_draw_utf8_x2);

// draw_utf8_line(x, y, w, s, border=0, invert=False): s centered in a box of
// width w, y is the top of the box
static mp_obj_t u8g2_draw_utf8_line(size_t n_args, const mp_obj_t *args) {
    u8g2_check_ready();
    u8g2_DrawUTF8Line(&mp_u8g2, ARG(0), ARG(1), ARG(2), mp_obj_str_get_str(args[3]),
        n_args > 4 ? ARG(4) : 0, n_args > 5 ? mp_obj_is_true(args[5]) : 0);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(u8g2_draw_utf8_line_obj, 4, 6, u8g2_draw_utf8_line);

// draw_utf8_lines(x, y, w, line_height, s): lines of s (split at newlines),
// each centered in width w. returns the total height.
static mp_obj_t u8g2_draw_utf8_lines(size_t n_args, const mp_obj_t *args) {
    u8g2_check_ready();
    return MP_OBJ_NEW_SMALL_INT(u8g2_DrawUTF8Lines(&mp_u8g2, ARG(0), ARG(1), ARG(2), ARG(3), mp_obj_str_get_str(args[4])));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(u8g2_draw_utf8_lines_obj, 5, 5, u8g2_draw_utf8_lines);

// glyph queries for the current font
static mp_obj_t u8g2_is_glyph(mp_obj_t enc) {
    u8g2_check_ready();
    return mp_obj_new_bool(u8g2_IsGlyph(&mp_u8g2, mp_obj_get_int(enc)));
}
static MP_DEFINE_CONST_FUN_OBJ_1(u8g2_is_glyph_obj, u8g2_is_glyph);

static mp_obj_t u8g2_get_glyph_width(mp_obj_t enc) {
    u8g2_check_ready();
    return MP_OBJ_NEW_SMALL_INT(u8g2_GetGlyphWidth(&mp_u8g2, mp_obj_get_int(enc)));
}
static MP_DEFINE_CONST_FUN_OBJ_1(u8g2_get_glyph_width_obj, u8g2_get_glyph_width);

static mp_obj_t u8g2_get_x_offset_glyph(mp_obj_t enc) {
    u8g2_check_ready();
    return MP_OBJ_NEW_SMALL_INT(u8g2_GetXOffsetGlyph(&mp_u8g2, mp_obj_get_int(enc)));
}
static MP_DEFINE_CONST_FUN_OBJ_1(u8g2_get_x_offset_glyph_obj, u8g2_get_x_offset_glyph);

static mp_obj_t u8g2_get_x_offset_utf8(mp_obj_t str) {
    u8g2_check_ready();
    return MP_OBJ_NEW_SMALL_INT(u8g2_GetXOffsetUTF8(&mp_u8g2, mp_obj_str_get_str(str)));
}
static MP_DEFINE_CONST_FUN_OBJ_1(u8g2_get_x_offset_utf8_obj, u8g2_get_x_offset_utf8);

// draw_polygon(points): filled polygon, points is a sequence of (x, y), 3..6 points
#define U8G2_POLYGON_MAX_POINTS (6)  // PG_MAX_POINTS in u8g2_polygon.c

static mp_obj_t u8g2_draw_polygon(mp_obj_t points_in) {
    u8g2_check_ready();
    size_t n;
    mp_obj_t *points;
    mp_obj_get_array(points_in, &n, &points);
    if (n < 3 || n > U8G2_POLYGON_MAX_POINTS) {
        mp_raise_ValueError(MP_ERROR_TEXT("polygon: 3..6 points"));
    }
    int16_t xy[U8G2_POLYGON_MAX_POINTS][2];
    for (size_t i = 0; i < n; i++) {
        mp_obj_t *p;
        mp_obj_get_array_fixed_n(points[i], 2, &p);
        xy[i][0] = mp_obj_get_int(p[0]);
        xy[i][1] = mp_obj_get_int(p[1]);
    }
    u8g2_ClearPolygonXY();
    for (size_t i = 0; i < n; i++) {
        u8g2_AddPolygonXY(&mp_u8g2, xy[i][0], xy[i][1]);
    }
    u8g2_DrawPolygon(&mp_u8g2);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(u8g2_draw_polygon_obj, u8g2_draw_polygon);

// draw_button_frame(x, y, flags, text_width, padding_h, padding_v)
static mp_obj_t u8g2_draw_button_frame(size_t n_args, const mp_obj_t *args) {
    u8g2_check_ready();
    u8g2_DrawButtonFrame(&mp_u8g2, ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(u8g2_draw_button_frame_obj, 6, 6, u8g2_draw_button_frame);

// draw_bitmap(x, y, cnt, h, buf): cnt bytes per row, h rows, msb is leftmost pixel
static mp_obj_t u8g2_draw_bitmap(size_t n_args, const mp_obj_t *args) {
    u8g2_check_ready();
    mp_int_t cnt = ARG(2), h = ARG(3);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[4], &bufinfo, MP_BUFFER_READ);
    if (cnt < 0 || h < 0 || bufinfo.len < (size_t)(cnt * h)) {
        mp_raise_ValueError(MP_ERROR_TEXT("buffer too small"));
    }
    u8g2_DrawBitmap(&mp_u8g2, ARG(0), ARG(1), cnt, h, bufinfo.buf);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(u8g2_draw_bitmap_obj, 5, 5, u8g2_draw_bitmap);

// draw_horizontal_bitmap(x, y, len, buf): one row of len pixels, msb is leftmost pixel
static mp_obj_t u8g2_draw_horizontal_bitmap(size_t n_args, const mp_obj_t *args) {
    u8g2_check_ready();
    mp_int_t len = ARG(2);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[3], &bufinfo, MP_BUFFER_READ);
    if (len < 0 || bufinfo.len < (size_t)((len + 7) / 8)) {
        mp_raise_ValueError(MP_ERROR_TEXT("buffer too small"));
    }
    u8g2_DrawHorizontalBitmap(&mp_u8g2, ARG(0), ARG(1), len, bufinfo.buf);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(u8g2_draw_horizontal_bitmap_obj, 4, 4, u8g2_draw_horizontal_bitmap);

// draw_hb(x, y, data): text pre-shaped by harfbuzz on the host (hbshape2u8g2):
// records of [encoding hi, encoding lo, dx, dy], ended by encoding 0.
// data must contain the terminator, so u8g2 does not read past the end.
static mp_obj_t u8g2_draw_hb(mp_obj_t x, mp_obj_t y, mp_obj_t data_in) {
    u8g2_check_ready();
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(data_in, &bufinfo, MP_BUFFER_READ);
    const uint8_t *d = bufinfo.buf;
    size_t i = 0;
    for (;;) {
        if (i + 2 > bufinfo.len) {
            mp_raise_ValueError(MP_ERROR_TEXT("hb data: no terminator"));
        }
        if (d[i] == 0 && d[i + 1] == 0) {
            break;
        }
        i += 4;
    }
    u8g2_DrawHB(&mp_u8g2, mp_obj_get_int(x), mp_obj_get_int(y), d);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(u8g2_draw_hb_obj, u8g2_draw_hb);

// screenshots: u8g2_WriteBufferPBM() (black and white, plain P1) and
// u8g2_WriteBufferPPM() (color, plain P3) write small pieces of text; they are
// collected and passed to stream.write() in blocks, so a screenshot needs no
// large allocation. destination None is stdout.
#define U8G2_OUT_BLOCK (512)

typedef void (*u8g2_write_buffer_fn)(u8g2_t *u8g2, void (*out)(const char *s));

typedef struct {
    vstr_t vstr;
    mp_obj_t write[3];  // write method, self, data. MP_OBJ_NULL: stdout
} u8g2_out_t;

static u8g2_out_t *u8g2_out;  // points to the C stack during a screenshot

static void u8g2_out_flush(void) {
    if (u8g2_out->vstr.len > 0) {
        if (u8g2_out->write[0] == MP_OBJ_NULL) {
            mp_hal_stdout_tx_strn(u8g2_out->vstr.buf, u8g2_out->vstr.len);
        } else {
            u8g2_out->write[2] = mp_obj_new_str(u8g2_out->vstr.buf, u8g2_out->vstr.len);
            mp_call_method_n_kw(1, 0, u8g2_out->write);
        }
        vstr_reset(&u8g2_out->vstr);
    }
}

static void u8g2_out_str(const char *s) {
    vstr_add_str(&u8g2_out->vstr, s);
    if (u8g2_out->vstr.len >= U8G2_OUT_BLOCK) {
        u8g2_out_flush();
    }
}

static void u8g2_write_stream(mp_obj_t stream, u8g2_write_buffer_fn fn) {
    u8g2_out_t out;
    vstr_init(&out.vstr, U8G2_OUT_BLOCK + 16);
    out.write[0] = MP_OBJ_NULL;
    if (stream != mp_const_none) {
        mp_load_method(stream, MP_QSTR_write, out.write);
    }
    u8g2_out = &out;
    fn(&mp_u8g2, u8g2_out_str);
    u8g2_out_flush();
    vstr_clear(&out.vstr);
}

// dest: None (stdout), a filename or a stream
static void u8g2_write_dest(mp_obj_t dest, u8g2_write_buffer_fn fn) {
    if (!mp_obj_is_str(dest)) {
        u8g2_write_stream(dest, fn);
        return;
    }
    // filename: open, write, close also on error
    mp_obj_t args[2] = { dest, MP_OBJ_NEW_QSTR(MP_QSTR_w) };
    mp_obj_t file = mp_call_function_n_kw(MP_OBJ_FROM_PTR(&mp_builtin_open_obj), 2, 0, args);
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        u8g2_write_stream(file, fn);
        nlr_pop();
    } else {
        mp_stream_close(file);
        nlr_jump(nlr.ret_val);
    }
    mp_stream_close(file);
}

// write_buffer_pbm(stream or filename): the buffer as plain pbm (P1). set pixels are black.
static mp_obj_t u8g2_write_buffer_pbm(mp_obj_t dest) {
    u8g2_check_ready();
    u8g2_write_dest(dest, u8g2_WriteBufferPBM);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(u8g2_write_buffer_pbm_obj, u8g2_write_buffer_pbm);

// write_buffer_ppm(stream or filename): the screen in color as plain ppm (P3),
// colors as shown on the display. see u8g2_WriteBufferPPM().
static mp_obj_t u8g2_write_buffer_ppm(mp_obj_t dest) {
    u8g2_check_ready();
    u8g2_write_dest(dest, u8g2_WriteBufferPPM);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(u8g2_write_buffer_ppm_obj, u8g2_write_buffer_ppm);

// screenshot(dest=None): ppm if there is a tile clut map, pbm otherwise.
// dest is a stream or filename; without dest, to stdout.
static mp_obj_t u8g2_screenshot(size_t n_args, const mp_obj_t *args) {
    u8g2_check_ready();
    u8g2_write_buffer_fn fn = u8g2_WriteBufferPBM;
    #ifdef U8X8_WITH_CLUT
    if (mp_u8g2.u8x8.tile_clut_map != NULL) {
        fn = u8g2_WriteBufferPPM;
    }
    #endif
    u8g2_write_dest(n_args > 0 ? args[0] : mp_const_none, fn);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(u8g2_screenshot_obj, 0, 1, u8g2_screenshot);

/******************************************************************************/
// clut: color displays only

static void u8g2_check_color(void) {
    u8g2_check_ready();
    if (!u8g2_is_color) {
        mp_raise_ValueError(MP_ERROR_TEXT("not a color display"));
    }
}

static uint8_t u8g2_clut_index(mp_obj_t idx_in) {
    mp_int_t idx = mp_obj_get_int(idx_in);
    if (idx < 0 || idx >= U8X8_CLUT_ENTRY_CNT) {
        mp_raise_ValueError(MP_ERROR_TEXT("clut index 0..15"));
    }
    return idx;
}

// set_clut_color(idx, fg, bg): fg, bg are 0xRRGGBB
static mp_obj_t u8g2_set_clut_color(mp_obj_t idx, mp_obj_t fg, mp_obj_t bg) {
    u8g2_check_color();
    u8g2_SetClutColor(&mp_u8g2, u8g2_clut_index(idx), mp_obj_get_int_truncated(fg), mp_obj_get_int_truncated(bg));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(u8g2_set_clut_color_obj, u8g2_set_clut_color);

// set_clut_region(tx, ty, tw, th, idx): tile units, clipped to the display
static mp_obj_t u8g2_set_clut_region(size_t n_args, const mp_obj_t *args) {
    u8g2_check_color();
    for (int i = 0; i < 4; i++) {
        if (ARG(i) < 0 || ARG(i) > 255) {
            mp_raise_ValueError(MP_ERROR_TEXT("tile out of range"));
        }
    }
    u8g2_SetClutRegion(&mp_u8g2, ARG(0), ARG(1), ARG(2), ARG(3), u8g2_clut_index(args[4]));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(u8g2_set_clut_region_obj, 5, 5, u8g2_set_clut_region);

// size of the tile clut map of the current display, bytes
static size_t u8g2_clut_map_size(void) {
    const u8x8_display_info_t *info = mp_u8g2.u8x8.display_info;
    return (info->tile_width * info->tile_height + 1) / 2;
}

static mp_obj_t u8g2_get_clut_map_size(void) {
    u8g2_check_color();
    return MP_OBJ_NEW_SMALL_INT(u8g2_clut_map_size());
}
static MP_DEFINE_CONST_FUN_OBJ_0(u8g2_get_clut_map_size_obj, u8g2_get_clut_map_size);

// load_clut_map(buf): copy a complete tile clut map
static mp_obj_t u8g2_load_clut_map(mp_obj_t buf_in) {
    u8g2_check_color();
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf_in, &bufinfo, MP_BUFFER_READ);
    if (bufinfo.len != u8g2_clut_map_size()) {
        mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("clut map is %d bytes"), (int)u8g2_clut_map_size());
    }
    memcpy(u8g2_clut_map, bufinfo.buf, bufinfo.len);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(u8g2_load_clut_map_obj, u8g2_load_clut_map);

/******************************************************************************/

static const mp_rom_map_elem_t u8g2_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_u8g2) },
    { MP_ROM_QSTR(MP_QSTR_Font), MP_ROM_PTR(&u8g2_font_type) },
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&u8g2_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_display_info), MP_ROM_PTR(&u8g2_display_info_obj) },

    // buffer and display
    { MP_ROM_QSTR(MP_QSTR_clear_buffer), MP_ROM_PTR(&u8g2_clear_buffer_obj) },
    { MP_ROM_QSTR(MP_QSTR_send_buffer), MP_ROM_PTR(&u8g2_send_buffer_obj) },
    { MP_ROM_QSTR(MP_QSTR_clear_display), MP_ROM_PTR(&u8g2_clear_display_obj) },
    { MP_ROM_QSTR(MP_QSTR_update_display), MP_ROM_PTR(&u8g2_update_display_obj) },
    { MP_ROM_QSTR(MP_QSTR_update_display_area), MP_ROM_PTR(&u8g2_update_display_area_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_power_save), MP_ROM_PTR(&u8g2_set_power_save_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_flip_mode), MP_ROM_PTR(&u8g2_set_flip_mode_obj) },
    { MP_ROM_QSTR(MP_QSTR_backlight), MP_ROM_PTR(&u8g2_backlight_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_clip_window), MP_ROM_PTR(&u8g2_set_clip_window_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_max_clip_window), MP_ROM_PTR(&u8g2_set_max_clip_window_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_display_width), MP_ROM_PTR(&u8g2_get_display_width_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_display_height), MP_ROM_PTR(&u8g2_get_display_height_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_cols), MP_ROM_PTR(&u8g2_get_cols_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_rows), MP_ROM_PTR(&u8g2_get_rows_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_buffer_tile_width), MP_ROM_PTR(&u8g2_get_buffer_tile_width_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_buffer_tile_height), MP_ROM_PTR(&u8g2_get_buffer_tile_height_obj) },

    // drawing state
    { MP_ROM_QSTR(MP_QSTR_set_draw_color), MP_ROM_PTR(&u8g2_set_draw_color_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_draw_color), MP_ROM_PTR(&u8g2_get_draw_color_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_bitmap_mode), MP_ROM_PTR(&u8g2_set_bitmap_mode_obj) },

    // text
    { MP_ROM_QSTR(MP_QSTR_set_font), MP_ROM_PTR(&u8g2_set_font_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_font_mode), MP_ROM_PTR(&u8g2_set_font_mode_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_font_direction), MP_ROM_PTR(&u8g2_set_font_direction_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_font_pos_baseline), MP_ROM_PTR(&u8g2_set_font_pos_baseline_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_font_pos_bottom), MP_ROM_PTR(&u8g2_set_font_pos_bottom_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_font_pos_top), MP_ROM_PTR(&u8g2_set_font_pos_top_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_font_pos_center), MP_ROM_PTR(&u8g2_set_font_pos_center_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_font_ref_height_text), MP_ROM_PTR(&u8g2_set_font_ref_height_text_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_font_ref_height_extended_text), MP_ROM_PTR(&u8g2_set_font_ref_height_extended_text_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_font_ref_height_all), MP_ROM_PTR(&u8g2_set_font_ref_height_all_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_ascent), MP_ROM_PTR(&u8g2_get_ascent_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_descent), MP_ROM_PTR(&u8g2_get_descent_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_font_ascent), MP_ROM_PTR(&u8g2_get_font_ascent_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_font_descent), MP_ROM_PTR(&u8g2_get_font_descent_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_max_char_width), MP_ROM_PTR(&u8g2_get_max_char_width_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_max_char_height), MP_ROM_PTR(&u8g2_get_max_char_height_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_str), MP_ROM_PTR(&u8g2_draw_str_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_utf8), MP_ROM_PTR(&u8g2_draw_utf8_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_glyph), MP_ROM_PTR(&u8g2_draw_glyph_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_str_width), MP_ROM_PTR(&u8g2_get_str_width_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_utf8_width), MP_ROM_PTR(&u8g2_get_utf8_width_obj) },
    { MP_ROM_QSTR(MP_QSTR_is_all_valid_utf8), MP_ROM_PTR(&u8g2_is_all_valid_utf8_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_button_utf8), MP_ROM_PTR(&u8g2_draw_button_utf8_obj) },

    // graphics
    { MP_ROM_QSTR(MP_QSTR_draw_pixel), MP_ROM_PTR(&u8g2_draw_pixel_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_hline), MP_ROM_PTR(&u8g2_draw_hline_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_vline), MP_ROM_PTR(&u8g2_draw_vline_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_line), MP_ROM_PTR(&u8g2_draw_line_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_box), MP_ROM_PTR(&u8g2_draw_box_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_frame), MP_ROM_PTR(&u8g2_draw_frame_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_rbox), MP_ROM_PTR(&u8g2_draw_rbox_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_rframe), MP_ROM_PTR(&u8g2_draw_rframe_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_arc), MP_ROM_PTR(&u8g2_draw_arc_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_circle), MP_ROM_PTR(&u8g2_draw_circle_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_disc), MP_ROM_PTR(&u8g2_draw_disc_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_ellipse), MP_ROM_PTR(&u8g2_draw_ellipse_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_filled_ellipse), MP_ROM_PTR(&u8g2_draw_filled_ellipse_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_triangle), MP_ROM_PTR(&u8g2_draw_triangle_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_xbm), MP_ROM_PTR(&u8g2_draw_xbm_obj) },

    { MP_ROM_QSTR(MP_QSTR_draw_glyph_x2), MP_ROM_PTR(&u8g2_draw_glyph_x2_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_str_x2), MP_ROM_PTR(&u8g2_draw_str_x2_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_utf8_x2), MP_ROM_PTR(&u8g2_draw_utf8_x2_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_utf8_line), MP_ROM_PTR(&u8g2_draw_utf8_line_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_utf8_lines), MP_ROM_PTR(&u8g2_draw_utf8_lines_obj) },
    { MP_ROM_QSTR(MP_QSTR_is_glyph), MP_ROM_PTR(&u8g2_is_glyph_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_glyph_width), MP_ROM_PTR(&u8g2_get_glyph_width_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_x_offset_glyph), MP_ROM_PTR(&u8g2_get_x_offset_glyph_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_x_offset_utf8), MP_ROM_PTR(&u8g2_get_x_offset_utf8_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_polygon), MP_ROM_PTR(&u8g2_draw_polygon_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_button_frame), MP_ROM_PTR(&u8g2_draw_button_frame_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_bitmap), MP_ROM_PTR(&u8g2_draw_bitmap_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_horizontal_bitmap), MP_ROM_PTR(&u8g2_draw_horizontal_bitmap_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_hb), MP_ROM_PTR(&u8g2_draw_hb_obj) },
    { MP_ROM_QSTR(MP_QSTR_screenshot), MP_ROM_PTR(&u8g2_screenshot_obj) },
    { MP_ROM_QSTR(MP_QSTR_write_buffer_pbm), MP_ROM_PTR(&u8g2_write_buffer_pbm_obj) },
    { MP_ROM_QSTR(MP_QSTR_write_buffer_ppm), MP_ROM_PTR(&u8g2_write_buffer_ppm_obj) },

    // clut
    { MP_ROM_QSTR(MP_QSTR_set_clut_color), MP_ROM_PTR(&u8g2_set_clut_color_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_clut_region), MP_ROM_PTR(&u8g2_set_clut_region_obj) },
    { MP_ROM_QSTR(MP_QSTR_load_clut_map), MP_ROM_PTR(&u8g2_load_clut_map_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_clut_map_size), MP_ROM_PTR(&u8g2_get_clut_map_size_obj) },

    // constants
    { MP_ROM_QSTR(MP_QSTR_DRAW_UPPER_RIGHT), MP_ROM_INT(U8G2_DRAW_UPPER_RIGHT) },
    { MP_ROM_QSTR(MP_QSTR_DRAW_UPPER_LEFT), MP_ROM_INT(U8G2_DRAW_UPPER_LEFT) },
    { MP_ROM_QSTR(MP_QSTR_DRAW_LOWER_LEFT), MP_ROM_INT(U8G2_DRAW_LOWER_LEFT) },
    { MP_ROM_QSTR(MP_QSTR_DRAW_LOWER_RIGHT), MP_ROM_INT(U8G2_DRAW_LOWER_RIGHT) },
    { MP_ROM_QSTR(MP_QSTR_DRAW_ALL), MP_ROM_INT(U8G2_DRAW_ALL) },
    { MP_ROM_QSTR(MP_QSTR_BTN_BW0), MP_ROM_INT(U8G2_BTN_BW0) },
    { MP_ROM_QSTR(MP_QSTR_BTN_BW1), MP_ROM_INT(U8G2_BTN_BW1) },
    { MP_ROM_QSTR(MP_QSTR_BTN_BW2), MP_ROM_INT(U8G2_BTN_BW2) },
    { MP_ROM_QSTR(MP_QSTR_BTN_BW3), MP_ROM_INT(U8G2_BTN_BW3) },
    { MP_ROM_QSTR(MP_QSTR_BTN_SHADOW0), MP_ROM_INT(U8G2_BTN_SHADOW0) },
    { MP_ROM_QSTR(MP_QSTR_BTN_SHADOW1), MP_ROM_INT(U8G2_BTN_SHADOW1) },
    { MP_ROM_QSTR(MP_QSTR_BTN_SHADOW2), MP_ROM_INT(U8G2_BTN_SHADOW2) },
    { MP_ROM_QSTR(MP_QSTR_BTN_INV), MP_ROM_INT(U8G2_BTN_INV) },
    { MP_ROM_QSTR(MP_QSTR_BTN_HCENTER), MP_ROM_INT(U8G2_BTN_HCENTER) },
    { MP_ROM_QSTR(MP_QSTR_BTN_XFRAME), MP_ROM_INT(U8G2_BTN_XFRAME) },

    // init(rotation=...)
    { MP_ROM_QSTR(MP_QSTR_R0), MP_ROM_INT(0) },
    { MP_ROM_QSTR(MP_QSTR_R1), MP_ROM_INT(1) },
    { MP_ROM_QSTR(MP_QSTR_R2), MP_ROM_INT(2) },
    { MP_ROM_QSTR(MP_QSTR_R3), MP_ROM_INT(3) },
    { MP_ROM_QSTR(MP_QSTR_MIRROR), MP_ROM_INT(4) },

    // display_info() bus
    { MP_ROM_QSTR(MP_QSTR_BUS_NONE), MP_ROM_INT(U8G2_MP_BUS_NONE) },
    { MP_ROM_QSTR(MP_QSTR_BUS_SPI), MP_ROM_INT(U8G2_MP_BUS_SPI) },
    { MP_ROM_QSTR(MP_QSTR_BUS_SPI_NO_DC), MP_ROM_INT(U8G2_MP_BUS_SPI_NO_DC) },
    { MP_ROM_QSTR(MP_QSTR_BUS_I2C), MP_ROM_INT(U8G2_MP_BUS_I2C) },

    // displays
    #define D(name, display_cb, cad_cb, ll_hvline, tile_width, tile_height, flags) \
    { MP_ROM_QSTR(MP_QSTR_##name), MP_ROM_INT(DISPLAY_##name) },
    #include "u8g2_displays_table.h"
    #undef D

    // fonts
    #define FONT(pyname, arr) { MP_ROM_QSTR(MP_QSTR_##pyname), MP_ROM_PTR(&pyname##_obj) },
    #include "u8g2_fonts_table.h"
    #undef FONT
};
static MP_DEFINE_CONST_DICT(u8g2_module_globals, u8g2_module_globals_table);

const mp_obj_module_t u8g2_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&u8g2_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_u8g2, u8g2_user_cmodule);
