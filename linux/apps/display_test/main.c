/**
 * Alif E7 Display Test
 *
 * Validates the CDC200 display controller + ILI9806E MIPI DSI panel (480x800).
 * Uses DRM/KMS to enumerate connectors and display color bars.
 * Falls back to /dev/fb0 framebuffer if DRM is unavailable.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <linux/fb.h>

#define HOLD_SECONDS 10

/* Color bar colors: white, yellow, cyan, green, magenta, red, blue, black */
static const uint32_t bar_colors_rgb888[] = {
	0x00FFFFFF, 0x00FFFF00, 0x0000FFFF, 0x0000FF00,
	0x00FF00FF, 0x00FF0000, 0x000000FF, 0x00000000,
};

static uint16_t rgb888_to_rgb565(uint32_t rgb)
{
	uint8_t r = (rgb >> 16) & 0xFF;
	uint8_t g = (rgb >> 8) & 0xFF;
	uint8_t b = rgb & 0xFF;

	return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

static void fill_bars_32(uint32_t *buf, int width, int height, int stride_px)
{
	int bar_w = width / 8;

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int bar = x / bar_w;
			if (bar > 7)
				bar = 7;
			buf[y * stride_px + x] = bar_colors_rgb888[bar];
		}
	}
}

static void fill_bars_16(uint16_t *buf, int width, int height, int stride_px)
{
	int bar_w = width / 8;

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int bar = x / bar_w;
			if (bar > 7)
				bar = 7;
			buf[y * stride_px + x] =
				rgb888_to_rgb565(bar_colors_rgb888[bar]);
		}
	}
}

/* --- DRM path --- */

struct drm_buf {
	uint32_t fb_id;
	uint32_t handle;
	uint32_t size;
	uint32_t stride;
	uint32_t width;
	uint32_t height;
	void *map;
};

static int drm_create_fb(int fd, uint32_t width, uint32_t height,
			 struct drm_buf *buf)
{
	struct drm_mode_create_dumb creq = {
		.width = width,
		.height = height,
		.bpp = 32,
	};

	if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
		perror("DRM_IOCTL_MODE_CREATE_DUMB");
		return -1;
	}

	buf->handle = creq.handle;
	buf->stride = creq.pitch;
	buf->size = creq.size;
	buf->width = width;
	buf->height = height;

	if (drmModeAddFB(fd, width, height, 24, 32, creq.pitch,
			 creq.handle, &buf->fb_id)) {
		perror("drmModeAddFB");
		return -1;
	}

	struct drm_mode_map_dumb mreq = { .handle = creq.handle };

	if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq) < 0) {
		perror("DRM_IOCTL_MODE_MAP_DUMB");
		return -1;
	}

	buf->map = mmap(NULL, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED,
			fd, mreq.offset);
	if (buf->map == MAP_FAILED) {
		perror("mmap");
		return -1;
	}

	return 0;
}

static void drm_destroy_fb(int fd, struct drm_buf *buf)
{
	if (buf->map && buf->map != MAP_FAILED)
		munmap(buf->map, buf->size);

	drmModeRmFB(fd, buf->fb_id);

	struct drm_mode_destroy_dumb dreq = { .handle = buf->handle };
	drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
}

static int try_drm(void)
{
	int fd = open("/dev/dri/card0", O_RDWR);

	if (fd < 0) {
		printf("DRM: /dev/dri/card0 not available (%s)\n",
		       strerror(errno));
		return -1;
	}

	/* Check dumb buffer support */
	uint64_t has_dumb = 0;

	if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb) < 0 || !has_dumb) {
		printf("DRM: driver does not support dumb buffers\n");
		close(fd);
		return -1;
	}

	drmModeRes *res = drmModeGetResources(fd);

	if (!res) {
		printf("DRM: failed to get resources (%s)\n", strerror(errno));
		close(fd);
		return -1;
	}

	printf("DRM: %d connectors, %d CRTCs, %d encoders\n",
	       res->count_connectors, res->count_crtcs, res->count_encoders);

	/* Find first connected connector */
	drmModeConnector *conn = NULL;

	for (int i = 0; i < res->count_connectors; i++) {
		conn = drmModeGetConnector(fd, res->connectors[i]);
		if (!conn)
			continue;

		printf("  Connector %d: type=%d, status=%s, modes=%d\n",
		       conn->connector_id, conn->connector_type,
		       conn->connection == DRM_MODE_CONNECTED ? "connected" :
		       conn->connection == DRM_MODE_DISCONNECTED ?
		       "disconnected" : "unknown",
		       conn->count_modes);

		if (conn->connection == DRM_MODE_CONNECTED &&
		    conn->count_modes > 0)
			break;

		drmModeFreeConnector(conn);
		conn = NULL;
	}

	if (!conn) {
		printf("DRM: no connected connector with modes found\n");
		drmModeFreeResources(res);
		close(fd);
		return -1;
	}

	/* Use first (preferred) mode */
	drmModeModeInfo *mode = &conn->modes[0];

	printf("DRM: using mode %s (%dx%d @ %dHz)\n",
	       mode->name, mode->hdisplay, mode->vdisplay, mode->vrefresh);

	/* Find encoder and CRTC */
	drmModeEncoder *enc = NULL;

	if (conn->encoder_id)
		enc = drmModeGetEncoder(fd, conn->encoder_id);

	if (!enc) {
		/* Try first supported encoder */
		for (int i = 0; i < conn->count_encoders; i++) {
			enc = drmModeGetEncoder(fd, conn->encoders[i]);
			if (enc)
				break;
		}
	}

	if (!enc) {
		printf("DRM: no encoder found\n");
		drmModeFreeConnector(conn);
		drmModeFreeResources(res);
		close(fd);
		return -1;
	}

	uint32_t crtc_id = enc->crtc_id;

	if (!crtc_id) {
		/* Pick first available CRTC */
		for (int i = 0; i < res->count_crtcs; i++) {
			if (enc->possible_crtcs & (1 << i)) {
				crtc_id = res->crtcs[i];
				break;
			}
		}
	}

	drmModeFreeEncoder(enc);

	if (!crtc_id) {
		printf("DRM: no CRTC available\n");
		drmModeFreeConnector(conn);
		drmModeFreeResources(res);
		close(fd);
		return -1;
	}

	printf("DRM: CRTC %d, connector %d\n", crtc_id, conn->connector_id);

	/* Save original CRTC for restore */
	drmModeCrtc *saved_crtc = drmModeGetCrtc(fd, crtc_id);

	/* Create framebuffer */
	struct drm_buf buf = {0};

	if (drm_create_fb(fd, mode->hdisplay, mode->vdisplay, &buf) < 0) {
		drmModeFreeConnector(conn);
		drmModeFreeResources(res);
		close(fd);
		return -1;
	}

	/* Draw color bars */
	fill_bars_32(buf.map, buf.width, buf.height, buf.stride / 4);

	/* Set mode */
	if (drmModeSetCrtc(fd, crtc_id, buf.fb_id, 0, 0,
			   &conn->connector_id, 1, mode)) {
		perror("drmModeSetCrtc");
		drm_destroy_fb(fd, &buf);
		drmModeFreeConnector(conn);
		drmModeFreeResources(res);
		close(fd);
		return -1;
	}

	printf("DRM: color bars displayed — holding for %d seconds\n",
	       HOLD_SECONDS);
	sleep(HOLD_SECONDS);

	/* Restore original CRTC */
	if (saved_crtc) {
		drmModeSetCrtc(fd, saved_crtc->crtc_id, saved_crtc->buffer_id,
			       saved_crtc->x, saved_crtc->y,
			       &conn->connector_id, 1, &saved_crtc->mode);
		drmModeFreeCrtc(saved_crtc);
	}

	drm_destroy_fb(fd, &buf);
	drmModeFreeConnector(conn);
	drmModeFreeResources(res);
	close(fd);

	printf("DRM: done\n");
	return 0;
}

/* --- Framebuffer fallback --- */

static int try_fbdev(void)
{
	int fd = open("/dev/fb0", O_RDWR);

	if (fd < 0) {
		printf("fbdev: /dev/fb0 not available (%s)\n",
		       strerror(errno));
		return -1;
	}

	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;

	if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
		perror("FBIOGET_VSCREENINFO");
		close(fd);
		return -1;
	}

	if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
		perror("FBIOGET_FSCREENINFO");
		close(fd);
		return -1;
	}

	printf("fbdev: %dx%d, %d bpp, line_length=%d\n",
	       vinfo.xres, vinfo.yres, vinfo.bits_per_pixel, finfo.line_length);

	size_t fb_size = finfo.line_length * vinfo.yres;
	void *fb = mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED,
			fd, 0);

	if (fb == MAP_FAILED) {
		perror("mmap");
		close(fd);
		return -1;
	}

	if (vinfo.bits_per_pixel == 32) {
		fill_bars_32(fb, vinfo.xres, vinfo.yres,
			     finfo.line_length / 4);
	} else if (vinfo.bits_per_pixel == 16) {
		fill_bars_16(fb, vinfo.xres, vinfo.yres,
			     finfo.line_length / 2);
	} else {
		printf("fbdev: unsupported bpp %d\n", vinfo.bits_per_pixel);
		munmap(fb, fb_size);
		close(fd);
		return -1;
	}

	printf("fbdev: color bars displayed — holding for %d seconds\n",
	       HOLD_SECONDS);
	sleep(HOLD_SECONDS);

	munmap(fb, fb_size);
	close(fd);

	printf("fbdev: done\n");
	return 0;
}

int main(void)
{
	printf("Display test — Alif E7 CDC200 + ILI9806E (480x800)\n\n");

	if (try_drm() == 0)
		return 0;

	printf("\nDRM unavailable, trying framebuffer...\n\n");

	if (try_fbdev() == 0)
		return 0;

	printf("\nNo display interface available.\n");
	printf("Check kernel config: CONFIG_DRM_CDC200, CONFIG_FB\n");
	return 1;
}
