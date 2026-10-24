# end-to-end tests of the u8g2 module on the unix port: decode the bus traffic,
# replay it into a model of the display ram, compare with the u8g2 framebuffer.
import sim
import u8g2

passed = 0


def check(cond, msg):
    global passed
    if not cond:
        raise AssertionError(msg)
    passed += 1


def draw_scene():
    u8g2.clear_buffer()
    u8g2.draw_frame(0, 0, u8g2.get_display_width(), u8g2.get_display_height())
    u8g2.set_font(u8g2.font_6x10_tf)
    u8g2.draw_str(3, 12, "Hello u8g2")
    u8g2.draw_disc(30, 40, 9)
    u8g2.draw_line(0, 0, u8g2.get_display_width() - 1, u8g2.get_display_height() - 1)
    u8g2.draw_pixel(u8g2.get_display_width() - 2, 1)


def compare(ram, x_off, label):
    w, h, rows = sim.read_pbm(u8g2)
    bad = 0
    for y in range(h):
        for x in range(w):
            if ram.pixel(x + x_off, y) != rows[y][x]:
                bad += 1
    check(bad == 0, "%s: %d pixel mismatches" % (label, bad))
    on = sum(sum(r) for r in rows)
    check(on > 100, "%s: scene drawn (%d pixels on)" % (label, on))
    return w, h


# --- i2c displays: every i2c display in the firmware initializes and sends a frame
i2c_names = [n for n in dir(u8g2) if "_I2C_" in n]
longest = 0
for name in i2c_names:
    bus = sim.I2cBus()
    u8g2.init(i2c=bus.softi2c(), display=getattr(u8g2, name))
    draw_scene()
    u8g2.send_buffer()
    trans = [t for t in bus.trans if t]
    check(all(t[0] == 0x78 for t in trans), name + ": default address 0x3c")
    longest = max(longest, max(len(t) - 1 for t in trans))
print("i2c displays:", len(i2c_names), "longest transfer:", longest, "bytes")

# --- ssd1306 i2c 128x64: pixel exact
for name, x_off in (("SSD1306_I2C_128X64_NONAME", 0), ("SH1106_I2C_128X64_NONAME", 2),
                    ("SSD1306_I2C_128X32_UNIVISION", 0)):
    bus = sim.I2cBus()
    u8g2.init(i2c=bus.softi2c(), display=getattr(u8g2, name))
    draw_scene()
    u8g2.send_buffer()
    ram = sim.PageRam()
    ram.feed_i2c([t for t in bus.trans if t], 0x3c)
    w, h = compare(ram, x_off, name)
    print(name, "%dx%d pixel exact" % (w, h))

# --- addr= and missing device
bus = sim.I2cBus()
u8g2.init(i2c=bus.softi2c(), display=u8g2.SSD1306_I2C_128X64_NONAME, addr=0x3d)
check(all(t[0] == 0x7a for t in bus.trans if t), "addr=0x3d used")
bus = sim.I2cBus(ack=False)
try:
    u8g2.init(i2c=bus.softi2c(), display=u8g2.SSD1306_I2C_128X64_NONAME)
    check(False, "no device: should raise")
except OSError as e:
    check(True, "")
    print("no device:", repr(e))
try:
    u8g2.clear_buffer()
    check(False, "not ready after failed init")
except RuntimeError:
    check(True, "")

# --- ssd1306 spi 128x64: pixel exact, with and without cs
for with_cs in (True, False):
    bus = sim.SpiBus()
    kw = {"cs": bus.cs} if with_cs else {}
    bus.has_cs = with_cs
    u8g2.init(bus.softspi(), dc=bus.dc, rst=sim.Pin("rst"), display=u8g2.SSD1306_128X64_NONAME, **kw)
    draw_scene()
    u8g2.send_buffer()
    ram = sim.PageRam()
    ram.feed_spi(bus.out)
    compare(ram, 0, "SSD1306_128X64_NONAME spi, cs=%s" % with_cs)
print("SSD1306_128X64_NONAME spi pixel exact, with and without cs")

# --- rotation: R2 on i2c, compare against the rotated framebuffer
bus = sim.I2cBus()
u8g2.init(i2c=bus.softi2c(), display=u8g2.SSD1306_I2C_128X64_NONAME, rotation=u8g2.R2)
u8g2.clear_buffer()
u8g2.draw_box(0, 0, 10, 5)
u8g2.send_buffer()
ram = sim.PageRam()
ram.feed_i2c([t for t in bus.trans if t], 0x3c)
check(ram.pixel(127, 63) == 1 and ram.pixel(0, 0) == 0, "R2: logical (0,0) at physical (127,63)")
bus = sim.I2cBus()
u8g2.init(i2c=bus.softi2c(), display=u8g2.SSD1306_I2C_128X64_NONAME, rotation=u8g2.R1)
check(u8g2.get_display_width() == 64 and u8g2.get_display_height() == 128, "R1 swaps width and height")
print("rotation ok")

# --- spi displays: all initialize and send without error
spi_names = [n for n in dir(u8g2) if n[0].isupper() and "_I2C_" not in n
             and n not in ("BUS_NONE", "BUS_SPI", "BUS_SPI_NO_DC", "BUS_I2C", "MIRROR", "R0", "R1", "R2", "R3")
             and not n.startswith("DRAW_") and not n.startswith("BTN_") and n != "Font"]
mono_spi = [n for n in spi_names if not n.startswith(("ST7789", "ILI9341", "GC9A01"))]
for name in mono_spi:
    w, h, b, mode = u8g2.display_info(getattr(u8g2, name))
    bus = sim.SpiBus(cs_active=1 if b == u8g2.BUS_SPI_NO_DC else 0)
    if b == u8g2.BUS_SPI_NO_DC:
        u8g2.init(bus.softspi(), cs=bus.cs, display=getattr(u8g2, name))
    else:
        u8g2.init(bus.softspi(), cs=bus.cs, dc=bus.dc, display=getattr(u8g2, name))
    n0 = len(bus.out)
    draw_scene()
    u8g2.send_buffer()
    check(len(bus.out) - n0 >= w * h // 8, "%s: a full frame sent (%d bytes)" % (name, len(bus.out) - n0))
print("monochrome spi displays:", len(mono_spi))

# --- argument errors
def raises(exc, f, **kw):
    try:
        f(**kw)
    except exc:
        return True
    return False

bus = sim.SpiBus()
check(raises(ValueError, u8g2.init, display=u8g2.SSD1306_I2C_128X64_NONAME), "i2c display without i2c")
check(raises(ValueError, u8g2.init, i2c=sim.I2cBus().softi2c(), dc=bus.dc, display=u8g2.SSD1306_I2C_128X64_NONAME), "dc with i2c")
check(not raises(Exception, u8g2.init, spi=bus.softspi(), display=u8g2.SSD1306_128X64_NONAME), "spi display without dc: accepted")
check(raises(ValueError, u8g2.init, spi=bus.softspi(), dc=bus.dc, addr=0x3c, display=u8g2.SSD1306_128X64_NONAME), "addr with spi")
check(raises(ValueError, u8g2.init, i2c=sim.I2cBus().softi2c(), addr=200, display=u8g2.SSD1306_I2C_128X64_NONAME), "addr range")
check(raises(ValueError, u8g2.init, i2c=sim.I2cBus().softi2c(), rotation=7, display=u8g2.SSD1306_I2C_128X64_NONAME), "rotation range")
check(raises(TypeError, u8g2.init, i2c=bus.softspi(), display=u8g2.SSD1306_I2C_128X64_NONAME), "spi object as i2c")
check(raises(TypeError, u8g2.init, spi=sim.I2cBus().softi2c(), dc=bus.dc, display=u8g2.SSD1306_128X64_NONAME), "i2c object as spi")
check(raises(TypeError, u8g2.init, spi=bus.softspi(), dc=bus.dc), "display required")
check(raises(ValueError, u8g2.init, spi=bus.softspi(), dc=bus.dc, display=999), "unknown display")
print("argument errors ok")

# --- color: clut on a color display, refused on a monochrome one
bus = sim.SpiBus()
bus.keep = 0
u8g2.init(bus.softspi(), cs=bus.cs, dc=bus.dc, display=u8g2.ST7789_135X240)
u8g2.set_clut_color(3, 0xFF0000, 0x0000FF)
u8g2.set_clut_region(0, 0, 2, 2, 3)
check(u8g2.get_clut_map_size() == (17 * 30 + 1) // 2, "clut map size")
u8g2.clear_buffer()
u8g2.draw_box(0, 0, 4, 4)
bus.out = []
bus.keep = 64
bus.data_count = 0
u8g2.send_buffer()
# pixel data plus 8 bytes of CASET/RASET arguments per tile row (cad 011: arguments have dc=1)
check(bus.data_count == 136 * 240 * 3 // 2 + 30 * 8, "st7789 rgb444 frame: %d bytes" % bus.data_count)
# first window row of tile (0,0): pixels 0..3 set (red, 0xf00), 4..7 clear (blue, 0x00f)
ramwr = bus.out.index((0, 0x2C))
data = [b for dc, b in bus.out[ramwr + 1:] if dc]
px = []
for i in range(0, 12, 3):
    a, b2, c = data[i], data[i + 1], data[i + 2]
    px += [(a << 4) | (b2 >> 4), ((b2 & 15) << 8) | c]
check(px == [0xF00] * 4 + [0x00F] * 4, "st7789 tile colors %s" % [hex(p) for p in px])
print("color st7789 ok")

# --- color displays: the window of the last tile, which is rounded up to 8 pixels, stays
# inside the ram of the controller, normal and flipped (flip_mode 1 is 180 degrees)
ram_size = {"ST7789": (240, 320), "ILI9341": (240, 320), "GC9A01": (240, 240)}
color_names = [n for n in spi_names if n.startswith(tuple(ram_size))]
for name in color_names:
    ram_w, ram_h = ram_size[name.split("_")[0]]
    if name.endswith("_ROT"):
        ram_w, ram_h = ram_h, ram_w  # MADCTL MV exchanges columns and rows
    for flip in (0, 1):
        bus = sim.SpiBus()
        bus.keep = 0
        u8g2.init(bus.softspi(), cs=bus.cs, dc=bus.dc, display=getattr(u8g2, name))
        u8g2.set_flip_mode(flip)
        u8g2.clear_buffer()
        bus.track_windows()
        # the bottom right tile has the largest column and row
        u8g2.update_display_area(u8g2.get_cols() - 1, u8g2.get_rows() - 1, 1, 1)
        caset = [w for w in bus.windows if w[0] == 0x2A]
        raset = [w for w in bus.windows if w[0] == 0x2B]
        label = "%s flip=%d" % (name, flip)
        check(caset and raset, label + ": window sent")
        check(all(w[1] <= w[2] < ram_w for w in caset), label + ": CASET %s, ram has %d columns" % (caset, ram_w))
        check(all(w[1] <= w[2] < ram_h for w in raset), label + ": RASET %s, ram has %d rows" % (raset, ram_h))
print("color displays:", len(color_names), "windows inside the controller ram, normal and flipped")

# --- ST7789_240X280_ROT: the 240x280 panel rotated 90 degrees. logical size 280x240,
# MADCTL with MV, the panel starts at ram column 20. flip 0 and 1 are 90 and 270 degrees.
bus = sim.SpiBus()
bus.keep = 0
bus.track_windows()
u8g2.init(bus.softspi(), cs=bus.cs, dc=bus.dc, display=u8g2.ST7789_240X280)
check(bus.madctl[-1] == 0xC0, "portrait: MADCTL after init %02x" % bus.madctl[-1])
check((u8g2.get_display_width(), u8g2.get_display_height()) == (240, 280), "portrait size")
bus = sim.SpiBus()
bus.keep = 0
bus.track_windows()
u8g2.init(bus.softspi(), cs=bus.cs, dc=bus.dc, display=u8g2.ST7789_240X280_ROT)
check(bus.madctl[-1] == 0xA0, "rot: MADCTL after init %02x" % bus.madctl[-1])
check((u8g2.get_display_width(), u8g2.get_display_height()) == (280, 240), "rot: logical size")
check((u8g2.get_cols(), u8g2.get_rows()) == (35, 30), "rot: 35x30 tiles")
for flip, madctl in ((1, 0x60), (0, 0xA0)):
    bus.track_windows()
    u8g2.set_flip_mode(flip)
    check(bus.madctl == [madctl], "rot flip=%d: MADCTL %s" % (flip, [hex(m) for m in bus.madctl]))
    bus.track_windows()
    u8g2.update_display_area(0, 0, 1, 1)
    check(bus.windows == [(0x2A, 20, 27), (0x2B, 0, 7)], "rot flip=%d: first tile %s" % (flip, bus.windows))
    bus.track_windows()
    u8g2.update_display_area(34, 29, 1, 1)
    check(bus.windows == [(0x2A, 292, 299), (0x2B, 232, 239)], "rot flip=%d: last tile %s" % (flip, bus.windows))
print("ST7789_240X280_ROT ok")


# --- the clut map of a rotated display is in rotated tiles: a color set on the last tile column
# is the color of that column
bus = sim.SpiBus()
bus.keep = 200
u8g2.init(bus.softspi(), cs=bus.cs, dc=bus.dc, display=u8g2.ST7789_240X280_ROT)
u8g2.set_clut_color(2, 0x00FF00, 0x000000)
u8g2.set_clut_region(u8g2.get_cols() - 1, 0, 1, 1, 2)
u8g2.clear_buffer()
u8g2.draw_box(34 * 8, 0, 4, 8)
bus.out = []
u8g2.update_display_area(34, 0, 1, 1)
ramwr = bus.out.index((0, 0x2C))
data = [b for dc, b in bus.out[ramwr + 1:] if dc][:12]
px = []
for i in range(0, 12, 3):
    px += [(data[i] << 4) | (data[i + 1] >> 4), ((data[i + 1] & 15) << 8) | data[i + 2]]
check(px == [0x0F0] * 4 + [0x000] * 4, "rot last tile column colors %s" % [hex(p) for p in px])
print("rot clut ok")

# --- ILI9341_240X320_ROT: 320x240, MADCTL with MV. flip 0 is 90 degrees clockwise (checked on the glass)
bus = sim.SpiBus()
bus.keep = 0
bus.track_windows()
u8g2.init(bus.softspi(), cs=bus.cs, dc=bus.dc, display=u8g2.ILI9341_240X320)
check(bus.madctl[-1] == 0x48, "ili9341 portrait: MADCTL after init %02x" % bus.madctl[-1])
bus = sim.SpiBus()
bus.keep = 0
bus.track_windows()
u8g2.init(bus.softspi(), cs=bus.cs, dc=bus.dc, display=u8g2.ILI9341_240X320_ROT)
check(bus.madctl[-1] == 0x28, "ili9341 rot: MADCTL after init %02x" % bus.madctl[-1])
check((u8g2.get_display_width(), u8g2.get_display_height()) == (320, 240), "ili9341 rot: logical size")
check((u8g2.get_cols(), u8g2.get_rows()) == (40, 30), "ili9341 rot: 40x30 tiles")
for flip, madctl in ((1, 0xE8), (0, 0x28)):
    bus.track_windows()
    u8g2.set_flip_mode(flip)
    check(bus.madctl == [madctl], "ili9341 rot flip=%d: MADCTL %s" % (flip, [hex(m) for m in bus.madctl]))
    bus.track_windows()
    u8g2.update_display_area(39, 29, 1, 1)
    check(bus.windows == [(0x2A, 312, 319), (0x2B, 232, 239)], "ili9341 rot flip=%d: last tile %s" % (flip, bus.windows))
print("ILI9341_240X320_ROT ok")

# --- GC9A01_240X240_ROT and _ADAFRUIT_ROT: 240x240 either way, MADCTL with MV.
# flip 0 is 90 degrees clockwise (checked on the glass), flip 1 is 270 degrees
for twin in ("GC9A01_240X240", "GC9A01_240X240_ADAFRUIT"):
    bus = sim.SpiBus()
    bus.keep = 0
    bus.track_windows()
    u8g2.init(bus.softspi(), cs=bus.cs, dc=bus.dc, display=getattr(u8g2, twin))
    check(bus.madctl[-1] == 0x48, "%s: MADCTL after init %02x" % (twin, bus.madctl[-1]))
    bus = sim.SpiBus()
    bus.keep = 0
    bus.track_windows()
    u8g2.init(bus.softspi(), cs=bus.cs, dc=bus.dc, display=getattr(u8g2, twin + "_ROT"))
    check(bus.madctl[-1] == 0x28, "%s_ROT: MADCTL after init %02x" % (twin, bus.madctl[-1]))
    check((u8g2.get_display_width(), u8g2.get_display_height()) == (240, 240), twin + "_ROT: size")
    for flip, madctl in ((1, 0xE8), (0, 0x28)):
        bus.track_windows()
        u8g2.set_flip_mode(flip)
        check(bus.madctl == [madctl], "%s_ROT flip=%d: MADCTL %s" % (twin, flip, [hex(m) for m in bus.madctl]))
print("GC9A01 _ROT ok")


# --- every _ROT display against its portrait twin. It is the same panel, so both
# cover the same rectangle of the controller ram, in both flips. A width or height that is
# not a multiple of 8 has padding: that must be outside the rectangle.
def ram_pos(madctl, c0, r0, x, y, n_rows=320):
    # where pixel (x, y) of the display lands in the 240 by n_rows ram. MADCTL: MY 0x80, MX 0x40, MV 0x20.
    # CASET starts at c0, RASET at r0. MV exchanges columns and rows.
    c = c0 + x
    r = r0 + y
    if madctl & 0x20:
        c, r = r, c
    if madctl & 0x40:
        c = 239 - c
    if madctl & 0x80:
        r = n_rows - 1 - r
    return c, r


def glass(name, flip):
    bus = sim.SpiBus()
    bus.keep = 0
    bus.track_windows()
    u8g2.init(bus.softspi(), cs=bus.cs, dc=bus.dc, display=getattr(u8g2, name))
    u8g2.set_flip_mode(flip)
    madctl = bus.madctl[-1]
    w, h = u8g2.get_display_width(), u8g2.get_display_height()
    bus.track_windows()
    u8g2.update_display_area(0, 0, 1, 1)
    c0, r0 = bus.windows[0][1], bus.windows[1][1]
    n_rows = ram_size[name.split("_")[0]][1]
    corners = [ram_pos(madctl, c0, r0, x, y, n_rows) for x in (0, w - 1) for y in (0, h - 1)]
    cols = [c for c, r in corners]
    rows = [r for c, r in corners]
    pad = []
    if w % 8:
        pad.append(ram_pos(madctl, c0, r0, w, 0, n_rows))
    if h % 8:
        pad.append(ram_pos(madctl, c0, r0, 0, h, n_rows))
    return (min(cols), max(cols), min(rows), max(rows)), pad, (w, h)


rot_names = [n for n in color_names if n.endswith("_ROT")]
for name in rot_names:
    twin = name[:-4]
    ref0, pad0, (pw, ph) = glass(twin, 0)
    ref1, pad1, _ = glass(twin, 1)
    check(ref0 == ref1, "%s: flips cover the same glass %s %s" % (twin, ref0, ref1))
    check((ref0[1] - ref0[0] + 1, ref0[3] - ref0[2] + 1) == (pw, ph), "%s: glass %s is %dx%d" % (twin, ref0, pw, ph))
    for flip in (0, 1):
        rect, pad, (w, h) = glass(name, flip)
        label = "%s flip=%d" % (name, flip)
        check((w, h) == (ph, pw), "%s: size %dx%d, portrait %dx%d" % (label, w, h, pw, ph))
        check(rect == ref0, "%s: covers %s, portrait covers %s" % (label, rect, ref0))
        for c, r in pad + pad0 + pad1:
            check(not (rect[0] <= c <= rect[1] and rect[2] <= r <= rect[3]),
                  "%s: padding at %s inside the glass %s" % (label, (c, r), rect))
print("rotated displays cover the same glass as the portrait ones:", len(rot_names))
# switching to a monochrome display drops the clut map
bus = sim.I2cBus()
u8g2.init(i2c=bus.softi2c(), display=u8g2.SSD1306_I2C_128X64_NONAME)
try:
    u8g2.set_clut_color(0, 0, 0)
    check(False, "clut on monochrome should raise")
except ValueError as e:
    check(str(e) == "not a color display", str(e))
import io
s = io.StringIO()
u8g2.screenshot(s)
check(s.getvalue().startswith("P1"), "monochrome screenshot is pbm")
print("clut refused on monochrome ok")
print("PASS", passed, "checks")
