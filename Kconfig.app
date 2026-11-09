# The application's options, shared with the test applications under tests/.

menu "Sensor node"

config APP_SAMPLE_PERIOD_MS
	int "Sampling period (ms)"
	range 10 10000
	default 100

config APP_AVERAGE_WINDOW
	int "Moving average window (readings)"
	range 1 64
	default 8

config APP_ALARM_HIGH_MILLI_C
	int "Alarm threshold (milli-degrees Celsius)"
	default 30000

config APP_ALARM_CLEAR_MILLI_C
	int "Alarm clear threshold (milli-degrees Celsius), below the alarm threshold"
	default 28000

config APP_FAULT_AFTER
	int "Consecutive failed reads that put the node in FAULT"
	range 1 100
	default 3

config APP_QUEUE_DEPTH
	int "Readings queued between acquisition and processing"
	range 2 256
	default 16

config APP_WATCHDOG_TIMEOUT_MS
	int "Watchdog timeout of each thread (ms)"
	range 100 60000
	default 1000

config APP_HEALTH_EVERY
	int "Log a health line every this many readings (0 = never)"
	default 50

config APP_TEST_STALL_AFTER
	int "Test only: the processing thread stops after this many readings (0 = never)"
	default 0

endmenu
