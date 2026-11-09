/*
 * Sensor node: an acquisition thread reads the temperature every
 * CONFIG_APP_SAMPLE_PERIOD_MS and queues it; a processing thread averages the readings,
 * runs the alarm state machine and keeps the history. Each thread has its own watchdog
 * channel, fed only from its own loop, so a stuck thread is named and the node reboots.
 */
#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/task_wdt/task_wdt.h>

#include "acquisition.h"
#include "format.h"
#include "node.h"
#include "sensor_source.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Acquisition runs above processing: sampling stays on time even when processing lags. */
#define ACQUISITION_PRIORITY K_PRIO_PREEMPT(5)
#define PROCESSING_PRIORITY  K_PRIO_PREEMPT(7)
#define STACK_SIZE           1024

K_MSGQ_DEFINE(readings, sizeof(struct reading), CONFIG_APP_QUEUE_DEPTH, 4);

static K_THREAD_STACK_DEFINE(acquisition_stack, STACK_SIZE);
static K_THREAD_STACK_DEFINE(processing_stack, STACK_SIZE);
static struct k_thread acquisition_thread;
static struct k_thread processing_thread;

static char acquisition_name[] = "acquisition";
static char processing_name[] = "processing";
static int acquisition_wdt = -1;
static int processing_wdt = -1;

/* Runs in the system timer's interrupt: print directly, then reset. */
static void watchdog_expired(int channel_id, void *user_data)
{
	ARG_UNUSED(channel_id);
	printk("WATCHDOG: %s thread stalled, rebooting\n", (const char *)user_data);
	LOG_PANIC();
	sys_reboot(SYS_REBOOT_COLD);
}

static void acquisition(void *p1, void *p2, void *p3)
{
	int64_t next = k_uptime_get();
	uint32_t seq = 0U;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	for (;;) {
		seq++;
		(void)acquisition_step(&readings, seq);
		(void)task_wdt_feed(acquisition_wdt);
		/* Absolute deadlines: the period does not drift by the time spent reading. */
		next += CONFIG_APP_SAMPLE_PERIOD_MS;
		k_sleep(K_TIMEOUT_ABS_MS(next));
	}
}

static void processing(void *p1, void *p2, void *p3)
{
	struct reading r;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	for (;;) {
		/* Wake up at least 4 times per watchdog period, readings or not. */
		if (k_msgq_get(&readings, &r, K_MSEC(CONFIG_APP_WATCHDOG_TIMEOUT_MS / 4)) == 0) {
			node_process(&r);
			if (CONFIG_APP_TEST_STALL_AFTER > 0 &&
			    r.seq >= (uint32_t)CONFIG_APP_TEST_STALL_AFTER) {
				LOG_WRN("test: processing thread stops here");
				k_sleep(K_FOREVER);
			}
		}
		(void)task_wdt_feed(processing_wdt);
	}
}

int main(void)
{
	int err = sensor_source_init();

	if (err != 0) {
		LOG_ERR("temperature sensor not ready: %d", err);
		return err;
	}
	err = node_init();
	if (err != 0) {
		LOG_ERR("invalid configuration: %d", err);
		return err;
	}
	err = task_wdt_init(NULL);
	if (err != 0) {
		LOG_ERR("task watchdog: %d", err);
		return err;
	}
	acquisition_wdt = task_wdt_add(CONFIG_APP_WATCHDOG_TIMEOUT_MS, watchdog_expired,
				       acquisition_name);
	processing_wdt = task_wdt_add(CONFIG_APP_WATCHDOG_TIMEOUT_MS, watchdog_expired,
				      processing_name);
	if (acquisition_wdt < 0 || processing_wdt < 0) {
		LOG_ERR("task watchdog channels: %d, %d", acquisition_wdt, processing_wdt);
		return -ENOMEM;
	}

	(void)k_thread_create(&processing_thread, processing_stack,
			      K_THREAD_STACK_SIZEOF(processing_stack), processing, NULL, NULL, NULL,
			      PROCESSING_PRIORITY, 0, K_NO_WAIT);
	(void)k_thread_name_set(&processing_thread, processing_name);
	(void)k_thread_create(&acquisition_thread, acquisition_stack,
			      K_THREAD_STACK_SIZEOF(acquisition_stack), acquisition, NULL, NULL,
			      NULL, ACQUISITION_PRIORITY, 0, K_NO_WAIT);
	(void)k_thread_name_set(&acquisition_thread, acquisition_name);

	LOG_INF("sensor node started: a reading every %d ms, alarm at " MILLI_FMT " C, clear at "
		MILLI_FMT " C", CONFIG_APP_SAMPLE_PERIOD_MS, MILLI_ARGS(CONFIG_APP_ALARM_HIGH_MILLI_C),
		MILLI_ARGS(CONFIG_APP_ALARM_CLEAR_MILLI_C));
	return 0;
}
