#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer
# Arduino library U8g2_clut: U8g2 with the color drivers (st7789, ili9341, gc9a01).
# Same layout as the official U8g2 Arduino release (see ../arduino/create_release.sh),
# made from this tree only. U8X8_WITH_CLUT is switched on in src/clib/u8x8.h,
# so sketches need no build flags, and MUI_U8G2_HIRES in src/clib/mui_u8g2.h,
# as in the MicroPython firmware.
#
# usage: make_library.sh <output dir>
# writes <output dir>/U8g2_clut.zip; install with
#   arduino-cli lib install --zip-path U8g2_clut.zip
# or Sketch > Include Library > Add .ZIP Library. Remove the official U8g2 first:
# both have U8g2lib.h.
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
U8G2=$(cd "$HERE/../../.." && pwd)
mkdir -p "$1"
OUT=$(cd "$1" && pwd)
NAME=U8g2_clut
VERSION=$(sh "$U8G2/tools/release/print_release.sh")

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
LIB=$WORK/$NAME
mkdir -p "$LIB/src/clib" "$LIB/examples" "$LIB/extras"

# C++ files, with the C include path of the release layout
cp "$U8G2"/cppsrc/*.cpp "$U8G2"/cppsrc/*.h "$LIB/src/"
sed -i 's|u8g2.h|clib/u8g2.h|g' "$LIB/src/U8g2lib.h"
sed -i 's|u8x8.h|clib/u8x8.h|g' "$LIB/src/U8x8lib.h"
sed -i 's|mui.h|clib/mui.h|g' "$LIB/src/MUIU8g2.h"
sed -i 's|mui_u8g2.h|clib/mui_u8g2.h|g' "$LIB/src/MUIU8g2.h"

# C files, without the host-only u8x8_d_stdio.c
cp "$U8G2"/csrc/*.c "$U8G2"/csrc/*.h "$LIB/src/clib/"
rm "$LIB/src/clib/u8x8_d_stdio.c"

# color drivers always on
sed -i 's|^//#define U8X8_WITH_CLUT$|#define U8X8_WITH_CLUT|' "$LIB/src/clib/u8x8.h"
grep -q '^#define U8X8_WITH_CLUT$' "$LIB/src/clib/u8x8.h" || {
    echo "make_library.sh: U8X8_WITH_CLUT not found in csrc/u8x8.h" >&2
    exit 1
}

# mui fields reach the whole display on displays of 255 pixels or more
sed -i 's|^//#define MUI_U8G2_HIRES$|#define MUI_U8G2_HIRES|' "$LIB/src/clib/mui_u8g2.h"
grep -q '^#define MUI_U8G2_HIRES$' "$LIB/src/clib/mui_u8g2.h" || {
    echo "make_library.sh: MUI_U8G2_HIRES not found in csrc/mui_u8g2.h" >&2
    exit 1
}

# examples: the ones of the official release, taken from its create_release.sh.
# Not its build_opt.h of the color examples: U8X8_WITH_CLUT is in u8x8.h
sed -n 's|^cp \.\./\.\./\.\./sys/arduino/\([^ ]*\)/\*\.ino \.\./\.\./\.\./\.\./U8g2_Arduino/examples/\([^ ]*\)/\.$|\1 \2|p' \
    "$U8G2/tools/release/arduino/create_release.sh" |
while read -r src dst; do
    mkdir -p "$LIB/examples/$dst"
    cp "$U8G2/sys/arduino/$src"/*.ino "$LIB/examples/$dst/"
done

cp "$U8G2/ChangeLog" "$LIB/extras/"
cp "$U8G2/LICENSE" "$LIB/"
cp "$U8G2/tools/font/build/keywords.txt" "$LIB/"

cat > "$LIB/library.properties" <<EOF
name=$NAME
version=$VERSION
author=oliver <olikraus@gmail.com>, Koen De Vleeschauwer
maintainer=Koen De Vleeschauwer
sentence=U8g2 with color displays: ST7789, ILI9341, GC9A01. Monochrome LCD, OLED and eInk Library.
paragraph=U8g2 stays monochrome; the color drivers add 16 foreground/background color pairs per 8x8 tile. Replaces the official U8g2 library: same headers, all U8g2 displays and fonts. Remove U8g2 before installing.
category=Display
url=https://github.com/koendv/u8g2/tree/clut
architectures=*
includes=U8g2lib.h
license=BSD-2-Clause
EOF

cat > "$LIB/library.json" <<EOF
{
  "name": "$NAME",
  "keywords": "display, lcd, oled, eink, color, st7789, ili9341, gc9a01",
  "description": "U8g2 with color displays: ST7789, ILI9341, GC9A01",
  "homepage": "https://github.com/koendv/u8g2/tree/clut",
  "repository": {
    "type": "git",
    "url": "https://github.com/koendv/u8g2.git",
    "branch": "clut"
  },
  "version": "$VERSION",
  "license": "BSD-2-Clause",
  "frameworks": "arduino",
  "platforms": "*",
  "build": {
    "libLDFMode": "chain+"
  }
}
EOF

cat > "$LIB/CMakeLists.txt" <<EOF
idf_component_register(SRC_DIRS "src" "src/clib"
                    INCLUDE_DIRS "src" "src/clib"
                    REQUIRES "arduino")
EOF

cat > "$LIB/README.md" <<EOF
# U8g2_clut

[U8g2](https://github.com/olikraus/u8g2) with color drivers for ST7789, ILI9341 and GC9A01 displays,
from the [clut branch](https://github.com/koendv/u8g2/tree/clut) of koendv/u8g2.

Replaces the official U8g2 library: same headers (\`U8g2lib.h\`), all U8g2 displays, fonts and examples.
Remove the official U8g2 before installing; with both installed, Arduino may pick either one.

\`U8X8_WITH_CLUT\` is defined in \`src/clib/u8x8.h\`: sketches need no build flags.
Color examples: File > Examples > U8g2_clut > full_buffer > Clut*.

\`MUI_U8G2_HIRES\` is defined in \`src/clib/mui_u8g2.h\`, as in the MicroPython firmware: the 8 bit x and y
of a mui field are multiplied by 2, 4 or 8 on a display of 255 pixels or more, so mui can use the
whole display. On displays smaller than 255 pixels, nothing changes. mui sketches written for the
official U8g2 on displays 255 pixels or more high, or 511 or more wide, need other field positions.

Version $VERSION, from u8g2 commit ${U8G2_COMMIT:-$(git -C "$U8G2" rev-parse HEAD 2>/dev/null || echo unknown)}.
EOF

rm -f "$OUT/$NAME.zip"
(cd "$WORK" && python3 -m zipfile -c "$OUT/$NAME.zip" "$NAME")
echo "make_library.sh: $OUT/$NAME.zip, version $VERSION, $(find "$LIB/examples" -name '*.ino' | wc -l) examples"
