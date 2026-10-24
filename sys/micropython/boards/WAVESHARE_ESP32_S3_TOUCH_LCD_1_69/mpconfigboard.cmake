# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer

# Waveshare ESP32-S3-Touch-LCD-1.69: ESP32-S3, 16 MiB flash, 8 MiB octal PSRAM,
# ST7789V2 240x280, CST816 touch. Console on the native USB port.

include(boards/mpconfigboard_esp32s3_common.cmake)

list(APPEND SDKCONFIG_DEFAULTS
    boards/sdkconfig.240mhz
    boards/sdkconfig.spiram_oct
    boards/sdkconfig.flash_qio_80m
)

# power hold at startup
set(MICROPY_SOURCE_BOARD ${MICROPY_BOARD_DIR}/board_init.c)

# frozen display.py and touch.py: this board's wiring
set(MICROPY_FROZEN_MANIFEST ${MICROPY_BOARD_DIR}/manifest.py)

# u8g2 and mui modules
set(USER_C_MODULES ${MICROPY_BOARD_DIR}/../../micropython.cmake)
