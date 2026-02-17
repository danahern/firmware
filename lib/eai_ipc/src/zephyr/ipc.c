/*
 * eai_ipc Zephyr IPC Service backend
 *
 * Thin wrapper around Zephyr's IPC Service API (RPMsg/ICMsg).
 * Requires ipc0 node in devicetree.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <eai_ipc/eai_ipc.h>
#include <zephyr/ipc/ipc_service.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <string.h>
#include <errno.h>

/* ── Internal state ─────────────────────────────────────────────────────── */

struct zephyr_ept_data {
	struct ipc_ept ept;
	struct eai_ipc_cb cb;
	void *ctx;
	bool bound;
};

_Static_assert(sizeof(struct zephyr_ept_data) <= EAI_IPC_EPT_BACKEND_SIZE,
	       "EAI_IPC_EPT_BACKEND_SIZE too small for Zephyr backend");

static const struct device *ipc_instance;

/* ── IPC Service callbacks ──────────────────────────────────────────────── */

static void ept_bound_cb(void *priv)
{
	struct zephyr_ept_data *d = priv;
	d->bound = true;
	if (d->cb.bound) {
		d->cb.bound(d->ctx);
	}
}

static void ept_received_cb(const void *data, size_t len, void *priv)
{
	struct zephyr_ept_data *d = priv;
	if (d->cb.received) {
		d->cb.received(data, len, d->ctx);
	}
}

static struct ipc_ept_cfg make_ipc_cfg(const char *name,
				       struct zephyr_ept_data *d)
{
	struct ipc_ept_cfg cfg = {
		.name = name,
		.cb = {
			.bound = ept_bound_cb,
			.received = ept_received_cb,
		},
		.priv = d,
	};
	return cfg;
}

/* ── API implementation ─────────────────────────────────────────────────── */

int eai_ipc_init(void)
{
	ipc_instance = DEVICE_DT_GET(DT_NODELABEL(ipc0));
	if (!device_is_ready(ipc_instance)) {
		return -EIO;
	}
	return ipc_service_open_instance(ipc_instance);
}

int eai_ipc_deinit(void)
{
	ipc_instance = NULL;
	return 0;
}

int eai_ipc_register_endpoint(struct eai_ipc_endpoint *ept,
			      const struct eai_ipc_ept_cfg *cfg)
{
	if (!ept || !cfg || !cfg->name || cfg->name[0] == '\0') {
		return -EINVAL;
	}
	if (!ipc_instance) {
		return -EIO;
	}

	struct zephyr_ept_data *d = (struct zephyr_ept_data *)ept->_backend_data;
	memset(d, 0, sizeof(*d));
	d->cb = cfg->cb;
	d->ctx = cfg->ctx;

	struct ipc_ept_cfg ipc_cfg = make_ipc_cfg(cfg->name, d);
	return ipc_service_register_endpoint(ipc_instance, &d->ept, &ipc_cfg);
}

int eai_ipc_deregister_endpoint(struct eai_ipc_endpoint *ept)
{
	if (!ept) {
		return -EINVAL;
	}

	struct zephyr_ept_data *d = (struct zephyr_ept_data *)ept->_backend_data;
	int ret = ipc_service_deregister_endpoint(&d->ept);
	if (ret == 0) {
		d->bound = false;
	}
	return ret;
}

int eai_ipc_send(struct eai_ipc_endpoint *ept, const void *data, size_t len)
{
	if (!ept || !data || len == 0) {
		return -EINVAL;
	}

	struct zephyr_ept_data *d = (struct zephyr_ept_data *)ept->_backend_data;
	if (!d->bound) {
		return -ENOTCONN;
	}

	return ipc_service_send(&d->ept, data, len);
}

int eai_ipc_get_max_packet_size(void)
{
	/* RPMsg default payload: 512 - 16 (header) = 496 */
	return 496;
}
