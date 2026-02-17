/*
 * eai_ipc native tests — Unity (loopback backend)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity/unity.h"
#include <eai_ipc/eai_ipc.h>
#include <string.h>
#include <errno.h>

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

void setUp(void)
{
	bound_count = 0;
	recv_count = 0;
	recv_len = 0;
	memset(recv_buf, 0, sizeof(recv_buf));
	eai_ipc_init();
}

void tearDown(void)
{
	eai_ipc_deinit();
}

/* ── Tests ──────────────────────────────────────────────────────────────── */

void test_init_deinit(void)
{
	/* setUp already called init, just verify deinit + re-init works */
	TEST_ASSERT_EQUAL(0, eai_ipc_deinit());
	TEST_ASSERT_EQUAL(0, eai_ipc_init());
}

void test_register_endpoint(void)
{
	struct eai_ipc_endpoint ept;
	struct eai_ipc_ept_cfg cfg = {
		.name = "test",
		.cb = test_cb,
		.ctx = NULL,
	};

	TEST_ASSERT_EQUAL(0, eai_ipc_register_endpoint(&ept, &cfg));
}

void test_register_null_config(void)
{
	struct eai_ipc_endpoint ept;

	TEST_ASSERT_EQUAL(-EINVAL, eai_ipc_register_endpoint(&ept, NULL));
	TEST_ASSERT_EQUAL(-EINVAL, eai_ipc_register_endpoint(NULL, NULL));
}

void test_register_empty_name(void)
{
	struct eai_ipc_endpoint ept;
	struct eai_ipc_ept_cfg cfg = {
		.name = "",
		.cb = test_cb,
		.ctx = NULL,
	};

	TEST_ASSERT_EQUAL(-EINVAL, eai_ipc_register_endpoint(&ept, &cfg));
}

void test_register_paired_endpoints_bound(void)
{
	struct eai_ipc_endpoint ept_a, ept_b;
	struct eai_ipc_ept_cfg cfg = {
		.name = "chan1",
		.cb = test_cb,
		.ctx = NULL,
	};

	TEST_ASSERT_EQUAL(0, eai_ipc_register_endpoint(&ept_a, &cfg));
	TEST_ASSERT_EQUAL(0, bound_count);

	TEST_ASSERT_EQUAL(0, eai_ipc_register_endpoint(&ept_b, &cfg));
	TEST_ASSERT_EQUAL(2, bound_count);  /* Both endpoints get bound */
}

void test_send_before_bound(void)
{
	struct eai_ipc_endpoint ept;
	struct eai_ipc_ept_cfg cfg = {
		.name = "lonely",
		.cb = test_cb,
		.ctx = NULL,
	};
	uint8_t data[] = {1, 2, 3};

	eai_ipc_register_endpoint(&ept, &cfg);
	TEST_ASSERT_EQUAL(-ENOTCONN, eai_ipc_send(&ept, data, sizeof(data)));
}

void test_send_receive_a_to_b(void)
{
	struct eai_ipc_endpoint ept_a, ept_b;
	struct eai_ipc_cb cb_a = { .bound = on_bound, .received = NULL };
	struct eai_ipc_cb cb_b = { .bound = on_bound, .received = on_received };

	struct eai_ipc_ept_cfg cfg_a = { .name = "data", .cb = cb_a, .ctx = NULL };
	struct eai_ipc_ept_cfg cfg_b = { .name = "data", .cb = cb_b, .ctx = NULL };

	eai_ipc_register_endpoint(&ept_a, &cfg_a);
	eai_ipc_register_endpoint(&ept_b, &cfg_b);

	uint8_t msg[] = "hello";
	TEST_ASSERT_EQUAL(0, eai_ipc_send(&ept_a, msg, sizeof(msg)));
	TEST_ASSERT_EQUAL(1, recv_count);
	TEST_ASSERT_EQUAL(sizeof(msg), recv_len);
	TEST_ASSERT_EQUAL_MEMORY(msg, recv_buf, sizeof(msg));
}

void test_send_receive_bidirectional(void)
{
	/* Use context pointers to distinguish which endpoint received */
	static int a_recv, b_recv;
	a_recv = 0;
	b_recv = 0;

	struct eai_ipc_cb cb_a = {
		.bound = NULL,
		.received = on_received,
	};
	struct eai_ipc_cb cb_b = {
		.bound = NULL,
		.received = on_received,
	};

	struct eai_ipc_endpoint ept_a, ept_b;
	struct eai_ipc_ept_cfg cfg_a = { .name = "bidir", .cb = cb_a, .ctx = NULL };
	struct eai_ipc_ept_cfg cfg_b = { .name = "bidir", .cb = cb_b, .ctx = NULL };

	eai_ipc_register_endpoint(&ept_a, &cfg_a);
	eai_ipc_register_endpoint(&ept_b, &cfg_b);

	/* A → B */
	uint8_t msg1[] = {0xAA};
	TEST_ASSERT_EQUAL(0, eai_ipc_send(&ept_a, msg1, 1));
	TEST_ASSERT_EQUAL(1, recv_count);

	/* B → A */
	uint8_t msg2[] = {0xBB};
	TEST_ASSERT_EQUAL(0, eai_ipc_send(&ept_b, msg2, 1));
	TEST_ASSERT_EQUAL(2, recv_count);
}

void test_send_null_data(void)
{
	struct eai_ipc_endpoint ept;
	struct eai_ipc_ept_cfg cfg = { .name = "x", .cb = test_cb, .ctx = NULL };
	eai_ipc_register_endpoint(&ept, &cfg);

	TEST_ASSERT_EQUAL(-EINVAL, eai_ipc_send(&ept, NULL, 10));
}

void test_send_zero_len(void)
{
	struct eai_ipc_endpoint ept;
	struct eai_ipc_ept_cfg cfg = { .name = "x", .cb = test_cb, .ctx = NULL };
	eai_ipc_register_endpoint(&ept, &cfg);

	uint8_t data[] = {1};
	TEST_ASSERT_EQUAL(-EINVAL, eai_ipc_send(&ept, data, 0));
}

void test_send_exceeds_max_packet_size(void)
{
	struct eai_ipc_endpoint ept_a, ept_b;
	struct eai_ipc_ept_cfg cfg = { .name = "big", .cb = test_cb, .ctx = NULL };
	eai_ipc_register_endpoint(&ept_a, &cfg);
	eai_ipc_register_endpoint(&ept_b, &cfg);

	int max = eai_ipc_get_max_packet_size();
	uint8_t big[512];
	memset(big, 0x42, sizeof(big));
	TEST_ASSERT_EQUAL(-EMSGSIZE, eai_ipc_send(&ept_a, big, max + 1));
}

void test_deregister_endpoint(void)
{
	struct eai_ipc_endpoint ept;
	struct eai_ipc_ept_cfg cfg = { .name = "tmp", .cb = test_cb, .ctx = NULL };

	eai_ipc_register_endpoint(&ept, &cfg);
	TEST_ASSERT_EQUAL(0, eai_ipc_deregister_endpoint(&ept));
}

void test_send_after_deregister(void)
{
	struct eai_ipc_endpoint ept_a, ept_b;
	struct eai_ipc_ept_cfg cfg = { .name = "gone", .cb = test_cb, .ctx = NULL };

	eai_ipc_register_endpoint(&ept_a, &cfg);
	eai_ipc_register_endpoint(&ept_b, &cfg);
	eai_ipc_deregister_endpoint(&ept_a);

	uint8_t data[] = {1};
	TEST_ASSERT_EQUAL(-ENOENT, eai_ipc_send(&ept_a, data, sizeof(data)));
}

void test_get_max_packet_size(void)
{
	int max = eai_ipc_get_max_packet_size();
	TEST_ASSERT_GREATER_THAN(0, max);
	TEST_ASSERT_EQUAL(496, max);
}

/* ── Main ───────────────────────────────────────────────────────────────── */

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_init_deinit);
	RUN_TEST(test_register_endpoint);
	RUN_TEST(test_register_null_config);
	RUN_TEST(test_register_empty_name);
	RUN_TEST(test_register_paired_endpoints_bound);
	RUN_TEST(test_send_before_bound);
	RUN_TEST(test_send_receive_a_to_b);
	RUN_TEST(test_send_receive_bidirectional);
	RUN_TEST(test_send_null_data);
	RUN_TEST(test_send_zero_len);
	RUN_TEST(test_send_exceeds_max_packet_size);
	RUN_TEST(test_deregister_endpoint);
	RUN_TEST(test_send_after_deregister);
	RUN_TEST(test_get_max_packet_size);
	return UNITY_END();
}
