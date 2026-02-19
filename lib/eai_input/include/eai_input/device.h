/*
 * eai_input device enumeration
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_INPUT_DEVICE_H
#define EAI_INPUT_DEVICE_H

#include <eai_input/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get the number of available input devices.
 *
 * @return Number of devices (>= 0), or negative errno.
 */
int eai_input_get_device_count(void);

/**
 * Get an input device by index.
 *
 * @param index  Device index (0..device_count-1).
 * @param dev    Output device descriptor.
 * @return 0 on success, -EINVAL if index out of range or dev is NULL.
 */
int eai_input_get_device(uint8_t index, struct eai_input_device *dev);

/**
 * Find the first input device matching a type.
 *
 * @param type  Device type to match.
 * @param dev   Output device descriptor.
 * @return 0 on success, -ENODEV if no matching device found.
 */
int eai_input_find_device(enum eai_input_device_type type,
			  struct eai_input_device *dev);

#ifdef __cplusplus
}
#endif

#endif /* EAI_INPUT_DEVICE_H */
