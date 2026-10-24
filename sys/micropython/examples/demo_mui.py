# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer

# mui menu on a display 240 pixels wide, driven from the keyboard over the REPL.
# Made for 240x240, also runs on taller displays such as 240x280.
# Port of the MUIInput3BtnBounce2 Arduino example
# (sys/arduino/u8g2_page_buffer/MUIInput3BtnBounce2): a menu with a number, a bar
# and a list, and a page that shows the values.
#
#   w, up arrow       previous field
#   s, down arrow     next field
#   a, left arrow     decrease the value of a field
#   d, right arrow    increase the value of a field
#   space, enter      select a button; on a value field, increase the value
#   q                 quit
#
#   >>> import demo_mui
#   >>> demo_mui.run_keys()
#
# Or with the touch screen, if the board has one. The screen has five zones,
# marked by arrows at the edge: tap or hold the upper zone for the previous
# field, the lower zone for the next field, the left zone to decrease and the
# right zone to increase the value of a field. Tap the square in the center to
# select. Hold the center for a second to leave the menu.
#
#   >>> demo_mui.run_touch()
#
# Or with swipes, the gestures of the touch controller: swipe up and down for
# previous and next field, left and right to decrease and increase a value,
# click to select, long press to leave the menu.
#
#   >>> demo_mui.run_gestures()
#
# On a rotated display, e.g. a _ROT display, the touch panel is not rotated.
# run_touch(rotation) and run_gestures(rotation) turn the directions: rotation is
# the number of quarter turns clockwise, 0 to 3, as u8g2.R0..R3.
#
# The Arduino example has three buttons and uses the "mud" (up/down) widgets:
# select switches a field to edit mode, and next and previous change the value.
# With a keyboard it is easier to change the value directly. This demo uses the
# "mse" widgets, where increment and decrement change the value, so there is no
# edit mode.
#
# mui does the drawing of the menu; the "Show Data" page is plain u8g2 calls on
# top of it, the Arduino example does that in a custom field function.
#
# mui is built with MUI_U8G2_HIRES: on a display of 255 pixels or more, the x and y of
# a field are multiplied by 2, 4 or 8. px() and py() convert pixels to mui units.

import asyncio
import display  # noqa: F401, u8g2.init() with this board's wiring
import u8g2
import mui
import events

W = u8g2.get_display_width()
H = u8g2.get_display_height()
COLS = u8g2.get_cols()


def _mui_scale(pixels):
    # the smallest factor of 1, 2, 4 or 8 that reaches the last pixel
    return 1 if pixels < 255 else 2 if pixels < 511 else 4 if pixels < 1021 else 8


XS = _mui_scale(W)
YS = _mui_scale(H)


def px(x):
    # x byte of a mui field at pixel x
    return x // XS


def py(y):
    # y byte of a mui field at pixel y
    return y // YS


# the mui functions with pixel positions
def label(x, y, text):
    return mui.label(px(x), py(y), text)


def xy(field_id, x, y):
    return mui.xy(field_id, px(x), py(y))


def xya(field_id, x, y, arg):
    return mui.xya(field_id, px(x), py(y), arg)


def xyat(field_id, x, y, arg, text):
    return mui.xyat(field_id, px(x), py(y), arg, text)


CLUT_BODY = 0
CLUT_TITLE = 1

ANIMALS = ("Bird", "Bison", "Cat", "Crow", "Dog", "Elephant", "Fish", "Gnu", "Horse",
           "Koala", "Lion", "Mouse", "Owl", "Rabbit", "Spider", "Turtle", "Zebra")

# fields. the values live in the Field objects
num = mui.Field("NV", mui.U8_MIN_MAX_WM_MSE_PI, min=0, max=99)
bar = mui.Field("NB", mui.U8_BAR_WM_MSE_PF, min=0, max=16, step=1, flags=mui.MMS_4X_BAR)
animal = mui.Field("NA", mui.U16_LIST_LINE_WA_MSE_PI, items=ANIMALS)

FIELDS = (
    mui.Field("S0", mui.FONT_STYLE, font=u8g2.font_profont17_tf),  # regular font
    mui.Field("S1", mui.FONT_STYLE, font=u8g2.font_profont22_tf),  # title font
    mui.Field("HR", mui.HLINE),
    mui.Field(".L", mui.LABEL),
    mui.Field("GP", mui.GOTO_DATA),
    mui.Field("GC", mui.GOTO_FORM_W1_PI),
    num,
    bar,
    animal,
    mui.Field("GO", mui.BTN_GOTO_WM_FI),
)

FORM_MENU, FORM_ENTER, FORM_SHOW = 1, 10, 12

FDS = (
    mui.form(FORM_MENU)
    + mui.style(1) + label(50, 50, "MUI Demo") + mui.style(0)
    + xy("HR", 0, 62)
    # the text of the data field: form number, then the button text, '|' separated
    + mui.data("GP", chr(FORM_ENTER) + "Enter Data|" + chr(FORM_SHOW) + "Show Data")
    + xya("GC", 70, 110, 0)
    + xya("GC", 70, 145, 1)

    + mui.form(FORM_ENTER)
    + mui.style(1) + label(40, 50, "Enter Data") + mui.style(0)
    + xy("HR", 0, 62)
    + label(40, 100, "Num:")
    + label(40, 135, "Bar:")
    + label(30, 170, "Animal:")
    + xy("NV", 110, 100)
    + xy("NB", 110, 135)
    + xya("NA", 110, 170, 90)
    + xyat("GO", 100, 210, FORM_MENU, " Ok ")

    + mui.form(FORM_SHOW)
    + mui.style(1) + label(40, 50, "Show Data") + mui.style(0)
    + xy("HR", 0, 62)
    + xyat("GO", 100, 210, FORM_MENU, " Ok ")
)


def show_data():
    # the values, below the title
    u8g2.set_font(u8g2.font_profont17_tf)
    u8g2.draw_str(50, 100, "Num:")
    u8g2.draw_str(120, 100, str(num.value))
    u8g2.draw_str(50, 125, "Bar:")
    u8g2.draw_str(120, 125, str(bar.value))
    u8g2.draw_str(50, 150, "Animal:")
    u8g2.draw_str(120, 150, ANIMALS[animal.value])


hints = False  # draw the edge arrows of the touch zones


def show_hints():
    # arrows at the middle of each edge: up, down, left, right zone
    cx = W // 2
    cy = H // 2
    u8g2.draw_triangle(cx, 3, cx - 7, 12, cx + 7, 12)
    u8g2.draw_triangle(cx, H - 4, cx - 7, H - 13, cx + 7, H - 13)
    u8g2.draw_triangle(3, cy, 12, cy - 7, 12, cy + 7)
    u8g2.draw_triangle(W - 4, cy, W - 13, cy - 7, W - 13, cy + 7)


def show():
    u8g2.clear_buffer()
    mui.draw()
    if mui.form_id() == FORM_SHOW:
        show_data()
    if hints:
        show_hints()
    u8g2.send_buffer()


def start():
    u8g2.set_font_pos_baseline()
    u8g2.set_clut_color(CLUT_BODY, 0xFFFFFF, 0x000000)
    u8g2.set_clut_color(CLUT_TITLE, 0x00C0FF, 0x000000)
    u8g2.set_clut_region(0, 0, COLS, u8g2.get_rows(), CLUT_BODY)
    u8g2.set_clut_region(0, 3, COLS, 4, CLUT_TITLE)  # title band, y 24..55
    mui.init(FDS, FIELDS)
    mui.goto_form(FORM_MENU)
    show()


ACTIONS = {
    events.UP: mui.prev,
    events.DOWN: mui.next,
    events.SELECT: mui.select,
    events.LEFT: mui.dec,
    events.RIGHT: mui.inc,
}


async def _loop(source):
    # read events from source.get() until QUIT. redraw after each action.
    # without an event, sleep in source.wait(): no cpu use until a key or a touch.
    start()
    while True:
        ev = source.get()
        if ev is None:
            await source.wait()
            continue
        if ev == events.QUIT:
            break
        if ev == events.LONG:  # long press on the touch screen
            mui.leave()
            break
        action = ACTIONS.get(ev)
        if action is not None:
            action()
            show()


def _run(source):
    try:
        asyncio.run(_loop(source))
    finally:
        asyncio.new_event_loop()  # after ctrl-c: ready for the next run


def run_gestures(rotation=0):
    # CST816 touch gestures: swipes, click, long press. ctrl-c quits.
    # rotation: 0..3, quarter turns clockwise of the display, as u8g2.R0..R3.
    import touch

    t = touch.init(rotation=rotation)
    try:
        _run(t)
    finally:
        t.deinit()


def run_touch(rotation=0):
    # touch screen as a d-pad: five zones. ctrl-c quits.
    # rotation: 0..3, quarter turns clockwise of the display, as u8g2.R0..R3.
    global hints
    import cst816
    import dpad
    import touch

    t = touch.init(cst816.CHANGE)  # interrupt when the touch changes, with x, y
    hints = True
    try:
        _run(dpad.DPad(t, u8g2.get_display_width(), u8g2.get_display_height(), rotation=rotation))
    finally:
        hints = False
        t.deinit()


def run_keys():
    # keyboard over the REPL. q or ctrl-c quits.
    import keys as kb

    with kb.Keys() as k:
        _run(k)


if __name__ == "__main__":
    run_keys()
