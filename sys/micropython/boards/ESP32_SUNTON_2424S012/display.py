# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer

# Display of the Sunton ESP32-2424S012: round GC9A01 240x240, no reset line.
# "import display" initializes u8g2. Needed again after soft reset.
# Pin names: pins.csv. Pin list: mpconfigboard.h.

from machine import Pin, SPI
import u8g2

P = Pin.board

spi = SPI(1, baudrate=40_000_000, sck=P.LCD_SCK, mosi=P.LCD_MOSI, miso=None)
u8g2.init(spi, cs=Pin(P.LCD_CS), dc=Pin(P.LCD_DC), bl=Pin(P.LCD_BL),
          display=u8g2.GC9A01_240X240)
