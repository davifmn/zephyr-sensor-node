#include <zephyr/ztest.h>

#include "ring_buffer.h"

static struct ring_buffer rb;

static void reset(void *fixture)
{
	ARG_UNUSED(fixture);
	rb_init(&rb);
}

ZTEST_SUITE(ring_buffer, NULL, NULL, reset, NULL, NULL);

/* Verifies: REQ-007 */
ZTEST(ring_buffer, test_empty_has_no_stats)
{
	struct rb_stats s;

	zassert_equal(rb_count(&rb), 0U);
	zassert_false(rb_stats(&rb, &s));
}

/* Verifies: REQ-007 */
ZTEST(ring_buffer, test_keeps_insertion_order)
{
	for (int32_t v = 1; v <= 5; v++) {
		rb_push(&rb, v * 10);
	}
	zassert_equal(rb_count(&rb), 5U);
	for (uint32_t i = 0U; i < 5U; i++) {
		zassert_equal(rb_get(&rb, i), (int32_t)(i + 1U) * 10);
	}
}

/* Verifies: REQ-007 */
ZTEST(ring_buffer, test_full_buffer_overwrites_the_oldest)
{
	for (int32_t v = 0; v < (int32_t)RB_CAPACITY + 10; v++) {
		rb_push(&rb, v);
	}
	zassert_equal(rb_count(&rb), RB_CAPACITY);
	zassert_equal(rb_get(&rb, 0U), 10, "the 10 oldest are gone");
	zassert_equal(rb_get(&rb, RB_CAPACITY - 1U), (int32_t)RB_CAPACITY + 9);
}

/* Verifies: REQ-007, REQ-008 */
ZTEST(ring_buffer, test_stats)
{
	struct rb_stats s;

	rb_push(&rb, -1500);
	rb_push(&rb, 2000);
	rb_push(&rb, 31250);
	zassert_true(rb_stats(&rb, &s));
	zassert_equal(s.count, 3U);
	zassert_equal(s.min, -1500);
	zassert_equal(s.max, 31250);
	zassert_equal(s.mean, 10583, "31750 / 3 = 10583.33");
}

/* Verifies: REQ-008 */
ZTEST(ring_buffer, test_mean_of_extremes_does_not_overflow)
{
	struct rb_stats s;

	for (uint32_t i = 0U; i < RB_CAPACITY; i++) {
		rb_push(&rb, INT32_MAX);
	}
	zassert_true(rb_stats(&rb, &s));
	zassert_equal(s.mean, INT32_MAX);

	rb_init(&rb);
	for (uint32_t i = 0U; i < RB_CAPACITY; i++) {
		rb_push(&rb, INT32_MIN);
	}
	zassert_true(rb_stats(&rb, &s));
	zassert_equal(s.mean, INT32_MIN);
}
