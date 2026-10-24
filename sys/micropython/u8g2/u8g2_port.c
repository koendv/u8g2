// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 Koen De Vleeschauwer

// u8x8 byte and gpio callbacks, portable.
// spi: any machine.SPI or machine.SoftSPI object, through the spi protocol.
// i2c: any machine.I2C or machine.SoftI2C object, through the i2c protocol.
// pins: machine.Pin objects, through mp_hal_pin_*.

#include <string.h>
#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "extmod/modmachine.h"
#include "modu8g2.h"

// keeps the bus and pin objects alive
MP_REGISTER_ROOT_POINTER(mp_obj_t u8g2_bus);
MP_REGISTER_ROOT_POINTER(mp_obj_t u8g2_pins[3]);

static const mp_machine_spi_p_t *u8g2_spi_p;
static const mp_machine_i2c_p_t *u8g2_i2c_p;
static mp_hal_pin_obj_t u8g2_pin_cs;
static mp_hal_pin_obj_t u8g2_pin_dc;
static mp_hal_pin_obj_t u8g2_pin_rst;
static bool u8g2_has_cs;
static bool u8g2_has_dc;
static bool u8g2_has_rst;

// u8x8 sends one i2c transfer as start, one or more sends, end. the transfer is
// collected here and written in one go. tested with all 122 i2c displays of u8g2:
// the longest transfer is 25 bytes. a longer one raises, it is not split.
#define U8G2_I2C_BUF_SIZE (64)
static uint8_t u8g2_i2c_buf[U8G2_I2C_BUF_SIZE];
static size_t u8g2_i2c_len;

static void u8g2_port_init_rst(mp_obj_t rst) {
    u8g2_has_rst = rst != mp_const_none;
    if (u8g2_has_rst) {
        u8g2_pin_rst = mp_hal_get_pin_obj(rst);
    }
}

void u8g2_port_init(mp_obj_t spi, mp_obj_t cs, mp_obj_t dc, mp_obj_t rst) {
    const mp_obj_type_t *type = mp_obj_get_type(spi);
    bool is_spi = false;
    #if MICROPY_PY_MACHINE_SPI
    is_spi |= type == &machine_spi_type;
    #endif
    #if MICROPY_PY_MACHINE_SOFTSPI
    is_spi |= type == &mp_machine_soft_spi_type;
    #endif
    if (!is_spi) {
        mp_raise_TypeError(MP_ERROR_TEXT("expecting SPI or SoftSPI"));
    }
    u8g2_spi_p = MP_OBJ_TYPE_GET_SLOT(type, protocol);
    u8g2_i2c_p = NULL;

    // raises if not a pin
    u8g2_has_cs = cs != mp_const_none;
    if (u8g2_has_cs) {
        u8g2_pin_cs = mp_hal_get_pin_obj(cs);
    }
    u8g2_has_dc = dc != mp_const_none;
    if (u8g2_has_dc) {
        u8g2_pin_dc = mp_hal_get_pin_obj(dc);
    }
    u8g2_port_init_rst(rst);

    MP_STATE_VM(u8g2_bus) = spi;
    MP_STATE_VM(u8g2_pins)[0] = cs;
    MP_STATE_VM(u8g2_pins)[1] = dc;
    MP_STATE_VM(u8g2_pins)[2] = rst;
}

void u8g2_port_init_i2c(mp_obj_t i2c, mp_obj_t rst) {
    const mp_obj_type_t *type = mp_obj_get_type(i2c);
    bool is_i2c = false;
    #if MICROPY_PY_MACHINE_I2C
    is_i2c |= type == &machine_i2c_type;
    #endif
    #if MICROPY_PY_MACHINE_SOFTI2C
    is_i2c |= type == &mp_machine_soft_i2c_type;
    #endif
    if (!is_i2c) {
        mp_raise_TypeError(MP_ERROR_TEXT("expecting I2C or SoftI2C"));
    }
    u8g2_i2c_p = MP_OBJ_TYPE_GET_SLOT(type, protocol);
    u8g2_spi_p = NULL;
    u8g2_has_cs = false;
    u8g2_has_dc = false;
    u8g2_port_init_rst(rst);

    MP_STATE_VM(u8g2_bus) = i2c;
    MP_STATE_VM(u8g2_pins)[0] = mp_const_none;
    MP_STATE_VM(u8g2_pins)[1] = mp_const_none;
    MP_STATE_VM(u8g2_pins)[2] = rst;
}

uint8_t u8g2_port_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
        case U8X8_MSG_BYTE_INIT:
            if (u8g2_has_cs) {
                mp_hal_pin_write(u8g2_pin_cs, u8x8->display_info->chip_disable_level);
            }
            break;
        case U8X8_MSG_BYTE_SET_DC:
            if (u8g2_has_dc) {
                mp_hal_pin_write(u8g2_pin_dc, arg_int);
            }
            break;
        case U8X8_MSG_BYTE_START_TRANSFER:
            if (u8g2_has_cs) {
                mp_hal_pin_write(u8g2_pin_cs, u8x8->display_info->chip_enable_level);
            }
            break;
        case U8X8_MSG_BYTE_SEND:
            u8g2_spi_p->transfer(MP_OBJ_TO_PTR(MP_STATE_VM(u8g2_bus)), arg_int, (const uint8_t *)arg_ptr, NULL);
            break;
        case U8X8_MSG_BYTE_END_TRANSFER:
            if (u8g2_has_cs) {
                mp_hal_pin_write(u8g2_pin_cs, u8x8->display_info->chip_disable_level);
            }
            break;
        default:
            return 0;
    }
    return 1;
}

uint8_t u8g2_port_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
        case U8X8_MSG_BYTE_INIT:
        case U8X8_MSG_BYTE_SET_DC:
            // dc is the control byte, sent by the cad
            break;
        case U8X8_MSG_BYTE_START_TRANSFER:
            u8g2_i2c_len = 0;
            break;
        case U8X8_MSG_BYTE_SEND:
            if (u8g2_i2c_len + arg_int > U8G2_I2C_BUF_SIZE) {
                mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("u8g2 i2c transfer too long"));
            }
            memcpy(u8g2_i2c_buf + u8g2_i2c_len, arg_ptr, arg_int);
            u8g2_i2c_len += arg_int;
            break;
        case U8X8_MSG_BYTE_END_TRANSFER: {
            mp_machine_i2c_buf_t buf = {.len = u8g2_i2c_len, .buf = u8g2_i2c_buf};
            // u8x8 keeps the 8 bit write address
            int ret = u8g2_i2c_p->transfer(MP_OBJ_TO_PTR(MP_STATE_VM(u8g2_bus)),
                u8x8_GetI2CAddress(u8x8) >> 1, 1, &buf, MP_MACHINE_I2C_FLAG_STOP);
            if (ret < 0) {
                mp_raise_OSError(-ret);
            }
            if ((size_t)ret != u8g2_i2c_len) {
                // the display did not acknowledge: wrong address, or not connected
                mp_raise_OSError(MP_ENODEV);
            }
            break;
        }
        default:
            return 0;
    }
    return 1;
}

uint8_t u8g2_port_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
            if (u8g2_has_cs) {
                mp_hal_pin_output(u8g2_pin_cs);
                mp_hal_pin_write(u8g2_pin_cs, 1);
            }
            if (u8g2_has_dc) {
                mp_hal_pin_output(u8g2_pin_dc);
                mp_hal_pin_write(u8g2_pin_dc, 1);
            }
            if (u8g2_has_rst) {
                mp_hal_pin_output(u8g2_pin_rst);
                mp_hal_pin_write(u8g2_pin_rst, 1);
            }
            break;
        case U8X8_MSG_DELAY_MILLI:
            mp_hal_delay_ms(arg_int);
            break;
        case U8X8_MSG_DELAY_10MICRO:
            mp_hal_delay_us(arg_int * 10);
            break;
        case U8X8_MSG_DELAY_100NANO:
            mp_hal_delay_us((arg_int + 9) / 10);
            break;
        case U8X8_MSG_DELAY_NANO:
            break;
        case U8X8_MSG_GPIO_RESET:
            if (u8g2_has_rst) {
                mp_hal_pin_write(u8g2_pin_rst, arg_int);
            }
            break;
        case U8X8_MSG_GPIO_CS:
            if (u8g2_has_cs) {
                mp_hal_pin_write(u8g2_pin_cs, arg_int);
            }
            break;
        case U8X8_MSG_GPIO_DC:
            if (u8g2_has_dc) {
                mp_hal_pin_write(u8g2_pin_dc, arg_int);
            }
            break;
        default:
            // menu pins etc: not used
            u8x8_SetGPIOResult(u8x8, 1);
            break;
    }
    return 1;
}
