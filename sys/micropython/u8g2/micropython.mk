# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer

# u8g2 and mui modules, make based ports (stm32, ...).
# u8g2 sources: csrc/ of this u8g2 tree.

U8G2_MOD_DIR := $(USERMOD_DIR)
U8G2_CSRC_DIR := $(abspath $(U8G2_MOD_DIR)/../../../csrc)

# display table and font subset of the board, from <board>/displays.txt and <board>/fonts.txt.
# Generated when make reads this file, before the qstr scan. Files only change when their content does.
# U8G2_CONFIG_DIR: directory with displays.txt and fonts.txt, default the board directory.
# U8G2_HOST_CC: compiler for codebuild, which runs on the build host. Not $(CC), the cross compiler.
U8G2_CONFIG_DIR ?= $(BOARD_DIR)
U8G2_HOST_CC ?= cc
U8G2_GEN_DIR := $(BUILD)/u8g2gen
U8G2_GEN := $(shell $(PYTHON) $(U8G2_MOD_DIR)/tools/gen_u8g2.py --cc "$(U8G2_HOST_CC)" --config "$(U8G2_CONFIG_DIR)" --out $(U8G2_GEN_DIR) && echo ok)
ifneq ($(U8G2_GEN),ok)
$(error u8g2: generating display table and fonts failed)
endif

# fonts: only the subset from fonts.txt, in u8g2_fonts_subset.c
U8G2_CSRC := $(filter-out %/u8g2_fonts.c %/u8x8_fonts.c,$(wildcard $(U8G2_CSRC_DIR)/*.c))

SRC_USERMOD_C += \
	$(U8G2_MOD_DIR)/modu8g2.c \
	$(U8G2_MOD_DIR)/u8g2_port.c \
	$(U8G2_MOD_DIR)/modmui.c

# no qstrs in these
SRC_USERMOD_LIB_C += \
	$(U8G2_MOD_DIR)/u8g2_fonts_subset.c \
	$(U8G2_CSRC)

# U8X8_WITH_CLUT and U8X8_USE_PINS change struct u8x8_struct: must be the same for every file.
# U8X8_USE_PINS: drivers ask u8x8->pins[] whether there is a reset pin. Defined empty, as
# u8x8.h itself defines it on some hosts.
# MUI_U8G2_HIRES: mui x and y are scaled by 1, 2, 4 or 8 on displays of 255 pixels or more
CFLAGS_USERMOD += -I$(U8G2_GEN_DIR) -I$(U8G2_MOD_DIR) -I$(U8G2_CSRC_DIR) -DU8X8_WITH_CLUT -DU8X8_USE_PINS= -DMUI_U8G2_HIRES

# third party code: show warnings, do not fail the build on them
$(addprefix $(BUILD)/, $(U8G2_CSRC:.c=.o)): CFLAGS += -Wno-error

# rescan for qstrs when the generated font or display table changes
QSTR_GLOBAL_DEPENDENCIES += $(U8G2_GEN_DIR)/u8g2_fonts_table.h $(U8G2_GEN_DIR)/u8g2_displays_table.h
