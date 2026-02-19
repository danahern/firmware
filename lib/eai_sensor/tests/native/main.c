/*
 * eai_sensor POSIX stub tests
 *
 * Verifies API contract using the POSIX stub backend:
 * init/deinit, device enumeration, session lifecycle, read, callback, flush.
 */

#include "unity.h"
#include <eai_sensor/eai_sensor.h>
#include <errno.h>
#include <string.h>

void setUp(void)
{
	eai_sensor_test_reset();
}

void tearDown(void)
{
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Init / Deinit
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_init_success(void)
{
	TEST_ASSERT_EQUAL(0, eai_sensor_init());
}

static void test_deinit_success(void)
{
	eai_sensor_init();
	TEST_ASSERT_EQUAL(0, eai_sensor_deinit());
}

static void test_deinit_without_init(void)
{
	TEST_ASSERT_EQUAL(-EINVAL, eai_sensor_deinit());
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Device enumeration
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_device_count(void)
{
	eai_sensor_init();
	TEST_ASSERT_EQUAL(2, eai_sensor_get_device_count());
}

static void test_get_device_accel(void)
{
	eai_sensor_init();
	struct eai_sensor_device dev;

	TEST_ASSERT_EQUAL(0, eai_sensor_get_device(0, &dev));
	TEST_ASSERT_EQUAL(0, dev.id);
	TEST_ASSERT_EQUAL_STRING("accel", dev.name);
	TEST_ASSERT_EQUAL(EAI_SENSOR_TYPE_ACCEL, dev.type);
	TEST_ASSERT_EQUAL(-16000, dev.range_min);
	TEST_ASSERT_EQUAL(16000, dev.range_max);
	TEST_ASSERT_EQUAL(400, dev.max_rate_hz);
}

static void test_get_device_temp(void)
{
	eai_sensor_init();
	struct eai_sensor_device dev;

	TEST_ASSERT_EQUAL(0, eai_sensor_get_device(1, &dev));
	TEST_ASSERT_EQUAL(1, dev.id);
	TEST_ASSERT_EQUAL_STRING("temp", dev.name);
	TEST_ASSERT_EQUAL(EAI_SENSOR_TYPE_TEMPERATURE, dev.type);
}

static void test_get_device_out_of_range(void)
{
	eai_sensor_init();
	struct eai_sensor_device dev;

	TEST_ASSERT_EQUAL(-EINVAL, eai_sensor_get_device(99, &dev));
}

static void test_get_device_null(void)
{
	eai_sensor_init();
	TEST_ASSERT_EQUAL(-EINVAL, eai_sensor_get_device(0, NULL));
}

static void test_find_device_accel(void)
{
	eai_sensor_init();
	struct eai_sensor_device dev;

	TEST_ASSERT_EQUAL(0, eai_sensor_find_device(
		EAI_SENSOR_TYPE_ACCEL, &dev));
	TEST_ASSERT_EQUAL_STRING("accel", dev.name);
}

static void test_find_device_temp(void)
{
	eai_sensor_init();
	struct eai_sensor_device dev;

	TEST_ASSERT_EQUAL(0, eai_sensor_find_device(
		EAI_SENSOR_TYPE_TEMPERATURE, &dev));
	TEST_ASSERT_EQUAL_STRING("temp", dev.name);
}

static void test_find_device_not_found(void)
{
	eai_sensor_init();
	struct eai_sensor_device dev;

	TEST_ASSERT_EQUAL(-ENODEV, eai_sensor_find_device(
		EAI_SENSOR_TYPE_GYRO, &dev));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Session lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */

static const struct eai_sensor_config test_config = {
	.rate_hz = 100,
	.max_latency_ms = 0,
};

static void test_session_open_close(void)
{
	eai_sensor_init();
	struct eai_sensor_session session;

	TEST_ASSERT_EQUAL(0, eai_sensor_session_open(&session, 0, &test_config));
	TEST_ASSERT_EQUAL(0, session.device_id);
	TEST_ASSERT_EQUAL(0, eai_sensor_session_close(&session));
}

static void test_session_open_invalid_device(void)
{
	eai_sensor_init();
	struct eai_sensor_session session;

	TEST_ASSERT_EQUAL(-ENODEV,
		eai_sensor_session_open(&session, 99, &test_config));
}

static void test_session_open_null(void)
{
	eai_sensor_init();
	TEST_ASSERT_EQUAL(-EINVAL,
		eai_sensor_session_open(NULL, 0, &test_config));
}

static void test_session_open_busy(void)
{
	eai_sensor_init();
	struct eai_sensor_session s1, s2;

	TEST_ASSERT_EQUAL(0, eai_sensor_session_open(&s1, 0, &test_config));
	TEST_ASSERT_EQUAL(-EBUSY, eai_sensor_session_open(&s2, 0, &test_config));

	eai_sensor_session_close(&s1);
}

static void test_session_reopen_after_close(void)
{
	eai_sensor_init();
	struct eai_sensor_session s1, s2;

	eai_sensor_session_open(&s1, 0, &test_config);
	eai_sensor_session_close(&s1);
	TEST_ASSERT_EQUAL(0, eai_sensor_session_open(&s2, 0, &test_config));
	eai_sensor_session_close(&s2);
}

static void test_session_start_stop(void)
{
	eai_sensor_init();
	struct eai_sensor_session session;

	eai_sensor_session_open(&session, 0, &test_config);
	TEST_ASSERT_EQUAL(0, eai_sensor_session_start(&session, NULL, NULL));
	TEST_ASSERT_EQUAL(0, eai_sensor_session_stop(&session));
	eai_sensor_session_close(&session);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Session read (polling)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_session_read(void)
{
	eai_sensor_init();

	/* Inject test data for device 0 (accel) */
	struct eai_sensor_data injected = {
		.device_id = 0,
		.type = EAI_SENSOR_TYPE_ACCEL,
		.timestamp_ns = 1000000,
		.vec3 = { .x = 100, .y = -200, .z = 9800 },
	};

	eai_sensor_test_inject_data(&injected);

	struct eai_sensor_session session;

	eai_sensor_session_open(&session, 0, &test_config);
	eai_sensor_session_start(&session, NULL, NULL);

	struct eai_sensor_data buf[4];
	int ret = eai_sensor_session_read(&session, buf, 4, 0);

	TEST_ASSERT_EQUAL(1, ret);
	TEST_ASSERT_EQUAL(0, buf[0].device_id);
	TEST_ASSERT_EQUAL(EAI_SENSOR_TYPE_ACCEL, buf[0].type);
	TEST_ASSERT_EQUAL(100, buf[0].vec3.x);
	TEST_ASSERT_EQUAL(-200, buf[0].vec3.y);
	TEST_ASSERT_EQUAL(9800, buf[0].vec3.z);

	eai_sensor_session_close(&session);
}

static void test_session_read_multiple(void)
{
	eai_sensor_init();

	struct eai_sensor_data d1 = {
		.device_id = 0, .type = EAI_SENSOR_TYPE_ACCEL,
		.timestamp_ns = 1000000,
		.vec3 = { .x = 10, .y = 20, .z = 30 },
	};
	struct eai_sensor_data d2 = {
		.device_id = 0, .type = EAI_SENSOR_TYPE_ACCEL,
		.timestamp_ns = 2000000,
		.vec3 = { .x = 40, .y = 50, .z = 60 },
	};

	eai_sensor_test_inject_data(&d1);
	eai_sensor_test_inject_data(&d2);

	struct eai_sensor_session session;

	eai_sensor_session_open(&session, 0, &test_config);
	eai_sensor_session_start(&session, NULL, NULL);

	struct eai_sensor_data buf[4];
	int ret = eai_sensor_session_read(&session, buf, 4, 0);

	TEST_ASSERT_EQUAL(2, ret);
	TEST_ASSERT_EQUAL(10, buf[0].vec3.x);
	TEST_ASSERT_EQUAL(40, buf[1].vec3.x);

	eai_sensor_session_close(&session);
}

static void test_session_read_not_started(void)
{
	eai_sensor_init();
	struct eai_sensor_session session;

	eai_sensor_session_open(&session, 0, &test_config);

	struct eai_sensor_data buf[1];

	TEST_ASSERT_EQUAL(-EINVAL,
		eai_sensor_session_read(&session, buf, 1, 0));

	eai_sensor_session_close(&session);
}

static void test_session_read_empty(void)
{
	eai_sensor_init();
	struct eai_sensor_session session;

	eai_sensor_session_open(&session, 0, &test_config);
	eai_sensor_session_start(&session, NULL, NULL);

	struct eai_sensor_data buf[4];
	int ret = eai_sensor_session_read(&session, buf, 4, 0);

	TEST_ASSERT_EQUAL(0, ret);

	eai_sensor_session_close(&session);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Session callback + flush
 * ═══════════════════════════════════════════════════════════════════════════ */

static struct eai_sensor_data callback_received[8];
static int callback_count;

static void test_data_callback(const struct eai_sensor_data *data,
			       void *user_data)
{
	(void)user_data;
	if (callback_count < 8) {
		callback_received[callback_count] = *data;
		callback_count++;
	}
}

static void test_session_callback_flush(void)
{
	eai_sensor_init();
	callback_count = 0;

	struct eai_sensor_data injected = {
		.device_id = 0, .type = EAI_SENSOR_TYPE_ACCEL,
		.timestamp_ns = 5000000,
		.vec3 = { .x = 999, .y = 888, .z = 777 },
	};

	eai_sensor_test_inject_data(&injected);

	struct eai_sensor_session session;

	eai_sensor_session_open(&session, 0, &test_config);
	eai_sensor_session_start(&session, test_data_callback, NULL);
	eai_sensor_session_flush(&session);

	TEST_ASSERT_EQUAL(1, callback_count);
	TEST_ASSERT_EQUAL(999, callback_received[0].vec3.x);

	eai_sensor_session_close(&session);
}

static void test_session_flush_not_started(void)
{
	eai_sensor_init();
	struct eai_sensor_session session;

	eai_sensor_session_open(&session, 0, &test_config);
	TEST_ASSERT_EQUAL(-EINVAL, eai_sensor_session_flush(&session));
	eai_sensor_session_close(&session);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Temperature sensor (scalar data)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_temp_sensor_read(void)
{
	eai_sensor_init();

	struct eai_sensor_data injected = {
		.device_id = 1,
		.type = EAI_SENSOR_TYPE_TEMPERATURE,
		.timestamp_ns = 3000000,
		.scalar = 25500, /* 25.5°C in m°C */
	};

	eai_sensor_test_inject_data(&injected);

	struct eai_sensor_session session;

	eai_sensor_session_open(&session, 1, &test_config);
	eai_sensor_session_start(&session, NULL, NULL);

	struct eai_sensor_data buf[1];
	int ret = eai_sensor_session_read(&session, buf, 1, 0);

	TEST_ASSERT_EQUAL(1, ret);
	TEST_ASSERT_EQUAL(25500, buf[0].scalar);
	TEST_ASSERT_EQUAL(EAI_SENSOR_TYPE_TEMPERATURE, buf[0].type);

	eai_sensor_session_close(&session);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Error cases
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_operations_before_init(void)
{
	/* All operations should fail before init */
	TEST_ASSERT_EQUAL(-EINVAL, eai_sensor_get_device_count());

	struct eai_sensor_device dev;

	TEST_ASSERT_EQUAL(-EINVAL, eai_sensor_get_device(0, &dev));
	TEST_ASSERT_EQUAL(-EINVAL,
		eai_sensor_find_device(EAI_SENSOR_TYPE_ACCEL, &dev));
}

/* ═══════════════════════════════════════════════════════════════════════════ */

int main(void)
{
	UNITY_BEGIN();

	/* Init/deinit */
	RUN_TEST(test_init_success);
	RUN_TEST(test_deinit_success);
	RUN_TEST(test_deinit_without_init);

	/* Device enumeration */
	RUN_TEST(test_device_count);
	RUN_TEST(test_get_device_accel);
	RUN_TEST(test_get_device_temp);
	RUN_TEST(test_get_device_out_of_range);
	RUN_TEST(test_get_device_null);
	RUN_TEST(test_find_device_accel);
	RUN_TEST(test_find_device_temp);
	RUN_TEST(test_find_device_not_found);

	/* Session lifecycle */
	RUN_TEST(test_session_open_close);
	RUN_TEST(test_session_open_invalid_device);
	RUN_TEST(test_session_open_null);
	RUN_TEST(test_session_open_busy);
	RUN_TEST(test_session_reopen_after_close);
	RUN_TEST(test_session_start_stop);

	/* Session read */
	RUN_TEST(test_session_read);
	RUN_TEST(test_session_read_multiple);
	RUN_TEST(test_session_read_not_started);
	RUN_TEST(test_session_read_empty);

	/* Callback + flush */
	RUN_TEST(test_session_callback_flush);
	RUN_TEST(test_session_flush_not_started);

	/* Temperature (scalar) */
	RUN_TEST(test_temp_sensor_read);

	/* Error cases */
	RUN_TEST(test_operations_before_init);

	return UNITY_END();
}
