// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 Koen De Vleeschauwer

// Waveshare ESP32-S3-Touch-LCD-1.69: power hold.
// On battery, the PWR key powers the board only while it is pressed. SYS_EN high
// keeps the board on after the key is released, so set it before anything else.

#include "py/mphal.h"
#include "driver/gpio.h"

#define SYS_EN_PIN (GPIO_NUM_41)

void waveshare_s3_touch_lcd_1_69_board_startup(void) {
    gpio_set_level(SYS_EN_PIN, 1);
    gpio_set_direction(SYS_EN_PIN, GPIO_MODE_OUTPUT);
    boardctrl_startup();
}
