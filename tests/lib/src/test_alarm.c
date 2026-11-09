#include <errno.h>

#include <zephyr/ztest.h>

#include "alarm.h"

#define HIGH  30000
#define CLEAR 28000

static struct alarm a;

static void reset(void *fixture)
{
	ARG_UNUSED(fixture);
	zassert_ok(alarm_init(&a, HIGH, CLEAR, 3U));
}

ZTEST_SUITE(alarm, NULL, NULL, reset, NULL, NULL);

/* Verifies: REQ-003, REQ-004 */
ZTEST(alarm, test_rejects_inconsistent_thresholds)
{
	struct alarm b;

	zassert_equal(alarm_init(&b, 30000, 30000, 3U), -EINVAL, "no hysteresis");
	zassert_equal(alarm_init(&b, 30000, 31000, 3U), -EINVAL);
	zassert_equal(alarm_init(&b, 30000, 28000, 0U), -EINVAL);
}

/* Verifies: REQ-003 */
ZTEST(alarm, test_raised_when_the_average_reaches_the_threshold)
{
	zassert_equal(alarm_update(&a, HIGH - 1), ALARM_NO_CHANGE);
	zassert_equal(a.state, ALARM_NORMAL);
	zassert_equal(alarm_update(&a, HIGH), ALARM_RAISED, "reaching it is enough");
	zassert_equal(a.state, ALARM_HIGH);
	zassert_equal(alarm_update(&a, HIGH + 5000), ALARM_NO_CHANGE, "raised once");
}

/* Verifies: REQ-004 */
ZTEST(alarm, test_hysteresis_no_chatter_around_the_threshold)
{
	zassert_equal(alarm_update(&a, HIGH), ALARM_RAISED);
	/* Wandering just below the threshold does not clear it... */
	for (int i = 0; i < 10; i++) {
		zassert_equal(alarm_update(&a, (i % 2 == 0) ? HIGH - 1 : HIGH + 1), ALARM_NO_CHANGE);
		zassert_equal(alarm_update(&a, CLEAR + 1), ALARM_NO_CHANGE);
	}
	/* ...only falling to the clear threshold does. */
	zassert_equal(alarm_update(&a, CLEAR), ALARM_CLEARED);
	zassert_equal(a.state, ALARM_NORMAL);
	zassert_equal(alarm_update(&a, HIGH - 1), ALARM_NO_CHANGE, "and it is not raised again below HIGH");
}

/* Verifies: REQ-005 */
ZTEST(alarm, test_fault_after_consecutive_failed_reads_only)
{
	zassert_equal(alarm_read_failed(&a), ALARM_NO_CHANGE);
	zassert_equal(alarm_read_failed(&a), ALARM_NO_CHANGE);
	zassert_equal(alarm_update(&a, 25000), ALARM_NO_CHANGE, "a good reading resets the count");
	zassert_equal(alarm_read_failed(&a), ALARM_NO_CHANGE);
	zassert_equal(alarm_read_failed(&a), ALARM_NO_CHANGE);
	zassert_equal(alarm_read_failed(&a), ALARM_FAULTED, "the third in a row");
	zassert_equal(a.state, ALARM_FAULT);
	zassert_equal(alarm_read_failed(&a), ALARM_NO_CHANGE, "reported once");
}

/* Verifies: REQ-005 */
ZTEST(alarm, test_recovers_into_the_state_the_average_calls_for)
{
	for (int i = 0; i < 3; i++) {
		(void)alarm_read_failed(&a);
	}
	zassert_equal(alarm_update(&a, 25000), ALARM_RECOVERED);
	zassert_equal(a.state, ALARM_NORMAL);

	for (int i = 0; i < 3; i++) {
		(void)alarm_read_failed(&a);
	}
	zassert_equal(alarm_update(&a, HIGH + 1), ALARM_RECOVERED);
	zassert_equal(a.state, ALARM_HIGH);
}

/* Verifies: REQ-005 */
ZTEST(alarm, test_state_names)
{
	zassert_str_equal(alarm_state_name(ALARM_NORMAL), "NORMAL");
	zassert_str_equal(alarm_state_name(ALARM_HIGH), "HIGH");
	zassert_str_equal(alarm_state_name(ALARM_FAULT), "FAULT");
}
