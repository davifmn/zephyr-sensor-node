#include <errno.h>

#include <zephyr/drivers/emul.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include "acquisition.h"
#include "drivers/thermo_i2c_emul.h"
#include "node.h"
#include "sensor_source.h"

static const struct emul *const emul = EMUL_DT_GET(DT_NODELABEL(temp_sensor));

K_MSGQ_DEFINE(test_queue, sizeof(struct reading), 2, 4);

static struct node_status s;

static void process(uint32_t seq, int status, int32_t milli_c)
{
	struct reading r = {.seq = seq, .status = status, .milli_c = milli_c};

	node_process(&r);
}

static void *setup(void)
{
	zassert_ok(sensor_source_init());
	return NULL;
}

static void before(void *fixture)
{
	ARG_UNUSED(fixture);
	zassert_ok(node_init());
	k_msgq_purge(&test_queue);
	thermo_emul_fail_next(emul, 0U);
	thermo_emul_use_profile(emul);
}

ZTEST_SUITE(node, NULL, setup, before, NULL, NULL);

/* Verifies: REQ-001 */
ZTEST(node, test_acquisition_queues_the_reading_with_its_sequence)
{
	struct reading r;

	thermo_emul_set_milli(emul, 23500);
	zassert_ok(acquisition_step(&test_queue, 1U));
	zassert_ok(k_msgq_get(&test_queue, &r, K_NO_WAIT));
	zassert_equal(r.seq, 1U);
	zassert_ok(r.status);
	zassert_equal(r.milli_c, 23500);
}

/* Verifies: REQ-001, REQ-011 */
ZTEST(node, test_a_failed_read_is_queued_as_an_error)
{
	struct reading r;

	thermo_emul_fail_next(emul, 1U);
	zassert_ok(acquisition_step(&test_queue, 7U));
	zassert_ok(k_msgq_get(&test_queue, &r, K_NO_WAIT));
	zassert_equal(r.seq, 7U);
	zassert_equal(r.status, -EIO);
}

/* Verifies: REQ-006 */
ZTEST(node, test_full_queue_drops_and_counts_without_blocking)
{
	struct reading r;
	int64_t start = k_uptime_get();

	zassert_ok(acquisition_step(&test_queue, 1U));
	zassert_ok(acquisition_step(&test_queue, 2U));
	zassert_equal(acquisition_step(&test_queue, 3U), -ENOMSG);
	zassert_true(k_uptime_get() - start < 50, "did not wait for room in the queue");

	node_snapshot(&s);
	zassert_equal(s.dropped, 1U);
	zassert_ok(k_msgq_get(&test_queue, &r, K_NO_WAIT));
	zassert_equal(r.seq, 1U, "the queued readings are the oldest, in order");
	zassert_ok(k_msgq_get(&test_queue, &r, K_NO_WAIT));
	zassert_equal(r.seq, 2U);
}

/* Verifies: REQ-001 */
ZTEST(node, test_gaps_in_the_sequence_are_counted)
{
	process(1U, 0, 20000);
	process(2U, 0, 20000);
	process(4U, 0, 20000);
	process(5U, 0, 20000);
	node_snapshot(&s);
	zassert_equal(s.processed, 4U);
	zassert_equal(s.out_of_order, 1U);
}

/* Verifies: REQ-002, REQ-003, REQ-004 */
ZTEST(node, test_average_drives_the_alarm)
{
	uint32_t seq = 0U;

	/* A single hot reading does not raise the alarm: the average of 8 does. */
	for (int i = 0; i < 7; i++) {
		process(++seq, 0, 25000);
	}
	process(++seq, 0, 60000);
	node_snapshot(&s);
	zassert_equal(s.average_milli_c, 29375, "(7 * 25000 + 60000) / 8");
	zassert_equal(s.state, ALARM_NORMAL);

	process(++seq, 0, 60000);
	node_snapshot(&s);
	zassert_equal(s.state, ALARM_HIGH, "(6 * 25000 + 2 * 60000) / 8 = 33750");

	for (int i = 0; i < 8; i++) {
		process(++seq, 0, 20000);
	}
	node_snapshot(&s);
	zassert_equal(s.state, ALARM_NORMAL);
	zassert_equal(s.last_milli_c, 20000);
}

/* Verifies: REQ-005 */
ZTEST(node, test_failed_reads_fault_and_a_good_one_recovers)
{
	process(1U, 0, 22000);
	process(2U, -EIO, 0);
	process(3U, -EIO, 0);
	node_snapshot(&s);
	zassert_equal(s.state, ALARM_NORMAL, "two failures are not a fault yet");
	process(4U, -EIO, 0);
	node_snapshot(&s);
	zassert_equal(s.state, ALARM_FAULT);
	zassert_equal(s.failed_reads, 3U);
	zassert_equal(s.last_milli_c, 22000, "a failed read never replaces the last value");

	process(5U, 0, 22500);
	node_snapshot(&s);
	zassert_equal(s.state, ALARM_NORMAL);
}

/* Verifies: REQ-007 */
ZTEST(node, test_history_keeps_good_readings_only)
{
	process(1U, 0, 21000);
	process(2U, -EIO, 0);
	process(3U, 0, 23000);
	node_snapshot(&s);
	zassert_equal(rb_count(&s.history), 2U);
	zassert_equal(rb_get(&s.history, 0U), 21000);
	zassert_equal(rb_get(&s.history, 1U), 23000);
}
