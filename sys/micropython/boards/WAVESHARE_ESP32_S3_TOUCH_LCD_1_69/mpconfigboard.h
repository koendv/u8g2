// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 Koen De Vleeschauwer

// Waveshare ESP32-S3-Touch-LCD-1.69, u8g2 + clut, mui.
// display wiring: display.py, frozen. "import display" calls u8g2.init().
// pin names: pins.csv, as Pin.board.LCD_CS etc.
// pins from the CircuitPython board definition, waveshare_esp32_s3_touch_lcd_1_69.
// That definition is for the board with 8 MiB octal PSRAM; the first hardware
// revision (V1) has the power key, power hold, buzzer and RTC interrupt on other pins.
//
//   LCD ST7789V2 240x280  SPI SCK 6, MOSI 7, DC 4, CS 5, RST 8, backlight 15
//   I2C bus               SCL 10, SDA 11: touch CST816 (0x15), RTC PCF85063 (0x51),
//                         IMU QMI8658 (0x6B)
//   touch                 INT 14, RST 13
//   RTC                   INT 39
//   power                 PWR key 40 (low while pressed), SYS_EN 41 (power hold)
//   buzzer                42
//   battery voltage       ADC 1
//   keys                  BOOT 0
//
// The PWR key switches the board on. SYS_EN high keeps it on from the battery
// after the key is released: board_init.c sets it at startup. Pin.board.SYS_EN
// low switches the board off.

#define MICROPY_HW_BOARD_NAME               "Waveshare ESP32-S3-Touch-LCD-1.69"
#define MICROPY_HW_MCU_NAME                 "ESP32-S3"

#define MICROPY_BOARD_STARTUP               waveshare_s3_touch_lcd_1_69_board_startup
void waveshare_s3_touch_lcd_1_69_board_startup(void);

// default pins: I2C(0) is the board's I2C bus, SPI(1) the LCD bus
#define MICROPY_HW_I2C0_SCL                 (10)
#define MICROPY_HW_I2C0_SDA                 (11)
#define MICROPY_HW_SPI1_SCK                 (6)
#define MICROPY_HW_SPI1_MOSI                (7)
#define MICROPY_HW_SPI1_MISO                (-1)
