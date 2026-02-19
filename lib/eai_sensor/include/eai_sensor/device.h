/*
 * eai_sensor device enumeration
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_SENSOR_DEVICE_H
#define EAI_SENSOR_DEVICE_H

#include <eai_sensor/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get the number of available sensor devices.
 *
 * @return Number of devices (>= 0), or negative errno.
 */
int eai_sensor_get_device_count(void);

/**
 * Get a sensor device by index.
 *
 * @param index  Device index (0..device_count-1).
 * @param dev    Output device descriptor.
 * @return 0 on success, -EINVAL if index out of range or dev is NULL.
 */
int eai_sensor_get_device(uint8_t index, struct eai_sensor_device *dev);

/**
 * Find the first sensor device matching a type.
 *
 * @param type  Sensor type to match.
 * @param dev   Output device descriptor.
 * @return 0 on success, -ENODEV if no matching device found.
 */
int eai_sensor_find_device(enum eai_sensor_type type,
			   struct eai_sensor_device *dev);

#ifdef __cplusplus
}
#endif

#endif /* EAI_SENSOR_DEVICE_H */
