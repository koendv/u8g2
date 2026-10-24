# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer

# "Hello!" on any display in the firmware, black and white.
# Edit the pins, uncomment the init() line of your display, comment the others.
# The displays in the firmware are the ones in displays.txt of the board.
#
#   >>> import hello

from machine import Pin, SPI, I2C
import u8g2

# wiring: change to your board. i2c displays use the i2c bus, the others spi.
spi = SPI(1, baudrate=8_000_000)
# spi = SPI(1, baudrate=1_000_000, polarity=1, phase=1)  # ST7920: spi mode 3
# i2c = I2C(0, freq=400_000)
cs = Pin(5)
dc = Pin(4)
rst = Pin(2)
bl = Pin(3)  # backlight, color displays only

# --- color lcd, spi ---
u8g2.init(spi, cs=cs, dc=dc, rst=rst, bl=bl, display=u8g2.ST7789_240X280)
# u8g2.init(spi, cs=cs, dc=dc, rst=rst, bl=bl, display=u8g2.ST7789_240X240)
# u8g2.init(spi, cs=cs, dc=dc, rst=rst, bl=bl, display=u8g2.ST7789_240X320)
# u8g2.init(spi, cs=cs, dc=dc, rst=rst, bl=bl, display=u8g2.ST7789_135X240)
# u8g2.init(spi, cs=cs, dc=dc, rst=rst, bl=bl, display=u8g2.ST7789_172X320)
# u8g2.init(spi, cs=cs, dc=dc, rst=rst, bl=bl, display=u8g2.ST7789_170X320)
# u8g2.init(spi, cs=cs, dc=dc, rst=rst, bl=bl, display=u8g2.ILI9341_240X320)
# u8g2.init(spi, cs=cs, dc=dc, rst=rst, bl=bl, display=u8g2.GC9A01_240X240)
# u8g2.init(spi, cs=cs, dc=dc, rst=rst, bl=bl, display=u8g2.GC9A01_240X240_ADAFRUIT)

# --- monochrome oled, i2c. address 0x3c, other address: addr=0x3d ---
# u8g2.init(i2c=i2c, display=u8g2.SSD1306_I2C_128X64_NONAME)
# u8g2.init(i2c=i2c, display=u8g2.SSD1306_I2C_128X32_UNIVISION)
# u8g2.init(i2c=i2c, display=u8g2.SSD1306_I2C_72X40_ER)
# u8g2.init(i2c=i2c, display=u8g2.SSD1306_I2C_64X48_ER)
# u8g2.init(i2c=i2c, display=u8g2.SSD1306_I2C_64X32_NONAME)
# u8g2.init(i2c=i2c, display=u8g2.SH1106_I2C_128X64_NONAME)
# u8g2.init(i2c=i2c, display=u8g2.SH1107_I2C_64X128)
# u8g2.init(i2c=i2c, display=u8g2.SH1107_I2C_128X128)
# u8g2.init(i2c=i2c, display=u8g2.SSD1309_I2C_128X64_NONAME2)
# u8g2.init(i2c=i2c, display=u8g2.SSD1315_I2C_128X64_NONAME)
# u8g2.init(i2c=i2c, display=u8g2.SSD1327_I2C_WS_128X128)

# --- monochrome oled, spi ---
# u8g2.init(spi, cs=cs, dc=dc, rst=rst, display=u8g2.SSD1306_128X64_NONAME)
# u8g2.init(spi, cs=cs, dc=dc, rst=rst, display=u8g2.SH1106_128X64_NONAME)
# u8g2.init(spi, cs=cs, dc=dc, rst=rst, display=u8g2.SSD1309_128X64_NONAME2)
# u8g2.init(spi, cs=cs, dc=dc, rst=rst, display=u8g2.SSD1322_NHD_256X64)
# u8g2.init(spi, cs=cs, dc=dc, rst=rst, display=u8g2.SSD1327_WS_128X128)

# --- monochrome lcd, spi ---
# u8g2.init(spi, cs=cs, rst=rst, display=u8g2.ST7920_S_128X64)  # no dc. cs is active high
# u8g2.init(spi, cs=cs, dc=dc, rst=rst, display=u8g2.ST7567_JLX12864)
# u8g2.init(spi, cs=cs, dc=dc, rst=rst, display=u8g2.ST7565_ERC12864)
# u8g2.init(spi, cs=cs, dc=dc, rst=rst, display=u8g2.UC1701_MINI12864)
# u8g2.init(spi, cs=cs, dc=dc, rst=rst, display=u8g2.PCD8544_84X48)

u8g2.set_font(u8g2.font_profont12_tf)
text = "Hello!"
x = (u8g2.get_display_width() - u8g2.get_str_width(text)) // 2
y = (u8g2.get_display_height() + u8g2.get_ascent()) // 2
u8g2.clear_buffer()
u8g2.draw_str(x, y, text)
u8g2.send_buffer()
