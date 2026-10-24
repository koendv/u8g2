# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Koen De Vleeschauwer

# u8g2 and mui modules, make based ports (stm32, ...).
# u8g2 sources: csrc/ of this u8g2 tree.

U8G2_MOD_DIR := $(USERMOD_DIR)
U8G2_CSRC_DIR := $(abspath $(U8G2_MOD_DIR)/../../../csrc)

# fonts: only the subset from tools/fonts.txt
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
CFLAGS_USERMOD += -I$(U8G2_MOD_DIR) -I$(U8G2_CSRC_DIR) -DU8X8_WITH_CLUT -DU8X8_USE_PINS= -DMUI_U8G2_HIRES

# third party code: show warnings, do not fail the build on them
$(addprefix $(BUILD)/, $(U8G2_CSRC:.c=.o)): CFLAGS += -Wno-error

# rescan for qstrs when the generated font or display table changes
QSTR_GLOBAL_DEPENDENCIES += $(U8G2_MOD_DIR)/u8g2_fonts_table.h $(U8G2_MOD_DIR)/u8g2_displays_table.h
