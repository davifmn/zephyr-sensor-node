/*
 * The alarm state machine (REQ-003, REQ-004, REQ-005):
 *
 *   NORMAL --average >= high--> HIGH --average <= clear--> NORMAL
 *   any --fault_after failed reads in a row--> FAULT --good reading--> NORMAL or HIGH
 *
 * The gap between `clear` and `high` is the hysteresis: an average that wanders around
 * the threshold does not make the alarm chatter.
 */
#ifndef ALARM_H
#define ALARM_H

#include <stdint.h>

enum alarm_state {
	ALARM_NORMAL,
	ALARM_HIGH,
	ALARM_FAULT,
};

enum alarm_event {
	ALARM_NO_CHANGE,
	ALARM_RAISED,
	ALARM_CLEARED,
	ALARM_FAULTED,
	ALARM_RECOVERED,
};

struct alarm {
	int32_t high;
	int32_t clear;
	uint32_t fault_after;
	uint32_t failures; /* consecutive failed reads */
	enum alarm_state state;
};

/* 0, or -EINVAL when clear >= high or fault_after is 0. */
int alarm_init(struct alarm *a, int32_t high, int32_t clear, uint32_t fault_after);
/* A good reading, as the current average. */
enum alarm_event alarm_update(struct alarm *a, int32_t average);
/* A failed reading. */
enum alarm_event alarm_read_failed(struct alarm *a);
const char *alarm_state_name(enum alarm_state state);

#endif
