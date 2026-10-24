# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer

# u8g2 and mui modules, cmake based ports (esp32, rp2).
# u8g2 sources: csrc/ of this u8g2 tree.

get_filename_component(U8G2_CSRC_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../csrc ABSOLUTE)

file(GLOB U8G2_CSRC ${U8G2_CSRC_DIR}/*.c)
# fonts: only the subset from tools/fonts.txt
list(REMOVE_ITEM U8G2_CSRC
    ${U8G2_CSRC_DIR}/u8g2_fonts.c
    ${U8G2_CSRC_DIR}/u8x8_fonts.c
)

add_library(usermod_u8g2 INTERFACE)

target_sources(usermod_u8g2 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/modu8g2.c
    ${CMAKE_CURRENT_LIST_DIR}/u8g2_port.c
    ${CMAKE_CURRENT_LIST_DIR}/modmui.c
    ${CMAKE_CURRENT_LIST_DIR}/u8g2_fonts_subset.c
    ${U8G2_CSRC}
)

target_include_directories(usermod_u8g2 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${U8G2_CSRC_DIR}
)

# U8X8_WITH_CLUT and U8X8_USE_PINS change struct u8x8_struct: must be the same for every file
# that includes u8x8.h. U8X8_USE_PINS: drivers ask u8x8->pins[] whether there is a reset pin.
# Defined empty, as u8x8.h itself defines it on some hosts.
# MUI_U8G2_HIRES: mui x and y are scaled by 1, 2, 4 or 8 on displays of 255 pixels or more
target_compile_definitions(usermod_u8g2 INTERFACE
    U8X8_WITH_CLUT
    U8X8_USE_PINS=
    MUI_U8G2_HIRES
)

# third party code: show warnings, do not fail the build on them
set_source_files_properties(${U8G2_CSRC}
    PROPERTIES COMPILE_OPTIONS "-Wno-error"
)

target_link_libraries(usermod INTERFACE usermod_u8g2)
