#include "u8g2.h"
#include "u8x8_clut_driver.h"

/* 
  screen dump to ppm. Pattern:

  void print_callback(const char *str)
  {
      Serial.print(str);
  }
  ...
  u8g2_WriteBufferPPM(u8g2.getU8g2(), print_callback);

*/

#ifdef U8X8_WITH_CLUT

/* native clut color to 8 bit red, green, blue */
static void u8x8_clut_to_rgb888(uint16_t c, uint8_t native_bpp, uint8_t *rgb)
{
  if ( native_bpp == U8X8_CLUT_NATIVE_RGB444 )
  {
    rgb[0] = (uint8_t)(((c >> 8) & 0x0f) * 17);
    rgb[1] = (uint8_t)(((c >> 4) & 0x0f) * 17);
    rgb[2] = (uint8_t)((c & 0x0f) * 17);
  }
  else if ( native_bpp == U8X8_CLUT_NATIVE_RGB565 )
  {
    uint8_t r = (uint8_t)((c >> 11) & 0x1f);
    uint8_t g = (uint8_t)((c >> 5) & 0x3f);
    uint8_t b = (uint8_t)(c & 0x1f);
    rgb[0] = (uint8_t)((r << 3) | (r >> 2));
    rgb[1] = (uint8_t)((g << 2) | (g >> 4));
    rgb[2] = (uint8_t)((b << 3) | (b >> 2));
  }
  else
  {
    rgb[0] = rgb[1] = rgb[2] = 0;
  }
}

/*
  write the buffer as plain ppm (P3) with the colors as shown on the display:
*/
void u8g2_WriteBufferPPM(u8g2_t *u8g2, void (*out)(const char *s))
{
  u8x8_t *u8x8 = u8g2_GetU8x8(u8g2);
  const uint8_t *tile_clut_map = u8x8->tile_clut_map;
  const uint16_t *clut = &u8x8->clut[0][0];
  uint8_t tile_width = u8g2_GetBufferTileWidth(u8g2);
  uint8_t tile_height = u8g2_GetBufferTileHeight(u8g2);
  uint8_t tile_row = u8g2_GetBufferCurrTileRow(u8g2);  /* first tile row of the buffer */
  const uint8_t *buf = u8g2_GetBufferPtr(u8g2);
  uint8_t ty, row, i, col;

  out("P3\n");
  out(u8x8_utoa((uint16_t)tile_width * 8));
  out(" ");
  out(u8x8_utoa((uint16_t)tile_height * 8));
  out("\n255\n");

  for ( ty = 0; ty < tile_height; ty++ )
  {
    const uint8_t *run_tile_ptr = buf + (uint16_t)ty * tile_width * 8;
    uint16_t first_tile_number = (uint16_t)(tile_row + ty) * tile_width;

    for ( row = 0; row < 8; row++ )
    {
      for ( i = 0; i < tile_width; i++ )
      {
        const uint8_t *tile_bytes = run_tile_ptr + (uint16_t)i * 8;
        uint8_t clut_idx = u8x8_clut_get_nibble(tile_clut_map, (uint16_t)(first_tile_number + i));
        uint8_t fg[3], bg[3];

        u8x8_clut_to_rgb888(clut[(uint16_t)clut_idx * 2 + 0], u8x8->clut_native_bpp, fg);
        u8x8_clut_to_rgb888(clut[(uint16_t)clut_idx * 2 + 1], u8x8->clut_native_bpp, bg);

        for ( col = 0; col < 8; col++ )
        {
          const uint8_t *rgb = (tile_bytes[col] & (1 << row)) ? fg : bg;

          out(u8x8_utoa(rgb[0]));
          out(" ");
          out(u8x8_utoa(rgb[1]));
          out(" ");
          out(u8x8_utoa(rgb[2]));
          out("\n");
        }
      }
    }
  }
}

#endif /* U8X8_WITH_CLUT */
