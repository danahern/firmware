/*
 * eai_audio gain control
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_AUDIO_GAIN_H
#define EAI_AUDIO_GAIN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Set gain for a port in centibels.
 *
 * @param port_id   Port ID.
 * @param gain_cb   Gain in centibels (clamped to port's min/max).
 * @return 0 on success, -EINVAL if port not found, -ENOTSUP if port has no gain.
 */
int eai_audio_set_gain(uint8_t port_id, int32_t gain_cb);

/**
 * Get current gain for a port in centibels.
 *
 * @param port_id   Port ID.
 * @param gain_cb   Output gain value.
 * @return 0 on success, -EINVAL if port not found or gain_cb is NULL,
 *         -ENOTSUP if port has no gain.
 */
int eai_audio_get_gain(uint8_t port_id, int32_t *gain_cb);

#ifdef __cplusplus
}
#endif

#endif /* EAI_AUDIO_GAIN_H */
