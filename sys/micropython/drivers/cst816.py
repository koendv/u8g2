# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer

# CST816 capacitive touch controller (CST716, CST816S, CST816T, CST816D).
# MicroPython port of the Arduino library https://github.com/koendv/cst816t
#
# Literature: DS-CST816T-V2.2 datasheet, AN-CST816T-v1 register description.
#
#   from machine import I2C, Pin
#   import cst816, events
#   touch = cst816.CST816(I2C(0), irq=Pin(11), mode=cst816.MOTION)
#   ev = touch.get()        # events.UP, DOWN, LEFT, RIGHT, SELECT, DOUBLE, LONG or None
#
# rotation: 0, 1, 2 or 3, as in u8g2.R0..R3, for a screen rotated by rotation quarter turns
# clockwise. Swipes are reported in the rotated screen: with rotation=1, a swipe up on the
# panel is a swipe left. x and y are not rotated: they are panel coordinates.
#
# Without polling, with asyncio: the loop sleeps until the controller interrupts.
#
#   async def main():
#       while True:
#           ev = touch.get()
#           if ev is None:
#               await touch.wait()     # returns when there may be an event
#               continue
#           ...
#
# callback: called, without arguments, after each interrupt, once x, y, fingers and gesture
# have been updated. Not in interrupt context: it runs as a scheduled function.
#
# Modes:
#   TOUCH   interrupt every 10 ms while touched. x, y, fingers only.
#   CHANGE  interrupt when the touch changes. x, y, fingers only.
#   FAST    interrupt after a click or swipe. no wait for double click/long press.
#   MOTION  interrupt after click, double click, swipe or long press.
# Gestures are decoded in the touch controller. Gesture events go into a fifo.

import micropython
import time
from machine import Pin
import events

ADDR = 0x15

TOUCH = 0
CHANGE = 1
FAST = 2
MOTION = 3

_REG_GESTURE_ID = 0x01  # gesture, fingers, x high, x low, y high, y low
_REG_CHIP_ID = 0xA7  # chip id, project id, firmware version, factory id
_REG_LONG_PRESS_TICK = 0xEB
_REG_MOTION_MASK = 0xEC
_REG_IRQ_CTL = 0xFA
_REG_DIS_AUTOSLEEP = 0xFE

_MOTION_MASK_DOUBLE_CLICK = 0x01

_IRQ_EN_TOUCH = 0x40
_IRQ_EN_CHANGE = 0x20
_IRQ_EN_MOTION = 0x10
_IRQ_EN_LONGPRESS = 0x01

CHIPS = {0x20: "CST716", 0xB4: "CST816S", 0xB5: "CST816T", 0xB6: "CST816D"}

# controller gesture id -> event, for a finger moving on the panel.
# Measured on the CST816D of the Sunton ESP32-2424S012: 0x03 and 0x04 are a finger moving to
# smaller and larger x, left and right. The controller's "up" 0x01 is a finger moving to larger y,
# towards the bottom of the panel, and "down" 0x02 the other way: the entries for 0x01 and 0x02
# are swapped here. If swipes point the wrong way on your panel, edit this table.
GESTURES = {
    0x01: events.DOWN,
    0x02: events.UP,
    0x03: events.LEFT,
    0x04: events.RIGHT,
    0x05: events.SELECT,
    0x0B: events.DOUBLE,
    0x0C: events.LONG,
}


class CST816:
    def __init__(self, i2c, irq=None, rst=None, mode=MOTION, addr=ADDR, fifo_size=8, callback=None, rotation=0):
        # i2c: machine.I2C. irq: Pin of the interrupt line, None to poll.
        # rst: Pin of the reset line, or None.
        # callback: function to call after each interrupt, see above.
        self.i2c = i2c
        self.callback = callback
        self._flag = None  # asyncio.ThreadSafeFlag, made by wait()
        self.addr = addr
        self.fifo = events.Fifo(fifo_size, rotation)
        self.gesture = 0
        self.fingers = 0
        self.x = 0
        self.y = 0
        self._buf = bytearray(6)
        self._old = bytearray(6)
        self._read_cb = self._read  # bound method, created once: no allocation in the isr

        if rst is not None:
            rst.init(Pin.OUT, value=1)
            time.sleep_ms(100)
            rst(0)
            time.sleep_ms(10)
            rst(1)
            time.sleep_ms(100)

        try:
            info = i2c.readfrom_mem(addr, _REG_CHIP_ID, 4)
        except OSError:
            raise OSError("no CST816 at i2c address 0x%02x" % addr)
        self.chip_id = info[0]
        self.firmware_version = info[2]

        irq_en = (_IRQ_EN_TOUCH, _IRQ_EN_CHANGE, _IRQ_EN_MOTION, _IRQ_EN_MOTION | _IRQ_EN_LONGPRESS)[mode]
        motion_mask = _MOTION_MASK_DOUBLE_CLICK if mode == MOTION else 0
        i2c.writeto_mem(addr, _REG_IRQ_CTL, bytes((irq_en,)))
        i2c.writeto_mem(addr, _REG_MOTION_MASK, bytes((motion_mask,)))
        if self.chip_id != 0x20:
            # CST716 has no auto sleep register
            i2c.writeto_mem(addr, _REG_DIS_AUTOSLEEP, b"\xff")

        self._irq = irq
        if irq is not None:
            irq.init(Pin.IN, Pin.PULL_UP)
            irq.irq(trigger=Pin.IRQ_FALLING, handler=self._isr)

    def version(self):
        return "%s firmware %d" % (CHIPS.get(self.chip_id, "chip 0x%02x" % self.chip_id), self.firmware_version)

    def _isr(self, pin):
        # no i2c in interrupt context: read in a scheduled callback
        try:
            micropython.schedule(self._read_cb, None)
        except RuntimeError:
            pass  # schedule queue full: event lost

    def _read(self, _):
        # scheduled after an interrupt
        self.update()
        if self._flag is not None:
            self._flag.set()
        if self.callback is not None:
            self.callback()

    def update(self):
        # read x, y, fingers and gesture from the controller. true if read.
        try:
            self.i2c.readfrom_mem_into(self.addr, _REG_GESTURE_ID, self._buf)
        except OSError:
            return False
        self._parse()
        return True

    async def wait(self):
        # asyncio: return when get() may return an event. sleeps while there is no
        # interrupt. get() returns None when woken without an event.
        if len(self.fifo):
            return
        if self._flag is None:
            import asyncio

            self._flag = asyncio.ThreadSafeFlag()
        await self._flag.wait()

    def _parse(self):
        b = self._buf
        self.gesture = b[0]
        self.fingers = b[1]
        self.x = ((b[2] & 0x0F) << 8) | b[3]
        self.y = ((b[4] & 0x0F) << 8) | b[5]
        event = GESTURES.get(b[0])
        if event is not None:
            self.fifo.put(event)

    def get(self):
        # next gesture event, or None. without irq pin: poll the controller.
        if self._irq is None:
            try:
                self.i2c.readfrom_mem_into(self.addr, _REG_GESTURE_ID, self._buf)
            except OSError:
                return self.fifo.get()
            if self._buf != self._old:
                self._old[:] = self._buf
                self._parse()
        return self.fifo.get()

    def deinit(self):
        if self._irq is not None:
            self._irq.irq(handler=None)
