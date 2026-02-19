/*
 * eai_input types — enums, structs, backend dispatch
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_INPUT_TYPES_H
#define EAI_INPUT_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Input device type ─────────────────────────────────────────────────── */

enum eai_input_device_type {
	EAI_INPUT_DEVICE_TOUCH = 0,
	EAI_INPUT_DEVICE_BUTTON,
	EAI_INPUT_DEVICE_ENCODER,
	EAI_INPUT_DEVICE_KEYBOARD,
	EAI_INPUT_DEVICE_GESTURE,
};

/* ── Input event type ──────────────────────────────────────────────────── */

enum eai_input_event_type {
	EAI_INPUT_EVENT_PRESS = 0,
	EAI_INPUT_EVENT_RELEASE,
	EAI_INPUT_EVENT_MOVE,
	EAI_INPUT_EVENT_LONG_PRESS,
};

/* ── Input event ───────────────────────────────────────────────────────── */

struct eai_input_event {
	uint8_t device_id;
	enum eai_input_event_type type;
	int16_t x;
	int16_t y;
	uint16_t code; /* key/button code */
	uint32_t timestamp_ms;
};

/* ── Input device info ─────────────────────────────────────────────────── */

#define EAI_INPUT_NAME_MAX 32

struct eai_input_device {
	uint8_t id;
	char name[EAI_INPUT_NAME_MAX];
	enum eai_input_device_type type;
	int16_t x_min;
	int16_t x_max;
	int16_t y_min;
	int16_t y_max;
};

/* ── Event callback ────────────────────────────────────────────────────── */

typedef void (*eai_input_event_cb_t)(const struct eai_input_event *event,
				     void *user_data);

/* ── Backend type dispatch ─────────────────────────────────────────────── */

#if defined(CONFIG_EAI_INPUT_BACKEND_ZEPHYR)
#include "../../src/zephyr/types.h"
#elif defined(CONFIG_EAI_INPUT_BACKEND_FREERTOS)
#include "../../src/freertos/types.h"
#elif defined(CONFIG_EAI_INPUT_BACKEND_POSIX)
#include "../../src/posix/types.h"
#else
#error "No EAI Input backend selected"
#endif

#ifdef __cplusplus
}
#endif

#endif /* EAI_INPUT_TYPES_H */
