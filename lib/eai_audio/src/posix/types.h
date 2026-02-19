/*
 * eai_audio POSIX backend types
 *
 * Internal header — included only via include/eai_audio/types.h.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_AUDIO_POSIX_TYPES_H
#define EAI_AUDIO_POSIX_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* Per-stream backend data stored in eai_audio_stream._backend[] */
struct eai_audio_posix_stream {
	uint64_t frame_position;
	bool active;
};

#define EAI_AUDIO_STREAM_BACKEND_SIZE sizeof(struct eai_audio_posix_stream)

#endif /* EAI_AUDIO_POSIX_TYPES_H */
