/*
  ClutClock.ino

  CLUT demo: analog clock on 240x280 ST7789.
  Purpose: show radial CLUT zoning and per-second color animation.

  Buffer: 1bpp framebuffer, CLUT expands to native bpp at draw time.
  Tiles are split by distance from center:
    - Tiles with center outside R_RING_ZONE -> CLUT_RING (dial)
    - Tiles with center inside  R_RING_ZONE -> CLUT_HANDS (hands)
  Per entry: bit=1 pixels get on-color, bit=0 pixels get off-color.

  Zone boundary is jagged at 8px granularity (tile center test).
  This is inherent to the CLUT approach. Do not expect a smooth
  ring edge.

  Animation:
    - Ring color cycles through hue wheel once per minute via
      rgb_cycle(). Only the CLUT_RING entry changes per tick.
    - Full framebuffer is redrawn and re-sent every second.

  ST7789 uses RGB444 (12bpp).

  RAM (240x280):
    framebuffer   240*280/8      = 8400 bytes
    tile_clut_map 240*280/8/8/2  =  525 bytes
    CLUT          16*2*2         =   64 bytes
    total                        = 8989 bytes

  Only two CLUT entries used (CLUT_RING, CLUT_HANDS). The other
  fourteen CLUT entries stay at the default palette.

  HW SPI: SPI.begin(PIN_SCK, -1, PIN_MOSI) called in setup().
  SW SPI: use SW SPI constructor instead.
*/

#include <Arduino.h>
#include <SPI.h>
#include <U8g2lib.h>

#define PIN_CS 5
#define PIN_DC 4
#define PIN_RESET 25
#define PIN_BL 26
#define PIN_SCK 18
#define PIN_MOSI 23
#define SPI_FREQ 2000000

#define MAX_PIXEL_WIDTH 240
#define MAX_PIXEL_HEIGHT 280

U8G2_ST7789_240X280_F_4W_HW_SPI u8g2(U8G2_R0, /*cs=*/PIN_CS, /*dc=*/PIN_DC, /*reset=*/PIN_RESET);

// dial geometry, pixels from center
#define R_TICK_OUTER 116
#define R_TICK_MINUTE 110
#define R_TICK_HOUR 101
#define R_NUMERAL 88
#define R_RING_ZONE 80  // tiles with center beyond this radius get the ring color
#define L_HOUR 50
#define L_MINUTE 78
#define L_SECOND 94
#define L_SECOND_TAIL 16
#define W_HOUR 4  // half width at the base
#define W_MINUTE 3
#define R_HUB 4

#define FONT_NUMERAL u8g2_font_profont17_tn

// clut indices
#define CLUT_RING 1
#define CLUT_HANDS 2

#define COLOR_BLACK 0x000000
#define COLOR_WHITE 0xFFFFFF
#define COLOR_CYAN 0x00C0FF
#define COLOR_YELLOW 0xFFE000

static int16_t cx, cy;          // dial center, center of display
static uint32_t startSecs = 0;  // random start time, seconds since midnight

static uint8_t tile_clut_map[u8x8_clut_map_size(MAX_PIXEL_WIDTH, MAX_PIXEL_HEIGHT)];

// smoothly changing color

uint32_t rgb_cycle(uint32_t t) {

  t = t * 1536 / 60;  // cycle every minute
  t %= 1536;          // t: 0..1535, then wraps

  uint32_t segment = t >> 8;  // 0..5
  uint32_t x = t & 255;       // 0..255

  uint32_t r, g, b;

  switch (segment) {
    case 0:
      r = 255;
      g = x;
      b = 0;
      break;  // red -> yellow
    case 1:
      r = 255 - x;
      g = 255;
      b = 0;
      break;  // yellow -> green
    case 2:
      r = 0;
      g = 255;
      b = x;
      break;  // green -> cyan
    case 3:
      r = 0;
      g = 255 - x;
      b = 255;
      break;  // cyan -> blue
    case 4:
      r = x;
      g = 0;
      b = 255;
      break;  // blue -> magenta
    default:
      r = 255;
      g = 0;
      b = 255 - x;
      break;  // magenta -> red
  }

  return (r << 16) | (g << 8) | b;  // 0xRRGGBB
}

// sin and cos of positions 0..7 (0..42 degrees), scaled by 1024.
static const int16_t SIN_TABLE[8] = { 0, 107, 213, 316, 416, 512, 602, 685 };
static const int16_t COS_TABLE[8] = { 1024, 1018, 1002, 974, 935, 887, 828, 761 };

// sin and cos of position pos (0..59, clockwise from 12), scaled by 1024
static void sin_cos(uint8_t pos, int16_t *s, int16_t *c) {
  uint8_t q = pos / 15, r = pos % 15;
  int16_t a, b;  // sin and cos within first quadrant
  if (r <= 7) {
    a = SIN_TABLE[r];
    b = COS_TABLE[r];
  } else {  // sin(90 - x) = cos x
    a = COS_TABLE[15 - r];
    b = SIN_TABLE[15 - r];
  }
  switch (q) {  // sin(x + 90) = cos x, cos(x + 90) = -sin x
    case 0:
      *s = a;
      *c = b;
      break;
    case 1:
      *s = b;
      *c = -a;
      break;
    case 2:
      *s = -a;
      *c = -b;
      break;
    default:
      *s = -b;
      *c = a;
      break;
  }
}

// v / 1024, rounded
static inline int16_t descale(int32_t v) {
  return (int16_t)((v + 512) >> 10);
}

// point at radius r at position pos, shifted sideways by off pixels.
static void polar(uint8_t pos, int16_t r, int16_t off, int16_t *x, int16_t *y) {
  int16_t s, c;
  sin_cos(pos, &s, &c);
  *x = cx + descale((int32_t)r * s + (int32_t)off * c);
  *y = cy + descale(-(int32_t)r * c + (int32_t)off * s);
}

// tile colors: outside R_RING_ZONE color CLUT_RING, inside R_RING_ZONE color CLUT_HANDS
static void setClutZones(void) {
  uint8_t cols = u8x8_GetCols(u8g2.getU8x8());
  uint8_t rows = u8x8_GetRows(u8g2.getU8x8());
  for (uint8_t ty = 0; ty < rows; ty++) {
    for (uint8_t tx = 0; tx < cols; tx++) {
      int16_t dx = tx * 8 + 4 - cx, dy = ty * 8 + 4 - cy;
      bool ring = (int32_t)dx * dx + (int32_t)dy * dy >= (int32_t)R_RING_ZONE * R_RING_ZONE;
      u8g2_SetClutRegion(u8g2.getU8g2(), tx, ty, 1, 1, ring ? CLUT_RING : CLUT_HANDS);
    }
  }
}

static void drawRadial(uint8_t pos, int16_t r0, int16_t r1, int16_t off) {
  int16_t x0, y0, x1, y1;
  polar(pos, r0, off, &x0, &y0);
  polar(pos, r1, off, &x1, &y1);
  u8g2.drawLine(x0, y0, x1, y1);
}

// watch hand: tip at length len, base of half width w at the center
static void drawHand(uint8_t pos, int16_t len, int16_t w) {
  int16_t xt, yt, xl, yl, xr, yr;
  polar(pos, len, 0, &xt, &yt);
  polar(pos, 0, -w, &xl, &yl);
  polar(pos, 0, w, &xr, &yr);
  u8g2.drawTriangle(xt, yt, xl, yl, xr, yr);
}

// watch dial
static void drawDial(void) {
  for (uint8_t i = 0; i < 60; i++) {
    if (i % 5 == 0) {
      for (int8_t off = -1; off <= 1; off++)
        drawRadial(i, R_TICK_HOUR, R_TICK_OUTER, off);
    } else {
      drawRadial(i, R_TICK_MINUTE, R_TICK_OUTER, 0);
    }
  }

  static const char *const numeral[4] = { "12", "3", "6", "9" };
  u8g2.setFont(FONT_NUMERAL);
  int16_t ascent = u8g2.getAscent();
  for (uint8_t i = 0; i < 4; i++) {
    int16_t x, y;
    polar(i * 15, R_NUMERAL, 0, &x, &y);
    u8g2.drawStr(x - u8g2.getStrWidth(numeral[i]) / 2, y + ascent / 2, numeral[i]);
  }
}

static void drawClock(uint32_t secs) {
  uint8_t hours = (secs / 3600) % 24;
  uint8_t minutes = (secs / 60) % 60;
  uint8_t seconds = secs % 60;

  uint8_t hourPos = (hours % 12) * 5 + minutes / 12;

  u8g2.clearBuffer();
  drawDial();
  drawHand(hourPos, L_HOUR, W_HOUR);
  drawHand(minutes, L_MINUTE, W_MINUTE);
  drawRadial(seconds, -L_SECOND_TAIL, L_SECOND, 0);
  u8g2.drawDisc(cx, cy, R_HUB);

  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_BL, OUTPUT);
  analogWrite(PIN_BL, 128);  // backlight 50%

  u8g2_SetClutMap(u8g2.getU8g2(), tile_clut_map);
  u8g2.setBusClock(SPI_FREQ);

#ifdef ARDUINO_ARCH_ESP32
  // hardware spi on PIN_SCK/PIN_MOSI
  SPI.begin(PIN_SCK, -1, PIN_MOSI);
#endif

  u8g2_SetClutColor(u8g2.getU8g2(), CLUT_RING, /*fg=*/COLOR_CYAN, /*bg=*/COLOR_BLACK);
  u8g2_SetClutColor(u8g2.getU8g2(), CLUT_HANDS, /*fg=*/COLOR_WHITE, /*bg=*/COLOR_BLACK);

  u8g2.begin();

  cx = u8g2.getDisplayWidth() / 2;
  cy = u8g2.getDisplayHeight() / 2;

  setClutZones();

#ifdef ARDUINO_ARCH_ESP32
  startSecs = esp_random() % 86400UL;
#else
  startSecs = random(86400L);
#endif
}

void loop() {
  uint32_t secs = startSecs + millis() / 1000;

  u8g2_SetClutColor(u8g2.getU8g2(), CLUT_RING, /*fg=*/rgb_cycle(secs), /*bg=*/COLOR_BLACK);
  drawClock(secs);

  delay(1000 - millis() % 1000);  // wait until next second
}
