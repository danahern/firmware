/*
 * eai_input event reading
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_INPUT_EVENT_H
#define EAI_INPUT_EVENT_H

#include <eai_input/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Read the next input event (polling mode).
 *
 * @param event       Output event.
 * @param timeout_ms  Maximum wait time (0 = non-blocking).
 * @return 0 on success, -EINVAL if event is NULL,
 *         -EAGAIN if no event available (non-blocking).
 */
int eai_input_read(struct eai_input_event *event, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* EAI_INPUT_EVENT_H */
