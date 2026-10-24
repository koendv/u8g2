// SPDX-License-Identifier: BSD-2-Clause
// unix test variant for the u8g2 micropython module: virtual pins, SoftSPI, SoftI2C
#define MICROPY_CONFIG_ROM_LEVEL (MICROPY_CONFIG_ROM_LEVEL_EXTRA_FEATURES)
#include "ports/unix/variants/mpconfigvariant_common.h"
#define MICROPY_PY_MACHINE_PIN_BASE (1)
#define MICROPY_PY_MACHINE_SOFTI2C (1)
#define MICROPY_PY_MACHINE_SOFTSPI (1)
int mp_virtual_pin_read(void *pin);
void mp_virtual_pin_write(void *pin, int value);
#define mp_hal_pin_obj_t void *
#define mp_hal_get_pin_obj(o) (o)
#define mp_hal_pin_name(p) (MP_QSTR_)
#define mp_hal_pin_read(p) mp_virtual_pin_read(p)
#define mp_hal_pin_write(p, v) mp_virtual_pin_write((p), (v))
#define mp_hal_pin_output(p) ((void)(p))
#define mp_hal_pin_input(p) ((void)(p))
#define mp_hal_pin_open_drain(p) ((void)(p))
#define mp_hal_pin_od_low(p) mp_virtual_pin_write((p), 0)
#define mp_hal_pin_od_high(p) mp_virtual_pin_write((p), 1)
#define mp_hal_delay_us_fast(us) ((void)(us))
#define MP_HAL_PIN_FMT "%q"
