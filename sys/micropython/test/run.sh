#!/bin/sh
# Host test of the u8g2 module: MicroPython unix port, virtual pins, SoftSPI and SoftI2C.
# The bus traffic is decoded and replayed into a model of the display ram.
# usage: test/run.sh <micropython source tree>
set -e
MPY=$(cd "$1" && pwd)
shift
HERE=$(cd "$(dirname "$0")" && pwd)
BUILD=build-u8g2-test
make -C "$MPY/mpy-cross"
make -C "$MPY/ports/unix" -j8 VARIANT_DIR="$HERE/unix_variant" BUILD=$BUILD \
    USER_C_MODULES="$HERE/.." MICROPY_PY_BTREE=0 MICROPY_PY_SSL=0 MICROPY_PY_FFI=0 \
    MICROPY_VFS_LFS1=0 MICROPY_VFS_LFS2=0 "$@"
MICROPYPATH="$HERE" "$MPY/ports/unix/$BUILD/micropython" "$HERE/test_u8g2.py"
MICROPYPATH="$HERE:$HERE/../drivers" "$MPY/ports/unix/$BUILD/micropython" "$HERE/test_touch.py"
