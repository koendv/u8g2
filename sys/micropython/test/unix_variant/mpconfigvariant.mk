# unix test variant for the u8g2 module, see ../run.sh
FROZEN_MANIFEST =
CFLAGS_EXTRA += -Wno-error=sign-compare -Wno-error=old-style-declaration
SHARED_SRC_C += drivers/bus/softspi.c
