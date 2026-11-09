/*
 * Fixed-size history of the last RB_CAPACITY readings (REQ-007). When full, a push
 * overwrites the oldest. No allocation, no Zephyr dependency: unit-testable anywhere.
 */
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

#define RB_CAPACITY 64U

struct ring_buffer {
	int32_t items[RB_CAPACITY];
	uint32_t head;  /* where the next push goes */
	uint32_t count; /* valid items, at most RB_CAPACITY */
};

struct rb_stats {
	uint32_t count;
	int32_t min;
	int32_t max;
	int32_t mean; /* rounded to the nearest integer */
};

void rb_init(struct ring_buffer *rb);
void rb_push(struct ring_buffer *rb, int32_t value);
uint32_t rb_count(const struct ring_buffer *rb);
/* Item i, 0 being the oldest; i must be below rb_count(). */
int32_t rb_get(const struct ring_buffer *rb, uint32_t i);
/* False when empty. */
bool rb_stats(const struct ring_buffer *rb, struct rb_stats *out);

#endif
