/*
 * WiFi Provisioning WiFi — Portable implementation using eai_wifi.
 */

#include <errno.h>
#include <string.h>

#include <eai_log/eai_log.h>
#include <eai_wifi/eai_wifi.h>

#include <wifi_prov/wifi_prov.h>

EAI_LOG_MODULE_DECLARE(wifi_prov, EAI_LOG_LEVEL_INF);

static wifi_prov_wifi_state_cb_t state_callback;
static wifi_prov_scan_result_cb_t scan_result_callback;
static void (*scan_done_cb)(void);

static void on_scan_result(const struct eai_wifi_scan_result *result)
{
	if (!scan_result_callback) {
		return;
	}

	struct wifi_prov_scan_result prov_result = {0};

	prov_result.ssid_len = result->ssid_len;
	memcpy(prov_result.ssid, result->ssid, result->ssid_len);
	prov_result.rssi = result->rssi;
	prov_result.channel = result->channel;

	switch (result->security) {
	case EAI_WIFI_SEC_OPEN:
		prov_result.security = WIFI_PROV_SEC_NONE;
		break;
	case EAI_WIFI_SEC_WPA_PSK:
		prov_result.security = WIFI_PROV_SEC_WPA_PSK;
		break;
	case EAI_WIFI_SEC_WPA3_SAE:
		prov_result.security = WIFI_PROV_SEC_WPA3_SAE;
		break;
	default:
		prov_result.security = WIFI_PROV_SEC_WPA2_PSK;
		break;
	}

	scan_result_callback(&prov_result);
}

static void on_scan_done(int status)
{
	EAI_LOG_INF("WiFi scan done (status %d)", status);
	scan_result_callback = NULL;
	if (scan_done_cb) {
		scan_done_cb();
	}
}

static void on_wifi_event(enum eai_wifi_event event)
{
	switch (event) {
	case EAI_WIFI_EVT_CONNECTED:
		EAI_LOG_INF("WiFi connected (IP obtained)");
		if (state_callback) {
			state_callback(true);
		}
		break;

	case EAI_WIFI_EVT_DISCONNECTED:
		EAI_LOG_INF("WiFi disconnected");
		if (state_callback) {
			state_callback(false);
		}
		break;

	case EAI_WIFI_EVT_CONNECT_FAILED:
		EAI_LOG_ERR("WiFi connection failed");
		if (state_callback) {
			state_callback(false);
		}
		break;
	}
}

int wifi_prov_wifi_init(wifi_prov_wifi_state_cb_t state_cb)
{
	state_callback = state_cb;

	int ret = eai_wifi_init();

	if (ret) {
		EAI_LOG_ERR("WiFi init failed: %d", ret);
		return ret;
	}

	eai_wifi_set_event_callback(on_wifi_event);

	EAI_LOG_INF("WiFi manager initialized");
	return 0;
}

void wifi_prov_wifi_set_scan_done_cb(void (*done_cb)(void))
{
	scan_done_cb = done_cb;
}

int wifi_prov_wifi_scan(wifi_prov_scan_result_cb_t result_cb)
{
	scan_result_callback = result_cb;

	int ret = eai_wifi_scan(on_scan_result, on_scan_done);

	if (ret) {
		EAI_LOG_ERR("WiFi scan request failed: %d", ret);
		scan_result_callback = NULL;
		return ret;
	}

	EAI_LOG_INF("WiFi scan started");
	return 0;
}

static enum eai_wifi_security map_security(uint8_t sec)
{
	switch (sec) {
	case WIFI_PROV_SEC_NONE:
		return EAI_WIFI_SEC_OPEN;
	case WIFI_PROV_SEC_WPA_PSK:
		return EAI_WIFI_SEC_WPA_PSK;
	case WIFI_PROV_SEC_WPA3_SAE:
		return EAI_WIFI_SEC_WPA3_SAE;
	default:
		return EAI_WIFI_SEC_WPA2_PSK;
	}
}

int wifi_prov_wifi_connect(const struct wifi_prov_cred *cred)
{
	if (!cred) {
		return -EINVAL;
	}

	EAI_LOG_INF("Connecting to WiFi (SSID len=%u)", cred->ssid_len);

	return eai_wifi_connect(cred->ssid, cred->ssid_len,
				cred->psk, cred->psk_len,
				map_security(cred->security));
}

int wifi_prov_wifi_disconnect(void)
{
	return eai_wifi_disconnect();
}

int wifi_prov_wifi_get_ip(uint8_t ip_addr[4])
{
	return eai_wifi_get_ip(ip_addr);
}

bool wifi_prov_wifi_is_connected(void)
{
	return eai_wifi_get_state() == EAI_WIFI_STATE_CONNECTED;
}
