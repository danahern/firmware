/*
 * eai_input POSIX stub backend
 *
 * Provides fake touch + 2 buttons for native testing.
 * Events are injected via test helpers and delivered via callback or polling.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <eai_input/eai_input.h>
#include <errno.h>
#include <string.h>

/* ── Configuration defaults ─────────────────────────────────────────────── */

#ifndef CONFIG_EAI_INPUT_MAX_DEVICES
#define CONFIG_EAI_INPUT_MAX_DEVICES 8
#endif

/* ── Event queue ────────────────────────────────────────────────────────── */

#define EVENT_QUEUE_MAX 64

/* ── Module state ───────────────────────────────────────────────────────── */

static bool initialized;

/* Device table */
static struct eai_input_device devices[CONFIG_EAI_INPUT_MAX_DEVICES];
static uint8_t device_count;

/* Callback */
static eai_input_event_cb_t event_callback;
static void *event_user_data;

/* Event ring buffer */
static struct eai_input_event event_queue[EVENT_QUEUE_MAX];
static uint32_t eq_head; /* next write */
static uint32_t eq_tail; /* next read */
static uint32_t eq_count;

/* ── Default device setup ───────────────────────────────────────────────── */

static void setup_default_devices(void)
{
	device_count = 3;

	/* Device 0: Touchscreen */
	memset(&devices[0], 0, sizeof(devices[0]));
	devices[0].id = 0;
	strncpy(devices[0].name, "touch", EAI_INPUT_NAME_MAX - 1);
	devices[0].type = EAI_INPUT_DEVICE_TOUCH;
	devices[0].x_min = 0;
	devices[0].x_max = 319;
	devices[0].y_min = 0;
	devices[0].y_max = 239;

	/* Device 1: Button A */
	memset(&devices[1], 0, sizeof(devices[1]));
	devices[1].id = 1;
	strncpy(devices[1].name, "btn_a", EAI_INPUT_NAME_MAX - 1);
	devices[1].type = EAI_INPUT_DEVICE_BUTTON;

	/* Device 2: Button B */
	memset(&devices[2], 0, sizeof(devices[2]));
	devices[2].id = 2;
	strncpy(devices[2].name, "btn_b", EAI_INPUT_NAME_MAX - 1);
	devices[2].type = EAI_INPUT_DEVICE_BUTTON;
}

/* ── Module lifecycle ───────────────────────────────────────────────────── */

int eai_input_init(eai_input_event_cb_t callback, void *user_data)
{
	eq_head = 0;
	eq_tail = 0;
	eq_count = 0;
	event_callback = callback;
	event_user_data = user_data;

	setup_default_devices();
	initialized = true;
	return 0;
}

int eai_input_deinit(void)
{
	if (!initialized) {
		return -EINVAL;
	}

	event_callback = NULL;
	event_user_data = NULL;
	initialized = false;
	return 0;
}

/* ── Device enumeration ─────────────────────────────────────────────────── */

int eai_input_get_device_count(void)
{
	if (!initialized) {
		return -EINVAL;
	}
	return (int)device_count;
}

int eai_input_get_device(uint8_t index, struct eai_input_device *dev)
{
	if (!initialized || !dev) {
		return -EINVAL;
	}
	if (index >= device_count) {
		return -EINVAL;
	}

	*dev = devices[index];
	return 0;
}

int eai_input_find_device(enum eai_input_device_type type,
			  struct eai_input_device *dev)
{
	if (!initialized || !dev) {
		return -EINVAL;
	}

	for (uint8_t i = 0; i < device_count; i++) {
		if (devices[i].type == type) {
			*dev = devices[i];
			return 0;
		}
	}

	return -ENODEV;
}

/* ── Event reading ──────────────────────────────────────────────────────── */

int eai_input_read(struct eai_input_event *event, uint32_t timeout_ms)
{
	(void)timeout_ms;

	if (!initialized || !event) {
		return -EINVAL;
	}

	if (eq_count == 0) {
		return -EAGAIN;
	}

	*event = event_queue[eq_tail];
	eq_tail = (eq_tail + 1) % EVENT_QUEUE_MAX;
	eq_count--;
	return 0;
}

/* ── Test helpers ───────────────────────────────────────────────────────── */

void eai_input_test_inject_event(const struct eai_input_event *event)
{
	if (!event) {
		return;
	}

	/* If callback mode and initialized, deliver immediately */
	if (initialized && event_callback) {
		event_callback(event, event_user_data);
		return;
	}

	/* Otherwise queue for polling */
	if (eq_count >= EVENT_QUEUE_MAX) {
		return;
	}

	event_queue[eq_head] = *event;
	eq_head = (eq_head + 1) % EVENT_QUEUE_MAX;
	eq_count++;
}

void eai_input_test_reset(void)
{
	initialized = false;
	device_count = 0;
	event_callback = NULL;
	event_user_data = NULL;
	eq_head = 0;
	eq_tail = 0;
	eq_count = 0;
}
