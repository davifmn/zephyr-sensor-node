#include "alarm.h"

#include <errno.h>

int alarm_init(struct alarm *a, int32_t high, int32_t clear, uint32_t fault_after)
{
	int err = 0;

	if ((clear >= high) || (fault_after == 0U)) {
		err = -EINVAL;
	} else {
		a->high = high;
		a->clear = clear;
		a->fault_after = fault_after;
		a->failures = 0U;
		a->state = ALARM_NORMAL;
	}
	return err;
}

enum alarm_event alarm_update(struct alarm *a, int32_t average)
{
	enum alarm_event ev = ALARM_NO_CHANGE;

	a->failures = 0U;
	switch (a->state) {
	case ALARM_FAULT:
		a->state = (average >= a->high) ? ALARM_HIGH : ALARM_NORMAL;
		ev = ALARM_RECOVERED;
		break;
	case ALARM_NORMAL:
		if (average >= a->high) {
			a->state = ALARM_HIGH;
			ev = ALARM_RAISED;
		}
		break;
	case ALARM_HIGH:
		if (average <= a->clear) {
			a->state = ALARM_NORMAL;
			ev = ALARM_CLEARED;
		}
		break;
	default:
		/* Not reachable: every state is handled above. */
		break;
	}
	return ev;
}

enum alarm_event alarm_read_failed(struct alarm *a)
{
	enum alarm_event ev = ALARM_NO_CHANGE;

	if (a->state != ALARM_FAULT) {
		a->failures++;
		if (a->failures >= a->fault_after) {
			a->state = ALARM_FAULT;
			ev = ALARM_FAULTED;
		}
	}
	return ev;
}

const char *alarm_state_name(enum alarm_state state)
{
	const char *name;

	switch (state) {
	case ALARM_NORMAL:
		name = "NORMAL";
		break;
	case ALARM_HIGH:
		name = "HIGH";
		break;
	case ALARM_FAULT:
		name = "FAULT";
		break;
	default:
		name = "?";
		break;
	}
	return name;
}
