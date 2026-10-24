#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer
# Build the MicroPython firmware of the boards in sys/micropython/boards.
# Used by the Dockerfile in this directory; also runs outside docker.
#
# usage: build_firmware.sh <micropython dir> <output dir> [board ...]
#   no boards: every board in sys/micropython/boards
#   esp32 boards need the esp-idf environment (export.sh) in the shell.
#
# output, per board, by the mcu in board.json:
#   esp32*: <board>.bin                       flash at address 0
#   stm32*: <board>.dfu, <board>.elf
# and U8g2_clut.zip (Arduino library, tools/release/arduino_clut), SHA256SUMS, release-notes.md
set -e

MPY=$(cd "$1" && pwd)
OUT=$2
shift 2
HERE=$(cd "$(dirname "$0")" && pwd)
U8G2_MPY=$(cd "$HERE/../.." && pwd)
U8G2=$(cd "$U8G2_MPY/../.." && pwd)
BOARDS_DIR=$U8G2_MPY/boards
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)

if [ $# -eq 0 ]; then
    set -- $(cd "$BOARDS_DIR" && ls -d */ | tr -d /)
fi

mcu() {
    python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["mcu"])' "$BOARDS_DIR/$1/board.json"
}

product() {
    python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["product"])' "$BOARDS_DIR/$1/board.json"
}

# before the esp-idf environment, which breaks the host assembler: build mpy-cross first
if [ ! -x "$MPY/mpy-cross/build/mpy-cross" ]; then
    make -C "$MPY/mpy-cross"
fi

files=""
for board in "$@"; do
    dir=$BOARDS_DIR/$board
    if [ ! -f "$dir/board.json" ]; then
        echo "build_firmware.sh: no board $dir" >&2
        exit 1
    fi
    case $(mcu "$board") in
    esp32*)
        make -C "$MPY/ports/esp32" BOARD_DIR="$dir" submodules
        make -C "$MPY/ports/esp32" BOARD_DIR="$dir"
        cp "$MPY/ports/esp32/build-$board/firmware.bin" "$OUT/$board.bin"
        files="$files $board.bin"
        ;;
    stm32*)
        make -C "$MPY/ports/stm32" BOARD_DIR="$dir" submodules
        make -C "$MPY/ports/stm32" -j"$(nproc)" BOARD_DIR="$dir" USER_C_MODULES="$U8G2_MPY"
        for ext in dfu elf; do
            cp "$MPY/ports/stm32/build-$board/firmware.$ext" "$OUT/$board.$ext"
            files="$files $board.$ext"
        done
        ;;
    *)
        echo "build_firmware.sh: $board: no build for mcu $(mcu "$board")" >&2
        exit 1
        ;;
    esac
done

# Arduino library with the color drivers
sh "$U8G2/tools/release/arduino_clut/make_library.sh" "$OUT"

cd "$OUT"
sha256sum $files U8g2_clut.zip > SHA256SUMS

# versions and the list of firmware files, for the github release
{
    echo "| file | board |"
    echo "|---|---|"
    for f in $files; do
        b=${f%.*}
        echo "| \`$f\` | $(product "$b") ($(mcu "$b")) |"
    done
    echo "| \`U8g2_clut.zip\` | Arduino library: U8g2 with the color drivers, replaces the official U8g2 |"
    echo
    echo "- u8g2: ${U8G2_COMMIT:-unknown}"
    echo "- MicroPython: $(git -C "$MPY" describe --tags --always)"
    if [ -n "$IDF_PATH" ]; then
        echo "- ESP-IDF: $(git -C "$IDF_PATH" describe --tags --always)"
    fi
    if command -v arm-none-eabi-gcc >/dev/null; then
        echo "- $(arm-none-eabi-gcc --version | head -1)"
    fi
} > release-notes.md
