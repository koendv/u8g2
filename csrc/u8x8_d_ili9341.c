#include "u8x8.h"

#ifdef U8X8_WITH_CLUT

#include "u8x8_clut_driver.h"

#define U8X8_D_ILI9341_MADCTL 0x048

/* init sequence from adafruit */
static const uint8_t u8x8_d_ili9341_240x320_init_seq[] = {
  U8X8_START_TRANSFER(),
  U8X8_C(0x001),                     /* software reset */
  U8X8_DLY(150),

  U8X8_C(0x0EF), U8X8_A(0x003), U8X8_A(0x080), U8X8_A(0x002), /* undocumented but necessary */
  U8X8_C(0x0CF), U8X8_A(0x000), U8X8_A(0x0C1), U8X8_A(0x030),
  U8X8_C(0x0ED), U8X8_A(0x064), U8X8_A(0x003), U8X8_A(0x012), U8X8_A(0x081),
  U8X8_C(0x0E8), U8X8_A(0x085), U8X8_A(0x000), U8X8_A(0x078),
  U8X8_C(0x0CB), U8X8_A(0x039), U8X8_A(0x02C), U8X8_A(0x000), U8X8_A(0x034), U8X8_A(0x002),
  U8X8_C(0x0F7), U8X8_A(0x020),
  U8X8_C(0x0EA), U8X8_A(0x000), U8X8_A(0x000),

  U8X8_CA(0x0C0, 0x023),             /* power control 1 */
  U8X8_CA(0x0C1, 0x010),             /* power control 2 */
  U8X8_C(0x0C5), U8X8_A(0x03E), U8X8_A(0x028), /* VCOM control 1 */
  U8X8_CA(0x0C7, 0x086),             /* VCOM control 2 */
  U8X8_CA(0x036, U8X8_D_ILI9341_MADCTL), /* MADCTL: fixed orientation, BGR order */
  U8X8_CA(0x037, 0x000),             /* vertical scroll start address, zero */
  U8X8_CA(0x03A, 0x055),             /* COLMOD: 16 bit/pixel (RGB565) */
  U8X8_C(0x0B1), U8X8_A(0x000), U8X8_A(0x018), /* frame rate control */
  U8X8_C(0x0B6), U8X8_A(0x008), U8X8_A(0x082), U8X8_A(0x027), /* display function control */
  U8X8_CA(0x0F2, 0x000),             /* 3 gamma function disable */
  U8X8_CA(0x026, 0x001),             /* gamma curve selected */

  U8X8_C(0x0E0),                     /* positive gamma correction */
  U8X8_A(0x00F), U8X8_A(0x031), U8X8_A(0x02B), U8X8_A(0x00C), U8X8_A(0x00E), U8X8_A(0x008),
  U8X8_A(0x04E), U8X8_A(0x0F1), U8X8_A(0x037), U8X8_A(0x007), U8X8_A(0x010), U8X8_A(0x003),
  U8X8_A(0x00E), U8X8_A(0x009), U8X8_A(0x000),

  U8X8_C(0x0E1),                     /* negative gamma correction */
  U8X8_A(0x000), U8X8_A(0x00E), U8X8_A(0x014), U8X8_A(0x003), U8X8_A(0x011), U8X8_A(0x007),
  U8X8_A(0x031), U8X8_A(0x0C1), U8X8_A(0x048), U8X8_A(0x008), U8X8_A(0x00F), U8X8_A(0x00C),
  U8X8_A(0x031), U8X8_A(0x036), U8X8_A(0x00F),

  U8X8_C(0x011),                     /* sleep out */
  U8X8_DLY(150),
  U8X8_C(0x029),                     /* display on */
  U8X8_DLY(150),

  U8X8_END_TRANSFER(),
  U8X8_END()
};

static const uint8_t u8x8_d_ili9341_240x320_sleep_on_seq[] = {
  U8X8_START_TRANSFER(),
  U8X8_C(0x010),           /* sleep in */
  U8X8_DLY(5),
  U8X8_END_TRANSFER(),
  U8X8_END()
};

static const uint8_t u8x8_d_ili9341_240x320_sleep_off_seq[] = {
  U8X8_START_TRANSFER(),
  U8X8_C(0x011),           /* sleep out */
  U8X8_DLY(120),           /* wait 120ms after sleep before further commands */
  U8X8_END_TRANSFER(),
  U8X8_END()
};

static void u8x8_d_ili9341_240x320_draw_tile(u8x8_t *u8x8, uint8_t run_cnt, u8x8_tile_t *tile)
{
  uint8_t tile_cols = u8x8->display_info->tile_width;
  uint8_t tx = tile->x_pos;
  uint8_t ty = tile->y_pos;
  uint16_t py0 = (uint16_t)ty * 8;
  uint16_t py1 = py0 + 7;

  u8x8_cad_StartTransfer(u8x8);
  do
  {
    uint8_t cnt = tile->cnt;
    uint16_t px0 = (uint16_t)tx * 8 + u8x8->x_offset;
    uint16_t px1 = (uint16_t)(tx + cnt) * 8 + u8x8->x_offset - 1;

    u8x8_cad_SendCmd(u8x8, 0x02A); /* CASET: column address set */
    u8x8_cad_SendArg(u8x8, px0 >> 8);
    u8x8_cad_SendArg(u8x8, px0 & 0x0ff);
    u8x8_cad_SendArg(u8x8, px1 >> 8);
    u8x8_cad_SendArg(u8x8, px1 & 0x0ff);

    u8x8_cad_SendCmd(u8x8, 0x02B); /* PASET: page (row) address set */
    u8x8_cad_SendArg(u8x8, py0 >> 8);
    u8x8_cad_SendArg(u8x8, py0 & 0x0ff);
    u8x8_cad_SendArg(u8x8, py1 >> 8);
    u8x8_cad_SendArg(u8x8, py1 & 0x0ff);

    u8x8_cad_SendCmd(u8x8, 0x02C); /* RAMWR: write display data to ram */

    u8x8_clut_stream_rgb565(u8x8, tile->tile_ptr, cnt, (uint16_t)ty * tile_cols + tx);

    tx = (uint8_t)(tx + cnt);
    run_cnt--;
  } while ( run_cnt > 0 );
  u8x8_cad_EndTransfer(u8x8);
}

static const u8x8_display_info_t u8x8_ili9341_240x320_display_info = {
  /* chip_enable_level = */ 0,
  /* chip_disable_level = */ 1,
  /* post_chip_enable_wait_ns = */ 20,
  /* pre_chip_disable_wait_ns = */ 20,
  /* reset_pulse_width_ms = */ 10,
  /* post_reset_wait_ms = */ 120,
  /* sda_setup_time_ns = */ 20,
  /* sck_pulse_width_ns = */ 25,
  /* sck_clock_hz = */ 10000000UL,
  /* spi_mode = */ 0,
  /* i2c_bus_clock_100kHz = */ 0,
  /* data_setup_time_ns = */ 20,
  /* write_pulse_width_ns = */ 40,
  /* tile_width = */ 30, /* tile_height = */ 40,
  /* default_x_offset = */ 0, /* flipmode_x_offset = */ 0,
  /* pixel_width = */ 240, /* pixel_height = */ 320
};

uint8_t u8x8_d_ili9341_240x320(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  switch(msg)
  {
    case U8X8_MSG_DISPLAY_SETUP_MEMORY:
      u8x8_d_helper_display_setup_memory(u8x8, &u8x8_ili9341_240x320_display_info);
      u8x8_clut_set_default_palette(u8x8, U8X8_CLUT_NATIVE_RGB565);
      break;

    case U8X8_MSG_DISPLAY_INIT:
      u8x8_d_helper_display_init(u8x8);
      u8x8_cad_SendSequence(u8x8, u8x8_d_ili9341_240x320_init_seq);
      break;

    case U8X8_MSG_DISPLAY_SET_POWER_SAVE:
      u8x8_cad_SendSequence(u8x8, arg_int == 0 ?
          u8x8_d_ili9341_240x320_sleep_off_seq : u8x8_d_ili9341_240x320_sleep_on_seq);
      break;

    case U8X8_MSG_DISPLAY_SET_FLIP_MODE:
    case U8X8_MSG_DISPLAY_SET_CONTRAST:
      break;

    case U8X8_MSG_DISPLAY_DRAW_TILE:
      u8x8_d_ili9341_240x320_draw_tile(u8x8, arg_int, (u8x8_tile_t *)arg_ptr);
      break;

    default:
      return 0;
  }
  return 1;
}
#else

/* satisfy linkage only */
uint8_t u8x8_d_ili9341_240x320(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  (void)u8x8;
  (void)msg;
  (void)arg_int;
  (void)arg_ptr;
  return 0;
}

#endif /* U8X8_WITH_CLUT */
