/*
 * eai_ipc — Portable Inter-Processor Communication
 *
 * Endpoint-based messaging with backends for Zephyr IPC Service
 * (RPMsg/ICMsg) and an in-process loopback for testing.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EAI_IPC_H
#define EAI_IPC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Configuration defaults ─────────────────────────────────────────────── */

#ifndef EAI_IPC_EPT_NAME_MAX
#define EAI_IPC_EPT_NAME_MAX 32
#endif

/* Backend data size — must fit the backend's per-endpoint state */
#if defined(CONFIG_EAI_IPC_BACKEND_ZEPHYR)
#define EAI_IPC_EPT_BACKEND_SIZE 128
#elif defined(CONFIG_EAI_IPC_BACKEND_LOOPBACK) || defined(EAI_IPC_BACKEND_LOOPBACK)
#define EAI_IPC_EPT_BACKEND_SIZE 8
#else
#define EAI_IPC_EPT_BACKEND_SIZE 8
#endif

/* ── Types ──────────────────────────────────────────────────────────────── */

/** Callbacks for an IPC endpoint. */
struct eai_ipc_cb {
	/** Called when endpoint is bound to its peer. */
	void (*bound)(void *ctx);
	/** Called when data is received from peer. Must not block. */
	void (*received)(const void *data, size_t len, void *ctx);
};

/** Endpoint configuration — name must match on both cores. */
struct eai_ipc_ept_cfg {
	const char *name;
	struct eai_ipc_cb cb;
	void *ctx;
};

/** Opaque endpoint handle — user-allocated, backend fills internals. */
struct eai_ipc_endpoint {
	uint8_t _backend_data[EAI_IPC_EPT_BACKEND_SIZE];
};

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

/**
 * Initialize the IPC subsystem.
 *
 * @return 0 on success, negative errno on error.
 */
int eai_ipc_init(void);

/**
 * Deinitialize the IPC subsystem.
 *
 * @return 0 on success, negative errno on error.
 */
int eai_ipc_deinit(void);

/* ── Endpoints ──────────────────────────────────────────────────────────── */

/**
 * Register an endpoint. If a peer with the same name exists,
 * both endpoints' bound() callbacks fire.
 *
 * @param ept  User-allocated endpoint handle.
 * @param cfg  Endpoint configuration (name, callbacks, context).
 * @return 0 on success, -EINVAL if ept or cfg is NULL or name is empty,
 *         -ENOMEM if endpoint table is full.
 */
int eai_ipc_register_endpoint(struct eai_ipc_endpoint *ept,
			      const struct eai_ipc_ept_cfg *cfg);

/**
 * Deregister an endpoint.
 *
 * @param ept  Endpoint to deregister.
 * @return 0 on success, -EINVAL if ept is NULL, -ENOENT if not registered.
 */
int eai_ipc_deregister_endpoint(struct eai_ipc_endpoint *ept);

/* ── Data ───────────────────────────────────────────────────────────────── */

/**
 * Send data to the peer endpoint.
 *
 * @param ept   Endpoint to send from.
 * @param data  Pointer to data buffer.
 * @param len   Length of data in bytes.
 * @return 0 on success, -EINVAL if ept or data is NULL or len is 0,
 *         -ENOTCONN if endpoint is not bound, -EMSGSIZE if len exceeds max,
 *         -ENOENT if endpoint is not registered.
 */
int eai_ipc_send(struct eai_ipc_endpoint *ept, const void *data, size_t len);

/**
 * Get the maximum packet size supported by the backend.
 *
 * @return Maximum payload size in bytes.
 */
int eai_ipc_get_max_packet_size(void);

#ifdef __cplusplus
}
#endif

#endif /* EAI_IPC_H */
