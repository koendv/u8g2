/*

  ClutPalette.ino

  displays default palette.

  ST7789 or ILI9341, selected below by uncommenting one constructor line.

  Wiring: edit PIN_CS/PIN_DC/PIN_RESET/PIN_BL for your board.
  Software SPI, edit PIN_SCK/PIN_MOSI.
  Hardware SPI, default pins.
*/

#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>

// wiring, edit for your board.
#define PIN_CS 5
#define PIN_DC 4
#define PIN_RESET 25
#define PIN_BL 26
#define PIN_SCK 18
#define PIN_MOSI 23
#define SPI_FREQ 150000

// Uncomment exactly one of the four lines below.
U8G2_ST7789_240X280_F_4W_HW_SPI u8g2(U8G2_R0, /*cs=*/ PIN_CS, /*dc=*/ PIN_DC, /*reset=*/ PIN_RESET);
//U8G2_ST7789_240X280_F_4W_SW_SPI u8g2(U8G2_R0, /*clock=*/ PIN_SCK, /*data=*/ PIN_MOSI, /*cs=*/ PIN_CS, /*dc=*/ PIN_DC, /*reset=*/ PIN_RESET);
//U8G2_ILI9341_240X320_F_4W_HW_SPI u8g2(U8G2_R0, /*cs=*/ PIN_CS, /*dc=*/ PIN_DC, /*reset=*/ PIN_RESET);
//U8G2_ILI9341_240X320_F_4W_SW_SPI u8g2(U8G2_R0, /*clock=*/ PIN_SCK, /*data=*/ PIN_MOSI, /*cs=*/ PIN_CS, /*dc=*/ PIN_DC, /*reset=*/ PIN_RESET);

// 2 columns x 8 rows = 16 cells, one per CLUT entry (0..15)
#define GRID_COLS 2
#define GRID_ROWS 8

#define FONT u8g2_font_profont22_tr

static const char *const color_name[GRID_COLS * GRID_ROWS] = {
  "BLK", "BLU", "GRN", "CYN", "RED", "MAG", "BRN", "LGY",
  "DGY", "LBU", "LGN", "LCY", "LRD", "LMG", "YEL", "WHT"
};

void setup(void) {
  pinMode(PIN_BL, OUTPUT);
  analogWrite(PIN_BL, 128);  // 50%

  // sized for the larger of the two panels so it fits either one;
  // must be assigned before the first draw call below.
  static uint8_t tile_clut_map[u8x8_clut_map_size(240, 320)];
  u8g2.getU8x8()->tile_clut_map = tile_clut_map;
  u8g2.setBusClock(SPI_FREQ);

  u8g2.begin();
  u8g2.setFont(FONT);

  const uint8_t cellTW = u8x8_GetCols(u8g2.getU8x8()) / GRID_COLS;
  const uint8_t cellTH = u8x8_GetRows(u8g2.getU8x8()) / GRID_ROWS;

  u8g2.clearBuffer();
  for (uint8_t i = 0; i < GRID_COLS * GRID_ROWS; i++) {
    uint8_t col = i / GRID_ROWS, row = i % GRID_ROWS;
    uint8_t tx0 = col * cellTW, ty0 = row * cellTH;

    u8g2_SetClutRegion(u8g2.getU8g2(), tx0, ty0, cellTW, cellTH, i);

    uint16_t px0 = tx0 * 8, py0 = ty0 * 8, w = cellTW * 8, h = cellTH * 8;
    uint16_t strW = u8g2.getStrWidth(color_name[i]);
    int16_t baselineY = (int16_t)py0 + (h + u8g2.getAscent() + u8g2.getDescent()) / 2;
    u8g2.drawStr((int16_t)(px0 + (w - strW) / 2), baselineY, color_name[i]);
  }
  u8g2.sendBuffer();
}

void loop(void) {
  delay(1000);
}
