cmake_minimum_required(VERSION 3.20.0)
# The application's devicetree binding lives in its dts/bindings.
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sensor_node_system_tests LANGUAGES C)

set(APP ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
target_sources(app PRIVATE
  src/test_driver.c
  src/test_node.c
  src/test_shell.c
  ${APP}/acquisition.c
  ${APP}/node.c
  ${APP}/sensor_source.c
  ${APP}/shell_cmds.c
  ${APP}/drivers/thermo_i2c.c
  ${APP}/drivers/thermo_i2c_emul.c
  ${APP}/lib/ring_buffer.c
  ${APP}/lib/moving_average.c
  ${APP}/lib/alarm.c
)
target_include_directories(app PRIVATE ${APP} ${APP}/lib)
