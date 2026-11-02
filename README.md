## u8g2 with color: the clut branch

This fork adds color displays (ST7789, ILI9341, GC9A01) to [u8g2](https://github.com/olikraus/u8g2/wiki) and [mui](https://github.com/olikraus/u8g2/wiki/muimanual), while keeping the memory use of a black and white display.
u8g2 draws in a 1 bpp frame buffer; each 8x8 tile gets one of 16 foreground/background color pairs, added by the display driver.

- [MicroPython](https://micropython.org/) firmware, demos and the color functions: [sys/micropython](sys/micropython/README.md)
- firmware for the boards and the [Arduino](https://www.arduino.cc/) library `U8g2_clut.zip`: [micropython-u8g2 releases](https://github.com/koendv/micropython-u8g2/releases)
- the same color scheme in hardware, a Direct Memory Access (DMA) engine for Arm microcontrollers: [clut-dma](https://github.com/koendv/clut-dma)

The rest of this page is the README of [U8g2](https://github.com/olikraus/u8g2) by olikraus.

---

[![Gitpod Ready-to-Code](https://img.shields.io/badge/Gitpod-Ready--to--Code-blue?logo=gitpod)](https://gitpod.io/#https://github.com/olikraus/u8g2) 

![https://raw.githubusercontent.com/wiki/olikraus/u8g2/ref/u8g2_logo_transparent_orange.png](https://raw.githubusercontent.com/wiki/olikraus/u8g2/ref/u8g2_logo_transparent_orange.png) 


U8g2: Library for monochrome displays, version 2

U8g2 is a monochrome graphics library for embedded devices. 
U8g2 supports monochrome OLEDs and LCDs, which include the following controllers:
SSD1305, SSD1306, SSD1309, SSD1312, SSD1316, SSD1318, SSD1320, SSD1322, 
SSD1325, SSD1327, SSD1329, SSD1606, SSD1607, SH1106, SH1107, SH1108, SH1122, 
T6963, RA8835, LC7981, PCD8544, PCF8812, HX1230, UC1601, UC1604, UC1608, UC1610, 
UC1611, UC1617, UC1638, UC1701, ST7511, ST7528, ST7565, ST7567, ST7571, ST7586, 
ST7588, ST75160, ST75256, ST75320, NT7534, ST7920, IST3020, IST3088, IST7920, 
LD7032, KS0108, KS0713, HD44102, T7932, SED1520, SBN1661, IL3820, MAX7219, 
GP1287, GP1247, GU800
(see [here](https://github.com/olikraus/u8g2/wiki/u8g2setupcpp) for a full list).

The Arduino library U8g2 can be installed from the library manager of the Arduino IDE. U8g2 also includes U8x8 library:
 * U8g2
   * Includes all graphics procedures (line/box/circle draw).
   * Supports many fonts. (Almost) no restriction on the font height.
   * Requires some memory in the microcontroller to render the display.
 * U8x8
   * Text output only (character) device.
   * Only fonts allowed with fit into a 8x8 pixel grid.
   * Writes directly to the display. No buffer in the microcontroller required.


[Setup Guide and Reference Manual](https://github.com/olikraus/u8g2/wiki)


