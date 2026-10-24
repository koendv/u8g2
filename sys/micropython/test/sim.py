# bus simulators for testing the u8g2 module on the unix port with virtual pins
import machine


class Pin(machine.PinBase):
    def __init__(self, name, on_write=None, read_value=0):
        self.name = name
        self.v = 1
        self.on_write = on_write
        self.read_value = read_value

    def value(self, v=None):
        if v is None:
            return self.read_value if self.read_value is not None else self.v
        v = 1 if v else 0
        old = self.v
        self.v = v
        if self.on_write is not None:
            self.on_write(self, old, v)


class SpiBus:
    """decodes mode 0 spi: (dc, byte) for every byte while cs is active"""

    def __init__(self, cs_active=0):
        self.cs_active = cs_active
        self.mosi = Pin("mosi")
        self.miso = Pin("miso")
        self.sck = Pin("sck", self._sck)
        self.sck.v = 0
        self.cs = Pin("cs")
        self.dc = Pin("dc")
        self.bits = 0
        self.n = 0
        self.out = []
        self.has_cs = True
        self.keep = None  # keep at most this many bytes, count the rest
        self.count = 0
        self.data_count = 0
        self.windows = None

    def track_windows(self):
        """record the CASET (0x2a) and RASET (0x2b) windows of st7789, ili9341, gc9a01 as
        (cmd, start, end), and the MADCTL (0x36) values, without keeping the bytes.
        cad 011: command dc=0, arguments dc=1"""
        self.windows = []
        self.madctl = []
        self._cmd = None
        self._args = []

    def _window(self, dc, b):
        if not dc:
            self._cmd = b
            self._args = []
        elif self._cmd == 0x36:
            self.madctl.append(b)
        elif self._cmd in (0x2A, 0x2B):
            self._args.append(b)
            if len(self._args) == 4:
                a = self._args
                self.windows.append((self._cmd, a[0] << 8 | a[1], a[2] << 8 | a[3]))

    def _sck(self, pin, old, v):
        if old == 0 and v == 1:
            if self.has_cs and self.cs.v != self.cs_active:
                return
            self.bits = (self.bits << 1) | self.mosi.v
            self.n += 1
            if self.n == 8:
                self.count += 1
                self.data_count += self.dc.v
                if self.windows is not None:
                    self._window(self.dc.v, self.bits)
                if self.keep is None or len(self.out) < self.keep:
                    self.out.append((self.dc.v, self.bits))
                self.bits = 0
                self.n = 0

    def softspi(self):
        return machine.SoftSPI(baudrate=500000, sck=self.sck, mosi=self.mosi, miso=self.miso)


class I2cBus:
    """decodes i2c: list of transactions, each a list of bytes (address byte first)"""

    def __init__(self, ack=True):
        self.sda = Pin("sda", self._sda, read_value=0 if ack else 1)
        self.scl = Pin("scl", self._scl, read_value=None)
        self.trans = []
        self.cur = None
        self.bits = 0
        self.n = 0

    def _sda(self, pin, old, v):
        if self.scl.v == 1:
            if old == 1 and v == 0:  # start
                self.cur = []
                self.bits = 0
                self.n = 0
            elif old == 0 and v == 1 and self.cur is not None:  # stop
                self.trans.append(self.cur)
                self.cur = None

    def _scl(self, pin, old, v):
        if old == 0 and v == 1 and self.cur is not None:
            if self.n < 8:
                self.bits = (self.bits << 1) | self.sda.v
                self.n += 1
            else:  # ack clock
                self.cur.append(self.bits)
                self.bits = 0
                self.n = 0

    def softi2c(self):
        return machine.SoftI2C(scl=self.scl, sda=self.sda, freq=400000)


class PageRam:
    """ssd1306 / sh1106 style page addressed ram: commands and data in order"""

    def __init__(self, cols=132, pages=16):
        self.cols = cols
        self.pages = pages
        self.ram = bytearray(cols * pages)
        self.col = 0
        self.page = 0

    def cmd(self, c):
        if c < 0x10:
            self.col = (self.col & 0xF0) | c
        elif c < 0x20:
            self.col = (self.col & 0x0F) | ((c & 0x0F) << 4)
        elif 0xB0 <= c <= 0xBF:
            self.page = c & 0x0F

    def data(self, d):
        if self.col < self.cols and self.page < self.pages:
            self.ram[self.page * self.cols + self.col] = d
        self.col += 1

    def feed_spi(self, out):
        for dc, b in out:
            if dc:
                self.data(b)
            else:
                self.cmd(b)

    def feed_i2c(self, trans, addr):
        for t in trans:
            assert t[0] == addr << 1, "address %x" % t[0]
            i = 1
            while i < len(t):
                ctrl = t[i]
                i += 1
                co = ctrl & 0x80
                isdata = ctrl & 0x40
                if co:
                    if i < len(t):
                        (self.data if isdata else self.cmd)(t[i])
                        i += 1
                else:
                    for b in t[i:]:
                        (self.data if isdata else self.cmd)(b)
                    break

    def pixel(self, x, y):
        return (self.ram[(y >> 3) * self.cols + x] >> (y & 7)) & 1


def read_pbm(u8g2):
    import io
    s = io.StringIO()
    u8g2.write_buffer_pbm(s)
    tok = s.getvalue().split()
    assert tok[0] == "P1"
    w, h = int(tok[1]), int(tok[2])
    bits = tok[3:]
    if len(bits) == 1:
        bits = list(bits[0])
    rows = []
    flat = "".join(bits)
    for y in range(h):
        rows.append([int(c) for c in flat[y * w:(y + 1) * w]])
    return w, h, rows
