/*
 * WiFi Provisioning Credentials — NVS implementation for ESP-IDF.
 * Replaces Zephyr settings subsystem.
 */

#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#include <errno.h>
#include <string.h>

#include <wifi_prov/wifi_prov.h>

static const char *TAG = "wifi_prov_cred";

#define NVS_NAMESPACE "wifi_prov"
#define NVS_KEY_SSID  "ssid"
#define NVS_KEY_PSK   "psk"
#define NVS_KEY_SEC   "sec"

static struct wifi_prov_cred stored_cred;
static bool cred_loaded;

static void load_from_nvs(void)
{
	nvs_handle_t handle;

	if (cred_loaded) {
		return;
	}

	esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
	if (err != ESP_OK) {
		cred_loaded = true;
		return;
	}

	size_t len;

	len = sizeof(stored_cred.ssid);
	if (nvs_get_blob(handle, NVS_KEY_SSID, stored_cred.ssid, &len) == ESP_OK) {
		stored_cred.ssid_len = len;
	}

	len = sizeof(stored_cred.psk);
	if (nvs_get_blob(handle, NVS_KEY_PSK, stored_cred.psk, &len) == ESP_OK) {
		stored_cred.psk_len = len;
	}

	uint8_t sec;
	if (nvs_get_u8(handle, NVS_KEY_SEC, &sec) == ESP_OK) {
		stored_cred.security = sec;
	}

	nvs_close(handle);
	cred_loaded = true;

	if (stored_cred.ssid_len > 0) {
		ESP_LOGI(TAG, "Loaded stored credentials (SSID len=%u)",
			 stored_cred.ssid_len);
	}
}

int wifi_prov_cred_store(const struct wifi_prov_cred *cred)
{
	nvs_handle_t handle;

	if (!cred || cred->ssid_len == 0 ||
	    cred->ssid_len > WIFI_PROV_SSID_MAX_LEN) {
		return -EINVAL;
	}

	/* Update in-memory copy */
	memcpy(&stored_cred, cred, sizeof(stored_cred));
	cred_loaded = true;

	/* Persist to NVS */
	esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ESP_LOGW(TAG, "NVS open failed: %s (in-memory OK)",
			 esp_err_to_name(err));
		return 0;
	}

	nvs_set_blob(handle, NVS_KEY_SSID, cred->ssid, cred->ssid_len);
	nvs_set_blob(handle, NVS_KEY_PSK, cred->psk, cred->psk_len);
	nvs_set_u8(handle, NVS_KEY_SEC, cred->security);
	nvs_commit(handle);
	nvs_close(handle);

	ESP_LOGI(TAG, "Credentials stored (SSID len=%u)", cred->ssid_len);
	return 0;
}

int wifi_prov_cred_load(struct wifi_prov_cred *cred)
{
	if (!cred) {
		return -EINVAL;
	}

	load_from_nvs();

	if (stored_cred.ssid_len == 0) {
		return -ENOENT;
	}

	memcpy(cred, &stored_cred, sizeof(*cred));
	return 0;
}

int wifi_prov_cred_erase(void)
{
	nvs_handle_t handle;

	memset(&stored_cred, 0, sizeof(stored_cred));
	cred_loaded = true;

	esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
	if (err == ESP_OK) {
		nvs_erase_key(handle, NVS_KEY_SSID);
		nvs_erase_key(handle, NVS_KEY_PSK);
		nvs_erase_key(handle, NVS_KEY_SEC);
		nvs_commit(handle);
		nvs_close(handle);
	}

	ESP_LOGI(TAG, "Credentials erased");
	return 0;
}

bool wifi_prov_cred_exists(void)
{
	load_from_nvs();
	return stored_cred.ssid_len > 0;
}
