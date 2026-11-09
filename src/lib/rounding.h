#ifndef ROUNDING_H
#define ROUNDING_H

#include <stdint.h>

/*
 * sum / n rounded to the nearest integer, halves away from zero, for negative sums too
 * (plain integer division rounds toward zero, which biases averages of negative values).
 * n must not be 0. The result fits in int32_t when sum is a sum of n int32_t values.
 */
static inline int32_t div_round_nearest(int64_t sum, uint32_t n)
{
	int64_t divisor = (int64_t)n;
	int64_t half = divisor / 2;
	int64_t quotient;

	if (sum >= 0) {
		quotient = (sum + half) / divisor;
	} else {
		quotient = (sum - half) / divisor;
	}
	return (int32_t)quotient;
}

#endif
