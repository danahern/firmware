/*
 * WiFi Provisioning Unit Tests — ESP-IDF / Unity
 *
 * Ports the 22 Zephyr ztest tests to Unity:
 *   8 message encode/decode (wifi_prov_msg)
 *   9 state machine transition (wifi_prov_sm)
 *   5 credential store (wifi_prov_cred via NVS)
 */

#include "unity.h"
#include "nvs_flash.h"

#include <string.h>
#include <errno.h>

#include <wifi_prov/wifi_prov_msg.h>
#include <wifi_prov/wifi_prov.h>

void setUp(void) {}
void tearDown(void) {}

/* ================================================================
 * Message Tests (8)
 * ================================================================ */

void test_msg_encode_decode_scan_result(void)
{
	struct wifi_prov_scan_result orig = {
		.ssid = "MyWiFi",
		.ssid_len = 6,
		.rssi = -42,
		.security = WIFI_PROV_SEC_WPA2_PSK,
		.channel = 6,
	};
	struct wifi_prov_scan_result decoded = {0};
	uint8_t buf[64];

	int len = wifi_prov_msg_encode_scan_result(&orig, buf, sizeof(buf));

	TEST_ASSERT_GREATER_THAN(0, len);
	TEST_ASSERT_EQUAL(0, wifi_prov_msg_decode_scan_result(buf, len, &decoded));
	TEST_ASSERT_EQUAL(6, decoded.ssid_len);
	TEST_ASSERT_EQUAL_MEMORY("MyWiFi", decoded.ssid, 6);
	TEST_ASSERT_EQUAL(-42, decoded.rssi);
	TEST_ASSERT_EQUAL(WIFI_PROV_SEC_WPA2_PSK, decoded.security);
	TEST_ASSERT_EQUAL(6, decoded.channel);
}

void test_msg_encode_decode_credentials(void)
{
	struct wifi_prov_cred orig = {
		.ssid = "HomeNet",
		.ssid_len = 7,
		.psk = "secret123",
		.psk_len = 9,
		.security = WIFI_PROV_SEC_WPA2_PSK,
	};
	struct wifi_prov_cred decoded = {0};
	uint8_t buf[128];

	int len = wifi_prov_msg_encode_credentials(&orig, buf, sizeof(buf));

	TEST_ASSERT_GREATER_THAN(0, len);
	TEST_ASSERT_EQUAL(0, wifi_prov_msg_decode_credentials(buf, len, &decoded));
	TEST_ASSERT_EQUAL(7, decoded.ssid_len);
	TEST_ASSERT_EQUAL_MEMORY("HomeNet", decoded.ssid, 7);
	TEST_ASSERT_EQUAL(9, decoded.psk_len);
	TEST_ASSERT_EQUAL_MEMORY("secret123", decoded.psk, 9);
	TEST_ASSERT_EQUAL(WIFI_PROV_SEC_WPA2_PSK, decoded.security);
}

void test_msg_encode_decode_status(void)
{
	uint8_t ip_orig[4] = {192, 168, 1, 42};
	uint8_t ip_decoded[4] = {0};
	enum wifi_prov_state state;
	uint8_t buf[8];

	int len = wifi_prov_msg_encode_status(WIFI_PROV_STATE_CONNECTED,
					      ip_orig, buf, sizeof(buf));

	TEST_ASSERT_EQUAL(5, len);
	TEST_ASSERT_EQUAL(0, wifi_prov_msg_decode_status(buf, len, &state,
							 ip_decoded));
	TEST_ASSERT_EQUAL(WIFI_PROV_STATE_CONNECTED, state);
	TEST_ASSERT_EQUAL_MEMORY(ip_orig, ip_decoded, 4);
}

void test_msg_decode_truncated_scan_result(void)
{
	/* Only 2 bytes — too short for any valid scan result */
	uint8_t buf[] = {6, 'A'};
	struct wifi_prov_scan_result result;

	TEST_ASSERT_EQUAL(-EINVAL,
			  wifi_prov_msg_decode_scan_result(buf, sizeof(buf),
							   &result));
}

void test_msg_decode_truncated_credentials(void)
{
	/* ssid_len=5 but only 3 bytes total */
	uint8_t buf[] = {5, 'A', 'B'};
	struct wifi_prov_cred cred;

	TEST_ASSERT_EQUAL(-EINVAL,
			  wifi_prov_msg_decode_credentials(buf, sizeof(buf),
							   &cred));
}

void test_msg_encode_buffer_too_small(void)
{
	struct wifi_prov_scan_result result = {
		.ssid = "Test",
		.ssid_len = 4,
		.rssi = -50,
		.security = 0,
		.channel = 1,
	};
	uint8_t buf[2]; /* Too small for 4 + 1 + 3 = 8 bytes */

	TEST_ASSERT_EQUAL(-ENOBUFS,
			  wifi_prov_msg_encode_scan_result(&result, buf,
							   sizeof(buf)));
}

void test_msg_max_length_ssid(void)
{
	struct wifi_prov_scan_result orig = {
		.ssid_len = WIFI_PROV_SSID_MAX_LEN,
		.rssi = -80,
		.security = WIFI_PROV_SEC_WPA3_SAE,
		.channel = 36,
	};
	struct wifi_prov_scan_result decoded = {0};
	uint8_t buf[64];

	memset(orig.ssid, 'X', WIFI_PROV_SSID_MAX_LEN);

	int len = wifi_prov_msg_encode_scan_result(&orig, buf, sizeof(buf));

	TEST_ASSERT_GREATER_THAN(0, len);
	TEST_ASSERT_EQUAL(0, wifi_prov_msg_decode_scan_result(buf, len,
							      &decoded));
	TEST_ASSERT_EQUAL(WIFI_PROV_SSID_MAX_LEN, decoded.ssid_len);
}

void test_msg_empty_psk(void)
{
	struct wifi_prov_cred orig = {
		.ssid = "OpenNet",
		.ssid_len = 7,
		.psk_len = 0,
		.security = WIFI_PROV_SEC_NONE,
	};
	struct wifi_prov_cred decoded = {0};
	uint8_t buf[64];

	int len = wifi_prov_msg_encode_credentials(&orig, buf, sizeof(buf));

	TEST_ASSERT_GREATER_THAN(0, len);
	TEST_ASSERT_EQUAL(0, wifi_prov_msg_decode_credentials(buf, len,
							      &decoded));
	TEST_ASSERT_EQUAL(0, decoded.psk_len);
	TEST_ASSERT_EQUAL(WIFI_PROV_SEC_NONE, decoded.security);
}

/* ================================================================
 * State Machine Tests (9)
 * ================================================================ */

static enum wifi_prov_state cb_old_state;
static enum wifi_prov_state cb_new_state;
static int cb_count;

static void test_state_callback(enum wifi_prov_state old_state,
				enum wifi_prov_state new_state)
{
	cb_old_state = old_state;
	cb_new_state = new_state;
	cb_count++;
}

static void sm_before(void)
{
	cb_count = 0;
	cb_old_state = WIFI_PROV_STATE_IDLE;
	cb_new_state = WIFI_PROV_STATE_IDLE;
	wifi_prov_sm_init(test_state_callback);
}

void test_sm_initial_state_is_idle(void)
{
	sm_before();
	TEST_ASSERT_EQUAL(WIFI_PROV_STATE_IDLE, wifi_prov_sm_get_state());
}

void test_sm_scan_flow(void)
{
	sm_before();
	TEST_ASSERT_EQUAL(0, wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_TRIGGER));
	TEST_ASSERT_EQUAL(WIFI_PROV_STATE_SCANNING, wifi_prov_sm_get_state());

	TEST_ASSERT_EQUAL(0, wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_DONE));
	TEST_ASSERT_EQUAL(WIFI_PROV_STATE_SCAN_COMPLETE, wifi_prov_sm_get_state());
}

void test_sm_provision_flow(void)
{
	sm_before();
	/* Get to SCAN_COMPLETE */
	wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_TRIGGER);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_DONE);

	TEST_ASSERT_EQUAL(0, wifi_prov_sm_process_event(WIFI_PROV_EVT_CREDENTIALS_RX));
	TEST_ASSERT_EQUAL(WIFI_PROV_STATE_PROVISIONING, wifi_prov_sm_get_state());

	TEST_ASSERT_EQUAL(0, wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_CONNECTING));
	TEST_ASSERT_EQUAL(WIFI_PROV_STATE_CONNECTING, wifi_prov_sm_get_state());

	TEST_ASSERT_EQUAL(0, wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_CONNECTED));
	TEST_ASSERT_EQUAL(WIFI_PROV_STATE_CONNECTED, wifi_prov_sm_get_state());
}

void test_sm_connection_failure(void)
{
	sm_before();
	/* Get to CONNECTING */
	wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_TRIGGER);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_DONE);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_CREDENTIALS_RX);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_CONNECTING);

	TEST_ASSERT_EQUAL(0, wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_FAILED));
	TEST_ASSERT_EQUAL(WIFI_PROV_STATE_IDLE, wifi_prov_sm_get_state());
}

void test_sm_disconnect(void)
{
	sm_before();
	/* Get to CONNECTED */
	wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_TRIGGER);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_DONE);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_CREDENTIALS_RX);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_CONNECTING);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_CONNECTED);

	TEST_ASSERT_EQUAL(0, wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_DISCONNECTED));
	TEST_ASSERT_EQUAL(WIFI_PROV_STATE_IDLE, wifi_prov_sm_get_state());
}

void test_sm_factory_reset_from_connected(void)
{
	sm_before();
	/* Get to CONNECTED */
	wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_TRIGGER);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_DONE);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_CREDENTIALS_RX);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_CONNECTING);
	wifi_prov_sm_process_event(WIFI_PROV_EVT_WIFI_CONNECTED);

	TEST_ASSERT_EQUAL(0, wifi_prov_sm_process_event(WIFI_PROV_EVT_FACTORY_RESET));
	TEST_ASSERT_EQUAL(WIFI_PROV_STATE_IDLE, wifi_prov_sm_get_state());
}

void test_sm_factory_reset_from_scanning(void)
{
	sm_before();
	wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_TRIGGER);
	TEST_ASSERT_EQUAL(WIFI_PROV_STATE_SCANNING, wifi_prov_sm_get_state());

	TEST_ASSERT_EQUAL(0, wifi_prov_sm_process_event(WIFI_PROV_EVT_FACTORY_RESET));
	TEST_ASSERT_EQUAL(WIFI_PROV_STATE_IDLE, wifi_prov_sm_get_state());
}

void test_sm_invalid_transition(void)
{
	sm_before();
	/* IDLE + SCAN_DONE should be invalid */
	int ret = wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_DONE);

	TEST_ASSERT_EQUAL(-EPERM, ret);
	TEST_ASSERT_EQUAL(WIFI_PROV_STATE_IDLE, wifi_prov_sm_get_state());
}

void test_sm_state_change_callback(void)
{
	sm_before();
	int initial_count = cb_count;

	wifi_prov_sm_process_event(WIFI_PROV_EVT_SCAN_TRIGGER);

	TEST_ASSERT_EQUAL(initial_count + 1, cb_count);
	TEST_ASSERT_EQUAL(WIFI_PROV_STATE_IDLE, cb_old_state);
	TEST_ASSERT_EQUAL(WIFI_PROV_STATE_SCANNING, cb_new_state);
}

/* ================================================================
 * Credential Tests (5)
 * ================================================================ */

static void cred_before(void)
{
	wifi_prov_cred_erase();
}

void test_cred_no_cred_on_clean_boot(void)
{
	cred_before();
	TEST_ASSERT_FALSE(wifi_prov_cred_exists());
}

void test_cred_store_and_load(void)
{
	cred_before();
	struct wifi_prov_cred cred = {
		.ssid = "TestNetwork",
		.ssid_len = 11,
		.psk = "password123",
		.psk_len = 11,
		.security = WIFI_PROV_SEC_WPA2_PSK,
	};
	struct wifi_prov_cred loaded = {0};

	TEST_ASSERT_EQUAL(0, wifi_prov_cred_store(&cred));
	TEST_ASSERT_TRUE(wifi_prov_cred_exists());
	TEST_ASSERT_EQUAL(0, wifi_prov_cred_load(&loaded));
	TEST_ASSERT_EQUAL(11, loaded.ssid_len);
	TEST_ASSERT_EQUAL_MEMORY("TestNetwork", loaded.ssid, 11);
	TEST_ASSERT_EQUAL(11, loaded.psk_len);
	TEST_ASSERT_EQUAL_MEMORY("password123", loaded.psk, 11);
	TEST_ASSERT_EQUAL(WIFI_PROV_SEC_WPA2_PSK, loaded.security);
}

void test_cred_erase(void)
{
	cred_before();
	struct wifi_prov_cred cred = {
		.ssid = "ToErase",
		.ssid_len = 7,
		.psk = "pass",
		.psk_len = 4,
		.security = WIFI_PROV_SEC_WPA_PSK,
	};

	TEST_ASSERT_EQUAL(0, wifi_prov_cred_store(&cred));
	TEST_ASSERT_TRUE(wifi_prov_cred_exists());
	TEST_ASSERT_EQUAL(0, wifi_prov_cred_erase());
	TEST_ASSERT_FALSE(wifi_prov_cred_exists());
}

void test_cred_load_when_empty(void)
{
	cred_before();
	struct wifi_prov_cred loaded = {0};
	int ret = wifi_prov_cred_load(&loaded);

	TEST_ASSERT_EQUAL(-ENOENT, ret);
}

void test_cred_overwrite(void)
{
	cred_before();
	struct wifi_prov_cred first = {
		.ssid = "First",
		.ssid_len = 5,
		.psk = "pass1",
		.psk_len = 5,
		.security = WIFI_PROV_SEC_WPA_PSK,
	};
	struct wifi_prov_cred second = {
		.ssid = "Second",
		.ssid_len = 6,
		.psk = "pass2",
		.psk_len = 5,
		.security = WIFI_PROV_SEC_WPA2_PSK,
	};
	struct wifi_prov_cred loaded = {0};

	TEST_ASSERT_EQUAL(0, wifi_prov_cred_store(&first));
	TEST_ASSERT_EQUAL(0, wifi_prov_cred_store(&second));
	TEST_ASSERT_EQUAL(0, wifi_prov_cred_load(&loaded));
	TEST_ASSERT_EQUAL(6, loaded.ssid_len);
	TEST_ASSERT_EQUAL_MEMORY("Second", loaded.ssid, 6);
}

/* ================================================================
 * Test Runner
 * ================================================================ */

void app_main(void)
{
	/* Initialize NVS for credential tests */
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
	    err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		nvs_flash_erase();
		err = nvs_flash_init();
	}
	assert(err == ESP_OK);
	wifi_prov_cred_erase();

	UNITY_BEGIN();

	/* Message encode/decode (8 tests) */
	RUN_TEST(test_msg_encode_decode_scan_result);
	RUN_TEST(test_msg_encode_decode_credentials);
	RUN_TEST(test_msg_encode_decode_status);
	RUN_TEST(test_msg_decode_truncated_scan_result);
	RUN_TEST(test_msg_decode_truncated_credentials);
	RUN_TEST(test_msg_encode_buffer_too_small);
	RUN_TEST(test_msg_max_length_ssid);
	RUN_TEST(test_msg_empty_psk);

	/* State machine transitions (9 tests) */
	RUN_TEST(test_sm_initial_state_is_idle);
	RUN_TEST(test_sm_scan_flow);
	RUN_TEST(test_sm_provision_flow);
	RUN_TEST(test_sm_connection_failure);
	RUN_TEST(test_sm_disconnect);
	RUN_TEST(test_sm_factory_reset_from_connected);
	RUN_TEST(test_sm_factory_reset_from_scanning);
	RUN_TEST(test_sm_invalid_transition);
	RUN_TEST(test_sm_state_change_callback);

	/* Credential store via NVS (5 tests) */
	RUN_TEST(test_cred_no_cred_on_clean_boot);
	RUN_TEST(test_cred_store_and_load);
	RUN_TEST(test_cred_erase);
	RUN_TEST(test_cred_load_when_empty);
	RUN_TEST(test_cred_overwrite);

	UNITY_END();
}
