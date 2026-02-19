/*
 * eai_audio types — enums, structs, backend dispatch
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_AUDIO_TYPES_H
#define EAI_AUDIO_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Audio format ───────────────────────────────────────────────────────── */

enum eai_audio_format {
	EAI_AUDIO_FORMAT_PCM_S16_LE = 0,
	EAI_AUDIO_FORMAT_PCM_S24_LE,
	EAI_AUDIO_FORMAT_PCM_S32_LE,
	EAI_AUDIO_FORMAT_PCM_F32_LE,
};

/* ── Channel mask (bit flags) ───────────────────────────────────────────── */

enum eai_audio_channel_mask {
	EAI_AUDIO_CHANNEL_MONO   = 0x01,
	EAI_AUDIO_CHANNEL_STEREO = 0x03,
};

/* ── Stream direction ───────────────────────────────────────────────────── */

enum eai_audio_direction {
	EAI_AUDIO_OUTPUT = 0,
	EAI_AUDIO_INPUT  = 1,
};

/* ── Port type ──────────────────────────────────────────────────────────── */

enum eai_audio_port_type {
	EAI_AUDIO_PORT_SPEAKER = 0,
	EAI_AUDIO_PORT_MIC,
	EAI_AUDIO_PORT_I2S,
	EAI_AUDIO_PORT_BT_SCO,
	EAI_AUDIO_PORT_USB,
	EAI_AUDIO_PORT_VIRTUAL,
};

/* ── Stream configuration ───────────────────────────────────────────────── */

struct eai_audio_config {
	uint32_t sample_rate;               /* Hz */
	enum eai_audio_format format;
	enum eai_audio_channel_mask channels;
	uint32_t frame_count;               /* frames per buffer period */
};

/* ── Port capabilities ──────────────────────────────────────────────────── */

#define EAI_AUDIO_MAX_FORMATS       4
#define EAI_AUDIO_MAX_SAMPLE_RATES  8
#define EAI_AUDIO_MAX_CHANNEL_MASKS 4

struct eai_audio_profile {
	enum eai_audio_format formats[EAI_AUDIO_MAX_FORMATS];
	uint8_t format_count;
	uint32_t sample_rates[EAI_AUDIO_MAX_SAMPLE_RATES];
	uint8_t sample_rate_count;
	enum eai_audio_channel_mask channels[EAI_AUDIO_MAX_CHANNEL_MASKS];
	uint8_t channel_mask_count;
};

/* ── Per-port gain (centibels) ──────────────────────────────────────────── */

struct eai_audio_gain {
	int32_t min_cb;
	int32_t max_cb;
	int32_t step_cb;
	int32_t current_cb;
};

/* ── Audio port ─────────────────────────────────────────────────────────── */

#define EAI_AUDIO_PORT_NAME_MAX 32
#define EAI_AUDIO_MAX_PROFILES  2

struct eai_audio_port {
	uint8_t id;
	char name[EAI_AUDIO_PORT_NAME_MAX];
	enum eai_audio_direction direction;
	enum eai_audio_port_type type;
	struct eai_audio_profile profiles[EAI_AUDIO_MAX_PROFILES];
	uint8_t profile_count;
	struct eai_audio_gain gain;
	bool has_gain;
};

/* ── Audio route ────────────────────────────────────────────────────────── */

struct eai_audio_route {
	uint8_t source_port_id;
	uint8_t sink_port_id;
	bool active;
};

/* ── Backend type dispatch ──────────────────────────────────────────────── */

#if defined(CONFIG_EAI_AUDIO_BACKEND_ZEPHYR)
#include "../../src/zephyr/types.h"
#elif defined(CONFIG_EAI_AUDIO_BACKEND_FREERTOS)
#include "../../src/freertos/types.h"
#elif defined(CONFIG_EAI_AUDIO_BACKEND_ALSA)
#include "../../src/alsa/types.h"
#elif defined(CONFIG_EAI_AUDIO_BACKEND_POSIX)
#include "../../src/posix/types.h"
#else
#error "No EAI Audio backend selected"
#endif

/* ── Audio stream ───────────────────────────────────────────────────────── */

#define EAI_AUDIO_MIXER_SLOT_NONE 0xFF

struct eai_audio_stream {
	uint8_t _backend[EAI_AUDIO_STREAM_BACKEND_SIZE];
	struct eai_audio_config config;
	enum eai_audio_direction direction;
	uint8_t port_id;
	uint8_t mixer_slot; /* EAI_AUDIO_MIXER_SLOT_NONE = bypass mixer */
};

#ifdef __cplusplus
}
#endif

#endif /* EAI_AUDIO_TYPES_H */
