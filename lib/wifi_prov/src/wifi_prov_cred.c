/*
 * WiFi Provisioning Credentials — Portable implementation using eai_settings.
 */

#include <eai_log/eai_log.h>
#include <eai_settings/eai_settings.h>
#include <string.h>
#include <errno.h>

#include <wifi_prov/wifi_prov.h>

EAI_LOG_MODULE_DECLARE(wifi_prov, EAI_LOG_LEVEL_INF);

static struct wifi_prov_cred stored_cred;
static bool cred_loaded;

static void load_from_storage(void)
{
	size_t actual;

	if (cred_loaded) {
		return;
	}

	cred_loaded = true;

	if (eai_settings_get("wifi_prov/ssid", stored_cred.ssid,
			     sizeof(stored_cred.ssid), &actual) == 0) {
		stored_cred.ssid_len = actual;
	}

	if (eai_settings_get("wifi_prov/psk", stored_cred.psk,
			     sizeof(stored_cred.psk), &actual) == 0) {
		stored_cred.psk_len = actual;
	}

	uint8_t sec;

	if (eai_settings_get("wifi_prov/sec", &sec, sizeof(sec),
			     &actual) == 0) {
		stored_cred.security = sec;
	}

	if (stored_cred.ssid_len > 0) {
		EAI_LOG_INF("Loaded stored credentials (SSID len=%u)",
			    stored_cred.ssid_len);
	}
}

int wifi_prov_cred_store(const struct wifi_prov_cred *cred)
{
	int ret;

	if (!cred || cred->ssid_len == 0 ||
	    cred->ssid_len > WIFI_PROV_SSID_MAX_LEN) {
		return -EINVAL;
	}

	/* Update in-memory copy */
	memcpy(&stored_cred, cred, sizeof(stored_cred));
	cred_loaded = true;

	/* Persist to storage */
	ret = eai_settings_set("wifi_prov/ssid", cred->ssid, cred->ssid_len);
	if (ret) {
		EAI_LOG_WRN("Failed to persist SSID: %d (in-memory OK)", ret);
	}

	ret = eai_settings_set("wifi_prov/psk", cred->psk, cred->psk_len);
	if (ret && cred->psk_len > 0) {
		EAI_LOG_WRN("Failed to persist PSK: %d (in-memory OK)", ret);
	}

	ret = eai_settings_set("wifi_prov/sec", &cred->security,
			       sizeof(cred->security));
	if (ret) {
		EAI_LOG_WRN("Failed to persist security: %d (in-memory OK)",
			    ret);
	}

	EAI_LOG_INF("Credentials stored (SSID len=%u)", cred->ssid_len);
	return 0;
}

int wifi_prov_cred_load(struct wifi_prov_cred *cred)
{
	if (!cred) {
		return -EINVAL;
	}

	load_from_storage();

	if (stored_cred.ssid_len == 0) {
		return -ENOENT;
	}

	memcpy(cred, &stored_cred, sizeof(*cred));
	return 0;
}

int wifi_prov_cred_erase(void)
{
	memset(&stored_cred, 0, sizeof(stored_cred));
	cred_loaded = true;

	/* Best-effort persistent delete */
	eai_settings_delete("wifi_prov/ssid");
	eai_settings_delete("wifi_prov/psk");
	eai_settings_delete("wifi_prov/sec");

	EAI_LOG_INF("Credentials erased");
	return 0;
}

bool wifi_prov_cred_exists(void)
{
	load_from_storage();
	return stored_cred.ssid_len > 0;
}
