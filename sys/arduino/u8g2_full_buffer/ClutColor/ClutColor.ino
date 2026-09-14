/*
  ClutColor.ino

  Small dashboard: 4 horizontal strips. One strip is header, three have values.

  Each strip is drawn once, in setup(),
  Each strip is assigned a different CLUT (color lookup table) index, using u8g2_SetClutRegion().

  loop() redraws only the strip value.
  Strip color depends upon strip value.
  Strip color change is changing CLUT entry using u8g2_SetClutColor().
  Strip color change using u8g2_SetClutRegion() also works, but is slower.

  Color change requires display update.
  If strip value has changed, but strip color is unchanged, redraw strip value.
  If strip color has changed, redraw whole strip.

  Display resolution is 240*280. In this sketch, memory use is:
  - frame buffer, 1 bit per pixel: 240*280/8 = 8400 bytes.
  - tile_clut_map, 4 bit per 8x8 pixel tile: 240*280/8/8/2 = 520 bytes.
  - color lookup table, 16 foreground/background color pairs: 16*2*2 = 64 bytes.
  total: 8984 bytes ram.

  writeClutMap() writes tile_clut_map to console as C source.
  See ClutFlash() for a sketch that stores tile_clut_map in flash.

  Wiring: edit PIN_CS/PIN_DC/PIN_RESET/PIN_BL for your board.
  Software SPI, edit PIN_SCK/PIN_MOSI.
  Hardware SPI, default pins.
*/

#include <Arduino.h>
#include <SPI.h>
#include <stdio.h>
#include <U8g2lib.h>

// wiring, edit for your board.
// PIN_SCK/PIN_MOSI are only used by the SW SPI constructor line below.
#define PIN_CS 5
#define PIN_DC 4
#define PIN_RESET 25
#define PIN_BL 26
#define PIN_SCK 18
#define PIN_MOSI 23
#define SPI_FREQ 150000

// Uncomment exactly one of the two lines below.
U8G2_ST7789_240X280_F_4W_HW_SPI u8g2(U8G2_R0, /*cs=*/ PIN_CS, /*dc=*/ PIN_DC, /*reset=*/ PIN_RESET);
//U8G2_ST7789_240X280_F_4W_SW_SPI u8g2(U8G2_R0, /*clock=*/ PIN_SCK, /*data=*/ PIN_MOSI, /*cs=*/ PIN_CS, /*dc=*/ PIN_DC, /*reset=*/ PIN_RESET);

#define GRID_ROWS 4  // strip 0 = header, 1..3 = TEMP/SUN/LOAD
#define GAP 1        // tile rows of black between strips
#define PIXEL_WIDTH 240
#define PIXEL_HEIGHT 280

#define FONT_LABEL u8g2_font_profont17_tf
#define FONT_VALUE u8g2_font_profont22_tf

#define COLOR_GREEN 0x00FF00
#define COLOR_YELLOW 0xFFFF00
#define COLOR_RED 0xFF0000
#define COLOR_BLACK 0x000000
#define COLOR_WHITE 0xFFFFFF
#define COLOR_BLUE 0x0000AA

// value field reserved width, in characters
#define VALUE_CHARS 6

// strip geometry, tile units
static uint8_t stripTY0[GRID_ROWS];
static uint8_t stripTH;
static uint8_t panelTileWidth;

// last threshold sent to the display
static uint8_t lastBucket[GRID_ROWS] = { 0xFF, 0xFF, 0xFF, 0xFF };

static uint8_t thresholdBucket(int8_t v) {
  if (v <= 20) return 0;  // green
  if (v <= 40) return 1;  // yellow
  return 2;               // red
}

static void bucketColor(uint8_t bucket, uint32_t *fg, uint32_t *bg) {
  switch (bucket) {
    case 0: *fg = COLOR_BLACK; *bg = COLOR_GREEN; break;
    case 1: *fg = COLOR_BLACK; *bg = COLOR_YELLOW; break;
    default: *fg = COLOR_WHITE; *bg = COLOR_RED; break;
  }
}

// redraws only strip value
static void drawValueStrip(uint8_t row, int8_t value, const char *unit, uint8_t clutIdx) {
  uint16_t px0 = 0, py0 = stripTY0[row] * 8;
  uint16_t w = panelTileWidth * 8, h = stripTH * 8, halfH = h / 2;

  u8g2.setFont(FONT_VALUE);
  uint16_t valueW = (uint16_t)(VALUE_CHARS * u8g2.getMaxCharWidth());
  uint16_t valueX0 = px0 + (w - valueW) / 2;

  u8g2.setDrawColor(0);
  u8g2.drawBox(valueX0, py0 + halfH, valueW, halfH);
  u8g2.setDrawColor(1);

  char valueStr[16];
  snprintf(valueStr, sizeof(valueStr), "%3d%s", value, unit);
  uint16_t strW = u8g2.getUTF8Width(valueStr);
  int16_t line2Y = (int16_t)py0 + halfH + (halfH + u8g2.getAscent() + u8g2.getDescent()) / 2;
  u8g2.drawUTF8(px0 + (w - strW) / 2, line2Y, valueStr);

  uint8_t bucket = thresholdBucket(value);
  bool colorChanged = (bucket != lastBucket[row]);
  lastBucket[row] = bucket;

  if (colorChanged) {
    uint32_t fg, bg;
    bucketColor(bucket, &fg, &bg);
    u8g2_SetClutColor(u8g2.getU8g2(), clutIdx, fg, bg);
    u8g2.updateDisplayArea(0, stripTY0[row], panelTileWidth, stripTH);
  } else {
    uint8_t tx0 = (uint8_t)(valueX0 / 8);
    uint8_t tx1 = (uint8_t)((valueX0 + valueW + 7) / 8);
    uint8_t ty0 = (uint8_t)((py0 + halfH) / 8);
    uint8_t ty1 = (uint8_t)((py0 + h + 7) / 8);
    u8g2.updateDisplayArea(tx0, ty0, (uint8_t)(tx1 - tx0), (uint8_t)(ty1 - ty0));
  }
}

// prints tile_clut_map as C source array literal on Serial.
// dev tool, not part of normal usage.
// run once, copy array into ClutFlash.ino,
// then comment out Serial.begin()/writeClutMap() calls below.

static void writeClutMap(const char *name) {
  u8x8_t *u8x8 = u8g2.getU8x8();
  const uint8_t *map = u8x8->tile_clut_map;
  uint16_t mapSize = u8x8_clut_map_size(PIXEL_WIDTH, PIXEL_HEIGHT);
  uint16_t bytesPerRow = (panelTileWidth + 1) / 2;

  Serial.print("static const uint8_t ");
  Serial.print(name);
  Serial.println("[] = {");
  for (uint16_t i = 0; i < mapSize; i++) {
    char buf[6];
    snprintf(buf, sizeof(buf), "0x%02X,", map[i]);
    Serial.print(buf);
    Serial.print(((i % bytesPerRow) == bytesPerRow - 1 || i == mapSize - 1) ? "\n" : " ");
  }
  Serial.println("};");
}

void setup(void) {
  Serial.begin(115200);
  delay(2000);  // let the host's terminal reconnect

  pinMode(PIN_BL, OUTPUT);
  analogWrite(PIN_BL, 128);  // 50%

  static uint8_t tile_clut_map[u8x8_clut_map_size(PIXEL_WIDTH, PIXEL_HEIGHT)];
  u8g2_SetClutMap(u8g2.getU8g2(), tile_clut_map);
  u8g2.setBusClock(SPI_FREQ);

  /* initialize CLUT */
  u8g2_SetClutColor(u8g2.getU8g2(), 1, /*fg=*/COLOR_WHITE, /*bg=*/COLOR_BLUE);
  for (uint8_t row = 1; row < GRID_ROWS; row++) {
    u8g2_SetClutColor(u8g2.getU8g2(), row + 1, /*fg=*/COLOR_BLACK, /*bg=*/COLOR_GREEN);
  }

  u8g2.begin();

  panelTileWidth = u8x8_GetCols(u8g2.getU8x8());
  uint8_t panelTileHeight = u8x8_GetRows(u8g2.getU8x8());
  stripTH = (panelTileHeight - (GRID_ROWS - 1) * GAP) / GRID_ROWS;
  for (uint8_t row = 0; row < GRID_ROWS; row++) {
    stripTY0[row] = row * (stripTH + GAP);
    u8g2_SetClutRegion(u8g2.getU8g2(), 0, stripTY0[row], panelTileWidth, stripTH, row + 1);
  }

  u8g2.clearBuffer();

  // header: drawn once, never touched again
  u8g2.setFont(FONT_VALUE);
  uint16_t w = panelTileWidth * 8, h = stripTH * 8;
  const char *title = "SOLAR CELL";
  uint16_t strW = u8g2.getStrWidth(title);
  int16_t titleY = (int16_t)(h + u8g2.getAscent() + u8g2.getDescent()) / 2;
  u8g2.drawStr((w - strW) / 2, titleY, title);
  u8g2_SetClutColor(u8g2.getU8g2(), 1, /*fg=*/COLOR_WHITE, /*bg=*/COLOR_BLUE);

  // strip labels: drawn once
  static const char *const kLabels[GRID_ROWS] = { NULL, "TEMP", "SUN", "LOAD" };
  uint16_t halfH = h / 2;
  u8g2.setFont(FONT_LABEL);
  for (uint8_t row = 1; row < GRID_ROWS; row++) {
    uint16_t rowPy0 = stripTY0[row] * 8;
    int16_t line1Y = (int16_t)rowPy0 + (halfH + u8g2.getAscent() + u8g2.getDescent()) / 2;
    u8g2.drawUTF8(4, line1Y, kLabels[row]);

    lastBucket[row] = thresholdBucket(0);
    uint32_t fg, bg;
    bucketColor(lastBucket[row], &fg, &bg);
    u8g2_SetClutColor(u8g2.getU8g2(), row + 1, fg, bg);
  }

  u8g2.sendBuffer();

  // one-time capture - see ClutFlash.ino
  writeClutMap("tile_clut_map_0");
}

void loop(void) {
  static int8_t v[3] = { 0, 0, 0 };
  static int8_t d[3] = { 2, 3, 5 };
  static uint32_t lastTick = 0;

  if (millis() - lastTick < 1000)
    return;
  lastTick += 1000;

  for (uint8_t i = 0; i < 3; i++) {
    v[i] += d[i];
    if (v[i] <= 0 || v[i] >= 60)
      d[i] = -d[i];
  }

  drawValueStrip(1, v[0], "\xc2\xb0" "C", 2);
  drawValueStrip(2, v[1], "%", 3);
  drawValueStrip(3, v[2], "%", 4);
}
