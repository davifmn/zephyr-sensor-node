/* Test and demo controls of the THERMO-1 emulator. */
#ifndef THERMO_I2C_EMUL_H
#define THERMO_I2C_EMUL_H

#include <stdint.h>

#include <zephyr/drivers/emul.h>

/* From now on every read returns this temperature (quantized like the chip). */
void thermo_emul_set_milli(const struct emul *target, int32_t milli_c);
/* Back to the triangle profile configured in the devicetree. */
void thermo_emul_use_profile(const struct emul *target);
/* The next `reads` temperature reads fail (no acknowledge). */
void thermo_emul_fail_next(const struct emul *target, uint32_t reads);
/* Temperature register reads so far, failed ones included. */
uint32_t thermo_emul_temp_reads(const struct emul *target);

#endif
