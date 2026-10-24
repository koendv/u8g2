# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer

# Analog clock on a 240x240 display, with a color cycling ring. On a display higher
# than 240 the dial is at the top, and the time is also shown in digits below.
# Port of the ClutClock Arduino example (sys/arduino/u8g2_full_buffer/ClutClock).
# Starts at a random time.
#
#   >>> import demo_clock
#   >>> demo_clock.run()       # ctrl-c to stop
#
# The dial is a 1bpp drawing. Each 8x8 tile has a clut index (set_clut_region),
# each clut entry is a foreground/background color pair (set_clut_color):
# tiles around the edge get CLUT_RING, whose color changes every second, the
# tiles inside get CLUT_HANDS.

import os
import time
import display  # noqa: F401, u8g2.init() with this board's wiring
import u8g2

# dial geometry, pixels from center
R_TICK_OUTER = 116
R_TICK_MINUTE = 110
R_TICK_HOUR = 101
R_NUMERAL = 88
R_RING_ZONE = 80  # tiles with center beyond this radius get the ring color
L_HOUR = 50
L_MINUTE = 78
L_SECOND = 94
L_SECOND_TAIL = 16
W_HOUR = 4  # half width at the base
W_MINUTE = 3
R_HUB = 4

FONT_NUMERAL = u8g2.font_profont17_tf

# clut indices
CLUT_RING = 1
CLUT_HANDS = 2

COLOR_BLACK = 0x000000
COLOR_WHITE = 0xFFFFFF
COLOR_CYAN = 0x00C0FF

# sin and cos of positions 0..7 (0..42 degrees), scaled by 1024
SIN_TABLE = (0, 107, 213, 316, 416, 512, 602, 685)
COS_TABLE = (1024, 1018, 1002, 974, 935, 887, 828, 761)

DIAL = 240  # the dial is DIAL x DIAL pixels, at the top of the display
cx = u8g2.get_display_width() // 2
cy = DIAL // 2
digital_y = DIAL + 28  # baseline of the time in digits. below the dial, if there is room
FONT_DIGITAL = u8g2.font_profont22_tf


def rgb_cycle(t):
    # smoothly changing color, cycles every minute. returns 0xRRGGBB
    t = t * 1536 // 60
    t %= 1536  # t: 0..1535, then wraps
    segment = t >> 8  # 0..5
    x = t & 255  # 0..255
    if segment == 0:
        r, g, b = 255, x, 0  # red -> yellow
    elif segment == 1:
        r, g, b = 255 - x, 255, 0  # yellow -> green
    elif segment == 2:
        r, g, b = 0, 255, x  # green -> cyan
    elif segment == 3:
        r, g, b = 0, 255 - x, 255  # cyan -> blue
    elif segment == 4:
        r, g, b = x, 0, 255  # blue -> magenta
    else:
        r, g, b = 255, 0, 255 - x  # magenta -> red
    return (r << 16) | (g << 8) | b


def sin_cos(pos):
    # sin and cos of position pos (0..59, clockwise from 12), scaled by 1024
    q, r = divmod(pos, 15)
    if r <= 7:
        a, b = SIN_TABLE[r], COS_TABLE[r]
    else:  # sin(90 - x) = cos x
        a, b = COS_TABLE[15 - r], SIN_TABLE[15 - r]
    if q == 0:
        return a, b
    if q == 1:  # sin(x + 90) = cos x, cos(x + 90) = -sin x
        return b, -a
    if q == 2:
        return -a, -b
    return -b, a


def descale(v):
    # v / 1024, rounded
    return (v + 512) >> 10


def polar(pos, r, off):
    # point at radius r at position pos, shifted sideways by off pixels
    s, c = sin_cos(pos)
    return cx + descale(r * s + off * c), cy + descale(-r * c + off * s)


def set_clut_zones():
    # tile colors: outside R_RING_ZONE CLUT_RING, inside CLUT_HANDS
    for ty in range(u8g2.get_rows()):
        for tx in range(u8g2.get_cols()):
            dx = tx * 8 + 4 - cx
            dy = ty * 8 + 4 - cy
            ring = dx * dx + dy * dy >= R_RING_ZONE * R_RING_ZONE
            u8g2.set_clut_region(tx, ty, 1, 1, CLUT_RING if ring else CLUT_HANDS)


def draw_radial(pos, r0, r1, off):
    x0, y0 = polar(pos, r0, off)
    x1, y1 = polar(pos, r1, off)
    u8g2.draw_line(x0, y0, x1, y1)


def draw_hand(pos, length, w):
    # watch hand: tip at length, base of half width w at the center
    xt, yt = polar(pos, length, 0)
    xl, yl = polar(pos, 0, -w)
    xr, yr = polar(pos, 0, w)
    u8g2.draw_triangle(xt, yt, xl, yl, xr, yr)


def draw_dial():
    for i in range(60):
        if i % 5 == 0:
            for off in (-1, 0, 1):
                draw_radial(i, R_TICK_HOUR, R_TICK_OUTER, off)
        else:
            draw_radial(i, R_TICK_MINUTE, R_TICK_OUTER, 0)

    u8g2.set_font(FONT_NUMERAL)
    ascent = u8g2.get_ascent()
    for i, numeral in enumerate(("12", "3", "6", "9")):
        x, y = polar(i * 15, R_NUMERAL, 0)
        u8g2.draw_str(x - u8g2.get_str_width(numeral) // 2, y + ascent // 2, numeral)


def draw_clock(secs):
    hours = (secs // 3600) % 24
    minutes = (secs // 60) % 60
    seconds = secs % 60
    hour_pos = (hours % 12) * 5 + minutes // 12

    u8g2.clear_buffer()
    draw_dial()
    draw_hand(hour_pos, L_HOUR, W_HOUR)
    draw_hand(minutes, L_MINUTE, W_MINUTE)
    draw_radial(seconds, -L_SECOND_TAIL, L_SECOND, 0)
    u8g2.draw_disc(cx, cy, R_HUB)
    if u8g2.get_display_height() >= DIAL + 40:
        text = "%02d:%02d:%02d" % (hours, minutes, seconds)
        u8g2.set_font(FONT_DIGITAL)
        u8g2.draw_str(cx - u8g2.get_str_width(text) // 2, digital_y, text)
    u8g2.send_buffer()


def run():
    if u8g2.get_display_width() < DIAL or u8g2.get_display_height() < DIAL:
        raise RuntimeError("needs a display of at least 240x240")
    u8g2.set_clut_color(CLUT_RING, COLOR_CYAN, COLOR_BLACK)
    u8g2.set_clut_color(CLUT_HANDS, COLOR_WHITE, COLOR_BLACK)
    set_clut_zones()

    start_secs = int.from_bytes(os.urandom(4), "little") % 86400  # random start time
    t0 = time.ticks_ms()
    while True:
        elapsed = time.ticks_diff(time.ticks_ms(), t0)
        secs = start_secs + elapsed // 1000
        u8g2.set_clut_color(CLUT_RING, rgb_cycle(secs), COLOR_BLACK)
        draw_clock(secs)
        elapsed = time.ticks_diff(time.ticks_ms(), t0)
        time.sleep_ms(1000 - elapsed % 1000)  # wait until next second


if __name__ == "__main__":
    run()
