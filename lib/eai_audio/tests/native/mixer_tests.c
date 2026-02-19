/*
 * eai_audio mixer tests
 *
 * Tests the mini-flinger in isolation using a test hw_write callback.
 * Requires eai_osal POSIX backend.
 */

#include "unity.h"
#include "mixer.h"
#include <eai_osal/eai_osal.h>
#include <string.h>

/* ── Test hw_write callback ─────────────────────────────────────────────── */

#define HW_BUF_MAX_SAMPLES 4096

static int16_t hw_output[HW_BUF_MAX_SAMPLES];
static uint32_t hw_output_frames;
static int hw_write_count;

static int test_hw_write(const void *buf, uint32_t frames)
{
	uint32_t samples = frames; /* mono for most tests */

	/* Detect channel count from mixer config (not available here,
	 * so we trust the caller set up mono) */
	if (samples <= HW_BUF_MAX_SAMPLES - hw_output_frames) {
		memcpy(&hw_output[hw_output_frames], buf,
		       samples * sizeof(int16_t));
		hw_output_frames += samples;
	}
	hw_write_count++;
	return 0;
}

static void reset_hw_output(void)
{
	memset(hw_output, 0, sizeof(hw_output));
	hw_output_frames = 0;
	hw_write_count = 0;
}

/* ── Test helpers ───────────────────────────────────────────────────────── */

static const struct eai_audio_mixer_config mono_config = {
	.sample_rate = 16000,
	.channels = 1,
	.period_frames = 64,
	.hw_write = test_hw_write,
};

/* ── Tests ──────────────────────────────────────────────────────────────── */

static void test_mixer_init_deinit(void)
{
	reset_hw_output();
	TEST_ASSERT_EQUAL(0, eai_audio_mixer_init(&mono_config));
	TEST_ASSERT_EQUAL(0, eai_audio_mixer_deinit());
}

static void test_mixer_init_null(void)
{
	TEST_ASSERT_NOT_EQUAL(0, eai_audio_mixer_init(NULL));
}

static void test_mixer_init_bad_period(void)
{
	struct eai_audio_mixer_config bad = mono_config;

	bad.period_frames = 0;
	TEST_ASSERT_NOT_EQUAL(0, eai_audio_mixer_init(&bad));

	bad.period_frames = EAI_AUDIO_MIXER_MAX_PERIOD_FRAMES + 1;
	TEST_ASSERT_NOT_EQUAL(0, eai_audio_mixer_init(&bad));
}

static void test_mixer_slot_open_close(void)
{
	reset_hw_output();
	eai_audio_mixer_init(&mono_config);

	uint8_t slot;

	TEST_ASSERT_EQUAL(0, eai_audio_mixer_slot_open(&slot));
	TEST_ASSERT_EQUAL(0, slot);
	TEST_ASSERT_EQUAL(0, eai_audio_mixer_slot_close(slot));

	eai_audio_mixer_deinit();
}

static void test_mixer_slot_exhaustion(void)
{
	reset_hw_output();
	eai_audio_mixer_init(&mono_config);

	uint8_t slots[EAI_AUDIO_MIXER_MAX_SLOTS];

	for (int i = 0; i < EAI_AUDIO_MIXER_MAX_SLOTS; i++) {
		TEST_ASSERT_EQUAL(0, eai_audio_mixer_slot_open(&slots[i]));
	}

	uint8_t extra;

	TEST_ASSERT_NOT_EQUAL(0, eai_audio_mixer_slot_open(&extra));

	for (int i = 0; i < EAI_AUDIO_MIXER_MAX_SLOTS; i++) {
		eai_audio_mixer_slot_close(slots[i]);
	}

	eai_audio_mixer_deinit();
}

static void test_mixer_single_stream(void)
{
	reset_hw_output();
	eai_audio_mixer_init(&mono_config);

	uint8_t slot;

	eai_audio_mixer_slot_open(&slot);

	/* Write one period of data */
	int16_t data[64];

	for (int i = 0; i < 64; i++) {
		data[i] = (int16_t)(i * 100);
	}

	int written = eai_audio_mixer_write(slot, data, 64);

	TEST_ASSERT_EQUAL(64, written);

	/* Kick mixer and wait for it to process */
	eai_audio_mixer_kick();
	eai_osal_thread_sleep(50);

	/* Verify hw_write was called with our data (at unity volume) */
	TEST_ASSERT_GREATER_THAN(0, hw_write_count);
	TEST_ASSERT_GREATER_OR_EQUAL(64, hw_output_frames);

	/* Verify first few samples match input */
	TEST_ASSERT_EQUAL(0, hw_output[0]);
	TEST_ASSERT_EQUAL(100, hw_output[1]);
	TEST_ASSERT_EQUAL(200, hw_output[2]);

	eai_audio_mixer_slot_close(slot);
	eai_audio_mixer_deinit();
}

static void test_mixer_two_streams(void)
{
	reset_hw_output();
	eai_audio_mixer_init(&mono_config);

	uint8_t slot_a, slot_b;

	eai_audio_mixer_slot_open(&slot_a);
	eai_audio_mixer_slot_open(&slot_b);

	/* Write complementary data to two slots */
	int16_t data_a[64], data_b[64];

	for (int i = 0; i < 64; i++) {
		data_a[i] = 1000;
		data_b[i] = 2000;
	}

	eai_audio_mixer_write(slot_a, data_a, 64);
	eai_audio_mixer_write(slot_b, data_b, 64);

	eai_audio_mixer_kick();
	eai_osal_thread_sleep(50);

	/* Mixed output should be 1000 + 2000 = 3000 */
	TEST_ASSERT_GREATER_THAN(0, hw_write_count);

	/* Check mixed values */
	for (uint32_t i = 0; i < 64 && i < hw_output_frames; i++) {
		TEST_ASSERT_EQUAL(3000, hw_output[i]);
	}

	eai_audio_mixer_slot_close(slot_a);
	eai_audio_mixer_slot_close(slot_b);
	eai_audio_mixer_deinit();
}

static void test_mixer_clipping(void)
{
	reset_hw_output();
	eai_audio_mixer_init(&mono_config);

	uint8_t slot_a, slot_b;

	eai_audio_mixer_slot_open(&slot_a);
	eai_audio_mixer_slot_open(&slot_b);

	/* Both at near-max: should clip to 32767 */
	int16_t data_a[64], data_b[64];

	for (int i = 0; i < 64; i++) {
		data_a[i] = 20000;
		data_b[i] = 20000;
	}

	eai_audio_mixer_write(slot_a, data_a, 64);
	eai_audio_mixer_write(slot_b, data_b, 64);

	eai_audio_mixer_kick();
	eai_osal_thread_sleep(50);

	/* 20000 + 20000 = 40000 -> clipped to 32767 */
	TEST_ASSERT_GREATER_THAN(0, hw_write_count);
	for (uint32_t i = 0; i < 64 && i < hw_output_frames; i++) {
		TEST_ASSERT_EQUAL(32767, hw_output[i]);
	}

	eai_audio_mixer_slot_close(slot_a);
	eai_audio_mixer_slot_close(slot_b);
	eai_audio_mixer_deinit();
}

static void test_mixer_negative_clipping(void)
{
	reset_hw_output();
	eai_audio_mixer_init(&mono_config);

	uint8_t slot_a, slot_b;

	eai_audio_mixer_slot_open(&slot_a);
	eai_audio_mixer_slot_open(&slot_b);

	int16_t data_a[64], data_b[64];

	for (int i = 0; i < 64; i++) {
		data_a[i] = -20000;
		data_b[i] = -20000;
	}

	eai_audio_mixer_write(slot_a, data_a, 64);
	eai_audio_mixer_write(slot_b, data_b, 64);

	eai_audio_mixer_kick();
	eai_osal_thread_sleep(50);

	/* -20000 + -20000 = -40000 -> clipped to -32768 */
	TEST_ASSERT_GREATER_THAN(0, hw_write_count);
	for (uint32_t i = 0; i < 64 && i < hw_output_frames; i++) {
		TEST_ASSERT_EQUAL(-32768, hw_output[i]);
	}

	eai_audio_mixer_slot_close(slot_a);
	eai_audio_mixer_slot_close(slot_b);
	eai_audio_mixer_deinit();
}

static void test_mixer_volume(void)
{
	reset_hw_output();
	eai_audio_mixer_init(&mono_config);

	uint8_t slot;

	eai_audio_mixer_slot_open(&slot);

	/* Set volume to 50% (0x8000 = 0.5 in Q16) */
	eai_audio_mixer_set_volume(slot, 0x8000);

	int16_t data[64];

	for (int i = 0; i < 64; i++) {
		data[i] = 10000;
	}

	eai_audio_mixer_write(slot, data, 64);
	eai_audio_mixer_kick();
	eai_osal_thread_sleep(50);

	/* 10000 * 0.5 = 5000 */
	TEST_ASSERT_GREATER_THAN(0, hw_write_count);
	for (uint32_t i = 0; i < 64 && i < hw_output_frames; i++) {
		TEST_ASSERT_INT_WITHIN(1, 5000, hw_output[i]);
	}

	eai_audio_mixer_slot_close(slot);
	eai_audio_mixer_deinit();
}

static void test_mixer_mute(void)
{
	reset_hw_output();
	eai_audio_mixer_init(&mono_config);

	uint8_t slot;

	eai_audio_mixer_slot_open(&slot);
	eai_audio_mixer_set_volume(slot, EAI_AUDIO_MIXER_VOLUME_MUTE);

	int16_t data[64];

	for (int i = 0; i < 64; i++) {
		data[i] = 10000;
	}

	eai_audio_mixer_write(slot, data, 64);
	eai_audio_mixer_kick();
	eai_osal_thread_sleep(50);

	/* Muted: output should be silence */
	TEST_ASSERT_GREATER_THAN(0, hw_write_count);
	for (uint32_t i = 0; i < 64 && i < hw_output_frames; i++) {
		TEST_ASSERT_EQUAL(0, hw_output[i]);
	}

	eai_audio_mixer_slot_close(slot);
	eai_audio_mixer_deinit();
}

static void test_mixer_underrun(void)
{
	reset_hw_output();
	eai_audio_mixer_init(&mono_config);

	uint8_t slot;

	eai_audio_mixer_slot_open(&slot);

	/* Write only 10 frames but period is 64 */
	int16_t data[10];

	for (int i = 0; i < 10; i++) {
		data[i] = 1000;
	}

	eai_audio_mixer_write(slot, data, 10);
	eai_audio_mixer_kick();
	eai_osal_thread_sleep(50);

	/* Should have underrun */
	TEST_ASSERT_GREATER_THAN(0, eai_audio_mixer_get_underruns(slot));

	eai_audio_mixer_slot_close(slot);
	eai_audio_mixer_deinit();
}

/* ── Runner ─────────────────────────────────────────────────────────────── */

void run_mixer_tests(void)
{
	RUN_TEST(test_mixer_init_deinit);
	RUN_TEST(test_mixer_init_null);
	RUN_TEST(test_mixer_init_bad_period);
	RUN_TEST(test_mixer_slot_open_close);
	RUN_TEST(test_mixer_slot_exhaustion);
	RUN_TEST(test_mixer_single_stream);
	RUN_TEST(test_mixer_two_streams);
	RUN_TEST(test_mixer_clipping);
	RUN_TEST(test_mixer_negative_clipping);
	RUN_TEST(test_mixer_volume);
	RUN_TEST(test_mixer_mute);
	RUN_TEST(test_mixer_underrun);
}
