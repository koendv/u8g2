# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer

# Basic u8g2 graphics: boxes, circles, lines, triangles, text, bitmaps.
# Port of the GraphicsTest Arduino example (sys/arduino/u8g2_full_buffer/GraphicsTest),
# which is made for 128x64 monochrome displays. Here the shapes are drawn twice
# the size, inside the part of a round 240x240 display that is not cut off by
# the corners, and each page gets its own color.
#
#   >>> import demo_graphics
#   >>> demo_graphics.run()      # ctrl-c to stop
#
# Left out: the Unicode page, it needs the unifont symbols font.

import time
import display  # noqa: F401, u8g2.init() with this board's wiring
import u8g2

K = 2  # scale of the 128x64 example geometry
OX = 24  # origin of the example geometry
OY = 56

TITLE_Y = 28  # page title, centered
CLUT_TITLE = 1
CLUT_BODY = 2

FONT = u8g2.font_profont15_tf  # the example uses 6x10
FONT_ASCII = u8g2.font_profont17_tf

BLACK = 0x000000
TITLE_COLOR = 0x00C0FF
# body color of the pages
COLORS = (0xFFFFFF, 0xFFE000, 0x40FF40, 0xFF8040, 0xFF60FF, 0x60A0FF, 0xFFFFFF, 0xFFFFFF, 0xFFE000, 0x40FF40, 0xFF8040)

# bitmaps, xbm
CROSS_W = CROSS_H = 24
CROSS_BITS = bytes((
    0x00, 0x18, 0x00, 0x00, 0x24, 0x00, 0x00, 0x24, 0x00, 0x00, 0x42, 0x00,
    0x00, 0x42, 0x00, 0x00, 0x42, 0x00, 0x00, 0x81, 0x00, 0x00, 0x81, 0x00,
    0xC0, 0x00, 0x03, 0x38, 0x3C, 0x1C, 0x06, 0x42, 0x60, 0x01, 0x42, 0x80,
    0x01, 0x42, 0x80, 0x06, 0x42, 0x60, 0x38, 0x3C, 0x1C, 0xC0, 0x00, 0x03,
    0x00, 0x81, 0x00, 0x00, 0x81, 0x00, 0x00, 0x42, 0x00, 0x00, 0x42, 0x00,
    0x00, 0x42, 0x00, 0x00, 0x24, 0x00, 0x00, 0x24, 0x00, 0x00, 0x18, 0x00,
))
CROSS_BLOCK_W = CROSS_BLOCK_H = 14
CROSS_BLOCK_BITS = bytes((
    0xFF, 0x3F, 0x01, 0x20, 0x01, 0x20, 0x01, 0x20, 0x01, 0x20, 0x01, 0x20,
    0xC1, 0x20, 0xC1, 0x20, 0x01, 0x20, 0x01, 0x20, 0x01, 0x20, 0x01, 0x20,
    0x01, 0x20, 0xFF, 0x3F,
))


def X(x):
    return OX + K * x


def Y(y):
    return OY + K * y


def prepare():
    u8g2.set_font(FONT)
    u8g2.set_font_ref_height_extended_text()
    u8g2.set_draw_color(1)
    u8g2.set_font_pos_top()
    u8g2.set_font_direction(0)


def title(text):
    u8g2.draw_str((u8g2.get_display_width() - u8g2.get_str_width(text)) // 2, TITLE_Y, text)


def box_frame(a):
    title("draw_box, draw_frame")
    u8g2.draw_box(X(5), Y(10), K * 20, K * 10)
    u8g2.draw_box(X(10 + a), Y(15), K * 30, K * 7)
    u8g2.draw_frame(X(5), Y(10 + 30), K * 20, K * 10)
    u8g2.draw_frame(X(10 + a), Y(15 + 30), K * 30, K * 7)


def disc_circle(a):
    title("draw_disc, draw_circle")
    u8g2.draw_disc(X(10), Y(18), K * 9)
    u8g2.draw_disc(X(24 + a), Y(16), K * 7)
    u8g2.draw_circle(X(10), Y(18 + 30), K * 9)
    u8g2.draw_circle(X(24 + a), Y(16 + 30), K * 7)


def r_frame(a):
    title("draw_rframe, draw_rbox")
    u8g2.draw_rframe(X(5), Y(10), K * 40, K * 30, K * (a + 1))
    u8g2.draw_rbox(X(50), Y(10), K * 25, K * 40, K * (a + 1))


def string(a):
    title("font direction")
    u8g2.set_font_direction(0)
    u8g2.draw_str(X(30 + a), Y(31), " 0")
    u8g2.set_font_direction(1)
    u8g2.draw_str(X(30), Y(31 + a), " 90")
    u8g2.set_font_direction(2)
    u8g2.draw_str(X(30 - a), Y(31), " 180")
    u8g2.set_font_direction(3)
    u8g2.draw_str(X(30), Y(31 - a), " 270")


def line(a):
    title("draw_line")
    u8g2.draw_line(X(7 + a), Y(10), X(40), Y(55))
    u8g2.draw_line(X(7 + a * 2), Y(10), X(60), Y(55))
    u8g2.draw_line(X(7 + a * 3), Y(10), X(80), Y(55))
    u8g2.draw_line(X(7 + a * 4), Y(10), X(100), Y(55))


def triangle(a):
    title("draw_triangle")
    o = a
    u8g2.draw_triangle(X(14), Y(7), X(45), Y(30), X(10), Y(40))
    u8g2.draw_triangle(X(14 + o), Y(7 - o), X(45 + o), Y(30 - o), X(57 + o), Y(10 - o))
    u8g2.draw_triangle(X(57 + o * 2), Y(10), X(45 + o * 2), Y(30), X(86 + o * 2), Y(53))
    u8g2.draw_triangle(X(10 + o), Y(40 + o), X(45 + o), Y(30 + o), X(86 + o), Y(53 + o))


def ascii_page(first, text):
    title(text)
    u8g2.set_font(FONT_ASCII)
    for y in range(6):
        for x in range(16):
            u8g2.draw_glyph(24 + x * 12, 54 + y * 22, y * 16 + x + first)
    u8g2.set_font(FONT)


def ascii_1(a):
    ascii_page(32, "ASCII page 1")


def ascii_2(a):
    ascii_page(160, "ASCII page 2")


def bitmap_overlay(a):
    frame_size = 28
    title("bitmap overlay")
    u8g2.draw_str(X(0), Y(frame_size + 12), "Solid / transparent")
    u8g2.set_bitmap_mode(0)  # solid
    u8g2.draw_frame(X(0), Y(10), K * frame_size, K * frame_size)
    u8g2.draw_xbm(X(2), Y(12), CROSS_W, CROSS_H, CROSS_BITS)
    if a & 4:
        u8g2.draw_xbm(X(7), Y(17), CROSS_BLOCK_W, CROSS_BLOCK_H, CROSS_BLOCK_BITS)

    u8g2.set_bitmap_mode(1)  # transparent
    u8g2.draw_frame(X(frame_size + 5), Y(10), K * frame_size, K * frame_size)
    u8g2.draw_xbm(X(frame_size + 7), Y(12), CROSS_W, CROSS_H, CROSS_BITS)
    if a & 4:
        u8g2.draw_xbm(X(frame_size + 12), Y(17), CROSS_BLOCK_W, CROSS_BLOCK_H, CROSS_BLOCK_BITS)
    u8g2.set_bitmap_mode(0)


def bitmap_modes(transparent):
    frame_size = 24
    u8g2.draw_box(X(0), Y(frame_size // 2), K * frame_size * 4, K * frame_size)
    u8g2.draw_str(X(frame_size // 2), Y(50), "Black")
    u8g2.draw_str(X(frame_size * 2), Y(50), "White")
    u8g2.draw_str(X(frame_size * 7 // 2), Y(50), "XOR")

    if not transparent:
        u8g2.set_bitmap_mode(0)  # solid
        title("solid bitmap")
    else:
        u8g2.set_bitmap_mode(1)  # transparent
        title("transparent bitmap")
    u8g2.set_draw_color(0)  # black
    u8g2.draw_xbm(X(frame_size // 2), Y(24), CROSS_W, CROSS_H, CROSS_BITS)
    u8g2.set_draw_color(1)  # white
    u8g2.draw_xbm(X(frame_size * 2), Y(24), CROSS_W, CROSS_H, CROSS_BITS)
    u8g2.set_draw_color(2)  # xor
    u8g2.draw_xbm(X(frame_size * 7 // 2), Y(24), CROSS_W, CROSS_H, CROSS_BITS)
    u8g2.set_draw_color(1)
    u8g2.set_bitmap_mode(0)


def bitmap_modes_solid(a):
    bitmap_modes(0)


def bitmap_modes_transparent(a):
    bitmap_modes(1)


# (page, steps): steps is 8 for animated pages, 1 for static ones
PAGES = (
    (box_frame, 8),
    (disc_circle, 8),
    (r_frame, 8),
    (string, 8),
    (line, 8),
    (triangle, 8),
    (ascii_1, 1),
    (ascii_2, 1),
    (bitmap_modes_solid, 1),
    (bitmap_modes_transparent, 1),
    (bitmap_overlay, 8),
)


def setup():
    # title band, rest of the screen: body
    cols = u8g2.get_cols()
    rows = u8g2.get_rows()
    u8g2.set_clut_region(0, 0, cols, rows, CLUT_BODY)
    u8g2.set_clut_region(0, TITLE_Y // 8, cols, 3, CLUT_TITLE)
    u8g2.set_clut_color(CLUT_TITLE, TITLE_COLOR, BLACK)


def restore():
    # restore u8g2 drawing state back to defaults, for the demos that run next
    u8g2.set_font_direction(0)
    u8g2.set_font_ref_height_text()
    u8g2.set_font_pos_baseline()
    u8g2.set_draw_color(1)
    u8g2.set_bitmap_mode(0)


def run(delay_ms=100):
    if u8g2.get_display_width() < 240 or u8g2.get_display_height() < 240:
        raise RuntimeError("needs a display of at least 240x240")
    setup()
    try:
        while True:
            for n, (page, steps) in enumerate(PAGES):
                u8g2.set_clut_color(CLUT_BODY, COLORS[n], BLACK)
                for a in range(steps):
                    u8g2.clear_buffer()
                    prepare()
                    page(a)
                    u8g2.send_buffer()
                    time.sleep_ms(delay_ms)
                if steps == 1:
                    time.sleep_ms(2000)  # static page
    finally:  # ctrl-c
        restore()


if __name__ == "__main__":
    run()
