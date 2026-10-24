# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer

# Display of the WeAct mini STM32U585: ST7789 240x280.
# "import display" initializes u8g2. Needed again after soft reset.
# Wiring: mpconfigboard.h.
#
# SPI(1) is on PA5 (SCK) and PA7 (MOSI). On stm32 the pins of a hardware SPI
# can not be chosen: SPI(1, sck=..., miso=...) raises ValueError.

from machine import Pin, SPI
import u8g2

spi = SPI(1, baudrate=20_000_000)
u8g2.init(spi, cs=Pin("A4"), dc=Pin("B1"), rst=Pin("B2"), bl=Pin("B0"),
          display=u8g2.ST7789_240X280)
