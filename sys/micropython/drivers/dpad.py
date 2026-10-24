# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer

# Touch screen as a d-pad, as input events: the screen has five zones. A square
# in the center is "select". Outside the square, lines at 45 degrees through the
# center divide the screen in four: "up" above, "down" below, "left" and "right".
# The zones look the same when the screen is rotated by a quarter turn.
#
#   import cst816, dpad
#   touch = cst816.CST816(I2C(0), irq=Pin(0), mode=cst816.CHANGE)
#   pad = dpad.DPad(touch, 240, 240)
#   ev = pad.get()         # events.UP, DOWN, LEFT, RIGHT, SELECT, LONG or None
#
# width, height: size of the screen, in pixels, as the user sees it.
# rotation: 0, 1, 2 or 3, as in u8g2.R0..R3, for a screen rotated by rotation quarter
# turns clockwise, e.g. u8g2.init() with a _ROT display: DPad(touch, 240, 240, rotation=1).
# The touch panel does not rotate with the screen: with rotation=1, a touch above the center
# of the panel is "left" on the rotated screen. The rotation is set once, here.
# rotation and the parameters after it are keywords.
#
# Up, down, left and right act when the finger touches down, and repeat while
# it stays down. Select acts when the finger is lifted. Holding the center
# longer than long_ms gives LONG instead of SELECT. The zone is that of the
# first touch point; the finger can move after that.
#
# touch: a cst816.CST816 in mode CHANGE or TOUCH. Or any object with x, y and
# fingers, and a callback attribute, called when the touch changes. Optionally
# an update() method, to read the touch controller.
#
# Without polling, with asyncio: wait() returns when get() may return an event.
# Without a finger on the screen, wait() sleeps until the touch controller
# interrupts. With a finger, it sleeps until the interrupt or the next deadline,
# the next repeat or the long press, whichever comes first.
#
#   async def main():
#       while True:
#           ev = pad.get()
#           if ev is None:
#               await pad.wait()
#               continue
#           ...

import time
import asyncio
import events


class DPad:
    def __init__(self, touch, width, height, *, rotation=0, half=50, repeat_delay=400, repeat_period=120, long_ms=1000):
        # rotation: see above. events go through an events.Fifo, which rotates them.
        # half: half of the side of the select square, pixels
        # repeat_delay, repeat_period: ms until the first repeat, and between repeats
        # long_ms: ms of holding select until LONG
        self._touch = touch
        self._flag = asyncio.ThreadSafeFlag()
        touch.callback = self._flag.set
        self._fifo = events.Fifo(2, rotation)
        if rotation & 1:  # the touch panel is turned: its width is the height of the screen
            width, height = height, width
        self._cx = width // 2
        self._cy = height // 2
        self._half = half
        self._repeat_delay = repeat_delay
        self._repeat_period = repeat_period
        self._long_ms = long_ms
        self._down = False
        self._zone = None
        self._deadline = None  # ticks_ms of the next repeat or LONG, None: none
        self._long_sent = False

    def zone(self, x, y):
        # events.SELECT, UP, DOWN, LEFT or RIGHT for the point x, y of the touch panel.
        # not rotated: the rotation is applied when the event is put in the fifo.
        dx = x - self._cx
        dy = y - self._cy
        half = self._half
        if -half <= dx <= half and -half <= dy <= half:
            return events.SELECT
        if abs(dy) >= abs(dx):
            return events.UP if dy < 0 else events.DOWN
        return events.LEFT if dx < 0 else events.RIGHT

    async def wait(self):
        # asyncio: return when get() may return an event. no cpu use without touch.
        if self._deadline is None:
            await self._flag.wait()
            return
        ms = time.ticks_diff(self._deadline, time.ticks_ms())
        if ms > 0:
            try:
                await asyncio.wait_for_ms(self._flag.wait(), ms)
            except asyncio.TimeoutError:
                pass

    def get(self):
        # next event, or None. does not block.
        event = self._step()
        if event is not None:
            self._fifo.put(event)  # rotates UP, DOWN, LEFT and RIGHT
        return self._fifo.get()

    def _step(self):
        # the event of the touch panel, if any, not rotated
        t = self._touch
        now = time.ticks_ms()
        due = self._deadline is not None and time.ticks_diff(now, self._deadline) >= 0
        if due and hasattr(t, "update"):
            t.update()  # read the controller: in case an interrupt, "finger lifted", was lost
        if t.fingers:
            if not self._down:  # touch down
                self._down = True
                self._zone = self.zone(t.x, t.y)
                self._long_sent = False
                if self._zone == events.SELECT:
                    self._deadline = time.ticks_add(now, self._long_ms)
                    return None
                self._deadline = time.ticks_add(now, self._repeat_delay)
                return self._zone
            if due:  # held down
                if self._zone == events.SELECT:
                    self._long_sent = True
                    self._deadline = None
                    return events.LONG
                self._deadline = time.ticks_add(now, self._repeat_period)
                return self._zone
            return None
        if self._down:  # lifted
            self._down = False
            self._deadline = None
            if self._zone == events.SELECT and not self._long_sent:
                return events.SELECT
        return None
