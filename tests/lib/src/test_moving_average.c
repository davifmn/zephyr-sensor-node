#include <errno.h>

#include <zephyr/ztest.h>

#include "moving_average.h"

static struct moving_average ma;

ZTEST_SUITE(moving_average, NULL, NULL, NULL, NULL, NULL);

/* Verifies: REQ-002 */
ZTEST(moving_average, test_rejects_invalid_windows)
{
	zassert_equal(ma_init(&ma, 0U), -EINVAL);
	zassert_equal(ma_init(&ma, MA_MAX_WINDOW + 1U), -EINVAL);
	zassert_equal(ma_init(&ma, MA_MAX_WINDOW), 0);
}

/* Verifies: REQ-002 */
ZTEST(moving_average, test_averages_what_it_has_until_the_window_is_full)
{
	zassert_ok(ma_init(&ma, 4U));
	zassert_equal(ma_add(&ma, 1000), 1000);
	zassert_equal(ma_add(&ma, 2000), 1500);
	zassert_equal(ma_add(&ma, 3000), 2000);
	zassert_equal(ma_add(&ma, 4000), 2500);
}

/* Verifies: REQ-002 */
ZTEST(moving_average, test_oldest_reading_leaves_the_window)
{
	zassert_ok(ma_init(&ma, 4U));
	for (int32_t v = 1; v <= 4; v++) {
		(void)ma_add(&ma, v * 1000);
	}
	zassert_equal(ma_add(&ma, 9000), 4500, "(2000 + 3000 + 4000 + 9000) / 4");
	zassert_equal(ma_add(&ma, 9000), 6250);
}

/* Verifies: REQ-002, REQ-008 */
ZTEST(moving_average, test_rounds_to_nearest_for_negative_values_too)
{
	zassert_ok(ma_init(&ma, 2U));
	(void)ma_add(&ma, 1);
	zassert_equal(ma_add(&ma, 2), 2, "1.5 rounds away from zero");

	zassert_ok(ma_init(&ma, 2U));
	(void)ma_add(&ma, -1);
	zassert_equal(ma_add(&ma, -2), -2, "-1.5 rounds away from zero, not toward it");

	zassert_ok(ma_init(&ma, 3U));
	(void)ma_add(&ma, -1000);
	(void)ma_add(&ma, -1000);
	zassert_equal(ma_add(&ma, -1001), -1000, "-1000.33 rounds to -1000");
}

/* Verifies: REQ-008 */
ZTEST(moving_average, test_full_window_of_extremes_does_not_overflow)
{
	zassert_ok(ma_init(&ma, MA_MAX_WINDOW));
	for (uint32_t i = 0U; i < 3U * MA_MAX_WINDOW; i++) {
		zassert_equal(ma_add(&ma, INT32_MAX), INT32_MAX);
	}
	for (uint32_t i = 0U; i < MA_MAX_WINDOW; i++) {
		(void)ma_add(&ma, INT32_MIN);
	}
	zassert_equal(ma_add(&ma, INT32_MIN), INT32_MIN);
}
