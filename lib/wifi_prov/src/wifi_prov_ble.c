/*
 * WiFi Provisioning BLE — Portable implementation using eai_ble.
 * Declarative GATT service with 5 characteristics.
 */

#include <errno.h>
#include <string.h>

#include <eai_log/eai_log.h>
#include <eai_ble/eai_ble.h>

#include <wifi_prov/wifi_prov.h>
#include <wifi_prov/wifi_prov_msg.h>

EAI_LOG_MODULE_DECLARE(wifi_prov, EAI_LOG_LEVEL_INF);

/* Callbacks set by wifi_prov.c */
static void (*scan_trigger_cb)(void);
static void (*credentials_rx_cb)(const struct wifi_prov_cred *cred);
static void (*factory_reset_cb)(void);

/* Characteristic indices */
#define CHAR_SCAN_TRIGGER 0
#define CHAR_SCAN_RESULTS 1
#define CHAR_CREDENTIALS  2
#define CHAR_STATUS       3
#define CHAR_FACTORY_RESET 4

/* --- GATT callbacks --- */

static void on_write(uint8_t char_index, const uint8_t *data, uint16_t len)
{
	switch (char_index) {
	case CHAR_SCAN_TRIGGER:
		EAI_LOG_INF("BLE: scan trigger received");
		if (scan_trigger_cb) {
			scan_trigger_cb();
		}
		break;

	case CHAR_CREDENTIALS: {
		struct wifi_prov_cred cred = {0};

		if (wifi_prov_msg_decode_credentials(data, len, &cred) < 0) {
			EAI_LOG_ERR("BLE: invalid credentials message");
			return;
		}
		EAI_LOG_INF("BLE: credentials received (SSID len=%u)",
			    cred.ssid_len);
		if (credentials_rx_cb) {
			credentials_rx_cb(&cred);
		}
		break;
	}

	case CHAR_FACTORY_RESET:
		if (len < 1 || data[0] != 0xFF) {
			return;
		}
		EAI_LOG_INF("BLE: factory reset triggered");
		if (factory_reset_cb) {
			factory_reset_cb();
		}
		break;

	default:
		break;
	}
}

static int on_read(uint8_t char_index, uint8_t *buf, uint16_t *len)
{
	if (char_index != CHAR_STATUS) {
		return -EINVAL;
	}

	uint8_t ip[4] = {0};

	wifi_prov_get_ip(ip);
	wifi_prov_msg_encode_status(wifi_prov_get_state(), ip, buf, 5);
	*len = 5;

	return 0;
}

static void on_ble_disconnect(void)
{
	eai_ble_adv_start(NULL);
}

/* UUID base: a0e4f2b0-XXXX-4c9a-b000-d0e6a7b8c9d0 */
#define WIFI_PROV_UUID(suffix) \
	EAI_BLE_UUID128_INIT(0xa0e4f2b0, suffix, 0x4c9a, 0xb000, \
			     0xd0e6a7b8c9d0ULL)

static const struct eai_ble_char chars[] = {
	[CHAR_SCAN_TRIGGER] = {
		.uuid = WIFI_PROV_UUID(0x0002),
		.properties = EAI_BLE_PROP_WRITE,
		.on_write = on_write,
	},
	[CHAR_SCAN_RESULTS] = {
		.uuid = WIFI_PROV_UUID(0x0003),
		.properties = EAI_BLE_PROP_NOTIFY,
	},
	[CHAR_CREDENTIALS] = {
		.uuid = WIFI_PROV_UUID(0x0004),
		.properties = EAI_BLE_PROP_WRITE,
		.on_write = on_write,
	},
	[CHAR_STATUS] = {
		.uuid = WIFI_PROV_UUID(0x0005),
		.properties = EAI_BLE_PROP_READ | EAI_BLE_PROP_NOTIFY,
		.on_read = on_read,
		.on_write = NULL,
	},
	[CHAR_FACTORY_RESET] = {
		.uuid = WIFI_PROV_UUID(0x0006),
		.properties = EAI_BLE_PROP_WRITE,
		.on_write = on_write,
	},
};

static const struct eai_ble_service svc = {
	.uuid = WIFI_PROV_UUID(0x0001),
	.chars = chars,
	.char_count = 5,
};

/* --- Public API --- */

void wifi_prov_ble_set_callbacks(void (*on_scan_trigger)(void),
				 void (*on_credentials)(const struct wifi_prov_cred *),
				 void (*on_factory_reset)(void))
{
	scan_trigger_cb = on_scan_trigger;
	credentials_rx_cb = on_credentials;
	factory_reset_cb = on_factory_reset;
}

int wifi_prov_ble_init(void)
{
	int ret;
	static const struct eai_ble_callbacks cbs = {
		.on_disconnect = on_ble_disconnect,
	};

	ret = eai_ble_init(&cbs);
	if (ret) {
		EAI_LOG_ERR("BLE init failed: %d", ret);
		return ret;
	}

	ret = eai_ble_gatt_register(&svc);
	if (ret) {
		EAI_LOG_ERR("BLE GATT register failed: %d", ret);
		return ret;
	}

	EAI_LOG_INF("BLE initialized");
	return 0;
}

int wifi_prov_ble_start_advertising(void)
{
	int ret = eai_ble_adv_start(NULL);

	if (ret) {
		EAI_LOG_WRN("Advertising start skipped (err %d)", ret);
		return ret;
	}

	EAI_LOG_INF("BLE advertising started");
	return 0;
}

int wifi_prov_ble_notify_scan_result(const struct wifi_prov_scan_result *result)
{
	uint8_t buf[64];
	int len;

	if (!eai_ble_is_connected()) {
		return -ENOTCONN;
	}

	len = wifi_prov_msg_encode_scan_result(result, buf, sizeof(buf));
	if (len < 0) {
		return len;
	}

	return eai_ble_notify(CHAR_SCAN_RESULTS, buf, len);
}

int wifi_prov_ble_notify_status(enum wifi_prov_state state,
				const uint8_t ip[4])
{
	uint8_t buf[5];

	if (!eai_ble_is_connected()) {
		return -ENOTCONN;
	}

	wifi_prov_msg_encode_status(state, ip, buf, sizeof(buf));

	return eai_ble_notify(CHAR_STATUS, buf, sizeof(buf));
}
