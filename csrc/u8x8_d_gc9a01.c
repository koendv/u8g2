#include "u8x8.h"

#ifdef U8X8_WITH_CLUT

/*
  GC9A01 240x240 round lcd.

  Do not confuse with GC9107 (128*128), GC9C01 (360*360), GC9D01 (160*160).

  GC9A01 init sequences vary between vendors.
  The GC9A01 datasheet is incomplete.
  Undocumented registers exist.
  Variants differ only in their init sequence:

  u8x8_d_gc9a01_240x240          after Espressif esp_lcd_gc9a01
  u8x8_d_gc9a01_240x240_adafruit after Adafruit_GC9A01A.

  https://github.com/espressif/esp-bsp/tree/master/components/lcd/esp_lcd_gc9a01
  https://blog.csdn.net/weixin_35266799/article/details/154230153
*/

#include "u8x8_clut_driver.h"

/* MADCTL (0x36) bits */
#define U8X8_D_GC9A01_MADCTL_MY 0x080       /* row address order */
#define U8X8_D_GC9A01_MADCTL_MX 0x040       /* column address order */
#define U8X8_D_GC9A01_MADCTL_MV 0x020       /* row/column exchange */
#define U8X8_D_GC9A01_MADCTL_ML 0x010       /* vertical refresh order */
#define U8X8_D_GC9A01_MADCTL_BGR 0x008      /* color order BGR. bit clear: RGB */
#define U8X8_D_GC9A01_MADCTL_MH 0x004       /* horizontal refresh order */

/* panel orientation, same for all init sequences */
#define U8X8_D_GC9A01_MADCTL (U8X8_D_GC9A01_MADCTL_MX | U8X8_D_GC9A01_MADCTL_BGR)
#define U8X8_D_GC9A01_MADCTL_FLIP (U8X8_D_GC9A01_MADCTL_MY | U8X8_D_GC9A01_MADCTL_BGR)  /* rotate 180 degrees */
#define U8X8_D_GC9A01_MADCTL_ROT (U8X8_D_GC9A01_MADCTL_MV | U8X8_D_GC9A01_MADCTL_BGR)  /* panel rotated 90 degrees clockwise */
#define U8X8_D_GC9A01_MADCTL_ROT_FLIP (U8X8_D_GC9A01_MADCTL_MX | U8X8_D_GC9A01_MADCTL_MY | U8X8_D_GC9A01_MADCTL_MV | U8X8_D_GC9A01_MADCTL_BGR)  /* rotated 270 degrees */

/* init sequence fragments */
#define U8X8_D_GC9A01_SEQ_MADCTL \
  U8X8_START_TRANSFER(), U8X8_C(0x036), U8X8_A(U8X8_D_GC9A01_MADCTL), U8X8_END_TRANSFER()
#define U8X8_D_GC9A01_SEQ_COLMOD_RGB565 \
  U8X8_START_TRANSFER(), U8X8_C(0x03a), U8X8_A(0x055), U8X8_END_TRANSFER()
#define U8X8_D_GC9A01_SEQ_INVON \
  U8X8_START_TRANSFER(), U8X8_C(0x021), U8X8_END_TRANSFER()

/* esp_lcd_panel_reset(). software reset used if no reset pin defined */
static const uint8_t u8x8_d_gc9a01_240x240_swreset_seq[] = {
  U8X8_START_TRANSFER(), U8X8_C(0x001), U8X8_END_TRANSFER(),     /* software reset */
  U8X8_DLY(20),                                                  /* spec, wait at least 5ms before sending new command */
  U8X8_END()
};

/* after esp_lcd_gc9a01 */
static const uint8_t u8x8_d_gc9a01_240x240_esplcd_init_seq[] = {
  /* esp_lcd_panel_init() */
  U8X8_START_TRANSFER(), U8X8_C(0x011), U8X8_END_TRANSFER(),     /* sleep out */
  U8X8_DLY(100),
  U8X8_D_GC9A01_SEQ_MADCTL,
  U8X8_D_GC9A01_SEQ_COLMOD_RGB565,                               /* 16 bit/pixel */

  /* vendor_specific_init_default[] */
  U8X8_START_TRANSFER(), U8X8_C(0x0fe), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0ef), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0eb), U8X8_A(0x014), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x084), U8X8_A(0x060), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x085), U8X8_A(0x0ff), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x086), U8X8_A(0x0ff), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x087), U8X8_A(0x0ff), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x08e), U8X8_A(0x0ff), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x08f), U8X8_A(0x0ff), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x088), U8X8_A(0x00a), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x089), U8X8_A(0x023), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x08a), U8X8_A(0x000), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x08b), U8X8_A(0x080), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x08c), U8X8_A(0x001), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x08d), U8X8_A(0x003), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x090), U8X8_A(0x008), U8X8_A(0x008), U8X8_A(0x008), U8X8_A(0x008), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0ff), U8X8_A(0x060), U8X8_A(0x001), U8X8_A(0x004), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0c3), U8X8_A(0x013), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0c4), U8X8_A(0x013), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0c9), U8X8_A(0x030), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0be), U8X8_A(0x011), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0e1), U8X8_A(0x010), U8X8_A(0x00e), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0df), U8X8_A(0x021), U8X8_A(0x00c), U8X8_A(0x002), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0f0), U8X8_A(0x045), U8X8_A(0x009), U8X8_A(0x008), U8X8_A(0x008), U8X8_A(0x026), U8X8_A(0x02a),
  U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0f1), U8X8_A(0x043), U8X8_A(0x070), U8X8_A(0x072), U8X8_A(0x036), U8X8_A(0x037), U8X8_A(0x06f),
  U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0f2), U8X8_A(0x045), U8X8_A(0x009), U8X8_A(0x008), U8X8_A(0x008), U8X8_A(0x026), U8X8_A(0x02a),
  U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0f3), U8X8_A(0x043), U8X8_A(0x070), U8X8_A(0x072), U8X8_A(0x036), U8X8_A(0x037), U8X8_A(0x06f),
  U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0ed), U8X8_A(0x01b), U8X8_A(0x00b), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0ae), U8X8_A(0x077), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0cd), U8X8_A(0x063), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x070), U8X8_A(0x007), U8X8_A(0x007), U8X8_A(0x004), U8X8_A(0x00e), U8X8_A(0x00f), U8X8_A(0x009),
  U8X8_A(0x007), U8X8_A(0x008), U8X8_A(0x003), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0e8), U8X8_A(0x034), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x060), U8X8_A(0x038), U8X8_A(0x00b), U8X8_A(0x06d), U8X8_A(0x06d), U8X8_A(0x039), U8X8_A(0x0f0),
  U8X8_A(0x06d), U8X8_A(0x06d), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x061), U8X8_A(0x038), U8X8_A(0x0f4), U8X8_A(0x06d), U8X8_A(0x06d), U8X8_A(0x038), U8X8_A(0x0f7),
  U8X8_A(0x06d), U8X8_A(0x06d), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x062), U8X8_A(0x038), U8X8_A(0x00d), U8X8_A(0x071), U8X8_A(0x0ed), U8X8_A(0x070), U8X8_A(0x070),
  U8X8_A(0x038), U8X8_A(0x00f), U8X8_A(0x071), U8X8_A(0x0ef), U8X8_A(0x070), U8X8_A(0x070), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x063), U8X8_A(0x038), U8X8_A(0x011), U8X8_A(0x071), U8X8_A(0x0f1), U8X8_A(0x070), U8X8_A(0x070),
  U8X8_A(0x038), U8X8_A(0x013), U8X8_A(0x071), U8X8_A(0x0f3), U8X8_A(0x070), U8X8_A(0x070), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x064), U8X8_A(0x028), U8X8_A(0x029), U8X8_A(0x0f1), U8X8_A(0x001), U8X8_A(0x0f1), U8X8_A(0x000),
  U8X8_A(0x007), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x066), U8X8_A(0x03c), U8X8_A(0x000), U8X8_A(0x0cd), U8X8_A(0x067), U8X8_A(0x045), U8X8_A(0x045),
  U8X8_A(0x010), U8X8_A(0x000), U8X8_A(0x000), U8X8_A(0x000), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x067), U8X8_A(0x000), U8X8_A(0x03c), U8X8_A(0x000), U8X8_A(0x000), U8X8_A(0x000), U8X8_A(0x001),
  U8X8_A(0x054), U8X8_A(0x010), U8X8_A(0x032), U8X8_A(0x098), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x074), U8X8_A(0x010), U8X8_A(0x045), U8X8_A(0x080), U8X8_A(0x000), U8X8_A(0x000), U8X8_A(0x04e),
  U8X8_A(0x000), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x098), U8X8_A(0x03e), U8X8_A(0x007), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x099), U8X8_A(0x03e), U8X8_A(0x007), U8X8_END_TRANSFER(),

  /* esp_lcd_panel_invert_color(true) */
  U8X8_D_GC9A01_SEQ_INVON,
  U8X8_D_GC9A01_SEQ_MADCTL,                                      /* esp_lcd_panel_mirror() */
  U8X8_END()
};

/* after Adafruit_GC9A01A initcmd[] */
static const uint8_t u8x8_d_gc9a01_240x240_adafruit_init_seq[] = {
  U8X8_START_TRANSFER(), U8X8_C(0x0ef), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0eb), U8X8_A(0x014), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0fe), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0ef), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0eb), U8X8_A(0x014), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x084), U8X8_A(0x040), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x085), U8X8_A(0x0ff), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x086), U8X8_A(0x0ff), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x087), U8X8_A(0x0ff), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x088), U8X8_A(0x00a), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x089), U8X8_A(0x021), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x08a), U8X8_A(0x000), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x08b), U8X8_A(0x080), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x08c), U8X8_A(0x001), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x08d), U8X8_A(0x001), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x08e), U8X8_A(0x0ff), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x08f), U8X8_A(0x0ff), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0b6), U8X8_A(0x000), U8X8_A(0x000), U8X8_END_TRANSFER(),
  U8X8_D_GC9A01_SEQ_MADCTL,                                      /* Adafruit: MX | BGR */
  U8X8_D_GC9A01_SEQ_COLMOD_RGB565,
  U8X8_START_TRANSFER(), U8X8_C(0x090), U8X8_A(0x008), U8X8_A(0x008), U8X8_A(0x008), U8X8_A(0x008), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0bd), U8X8_A(0x006), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0bc), U8X8_A(0x000), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0ff), U8X8_A(0x060), U8X8_A(0x001), U8X8_A(0x004), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0c3), U8X8_A(0x013), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0c4), U8X8_A(0x013), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0c9), U8X8_A(0x022), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0be), U8X8_A(0x011), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0e1), U8X8_A(0x010), U8X8_A(0x00e), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0df), U8X8_A(0x021), U8X8_A(0x00c), U8X8_A(0x002), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0f0), U8X8_A(0x045), U8X8_A(0x009), U8X8_A(0x008), U8X8_A(0x008), U8X8_A(0x026), U8X8_A(0x02a),
  U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0f1), U8X8_A(0x043), U8X8_A(0x070), U8X8_A(0x072), U8X8_A(0x036), U8X8_A(0x037), U8X8_A(0x06f),
  U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0f2), U8X8_A(0x045), U8X8_A(0x009), U8X8_A(0x008), U8X8_A(0x008), U8X8_A(0x026), U8X8_A(0x02a),
  U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0f3), U8X8_A(0x043), U8X8_A(0x070), U8X8_A(0x072), U8X8_A(0x036), U8X8_A(0x037), U8X8_A(0x06f),
  U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0ed), U8X8_A(0x01b), U8X8_A(0x00b), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0ae), U8X8_A(0x077), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0cd), U8X8_A(0x063), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x0e8), U8X8_A(0x034), U8X8_END_TRANSFER(),   /* frame rate */
  U8X8_START_TRANSFER(), U8X8_C(0x062), U8X8_A(0x018), U8X8_A(0x00d), U8X8_A(0x071), U8X8_A(0x0ed), U8X8_A(0x070), U8X8_A(0x070),
  U8X8_A(0x018), U8X8_A(0x00f), U8X8_A(0x071), U8X8_A(0x0ef), U8X8_A(0x070), U8X8_A(0x070), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x063), U8X8_A(0x018), U8X8_A(0x011), U8X8_A(0x071), U8X8_A(0x0f1), U8X8_A(0x070), U8X8_A(0x070),
  U8X8_A(0x018), U8X8_A(0x013), U8X8_A(0x071), U8X8_A(0x0f3), U8X8_A(0x070), U8X8_A(0x070), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x064), U8X8_A(0x028), U8X8_A(0x029), U8X8_A(0x0f1), U8X8_A(0x001), U8X8_A(0x0f1), U8X8_A(0x000),
  U8X8_A(0x007), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x066), U8X8_A(0x03c), U8X8_A(0x000), U8X8_A(0x0cd), U8X8_A(0x067), U8X8_A(0x045), U8X8_A(0x045),
  U8X8_A(0x010), U8X8_A(0x000), U8X8_A(0x000), U8X8_A(0x000), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x067), U8X8_A(0x000), U8X8_A(0x03c), U8X8_A(0x000), U8X8_A(0x000), U8X8_A(0x000), U8X8_A(0x001),
  U8X8_A(0x054), U8X8_A(0x010), U8X8_A(0x032), U8X8_A(0x098), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x074), U8X8_A(0x010), U8X8_A(0x085), U8X8_A(0x080), U8X8_A(0x000), U8X8_A(0x000), U8X8_A(0x04e),
  U8X8_A(0x000), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x098), U8X8_A(0x03e), U8X8_A(0x007), U8X8_END_TRANSFER(),
  U8X8_START_TRANSFER(), U8X8_C(0x035), U8X8_END_TRANSFER(),     /* tearing effect line on */
  U8X8_D_GC9A01_SEQ_INVON,
  U8X8_START_TRANSFER(), U8X8_C(0x011), U8X8_END_TRANSFER(),     /* sleep out */
  U8X8_DLY(150),
  U8X8_END()
};

/* esp_lcd_panel_disp_on_off(false) */
static const uint8_t u8x8_d_gc9a01_240x240_powersave1_seq[] = {
  U8X8_START_TRANSFER(), U8X8_C(0x028), U8X8_END_TRANSFER(),     /* display off */
  U8X8_END()
};

/* esp_lcd_panel_disp_on_off(true) */
static const uint8_t u8x8_d_gc9a01_240x240_powersave0_seq[] = {
  U8X8_START_TRANSFER(), U8X8_C(0x029), U8X8_END_TRANSFER(),     /* display on */
  U8X8_END()
};

static const uint8_t u8x8_d_gc9a01_240x240_flip0_seq[] = {
  U8X8_D_GC9A01_SEQ_MADCTL,
  U8X8_END()
};

/* rotate 180 degrees */
static const uint8_t u8x8_d_gc9a01_240x240_flip1_seq[] = {
  U8X8_START_TRANSFER(), U8X8_C(0x036), U8X8_A(U8X8_D_GC9A01_MADCTL_FLIP), U8X8_END_TRANSFER(),
  U8X8_END()
};

/* the _rot displays: MADCTL with MV, columns and rows exchanged */
static const uint8_t u8x8_d_gc9a01_240x240_rot_flip0_seq[] = {
  U8X8_START_TRANSFER(), U8X8_C(0x036), U8X8_A(U8X8_D_GC9A01_MADCTL_ROT), U8X8_END_TRANSFER(),
  U8X8_END()
};

static const uint8_t u8x8_d_gc9a01_240x240_rot_flip1_seq[] = {
  U8X8_START_TRANSFER(), U8X8_C(0x036), U8X8_A(U8X8_D_GC9A01_MADCTL_ROT_FLIP), U8X8_END_TRANSFER(),
  U8X8_END()
};

/* esp_lcd_panel_draw_bitmap() */
static void u8x8_d_gc9a01_240x240_draw_tile(u8x8_t *u8x8, uint8_t run_cnt, u8x8_tile_t *tile)
{
  uint8_t tile_cols = u8x8->display_info->tile_width;
  uint8_t tx = tile->x_pos;
  uint8_t ty = tile->y_pos;
  uint16_t py0 = (uint16_t)ty * 8;
  uint16_t py1 = py0 + 7;

  do
  {
    uint8_t cnt = tile->cnt;
    uint16_t px0 = (uint16_t)tx * 8 + u8x8->x_offset;
    uint16_t px1 = (uint16_t)(tx + cnt) * 8 + u8x8->x_offset - 1;

    u8x8_cad_StartTransfer(u8x8);
    u8x8_cad_SendCmd(u8x8, 0x02A); /* CASET: column address set */
    u8x8_cad_SendArg(u8x8, px0 >> 8);
    u8x8_cad_SendArg(u8x8, px0 & 0x0ff);
    u8x8_cad_SendArg(u8x8, px1 >> 8);
    u8x8_cad_SendArg(u8x8, px1 & 0x0ff);
    u8x8_cad_EndTransfer(u8x8);

    u8x8_cad_StartTransfer(u8x8);
    u8x8_cad_SendCmd(u8x8, 0x02B); /* RASET: row address set */
    u8x8_cad_SendArg(u8x8, py0 >> 8);
    u8x8_cad_SendArg(u8x8, py0 & 0x0ff);
    u8x8_cad_SendArg(u8x8, py1 >> 8);
    u8x8_cad_SendArg(u8x8, py1 & 0x0ff);
    u8x8_cad_EndTransfer(u8x8);

    u8x8_cad_StartTransfer(u8x8);
    u8x8_cad_SendCmd(u8x8, 0x02C); /* RAMWR: write display data to ram */
    u8x8_clut_stream_rgb565(u8x8, tile->tile_ptr, cnt, (uint16_t)ty * tile_cols + tx);
    u8x8_cad_EndTransfer(u8x8);

    tx = (uint8_t)(tx + cnt);
    run_cnt--;
  } while ( run_cnt > 0 );
}

static const u8x8_display_info_t u8x8_gc9a01_240x240_display_info = {
  /* chip_enable_level = */ 0,
  /* chip_disable_level = */ 1,
  /* post_chip_enable_wait_ns = */ 20,  /* tcss */
  /* pre_chip_disable_wait_ns = */ 20,
  /* reset_pulse_width_ms = */ 10,      /* esp_lcd_panel_reset() */
  /* post_reset_wait_ms = */ 10,        /* esp_lcd_panel_reset() */
  /* sda_setup_time_ns = */ 5,          /* tds */
  /* sck_pulse_width_ns = */ 5,         /* twrh, twrl */
  /* sck_clock_hz = */ 15000000UL,
  /* spi_mode = */ 0,
  /* i2c_bus_clock_100kHz = */ 0,
  /* data_setup_time_ns = */ 20,
  /* write_pulse_width_ns = */ 40,
  /* tile_width = */ 30, /* tile_height = */ 30,
  /* default_x_offset = */ 0, /* flipmode_x_offset = */ 0,
  /* pixel_width = */ 240, /* pixel_height = */ 240
};

static uint8_t u8x8_d_gc9a01_240x240_generic(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr, const uint8_t *init_seq, const uint8_t *flip0_seq, const uint8_t *flip1_seq)
{
  switch(msg)
  {
    case U8X8_MSG_DISPLAY_SETUP_MEMORY:
      u8x8_d_helper_display_setup_memory(u8x8, &u8x8_gc9a01_240x240_display_info);
      u8x8_clut_set_default_palette(u8x8, U8X8_CLUT_NATIVE_RGB565);
      break;

    case U8X8_MSG_DISPLAY_INIT:
      u8x8_d_helper_display_init(u8x8);
      if ( u8x8->pins[U8X8_PIN_RESET] == U8X8_PIN_NONE )
        u8x8_cad_SendSequence(u8x8, u8x8_d_gc9a01_240x240_swreset_seq);
      u8x8_cad_SendSequence(u8x8, init_seq);
      u8x8_cad_SendSequence(u8x8, flip0_seq); /* orientation of this display */
      break;

    case U8X8_MSG_DISPLAY_SET_POWER_SAVE:
      u8x8_cad_SendSequence(u8x8, arg_int == 0 ?
          u8x8_d_gc9a01_240x240_powersave0_seq : u8x8_d_gc9a01_240x240_powersave1_seq);
      break;

    case U8X8_MSG_DISPLAY_SET_FLIP_MODE:
      u8x8_cad_SendSequence(u8x8, arg_int == 0 ? flip0_seq : flip1_seq);
      break;

    case U8X8_MSG_DISPLAY_SET_CONTRAST:
      break;

    case U8X8_MSG_DISPLAY_DRAW_TILE:
      u8x8_d_gc9a01_240x240_draw_tile(u8x8, arg_int, (u8x8_tile_t *)arg_ptr);
      break;

    default:
      return 0;
  }
  return 1;
}

uint8_t u8x8_d_gc9a01_240x240(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  return u8x8_d_gc9a01_240x240_generic(u8x8, msg, arg_int, arg_ptr, u8x8_d_gc9a01_240x240_esplcd_init_seq, u8x8_d_gc9a01_240x240_flip0_seq, u8x8_d_gc9a01_240x240_flip1_seq);
}

uint8_t u8x8_d_gc9a01_240x240_adafruit(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  return u8x8_d_gc9a01_240x240_generic(u8x8, msg, arg_int, arg_ptr, u8x8_d_gc9a01_240x240_adafruit_init_seq, u8x8_d_gc9a01_240x240_flip0_seq, u8x8_d_gc9a01_240x240_flip1_seq);
}

/* the panel rotated 90 degrees. flip 0 is rotated 90 degrees clockwise, flip 1 is 270 degrees. 240x240 either way. */
uint8_t u8x8_d_gc9a01_240x240_rot(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  return u8x8_d_gc9a01_240x240_generic(u8x8, msg, arg_int, arg_ptr, u8x8_d_gc9a01_240x240_esplcd_init_seq, u8x8_d_gc9a01_240x240_rot_flip0_seq, u8x8_d_gc9a01_240x240_rot_flip1_seq);
}

uint8_t u8x8_d_gc9a01_240x240_adafruit_rot(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  return u8x8_d_gc9a01_240x240_generic(u8x8, msg, arg_int, arg_ptr, u8x8_d_gc9a01_240x240_adafruit_init_seq, u8x8_d_gc9a01_240x240_rot_flip0_seq, u8x8_d_gc9a01_240x240_rot_flip1_seq);
}

#else

/* dummy for linkage only */
uint8_t u8x8_d_gc9a01_240x240(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  (void)u8x8;
  (void)msg;
  (void)arg_int;
  (void)arg_ptr;
  return 0;
}

uint8_t u8x8_d_gc9a01_240x240_adafruit(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  (void)u8x8;
  (void)msg;
  (void)arg_int;
  (void)arg_ptr;
  return 0;
}

uint8_t u8x8_d_gc9a01_240x240_rot(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  (void)u8x8;
  (void)msg;
  (void)arg_int;
  (void)arg_ptr;
  return 0;
}

uint8_t u8x8_d_gc9a01_240x240_adafruit_rot(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  (void)u8x8;
  (void)msg;
  (void)arg_int;
  (void)arg_ptr;
  return 0;
}

#endif /* U8X8_WITH_CLUT */
