/*
 * eai_sensor — Portable Sensor HAL
 *
 * Android ISensors/ISensorModule concepts mapped to embedded platforms.
 * Compile-time backend dispatch (Zephyr Sensor API, ESP-IDF I2C, POSIX stub).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_SENSOR_H
#define EAI_SENSOR_H

#include <eai_sensor/types.h>
#include <eai_sensor/device.h>
#include <eai_sensor/session.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the sensor subsystem.
 * Discovers sensor devices from the backend.
 *
 * @return 0 on success, negative errno on error.
 */
int eai_sensor_init(void);

/**
 * Deinitialize the sensor subsystem.
 * Closes all sessions and releases resources.
 *
 * @return 0 on success, negative errno on error.
 */
int eai_sensor_deinit(void);

/* ── Test helpers (POSIX backend only) ───────────────────────────────────── */

#if defined(CONFIG_EAI_SENSOR_BACKEND_POSIX) || defined(EAI_SENSOR_TEST)

/**
 * Inject sensor data for read-path testing.
 * Data is queued and returned by session_read() or delivered via callback.
 *
 * @param data   Sensor reading to inject.
 */
void eai_sensor_test_inject_data(const struct eai_sensor_data *data);

/**
 * Reset all POSIX test state (devices, sessions, buffers).
 */
void eai_sensor_test_reset(void);

#endif /* CONFIG_EAI_SENSOR_BACKEND_POSIX || EAI_SENSOR_TEST */

#ifdef __cplusplus
}
#endif

#endif /* EAI_SENSOR_H */
