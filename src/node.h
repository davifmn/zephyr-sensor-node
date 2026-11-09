/* The node's processing: readings in, averages, alarm events and history out. */
#ifndef NODE_H
#define NODE_H

#include <stdint.h>

#include "alarm.h"
#include "ring_buffer.h"

struct reading {
	uint32_t seq;    /* 1, 2, 3, ... per acquisition */
	int status;      /* 0, or the negative error of the sensor read */
	int32_t milli_c; /* valid when status == 0 */
};

struct node_status {
	uint32_t processed;
	uint32_t dropped;
	uint32_t out_of_order;
	uint32_t failed_reads;
	int32_t last_milli_c;
	int32_t average_milli_c;
	enum alarm_state state;
	struct ring_buffer history;
};

/* (Re)starts from a clean state: counters, average, alarm and history. */
int node_init(void);
void node_process(const struct reading *r);
void node_count_dropped(void);
/* A consistent copy (taken under the node's mutex), for the shell. */
void node_snapshot(struct node_status *out);

#endif
