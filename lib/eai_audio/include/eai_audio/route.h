/*
 * eai_audio routing
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_AUDIO_ROUTE_H
#define EAI_AUDIO_ROUTE_H

#include <eai_audio/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Set a route from a source port to a sink port.
 * Source must be input direction, sink must be output direction.
 *
 * @param source_port_id  Input port ID.
 * @param sink_port_id    Output port ID.
 * @return 0 on success, -EINVAL if ports invalid, -ENOMEM if no route slots.
 */
int eai_audio_set_route(uint8_t source_port_id, uint8_t sink_port_id);

/**
 * Get the number of active routes.
 *
 * @return Number of active routes (>= 0).
 */
int eai_audio_get_route_count(void);

/**
 * Get a route by index.
 *
 * @param index  Route index (0..route_count-1).
 * @param route  Output route descriptor.
 * @return 0 on success, -EINVAL if index out of range or route is NULL.
 */
int eai_audio_get_route(uint8_t index, struct eai_audio_route *route);

#ifdef __cplusplus
}
#endif

#endif /* EAI_AUDIO_ROUTE_H */
