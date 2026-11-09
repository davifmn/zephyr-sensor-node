#include <errno.h>
#include <string.h>

#include <zephyr/drivers/emul.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_dummy.h>
#include <zephyr/sys/util.h>
#include <zephyr/ztest.h>

#include "drivers/thermo_i2c_emul.h"
#include "node.h"
#include "sensor_source.h"

static const struct emul *const emul = EMUL_DT_GET(DT_NODELABEL(temp_sensor));
static const struct shell *sh;

static const char *run(const char *cmd, int expected)
{
	size_t size;
	int err;

	shell_backend_dummy_clear_output(sh);
	err = shell_execute_cmd(sh, cmd);
	zassert_equal(err, expected, "'%s' returned %d", cmd, err);
	return shell_backend_dummy_get_output(sh, &size);
}

#define assert_contains(out, text) zassert_not_null(strstr(out, text), "no '%s' in:\n%s", text, out)

static void *setup(void)
{
	sh = shell_backend_dummy_get_ptr();
	WAIT_FOR(shell_ready(sh), 20000, k_msleep(1));
	zassert_true(shell_ready(sh), "the dummy shell backend is up");
	zassert_ok(sensor_source_init());
	return NULL;
}

static void before(void *fixture)
{
	ARG_UNUSED(fixture);
	zassert_ok(node_init());
	thermo_emul_fail_next(emul, 0U);
	thermo_emul_set_milli(emul, 23125);
}

static void after(void *fixture)
{
	ARG_UNUSED(fixture);
	thermo_emul_use_profile(emul);
}

ZTEST_SUITE(shell_commands, NULL, setup, before, after, NULL);

/* Verifies: REQ-013 */
ZTEST(shell_commands, test_sensor_read)
{
	assert_contains(run("sensor read", 0), "temperature: 23.125 C");

	thermo_emul_set_milli(emul, -1500);
	assert_contains(run("sensor read", 0), "temperature: -1.500 C");
}

/* Verifies: REQ-011, REQ-013 */
ZTEST(shell_commands, test_sensor_read_reports_a_failure)
{
	assert_contains(run("sensor fail 1", 0), "the next 1 reads will fail");
	assert_contains(run("sensor read", -EIO), "read failed: -5");
	assert_contains(run("sensor read", 0), "temperature: 23.125 C");
}

/* Verifies: REQ-013 */
ZTEST(shell_commands, test_sensor_fail_checks_its_argument)
{
	(void)run("sensor fail 0", -EINVAL);
	(void)run("sensor fail 12x", -EINVAL);
	(void)run("sensor fail 101", -EINVAL);
}

/* Verifies: REQ-007, REQ-013 */
ZTEST(shell_commands, test_stats)
{
	const char *out = run("stats", 0);

	assert_contains(out, "state: NORMAL");
	assert_contains(out, "no readings yet");

	struct reading readings[] = {
		{.seq = 1U, .status = 0, .milli_c = 21000},
		{.seq = 2U, .status = -EIO, .milli_c = 0},
		{.seq = 3U, .status = 0, .milli_c = 24000},
		{.seq = 5U, .status = 0, .milli_c = 30000},
	};
	ARRAY_FOR_EACH(readings, i) {
		node_process(&readings[i]);
	}
	node_count_dropped();

	out = run("stats", 0);
	assert_contains(out, "readings: 4 processed, 1 dropped, 1 out of order, 1 failed");
	assert_contains(out, "last: 30.000 C, average: 25.000 C");
	assert_contains(out, "last 3: min 21.000 C, max 30.000 C, mean 25.000 C");
}
