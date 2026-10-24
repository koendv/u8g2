// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 Koen De Vleeschauwer

// WeAct Studio mini STM32U585 with an ST7789 240x280, u8g2 + clut, mui.
// the board of the MicroPython port. display wiring: display.py, frozen.
// "import display" calls u8g2.init().
//
//   ST7789   SCL PA5 (SPI1 SCK), SDA PA7 (SPI1 MOSI), RES PB2, DC PB1,
//            CS PA4 (gpio), BLK PB0 (TIM3 channel 3, pwm)

#include "boards/WEACTSTUDIO_MINI_STM32U585/mpconfigboard.h"
