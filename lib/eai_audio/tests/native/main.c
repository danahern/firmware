/*
 * eai_audio POSIX stub tests — core API
 *
 * Verifies API contract using the POSIX stub backend:
 * init/deinit, port enumeration, stream lifecycle, I/O, gain, routes.
 */

#include "unity.h"
#include <eai_audio/eai_audio.h>
#include <errno.h>
#include <string.h>

/* Declared in mixer_tests.c when mixer is enabled */
#ifdef EAI_AUDIO_MIXER_TESTS
extern void run_mixer_tests(void);
#endif

void setUp(void)
{
	eai_audio_test_reset();
}

void tearDown(void)
{
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Init / Deinit
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_init_success(void)
{
	TEST_ASSERT_EQUAL(0, eai_audio_init());
}

static void test_deinit_success(void)
{
	eai_audio_init();
	TEST_ASSERT_EQUAL(0, eai_audio_deinit());
}

static void test_deinit_without_init(void)
{
	TEST_ASSERT_EQUAL(-EINVAL, eai_audio_deinit());
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Port enumeration
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_port_count(void)
{
	eai_audio_init();
	TEST_ASSERT_EQUAL(2, eai_audio_get_port_count());
}

static void test_get_port_speaker(void)
{
	eai_audio_init();
	struct eai_audio_port port;

	TEST_ASSERT_EQUAL(0, eai_audio_get_port(0, &port));
	TEST_ASSERT_EQUAL(0, port.id);
	TEST_ASSERT_EQUAL_STRING("speaker", port.name);
	TEST_ASSERT_EQUAL(EAI_AUDIO_OUTPUT, port.direction);
	TEST_ASSERT_EQUAL(EAI_AUDIO_PORT_SPEAKER, port.type);
	TEST_ASSERT_TRUE(port.has_gain);
}

static void test_get_port_mic(void)
{
	eai_audio_init();
	struct eai_audio_port port;

	TEST_ASSERT_EQUAL(0, eai_audio_get_port(1, &port));
	TEST_ASSERT_EQUAL(1, port.id);
	TEST_ASSERT_EQUAL_STRING("mic", port.name);
	TEST_ASSERT_EQUAL(EAI_AUDIO_INPUT, port.direction);
	TEST_ASSERT_EQUAL(EAI_AUDIO_PORT_MIC, port.type);
	TEST_ASSERT_FALSE(port.has_gain);
}

static void test_get_port_out_of_range(void)
{
	eai_audio_init();
	struct eai_audio_port port;

	TEST_ASSERT_EQUAL(-EINVAL, eai_audio_get_port(99, &port));
}

static void test_get_port_null(void)
{
	eai_audio_init();
	TEST_ASSERT_EQUAL(-EINVAL, eai_audio_get_port(0, NULL));
}

static void test_find_port_speaker(void)
{
	eai_audio_init();
	struct eai_audio_port port;

	TEST_ASSERT_EQUAL(0, eai_audio_find_port(
		EAI_AUDIO_PORT_SPEAKER, EAI_AUDIO_OUTPUT, &port));
	TEST_ASSERT_EQUAL_STRING("speaker", port.name);
}

static void test_find_port_not_found(void)
{
	eai_audio_init();
	struct eai_audio_port port;

	TEST_ASSERT_EQUAL(-ENODEV, eai_audio_find_port(
		EAI_AUDIO_PORT_BT_SCO, EAI_AUDIO_OUTPUT, &port));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Stream lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */

static const struct eai_audio_config test_config = {
	.sample_rate = 16000,
	.format = EAI_AUDIO_FORMAT_PCM_S16_LE,
	.channels = EAI_AUDIO_CHANNEL_MONO,
	.frame_count = 256,
};

static void test_stream_open_close(void)
{
	eai_audio_init();
	struct eai_audio_stream stream;

	TEST_ASSERT_EQUAL(0, eai_audio_stream_open(&stream, 0, &test_config));
	TEST_ASSERT_EQUAL(EAI_AUDIO_OUTPUT, stream.direction);
	TEST_ASSERT_EQUAL(0, stream.port_id);
	TEST_ASSERT_EQUAL(0, eai_audio_stream_close(&stream));
}

static void test_stream_open_invalid_port(void)
{
	eai_audio_init();
	struct eai_audio_stream stream;

	TEST_ASSERT_EQUAL(-ENODEV,
		eai_audio_stream_open(&stream, 99, &test_config));
}

static void test_stream_open_null(void)
{
	eai_audio_init();
	TEST_ASSERT_EQUAL(-EINVAL,
		eai_audio_stream_open(NULL, 0, &test_config));
}

static void test_stream_open_busy(void)
{
	eai_audio_init();
	struct eai_audio_stream s1, s2;

	TEST_ASSERT_EQUAL(0, eai_audio_stream_open(&s1, 0, &test_config));
	TEST_ASSERT_EQUAL(-EBUSY, eai_audio_stream_open(&s2, 0, &test_config));

	eai_audio_stream_close(&s1);
}

static void test_stream_reopen_after_close(void)
{
	eai_audio_init();
	struct eai_audio_stream s1, s2;

	eai_audio_stream_open(&s1, 0, &test_config);
	eai_audio_stream_close(&s1);
	TEST_ASSERT_EQUAL(0, eai_audio_stream_open(&s2, 0, &test_config));
	eai_audio_stream_close(&s2);
}

static void test_stream_start_pause(void)
{
	eai_audio_init();
	struct eai_audio_stream stream;

	eai_audio_stream_open(&stream, 0, &test_config);
	TEST_ASSERT_EQUAL(0, eai_audio_stream_start(&stream));
	TEST_ASSERT_EQUAL(0, eai_audio_stream_pause(&stream));
	eai_audio_stream_close(&stream);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Stream write
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_stream_write(void)
{
	eai_audio_init();
	struct eai_audio_stream stream;

	eai_audio_stream_open(&stream, 0, &test_config);
	eai_audio_stream_start(&stream);

	int16_t data[] = {100, 200, 300, 400};
	int ret = eai_audio_stream_write(&stream, data, 4, 0);

	TEST_ASSERT_EQUAL(4, ret);

	const int16_t *out;
	uint32_t frames;

	eai_audio_test_get_output(&out, &frames);
	TEST_ASSERT_EQUAL(4, frames);
	TEST_ASSERT_EQUAL(100, out[0]);
	TEST_ASSERT_EQUAL(200, out[1]);
	TEST_ASSERT_EQUAL(300, out[2]);
	TEST_ASSERT_EQUAL(400, out[3]);

	eai_audio_stream_close(&stream);
}

static void test_stream_write_not_started(void)
{
	eai_audio_init();
	struct eai_audio_stream stream;

	eai_audio_stream_open(&stream, 0, &test_config);

	int16_t data[] = {100};

	TEST_ASSERT_EQUAL(-EINVAL,
		eai_audio_stream_write(&stream, data, 1, 0));

	eai_audio_stream_close(&stream);
}

static void test_stream_write_input_stream(void)
{
	eai_audio_init();
	struct eai_audio_stream stream;

	eai_audio_stream_open(&stream, 1, &test_config); /* mic = input */
	eai_audio_stream_start(&stream);

	int16_t data[] = {100};

	TEST_ASSERT_EQUAL(-ENOTSUP,
		eai_audio_stream_write(&stream, data, 1, 0));

	eai_audio_stream_close(&stream);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Stream read
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_stream_read(void)
{
	eai_audio_init();

	/* Pre-load input data */
	int16_t input[] = {500, 600, 700};

	eai_audio_test_set_input(input, 3);

	struct eai_audio_stream stream;

	eai_audio_stream_open(&stream, 1, &test_config); /* mic */
	eai_audio_stream_start(&stream);

	int16_t buf[3] = {0};
	int ret = eai_audio_stream_read(&stream, buf, 3, 0);

	TEST_ASSERT_EQUAL(3, ret);
	TEST_ASSERT_EQUAL(500, buf[0]);
	TEST_ASSERT_EQUAL(600, buf[1]);
	TEST_ASSERT_EQUAL(700, buf[2]);

	eai_audio_stream_close(&stream);
}

static void test_stream_read_output_stream(void)
{
	eai_audio_init();
	struct eai_audio_stream stream;

	eai_audio_stream_open(&stream, 0, &test_config); /* speaker = output */
	eai_audio_stream_start(&stream);

	int16_t buf[1];

	TEST_ASSERT_EQUAL(-ENOTSUP,
		eai_audio_stream_read(&stream, buf, 1, 0));

	eai_audio_stream_close(&stream);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Stream position
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_stream_position(void)
{
	eai_audio_init();
	struct eai_audio_stream stream;

	eai_audio_stream_open(&stream, 0, &test_config);
	eai_audio_stream_start(&stream);

	int16_t data[10];

	memset(data, 0, sizeof(data));
	eai_audio_stream_write(&stream, data, 10, 0);

	uint64_t pos;

	TEST_ASSERT_EQUAL(0, eai_audio_stream_get_position(&stream, &pos));
	TEST_ASSERT_EQUAL(10, pos);

	eai_audio_stream_close(&stream);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Gain control
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_gain_set_get(void)
{
	eai_audio_init();

	TEST_ASSERT_EQUAL(0, eai_audio_set_gain(0, -2000));

	int32_t gain;

	TEST_ASSERT_EQUAL(0, eai_audio_get_gain(0, &gain));
	TEST_ASSERT_EQUAL(-2000, gain);
}

static void test_gain_clamp(void)
{
	eai_audio_init();

	/* Speaker gain range: -6000 to 0 */
	eai_audio_set_gain(0, -9999);

	int32_t gain;

	eai_audio_get_gain(0, &gain);
	TEST_ASSERT_EQUAL(-6000, gain); /* clamped to min */

	eai_audio_set_gain(0, 500);
	eai_audio_get_gain(0, &gain);
	TEST_ASSERT_EQUAL(0, gain); /* clamped to max */
}

static void test_gain_no_gain_port(void)
{
	eai_audio_init();

	/* Mic (port 1) has no gain */
	TEST_ASSERT_EQUAL(-ENOTSUP, eai_audio_set_gain(1, 0));

	int32_t gain;

	TEST_ASSERT_EQUAL(-ENOTSUP, eai_audio_get_gain(1, &gain));
}

static void test_gain_invalid_port(void)
{
	eai_audio_init();
	TEST_ASSERT_EQUAL(-EINVAL, eai_audio_set_gain(99, 0));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Routing
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_route_set(void)
{
	eai_audio_init();

	/* Route mic (input, id=1) -> speaker (output, id=0) */
	TEST_ASSERT_EQUAL(0, eai_audio_set_route(1, 0));
	TEST_ASSERT_EQUAL(1, eai_audio_get_route_count());

	struct eai_audio_route route;

	TEST_ASSERT_EQUAL(0, eai_audio_get_route(0, &route));
	TEST_ASSERT_EQUAL(1, route.source_port_id);
	TEST_ASSERT_EQUAL(0, route.sink_port_id);
	TEST_ASSERT_TRUE(route.active);
}

static void test_route_invalid_direction(void)
{
	eai_audio_init();

	/* speaker (output) -> mic (input) — wrong direction */
	TEST_ASSERT_EQUAL(-EINVAL, eai_audio_set_route(0, 1));
}

static void test_route_duplicate(void)
{
	eai_audio_init();

	eai_audio_set_route(1, 0);
	eai_audio_set_route(1, 0); /* should reactivate, not duplicate */
	TEST_ASSERT_EQUAL(1, eai_audio_get_route_count());
}

static void test_route_out_of_range(void)
{
	eai_audio_init();

	struct eai_audio_route route;

	TEST_ASSERT_EQUAL(-EINVAL, eai_audio_get_route(99, &route));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Port profiles
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_port_profile(void)
{
	eai_audio_init();
	struct eai_audio_port port;

	eai_audio_get_port(0, &port);

	TEST_ASSERT_EQUAL(1, port.profile_count);
	TEST_ASSERT_EQUAL(1, port.profiles[0].format_count);
	TEST_ASSERT_EQUAL(EAI_AUDIO_FORMAT_PCM_S16_LE,
			  port.profiles[0].formats[0]);
	TEST_ASSERT_EQUAL(2, port.profiles[0].sample_rate_count);
	TEST_ASSERT_EQUAL(16000, port.profiles[0].sample_rates[0]);
	TEST_ASSERT_EQUAL(48000, port.profiles[0].sample_rates[1]);
}

/* ═══════════════════════════════════════════════════════════════════════════ */

int main(void)
{
	UNITY_BEGIN();

	/* Init/deinit */
	RUN_TEST(test_init_success);
	RUN_TEST(test_deinit_success);
	RUN_TEST(test_deinit_without_init);

	/* Port enumeration */
	RUN_TEST(test_port_count);
	RUN_TEST(test_get_port_speaker);
	RUN_TEST(test_get_port_mic);
	RUN_TEST(test_get_port_out_of_range);
	RUN_TEST(test_get_port_null);
	RUN_TEST(test_find_port_speaker);
	RUN_TEST(test_find_port_not_found);

	/* Stream lifecycle */
	RUN_TEST(test_stream_open_close);
	RUN_TEST(test_stream_open_invalid_port);
	RUN_TEST(test_stream_open_null);
	RUN_TEST(test_stream_open_busy);
	RUN_TEST(test_stream_reopen_after_close);
	RUN_TEST(test_stream_start_pause);

	/* Stream I/O */
	RUN_TEST(test_stream_write);
	RUN_TEST(test_stream_write_not_started);
	RUN_TEST(test_stream_write_input_stream);
	RUN_TEST(test_stream_read);
	RUN_TEST(test_stream_read_output_stream);
	RUN_TEST(test_stream_position);

	/* Gain */
	RUN_TEST(test_gain_set_get);
	RUN_TEST(test_gain_clamp);
	RUN_TEST(test_gain_no_gain_port);
	RUN_TEST(test_gain_invalid_port);

	/* Routes */
	RUN_TEST(test_route_set);
	RUN_TEST(test_route_invalid_direction);
	RUN_TEST(test_route_duplicate);
	RUN_TEST(test_route_out_of_range);

	/* Port profiles */
	RUN_TEST(test_port_profile);

#ifdef EAI_AUDIO_MIXER_TESTS
	run_mixer_tests();
#endif

	return UNITY_END();
}
