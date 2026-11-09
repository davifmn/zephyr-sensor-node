cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sensor_node LANGUAGES C)

target_sources(app PRIVATE
  src/main.c
  src/acquisition.c
  src/node.c
  src/sensor_source.c
  src/shell_cmds.c
  src/drivers/thermo_i2c.c
  src/lib/ring_buffer.c
  src/lib/moving_average.c
  src/lib/alarm.c
)
# The emulated sensor stands in for the hardware (app.overlay puts it on an emulated
# I2C bus). A build for a real board drops the overlay and this file.
target_sources_ifdef(CONFIG_EMUL app PRIVATE src/drivers/thermo_i2c_emul.c)
target_include_directories(app PRIVATE src src/lib)

# The portable modules (no Zephyr headers) also get the conversion warnings; the whole
# build treats warnings as errors (CONFIG_COMPILER_WARNINGS_AS_ERRORS).
set_source_files_properties(src/lib/ring_buffer.c src/lib/moving_average.c src/lib/alarm.c
  PROPERTIES COMPILE_OPTIONS "-Wextra;-Wshadow;-Wconversion;-Wsign-conversion")
