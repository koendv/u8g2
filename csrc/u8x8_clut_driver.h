#ifndef U8X8_CLUT_DRIVER_H
#define U8X8_CLUT_DRIVER_H

#include "u8x8.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef U8X8_WITH_CLUT

/* set default palette, 16-color vga */
void u8x8_clut_set_default_palette(u8x8_t *u8x8, uint8_t native_bpp);

/* send rgb565 to display */
void u8x8_clut_stream_rgb565(u8x8_t *u8x8,
    const uint8_t *run_tile_ptr, uint8_t cnt, uint16_t first_tile_number);

/* send rgb444 to display */
void u8x8_clut_stream_rgb444(u8x8_t *u8x8,
    const uint8_t *run_tile_ptr, uint8_t cnt, uint16_t first_tile_number);

#endif /* U8X8_WITH_CLUT */

#ifdef __cplusplus
}
#endif

#endif /* U8X8_CLUT_DRIVER_H */
