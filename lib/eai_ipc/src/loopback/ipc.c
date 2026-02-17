/*
 * eai_ipc loopback backend
 *
 * In-process endpoint pairing by name. When endpoint A sends, peer B's
 * on_received fires synchronously. Used for unit testing on QEMU/native.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <eai_ipc/eai_ipc.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

/* ── Configuration ──────────────────────────────────────────────────────── */

#if defined(CONFIG_EAI_IPC_MAX_ENDPOINTS)
#define MAX_ENDPOINTS CONFIG_EAI_IPC_MAX_ENDPOINTS
#else
#define MAX_ENDPOINTS 8
#endif

#if defined(CONFIG_EAI_IPC_LOOPBACK_MAX_PACKET_SIZE)
#define MAX_PACKET_SIZE CONFIG_EAI_IPC_LOOPBACK_MAX_PACKET_SIZE
#else
#define MAX_PACKET_SIZE 496
#endif

/* ── Internal state ─────────────────────────────────────────────────────── */

struct loopback_ept {
	struct eai_ipc_endpoint *handle;
	char name[EAI_IPC_EPT_NAME_MAX];
	struct eai_ipc_cb cb;
	void *ctx;
	struct loopback_ept *peer;
	bool active;
};

static struct loopback_ept epts[MAX_ENDPOINTS];
static bool initialized;

/* ── Helpers ────────────────────────────────────────────────────────────── */

static struct loopback_ept *find_slot(void)
{
	for (int i = 0; i < MAX_ENDPOINTS; i++) {
		if (!epts[i].active) {
			return &epts[i];
		}
	}
	return NULL;
}

static struct loopback_ept *find_by_handle(struct eai_ipc_endpoint *handle)
{
	for (int i = 0; i < MAX_ENDPOINTS; i++) {
		if (epts[i].active && epts[i].handle == handle) {
			return &epts[i];
		}
	}
	return NULL;
}

static struct loopback_ept *find_unbound_peer(const char *name,
					      struct loopback_ept *self)
{
	for (int i = 0; i < MAX_ENDPOINTS; i++) {
		if (epts[i].active && &epts[i] != self &&
		    epts[i].peer == NULL &&
		    strcmp(epts[i].name, name) == 0) {
			return &epts[i];
		}
	}
	return NULL;
}

/* ── API implementation ─────────────────────────────────────────────────── */

int eai_ipc_init(void)
{
	memset(epts, 0, sizeof(epts));
	initialized = true;
	return 0;
}

int eai_ipc_deinit(void)
{
	memset(epts, 0, sizeof(epts));
	initialized = false;
	return 0;
}

int eai_ipc_register_endpoint(struct eai_ipc_endpoint *ept,
			      const struct eai_ipc_ept_cfg *cfg)
{
	if (!ept || !cfg || !cfg->name || cfg->name[0] == '\0') {
		return -EINVAL;
	}

	struct loopback_ept *slot = find_slot();
	if (!slot) {
		return -ENOMEM;
	}

	slot->handle = ept;
	strncpy(slot->name, cfg->name, EAI_IPC_EPT_NAME_MAX - 1);
	slot->name[EAI_IPC_EPT_NAME_MAX - 1] = '\0';
	slot->cb = cfg->cb;
	slot->ctx = cfg->ctx;
	slot->peer = NULL;
	slot->active = true;

	/* Store slot index in backend data for fast lookup */
	int idx = (int)(slot - epts);
	memcpy(ept->_backend_data, &idx, sizeof(idx));

	/* Try to find a peer with the same name */
	struct loopback_ept *peer = find_unbound_peer(slot->name, slot);
	if (peer) {
		slot->peer = peer;
		peer->peer = slot;

		/* Fire bound callbacks */
		if (peer->cb.bound) {
			peer->cb.bound(peer->ctx);
		}
		if (slot->cb.bound) {
			slot->cb.bound(slot->ctx);
		}
	}

	return 0;
}

int eai_ipc_deregister_endpoint(struct eai_ipc_endpoint *ept)
{
	if (!ept) {
		return -EINVAL;
	}

	struct loopback_ept *slot = find_by_handle(ept);
	if (!slot) {
		return -ENOENT;
	}

	/* Unbind peer */
	if (slot->peer) {
		slot->peer->peer = NULL;
	}

	memset(slot, 0, sizeof(*slot));
	return 0;
}

int eai_ipc_send(struct eai_ipc_endpoint *ept, const void *data, size_t len)
{
	if (!ept || !data || len == 0) {
		return -EINVAL;
	}

	if ((int)len > MAX_PACKET_SIZE) {
		return -EMSGSIZE;
	}

	struct loopback_ept *slot = find_by_handle(ept);
	if (!slot) {
		return -ENOENT;
	}

	if (!slot->peer) {
		return -ENOTCONN;
	}

	/* Deliver synchronously to peer's received callback */
	if (slot->peer->cb.received) {
		slot->peer->cb.received(data, len, slot->peer->ctx);
	}

	return 0;
}

int eai_ipc_get_max_packet_size(void)
{
	return MAX_PACKET_SIZE;
}
