# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer

# u8g2 and mui modules, cmake based ports (esp32, rp2).
# u8g2 sources: csrc/ of this u8g2 tree.

get_filename_component(U8G2_CSRC_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../csrc ABSOLUTE)

# display table and font subset of the board, from <board>/displays.txt and <board>/fonts.txt.
# Generated now, before the qstr scan; edits of the lists rerun cmake.
# U8G2_CONFIG_DIR: directory with displays.txt and fonts.txt, default the board directory.
# U8G2_HOST_CC: compiler for codebuild, which runs on the build host.
if(NOT U8G2_CONFIG_DIR)
    set(U8G2_CONFIG_DIR ${MICROPY_BOARD_DIR})
endif()
find_program(U8G2_HOST_CC NAMES cc gcc clang)
if(NOT U8G2_HOST_CC)
    message(FATAL_ERROR "u8g2: no host c compiler found, set U8G2_HOST_CC")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(U8G2_GEN_DIR ${CMAKE_BINARY_DIR}/u8g2gen)
set(U8G2_GEN_PY ${CMAKE_CURRENT_LIST_DIR}/tools/gen_u8g2.py)
execute_process(
    COMMAND ${Python3_EXECUTABLE} ${U8G2_GEN_PY} --cc ${U8G2_HOST_CC} --config ${U8G2_CONFIG_DIR} --out ${U8G2_GEN_DIR}
    RESULT_VARIABLE U8G2_GEN_RESULT
)
if(NOT U8G2_GEN_RESULT EQUAL 0)
    message(FATAL_ERROR "u8g2: generating display table and fonts failed")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${U8G2_CONFIG_DIR}/displays.txt
    ${U8G2_CONFIG_DIR}/fonts.txt
    ${U8G2_GEN_PY}
    ${U8G2_CSRC_DIR}/../tools/codebuild/codebuild.c
)

file(GLOB U8G2_CSRC ${U8G2_CSRC_DIR}/*.c)
# fonts: only the subset from fonts.txt, in u8g2_fonts_subset.c
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
    # not compiled: as sources they are dependencies of the qstr scan, which reruns when they change
    ${U8G2_GEN_DIR}/u8g2_fonts_table.h
    ${U8G2_GEN_DIR}/u8g2_displays_table.h
)

target_include_directories(usermod_u8g2 INTERFACE
    ${U8G2_GEN_DIR}
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
