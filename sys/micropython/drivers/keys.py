# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer

# Keyboard input over the REPL (serial or USB), as input events.
#
#   w, arrow up       UP
#   s, arrow down     DOWN
#   a, arrow left     LEFT
#   d, arrow right    RIGHT
#   space, enter      SELECT
#   q, ctrl-c         QUIT
#
#   import keys
#   with keys.Keys() as k:
#       ev = k.get()       # events.UP, ... or None
#
# While active, ctrl-c does not raise KeyboardInterrupt: it is read as QUIT.
# The REPL setting is restored on exit.
#
# Without polling, with asyncio: wait() sleeps until a key is pressed.
#
#   async def main():
#       with keys.Keys() as k:
#           while True:
#               ev = k.get()
#               if ev is None:
#                   await k.wait()
#                   continue
#               ...

import sys
import select
import micropython
import events

_KEYS = {
    "w": events.UP,
    "s": events.DOWN,
    "a": events.LEFT,
    "d": events.RIGHT,
    " ": events.SELECT,
    "\r": events.SELECT,
    "\n": events.SELECT,
    "q": events.QUIT,
    "\x03": events.QUIT,
}

# arrow keys: ESC [ A..D or ESC O A..D
_ARROWS = {"A": events.UP, "B": events.DOWN, "C": events.RIGHT, "D": events.LEFT}


class Keys:
    def __init__(self, fifo_size=8):
        self.fifo = events.Fifo(fifo_size)
        self._poll = select.poll()
        self._poll.register(sys.stdin, select.POLLIN)
        self._esc = 0  # 0: normal, 1: after ESC, 2: after ESC [ or ESC O
        self._cr = False  # previous key was CR: ignore LF of CR LF
        # raw bytes: text mode sys.stdin turns CR into LF
        self._stdin = getattr(sys.stdin, "buffer", sys.stdin)

    def start(self):
        micropython.kbd_intr(-1)

    def stop(self):
        micropython.kbd_intr(3)

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, *args):
        self.stop()

    def _key(self, c):
        cr, self._cr = self._cr, c == "\r"
        if c == "\n" and cr:
            return
        if self._esc == 1:
            self._esc = 2 if c in "[O" else 0
            return
        if self._esc == 2:
            self._esc = 0
            event = _ARROWS.get(c)
        elif c == "\x1b":
            self._esc = 1
            return
        else:
            event = _KEYS.get(c.lower())
        if event is not None:
            self.fifo.put(event)

    async def wait(self):
        # asyncio: return when get() may return an event. sleeps until a key is
        # pressed. blocks other asyncio tasks while it sleeps.
        if len(self.fifo):
            return
        self._poll.poll(-1)

    def get(self):
        # next event, or None. does not block.
        while self._poll.poll(0):
            c = self._stdin.read(1)
            self._key(chr(c[0]) if isinstance(c, bytes) else c)
        return self.fifo.get()
