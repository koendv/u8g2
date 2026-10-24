# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer

# Display of the Waveshare ESP32-S3-Touch-LCD-1.69: ST7789V2 240x280.
# "import display" initializes u8g2. Needed again after soft reset.
# Pin names: pins.csv. Pin list: mpconfigboard.h.

from machine import Pin, SPI
import u8g2

P = Pin.board

spi = SPI(1, baudrate=40_000_000, sck=P.LCD_SCK, mosi=P.LCD_MOSI, miso=None)
u8g2.init(spi, cs=Pin(P.LCD_CS), dc=Pin(P.LCD_DC), rst=Pin(P.LCD_RST), bl=Pin(P.LCD_BL),
          display=u8g2.ST7789_240X280)
