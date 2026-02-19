/*
 * eai_display POSIX stub tests
 *
 * Verifies API contract using the POSIX stub backend:
 * init/deinit, device enumeration, layer lifecycle, write, commit,
 * brightness, vsync.
 */

#include "unity.h"
#include <eai_display/eai_display.h>
#include <errno.h>
#include <string.h>

void setUp(void)
{
	eai_display_test_reset();
}

void tearDown(void)
{
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Init / Deinit
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_init_success(void)
{
	TEST_ASSERT_EQUAL(0, eai_display_init());
}

static void test_deinit_success(void)
{
	eai_display_init();
	TEST_ASSERT_EQUAL(0, eai_display_deinit());
}

static void test_deinit_without_init(void)
{
	TEST_ASSERT_EQUAL(-EINVAL, eai_display_deinit());
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Device enumeration
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_device_count(void)
{
	eai_display_init();
	TEST_ASSERT_EQUAL(1, eai_display_get_device_count());
}

static void test_get_device_lcd(void)
{
	eai_display_init();
	struct eai_display_device dev;

	TEST_ASSERT_EQUAL(0, eai_display_get_device(0, &dev));
	TEST_ASSERT_EQUAL(0, dev.id);
	TEST_ASSERT_EQUAL_STRING("lcd", dev.name);
	TEST_ASSERT_EQUAL(320, dev.width);
	TEST_ASSERT_EQUAL(240, dev.height);
	TEST_ASSERT_EQUAL(60, dev.max_fps);
	TEST_ASSERT_EQUAL(2, dev.format_count);
	TEST_ASSERT_EQUAL(EAI_DISPLAY_FORMAT_RGB565, dev.formats[0]);
	TEST_ASSERT_EQUAL(EAI_DISPLAY_FORMAT_RGB888, dev.formats[1]);
}

static void test_get_device_out_of_range(void)
{
	eai_display_init();
	struct eai_display_device dev;

	TEST_ASSERT_EQUAL(-EINVAL, eai_display_get_device(99, &dev));
}

static void test_get_device_null(void)
{
	eai_display_init();
	TEST_ASSERT_EQUAL(-EINVAL, eai_display_get_device(0, NULL));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Layer lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */

static const struct eai_display_layer_config test_layer_cfg = {
	.x = 0,
	.y = 0,
	.width = 320,
	.height = 240,
	.format = EAI_DISPLAY_FORMAT_RGB565,
};

static void test_layer_open_close(void)
{
	eai_display_init();
	struct eai_display_layer layer;

	TEST_ASSERT_EQUAL(0, eai_display_layer_open(&layer, 0, &test_layer_cfg));
	TEST_ASSERT_EQUAL(0, layer.display_id);
	TEST_ASSERT_EQUAL(0, eai_display_layer_close(&layer));
}

static void test_layer_open_invalid_display(void)
{
	eai_display_init();
	struct eai_display_layer layer;

	TEST_ASSERT_EQUAL(-ENODEV,
		eai_display_layer_open(&layer, 99, &test_layer_cfg));
}

static void test_layer_open_null(void)
{
	eai_display_init();
	TEST_ASSERT_EQUAL(-EINVAL,
		eai_display_layer_open(NULL, 0, &test_layer_cfg));
}

static void test_layer_open_out_of_bounds(void)
{
	eai_display_init();
	struct eai_display_layer layer;
	struct eai_display_layer_config cfg = {
		.x = 300,
		.y = 200,
		.width = 100, /* 300 + 100 = 400 > 320 */
		.height = 50,
		.format = EAI_DISPLAY_FORMAT_RGB565,
	};

	TEST_ASSERT_EQUAL(-EINVAL,
		eai_display_layer_open(&layer, 0, &cfg));
}

static void test_layer_open_max_layers(void)
{
	eai_display_init();
	struct eai_display_layer layers[5]; /* max is 4 */
	struct eai_display_layer_config small = {
		.x = 0, .y = 0, .width = 10, .height = 10,
		.format = EAI_DISPLAY_FORMAT_RGB565,
	};

	for (int i = 0; i < 4; i++) {
		TEST_ASSERT_EQUAL(0,
			eai_display_layer_open(&layers[i], 0, &small));
	}
	TEST_ASSERT_EQUAL(-ENOMEM,
		eai_display_layer_open(&layers[4], 0, &small));

	for (int i = 0; i < 4; i++) {
		eai_display_layer_close(&layers[i]);
	}
}

static void test_layer_reopen_after_close(void)
{
	eai_display_init();
	struct eai_display_layer l1, l2;

	eai_display_layer_open(&l1, 0, &test_layer_cfg);
	eai_display_layer_close(&l1);
	TEST_ASSERT_EQUAL(0, eai_display_layer_open(&l2, 0, &test_layer_cfg));
	eai_display_layer_close(&l2);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Layer write + commit
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_layer_write_commit(void)
{
	eai_display_init();
	struct eai_display_layer layer;

	eai_display_layer_open(&layer, 0, &test_layer_cfg);

	/* Write a small pattern (first 4 pixels of RGB565) */
	uint16_t pixels[] = {0xF800, 0x07E0, 0x001F, 0xFFFF}; /* R, G, B, W */

	TEST_ASSERT_EQUAL(0, eai_display_layer_write(&layer, pixels, sizeof(pixels)));
	TEST_ASSERT_EQUAL(0, eai_display_commit(0));

	const uint8_t *fb;
	uint32_t fb_size;

	eai_display_test_get_framebuffer(&fb, &fb_size);
	TEST_ASSERT_EQUAL(sizeof(pixels), fb_size);

	/* Verify pixel data */
	const uint16_t *fb16 = (const uint16_t *)fb;

	TEST_ASSERT_EQUAL_HEX16(0xF800, fb16[0]);
	TEST_ASSERT_EQUAL_HEX16(0x07E0, fb16[1]);
	TEST_ASSERT_EQUAL_HEX16(0x001F, fb16[2]);
	TEST_ASSERT_EQUAL_HEX16(0xFFFF, fb16[3]);

	eai_display_layer_close(&layer);
}

static void test_commit_count(void)
{
	eai_display_init();

	TEST_ASSERT_EQUAL(0, eai_display_test_get_commit_count());

	eai_display_commit(0);
	TEST_ASSERT_EQUAL(1, eai_display_test_get_commit_count());

	eai_display_commit(0);
	TEST_ASSERT_EQUAL(2, eai_display_test_get_commit_count());
}

static void test_layer_write_null(void)
{
	eai_display_init();
	struct eai_display_layer layer;

	eai_display_layer_open(&layer, 0, &test_layer_cfg);
	TEST_ASSERT_EQUAL(-EINVAL,
		eai_display_layer_write(&layer, NULL, 100));
	eai_display_layer_close(&layer);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Brightness
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_brightness_set_get(void)
{
	eai_display_init();

	TEST_ASSERT_EQUAL(0, eai_display_set_brightness(0, 50));

	uint8_t pct;

	TEST_ASSERT_EQUAL(0, eai_display_get_brightness(0, &pct));
	TEST_ASSERT_EQUAL(50, pct);
}

static void test_brightness_clamp(void)
{
	eai_display_init();

	eai_display_set_brightness(0, 200); /* clamp to 100 */

	uint8_t pct;

	eai_display_get_brightness(0, &pct);
	TEST_ASSERT_EQUAL(100, pct);
}

static void test_brightness_invalid_display(void)
{
	eai_display_init();
	TEST_ASSERT_EQUAL(-EINVAL, eai_display_set_brightness(99, 50));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Vsync
 * ═══════════════════════════════════════════════════════════════════════════ */

static int vsync_call_count;
static uint8_t vsync_display_id;

static void test_vsync_callback(uint8_t display_id, uint64_t timestamp_ns,
				void *user_data)
{
	(void)timestamp_ns;
	(void)user_data;
	vsync_call_count++;
	vsync_display_id = display_id;
}

static void test_vsync_on_commit(void)
{
	eai_display_init();
	vsync_call_count = 0;

	eai_display_set_vsync(0, true, test_vsync_callback, NULL);
	eai_display_commit(0);

	TEST_ASSERT_EQUAL(1, vsync_call_count);
	TEST_ASSERT_EQUAL(0, vsync_display_id);
}

static void test_vsync_disabled(void)
{
	eai_display_init();
	vsync_call_count = 0;

	eai_display_set_vsync(0, false, test_vsync_callback, NULL);
	eai_display_commit(0);

	TEST_ASSERT_EQUAL(0, vsync_call_count);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Error cases
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_operations_before_init(void)
{
	TEST_ASSERT_EQUAL(-EINVAL, eai_display_get_device_count());

	struct eai_display_device dev;

	TEST_ASSERT_EQUAL(-EINVAL, eai_display_get_device(0, &dev));
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
	RUN_TEST(test_get_device_lcd);
	RUN_TEST(test_get_device_out_of_range);
	RUN_TEST(test_get_device_null);

	/* Layer lifecycle */
	RUN_TEST(test_layer_open_close);
	RUN_TEST(test_layer_open_invalid_display);
	RUN_TEST(test_layer_open_null);
	RUN_TEST(test_layer_open_out_of_bounds);
	RUN_TEST(test_layer_open_max_layers);
	RUN_TEST(test_layer_reopen_after_close);

	/* Layer write + commit */
	RUN_TEST(test_layer_write_commit);
	RUN_TEST(test_commit_count);
	RUN_TEST(test_layer_write_null);

	/* Brightness */
	RUN_TEST(test_brightness_set_get);
	RUN_TEST(test_brightness_clamp);
	RUN_TEST(test_brightness_invalid_display);

	/* Vsync */
	RUN_TEST(test_vsync_on_commit);
	RUN_TEST(test_vsync_disabled);

	/* Error cases */
	RUN_TEST(test_operations_before_init);

	return UNITY_END();
}
