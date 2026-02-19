/*
 * eai_sensor types — enums, structs, backend dispatch
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_SENSOR_TYPES_H
#define EAI_SENSOR_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Sensor type ───────────────────────────────────────────────────────── */

enum eai_sensor_type {
	EAI_SENSOR_TYPE_ACCEL = 0,
	EAI_SENSOR_TYPE_GYRO,
	EAI_SENSOR_TYPE_MAG,
	EAI_SENSOR_TYPE_PRESSURE,
	EAI_SENSOR_TYPE_TEMPERATURE,
	EAI_SENSOR_TYPE_HUMIDITY,
	EAI_SENSOR_TYPE_LIGHT,
	EAI_SENSOR_TYPE_PROXIMITY,
};

/* ── Sensor data ───────────────────────────────────────────────────────── */

struct eai_sensor_data {
	uint8_t device_id;
	enum eai_sensor_type type;
	uint64_t timestamp_ns;
	union {
		struct {
			int32_t x;
			int32_t y;
			int32_t z;
		} vec3; /* mg, mdps, mgauss */
		int32_t scalar; /* mPa, m°C, m%RH, mlux, mm */
	};
};

/* ── Sensor device info ────────────────────────────────────────────────── */

#define EAI_SENSOR_NAME_MAX 32

struct eai_sensor_device {
	uint8_t id;
	char name[EAI_SENSOR_NAME_MAX];
	enum eai_sensor_type type;
	int32_t range_min;   /* milliunits */
	int32_t range_max;   /* milliunits */
	int32_t resolution;  /* milliunits per LSB */
	uint32_t max_rate_hz;
};

/* ── Session configuration ─────────────────────────────────────────────── */

struct eai_sensor_config {
	uint32_t rate_hz;
	uint32_t max_latency_ms;
};

/* ── Data callback ─────────────────────────────────────────────────────── */

typedef void (*eai_sensor_data_cb_t)(const struct eai_sensor_data *data,
				     void *user_data);

/* ── Backend type dispatch ─────────────────────────────────────────────── */

#if defined(CONFIG_EAI_SENSOR_BACKEND_ZEPHYR)
#include "../../src/zephyr/types.h"
#elif defined(CONFIG_EAI_SENSOR_BACKEND_FREERTOS)
#include "../../src/freertos/types.h"
#elif defined(CONFIG_EAI_SENSOR_BACKEND_POSIX)
#include "../../src/posix/types.h"
#else
#error "No EAI Sensor backend selected"
#endif

/* ── Sensor session ────────────────────────────────────────────────────── */

struct eai_sensor_session {
	uint8_t _backend[EAI_SENSOR_SESSION_BACKEND_SIZE];
	struct eai_sensor_config config;
	uint8_t device_id;
	eai_sensor_data_cb_t callback;
	void *user_data;
};

#ifdef __cplusplus
}
#endif

#endif /* EAI_SENSOR_TYPES_H */
