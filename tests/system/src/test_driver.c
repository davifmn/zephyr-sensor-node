#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/ztest.h>

#include "drivers/thermo_i2c.h"
#include "drivers/thermo_i2c_emul.h"

#define SENSOR DT_NODELABEL(temp_sensor)

static const struct device *const dev = DEVICE_DT_GET(SENSOR);
static const struct emul *const emul = EMUL_DT_GET(SENSOR);

static int32_t read_milli(int *err)
{
	struct sensor_value val = {0};

	*err = sensor_sample_fetch(dev);
	if (*err == 0) {
		*err = sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &val);
	}
	return (int32_t)sensor_value_to_milli(&val);
}

static void *setup(void)
{
	zassert_true(device_is_ready(dev), "the sensor initialized (DEVICE_ID matched)");
	return NULL;
}

static void after(void *fixture)
{
	ARG_UNUSED(fixture);
	thermo_emul_fail_next(emul, 0U);
	thermo_emul_use_profile(emul);
}

ZTEST_SUITE(thermo_driver, NULL, setup, NULL, after, NULL);

/* Verifies: REQ-009 */
ZTEST(thermo_driver, test_reads_and_converts_the_temperature)
{
	static const int32_t cases[] = {25000, 21500, 0, -40000, -1500, 125000};
	int err;

	ARRAY_FOR_EACH(cases, i) {
		thermo_emul_set_milli(emul, cases[i]);
		zassert_equal(read_milli(&err), cases[i]);
		zassert_ok(err);
	}
}

/* Verifies: REQ-009 */
ZTEST(thermo_driver, test_resolution_is_one_lsb)
{
	int err;

	/* 7.8125 m°C per LSB: anything is read back within one LSB. */
	for (int32_t m = 30000; m < 30100; m += 3) {
		thermo_emul_set_milli(emul, m);
		int32_t got = read_milli(&err);

		zassert_ok(err);
		zassert_within(got, m, 8, "%d read as %d", m, got);
	}
}

/* Verifies: REQ-009 */
ZTEST(thermo_driver, test_conversion_covers_the_register_range)
{
	zassert_equal(thermo_raw_to_milli(INT16_MAX), 255992);
	zassert_equal(thermo_raw_to_milli(INT16_MIN), -256000);
	zassert_equal(thermo_raw_to_milli(0x0C80), 25000);
	zassert_equal(thermo_milli_to_raw(1000000), INT16_MAX, "clamped");
	zassert_equal(thermo_milli_to_raw(-1000000), INT16_MIN, "clamped");
}

/* Verifies: REQ-009 */
ZTEST(thermo_driver, test_one_register_read_per_fetch)
{
	uint32_t before = thermo_emul_temp_reads(emul);

	zassert_ok(sensor_sample_fetch(dev));
	zassert_equal(thermo_emul_temp_reads(emul), before + 1U);
}

/* Verifies: REQ-009 */
ZTEST(thermo_driver, test_other_channels_are_refused)
{
	struct sensor_value val;

	zassert_equal(sensor_sample_fetch_chan(dev, SENSOR_CHAN_HUMIDITY), -ENOTSUP);
	zassert_ok(sensor_sample_fetch(dev));
	zassert_equal(sensor_channel_get(dev, SENSOR_CHAN_HUMIDITY, &val), -ENOTSUP);
}

/* Verifies: REQ-011 */
ZTEST(thermo_driver, test_bus_error_is_reported_and_the_old_value_not_reused)
{
	struct sensor_value val;
	int err;

	thermo_emul_set_milli(emul, 22000);
	zassert_equal(read_milli(&err), 22000);

	thermo_emul_fail_next(emul, 1U);
	zassert_equal(sensor_sample_fetch(dev), -EIO);
	zassert_equal(sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &val), -ENODATA,
		      "the 22.000 C from before is not handed out again");

	zassert_equal(read_milli(&err), 22000, "the next read works");
	zassert_ok(err);
}

/* Verifies: REQ-010 */
ZTEST(thermo_driver, test_a_chip_with_the_wrong_id_is_refused)
{
	const struct device *wrong = DEVICE_DT_GET(DT_NODELABEL(wrong_sensor));

	zassert_false(device_is_ready(wrong));
}
