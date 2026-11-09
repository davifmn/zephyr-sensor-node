# Requirements

Each requirement has an id. The tests name the requirements they verify:

- a `/* Verifies: REQ-nnn */` comment right above a ztest;
- a `# Verifies: REQ-nnn` comment right above a Twister scenario in `sample.yaml`.

`scripts/trace.py` builds the requirement → test → result matrix from Twister's
`twister.json`. CI publishes the matrix and fails when a requirement has no test, a test
cites an unknown requirement, or a test did not pass on every platform.

Defaults are in parentheses. Every value is a Kconfig option (`Kconfig.app`).

### REQ-001 Periodic sampling
The node reads the temperature every `CONFIG_APP_SAMPLE_PERIOD_MS` (100 ms), on absolute
deadlines so the period does not drift. It hands each reading to processing with a
sequence number (1, 2, 3, …), whether the read succeeded or failed. Processing counts
gaps in the sequence.

### REQ-002 Moving average
The average is taken over the last `CONFIG_APP_AVERAGE_WINDOW` (8) good readings.
Until the window is full, it is the average of the readings so far. It is rounded to the
nearest milli-degree, halves away from zero, negative values included.

### REQ-003 Alarm on
When the average reaches `CONFIG_APP_ALARM_HIGH_MILLI_C` (30.000 °C), the node goes to
HIGH and reports it once.

### REQ-004 Alarm off, with hysteresis
The alarm clears only when the average falls to `CONFIG_APP_ALARM_CLEAR_MILLI_C`
(28.000 °C), which must be below the alarm threshold. Between the two thresholds the
state does not change, so an average that wanders around 30 °C does not make the alarm
chatter.

### REQ-005 Sensor fault and recovery
After `CONFIG_APP_FAULT_AFTER` (3) failed reads in a row, the node goes to FAULT and
reports it once. Isolated failures do not cause a fault. The first good reading
recovers the node into the state its average calls for, NORMAL or HIGH.

### REQ-006 No blocking under overload
When processing falls behind and the queue (`CONFIG_APP_QUEUE_DEPTH`, 16) is full, the
new reading is dropped and counted. Acquisition never waits for room, so sampling stays
on time.

### REQ-007 History
The last 64 good readings are kept, with their minimum, maximum and mean on request.

### REQ-008 Integer arithmetic without overflow
Averages and statistics are computed in integer milli-degrees, with no overflow over the
whole `int32_t` range of readings. No floating point is used.

### REQ-009 Sensor driver
The driver reads the THERMO-1 temperature register in one I2C transaction: the register
pointer, a repeated start, then 2 bytes. It converts the value (big-endian, two's
complement, 7.8125 m°C per LSB) to milli-degrees within one LSB, through the standard
Zephyr sensor API. Other channels are refused with `-ENOTSUP`.

### REQ-010 Device identification
At initialization the driver reads `DEVICE_ID`. On any value other than `0x5431` the
device is not ready, and the application refuses to start rather than read the wrong
chip.

### REQ-011 Bus errors
A failed read is reported to the caller (`-EIO`) and reaches processing as an error
reading. The previous value is never returned as a new one: `channel_get` after a failed
fetch returns `-ENODATA`.

### REQ-012 Watchdog
Each thread has its own watchdog channel, fed only from its own loop. If either thread
stops for `CONFIG_APP_WATCHDOG_TIMEOUT_MS` (1 s), the node prints which thread stalled
and reboots.

### REQ-013 Shell
- `sensor read` reads the sensor now (sharing it safely with the acquisition thread).
- `sensor fail <n>` makes the next n reads fail (emulated sensor only, 1 to 100).
- `stats` shows the state, the counters (processed, dropped, out of order, failed), the
  last value, the average and the history statistics.
