/*
 * eai_display types — enums, structs, backend dispatch
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_DISPLAY_TYPES_H
#define EAI_DISPLAY_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Pixel format ──────────────────────────────────────────────────────── */

enum eai_display_format {
	EAI_DISPLAY_FORMAT_MONO1 = 0,   /* 1 bit per pixel */
	EAI_DISPLAY_FORMAT_RGB565,       /* 16 bpp */
	EAI_DISPLAY_FORMAT_RGB888,       /* 24 bpp */
	EAI_DISPLAY_FORMAT_ARGB8888,     /* 32 bpp */
};

/* ── Display device info ───────────────────────────────────────────────── */

#define EAI_DISPLAY_NAME_MAX 32
#define EAI_DISPLAY_MAX_FORMATS 4

struct eai_display_device {
	uint8_t id;
	char name[EAI_DISPLAY_NAME_MAX];
	uint16_t width;
	uint16_t height;
	enum eai_display_format formats[EAI_DISPLAY_MAX_FORMATS];
	uint8_t format_count;
	uint8_t max_fps;
	uint8_t max_layers;
};

/* ── Layer configuration ───────────────────────────────────────────────── */

struct eai_display_layer_config {
	uint16_t x;
	uint16_t y;
	uint16_t width;
	uint16_t height;
	enum eai_display_format format;
};

/* ── Vsync callback ────────────────────────────────────────────────────── */

typedef void (*eai_display_vsync_cb_t)(uint8_t display_id, uint64_t timestamp_ns,
				       void *user_data);

/* ── Backend type dispatch ─────────────────────────────────────────────── */

#if defined(CONFIG_EAI_DISPLAY_BACKEND_ZEPHYR)
#include "../../src/zephyr/types.h"
#elif defined(CONFIG_EAI_DISPLAY_BACKEND_FREERTOS)
#include "../../src/freertos/types.h"
#elif defined(CONFIG_EAI_DISPLAY_BACKEND_POSIX)
#include "../../src/posix/types.h"
#else
#error "No EAI Display backend selected"
#endif

/* ── Display layer handle ──────────────────────────────────────────────── */

struct eai_display_layer {
	uint8_t _backend[EAI_DISPLAY_LAYER_BACKEND_SIZE];
	struct eai_display_layer_config config;
	uint8_t display_id;
};

#ifdef __cplusplus
}
#endif

#endif /* EAI_DISPLAY_TYPES_H */
