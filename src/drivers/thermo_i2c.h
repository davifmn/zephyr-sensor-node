/* THERMO-1 register map and conversions (docs/sensor.md). */
#ifndef THERMO_I2C_H
#define THERMO_I2C_H

#include <stdint.h>

#define THERMO_REG_TEMP      0x00U /* int16, big-endian, 1 LSB = 1/128 C = 7.8125 mC */
#define THERMO_REG_DEVICE_ID 0x0FU /* uint16, big-endian */
#define THERMO_DEVICE_ID     0x5431U

/* Register value to milli-degrees Celsius: raw * 1000 / 128 = raw * 125 / 16. */
static inline int32_t thermo_raw_to_milli(int16_t raw)
{
	return ((int32_t)raw * 125) / 16;
}

/* Milli-degrees Celsius to the nearest register value, clamped to the int16 range. */
static inline int16_t thermo_milli_to_raw(int32_t milli)
{
	int64_t scaled = (int64_t)milli * 16;
	int64_t raw = scaled >= 0 ? (scaled + 62) / 125 : (scaled - 62) / 125;

	if (raw > INT16_MAX) {
		raw = INT16_MAX;
	} else if (raw < INT16_MIN) {
		raw = INT16_MIN;
	} else {
		/* in range */
	}
	return (int16_t)raw;
}

#endif
