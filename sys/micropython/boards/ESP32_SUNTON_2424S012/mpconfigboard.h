// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 Koen De Vleeschauwer

// Sunton ESP32-2424S012, u8g2 + clut, mui.
// display wiring: display.py, frozen. "import display" calls u8g2.init().
// pin names: pins.csv, as Pin.board.LCD_CS etc.
// pins from the CircuitPython board definition, sunton_esp32_2424S012
//
//   LCD GC9A01 240x240    SPI SCK 6, MOSI 7, DC 2, CS 10, backlight 3 (PWM,
//                         active high, MOSFET), no reset line
//   I2C bus               SCL 5, SDA 4: touch CST816
//   touch                 INT 0, RST 1
//   keys                  BOOT 9
//   UART0                 TX 21, RX 20
//
// DC is also the default MISO pin of SPI(1): the LCD is write only, so
// display.py passes miso=None.

#define MICROPY_HW_BOARD_NAME               "Sunton ESP32-2424S012"
#define MICROPY_HW_MCU_NAME                 "ESP32-C3FN4"

// default pins: I2C(0) is the board's I2C bus, SPI(1) the LCD bus
#define MICROPY_HW_I2C0_SCL                 (5)
#define MICROPY_HW_I2C0_SDA                 (4)
#define MICROPY_HW_SPI1_SCK                 (6)
#define MICROPY_HW_SPI1_MOSI                (7)
#define MICROPY_HW_SPI1_MISO                (-1)
