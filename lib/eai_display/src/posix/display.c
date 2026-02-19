/*
 * eai_display POSIX stub backend
 *
 * Provides a fake 320x240 display for native testing.
 * No actual display hardware interaction.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <eai_display/eai_display.h>
#include <errno.h>
#include <string.h>

/* ── Configuration defaults ─────────────────────────────────────────────── */

#ifndef CONFIG_EAI_DISPLAY_MAX_DEVICES
#define CONFIG_EAI_DISPLAY_MAX_DEVICES 2
#endif

#ifndef CONFIG_EAI_DISPLAY_MAX_LAYERS
#define CONFIG_EAI_DISPLAY_MAX_LAYERS 4
#endif

/* ── Fake display dimensions ────────────────────────────────────────────── */

#define FAKE_WIDTH  320
#define FAKE_HEIGHT 240
#define FAKE_BPP    2 /* RGB565 = 2 bytes per pixel */
#define FAKE_FB_SIZE (FAKE_WIDTH * FAKE_HEIGHT * FAKE_BPP)

/* ── Module state ───────────────────────────────────────────────────────── */

static bool initialized;

/* Device table */
static struct eai_display_device devices[CONFIG_EAI_DISPLAY_MAX_DEVICES];
static uint8_t device_count;

/* Layer slots */
static bool layer_slots[CONFIG_EAI_DISPLAY_MAX_LAYERS];
static uint8_t layer_display[CONFIG_EAI_DISPLAY_MAX_LAYERS]; /* which display */

/* Framebuffer (one per display, only display 0 for POSIX stub) */
static uint8_t framebuffer[FAKE_FB_SIZE];
static uint32_t fb_written_size;

/* Per-layer pixel buffer (staging before commit) */
static uint8_t layer_buf[CONFIG_EAI_DISPLAY_MAX_LAYERS][FAKE_FB_SIZE];
static uint32_t layer_buf_size[CONFIG_EAI_DISPLAY_MAX_LAYERS];

/* Commit counter */
static uint32_t commit_count;

/* Brightness */
static uint8_t brightness;

/* Vsync */
static eai_display_vsync_cb_t vsync_cb;
static void *vsync_user_data;
static bool vsync_enabled;

/* ── Helper: get posix layer data from opaque backend ───────────────────── */

static struct eai_display_posix_layer *layer_backend(
	struct eai_display_layer *l)
{
	return (struct eai_display_posix_layer *)l->_backend;
}

/* ── Helper: bytes per pixel ────────────────────────────────────────────── */

static uint32_t bpp(enum eai_display_format fmt)
{
	switch (fmt) {
	case EAI_DISPLAY_FORMAT_MONO1:   return 0; /* sub-byte */
	case EAI_DISPLAY_FORMAT_RGB565:  return 2;
	case EAI_DISPLAY_FORMAT_RGB888:  return 3;
	case EAI_DISPLAY_FORMAT_ARGB8888: return 4;
	default: return 0;
	}
}

/* ── Default device setup ───────────────────────────────────────────────── */

static void setup_default_devices(void)
{
	device_count = 1;

	memset(&devices[0], 0, sizeof(devices[0]));
	devices[0].id = 0;
	strncpy(devices[0].name, "lcd", EAI_DISPLAY_NAME_MAX - 1);
	devices[0].width = FAKE_WIDTH;
	devices[0].height = FAKE_HEIGHT;
	devices[0].format_count = 2;
	devices[0].formats[0] = EAI_DISPLAY_FORMAT_RGB565;
	devices[0].formats[1] = EAI_DISPLAY_FORMAT_RGB888;
	devices[0].max_fps = 60;
	devices[0].max_layers = CONFIG_EAI_DISPLAY_MAX_LAYERS;
}

/* ── Module lifecycle ───────────────────────────────────────────────────── */

int eai_display_init(void)
{
	memset(layer_slots, 0, sizeof(layer_slots));
	memset(framebuffer, 0, sizeof(framebuffer));
	fb_written_size = 0;
	commit_count = 0;
	brightness = 100;
	vsync_cb = NULL;
	vsync_user_data = NULL;
	vsync_enabled = false;

	for (int i = 0; i < CONFIG_EAI_DISPLAY_MAX_LAYERS; i++) {
		layer_buf_size[i] = 0;
	}

	setup_default_devices();
	initialized = true;
	return 0;
}

int eai_display_deinit(void)
{
	if (!initialized) {
		return -EINVAL;
	}

	memset(layer_slots, 0, sizeof(layer_slots));
	initialized = false;
	return 0;
}

/* ── Device enumeration ─────────────────────────────────────────────────── */

int eai_display_get_device_count(void)
{
	if (!initialized) {
		return -EINVAL;
	}
	return (int)device_count;
}

int eai_display_get_device(uint8_t index, struct eai_display_device *dev)
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

/* ── Layer lifecycle ────────────────────────────────────────────────────── */

int eai_display_layer_open(struct eai_display_layer *layer,
			   uint8_t display_id,
			   const struct eai_display_layer_config *config)
{
	if (!initialized || !layer || !config) {
		return -EINVAL;
	}

	if (display_id >= device_count) {
		return -ENODEV;
	}

	/* Find a free layer slot */
	int slot = -1;

	for (int i = 0; i < CONFIG_EAI_DISPLAY_MAX_LAYERS; i++) {
		if (!layer_slots[i]) {
			slot = i;
			break;
		}
	}

	if (slot < 0) {
		return -ENOMEM;
	}

	/* Validate layer bounds */
	if (config->x + config->width > devices[display_id].width ||
	    config->y + config->height > devices[display_id].height) {
		return -EINVAL;
	}

	memset(layer, 0, sizeof(*layer));
	layer->config = *config;
	layer->display_id = display_id;

	struct eai_display_posix_layer *pl = layer_backend(layer);

	pl->opened = true;
	pl->slot_index = (uint8_t)slot;

	layer_slots[slot] = true;
	layer_display[slot] = display_id;
	layer_buf_size[slot] = 0;
	return 0;
}

int eai_display_layer_write(struct eai_display_layer *layer,
			    const void *pixels, uint32_t size)
{
	if (!initialized || !layer || !pixels || size == 0) {
		return -EINVAL;
	}

	struct eai_display_posix_layer *pl = layer_backend(layer);

	if (!pl->opened) {
		return -EINVAL;
	}

	/* Calculate expected size */
	uint32_t pixel_bytes = bpp(layer->config.format);
	uint32_t expected;

	if (layer->config.format == EAI_DISPLAY_FORMAT_MONO1) {
		expected = ((uint32_t)layer->config.width *
			    layer->config.height + 7) / 8;
	} else {
		expected = (uint32_t)layer->config.width *
			   layer->config.height * pixel_bytes;
	}

	uint32_t to_write = size < expected ? size : expected;

	if (to_write > FAKE_FB_SIZE) {
		to_write = FAKE_FB_SIZE;
	}

	memcpy(layer_buf[pl->slot_index], pixels, to_write);
	layer_buf_size[pl->slot_index] = to_write;
	return 0;
}

int eai_display_layer_close(struct eai_display_layer *layer)
{
	if (!initialized || !layer) {
		return -EINVAL;
	}

	struct eai_display_posix_layer *pl = layer_backend(layer);

	if (pl->opened && pl->slot_index < CONFIG_EAI_DISPLAY_MAX_LAYERS) {
		layer_slots[pl->slot_index] = false;
		layer_buf_size[pl->slot_index] = 0;
	}

	pl->opened = false;
	return 0;
}

/* ── Display commit ─────────────────────────────────────────────────────── */

int eai_display_commit(uint8_t display_id)
{
	if (!initialized) {
		return -EINVAL;
	}
	if (display_id >= device_count) {
		return -EINVAL;
	}

	/* Compose: copy first active layer's buffer to framebuffer */
	memset(framebuffer, 0, sizeof(framebuffer));
	fb_written_size = 0;

	for (int i = 0; i < CONFIG_EAI_DISPLAY_MAX_LAYERS; i++) {
		if (layer_slots[i] && layer_display[i] == display_id &&
		    layer_buf_size[i] > 0) {
			uint32_t to_copy = layer_buf_size[i];

			if (to_copy > FAKE_FB_SIZE) {
				to_copy = FAKE_FB_SIZE;
			}
			memcpy(framebuffer, layer_buf[i], to_copy);
			fb_written_size = to_copy;
			break; /* Simple: first layer wins */
		}
	}

	commit_count++;

	/* Trigger vsync callback if enabled */
	if (vsync_enabled && vsync_cb) {
		vsync_cb(display_id, 0, vsync_user_data);
	}

	return 0;
}

/* ── Brightness ─────────────────────────────────────────────────────────── */

int eai_display_set_brightness(uint8_t display_id, uint8_t percent)
{
	if (!initialized) {
		return -EINVAL;
	}
	if (display_id >= device_count) {
		return -EINVAL;
	}

	brightness = percent > 100 ? 100 : percent;
	return 0;
}

int eai_display_get_brightness(uint8_t display_id, uint8_t *percent)
{
	if (!initialized || !percent) {
		return -EINVAL;
	}
	if (display_id >= device_count) {
		return -EINVAL;
	}

	*percent = brightness;
	return 0;
}

/* ── Vsync ──────────────────────────────────────────────────────────────── */

int eai_display_set_vsync(uint8_t display_id, bool enabled,
			  eai_display_vsync_cb_t cb, void *user_data)
{
	if (!initialized) {
		return -EINVAL;
	}
	if (display_id >= device_count) {
		return -EINVAL;
	}

	vsync_enabled = enabled;
	vsync_cb = cb;
	vsync_user_data = user_data;
	return 0;
}

/* ── Test helpers ───────────────────────────────────────────────────────── */

void eai_display_test_get_framebuffer(const uint8_t **buf, uint32_t *size)
{
	if (buf) {
		*buf = framebuffer;
	}
	if (size) {
		*size = fb_written_size;
	}
}

uint32_t eai_display_test_get_commit_count(void)
{
	return commit_count;
}

void eai_display_test_reset(void)
{
	initialized = false;
	device_count = 0;
	memset(layer_slots, 0, sizeof(layer_slots));
	memset(framebuffer, 0, sizeof(framebuffer));
	fb_written_size = 0;
	commit_count = 0;
	brightness = 100;
	vsync_cb = NULL;
	vsync_user_data = NULL;
	vsync_enabled = false;

	for (int i = 0; i < CONFIG_EAI_DISPLAY_MAX_LAYERS; i++) {
		layer_buf_size[i] = 0;
	}
}
