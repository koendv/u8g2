# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer

# u8g2 and mui MicroPython modules, cmake based ports (esp32, rp2):
#   make USER_C_MODULES=<u8g2>/sys/micropython/micropython.cmake
# make based ports (stm32, ...):
#   make USER_C_MODULES=<u8g2>/sys/micropython

include(${CMAKE_CURRENT_LIST_DIR}/u8g2/micropython.cmake)
