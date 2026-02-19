/*
 * eai_sensor session lifecycle + I/O
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_SENSOR_SESSION_H
#define EAI_SENSOR_SESSION_H

#include <eai_sensor/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Open a sensor session on a device.
 *
 * @param session    Session to initialize (caller-allocated).
 * @param device_id  Target device ID.
 * @param config     Desired session configuration.
 * @return 0 on success, -EINVAL if args invalid, -ENODEV if device not found,
 *         -EBUSY if device already has an active session.
 */
int eai_sensor_session_open(struct eai_sensor_session *session,
			    uint8_t device_id,
			    const struct eai_sensor_config *config);

/**
 * Start sensor data delivery.
 *
 * @param session    Opened session.
 * @param callback   Data callback (NULL for polling-only mode).
 * @param user_data  Context pointer passed to callback.
 * @return 0 on success, -EINVAL if session is NULL or not opened.
 */
int eai_sensor_session_start(struct eai_sensor_session *session,
			     eai_sensor_data_cb_t callback,
			     void *user_data);

/**
 * Read sensor data (polling mode).
 *
 * @param session     Started session.
 * @param data        Output data buffer.
 * @param count       Maximum number of readings to return.
 * @param timeout_ms  Maximum wait time (0 = non-blocking).
 * @return Number of readings on success, negative errno on error.
 *         -EINVAL if args invalid.
 */
int eai_sensor_session_read(struct eai_sensor_session *session,
			    struct eai_sensor_data *data,
			    uint32_t count, uint32_t timeout_ms);

/**
 * Flush buffered sensor data. Triggers delivery of any pending readings.
 *
 * @param session  Started session.
 * @return 0 on success, -EINVAL if session is NULL.
 */
int eai_sensor_session_flush(struct eai_sensor_session *session);

/**
 * Stop sensor data delivery.
 *
 * @param session  Started session.
 * @return 0 on success, -EINVAL if session is NULL.
 */
int eai_sensor_session_stop(struct eai_sensor_session *session);

/**
 * Close a sensor session and release resources.
 *
 * @param session  Session to close.
 * @return 0 on success, -EINVAL if session is NULL.
 */
int eai_sensor_session_close(struct eai_sensor_session *session);

#ifdef __cplusplus
}
#endif

#endif /* EAI_SENSOR_SESSION_H */
