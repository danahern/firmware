/*
 * eai_sensor Zephyr backend types
 *
 * Internal header — included only via include/eai_sensor/types.h.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_SENSOR_ZEPHYR_TYPES_H
#define EAI_SENSOR_ZEPHYR_TYPES_H

#include <stdbool.h>
#include <stdint.h>

/* Per-session backend data stored in eai_sensor_session._backend[] */
struct eai_sensor_zephyr_session {
	bool opened;
	bool active;
	/* Zephyr sensor device pointer would go here */
};

#define EAI_SENSOR_SESSION_BACKEND_SIZE sizeof(struct eai_sensor_zephyr_session)

#endif /* EAI_SENSOR_ZEPHYR_TYPES_H */
