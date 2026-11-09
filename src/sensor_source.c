#include "sensor_source.h"

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

static const struct device *const sensor = DEVICE_DT_GET(DT_NODELABEL(temp_sensor));

/* A fetch and the following channel_get must not interleave with another reader's. */
static K_MUTEX_DEFINE(sensor_lock);

int sensor_source_init(void)
{
	return device_is_ready(sensor) ? 0 : -ENODEV;
}

int sensor_source_read(int32_t *milli_c)
{
	struct sensor_value val;
	int err;

	k_mutex_lock(&sensor_lock, K_FOREVER);
	err = sensor_sample_fetch_chan(sensor, SENSOR_CHAN_AMBIENT_TEMP);
	if (err == 0) {
		err = sensor_channel_get(sensor, SENSOR_CHAN_AMBIENT_TEMP, &val);
	}
	k_mutex_unlock(&sensor_lock);

	if (err == 0) {
		*milli_c = (int32_t)sensor_value_to_milli(&val);
	}
	return err;
}
