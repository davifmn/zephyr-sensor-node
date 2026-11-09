/*
 * Emulator of the THERMO-1 on Zephyr's emulated I2C bus. The I2C controller emulator
 * hands it the driver's messages; it checks their shape (a 1-byte register write, then a
 * 2-byte read) and answers from its registers, as the chip would. Anything else is
 * refused like a NACK, so a driver bug shows up as an I/O error in the tests.
 */
#define DT_DRV_COMPAT vnd_thermo_i2c

#include <errno.h>
#include <stdbool.h>

#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>
#include <zephyr/sys/byteorder.h>

#include "thermo_i2c.h"
#include "thermo_i2c_emul.h"

struct thermo_emul_config {
	int32_t baseline;
	int32_t amplitude;
	uint32_t period;
	uint32_t fail_at;
	uint16_t device_id;
};

struct thermo_emul_data {
	uint32_t temp_reads;
	uint32_t fail_next;
	bool fixed;
	int32_t fixed_milli;
};

/* baseline → baseline + amplitude → baseline over one period. */
static int32_t profile(const struct thermo_emul_config *cfg, uint32_t n)
{
	uint32_t half = cfg->period / 2U;
	uint32_t phase = n % cfg->period;
	uint32_t up = phase < half ? phase : cfg->period - phase;

	return cfg->baseline + (int32_t)(((int64_t)cfg->amplitude * up) / half);
}

static bool fails(const struct thermo_emul_config *cfg, struct thermo_emul_data *data)
{
	if (data->fail_next > 0U) {
		data->fail_next--;
		return true;
	}
	return cfg->fail_at != 0U && data->temp_reads >= cfg->fail_at &&
	       data->temp_reads < cfg->fail_at + 3U;
}

static int thermo_emul_transfer(const struct emul *target, struct i2c_msg *msgs, int num_msgs,
				int addr)
{
	const struct thermo_emul_config *cfg = target->cfg;
	struct thermo_emul_data *data = target->data;
	uint16_t value;

	ARG_UNUSED(addr);
	if (num_msgs != 2 || (msgs[0].flags & I2C_MSG_READ) != 0U || msgs[0].len != 1U ||
	    (msgs[1].flags & I2C_MSG_READ) == 0U || msgs[1].len != 2U) {
		return -EIO;
	}
	switch (msgs[0].buf[0]) {
	case THERMO_REG_DEVICE_ID:
		value = cfg->device_id;
		break;
	case THERMO_REG_TEMP:
		data->temp_reads++;
		if (fails(cfg, data)) {
			return -EIO;
		}
		value = (uint16_t)thermo_milli_to_raw(data->fixed ? data->fixed_milli
								 : profile(cfg, data->temp_reads));
		break;
	default:
		return -EIO;
	}
	sys_put_be16(value, msgs[1].buf);
	return 0;
}

void thermo_emul_set_milli(const struct emul *target, int32_t milli_c)
{
	struct thermo_emul_data *data = target->data;

	data->fixed = true;
	data->fixed_milli = milli_c;
}

void thermo_emul_use_profile(const struct emul *target)
{
	struct thermo_emul_data *data = target->data;

	data->fixed = false;
}

void thermo_emul_fail_next(const struct emul *target, uint32_t reads)
{
	struct thermo_emul_data *data = target->data;

	data->fail_next = reads;
}

uint32_t thermo_emul_temp_reads(const struct emul *target)
{
	const struct thermo_emul_data *data = target->data;

	return data->temp_reads;
}

static int thermo_emul_init(const struct emul *target, const struct device *parent)
{
	ARG_UNUSED(target);
	ARG_UNUSED(parent);
	return 0;
}

static const struct i2c_emul_api thermo_emul_api = {
	.transfer = thermo_emul_transfer,
};

#define THERMO_EMUL_DEFINE(inst)                                                                   \
	BUILD_ASSERT(DT_INST_PROP(inst, sim_period_reads) >= 2 &&                                   \
			     DT_INST_PROP(inst, sim_period_reads) % 2 == 0,                            \
		     "sim-period-reads must be even and at least 2");                                   \
	static struct thermo_emul_data thermo_emul_data_##inst;                                    \
	static const struct thermo_emul_config thermo_emul_config_##inst = {                       \
		.baseline = DT_INST_PROP(inst, sim_baseline_milli_celsius),                         \
		.amplitude = DT_INST_PROP(inst, sim_amplitude_milli_celsius),                       \
		.period = DT_INST_PROP(inst, sim_period_reads),                                     \
		.fail_at = DT_INST_PROP(inst, sim_fail_at_read),                                    \
		.device_id = DT_INST_PROP(inst, sim_device_id),                                     \
	};                                                                                         \
	EMUL_DT_INST_DEFINE(inst, thermo_emul_init, &thermo_emul_data_##inst,                      \
			    &thermo_emul_config_##inst, &thermo_emul_api, NULL);

DT_INST_FOREACH_STATUS_OKAY(THERMO_EMUL_DEFINE)
