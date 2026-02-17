/*
 * eai_ipc Zephyr tests — ztest (loopback backend)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <eai_ipc/eai_ipc.h>
#include <string.h>

/* ── Test helpers ───────────────────────────────────────────────────────── */

static int bound_count;
static int recv_count;
static uint8_t recv_buf[512];
static size_t recv_len;

static void on_bound(void *ctx)
{
	(void)ctx;
	bound_count++;
}

static void on_received(const void *data, size_t len, void *ctx)
{
	(void)ctx;
	recv_count++;
	recv_len = len;
	if (len <= sizeof(recv_buf)) {
		memcpy(recv_buf, data, len);
	}
}

static struct eai_ipc_cb test_cb = {
	.bound = on_bound,
	.received = on_received,
};

static void reset_state(void)
{
	bound_count = 0;
	recv_count = 0;
	recv_len = 0;
	memset(recv_buf, 0, sizeof(recv_buf));
}

/* ── Suite setup ────────────────────────────────────────────────────────── */

static void *ipc_setup(void)
{
	return NULL;
}

static void ipc_before(void *fixture)
{
	ARG_UNUSED(fixture);
	reset_state();
	eai_ipc_init();
}

static void ipc_after(void *fixture)
{
	ARG_UNUSED(fixture);
	eai_ipc_deinit();
}

ZTEST_SUITE(eai_ipc, NULL, ipc_setup, ipc_before, ipc_after, NULL);

/* ── Tests ──────────────────────────────────────────────────────────────── */

ZTEST(eai_ipc, test_init_deinit)
{
	zassert_equal(0, eai_ipc_deinit());
	zassert_equal(0, eai_ipc_init());
}

ZTEST(eai_ipc, test_register_endpoint)
{
	struct eai_ipc_endpoint ept;
	struct eai_ipc_ept_cfg cfg = {
		.name = "test",
		.cb = test_cb,
		.ctx = NULL,
	};

	zassert_equal(0, eai_ipc_register_endpoint(&ept, &cfg));
}

ZTEST(eai_ipc, test_register_null_config)
{
	struct eai_ipc_endpoint ept;

	zassert_equal(-EINVAL, eai_ipc_register_endpoint(&ept, NULL));
	zassert_equal(-EINVAL, eai_ipc_register_endpoint(NULL, NULL));
}

ZTEST(eai_ipc, test_register_empty_name)
{
	struct eai_ipc_endpoint ept;
	struct eai_ipc_ept_cfg cfg = {
		.name = "",
		.cb = test_cb,
		.ctx = NULL,
	};

	zassert_equal(-EINVAL, eai_ipc_register_endpoint(&ept, &cfg));
}

ZTEST(eai_ipc, test_register_paired_endpoints_bound)
{
	struct eai_ipc_endpoint ept_a, ept_b;
	struct eai_ipc_ept_cfg cfg = {
		.name = "chan1",
		.cb = test_cb,
		.ctx = NULL,
	};

	zassert_equal(0, eai_ipc_register_endpoint(&ept_a, &cfg));
	zassert_equal(0, bound_count);

	zassert_equal(0, eai_ipc_register_endpoint(&ept_b, &cfg));
	zassert_equal(2, bound_count);
}

ZTEST(eai_ipc, test_send_before_bound)
{
	struct eai_ipc_endpoint ept;
	struct eai_ipc_ept_cfg cfg = {
		.name = "lonely",
		.cb = test_cb,
		.ctx = NULL,
	};
	uint8_t data[] = {1, 2, 3};

	eai_ipc_register_endpoint(&ept, &cfg);
	zassert_equal(-ENOTCONN, eai_ipc_send(&ept, data, sizeof(data)));
}

ZTEST(eai_ipc, test_send_receive_a_to_b)
{
	struct eai_ipc_endpoint ept_a, ept_b;
	struct eai_ipc_cb cb_a = { .bound = on_bound, .received = NULL };
	struct eai_ipc_cb cb_b = { .bound = on_bound, .received = on_received };

	struct eai_ipc_ept_cfg cfg_a = { .name = "data", .cb = cb_a, .ctx = NULL };
	struct eai_ipc_ept_cfg cfg_b = { .name = "data", .cb = cb_b, .ctx = NULL };

	eai_ipc_register_endpoint(&ept_a, &cfg_a);
	eai_ipc_register_endpoint(&ept_b, &cfg_b);

	uint8_t msg[] = "hello";
	zassert_equal(0, eai_ipc_send(&ept_a, msg, sizeof(msg)));
	zassert_equal(1, recv_count);
	zassert_equal(sizeof(msg), recv_len);
	zassert_mem_equal(msg, recv_buf, sizeof(msg));
}

ZTEST(eai_ipc, test_send_receive_bidirectional)
{
	struct eai_ipc_endpoint ept_a, ept_b;
	struct eai_ipc_cb cb = { .bound = NULL, .received = on_received };

	struct eai_ipc_ept_cfg cfg_a = { .name = "bidir", .cb = cb, .ctx = NULL };
	struct eai_ipc_ept_cfg cfg_b = { .name = "bidir", .cb = cb, .ctx = NULL };

	eai_ipc_register_endpoint(&ept_a, &cfg_a);
	eai_ipc_register_endpoint(&ept_b, &cfg_b);

	/* A → B */
	uint8_t msg1[] = {0xAA};
	zassert_equal(0, eai_ipc_send(&ept_a, msg1, 1));
	zassert_equal(1, recv_count);

	/* B → A */
	uint8_t msg2[] = {0xBB};
	zassert_equal(0, eai_ipc_send(&ept_b, msg2, 1));
	zassert_equal(2, recv_count);
}

ZTEST(eai_ipc, test_send_null_data)
{
	struct eai_ipc_endpoint ept;
	struct eai_ipc_ept_cfg cfg = { .name = "x", .cb = test_cb, .ctx = NULL };
	eai_ipc_register_endpoint(&ept, &cfg);

	zassert_equal(-EINVAL, eai_ipc_send(&ept, NULL, 10));
}

ZTEST(eai_ipc, test_send_zero_len)
{
	struct eai_ipc_endpoint ept;
	struct eai_ipc_ept_cfg cfg = { .name = "x", .cb = test_cb, .ctx = NULL };
	eai_ipc_register_endpoint(&ept, &cfg);

	uint8_t data[] = {1};
	zassert_equal(-EINVAL, eai_ipc_send(&ept, data, 0));
}

ZTEST(eai_ipc, test_send_exceeds_max_packet_size)
{
	struct eai_ipc_endpoint ept_a, ept_b;
	struct eai_ipc_ept_cfg cfg = { .name = "big", .cb = test_cb, .ctx = NULL };
	eai_ipc_register_endpoint(&ept_a, &cfg);
	eai_ipc_register_endpoint(&ept_b, &cfg);

	int max = eai_ipc_get_max_packet_size();
	uint8_t big[512];
	memset(big, 0x42, sizeof(big));
	zassert_equal(-EMSGSIZE, eai_ipc_send(&ept_a, big, max + 1));
}

ZTEST(eai_ipc, test_deregister_endpoint)
{
	struct eai_ipc_endpoint ept;
	struct eai_ipc_ept_cfg cfg = { .name = "tmp", .cb = test_cb, .ctx = NULL };

	eai_ipc_register_endpoint(&ept, &cfg);
	zassert_equal(0, eai_ipc_deregister_endpoint(&ept));
}

ZTEST(eai_ipc, test_send_after_deregister)
{
	struct eai_ipc_endpoint ept_a, ept_b;
	struct eai_ipc_ept_cfg cfg = { .name = "gone", .cb = test_cb, .ctx = NULL };

	eai_ipc_register_endpoint(&ept_a, &cfg);
	eai_ipc_register_endpoint(&ept_b, &cfg);
	eai_ipc_deregister_endpoint(&ept_a);

	uint8_t data[] = {1};
	zassert_equal(-ENOENT, eai_ipc_send(&ept_a, data, sizeof(data)));
}

ZTEST(eai_ipc, test_get_max_packet_size)
{
	int max = eai_ipc_get_max_packet_size();
	zassert_true(max > 0);
	zassert_equal(496, max);
}
