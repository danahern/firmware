/*
 * eai_audio mini-flinger — internal mixer API
 *
 * Platform-independent software mixer using eai_osal primitives.
 * Not part of the public API — used by backend integration.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_AUDIO_MIXER_H
#define EAI_AUDIO_MIXER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Static limits for compile-time allocation */
#ifndef EAI_AUDIO_MIXER_MAX_SLOTS
#define EAI_AUDIO_MIXER_MAX_SLOTS 4
#endif

#define EAI_AUDIO_MIXER_MAX_PERIOD_FRAMES 1024
#define EAI_AUDIO_MIXER_MAX_CHANNELS      2

/* Q16 fixed-point volume: 0x10000 = unity (1.0), 0 = mute */
#define EAI_AUDIO_MIXER_VOLUME_UNITY  0x10000
#define EAI_AUDIO_MIXER_VOLUME_MUTE   0

/** Callback to write mixed audio to hardware. */
typedef int (*eai_audio_mixer_hw_write_t)(const void *buf, uint32_t frames);

/** Mixer configuration. */
struct eai_audio_mixer_config {
	uint32_t sample_rate;
	uint8_t channels;
	uint32_t period_frames;
	eai_audio_mixer_hw_write_t hw_write;
};

/**
 * Initialize the mixer thread.
 * Validates config, creates OSAL thread/mutex/semaphore.
 *
 * @return 0 on success, -EINVAL if config invalid.
 */
int eai_audio_mixer_init(const struct eai_audio_mixer_config *config);

/**
 * Stop the mixer thread and release resources.
 *
 * @return 0 on success.
 */
int eai_audio_mixer_deinit(void);

/**
 * Open a mixer slot for a new output stream.
 *
 * @param slot  Output slot index.
 * @return 0 on success, -ENOMEM if no slots available.
 */
int eai_audio_mixer_slot_open(uint8_t *slot);

/**
 * Close a mixer slot.
 *
 * @param slot  Slot index.
 * @return 0 on success, -EINVAL if slot invalid.
 */
int eai_audio_mixer_slot_close(uint8_t slot);

/**
 * Write audio data to a mixer slot's ring buffer.
 *
 * @param slot    Slot index.
 * @param data    S16_LE audio samples.
 * @param frames  Number of frames to write.
 * @return Number of frames written (may be < frames if ring full),
 *         negative errno on error.
 */
int eai_audio_mixer_write(uint8_t slot, const int16_t *data, uint32_t frames);

/**
 * Wake the mixer thread to process pending data.
 */
void eai_audio_mixer_kick(void);

/**
 * Set per-slot volume.
 *
 * @param slot       Slot index.
 * @param volume_q16 Volume in Q16 fixed-point (0x10000 = unity).
 * @return 0 on success, -EINVAL if slot invalid.
 */
int eai_audio_mixer_set_volume(uint8_t slot, uint32_t volume_q16);

/**
 * Get underrun count for a slot.
 *
 * @param slot  Slot index.
 * @return Underrun count, or 0 if slot invalid.
 */
uint32_t eai_audio_mixer_get_underruns(uint8_t slot);

#ifdef __cplusplus
}
#endif

#endif /* EAI_AUDIO_MIXER_H */
