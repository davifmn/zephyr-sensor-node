#include "moving_average.h"

#include <errno.h>

#include "rounding.h"

int ma_init(struct moving_average *ma, uint32_t size)
{
	int err = 0;

	if ((size == 0U) || (size > MA_MAX_WINDOW)) {
		err = -EINVAL;
	} else {
		ma->size = size;
		ma->next = 0U;
		ma->count = 0U;
		ma->sum = 0;
	}
	return err;
}

int32_t ma_add(struct moving_average *ma, int32_t reading)
{
	if (ma->count == ma->size) {
		ma->sum -= ma->window[ma->next]; /* the oldest leaves the window */
	} else {
		ma->count++;
	}
	ma->window[ma->next] = reading;
	ma->sum += reading;
	ma->next = (ma->next + 1U) % ma->size;
	return div_round_nearest(ma->sum, ma->count);
}
