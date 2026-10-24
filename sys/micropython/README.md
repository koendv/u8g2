# u8g2 and mui for MicroPython

Goal is using low-cost commodity color displays from smartwatches, while keeping memory use similar to black-and-white displays.

## description

- [u8g2](https://github.com/olikraus/u8g2/wiki) is monochrome graphics for micro-controllers: drawing text, lines, rectangles, etc.
- [mui](https://github.com/olikraus/u8g2/wiki/muimanual) is a user interface for micro-controllers: menus, click boxes, scroll bars, etc.
- this project extends u8g2 and mui to color displays: st7789, ili9341, gc9a01.
- a demo of micropython with u8g2/mui on three boards:
    - sunton esp32-2424S012: riscv processor, gc9a01 display, cst816t touch.
    - waveshare esp32-s3-touch-lcd-1.69: xtensa processor, st7789 display, cst816 touch.
    - weact stm32u585: arm processor, st7789 display

## building

How to compile and flash the firmware

### with docker

`tools/docker/Dockerfile` builds the firmware of all boards, with pinned versions of
ESP-IDF, arm-none-eabi-gcc and MicroPython. From the top of the u8g2 tree:

```
docker build -f sys/micropython/tools/docker/Dockerfile \
  --build-arg U8G2_COMMIT=$(git rev-parse HEAD) -t u8g2-micropython .
id=$(docker create u8g2-micropython)
docker cp "$id":/output ./firmware
docker rm "$id"
```

`./firmware` then has `<board>.bin` for esp32 boards, `<board>.dfu` and `.elf` for stm32 boards,
`U8g2_clut.zip`, the Arduino library with the color drivers (`tools/release/arduino_clut/make_library.sh`),
and `SHA256SUMS`. To build some boards only: `--build-arg BOARDS="ESP32_SUNTON_2424S012"`.

### sunton esp32-2424S012

First install

- micropython source: `git clone http://github.com/micropython/micropython`
- espressif build tools
- u8g2 with clut: `git clone -b clut http://github.com/koendv/u8g2`

Follow the instructions in ~/src/micropython/ports/esp32/README.md .

Then:

```
. ~/.espressif/v5.5.5/esp-idf/export.sh
cd ~/src/micropython
make -C mpy-cross
cd ports/esp32
make BOARD_DIR=~/src/u8g2/sys/micropython/boards/ESP32_SUNTON_2424S012 submodules
make BOARD_DIR=~/src/u8g2/sys/micropython/boards/ESP32_SUNTON_2424S012
cd build-ESP32_SUNTON_2424S012
python -m esptool --chip esp32c3 -p /dev/ttyACM0 -b 460800 write_flash "@flash_args"
```

If micropython prints "The filesystem appears to be corrupted" and the REPL does not start,
erase the filesystem area, the flash after the firmware (the 2 MiB from 0x200000), and reset:

```
python -m esptool --chip esp32c3 -p /dev/ttyACM0 erase_region 0x200000 0x200000
```

### waveshare esp32-s3-touch-lcd-1.69

As the sunton, with the board directory `WAVESHARE_ESP32_S3_TOUCH_LCD_1_69`:

```
make BOARD_DIR=~/src/u8g2/sys/micropython/boards/WAVESHARE_ESP32_S3_TOUCH_LCD_1_69 submodules
make BOARD_DIR=~/src/u8g2/sys/micropython/boards/WAVESHARE_ESP32_S3_TOUCH_LCD_1_69
cd build-WAVESHARE_ESP32_S3_TOUCH_LCD_1_69
python -m esptool --chip esp32s3 -p /dev/ttyACM0 -b 460800 write_flash "@flash_args"
```

Pins are from the CircuitPython board definition, for the board with 8 MiB octal PSRAM.
On battery, the PWR key switches the board on; the firmware then sets SYS_EN (GPIO41) to keep it on.
`Pin.board.SYS_EN.value(0)` switches the board off.

### weact stm32u585 with st7789

WeAct mini-stm32u585 with 240x280 st7789.

Wiring:

| ST7789 | STM32U585 | |
|---|---|---|
| SCL | PA5 | SPI1 SCK |
| SDA | PA7 | SPI1 MOSI |
| RES | PB2 | |
| DC | PB1 | |
| CS | PA4 | GPIO |
| BLK | PB0 | TIM3 channel 3 |
| VCC, GND | 3V3, GND | |

First install

- micropython source: `git clone http://github.com/micropython/micropython`
- arm-none-eabi-gcc, e.g. from [xpack](https://github.com/xpack)
- u8g2 with clut: `git clone -b clut http://github.com/koendv/u8g2`

Follow the instructions in ~/src/micropython/ports/stm32/README.md .

Then:

```
cd ~/src/micropython
make -C mpy-cross
cd ports/stm32
make BOARD=WEACTSTUDIO_MINI_STM32U585 submodules
make BOARD_DIR=~/src/u8g2/sys/micropython/boards/WEACTSTUDIO_MINI_STM32U585 \
     USER_C_MODULES=~/src/u8g2/sys/micropython
```

Flash with dfu. To put the board in dfu mode:

1. Press BOOT0 and NRST. Wait one second.
2. Release NRST. Keep BOOT0 pressed. Wait one second.
3. Release BOOT0. Wait one second.

When the board is in dfu mode, `lsusb` shows `0483:df11`. Then:

```
dfu-util -d 0483:df11 -a 0 -s 0x08000000:leave -D build-WEACTSTUDIO_MINI_STM32U585/firmware.bin
```

### mui on large displays

The x and y position of a mui field is a single byte, 0..255.
To use mui on displays larger than 255 pixels, `MUI_U8G2_HIRES` is defined.
With `MUI_U8G2_HIRES`, x and/or y of a field are multiplied by 1, 2, 4 or 8, the smallest factor that reaches the last pixel of the display.

Example: 240*280 display.

- without MUI_U8G2_HIRES, mui can only use 240*256 pixels of the display.
- with MUI_U8G2_HIRES, x coordinates stay the same, y coordinates are multiplied by 2, and mui can use the complete display.

### displays

Each board directory has a `displays.txt`, the display drivers compiled in the firmware of that board,
and a `fonts.txt` (see fonts). Both are required: a board without them does not build.

The display table and font subset are generated in the build directory when the build is configured,
by `u8g2/tools/gen_u8g2.py`. After editing a list, run make again.

To see all display drivers: `python3 u8g2/tools/gen_u8g2.py --list`

The display table comes from codebuild, the u8g2 code generator. codebuild is compiled with the host compiler,
`cc` by default. Another compiler: `make U8G2_HOST_CC=gcc` or `cmake -DU8G2_HOST_CC=gcc`.
`U8G2_CONFIG_DIR` overrides the directory with `displays.txt` and `fonts.txt`, default the board directory.

Display initialization:

```
u8g2.init(i2c=i2c, display=u8g2.SSD1306_I2C_128X64_NONAME)
u8g2.init(i2c=i2c, addr=0x3d, display=u8g2.SSD1306_I2C_128X64_NONAME)
u8g2.init(spi, cs=Pin("A4"), dc=Pin("B1"), rst=Pin("B2"), display=u8g2.SSD1306_128X64_NONAME)
```

### touch panel

Two drivers:

- `touch.py` for a menu you drive by swiping.  (swipes up, down, left and right, click, double click, long press).
- `dpad.py` for a menu you drive by tapping zones of the screen. Tap left, right, top, bottom of the display for left, right, up, down. Tap center for select. Long press for long. Auto-repeats.

### rotation

For rotation use the `_ROT` drivers; not U8G2_R1 to R3. If U8G2_R1 to R3 are used the monochrome image rotates, but not the color.

A color display with a name ending in `_ROT` is rotated 90 degrees clockwise. Rotation is in hardware (MADCTL).

Touch panel rotation and display rotation are independent. 

To rotate touch panel input to mui, add a `rotation=0..3` parameter:
`touch.init(rotation=1)` if decoding swipes,  `dpad.DPad(touch, width, height, rotation=1)` if using taps.

`u8g2.set_flip_mode(1)` rotates 180 degrees.

### fonts

`fonts.txt` in the board directory lists the fonts compiled in the firmware of that board.
The first font is the font after `u8g2.init()`.

Before removing a font, check the demos frozen in the board's `manifest.py`.

## demos

### hello

`examples/hello.py` prints "Hello!" on the display. Uncomment the driver compiled in the firmware.

### analog clock

```
import demo_clock
demo_clock.run()
```

### u8g2 graphics

```
import demo_graphics
demo_graphics.run()
```

### mui with keyboard input

```
import demo_mui
demo_mui.run_keys()
```

use arrow keys, wasd keys, spacebar and enter.

### mui with touchscreen input

```
import demo_mui
demo_mui.run_touch()
```

## AI assistance

Written with AI assistance, including:

The MicroPython driver for the CST816T touch is mechanically rewritten from C source with Claude Code.

The MicroPython bindings were mechanically rewritten from the u8g2 C headers
with Claude Code.

Output compared with the C code and tested on hardware.

