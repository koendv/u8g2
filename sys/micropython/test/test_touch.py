# tests of events.py, dpad.py and the rotation of cst816.py on the unix port.
# the touch panel is a fake: no hardware, no i2c.
import sys

passed = 0


def check(cond, msg):
    global passed
    if not cond:
        raise AssertionError(msg)
    passed += 1


# modules the unix test build does not have
class _Flag:
    def set(self):
        pass

    async def wait(self):
        pass


class _Asyncio:
    ThreadSafeFlag = _Flag
    TimeoutError = Exception


class _Pin:
    OUT = 1
    IN = 0
    PULL_UP = 2
    IRQ_FALLING = 4


class _Machine:
    Pin = _Pin


sys.modules["asyncio"] = _Asyncio()
sys.modules["machine"] = _Machine()

import events
import dpad
import cst816

U, L, D, R = events.UP, events.LEFT, events.DOWN, events.RIGHT

# --- events.Fifo: the screen is rotated by `rotation` quarter turns clockwise.
# a direction of the panel becomes this direction on the screen:
EXPECTED = (
    {U: U, D: D, L: L, R: R},
    {U: L, D: R, L: D, R: U},
    {U: D, D: U, L: R, R: L},
    {U: R, D: L, L: U, R: D},
)
for rotation in range(4):
    for ev in range(len(events.NAMES)):
        f = events.Fifo(4, rotation)
        f.put(ev)
        want = EXPECTED[rotation].get(ev, ev)
        got = f.get()
        check(got == want, "rotation %d: %s -> %s, expected %s" % (rotation, events.NAMES[ev], events.NAMES[got], events.NAMES[want]))
f = events.Fifo(4)
f.put(U)
check(f.get() == U and f.get() is None, "fifo without rotation")
try:
    events.Fifo(4, 4)
    check(False, "rotation 4 should raise")
except ValueError:
    check(True, "")
print("fifo rotation ok")


# --- dpad: a touch on the rotated screen gives the event of the screen.
class FakeTouch:
    def __init__(self):
        self.x = 0
        self.y = 0
        self.fingers = 0
        self.callback = None


def logical_zone(x, y, w, h, half=50):
    # the zones of the screen the user sees, 45 degree lines through the center
    dx = x - w // 2
    dy = y - h // 2
    if abs(dx) <= half and abs(dy) <= half:
        return events.SELECT
    if abs(dy) >= abs(dx):
        return U if dy < 0 else D
    return L if dx < 0 else R


def to_panel(rotation, x, y, w, h):
    # where a touch at x, y of the screen (w by h) is on the touch panel. the screen is
    # the panel rotated by `rotation` quarter turns clockwise, so w and h swap for 1 and 3
    wp, hp = (w, h) if rotation % 2 == 0 else (h, w)
    if rotation == 0:
        return x, y
    if rotation == 1:
        return wp - 1 - y, x
    if rotation == 2:
        return wp - 1 - x, hp - 1 - y
    return y, hp - 1 - x


def touch_event(pad, touch, x, y):
    touch.x, touch.y, touch.fingers = x, y, 1
    ev = pad.get()
    touch.fingers = 0
    up = pad.get()  # lifted: select
    return ev if ev is not None else up


for w, h in ((240, 240), (240, 280), (320, 240), (240, 320)):
    for rotation in range(4):
        touch = FakeTouch()
        pad = dpad.DPad(touch, w, h, rotation=rotation)
        cx, cy = w // 2, h // 2
        points = [(cx, cy), (cx + 20, cy - 10), (cx, 10), (cx, h - 10), (10, cy), (w - 10, cy),
                  (cx + 60, 10), (cx - 60, h - 10), (10, cy + 60), (w - 10, cy - 60), (20, 30), (w - 20, h - 30)]
        for x, y in points:
            px, py = to_panel(rotation, x, y, w, h)
            want = logical_zone(x, y, w, h)
            got = touch_event(pad, touch, px, py)
            check(got == want, "%dx%d rotation %d: screen (%d,%d) panel (%d,%d): %s, expected %s" % (
                w, h, rotation, x, y, px, py, events.NAMES[got] if got is not None else None, events.NAMES[want]))
print("dpad zones and rotation ok")

# the zones are the same under rotation: 45 degrees, on any panel size
touch = FakeTouch()
pad = dpad.DPad(touch, 240, 280)
for (x, y), want in {(120, 140): events.SELECT, (120, 20): U, (120, 270): D, (10, 140): L, (230, 140): R,
                     (60, 40): U, (30, 100): L, (200, 240): D, (230, 200): R}.items():
    check(pad.zone(x, y) == want, "zone(%d,%d) = %s, expected %s" % (x, y, events.NAMES[pad.zone(x, y)], events.NAMES[want]))
print("dpad 45 degree zones ok")


# --- cst816: swipes follow the rotation, x and y are the panel coordinates
class FakeI2C:
    def __init__(self):
        self.reg = bytes(6)

    def readfrom_mem(self, addr, reg, n):
        return bytes((0xB5, 0, 1, 0))[:n]

    def writeto_mem(self, addr, reg, data):
        pass

    def readfrom_mem_into(self, addr, reg, buf):
        buf[:] = self.reg


# chip gesture codes for a finger moving up, down, left and right on the panel (Sunton CST816D)
GESTURE = {U: 0x02, D: 0x01, L: 0x03, R: 0x04}
for rotation in range(4):
    i2c = FakeI2C()
    t = cst816.CST816(i2c, rotation=rotation)
    for panel, code in GESTURE.items():
        i2c.reg = bytes((code, 1, 0, 100, 0, 50))
        t.update()
        got = t.fifo.get()
        want = EXPECTED[rotation][panel]
        check(got == want, "cst816 rotation %d: swipe %s -> %s, expected %s" % (rotation, events.NAMES[panel], events.NAMES[got], events.NAMES[want]))
        check((t.x, t.y, t.fingers) == (100, 50, 1), "cst816 x, y are panel coordinates")
    i2c.reg = bytes((0x05, 1, 0, 1, 0, 2))  # click
    t.update()
    check(t.fifo.get() == events.SELECT, "cst816 rotation %d: click" % rotation)
print("cst816 rotation ok")
print("PASS", passed, "checks")
