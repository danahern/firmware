/*
 * eai_display device enumeration
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_DISPLAY_DEVICE_H
#define EAI_DISPLAY_DEVICE_H

#include <eai_display/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get the number of available display devices.
 *
 * @return Number of devices (>= 0), or negative errno.
 */
int eai_display_get_device_count(void);

/**
 * Get a display device by index.
 *
 * @param index  Device index (0..device_count-1).
 * @param dev    Output device descriptor.
 * @return 0 on success, -EINVAL if index out of range or dev is NULL.
 */
int eai_display_get_device(uint8_t index, struct eai_display_device *dev);

#ifdef __cplusplus
}
#endif

#endif /* EAI_DISPLAY_DEVICE_H */
