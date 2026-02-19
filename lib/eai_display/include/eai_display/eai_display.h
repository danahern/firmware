/*
 * eai_display — Portable Display HAL
 *
 * Android HWComposer concepts simplified for embedded platforms.
 * Compile-time backend dispatch (Zephyr Display API, ESP LCD, Linux DRM, POSIX stub).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_DISPLAY_H
#define EAI_DISPLAY_H

#include <eai_display/types.h>
#include <eai_display/device.h>
#include <eai_display/layer.h>
#include <eai_display/property.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the display subsystem.
 * Discovers display devices from the backend.
 *
 * @return 0 on success, negative errno on error.
 */
int eai_display_init(void);

/**
 * Deinitialize the display subsystem.
 * Closes all layers and releases resources.
 *
 * @return 0 on success, negative errno on error.
 */
int eai_display_deinit(void);

/* ── Test helpers (POSIX backend only) ───────────────────────────────────── */

#if defined(CONFIG_EAI_DISPLAY_BACKEND_POSIX) || defined(EAI_DISPLAY_TEST)

/**
 * Get the framebuffer written by layers (POSIX stub only).
 * Returns pointer to the composed framebuffer after commit.
 *
 * @param buf   Output pointer to framebuffer data.
 * @param size  Output size of framebuffer in bytes.
 */
void eai_display_test_get_framebuffer(const uint8_t **buf, uint32_t *size);

/**
 * Get the commit count (number of times commit was called).
 *
 * @return Number of commits since init.
 */
uint32_t eai_display_test_get_commit_count(void);

/**
 * Reset all POSIX test state (devices, layers, framebuffer).
 */
void eai_display_test_reset(void);

#endif /* CONFIG_EAI_DISPLAY_BACKEND_POSIX || EAI_DISPLAY_TEST */

#ifdef __cplusplus
}
#endif

#endif /* EAI_DISPLAY_H */
