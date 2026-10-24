# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer

# Input events shared by input sources (touch gestures, keyboard) and the
# code that acts on them, e.g. a mui menu.

# The four directions are numbered counter-clockwise, 0 to 3, so that rotating the
# screen by a quarter turn is an addition: see Fifo.
UP = 0
LEFT = 1
DOWN = 2
RIGHT = 3
SELECT = 4
DOUBLE = 5
LONG = 6
QUIT = 7
NONE = 8

NAMES = ("UP", "LEFT", "DOWN", "RIGHT", "SELECT", "DOUBLE", "LONG", "QUIT", "NONE")


class Fifo:
    # small ring buffer of events. put() does not allocate, so it can be
    # called from interrupt or scheduled handlers. events are dropped when full.
    #
    # rotation: 0, 1, 2 or 3. The screen is turned by rotation quarter turns clockwise,
    # as in u8g2.R0..R3. A touch source reports directions of the touch panel: with
    # rotation 1 a swipe up on the panel is a swipe left on the rotated screen.
    # put() turns UP, LEFT, DOWN and RIGHT; other events are not changed.

    def __init__(self, size=8, rotation=0):
        if not 0 <= rotation <= 3:
            raise ValueError("rotation 0..3")
        self._rotation = rotation
        self._buf = bytearray(size)
        self._size = size
        self._rd = 0
        self._wr = 0

    def put(self, event):
        if event < 4:
            event = (event + self._rotation) & 3
        nxt = (self._wr + 1) % self._size
        if nxt == self._rd:
            return False
        self._buf[self._wr] = event
        self._wr = nxt
        return True

    def get(self):
        # next event, or None
        if self._rd == self._wr:
            return None
        event = self._buf[self._rd]
        self._rd = (self._rd + 1) % self._size
        return event

    def clear(self):
        self._rd = self._wr

    def __len__(self):
        return (self._wr - self._rd) % self._size
