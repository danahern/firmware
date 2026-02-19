/*
 * eai_audio POSIX stub backend
 *
 * Provides fake ports and buffer I/O for native testing.
 * No actual audio hardware interaction.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <eai_audio/eai_audio.h>
#include <errno.h>
#include <string.h>

/* ── Configuration defaults ─────────────────────────────────────────────── */

#ifndef CONFIG_EAI_AUDIO_MAX_PORTS
#define CONFIG_EAI_AUDIO_MAX_PORTS 4
#endif

#ifndef CONFIG_EAI_AUDIO_MAX_ROUTES
#define CONFIG_EAI_AUDIO_MAX_ROUTES 4
#endif

/* ── Test buffer sizes ──────────────────────────────────────────────────── */

/* Max frames for test I/O buffers (stereo S16 = 4 bytes/frame) */
#define TEST_BUF_MAX_FRAMES 4096
#define TEST_BUF_MAX_SAMPLES (TEST_BUF_MAX_FRAMES * 2) /* stereo */

/* ── Module state ───────────────────────────────────────────────────────── */

static bool initialized;

/* Port table */
static struct eai_audio_port ports[CONFIG_EAI_AUDIO_MAX_PORTS];
static uint8_t port_count;

/* Route table */
static struct eai_audio_route routes[CONFIG_EAI_AUDIO_MAX_ROUTES];
static uint8_t route_count;

/* Test I/O buffers */
static int16_t output_buf[TEST_BUF_MAX_SAMPLES];
static uint32_t output_frames;

static int16_t input_buf[TEST_BUF_MAX_SAMPLES];
static uint32_t input_frames;
static uint32_t input_read_pos;

/* Track open streams for single-stream-per-port enforcement */
static bool port_has_stream[CONFIG_EAI_AUDIO_MAX_PORTS];

/* ── Helper: bytes per frame ────────────────────────────────────────────── */

static uint32_t channels_from_mask(enum eai_audio_channel_mask mask)
{
	uint32_t count = 0;
	uint32_t m = (uint32_t)mask;

	while (m) {
		count += m & 1;
		m >>= 1;
	}
	return count;
}

static uint32_t bytes_per_sample(enum eai_audio_format fmt)
{
	switch (fmt) {
	case EAI_AUDIO_FORMAT_PCM_S16_LE: return 2;
	case EAI_AUDIO_FORMAT_PCM_S24_LE: return 3;
	case EAI_AUDIO_FORMAT_PCM_S32_LE: return 4;
	case EAI_AUDIO_FORMAT_PCM_F32_LE: return 4;
	default: return 2;
	}
}

static uint32_t frame_size(const struct eai_audio_config *config)
{
	return bytes_per_sample(config->format) *
	       channels_from_mask(config->channels);
}

/* ── Helper: get posix stream data from opaque backend ──────────────────── */

static struct eai_audio_posix_stream *stream_backend(struct eai_audio_stream *s)
{
	return (struct eai_audio_posix_stream *)s->_backend;
}

/* ── Helper: find port by ID ────────────────────────────────────────────── */

static struct eai_audio_port *find_port_by_id(uint8_t id)
{
	for (uint8_t i = 0; i < port_count; i++) {
		if (ports[i].id == id) {
			return &ports[i];
		}
	}
	return NULL;
}

/* ── Default port setup ─────────────────────────────────────────────────── */

static void setup_default_ports(void)
{
	port_count = 2;

	/* Port 0: Speaker (output) */
	memset(&ports[0], 0, sizeof(ports[0]));
	ports[0].id = 0;
	strncpy(ports[0].name, "speaker", EAI_AUDIO_PORT_NAME_MAX - 1);
	ports[0].direction = EAI_AUDIO_OUTPUT;
	ports[0].type = EAI_AUDIO_PORT_SPEAKER;
	ports[0].profile_count = 1;
	ports[0].profiles[0].format_count = 1;
	ports[0].profiles[0].formats[0] = EAI_AUDIO_FORMAT_PCM_S16_LE;
	ports[0].profiles[0].sample_rate_count = 2;
	ports[0].profiles[0].sample_rates[0] = 16000;
	ports[0].profiles[0].sample_rates[1] = 48000;
	ports[0].profiles[0].channel_mask_count = 2;
	ports[0].profiles[0].channels[0] = EAI_AUDIO_CHANNEL_MONO;
	ports[0].profiles[0].channels[1] = EAI_AUDIO_CHANNEL_STEREO;
	ports[0].has_gain = true;
	ports[0].gain.min_cb = -6000; /* -60 dB */
	ports[0].gain.max_cb = 0;
	ports[0].gain.step_cb = 100;  /* 1 dB steps */
	ports[0].gain.current_cb = 0;

	/* Port 1: Mic (input) */
	memset(&ports[1], 0, sizeof(ports[1]));
	ports[1].id = 1;
	strncpy(ports[1].name, "mic", EAI_AUDIO_PORT_NAME_MAX - 1);
	ports[1].direction = EAI_AUDIO_INPUT;
	ports[1].type = EAI_AUDIO_PORT_MIC;
	ports[1].profile_count = 1;
	ports[1].profiles[0].format_count = 1;
	ports[1].profiles[0].formats[0] = EAI_AUDIO_FORMAT_PCM_S16_LE;
	ports[1].profiles[0].sample_rate_count = 1;
	ports[1].profiles[0].sample_rates[0] = 16000;
	ports[1].profiles[0].channel_mask_count = 1;
	ports[1].profiles[0].channels[0] = EAI_AUDIO_CHANNEL_MONO;
	ports[1].has_gain = false;
}

/* ── Module lifecycle ───────────────────────────────────────────────────── */

int eai_audio_init(void)
{
	memset(port_has_stream, 0, sizeof(port_has_stream));
	memset(output_buf, 0, sizeof(output_buf));
	output_frames = 0;
	memset(input_buf, 0, sizeof(input_buf));
	input_frames = 0;
	input_read_pos = 0;
	route_count = 0;

	setup_default_ports();
	initialized = true;
	return 0;
}

int eai_audio_deinit(void)
{
	if (!initialized) {
		return -EINVAL;
	}

	memset(port_has_stream, 0, sizeof(port_has_stream));
	initialized = false;
	return 0;
}

/* ── Port enumeration ───────────────────────────────────────────────────── */

int eai_audio_get_port_count(void)
{
	if (!initialized) {
		return -EINVAL;
	}
	return (int)port_count;
}

int eai_audio_get_port(uint8_t index, struct eai_audio_port *port)
{
	if (!initialized || !port) {
		return -EINVAL;
	}
	if (index >= port_count) {
		return -EINVAL;
	}

	*port = ports[index];
	return 0;
}

int eai_audio_find_port(enum eai_audio_port_type type,
			enum eai_audio_direction dir,
			struct eai_audio_port *port)
{
	if (!initialized || !port) {
		return -EINVAL;
	}

	for (uint8_t i = 0; i < port_count; i++) {
		if (ports[i].type == type && ports[i].direction == dir) {
			*port = ports[i];
			return 0;
		}
	}

	return -ENODEV;
}

/* ── Stream lifecycle ───────────────────────────────────────────────────── */

int eai_audio_stream_open(struct eai_audio_stream *stream, uint8_t port_id,
			  const struct eai_audio_config *config)
{
	if (!initialized || !stream || !config) {
		return -EINVAL;
	}

	struct eai_audio_port *port = find_port_by_id(port_id);

	if (!port) {
		return -ENODEV;
	}

	if (port_has_stream[port_id]) {
		return -EBUSY;
	}

	memset(stream, 0, sizeof(*stream));
	stream->config = *config;
	stream->direction = port->direction;
	stream->port_id = port_id;
	stream->mixer_slot = EAI_AUDIO_MIXER_SLOT_NONE;

	struct eai_audio_posix_stream *ps = stream_backend(stream);

	ps->frame_position = 0;
	ps->active = false;

	port_has_stream[port_id] = true;
	return 0;
}

int eai_audio_stream_close(struct eai_audio_stream *stream)
{
	if (!initialized || !stream) {
		return -EINVAL;
	}

	if (stream->port_id < CONFIG_EAI_AUDIO_MAX_PORTS) {
		port_has_stream[stream->port_id] = false;
	}

	struct eai_audio_posix_stream *ps = stream_backend(stream);

	ps->active = false;
	return 0;
}

int eai_audio_stream_start(struct eai_audio_stream *stream)
{
	if (!initialized || !stream) {
		return -EINVAL;
	}

	struct eai_audio_posix_stream *ps = stream_backend(stream);

	ps->active = true;
	return 0;
}

int eai_audio_stream_pause(struct eai_audio_stream *stream)
{
	if (!initialized || !stream) {
		return -EINVAL;
	}

	struct eai_audio_posix_stream *ps = stream_backend(stream);

	ps->active = false;
	return 0;
}

int eai_audio_stream_write(struct eai_audio_stream *stream,
			   const void *data, uint32_t frames,
			   uint32_t timeout_ms)
{
	(void)timeout_ms;

	if (!initialized || !stream || !data || frames == 0) {
		return -EINVAL;
	}
	if (stream->direction != EAI_AUDIO_OUTPUT) {
		return -ENOTSUP;
	}

	struct eai_audio_posix_stream *ps = stream_backend(stream);

	if (!ps->active) {
		return -EINVAL;
	}

	/* Copy to test output buffer */
	uint32_t fsize = frame_size(&stream->config);
	uint32_t samples_per_frame = channels_from_mask(stream->config.channels);
	uint32_t avail = TEST_BUF_MAX_FRAMES - output_frames;
	uint32_t to_write = frames < avail ? frames : avail;

	if (to_write > 0) {
		memcpy(&output_buf[output_frames * samples_per_frame],
		       data, to_write * fsize);
		output_frames += to_write;
	}

	ps->frame_position += to_write;
	return (int)to_write;
}

int eai_audio_stream_read(struct eai_audio_stream *stream,
			  void *data, uint32_t frames,
			  uint32_t timeout_ms)
{
	(void)timeout_ms;

	if (!initialized || !stream || !data || frames == 0) {
		return -EINVAL;
	}
	if (stream->direction != EAI_AUDIO_INPUT) {
		return -ENOTSUP;
	}

	struct eai_audio_posix_stream *ps = stream_backend(stream);

	if (!ps->active) {
		return -EINVAL;
	}

	/* Copy from test input buffer */
	uint32_t fsize = frame_size(&stream->config);
	uint32_t samples_per_frame = channels_from_mask(stream->config.channels);
	uint32_t avail = input_frames - input_read_pos;
	uint32_t to_read = frames < avail ? frames : avail;

	if (to_read > 0) {
		memcpy(data,
		       &input_buf[input_read_pos * samples_per_frame],
		       to_read * fsize);
		input_read_pos += to_read;
	}

	ps->frame_position += to_read;
	return (int)to_read;
}

int eai_audio_stream_get_position(struct eai_audio_stream *stream,
				  uint64_t *frames)
{
	if (!initialized || !stream || !frames) {
		return -EINVAL;
	}

	struct eai_audio_posix_stream *ps = stream_backend(stream);

	*frames = ps->frame_position;
	return 0;
}

/* ── Gain control ───────────────────────────────────────────────────────── */

int eai_audio_set_gain(uint8_t port_id, int32_t gain_cb)
{
	if (!initialized) {
		return -EINVAL;
	}

	struct eai_audio_port *port = find_port_by_id(port_id);

	if (!port) {
		return -EINVAL;
	}
	if (!port->has_gain) {
		return -ENOTSUP;
	}

	/* Clamp to valid range */
	if (gain_cb < port->gain.min_cb) {
		gain_cb = port->gain.min_cb;
	}
	if (gain_cb > port->gain.max_cb) {
		gain_cb = port->gain.max_cb;
	}

	port->gain.current_cb = gain_cb;
	return 0;
}

int eai_audio_get_gain(uint8_t port_id, int32_t *gain_cb)
{
	if (!initialized || !gain_cb) {
		return -EINVAL;
	}

	struct eai_audio_port *port = find_port_by_id(port_id);

	if (!port) {
		return -EINVAL;
	}
	if (!port->has_gain) {
		return -ENOTSUP;
	}

	*gain_cb = port->gain.current_cb;
	return 0;
}

/* ── Routing ────────────────────────────────────────────────────────────── */

int eai_audio_set_route(uint8_t source_port_id, uint8_t sink_port_id)
{
	if (!initialized) {
		return -EINVAL;
	}

	struct eai_audio_port *src = find_port_by_id(source_port_id);
	struct eai_audio_port *sink = find_port_by_id(sink_port_id);

	if (!src || !sink) {
		return -EINVAL;
	}
	if (src->direction != EAI_AUDIO_INPUT ||
	    sink->direction != EAI_AUDIO_OUTPUT) {
		return -EINVAL;
	}

	/* Check for existing route with same endpoints */
	for (uint8_t i = 0; i < route_count; i++) {
		if (routes[i].source_port_id == source_port_id &&
		    routes[i].sink_port_id == sink_port_id) {
			routes[i].active = true;
			return 0;
		}
	}

	if (route_count >= CONFIG_EAI_AUDIO_MAX_ROUTES) {
		return -ENOMEM;
	}

	routes[route_count].source_port_id = source_port_id;
	routes[route_count].sink_port_id = sink_port_id;
	routes[route_count].active = true;
	route_count++;
	return 0;
}

int eai_audio_get_route_count(void)
{
	if (!initialized) {
		return -EINVAL;
	}
	return (int)route_count;
}

int eai_audio_get_route(uint8_t index, struct eai_audio_route *route)
{
	if (!initialized || !route) {
		return -EINVAL;
	}
	if (index >= route_count) {
		return -EINVAL;
	}

	*route = routes[index];
	return 0;
}

/* ── Test helpers ───────────────────────────────────────────────────────── */

void eai_audio_test_get_output(const int16_t **buf, uint32_t *frames)
{
	if (buf) {
		*buf = output_buf;
	}
	if (frames) {
		*frames = output_frames;
	}
}

void eai_audio_test_set_input(const int16_t *data, uint32_t frames)
{
	uint32_t to_copy = frames < TEST_BUF_MAX_FRAMES ? frames : TEST_BUF_MAX_FRAMES;

	if (data && to_copy > 0) {
		memcpy(input_buf, data, to_copy * sizeof(int16_t));
	}
	input_frames = to_copy;
	input_read_pos = 0;
}

void eai_audio_test_reset(void)
{
	initialized = false;
	port_count = 0;
	route_count = 0;
	memset(port_has_stream, 0, sizeof(port_has_stream));
	memset(output_buf, 0, sizeof(output_buf));
	output_frames = 0;
	memset(input_buf, 0, sizeof(input_buf));
	input_frames = 0;
	input_read_pos = 0;
}
