/*
 * Shell commands (REQ-013):
 *   sensor read      read the temperature now
 *   sensor fail <n>  make the next n reads fail (emulated sensor only)
 *   stats            state, counters, last value, average and history
 */
#include <errno.h>
#include <stdlib.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/shell/shell.h>

#include "drivers/thermo_i2c_emul.h"
#include "format.h"
#include "node.h"
#include "sensor_source.h"

static int cmd_sensor_read(const struct shell *sh, size_t argc, char **argv)
{
	int32_t milli_c = 0;
	int err;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	err = sensor_source_read(&milli_c);
	if (err != 0) {
		shell_error(sh, "read failed: %d", err);
		return err;
	}
	shell_print(sh, "temperature: " MILLI_FMT " C", MILLI_ARGS(milli_c));
	return 0;
}

static int cmd_sensor_fail(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
#if defined(CONFIG_EMUL)
	char *end = NULL;
	unsigned long n = strtoul(argv[1], &end, 10);

	if (end == argv[1] || *end != '\0' || n == 0UL || n > 100UL) {
		shell_error(sh, "usage: sensor fail <1-100>");
		return -EINVAL;
	}
	thermo_emul_fail_next(EMUL_DT_GET(DT_NODELABEL(temp_sensor)), (uint32_t)n);
	shell_print(sh, "the next %lu reads will fail", n);
	return 0;
#else
	ARG_UNUSED(argv);
	shell_error(sh, "only with the emulated sensor");
	return -ENOTSUP;
#endif
}

static int cmd_stats(const struct shell *sh, size_t argc, char **argv)
{
	static struct node_status s; /* the shell runs one command at a time */
	struct rb_stats h;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	node_snapshot(&s);
	shell_print(sh, "state: %s", alarm_state_name(s.state));
	shell_print(sh, "readings: %u processed, %u dropped, %u out of order, %u failed", s.processed,
		    s.dropped, s.out_of_order, s.failed_reads);
	if (!rb_stats(&s.history, &h)) {
		shell_print(sh, "no readings yet");
		return 0;
	}
	shell_print(sh, "last: " MILLI_FMT " C, average: " MILLI_FMT " C", MILLI_ARGS(s.last_milli_c),
		    MILLI_ARGS(s.average_milli_c));
	shell_print(sh, "last %u: min " MILLI_FMT " C, max " MILLI_FMT " C, mean " MILLI_FMT " C",
		    h.count, MILLI_ARGS(h.min), MILLI_ARGS(h.max), MILLI_ARGS(h.mean));
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sensor_cmds,
	SHELL_CMD(read, NULL, "Read the temperature now.", cmd_sensor_read),
	SHELL_CMD_ARG(fail, NULL, "Make the next <n> reads fail (emulated sensor).",
		      cmd_sensor_fail, 2, 0),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(sensor, &sensor_cmds, "Temperature sensor.", NULL);
SHELL_CMD_REGISTER(stats, NULL, "Processing statistics.", cmd_stats);
