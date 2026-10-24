# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer

# Sunton ESP32-2424S012: ESP32-C3FN4 (4 MiB flash), round GC9A01 240x240,
# CST816 touch. Console on the native USB serial/JTAG port.

include(boards/mpconfigboard_esp32c3_common.cmake)

# frozen display.py: this board's display wiring
set(MICROPY_FROZEN_MANIFEST ${MICROPY_BOARD_DIR}/manifest.py)

# u8g2 and mui modules
set(USER_C_MODULES ${MICROPY_BOARD_DIR}/../../micropython.cmake)
