/* The temperature sensor, shared by the acquisition thread and the shell. */
#ifndef SENSOR_SOURCE_H
#define SENSOR_SOURCE_H

#include <stdint.h>

/* 0, or -ENODEV when the sensor did not initialize. */
int sensor_source_init(void);
/* One fresh reading in milli-degrees Celsius; 0 or the driver's negative error. */
int sensor_source_read(int32_t *milli_c);

#endif
