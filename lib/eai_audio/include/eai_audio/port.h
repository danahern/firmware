/*
 * eai_audio port enumeration
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_AUDIO_PORT_H
#define EAI_AUDIO_PORT_H

#include <eai_audio/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get the number of available audio ports.
 *
 * @return Number of ports (>= 0).
 */
int eai_audio_get_port_count(void);

/**
 * Get an audio port by index.
 *
 * @param index  Port index (0..port_count-1).
 * @param port   Output port descriptor.
 * @return 0 on success, -EINVAL if index out of range or port is NULL.
 */
int eai_audio_get_port(uint8_t index, struct eai_audio_port *port);

/**
 * Find the first port matching a type and direction.
 *
 * @param type  Port type to match.
 * @param dir   Direction to match.
 * @param port  Output port descriptor.
 * @return 0 on success, -ENODEV if no matching port found.
 */
int eai_audio_find_port(enum eai_audio_port_type type,
			enum eai_audio_direction dir,
			struct eai_audio_port *port);

#ifdef __cplusplus
}
#endif

#endif /* EAI_AUDIO_PORT_H */
