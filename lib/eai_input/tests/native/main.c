/*
 * eai_input POSIX stub tests
 *
 * Verifies API contract using the POSIX stub backend:
 * init/deinit, device enumeration, event polling, event callback.
 */

#include "unity.h"
#include <eai_input/eai_input.h>
#include <errno.h>
#include <string.h>

void setUp(void)
{
	eai_input_test_reset();
}

void tearDown(void)
{
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Init / Deinit
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_init_success(void)
{
	TEST_ASSERT_EQUAL(0, eai_input_init(NULL, NULL));
}

static void test_deinit_success(void)
{
	eai_input_init(NULL, NULL);
	TEST_ASSERT_EQUAL(0, eai_input_deinit());
}

static void test_deinit_without_init(void)
{
	TEST_ASSERT_EQUAL(-EINVAL, eai_input_deinit());
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Device enumeration
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_device_count(void)
{
	eai_input_init(NULL, NULL);
	TEST_ASSERT_EQUAL(3, eai_input_get_device_count());
}

static void test_get_device_touch(void)
{
	eai_input_init(NULL, NULL);
	struct eai_input_device dev;

	TEST_ASSERT_EQUAL(0, eai_input_get_device(0, &dev));
	TEST_ASSERT_EQUAL(0, dev.id);
	TEST_ASSERT_EQUAL_STRING("touch", dev.name);
	TEST_ASSERT_EQUAL(EAI_INPUT_DEVICE_TOUCH, dev.type);
	TEST_ASSERT_EQUAL(0, dev.x_min);
	TEST_ASSERT_EQUAL(319, dev.x_max);
	TEST_ASSERT_EQUAL(0, dev.y_min);
	TEST_ASSERT_EQUAL(239, dev.y_max);
}

static void test_get_device_button(void)
{
	eai_input_init(NULL, NULL);
	struct eai_input_device dev;

	TEST_ASSERT_EQUAL(0, eai_input_get_device(1, &dev));
	TEST_ASSERT_EQUAL(1, dev.id);
	TEST_ASSERT_EQUAL_STRING("btn_a", dev.name);
	TEST_ASSERT_EQUAL(EAI_INPUT_DEVICE_BUTTON, dev.type);
}

static void test_get_device_out_of_range(void)
{
	eai_input_init(NULL, NULL);
	struct eai_input_device dev;

	TEST_ASSERT_EQUAL(-EINVAL, eai_input_get_device(99, &dev));
}

static void test_get_device_null(void)
{
	eai_input_init(NULL, NULL);
	TEST_ASSERT_EQUAL(-EINVAL, eai_input_get_device(0, NULL));
}

static void test_find_device_touch(void)
{
	eai_input_init(NULL, NULL);
	struct eai_input_device dev;

	TEST_ASSERT_EQUAL(0, eai_input_find_device(
		EAI_INPUT_DEVICE_TOUCH, &dev));
	TEST_ASSERT_EQUAL_STRING("touch", dev.name);
}

static void test_find_device_button(void)
{
	eai_input_init(NULL, NULL);
	struct eai_input_device dev;

	TEST_ASSERT_EQUAL(0, eai_input_find_device(
		EAI_INPUT_DEVICE_BUTTON, &dev));
	TEST_ASSERT_EQUAL_STRING("btn_a", dev.name); /* first match */
}

static void test_find_device_not_found(void)
{
	eai_input_init(NULL, NULL);
	struct eai_input_device dev;

	TEST_ASSERT_EQUAL(-ENODEV, eai_input_find_device(
		EAI_INPUT_DEVICE_ENCODER, &dev));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Event polling
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_read_empty(void)
{
	eai_input_init(NULL, NULL);

	struct eai_input_event event;

	TEST_ASSERT_EQUAL(-EAGAIN, eai_input_read(&event, 0));
}

static void test_read_injected_touch(void)
{
	eai_input_init(NULL, NULL);

	struct eai_input_event injected = {
		.device_id = 0,
		.type = EAI_INPUT_EVENT_PRESS,
		.x = 160,
		.y = 120,
		.code = 0,
		.timestamp_ms = 1000,
	};

	eai_input_test_inject_event(&injected);

	struct eai_input_event event;

	TEST_ASSERT_EQUAL(0, eai_input_read(&event, 0));
	TEST_ASSERT_EQUAL(0, event.device_id);
	TEST_ASSERT_EQUAL(EAI_INPUT_EVENT_PRESS, event.type);
	TEST_ASSERT_EQUAL(160, event.x);
	TEST_ASSERT_EQUAL(120, event.y);
	TEST_ASSERT_EQUAL(1000, event.timestamp_ms);
}

static void test_read_injected_button(void)
{
	eai_input_init(NULL, NULL);

	struct eai_input_event press = {
		.device_id = 1,
		.type = EAI_INPUT_EVENT_PRESS,
		.code = 1,
		.timestamp_ms = 2000,
	};
	struct eai_input_event release = {
		.device_id = 1,
		.type = EAI_INPUT_EVENT_RELEASE,
		.code = 1,
		.timestamp_ms = 2100,
	};

	eai_input_test_inject_event(&press);
	eai_input_test_inject_event(&release);

	struct eai_input_event event;

	TEST_ASSERT_EQUAL(0, eai_input_read(&event, 0));
	TEST_ASSERT_EQUAL(EAI_INPUT_EVENT_PRESS, event.type);

	TEST_ASSERT_EQUAL(0, eai_input_read(&event, 0));
	TEST_ASSERT_EQUAL(EAI_INPUT_EVENT_RELEASE, event.type);

	/* Queue empty now */
	TEST_ASSERT_EQUAL(-EAGAIN, eai_input_read(&event, 0));
}

static void test_read_null(void)
{
	eai_input_init(NULL, NULL);
	TEST_ASSERT_EQUAL(-EINVAL, eai_input_read(NULL, 0));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Event callback
 * ═══════════════════════════════════════════════════════════════════════════ */

static struct eai_input_event cb_events[8];
static int cb_count;

static void test_event_callback(const struct eai_input_event *event,
				void *user_data)
{
	(void)user_data;
	if (cb_count < 8) {
		cb_events[cb_count] = *event;
		cb_count++;
	}
}

static void test_callback_mode(void)
{
	cb_count = 0;
	eai_input_init(test_event_callback, NULL);

	struct eai_input_event injected = {
		.device_id = 0,
		.type = EAI_INPUT_EVENT_MOVE,
		.x = 50,
		.y = 75,
		.timestamp_ms = 3000,
	};

	eai_input_test_inject_event(&injected);

	TEST_ASSERT_EQUAL(1, cb_count);
	TEST_ASSERT_EQUAL(EAI_INPUT_EVENT_MOVE, cb_events[0].type);
	TEST_ASSERT_EQUAL(50, cb_events[0].x);
	TEST_ASSERT_EQUAL(75, cb_events[0].y);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Error cases
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_operations_before_init(void)
{
	TEST_ASSERT_EQUAL(-EINVAL, eai_input_get_device_count());

	struct eai_input_device dev;

	TEST_ASSERT_EQUAL(-EINVAL, eai_input_get_device(0, &dev));
	TEST_ASSERT_EQUAL(-EINVAL,
		eai_input_find_device(EAI_INPUT_DEVICE_TOUCH, &dev));

	struct eai_input_event event;

	TEST_ASSERT_EQUAL(-EINVAL, eai_input_read(&event, 0));
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
	RUN_TEST(test_get_device_touch);
	RUN_TEST(test_get_device_button);
	RUN_TEST(test_get_device_out_of_range);
	RUN_TEST(test_get_device_null);
	RUN_TEST(test_find_device_touch);
	RUN_TEST(test_find_device_button);
	RUN_TEST(test_find_device_not_found);

	/* Event polling */
	RUN_TEST(test_read_empty);
	RUN_TEST(test_read_injected_touch);
	RUN_TEST(test_read_injected_button);
	RUN_TEST(test_read_null);

	/* Callback */
	RUN_TEST(test_callback_mode);

	/* Error cases */
	RUN_TEST(test_operations_before_init);

	return UNITY_END();
}
