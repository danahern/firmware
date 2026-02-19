/*
 * eai_audio — Portable Audio HAL
 *
 * Android Audio HAL concepts mapped to embedded platforms.
 * Compile-time backend dispatch (Zephyr I2S, ALSA, ESP-IDF, POSIX stub).
 * Optional mini-flinger software mixer for multiple output streams.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_AUDIO_H
#define EAI_AUDIO_H

#include <eai_audio/types.h>
#include <eai_audio/port.h>
#include <eai_audio/stream.h>
#include <eai_audio/gain.h>
#include <eai_audio/route.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the audio subsystem.
 * Discovers ports from the backend and sets up internal state.
 *
 * @return 0 on success, negative errno on error.
 */
int eai_audio_init(void);

/**
 * Deinitialize the audio subsystem.
 * Closes all streams and releases resources.
 *
 * @return 0 on success, negative errno on error.
 */
int eai_audio_deinit(void);

/* ── Test helpers (POSIX backend only) ───────────────────────────────────── */

#if defined(CONFIG_EAI_AUDIO_BACKEND_POSIX) || defined(EAI_AUDIO_TEST)

/**
 * Get the output buffer written by streams (POSIX stub only).
 * Returns pointer to internal buffer and the number of frames written.
 *
 * @param buf     Output pointer to buffer data (int16_t samples).
 * @param frames  Output number of frames in buffer.
 */
void eai_audio_test_get_output(const int16_t **buf, uint32_t *frames);

/**
 * Set input data that stream_read() will return (POSIX stub only).
 *
 * @param data    Audio data (int16_t samples).
 * @param frames  Number of frames.
 */
void eai_audio_test_set_input(const int16_t *data, uint32_t frames);

/**
 * Reset all POSIX test state (ports, streams, buffers).
 */
void eai_audio_test_reset(void);

#endif /* CONFIG_EAI_AUDIO_BACKEND_POSIX || EAI_AUDIO_TEST */

#ifdef __cplusplus
}
#endif

#endif /* EAI_AUDIO_H */
