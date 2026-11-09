#include "node.h"

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "format.h"
#include "moving_average.h"

LOG_MODULE_REGISTER(node, LOG_LEVEL_INF);

static K_MUTEX_DEFINE(lock);
static struct moving_average average;
static struct alarm alarm;
static struct node_status status;
static uint32_t last_seq;

int node_init(void)
{
	int err;

	k_mutex_lock(&lock, K_FOREVER);
	(void)memset(&status, 0, sizeof(status));
	last_seq = 0U;
	rb_init(&status.history);
	err = ma_init(&average, CONFIG_APP_AVERAGE_WINDOW);
	if (err == 0) {
		err = alarm_init(&alarm, CONFIG_APP_ALARM_HIGH_MILLI_C, CONFIG_APP_ALARM_CLEAR_MILLI_C,
				 CONFIG_APP_FAULT_AFTER);
	}
	status.state = alarm.state;
	k_mutex_unlock(&lock);
	return err;
}

static void log_event(enum alarm_event ev, enum alarm_state state, int32_t avg)
{
	switch (ev) {
	case ALARM_RAISED:
		LOG_WRN("ALARM ON: average " MILLI_FMT " C reached " MILLI_FMT " C", MILLI_ARGS(avg),
			MILLI_ARGS(CONFIG_APP_ALARM_HIGH_MILLI_C));
		break;
	case ALARM_CLEARED:
		LOG_INF("ALARM OFF: average " MILLI_FMT " C back to " MILLI_FMT " C", MILLI_ARGS(avg),
			MILLI_ARGS(CONFIG_APP_ALARM_CLEAR_MILLI_C));
		break;
	case ALARM_FAULTED:
		LOG_ERR("FAULT: %d failed reads in a row", CONFIG_APP_FAULT_AFTER);
		break;
	case ALARM_RECOVERED:
		LOG_INF("RECOVERED: sensor answers again, state %s", alarm_state_name(state));
		break;
	case ALARM_NO_CHANGE:
	default:
		break;
	}
}

void node_process(const struct reading *r)
{
	enum alarm_event ev;
	enum alarm_state state;
	int32_t avg = 0;
	uint32_t processed, dropped, out_of_order, failed;

	k_mutex_lock(&lock, K_FOREVER);
	if (r->seq != last_seq + 1U) {
		status.out_of_order++;
	}
	last_seq = r->seq;
	status.processed++;
	if (r->status != 0) {
		status.failed_reads++;
		ev = alarm_read_failed(&alarm);
	} else {
		avg = ma_add(&average, r->milli_c);
		rb_push(&status.history, r->milli_c);
		status.last_milli_c = r->milli_c;
		status.average_milli_c = avg;
		ev = alarm_update(&alarm, avg);
	}
	status.state = alarm.state;
	state = status.state;
	processed = status.processed;
	dropped = status.dropped;
	out_of_order = status.out_of_order;
	failed = status.failed_reads;
	k_mutex_unlock(&lock);

	log_event(ev, state, avg);
	if (CONFIG_APP_HEALTH_EVERY > 0 && (processed % (uint32_t)MAX(CONFIG_APP_HEALTH_EVERY, 1)) == 0U) {
		LOG_INF("health: %u readings processed, %u dropped, %u out of order, %u failed reads",
			processed, dropped, out_of_order, failed);
	}
}

void node_count_dropped(void)
{
	k_mutex_lock(&lock, K_FOREVER);
	status.dropped++;
	k_mutex_unlock(&lock);
}

void node_snapshot(struct node_status *out)
{
	k_mutex_lock(&lock, K_FOREVER);
	*out = status;
	k_mutex_unlock(&lock);
}
