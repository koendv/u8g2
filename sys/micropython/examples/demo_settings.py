# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer

# A settings menu for a tool, with simulated values, as a mui example that uses the
# full height of the display. Menu adapted from https://github.com/koendv/arm_can_tool
# (applications/u8g2): a title line at the top, one setting per line, a "Back" line
# at the bottom. The number of lines depends on the height of the display: 9 on
# 240x280, 7 on 240x240. Text is kept away from the corners, for displays with
# round corners.
#
#   w, s       previous field, next field
#   a, d       decrease, increase the value of a field
#   space, enter   select a button
#   q          quit
#
#   >>> import demo_settings
#   >>> demo_settings.run_keys()
#
# Pages: the main menu, "Startup" (checkboxes), "Serial" and "CAN bus" (lists of
# values), "Display" (the brightness is the real backlight), "About" (free ram and
# the firmware). The values are only stored in the fields: nothing is connected.
#
# mui is built with MUI_U8G2_HIRES: on a display of 255 pixels or more, the x and y of
# a field are multiplied by 2, 4 or 8. px() and py() convert pixels to mui units.

import asyncio
import gc
import os
import display  # noqa: F401, u8g2.init() with this board's wiring
import u8g2
import mui
import events

W = u8g2.get_display_width()
H = u8g2.get_display_height()
COLS = u8g2.get_cols()
ROWS = u8g2.get_rows()


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


# lines, font profont22 (11 pixels wide). baseline of line i, i = 1..LINES.
# the last line is the "Back" button
CHAR_W = 11
LINE = 26
TITLE_Y = 24
RULE_Y = 31
FIRST = 58
BOTTOM = 14  # free pixels below the last line
LINES = (H - BOTTOM - FIRST) // LINE + 1


def line(i):
    return FIRST + LINE * (i - 1)


X_LABEL = 24  # away from the round corners
X_VALUE = 140  # option lists
X_BAR = 110  # bars
X_CHECK = 200  # checkboxes
W_VALUE = 84  # width of a list field, pixels

CLUT_BODY, CLUT_TITLE = 0, 1
TITLE_COLOR = 0x00C0FF

FORM_MAIN, FORM_STARTUP, FORM_SERIAL, FORM_CAN, FORM_DISPLAY, FORM_ABOUT = 1, 10, 20, 30, 40, 50

SERIAL_SPEEDS = ("2400", "4800", "9600", "19200", "38400", "57600", "115200", "230400",
                 "460800", "921600", "1000000", "2000000")
CAN_SPEEDS = ("10000", "20000", "50000", "100000", "125000", "250000", "500000", "1000000")
PARITY = ("none", "even", "odd")
USB_OUT = ("serial0", "serial1", "rtt")
SLEEP = ("never", "10 s", "30 s", "1 min", "5 min")

STARTUP = ("CAN bus", "debugger", "memwatch", "lua autoexec", "logging", "rtt", "swo",
           "watchdog", "trigger")[:LINES - 1]  # as many as there are lines


def on_brightness(v):
    try:
        u8g2.backlight(v * 255 // 100)
    except ValueError:  # no backlight pin
        pass


# fields. ids are two characters, unique. the values are in the Field objects
startup = [mui.Field("C%d" % i, mui.U8_CHKBOX_WM_PI, value=i & 1) for i in range(len(STARTUP))]
usb_out = mui.Field("U0", mui.U16_LIST_LINE_WA_MSE_PI, items=USB_OUT)
serial0 = mui.Field("N0", mui.U16_LIST_LINE_WA_MSE_PI, items=SERIAL_SPEEDS, value=6)
serial1 = mui.Field("N1", mui.U16_LIST_LINE_WA_MSE_PI, items=SERIAL_SPEEDS, value=6)
parity = mui.Field("PA", mui.U16_LIST_LINE_WA_MSE_PI, items=PARITY)
can_speed = mui.Field("CS", mui.U16_LIST_LINE_WA_MSE_PI, items=CAN_SPEEDS, value=3)
can_log = mui.Field("CL", mui.U8_CHKBOX_WM_PI, value=1)
can_filter = mui.Field("CF", mui.U8_CHKBOX_WM_PI)
brightness = mui.Field("BR", mui.U8_FIXED_WIDTH_BAR_WM_MSE_PI, min=0, max=100, step=10, width=60,
                       flags=mui.MMS_SHOW_VALUE, value=50, on_change=on_brightness)
sleep = mui.Field("SL", mui.U16_LIST_LINE_WA_MSE_PI, items=SLEEP, value=2)
invert = mui.Field("IV", mui.U8_CHKBOX_WM_PI)

FIELDS = tuple(startup) + (
    usb_out, serial0, serial1, parity, can_speed, can_log, can_filter, brightness, sleep, invert,
    mui.Field("S0", mui.FONT_STYLE, font=u8g2.font_profont22_tf),  # style 0: lines
    mui.Field("S1", mui.FONT_STYLE, font=u8g2.font_profont22_tf),  # style 1: title, cyan
    mui.Field("HR", mui.HLINE),
    mui.Field(".L", mui.LABEL),
    mui.Field("GP", mui.GOTO_DATA),
    mui.Field("GC", mui.GOTO_FORM_W1_PI),
    mui.Field("BK", mui.BTN_GOTO_WM_FI),
)


def head(form, title):
    # form number, title in the middle, and the line below it
    return (mui.form(form) + mui.style(1)
            + mui.label(px((W - CHAR_W * len(title)) // 2), py(TITLE_Y), title)
            + mui.style(0) + mui.xy("HR", 0, py(RULE_Y)))


def back():
    # "Back" in the middle of the last line
    return mui.xyat("BK", px((W - CHAR_W * 6) // 2), py(line(LINES)), FORM_MAIN, " Back ")


def label(i, text):
    return mui.label(px(X_LABEL), py(line(i)), text)


def check(i, field_id):
    return mui.xy(field_id, px(X_CHECK), py(line(i)))


def option(i, field_id):
    return mui.xya(field_id, px(X_VALUE), py(line(i)), W_VALUE)


MENU = (("Startup", FORM_STARTUP), ("Serial", FORM_SERIAL), ("CAN bus", FORM_CAN),
        ("Display", FORM_DISPLAY), ("About", FORM_ABOUT))


def build():
    fds = head(FORM_MAIN, "SETTINGS")
    # the goto buttons: the data field lists the forms, each button picks one
    fds += mui.data("GP", "|".join(chr(form) + name for name, form in MENU))
    for k in range(len(MENU)):
        fds += mui.xya("GC", px(X_LABEL), py(line(k + 1)), k)

    fds += head(FORM_STARTUP, "STARTUP")
    for k, name in enumerate(STARTUP):
        fds += label(k + 1, name) + check(k + 1, "C%d" % k)
    fds += back()

    fds += head(FORM_SERIAL, "SERIAL")
    fds += label(1, "usb out") + option(1, "U0")
    fds += label(2, "serial0") + option(2, "N0")
    fds += label(3, "serial1") + option(3, "N1")
    fds += label(4, "parity") + option(4, "PA")
    fds += back()

    fds += head(FORM_CAN, "CAN BUS")
    fds += label(1, "speed") + option(1, "CS")
    fds += label(2, "logging") + check(2, "CL")
    fds += label(3, "canfilter") + check(3, "CF")
    fds += back()

    fds += head(FORM_DISPLAY, "DISPLAY")
    fds += label(1, "light") + mui.xy("BR", px(X_BAR), py(line(1)))
    fds += label(2, "sleep") + option(2, "SL")
    fds += label(3, "invert") + check(3, "IV")
    fds += back()

    fds += head(FORM_ABOUT, "ABOUT")
    fds += back()
    return fds


def show_about():
    # plain u8g2 text, below the title: free ram and the firmware
    u8g2.set_font(u8g2.font_profont22_tf)
    uname = os.uname()
    lines = ("free ram:", "%d k" % (gc.mem_free() // 1024), "firmware:", uname.release,
             "mcu:", uname.machine.split(" with ")[-1])
    for k, text in enumerate(lines[:LINES - 1]):
        u8g2.draw_str(X_LABEL, line(k + 1), text[:(W - 2 * X_LABEL) // CHAR_W])


def show():
    u8g2.clear_buffer()
    mui.draw()
    if mui.form_id() == FORM_ABOUT:
        show_about()
    u8g2.send_buffer()


def start():
    if W < 240 or H < 240:
        raise RuntimeError("needs a display of at least 240x240")
    u8g2.set_font_pos_baseline()  # mui draws text at the baseline
    u8g2.set_clut_color(CLUT_BODY, 0xFFFFFF, 0x000000)
    u8g2.set_clut_color(CLUT_TITLE, TITLE_COLOR, 0x000000)
    u8g2.set_clut_region(0, 0, COLS, ROWS, CLUT_BODY)
    u8g2.set_clut_region(0, 0, COLS, 4, CLUT_TITLE)  # the title and the line below it, y 0..31
    mui.init(build(), FIELDS)
    mui.goto_form(FORM_MAIN)
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
    # without an event, sleep in source.wait(): no cpu use until a key.
    start()
    while True:
        ev = source.get()
        if ev is None:
            await source.wait()
            continue
        if ev == events.QUIT:
            break
        action = ACTIONS.get(ev)
        if action is not None:
            action()
            show()


def run_keys():
    # keyboard over the REPL. q or ctrl-c quits.
    import keys as kb

    with kb.Keys() as k:
        try:
            asyncio.run(_loop(k))
        finally:
            asyncio.new_event_loop()  # after ctrl-c: ready for the next run
