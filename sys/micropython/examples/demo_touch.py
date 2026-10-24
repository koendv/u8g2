# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer

# Touch screen test: a dot follows the finger. The text shows what the CST816
# controller reports: x, y and the number of fingers. For gestures, see demo_mui.
#
#   >>> import demo_touch
#   >>> demo_touch.run()       # ctrl-c to stop
#
# If the dot is not under the finger, the touch axes are not those of the display.

import asyncio
import display  # noqa: F401, u8g2.init() with this board's wiring
import u8g2
import touch
import cst816

CLUT_BODY = 0

R_DOT = 8


def show(t):
    u8g2.clear_buffer()
    u8g2.set_font(u8g2.font_profont17_tf)
    u8g2.draw_str(60, 96, "x %3d  y %3d" % (t.x, t.y))
    u8g2.draw_str(60, 118, "fingers %d" % t.fingers)
    if t.fingers:
        u8g2.draw_disc(t.x, t.y, R_DOT)
    u8g2.send_buffer()


async def _loop():
    flag = asyncio.ThreadSafeFlag()
    # TOUCH: an interrupt every 10 ms while touched, with the position. none without touch.
    t = touch.init(cst816.TOUCH, callback=flag.set)
    print(t.version())
    old = None
    show(t)
    try:
        while True:
            await flag.wait()  # sleeps until the touch controller interrupts
            state = (t.x, t.y, t.fingers)
            if state != old:
                old = state
                show(t)
                if t.fingers:
                    print(t.x, t.y)
    finally:
        t.deinit()


def run():
    u8g2.set_clut_color(CLUT_BODY, 0xFFFFFF, 0x000000)
    u8g2.set_clut_region(0, 0, u8g2.get_cols(), u8g2.get_rows(), CLUT_BODY)
    try:
        asyncio.run(_loop())
    finally:
        asyncio.new_event_loop()  # after ctrl-c: ready for the next run


if __name__ == "__main__":
    run()
