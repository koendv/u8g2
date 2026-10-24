# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer

# CST816 touch controller of the Waveshare ESP32-S3-Touch-LCD-1.69.
# Pin names: pins.csv. Pin list: mpconfigboard.h.
#
#   import touch
#   t = touch.init()       # gestures: t.get() is events.UP, DOWN, LEFT, RIGHT, SELECT, ...
#   t.deinit()
#
# callback: function called after each interrupt of the controller, see cst816.py.
#
# The controller does not follow set_flip_mode() or a _ROT display: the swipes point the
# other way, and x and y are panel coordinates. rotation=0..3 turns the swipes: see
# cst816.py. Use the same rotation for dpad.DPad.

from machine import I2C, Pin
import cst816


def init(mode=cst816.MOTION, callback=None, rotation=0):
    # see cst816.CST816 for the modes and the rotation. I2C(0) is the board's I2C bus.
    P = Pin.board
    return cst816.CST816(I2C(0), irq=Pin(P.TP_INT), rst=Pin(P.TP_RST), mode=mode, callback=callback, rotation=rotation)
