/*
 * eai_display POSIX backend types
 *
 * Internal header — included only via include/eai_display/types.h.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_DISPLAY_POSIX_TYPES_H
#define EAI_DISPLAY_POSIX_TYPES_H

#include <stdbool.h>
#include <stdint.h>

/* Per-layer backend data stored in eai_display_layer._backend[] */
struct eai_display_posix_layer {
	bool opened;
	uint8_t slot_index;
};

#define EAI_DISPLAY_LAYER_BACKEND_SIZE sizeof(struct eai_display_posix_layer)

#endif /* EAI_DISPLAY_POSIX_TYPES_H */
