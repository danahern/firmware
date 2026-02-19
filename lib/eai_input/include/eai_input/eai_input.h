/*
 * eai_input — Portable Input HAL
 *
 * Android EventHub/InputReader concepts simplified for embedded platforms.
 * Compile-time backend dispatch (Zephyr Input, ESP-IDF GPIO/Touch, POSIX stub).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_INPUT_H
#define EAI_INPUT_H

#include <eai_input/types.h>
#include <eai_input/device.h>
#include <eai_input/event.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the input subsystem.
 * Discovers input devices from the backend.
 *
 * @param callback   Event callback (NULL for polling-only mode).
 * @param user_data  Context pointer passed to callback.
 * @return 0 on success, negative errno on error.
 */
int eai_input_init(eai_input_event_cb_t callback, void *user_data);

/**
 * Deinitialize the input subsystem.
 * Releases resources.
 *
 * @return 0 on success, negative errno on error.
 */
int eai_input_deinit(void);

/* ── Test helpers (POSIX backend only) ───────────────────────────────────── */

#if defined(CONFIG_EAI_INPUT_BACKEND_POSIX) || defined(EAI_INPUT_TEST)

/**
 * Inject an input event for testing.
 * Event is queued and delivered via callback or eai_input_read().
 *
 * @param event  Event to inject.
 */
void eai_input_test_inject_event(const struct eai_input_event *event);

/**
 * Reset all POSIX test state (devices, event queue).
 */
void eai_input_test_reset(void);

#endif /* CONFIG_EAI_INPUT_BACKEND_POSIX || EAI_INPUT_TEST */

#ifdef __cplusplus
}
#endif

#endif /* EAI_INPUT_H */
