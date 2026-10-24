# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer

# WeAct Studio mini STM32U585 with an ST7789 240x280 display.
# The board of the MicroPython port, plus a frozen display.py.

include boards/WEACTSTUDIO_MINI_STM32U585/mpconfigboard.mk

# frozen display.py: this board's display wiring
FROZEN_MANIFEST = $(BOARD_DIR)/manifest.py
