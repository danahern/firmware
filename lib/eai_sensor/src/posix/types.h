/*
 * eai_sensor POSIX backend types
 *
 * Internal header — included only via include/eai_sensor/types.h.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_SENSOR_POSIX_TYPES_H
#define EAI_SENSOR_POSIX_TYPES_H

#include <stdbool.h>
#include <stdint.h>

/* Per-session backend data stored in eai_sensor_session._backend[] */
struct eai_sensor_posix_session {
	bool opened;
	bool active;
};

#define EAI_SENSOR_SESSION_BACKEND_SIZE sizeof(struct eai_sensor_posix_session)

#endif /* EAI_SENSOR_POSIX_TYPES_H */
