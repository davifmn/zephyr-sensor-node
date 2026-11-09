# zephyr-sensor-node

A temperature sensor node in C on the **Zephyr RTOS**. It runs on QEMU (Arm Cortex-M3
and the Arm MPS2 AN385), so no board is needed:

- **I2C sensor driver** behind the standard Zephyr sensor API. It is tested against an
  emulator of the chip on Zephyr's emulated I2C bus, which checks the shape of every
  transfer, as the chip would.
- **Two threads and a message queue.** Acquisition samples on absolute deadlines and
  never blocks. Processing keeps a moving average, an alarm state machine (threshold
  with hysteresis, fault after consecutive failed reads) and a 64-reading history.
- **One watchdog channel per thread.** A stuck thread is named on the console and the
  node reboots.
- **Shell:** `sensor read`, `sensor fail <n>` and `stats`.
- **Requirements → tests → results.** 13 numbered requirements. Every ztest and every
  Twister scenario cites the ones it verifies, and CI builds the traceability matrix from
  Twister's results. It fails if a requirement is untested or a test fails.
- **Static analysis:** MISRA C:2012 (cppcheck addon), clang-tidy and strict warnings on
  the portable modules, and warnings as errors everywhere.
- **A scripted GDB session** that CI runs against QEMU: a conditional breakpoint in a
  thread, then a breakpoint in interrupt context.

C11 · Zephyr 4.5 (west, Kconfig, devicetree) · Arm Cortex-M · I2C · ztest / Twister · QEMU · GDB · cppcheck + MISRA C:2012 · clang-tidy · GitHub Actions

```
                      ┌────────────── emulated I2C bus (zephyr,i2c-emul-controller) ──────────────┐
                      │  THERMO-1 emulator @0x48: answers register reads, can drop off the bus   │
                      └───────────────────────────────▲──────────────────────────────────────────┘
                                                      │ i2c_write_read: reg pointer, Sr, 2 bytes
                                       src/drivers/thermo_i2c.c (Zephyr sensor API)
                                                      │ sensor_sample_fetch / channel_get (under a mutex)
  shell: sensor read ─────────────────────────────────┤
                                                      │
  acquisition thread (prio 5) ── every 100 ms, absolute deadlines ──▶ k_msgq (16) ──▶ processing thread (prio 7)
      │ reading + sequence number; if the queue is full:                         │ moving average (8)
      │ drop and count, never wait                                                │ alarm: NORMAL / HIGH / FAULT
      ▼                                                                           │ history (64): min, max, mean
  task_wdt channel 1                                                              ▼
                                                                         task_wdt channel 2   ◀── shell: stats
  either channel expires (1 s) ──▶ "WATCHDOG: <thread> thread stalled, rebooting" ──▶ sys_reboot
```

## Build and run

You need a Zephyr workspace: west and the Zephyr SDK 1.0.1, set up as in Zephyr's
*Getting Started* guide. `west.yml` pins Zephyr v4.5.0 and fetches only the modules
needed.

```sh
mkdir sensor-ws && cd sensor-ws
git clone https://github.com/davifmn/zephyr-sensor-node
west init -l zephyr-sensor-node && west update
cd zephyr-sensor-node

west build -b qemu_cortex_m3 .        # or -b mps2/an385
west build -t run                     # boots in QEMU, with the shell on the console
```

The console of a CI run (the `sensor_node.app.shell` scenario on qemu_cortex_m3; colours
removed, prompts trimmed):

```
*** Booting Zephyr OS build 41d10b597304 ***
[00:00:00.000,000] <inf> main: sensor node started: a reading every 100 ms, alarm at 30.000 C, clear at 28.000 C
[00:00:02.900,000] <wrn> node: ALARM ON: average 30.274 C reached 30.000 C
[00:00:04.900,000] <inf> node: health: 50 readings processed, 0 dropped, 0 out of order, 0 failed reads
[00:00:06.300,000] <inf> node: ALARM OFF: average 27.825 C back to 28.000 C
uart:~$ sensor read
temperature: 30.101 C
uart:~$ stats
state: NORMAL
readings: 105 processed, 0 dropped, 0 out of order, 0 failed
last: 29.750 C, average: 28.524 C
last 64: min 21.000 C, max 34.296 C, mean 26.829 C
uart:~$ sensor fail 3
the next 3 reads will fail
uart:~$ sensor fail 0
usage: sensor fail <1-100>
[00:00:09.900,000] <inf> node: health: 100 readings processed, 0 dropped, 0 out of order, 0 failed reads
[00:00:10.800,000] <err> node: FAULT: 3 failed reads in a row
[00:00:10.900,000] <inf> node: RECOVERED: sensor answers again, state NORMAL
[00:00:11.100,000] <wrn> node: ALARM ON: average 30.494 C reached 30.000 C
[00:00:14.900,000] <inf> node: health: 150 readings processed, 0 dropped, 0 out of order, 3 failed reads
```

The emulated temperature follows a triangle from 21 °C up to 35 °C and back, over 80
readings. Each cycle raises and clears the alarm.

## Tests

```sh
west twister -T . --integration       # everything below, on qemu_cortex_m3 and mps2/an385
                                      # CI: 12 of 12 configurations, 76 of 76 test cases passed
python3 scripts/trace.py twister-out/twister.json   # the requirement → test → result matrix
```

| Suite | What it checks |
|---|---|
| `tests/lib` (ztest) | ring buffer, moving average, alarm state machine: window and order, rounding of negative averages, overflow over the whole `int32_t` range, the hysteresis (no chatter around the threshold), fault only on *consecutive* failures, recovery into NORMAL or HIGH |
| `tests/system` (ztest) | **the driver against the I2C emulator:** conversion within one LSB, one register read per fetch, `-ENOTSUP` for other channels, a bus error reported and the old value *not* handed out again (`-ENODATA`), a chip with the wrong `DEVICE_ID` refused at init. **The node:** sequence gaps, drops when the queue is full (without waiting), the average driving the alarm. **The shell**, through the dummy backend: `sensor read`, `sensor fail`, `stats` |
| `sample.yaml` (whole application on QEMU, checked from the console) | `alarm_cycle`: the alarm turns on and off, and health lines at 50 and 100 readings show nothing dropped. `sensor_fault`: the chip drops off the bus for 3 reads, then FAULT, then RECOVERED. `watchdog`: the processing thread stops, the watchdog names it, and the node **boots again**. `shell`: the commands typed on the real serial console (Twister's shell harness) |

Requirements are in [docs/requirements.md](docs/requirements.md). A test cites them right
above itself:

```c
/* Verifies: REQ-011 */
ZTEST(thermo_driver, test_bus_error_is_reported_and_the_old_value_not_reused)
```

`scripts/trace.py` turns those comments and Twister's `twister.json` into the matrix that
CI publishes on every run (the job summary, and `traceability.md` in the artifact).

## Static analysis

```sh
scripts/misra.sh                                     # MISRA C:2012 on src/lib (cppcheck addon)
clang-tidy src/lib/*.c -- -std=c11 -Isrc/lib         # .clang-tidy: bugprone, cert, clang-analyzer, misc
```

The portable modules (`src/lib`) have zero MISRA findings, with no deviations. They also
compile cleanly with `-Wextra -Wconversion -Wsign-conversion -Wpedantic -Werror`. The rest
of the code builds with warnings as errors.

[docs/misra.md](docs/misra.md) lists what is checked and the rules that came up. It also
explains why the script tests itself first: `cppcheck --addon=misra` can fail silently
and report nothing.

## Debugging with GDB

```sh
west build -b qemu_cortex_m3 -d build-gdb . -- -DEXTRA_CONF_FILE=debug.conf   # -Og, thread info
west build -d build-gdb -t debugserver_qemu        # QEMU halted at reset, GDB server on :1234
arm-zephyr-eabi-gdb build-gdb/zephyr/zephyr.elf    # (or gdb-multiarch)
(gdb) target remote :1234
```

CI runs `scripts/gdb-session.gdb` against a build whose processing thread stalls
(`tests/overlays/stall.conf`). It saves the output as `gdb-session.txt` in the run
artifact. Excerpt, with paths shortened:

```
(gdb) break node_process if r->seq == 5
(gdb) continue
Breakpoint 1, node_process (r=r@entry=0x20001b8c <processing_stack+980>) at src/node.c:61
(gdb) print *r
$1 = {seq = 5, status = 0, milli_c = 22750}
(gdb) backtrace
#0  node_process (r=...) at src/node.c:61
#1  processing (...) at src/main.c:77
#2  z_thread_entry (entry=0x9a9 <processing>, ...) at zephyr/kernel/sys/thread_entry.c:60
(gdb) print 'node.c'::alarm
$3 = {high = 30000, clear = 28000, fault_after = 3, failures = 0, state = ALARM_NORMAL}

(gdb) break watchdog_expired
(gdb) continue
Breakpoint 2, watchdog_expired (channel_id=1, user_data=0x20000010 <processing_name>) at src/main.c:42
(gdb) print (const char *)user_data
$4 = 0x20000010 <processing_name> "processing"
(gdb) print/d $xpsr & 0x1ff               # the active exception: 15 = SysTick
$5 = 15
(gdb) backtrace
#0  watchdog_expired (channel_id=1, user_data=...) at src/main.c:42
#1  task_wdt_trigger (...) at zephyr/subsys/task_wdt/task_wdt.c:122
#2  z_timer_expiration_handler (t=... <timer>) at zephyr/kernel/timer.c:129
#3  sys_clock_announce_locked (...) at zephyr/kernel/timeout.c:401
#4  timer_core_announce_from (...) at zephyr/drivers/timer/system_timer_generic.h:705
#5  sys_clock_isr () at zephyr/drivers/timer/cortex_m_systick.c:340
#6  <signal handler called>
#7  __ISB () at cmsis_gcc.h:175
#8  arch_cpu_idle () at zephyr/arch/arm/core/cortex_m/cpu_idle.c:105
#10 idle (...) at zephyr/kernel/idle.c:78
```

The second backtrace runs from the watchdog callback down through the SysTick interrupt
(`<signal handler called>` is GDB unwinding the Cortex-M exception frame) to the idle
thread it interrupted. The processing thread is stalled, so the CPU was idle when the
watchdog fired.

## Footprint

`qemu_cortex_m3` (TI LM3S6965: 256 KB flash, 64 KB RAM), with shell, logging, asserts and
the stack sentinel enabled:

| | Used | Of |
|---|---|---|
| Flash | 72,300 B | 28 % |
| RAM | 16,240 B | 25 % |

## Design decisions

- **The sensor is emulated at the bus, not at the API.**
  - A fake `sensor_channel_get` would test nothing of the driver.
  - The emulator sits behind Zephyr's I2C controller emulator and answers the actual
    transfers: the register pointer, a repeated start, 2 bytes. A driver that sent the
    wrong shape of transaction would get a NACK in the tests.
  - On real hardware, the same sensor node goes under the board's I2C controller in the
    devicetree, and the driver is unchanged.
- **Acquisition never blocks.**
  - Readings go into a `k_msgq` with `K_NO_WAIT`, and a full queue drops the reading and
    counts it.
  - Acquisition runs at a higher priority, so sampling stays on time when processing
    lags.
  - The period uses absolute deadlines (`K_TIMEOUT_ABS_MS`), so it does not drift by the
    time spent reading.
- **One watchdog channel per thread.**
  - A single watchdog fed from one place proves only that that place runs.
  - With `task_wdt`, each thread feeds its own channel from its own loop. The callback
    knows which channel expired, so the console names the thread before the reset.
  - On a board with a hardware watchdog, `CONFIG_TASK_WDT_HW_FALLBACK` covers a stuck
    kernel too.
- **The arithmetic stays in integers.**
  - Readings are in milli-degrees (`int32_t`), sums are 64-bit, and rounding is to the
    nearest, negative values included.
  - Printing uses `"%s%u.%03u"`, so neither the firmware nor `printk` needs floating
    point.
- **The portable core is separate.** The ring buffer, average, rounding and alarm use no
  Zephyr headers, so they get MISRA, clang-tidy and host-compiler checks. The
  Zephyr-dependent code is covered by the ztest suites and the application scenarios.

## Layout

```
src/main.c              threads, message queue, watchdog channels
src/acquisition.c       one sampling step: read, queue, count drops
src/node.c              processing: average, alarm, history, health log
src/sensor_source.c     the sensor device, shared under a mutex (thread and shell)
src/shell_cmds.c        sensor read | sensor fail | stats
src/drivers/            THERMO-1 driver and its I2C emulator (docs/sensor.md)
src/lib/                portable modules: ring buffer, moving average, alarm, rounding
dts/bindings/           devicetree binding of the sensor
tests/lib, tests/system ztest suites; sample.yaml has the application scenarios
scripts/                trace.py, misra.sh, gdb-session.gdb
```

## License

MIT
