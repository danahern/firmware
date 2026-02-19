/*
 * eai_sensor POSIX stub backend
 *
 * Provides fake sensors and injectable data for native testing.
 * No actual sensor hardware interaction.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <eai_sensor/eai_sensor.h>
#include <errno.h>
#include <string.h>

/* ── Configuration defaults ─────────────────────────────────────────────── */

#ifndef CONFIG_EAI_SENSOR_MAX_DEVICES
#define CONFIG_EAI_SENSOR_MAX_DEVICES 8
#endif

#ifndef CONFIG_EAI_SENSOR_MAX_SESSIONS
#define CONFIG_EAI_SENSOR_MAX_SESSIONS 4
#endif

/* ── Test data buffer ──────────────────────────────────────────────────── */

#define TEST_DATA_MAX 64

/* ── Module state ───────────────────────────────────────────────────────── */

static bool initialized;

/* Device table */
static struct eai_sensor_device devices[CONFIG_EAI_SENSOR_MAX_DEVICES];
static uint8_t device_count;

/* Track open sessions per device */
static bool device_has_session[CONFIG_EAI_SENSOR_MAX_DEVICES];

/* Injected test data ring buffer */
static struct eai_sensor_data test_data[TEST_DATA_MAX];
static uint32_t test_data_head; /* next write position */
static uint32_t test_data_tail; /* next read position */
static uint32_t test_data_count;

/* ── Helper: get posix session data from opaque backend ─────────────────── */

static struct eai_sensor_posix_session *session_backend(
	struct eai_sensor_session *s)
{
	return (struct eai_sensor_posix_session *)s->_backend;
}

/* ── Helper: find device by ID ──────────────────────────────────────────── */

static struct eai_sensor_device *find_device_by_id(uint8_t id)
{
	for (uint8_t i = 0; i < device_count; i++) {
		if (devices[i].id == id) {
			return &devices[i];
		}
	}
	return NULL;
}

/* ── Default device setup ───────────────────────────────────────────────── */

static void setup_default_devices(void)
{
	device_count = 2;

	/* Device 0: Accelerometer */
	memset(&devices[0], 0, sizeof(devices[0]));
	devices[0].id = 0;
	strncpy(devices[0].name, "accel", EAI_SENSOR_NAME_MAX - 1);
	devices[0].type = EAI_SENSOR_TYPE_ACCEL;
	devices[0].range_min = -16000; /* -16g in mg */
	devices[0].range_max = 16000;  /* +16g in mg */
	devices[0].resolution = 1;     /* 1 mg per LSB */
	devices[0].max_rate_hz = 400;

	/* Device 1: Temperature */
	memset(&devices[1], 0, sizeof(devices[1]));
	devices[1].id = 1;
	strncpy(devices[1].name, "temp", EAI_SENSOR_NAME_MAX - 1);
	devices[1].type = EAI_SENSOR_TYPE_TEMPERATURE;
	devices[1].range_min = -40000;  /* -40°C in m°C */
	devices[1].range_max = 125000;  /* +125°C in m°C */
	devices[1].resolution = 10;     /* 10 m°C per LSB */
	devices[1].max_rate_hz = 10;
}

/* ── Module lifecycle ───────────────────────────────────────────────────── */

int eai_sensor_init(void)
{
	memset(device_has_session, 0, sizeof(device_has_session));
	test_data_head = 0;
	test_data_tail = 0;
	test_data_count = 0;

	setup_default_devices();
	initialized = true;
	return 0;
}

int eai_sensor_deinit(void)
{
	if (!initialized) {
		return -EINVAL;
	}

	memset(device_has_session, 0, sizeof(device_has_session));
	initialized = false;
	return 0;
}

/* ── Device enumeration ─────────────────────────────────────────────────── */

int eai_sensor_get_device_count(void)
{
	if (!initialized) {
		return -EINVAL;
	}
	return (int)device_count;
}

int eai_sensor_get_device(uint8_t index, struct eai_sensor_device *dev)
{
	if (!initialized || !dev) {
		return -EINVAL;
	}
	if (index >= device_count) {
		return -EINVAL;
	}

	*dev = devices[index];
	return 0;
}

int eai_sensor_find_device(enum eai_sensor_type type,
			   struct eai_sensor_device *dev)
{
	if (!initialized || !dev) {
		return -EINVAL;
	}

	for (uint8_t i = 0; i < device_count; i++) {
		if (devices[i].type == type) {
			*dev = devices[i];
			return 0;
		}
	}

	return -ENODEV;
}

/* ── Session lifecycle ──────────────────────────────────────────────────── */

int eai_sensor_session_open(struct eai_sensor_session *session,
			    uint8_t device_id,
			    const struct eai_sensor_config *config)
{
	if (!initialized || !session || !config) {
		return -EINVAL;
	}

	struct eai_sensor_device *dev = find_device_by_id(device_id);

	if (!dev) {
		return -ENODEV;
	}

	if (device_has_session[device_id]) {
		return -EBUSY;
	}

	memset(session, 0, sizeof(*session));
	session->config = *config;
	session->device_id = device_id;
	session->callback = NULL;
	session->user_data = NULL;

	struct eai_sensor_posix_session *ps = session_backend(session);

	ps->opened = true;
	ps->active = false;

	device_has_session[device_id] = true;
	return 0;
}

int eai_sensor_session_start(struct eai_sensor_session *session,
			     eai_sensor_data_cb_t callback,
			     void *user_data)
{
	if (!initialized || !session) {
		return -EINVAL;
	}

	struct eai_sensor_posix_session *ps = session_backend(session);

	if (!ps->opened) {
		return -EINVAL;
	}

	session->callback = callback;
	session->user_data = user_data;
	ps->active = true;
	return 0;
}

int eai_sensor_session_read(struct eai_sensor_session *session,
			    struct eai_sensor_data *data,
			    uint32_t count, uint32_t timeout_ms)
{
	(void)timeout_ms;

	if (!initialized || !session || !data || count == 0) {
		return -EINVAL;
	}

	struct eai_sensor_posix_session *ps = session_backend(session);

	if (!ps->active) {
		return -EINVAL;
	}

	/* Return injected data matching this session's device */
	uint32_t read_count = 0;

	while (read_count < count && test_data_count > 0) {
		struct eai_sensor_data *td = &test_data[test_data_tail];

		if (td->device_id == session->device_id) {
			data[read_count] = *td;
			read_count++;
		}
		test_data_tail = (test_data_tail + 1) % TEST_DATA_MAX;
		test_data_count--;
	}

	return (int)read_count;
}

int eai_sensor_session_flush(struct eai_sensor_session *session)
{
	if (!initialized || !session) {
		return -EINVAL;
	}

	struct eai_sensor_posix_session *ps = session_backend(session);

	if (!ps->active) {
		return -EINVAL;
	}

	/* In callback mode, deliver all pending data via callback */
	if (session->callback) {
		while (test_data_count > 0) {
			struct eai_sensor_data *td = &test_data[test_data_tail];

			if (td->device_id == session->device_id) {
				session->callback(td, session->user_data);
			}
			test_data_tail = (test_data_tail + 1) % TEST_DATA_MAX;
			test_data_count--;
		}
	}

	return 0;
}

int eai_sensor_session_stop(struct eai_sensor_session *session)
{
	if (!initialized || !session) {
		return -EINVAL;
	}

	struct eai_sensor_posix_session *ps = session_backend(session);

	ps->active = false;
	session->callback = NULL;
	session->user_data = NULL;
	return 0;
}

int eai_sensor_session_close(struct eai_sensor_session *session)
{
	if (!initialized || !session) {
		return -EINVAL;
	}

	struct eai_sensor_posix_session *ps = session_backend(session);

	if (ps->active) {
		eai_sensor_session_stop(session);
	}

	if (session->device_id < CONFIG_EAI_SENSOR_MAX_DEVICES) {
		device_has_session[session->device_id] = false;
	}

	ps->opened = false;
	return 0;
}

/* ── Test helpers ───────────────────────────────────────────────────────── */

void eai_sensor_test_inject_data(const struct eai_sensor_data *data)
{
	if (!data || test_data_count >= TEST_DATA_MAX) {
		return;
	}

	test_data[test_data_head] = *data;
	test_data_head = (test_data_head + 1) % TEST_DATA_MAX;
	test_data_count++;
}

void eai_sensor_test_reset(void)
{
	initialized = false;
	device_count = 0;
	memset(device_has_session, 0, sizeof(device_has_session));
	test_data_head = 0;
	test_data_tail = 0;
	test_data_count = 0;
}
