/*
 * eai_audio mini-flinger — software mixer
 *
 * Platform-independent. Uses eai_osal for thread, mutex, semaphore.
 * Mixes up to N output streams (S16_LE) via int32 accumulator with
 * per-slot Q16 volume and hard clipping.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mixer.h"
#include <eai_osal/eai_osal.h>
#include <string.h>

/* ── Ring buffer sizing ─────────────────────────────────────────────────── */

/* Per-slot ring capacity in samples (2x max period * max channels) */
#define RING_CAP_SAMPLES \
	(2 * EAI_AUDIO_MIXER_MAX_PERIOD_FRAMES * EAI_AUDIO_MIXER_MAX_CHANNELS)

/* Mix output buffer in samples */
#define MIX_BUF_SAMPLES \
	(EAI_AUDIO_MIXER_MAX_PERIOD_FRAMES * EAI_AUDIO_MIXER_MAX_CHANNELS)

/* ── Per-slot state ─────────────────────────────────────────────────────── */

struct mixer_slot {
	int16_t ring[RING_CAP_SAMPLES];
	uint32_t wr; /* total samples written (monotonic) */
	uint32_t rd; /* total samples read (monotonic) */
	uint32_t volume; /* Q16: 0x10000 = unity */
	uint32_t underruns;
	bool active;
};

/* ── Module state ───────────────────────────────────────────────────────── */

static struct {
	struct eai_audio_mixer_config config;
	struct mixer_slot slots[EAI_AUDIO_MIXER_MAX_SLOTS];

	int16_t mix_buf[MIX_BUF_SAMPLES];

	eai_osal_thread_t thread;
	eai_osal_mutex_t mutex;
	eai_osal_sem_t sem;

	bool running;
	bool initialized;
} mixer;

EAI_OSAL_THREAD_STACK_DEFINE(mixer_stack, 2048);

/* ── Ring buffer helpers ────────────────────────────────────────────────── */

static uint32_t ring_count(const struct mixer_slot *s)
{
	return s->wr - s->rd;
}

static uint32_t ring_space(const struct mixer_slot *s)
{
	return RING_CAP_SAMPLES - ring_count(s);
}

static void ring_write(struct mixer_slot *s, const int16_t *data,
		       uint32_t samples)
{
	for (uint32_t i = 0; i < samples; i++) {
		s->ring[s->wr % RING_CAP_SAMPLES] = data[i];
		s->wr++;
	}
}

static void ring_read(struct mixer_slot *s, int16_t *data, uint32_t samples)
{
	for (uint32_t i = 0; i < samples; i++) {
		data[i] = s->ring[s->rd % RING_CAP_SAMPLES];
		s->rd++;
	}
}

/* ── Mixer thread ───────────────────────────────────────────────────────── */

static void mixer_thread_entry(void *arg)
{
	(void)arg;

	uint32_t period_samples =
		mixer.config.period_frames * mixer.config.channels;

	/* Compute period in ms for timeout-based wakeup */
	uint32_t period_ms = (mixer.config.period_frames * 1000) /
			     mixer.config.sample_rate;
	if (period_ms == 0) {
		period_ms = 1;
	}

	int16_t slot_buf[MIX_BUF_SAMPLES];

	while (mixer.running) {
		/* Wait for kick or timeout */
		eai_osal_sem_take(&mixer.sem, period_ms);

		if (!mixer.running) {
			break;
		}

		eai_osal_mutex_lock(&mixer.mutex, EAI_OSAL_WAIT_FOREVER);

		/* Zero mix buffer */
		memset(mixer.mix_buf, 0, period_samples * sizeof(int16_t));

		bool any_active = false;

		for (uint8_t i = 0; i < EAI_AUDIO_MIXER_MAX_SLOTS; i++) {
			struct mixer_slot *slot = &mixer.slots[i];

			if (!slot->active) {
				continue;
			}
			any_active = true;

			uint32_t avail = ring_count(slot);

			if (avail < period_samples) {
				/* Underrun: read what's available, rest is silence */
				slot->underruns++;
				memset(slot_buf, 0,
				       period_samples * sizeof(int16_t));
				if (avail > 0) {
					ring_read(slot, slot_buf, avail);
				}
			} else {
				ring_read(slot, slot_buf, period_samples);
			}

			/* Mix into accumulator with volume */
			for (uint32_t j = 0; j < period_samples; j++) {
				int32_t scaled = ((int32_t)slot_buf[j] *
						  (int32_t)slot->volume) >> 16;
				int32_t acc = (int32_t)mixer.mix_buf[j] + scaled;

				/* Hard clip to int16 range */
				if (acc > 32767) {
					acc = 32767;
				} else if (acc < -32768) {
					acc = -32768;
				}

				mixer.mix_buf[j] = (int16_t)acc;
			}
		}

		eai_osal_mutex_unlock(&mixer.mutex);

		/* Write mixed output to hardware */
		if (any_active && mixer.config.hw_write) {
			mixer.config.hw_write(mixer.mix_buf,
					      mixer.config.period_frames);
		}
	}
}

/* ── Public API ─────────────────────────────────────────────────────────── */

int eai_audio_mixer_init(const struct eai_audio_mixer_config *config)
{
	if (!config || !config->hw_write) {
		return -1; /* EINVAL */
	}
	if (config->period_frames == 0 ||
	    config->period_frames > EAI_AUDIO_MIXER_MAX_PERIOD_FRAMES) {
		return -1;
	}
	if (config->channels == 0 ||
	    config->channels > EAI_AUDIO_MIXER_MAX_CHANNELS) {
		return -1;
	}
	if (mixer.initialized) {
		return -1;
	}

	memset(&mixer, 0, sizeof(mixer));
	mixer.config = *config;

	/* Default all slots to unity volume */
	for (uint8_t i = 0; i < EAI_AUDIO_MIXER_MAX_SLOTS; i++) {
		mixer.slots[i].volume = EAI_AUDIO_MIXER_VOLUME_UNITY;
	}

	eai_osal_status_t rc;

	rc = eai_osal_mutex_create(&mixer.mutex);
	if (rc != EAI_OSAL_OK) {
		return -1;
	}

	rc = eai_osal_sem_create(&mixer.sem, 0, 1);
	if (rc != EAI_OSAL_OK) {
		eai_osal_mutex_destroy(&mixer.mutex);
		return -1;
	}

	mixer.running = true;
	mixer.initialized = true;

	rc = eai_osal_thread_create(&mixer.thread, "mixer",
				    mixer_thread_entry, NULL,
				    mixer_stack,
				    EAI_OSAL_THREAD_STACK_SIZEOF(mixer_stack),
				    20); /* high priority */
	if (rc != EAI_OSAL_OK) {
		mixer.running = false;
		mixer.initialized = false;
		eai_osal_sem_destroy(&mixer.sem);
		eai_osal_mutex_destroy(&mixer.mutex);
		return -1;
	}

	return 0;
}

int eai_audio_mixer_deinit(void)
{
	if (!mixer.initialized) {
		return -1;
	}

	mixer.running = false;
	eai_osal_sem_give(&mixer.sem); /* wake thread so it exits */
	eai_osal_thread_join(&mixer.thread, 1000);

	eai_osal_sem_destroy(&mixer.sem);
	eai_osal_mutex_destroy(&mixer.mutex);

	mixer.initialized = false;
	return 0;
}

int eai_audio_mixer_slot_open(uint8_t *slot)
{
	if (!mixer.initialized || !slot) {
		return -1;
	}

	eai_osal_mutex_lock(&mixer.mutex, EAI_OSAL_WAIT_FOREVER);

	for (uint8_t i = 0; i < EAI_AUDIO_MIXER_MAX_SLOTS; i++) {
		if (!mixer.slots[i].active) {
			mixer.slots[i].active = true;
			mixer.slots[i].wr = 0;
			mixer.slots[i].rd = 0;
			mixer.slots[i].underruns = 0;
			mixer.slots[i].volume = EAI_AUDIO_MIXER_VOLUME_UNITY;
			*slot = i;
			eai_osal_mutex_unlock(&mixer.mutex);
			return 0;
		}
	}

	eai_osal_mutex_unlock(&mixer.mutex);
	return -12; /* ENOMEM */
}

int eai_audio_mixer_slot_close(uint8_t slot)
{
	if (!mixer.initialized || slot >= EAI_AUDIO_MIXER_MAX_SLOTS) {
		return -1;
	}

	eai_osal_mutex_lock(&mixer.mutex, EAI_OSAL_WAIT_FOREVER);
	mixer.slots[slot].active = false;
	mixer.slots[slot].wr = 0;
	mixer.slots[slot].rd = 0;
	eai_osal_mutex_unlock(&mixer.mutex);
	return 0;
}

int eai_audio_mixer_write(uint8_t slot, const int16_t *data, uint32_t frames)
{
	if (!mixer.initialized || !data || frames == 0) {
		return -1;
	}
	if (slot >= EAI_AUDIO_MIXER_MAX_SLOTS || !mixer.slots[slot].active) {
		return -1;
	}

	uint32_t samples = frames * mixer.config.channels;

	eai_osal_mutex_lock(&mixer.mutex, EAI_OSAL_WAIT_FOREVER);

	uint32_t space = ring_space(&mixer.slots[slot]);
	uint32_t to_write = samples < space ? samples : space;

	/* Round down to whole frames */
	to_write = (to_write / mixer.config.channels) * mixer.config.channels;

	if (to_write > 0) {
		ring_write(&mixer.slots[slot], data, to_write);
	}

	eai_osal_mutex_unlock(&mixer.mutex);

	/* Wake mixer thread */
	eai_osal_sem_give(&mixer.sem);

	return (int)(to_write / mixer.config.channels);
}

void eai_audio_mixer_kick(void)
{
	if (mixer.initialized) {
		eai_osal_sem_give(&mixer.sem);
	}
}

int eai_audio_mixer_set_volume(uint8_t slot, uint32_t volume_q16)
{
	if (!mixer.initialized || slot >= EAI_AUDIO_MIXER_MAX_SLOTS) {
		return -1;
	}

	eai_osal_mutex_lock(&mixer.mutex, EAI_OSAL_WAIT_FOREVER);
	mixer.slots[slot].volume = volume_q16;
	eai_osal_mutex_unlock(&mixer.mutex);
	return 0;
}

uint32_t eai_audio_mixer_get_underruns(uint8_t slot)
{
	if (!mixer.initialized || slot >= EAI_AUDIO_MIXER_MAX_SLOTS) {
		return 0;
	}
	return mixer.slots[slot].underruns;
}
