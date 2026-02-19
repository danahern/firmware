/*
 * eai_display property get/set
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_DISPLAY_PROPERTY_H
#define EAI_DISPLAY_PROPERTY_H

#include <eai_display/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Commit all pending layer writes to the display.
 * This presents the composed frame.
 *
 * @param display_id  Display to commit.
 * @return 0 on success, -EINVAL if display not found.
 */
int eai_display_commit(uint8_t display_id);

/**
 * Set display brightness.
 *
 * @param display_id  Display ID.
 * @param percent     Brightness percentage (0-100).
 * @return 0 on success, -EINVAL if display not found, -ENOTSUP if no backlight.
 */
int eai_display_set_brightness(uint8_t display_id, uint8_t percent);

/**
 * Get display brightness.
 *
 * @param display_id  Display ID.
 * @param percent     Output brightness percentage.
 * @return 0 on success, -EINVAL if display not found or percent is NULL,
 *         -ENOTSUP if no backlight.
 */
int eai_display_get_brightness(uint8_t display_id, uint8_t *percent);

/**
 * Enable or disable vsync callback.
 *
 * @param display_id  Display ID.
 * @param enabled     Enable or disable vsync.
 * @param cb          Callback function (NULL to disable).
 * @param user_data   Context pointer passed to callback.
 * @return 0 on success, -EINVAL if display not found.
 */
int eai_display_set_vsync(uint8_t display_id, bool enabled,
			  eai_display_vsync_cb_t cb, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* EAI_DISPLAY_PROPERTY_H */
