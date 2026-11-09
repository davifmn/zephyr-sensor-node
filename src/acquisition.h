#ifndef ACQUISITION_H
#define ACQUISITION_H

#include <stdint.h>

#include <zephyr/kernel.h>

/*
 * One acquisition: read the sensor and queue the reading (or the error) with its sequence
 * number. Never blocks: when the queue is full the reading is dropped and counted
 * (REQ-001, REQ-006). Returns 0, or -ENOMSG when the reading was dropped.
 */
int acquisition_step(struct k_msgq *queue, uint32_t seq);

#endif
