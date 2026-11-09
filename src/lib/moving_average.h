/*
 * Moving average over the last `size` readings (REQ-002), with a running sum: O(1) per
 * reading. The sum is 64-bit, so a full window of extreme int32_t readings cannot
 * overflow (REQ-008).
 */
#ifndef MOVING_AVERAGE_H
#define MOVING_AVERAGE_H

#include <stdint.h>

#define MA_MAX_WINDOW 64U

struct moving_average {
	int32_t window[MA_MAX_WINDOW];
	uint32_t size;  /* window length */
	uint32_t next;  /* slot of the next reading */
	uint32_t count; /* readings in the window, at most size */
	int64_t sum;
};

/* 0, or -EINVAL when size is 0 or above MA_MAX_WINDOW. */
int ma_init(struct moving_average *ma, uint32_t size);
/* Adds a reading and returns the average of the window, rounded to the nearest. */
int32_t ma_add(struct moving_average *ma, int32_t reading);

#endif
