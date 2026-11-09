#include "ring_buffer.h"

#include "rounding.h"

void rb_init(struct ring_buffer *rb)
{
	rb->head = 0U;
	rb->count = 0U;
}

void rb_push(struct ring_buffer *rb, int32_t value)
{
	rb->items[rb->head] = value;
	rb->head = (rb->head + 1U) % RB_CAPACITY;
	if (rb->count < RB_CAPACITY) {
		rb->count++;
	}
}

uint32_t rb_count(const struct ring_buffer *rb)
{
	return rb->count;
}

int32_t rb_get(const struct ring_buffer *rb, uint32_t i)
{
	uint32_t oldest = ((rb->head + RB_CAPACITY) - rb->count) % RB_CAPACITY;

	return rb->items[(oldest + i) % RB_CAPACITY];
}

bool rb_stats(const struct ring_buffer *rb, struct rb_stats *out)
{
	bool has_items = false;

	if (rb->count > 0U) {
		int64_t sum = 0;
		int32_t min = rb_get(rb, 0U);
		int32_t max = min;

		for (uint32_t i = 0U; i < rb->count; i++) {
			int32_t v = rb_get(rb, i);

			sum += v;
			min = (v < min) ? v : min;
			max = (v > max) ? v : max;
		}
		out->count = rb->count;
		out->min = min;
		out->max = max;
		out->mean = div_round_nearest(sum, rb->count);
		has_items = true;
	}
	return has_items;
}
