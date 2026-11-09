cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sensor_node_lib_tests LANGUAGES C)

set(LIB ${CMAKE_CURRENT_SOURCE_DIR}/../../src/lib)
target_sources(app PRIVATE
  src/test_ring_buffer.c
  src/test_moving_average.c
  src/test_alarm.c
  ${LIB}/ring_buffer.c
  ${LIB}/moving_average.c
  ${LIB}/alarm.c
)
target_include_directories(app PRIVATE ${LIB})
