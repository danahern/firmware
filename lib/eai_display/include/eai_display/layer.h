/*
 * eai_display layer lifecycle + pixel I/O
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_DISPLAY_LAYER_H
#define EAI_DISPLAY_LAYER_H

#include <eai_display/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Open a layer on a display.
 *
 * @param layer       Layer to initialize (caller-allocated).
 * @param display_id  Target display ID.
 * @param config      Layer configuration (position, size, format).
 * @return 0 on success, -EINVAL if args invalid, -ENODEV if display not found,
 *         -ENOMEM if no layer slots available.
 */
int eai_display_layer_open(struct eai_display_layer *layer,
			   uint8_t display_id,
			   const struct eai_display_layer_config *config);

/**
 * Write pixel data to a layer.
 *
 * @param layer   Layer to write to.
 * @param pixels  Pixel data (format per layer config).
 * @param size    Size of pixel data in bytes.
 * @return 0 on success, -EINVAL if args invalid.
 */
int eai_display_layer_write(struct eai_display_layer *layer,
			    const void *pixels, uint32_t size);

/**
 * Close a layer and release resources.
 *
 * @param layer  Layer to close.
 * @return 0 on success, -EINVAL if layer is NULL.
 */
int eai_display_layer_close(struct eai_display_layer *layer);

#ifdef __cplusplus
}
#endif

#endif /* EAI_DISPLAY_LAYER_H */
