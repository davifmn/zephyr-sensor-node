/*
 * Driver for the THERMO-1 I2C temperature sensor. It implements the standard Zephyr
 * sensor API (sample_fetch, then channel_get), so the application would not change on
 * real hardware. Each fetch is one I2C transaction: the register pointer, then a
 * repeated start and a 2-byte read.
 */
#define DT_DRV_COMPAT vnd_thermo_i2c

#include <errno.h>
#include <stdbool.h>

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include "thermo_i2c.h"

LOG_MODULE_REGISTER(thermo_i2c, CONFIG_SENSOR_LOG_LEVEL);

struct thermo_config {
	struct i2c_dt_spec bus;
};

struct thermo_data {
	int16_t raw;
	bool valid; /* false until a fetch succeeds, and again after a failed one */
};

static int read_reg(const struct i2c_dt_spec *bus, uint8_t reg, uint16_t *value)
{
	uint8_t buf[2];
	int err = i2c_write_read_dt(bus, &reg, sizeof(reg), buf, sizeof(buf));

	if (err == 0) {
		*value = sys_get_be16(buf);
	}
	return err;
}

static int thermo_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct thermo_config *cfg = dev->config;
	struct thermo_data *data = dev->data;
	uint16_t value;
	int err;

	if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_AMBIENT_TEMP) {
		return -ENOTSUP;
	}
	err = read_reg(&cfg->bus, THERMO_REG_TEMP, &value);
	if (err != 0) {
		/* Never hand out the previous value as if it were new. */
		data->valid = false;
		LOG_DBG("temperature read failed: %d", err);
		return err;
	}
	data->raw = (int16_t)value;
	data->valid = true;
	return 0;
}

static int thermo_channel_get(const struct device *dev, enum sensor_channel chan,
			      struct sensor_value *val)
{
	const struct thermo_data *data = dev->data;

	if (chan != SENSOR_CHAN_AMBIENT_TEMP) {
		return -ENOTSUP;
	}
	if (!data->valid) {
		return -ENODATA;
	}
	return sensor_value_from_milli(val, thermo_raw_to_milli(data->raw));
}

static int thermo_init(const struct device *dev)
{
	const struct thermo_config *cfg = dev->config;
	uint16_t id;
	int err;

	if (!i2c_is_ready_dt(&cfg->bus)) {
		LOG_ERR("%s: bus not ready", dev->name);
		return -ENODEV;
	}
	err = read_reg(&cfg->bus, THERMO_REG_DEVICE_ID, &id);
	if (err != 0) {
		LOG_ERR("%s: no answer at 0x%02x: %d", dev->name, cfg->bus.addr, err);
		return err;
	}
	if (id != THERMO_DEVICE_ID) {
		LOG_ERR("%s: device id 0x%04x, expected 0x%04x", dev->name, id, THERMO_DEVICE_ID);
		return -ENODEV;
	}
	return 0;
}

static DEVICE_API(sensor, thermo_api) = {
	.sample_fetch = thermo_sample_fetch,
	.channel_get = thermo_channel_get,
};

#define THERMO_DEFINE(inst)                                                                        \
	static struct thermo_data thermo_data_##inst;                                              \
	static const struct thermo_config thermo_config_##inst = {                                 \
		.bus = I2C_DT_SPEC_INST_GET(inst),                                                  \
	};                                                                                         \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, thermo_init, NULL, &thermo_data_##inst,                 \
				     &thermo_config_##inst, POST_KERNEL,                           \
				     CONFIG_SENSOR_INIT_PRIORITY, &thermo_api);

DT_INST_FOREACH_STATUS_OKAY(THERMO_DEFINE)
