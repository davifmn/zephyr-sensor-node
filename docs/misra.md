# MISRA C:2012

## What is checked

The portable modules in `src/lib` hold the arithmetic and the decisions:
- the ring buffer;
- the moving average;
- the rounding;
- the alarm state machine.

They have no Zephyr dependency. CI checks them with the MISRA C:2012 addon of cppcheck
(`scripts/misra.sh`), on every rule the addon implements, Required and Advisory
alike. The result is zero findings, with no suppressions and no deviations.

The script runs the addon on cppcheck's dump files. It first checks itself on a file
with known violations: `cppcheck --addon=misra` can fail silently when the addon does
not start, and then reports nothing. That happened during development, and the
self-test makes sure a clean result means a check was actually run.

The same modules are also compiled on the host with
`-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic -Werror`, and checked
by `clang-tidy` (bugprone, cert, clang-analyzer and misc checks, warnings as errors;
see `.clang-tidy`).

## What came up, and how it was resolved

| Rule | Category | Where | Change |
|---|---|---|---|
| 12.1 (precedence made explicit) | Advisory | conditions such as `a >= b \|\| c == 0U` | parentheses around each operand |
| 15.5 (single point of exit) | Advisory | early returns in the alarm and average functions | one result variable, one `return` |
| 16.4 (every `switch` has a `default`) | Required | the switches over `enum alarm_state` | a `default` documented as unreachable |
| 10.8 (a composite expression cast to a different essential type) | Required | `(int64_t)(n / 2U)` in the rounding | cast first, then divide: `(int64_t)n / 2` |

## What is not checked, and why

The Zephyr-dependent code is outside this check:
- the driver;
- the emulator;
- the threads;
- the shell commands.

It is written in the style of the Zephyr project, which has its own coding guidelines
based on MISRA C:2012. Zephyr's devicetree, logging and shell macros would need project-wide deviations
that would hide the findings that matter. This code is covered instead by the build
with warnings as errors, the ztest suites and the application tests on QEMU.

The MISRA rule texts are licensed, so the addon reports rule numbers only. A
certification would need the licensed rule texts, a qualified tool, and a deviation
process. This project does none of that: the check is a learning and review aid.
