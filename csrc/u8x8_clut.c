#include "u8g2.h"
#include "u8x8_clut_driver.h"

#ifdef U8X8_WITH_CLUT

/* classic 16 color VGA/CGA palette, 24-bit rgb888 {foreground, background} pairs */
static const clut_rgb888_t u8x8_clut_default_table = {
  { 0xFFFFFF, 0x000000 }, /*  0: black */
  { 0xFFFFFF, 0x0000AD }, /*  1: blue */
  { 0xFFFFFF, 0x00AA00 }, /*  2: green */
  { 0xFFFFFF, 0x00AAAD }, /*  3: cyan */
  { 0xFFFFFF, 0xAD0000 }, /*  4: red */
  { 0xFFFFFF, 0xAD00AD }, /*  5: magenta */
  { 0xFFFFFF, 0xAD5500 }, /*  6: brown */
  { 0x000000, 0xADAAAD }, /*  7: light gray */
  { 0xFFFFFF, 0x525552 }, /*  8: dark gray */
  { 0x000000, 0x5255FF }, /*  9: light blue */
  { 0x000000, 0x52FF52 }, /* 10: light green */
  { 0x000000, 0x52FFFF }, /* 11: light cyan */
  { 0x000000, 0xFF5552 }, /* 12: light red */
  { 0x000000, 0xFF55FF }, /* 13: light magenta */
  { 0x000000, 0xFFFF52 }, /* 14: yellow */
  { 0x000000, 0xFFFFFF }, /* 15: white */
};

/* quantize a 24-bit rgb888 color down to rgb565 or rgb444 */
static uint16_t u8x8_clut_quantize(uint32_t rgb888, uint8_t native_bpp)
{
  uint8_t r8 = (uint8_t)(rgb888 >> 16);
  uint8_t g8 = (uint8_t)(rgb888 >> 8);
  uint8_t b8 = (uint8_t)rgb888;

  if ( native_bpp == U8X8_CLUT_NATIVE_RGB444 )
    return (uint16_t)((((uint16_t)(r8 >> 4)) << 8) | (((uint16_t)(g8 >> 4)) << 4) | (uint16_t)(b8 >> 4));

  return (uint16_t)((((uint16_t)(r8 >> 3)) << 11) | (((uint16_t)(g8 >> 2)) << 5) | (uint16_t)(b8 >> 3));
}

/* set default vga palette */
void u8x8_clut_set_default_palette(u8x8_t *u8x8, uint8_t native_bpp)
{
  uint8_t i;

  u8x8->clut_native_bpp = native_bpp;
  for ( i = 0; i < U8X8_CLUT_ENTRY_CNT; i++ )
  {
    u8x8->clut[i][0] = u8x8_clut_quantize(u8x8_clut_default_table[i][0], native_bpp);
    u8x8->clut[i][1] = u8x8_clut_quantize(u8x8_clut_default_table[i][1], native_bpp);
  }
}

/* set a single foregound/background color pair from rgb888 */
void u8g2_SetClutColor(u8g2_t *u8g2, uint8_t idx, uint32_t fg, uint32_t bg)
{
  u8x8_t *u8x8 = u8g2_GetU8x8(u8g2);
  if ( idx >= U8X8_CLUT_ENTRY_CNT )
    return;
  u8x8->clut[idx][0] = u8x8_clut_quantize(fg, u8x8->clut_native_bpp);
  u8x8->clut[idx][1] = u8x8_clut_quantize(bg, u8x8->clut_native_bpp);
}

/* tile_clut_map is two 4 bit clut indices per byte, low nibble first. */
static inline void u8x8_clut_set_nibble(uint8_t *map, uint16_t index, uint8_t value)
{
  uint8_t *ptr = map + (index >> 1);
  if ( index & 1 )
    *ptr = (uint8_t)((*ptr & 0x0f) | (value << 4));
  else
    *ptr = (uint8_t)((*ptr & 0xf0) | (value & 0x0f));
}

static inline uint8_t u8x8_clut_get_nibble(const uint8_t *map, uint16_t index)
{
  uint8_t b = map[index >> 1];
  return (index & 1) ? (uint8_t)(b >> 4) : (uint8_t)(b & 0x0f);
}

void u8g2_SetClutRegion(u8g2_t *u8g2, uint8_t tx, uint8_t ty, uint8_t tw, uint8_t th, uint8_t idx)
{
  u8x8_t *u8x8 = u8g2_GetU8x8(u8g2);
  const u8x8_display_info_t *info = u8x8->display_info;
  uint8_t tile_cols = info->tile_width;
  uint8_t tile_rows = info->tile_height;
  uint16_t x, y;

  if ( idx >= U8X8_CLUT_ENTRY_CNT )
    return;

  if ( u8x8->tile_clut_map == NULL )
    return;

  /* clip to display */
  if ( tx >= tile_cols || ty >= tile_rows )
    return;
  if ( tw > (uint8_t)(tile_cols - tx) )
    tw = (uint8_t)(tile_cols - tx);
  if ( th > (uint8_t)(tile_rows - ty) )
    th = (uint8_t)(tile_rows - ty);

  for ( y = ty; y < (uint16_t)ty + th; y++ )
    for ( x = tx; x < (uint16_t)tx + tw; x++ )
      u8x8_clut_set_nibble(u8x8->tile_clut_map, y * tile_cols + x, idx);
}

void u8g2_SetClutMap(u8g2_t *u8g2, uint8_t *map)
{
  u8g2_GetU8x8(u8g2)->tile_clut_map = map;
}

/* send a run of tiles to rgb565 display */
void u8x8_clut_stream_rgb565(u8x8_t *u8x8,
    const uint8_t *run_tile_ptr, uint8_t cnt, uint16_t first_tile_number)
{
  const uint8_t *tile_clut_map = u8x8->tile_clut_map;
  const uint16_t *clut = &u8x8->clut[0][0];
  uint8_t buf[254]; /* 1 pixel -> 2 bytes, 254 = multiple of 2 */
  uint8_t pos;
  uint8_t row, i;
  uint16_t fg_cache[cnt];
  uint16_t bg_cache[cnt];

  if ( tile_clut_map )
  {
    for ( i = 0; i < cnt; i++ )
    {
      uint8_t clut_idx = u8x8_clut_get_nibble(tile_clut_map, (uint16_t)(first_tile_number + i));
      fg_cache[i] = clut[(uint16_t)clut_idx * 2 + 0];
      bg_cache[i] = clut[(uint16_t)clut_idx * 2 + 1];
    }
  }
  else
  {
    for ( i = 0; i < cnt; i++ )
    {
      fg_cache[i] = clut[0];
      bg_cache[i] = clut[1];
    }
  }

  pos = 0;

  for ( row = 0; row < 8; row++ )
  {
    for ( i = 0; i < cnt; i++ )
    {
      const uint8_t *tile_bytes = run_tile_ptr + (uint16_t)i * 8;
      uint8_t col;
      uint16_t fg = fg_cache[i], bg = bg_cache[i];

      for ( col = 0; col < 8; col++ )
      {
        uint16_t color_be = (tile_bytes[col] & (1 << row)) ? fg : bg;

        buf[pos++] = (uint8_t)(color_be >> 8);
        buf[pos++] = (uint8_t)(color_be & 0x0ff);
        if ( pos == sizeof(buf) )
        {
          u8x8_cad_SendData(u8x8, pos, buf);
          pos = 0;
        }
      }
    }
  }

  if ( pos > 0 )
    u8x8_cad_SendData(u8x8, pos, buf);
}

/* send a run of tiles to rgb444 display */
void u8x8_clut_stream_rgb444(u8x8_t *u8x8,
    const uint8_t *run_tile_ptr, uint8_t cnt, uint16_t first_tile_number)
{
  const uint8_t *tile_clut_map = u8x8->tile_clut_map;
  const uint16_t *clut = &u8x8->clut[0][0];
  uint8_t buf[252]; /* 2 pixel -> 3 bytes, 252 = multiple of 3 */
  uint8_t pos;
  uint8_t row, i;
  uint16_t fg_cache[cnt];
  uint16_t bg_cache[cnt];

  if ( tile_clut_map )
  {
    for ( i = 0; i < cnt; i++ )
    {
      uint8_t clut_idx = u8x8_clut_get_nibble(tile_clut_map, (uint16_t)(first_tile_number + i));
      fg_cache[i] = clut[(uint16_t)clut_idx * 2 + 0];
      bg_cache[i] = clut[(uint16_t)clut_idx * 2 + 1];
    }
  }
  else
  {
    for ( i = 0; i < cnt; i++ )
    {
      fg_cache[i] = clut[0];
      bg_cache[i] = clut[1];
    }
  }

  pos = 0;

  for ( row = 0; row < 8; row++ )
  {
    for ( i = 0; i < cnt; i++ )
    {
      const uint8_t *tile_bytes = run_tile_ptr + (uint16_t)i * 8;
      uint8_t pair;
      uint16_t fg = fg_cache[i], bg = bg_cache[i];

      for ( pair = 0; pair < 4; pair++ )
      {
        uint16_t color0 = (tile_bytes[pair * 2 + 0] & (1 << row)) ? fg : bg;
        uint16_t color1 = (tile_bytes[pair * 2 + 1] & (1 << row)) ? fg : bg;

        /* ST7789V3 datasheet 8.7.14, 12bpp: 2 pixels -> 3 bytes, R0G0|B0R1|G1B1 */
        buf[pos++] = (uint8_t)(color0 >> 4);
        buf[pos++] = (uint8_t)(((color0 & 0x0f) << 4) | ((color1 >> 8) & 0x0f));
        buf[pos++] = (uint8_t)(color1 & 0x0ff);
        if ( pos == sizeof(buf) )
        {
          u8x8_cad_SendData(u8x8, pos, buf);
          pos = 0;
        }
      }
    }
  }

  if ( pos > 0 )
    u8x8_cad_SendData(u8x8, pos, buf);
}

#endif /* U8X8_WITH_CLUT */
