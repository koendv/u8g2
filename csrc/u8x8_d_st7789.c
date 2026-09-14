#include "u8x8.h"

#ifdef U8X8_WITH_CLUT

#include "u8x8_clut_driver.h"

#define U8X8_D_ST7789_MADCTL 0x0C0

/* init sequence from adafruit */
static const uint8_t u8x8_d_st7789_init_seq[] = {
  U8X8_START_TRANSFER(),
  U8X8_C(0x001),           /* software reset */
  U8X8_DLY(150),
  U8X8_C(0x011),           /* sleep out */
  U8X8_DLY(120),
  U8X8_CA(0x03A, 0x003),   /* COLMOD: 12 bit/pixel (RGB444) */
  U8X8_DLY(10),
  U8X8_CA(0x036, U8X8_D_ST7789_MADCTL), /* MADCTL: fixed orientation, RGB order */
  U8X8_C(0x021),           /* display inversion on */
  U8X8_DLY(10),
  U8X8_C(0x013),           /* normal display mode on */
  U8X8_DLY(10),
  U8X8_C(0x029),           /* display on */
  U8X8_DLY(10),
  U8X8_END_TRANSFER(),
  U8X8_END()
};

static const uint8_t u8x8_d_st7789_sleep_on_seq[] = {
  U8X8_START_TRANSFER(),
  U8X8_C(0x010),           /* sleep in */
  U8X8_DLY(5),
  U8X8_END_TRANSFER(),
  U8X8_END()
};

static const uint8_t u8x8_d_st7789_sleep_off_seq[] = {
  U8X8_START_TRANSFER(),
  U8X8_C(0x011),           /* sleep out */
  U8X8_DLY(120),           /* wait 120ms after sleep before further commands */
  U8X8_END_TRANSFER(),
  U8X8_END()
};

static void u8x8_d_st7789_draw_tile(u8x8_t *u8x8, uint8_t run_cnt, u8x8_tile_t *tile, uint8_t y_offset)
{
  uint8_t tile_cols = u8x8->display_info->tile_width;
  uint8_t tx = tile->x_pos;
  uint8_t ty = tile->y_pos;
  uint16_t py0 = (uint16_t)ty * 8 + y_offset;
  uint16_t py1 = py0 + 7;

  u8x8_cad_StartTransfer(u8x8);
  do
  {
    uint8_t cnt = tile->cnt;
    uint16_t px0 = (uint16_t)tx * 8 + u8x8->x_offset;
    uint16_t px1 = (uint16_t)(tx + cnt) * 8 + u8x8->x_offset - 1;

    u8x8_cad_SendCmd(u8x8, 0x02A); /* CASET: column address set */
    u8x8_cad_SendArg(u8x8, px0 >> 8);
    u8x8_cad_SendArg(u8x8, px0 & 0x0ff);
    u8x8_cad_SendArg(u8x8, px1 >> 8);
    u8x8_cad_SendArg(u8x8, px1 & 0x0ff);

    u8x8_cad_SendCmd(u8x8, 0x02B); /* RASET: row address set */
    u8x8_cad_SendArg(u8x8, py0 >> 8);
    u8x8_cad_SendArg(u8x8, py0 & 0x0ff);
    u8x8_cad_SendArg(u8x8, py1 >> 8);
    u8x8_cad_SendArg(u8x8, py1 & 0x0ff);

    u8x8_cad_SendCmd(u8x8, 0x02C); /* RAMWR: write display data to ram */

    u8x8_clut_stream_rgb444(u8x8, tile->tile_ptr, cnt, (uint16_t)ty * tile_cols + tx);

    tx = (uint8_t)(tx + cnt);
    run_cnt--;
  } while ( run_cnt > 0 );
  u8x8_cad_EndTransfer(u8x8);
}

static uint8_t u8x8_d_st7789_generic(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr, uint8_t y_offset, const u8x8_display_info_t *display_info)
{
  switch(msg)
  {
    case U8X8_MSG_DISPLAY_SETUP_MEMORY:
      u8x8_d_helper_display_setup_memory(u8x8, display_info);
      u8x8_clut_set_default_palette(u8x8, U8X8_CLUT_NATIVE_RGB444);
      break;

    case U8X8_MSG_DISPLAY_INIT:
      u8x8_d_helper_display_init(u8x8);
      u8x8_cad_SendSequence(u8x8, u8x8_d_st7789_init_seq);
      break;

    case U8X8_MSG_DISPLAY_SET_POWER_SAVE:
      u8x8_cad_SendSequence(u8x8, arg_int == 0 ?
          u8x8_d_st7789_sleep_off_seq : u8x8_d_st7789_sleep_on_seq);
      break;

    case U8X8_MSG_DISPLAY_SET_FLIP_MODE:
    case U8X8_MSG_DISPLAY_SET_CONTRAST:
      break;

    case U8X8_MSG_DISPLAY_DRAW_TILE:
      u8x8_d_st7789_draw_tile(u8x8, arg_int, (u8x8_tile_t *)arg_ptr, y_offset);
      break;

    default:
      return 0;
  }
  return 1;
}

#else

/* dummy for linkage only */
static uint8_t u8x8_d_st7789_generic(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr, uint8_t y_offset, const u8x8_display_info_t *display_info)
{
  (void)u8x8;
  (void)msg;
  (void)arg_int;
  (void)arg_ptr;
  (void)y_offset;
  (void)display_info;
  return 0;
}

#endif /* U8X8_WITH_CLUT */

/* 240x240 */

static const u8x8_display_info_t u8x8_st7789_240x240_display_info = {
  /* chip_enable_level = */ 0,
  /* chip_disable_level = */ 1,
  /* post_chip_enable_wait_ns = */ 20,
  /* pre_chip_disable_wait_ns = */ 20,
  /* reset_pulse_width_ms = */ 10,
  /* post_reset_wait_ms = */ 120,
  /* sda_setup_time_ns = */ 20,
  /* sck_pulse_width_ns = */ 25,
  /* sck_clock_hz = */ 15000000UL,
  /* spi_mode = */ 0,
  /* i2c_bus_clock_100kHz = */ 0,
  /* data_setup_time_ns = */ 20,
  /* write_pulse_width_ns = */ 40,
  /* tile_width = */ 30, /* tile_height = */ 30,
  /* default_x_offset = */ 0, /* flipmode_x_offset = */ 0,
  /* pixel_width = */ 240, /* pixel_height = */ 240
};

#define U8X8_D_ST7789_240X240_Y_OFFSET 80

uint8_t u8x8_d_st7789_240x240(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  return u8x8_d_st7789_generic(u8x8, msg, arg_int, arg_ptr, U8X8_D_ST7789_240X240_Y_OFFSET, &u8x8_st7789_240x240_display_info);
}

/* 240x280 */

static const u8x8_display_info_t u8x8_st7789_240x280_display_info = {
  /* chip_enable_level = */ 0,
  /* chip_disable_level = */ 1,
  /* post_chip_enable_wait_ns = */ 20,
  /* pre_chip_disable_wait_ns = */ 20,
  /* reset_pulse_width_ms = */ 10,
  /* post_reset_wait_ms = */ 120,
  /* sda_setup_time_ns = */ 20,
  /* sck_pulse_width_ns = */ 25,
  /* sck_clock_hz = */ 15000000UL,
  /* spi_mode = */ 0,
  /* i2c_bus_clock_100kHz = */ 0,
  /* data_setup_time_ns = */ 20,
  /* write_pulse_width_ns = */ 40,
  /* tile_width = */ 30, /* tile_height = */ 35,
  /* default_x_offset = */ 0, /* flipmode_x_offset = */ 0,
  /* pixel_width = */ 240, /* pixel_height = */ 280
};

#define U8X8_D_ST7789_240X280_Y_OFFSET 20

uint8_t u8x8_d_st7789_240x280(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  return u8x8_d_st7789_generic(u8x8, msg, arg_int, arg_ptr, U8X8_D_ST7789_240X280_Y_OFFSET, &u8x8_st7789_240x280_display_info);
}

/* 240x320 */

static const u8x8_display_info_t u8x8_st7789_240x320_display_info = {
  /* chip_enable_level = */ 0,
  /* chip_disable_level = */ 1,
  /* post_chip_enable_wait_ns = */ 20,
  /* pre_chip_disable_wait_ns = */ 20,
  /* reset_pulse_width_ms = */ 10,
  /* post_reset_wait_ms = */ 120,
  /* sda_setup_time_ns = */ 20,
  /* sck_pulse_width_ns = */ 25,
  /* sck_clock_hz = */ 15000000UL,
  /* spi_mode = */ 0,
  /* i2c_bus_clock_100kHz = */ 0,
  /* data_setup_time_ns = */ 20,
  /* write_pulse_width_ns = */ 40,
  /* tile_width = */ 30, /* tile_height = */ 40,
  /* default_x_offset = */ 0, /* flipmode_x_offset = */ 0,
  /* pixel_width = */ 240, /* pixel_height = */ 320
};

#define U8X8_D_ST7789_240X320_Y_OFFSET 0

uint8_t u8x8_d_st7789_240x320(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  return u8x8_d_st7789_generic(u8x8, msg, arg_int, arg_ptr, U8X8_D_ST7789_240X320_Y_OFFSET, &u8x8_st7789_240x320_display_info);
}

/* 135x240 */

static const u8x8_display_info_t u8x8_st7789_135x240_display_info = {
  /* chip_enable_level = */ 0,
  /* chip_disable_level = */ 1,
  /* post_chip_enable_wait_ns = */ 20,
  /* pre_chip_disable_wait_ns = */ 20,
  /* reset_pulse_width_ms = */ 10,
  /* post_reset_wait_ms = */ 120,
  /* sda_setup_time_ns = */ 20,
  /* sck_pulse_width_ns = */ 25,
  /* sck_clock_hz = */ 15000000UL,
  /* spi_mode = */ 0,
  /* i2c_bus_clock_100kHz = */ 0,
  /* data_setup_time_ns = */ 20,
  /* write_pulse_width_ns = */ 40,
  /* tile_width = */ 17, /* tile_height = */ 30,
  /* default_x_offset = */ 53, /* flipmode_x_offset = */ 0,
  /* pixel_width = */ 135, /* pixel_height = */ 240
};

#define U8X8_D_ST7789_135X240_Y_OFFSET 40

uint8_t u8x8_d_st7789_135x240(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  return u8x8_d_st7789_generic(u8x8, msg, arg_int, arg_ptr, U8X8_D_ST7789_135X240_Y_OFFSET, &u8x8_st7789_135x240_display_info);
}

/* 172x320 */

static const u8x8_display_info_t u8x8_st7789_172x320_display_info = {
  /* chip_enable_level = */ 0,
  /* chip_disable_level = */ 1,
  /* post_chip_enable_wait_ns = */ 20,
  /* pre_chip_disable_wait_ns = */ 20,
  /* reset_pulse_width_ms = */ 10,
  /* post_reset_wait_ms = */ 120,
  /* sda_setup_time_ns = */ 20,
  /* sck_pulse_width_ns = */ 25,
  /* sck_clock_hz = */ 15000000UL,
  /* spi_mode = */ 0,
  /* i2c_bus_clock_100kHz = */ 0,
  /* data_setup_time_ns = */ 20,
  /* write_pulse_width_ns = */ 40,
  /* tile_width = */ 22, /* tile_height = */ 40,
  /* default_x_offset = */ 34, /* flipmode_x_offset = */ 0,
  /* pixel_width = */ 172, /* pixel_height = */ 320
};

#define U8X8_D_ST7789_172X320_Y_OFFSET 0

uint8_t u8x8_d_st7789_172x320(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  return u8x8_d_st7789_generic(u8x8, msg, arg_int, arg_ptr, U8X8_D_ST7789_172X320_Y_OFFSET, &u8x8_st7789_172x320_display_info);
}

/* 170x320 */

static const u8x8_display_info_t u8x8_st7789_170x320_display_info = {
  /* chip_enable_level = */ 0,
  /* chip_disable_level = */ 1,
  /* post_chip_enable_wait_ns = */ 20,
  /* pre_chip_disable_wait_ns = */ 20,
  /* reset_pulse_width_ms = */ 10,
  /* post_reset_wait_ms = */ 120,
  /* sda_setup_time_ns = */ 20,
  /* sck_pulse_width_ns = */ 25,
  /* sck_clock_hz = */ 15000000UL,
  /* spi_mode = */ 0,
  /* i2c_bus_clock_100kHz = */ 0,
  /* data_setup_time_ns = */ 20,
  /* write_pulse_width_ns = */ 40,
  /* tile_width = */ 22, /* tile_height = */ 40,
  /* default_x_offset = */ 35, /* flipmode_x_offset = */ 0,
  /* pixel_width = */ 170, /* pixel_height = */ 320
};

#define U8X8_D_ST7789_170X320_Y_OFFSET 0

uint8_t u8x8_d_st7789_170x320(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  return u8x8_d_st7789_generic(u8x8, msg, arg_int, arg_ptr, U8X8_D_ST7789_170X320_Y_OFFSET, &u8x8_st7789_170x320_display_info);
}

