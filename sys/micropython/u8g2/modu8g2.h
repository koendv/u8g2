// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 Koen De Vleeschauwer

#ifndef MICROPY_INCLUDED_MODU8G2_H
#define MICROPY_INCLUDED_MODU8G2_H

#include "py/obj.h"
#include "u8g2.h"

// the one display
extern u8g2_t mp_u8g2;

// true after u8g2.init(), false after soft reset
bool u8g2_is_ready(void);

// font object: wraps a u8g2 font array in flash
typedef struct _u8g2_font_obj_t {
    mp_obj_base_t base;
    const uint8_t *data;
} u8g2_font_obj_t;

extern const mp_obj_type_t u8g2_font_type;

// returns font data, raises TypeError if not a font
const uint8_t *u8g2_font_get(mp_obj_t font_in);

// u8g2_port.c: spi, i2c and gpio through the micropython hal
// spi: cs and dc may be None. i2c: rst may be None.
void u8g2_port_init(mp_obj_t spi, mp_obj_t cs, mp_obj_t dc, mp_obj_t rst);
void u8g2_port_init_i2c(mp_obj_t i2c, mp_obj_t rst);
uint8_t u8g2_port_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
uint8_t u8g2_port_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
uint8_t u8g2_port_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

#endif // MICROPY_INCLUDED_MODU8G2_H
