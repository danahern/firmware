#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <wifi_prov/wifi_prov_msg.h>

int wifi_prov_msg_encode_scan_result(const struct wifi_prov_scan_result *result,
				     uint8_t *buf, size_t buf_size)
{
	if (!result || !buf) {
		return -EINVAL;
	}

	if (result->ssid_len > WIFI_PROV_SSID_MAX_LEN) {
		return -EINVAL;
	}

	/* ssid_len(1) + ssid(N) + rssi(1) + security(1) + channel(1) */
	size_t needed = 1 + result->ssid_len + 3;

	if (buf_size < needed) {
		return -ENOBUFS;
	}

	size_t pos = 0;

	buf[pos++] = result->ssid_len;
	memcpy(&buf[pos], result->ssid, result->ssid_len);
	pos += result->ssid_len;
	buf[pos++] = (uint8_t)result->rssi;
	buf[pos++] = result->security;
	buf[pos++] = result->channel;

	return (int)pos;
}

int wifi_prov_msg_decode_scan_result(const uint8_t *buf, size_t len,
				     struct wifi_prov_scan_result *result)
{
	if (!buf || !result) {
		return -EINVAL;
	}

	if (len < 1) {
		return -EINVAL;
	}

	uint8_t ssid_len = buf[0];

	if (ssid_len > WIFI_PROV_SSID_MAX_LEN) {
		return -EINVAL;
	}

	/* ssid_len(1) + ssid(N) + rssi(1) + security(1) + channel(1) */
	size_t needed = 1 + ssid_len + 3;

	if (len < needed) {
		return -EINVAL;
	}

	size_t pos = 0;

	result->ssid_len = buf[pos++];
	memcpy(result->ssid, &buf[pos], result->ssid_len);
	pos += result->ssid_len;
	result->rssi = (int8_t)buf[pos++];
	result->security = buf[pos++];
	result->channel = buf[pos++];

	return 0;
}

int wifi_prov_msg_encode_credentials(const struct wifi_prov_cred *cred,
				     uint8_t *buf, size_t buf_size)
{
	if (!cred || !buf) {
		return -EINVAL;
	}

	if (cred->ssid_len > WIFI_PROV_SSID_MAX_LEN ||
	    cred->psk_len > WIFI_PROV_PSK_MAX_LEN) {
		return -EINVAL;
	}

	/* ssid_len(1) + ssid(N) + psk_len(1) + psk(N) + security(1) */
	size_t needed = 1 + cred->ssid_len + 1 + cred->psk_len + 1;

	if (buf_size < needed) {
		return -ENOBUFS;
	}

	size_t pos = 0;

	buf[pos++] = cred->ssid_len;
	memcpy(&buf[pos], cred->ssid, cred->ssid_len);
	pos += cred->ssid_len;
	buf[pos++] = cred->psk_len;
	memcpy(&buf[pos], cred->psk, cred->psk_len);
	pos += cred->psk_len;
	buf[pos++] = cred->security;

	return (int)pos;
}

int wifi_prov_msg_decode_credentials(const uint8_t *buf, size_t len,
				     struct wifi_prov_cred *cred)
{
	if (!buf || !cred) {
		return -EINVAL;
	}

	if (len < 3) {
		return -EINVAL;
	}

	size_t pos = 0;
	uint8_t ssid_len = buf[pos];

	if (ssid_len > WIFI_PROV_SSID_MAX_LEN) {
		return -EINVAL;
	}

	/* Minimum: ssid_len(1) + ssid(N) + psk_len(1) + security(1) */
	if (len < 1 + ssid_len + 2) {
		return -EINVAL;
	}

	pos++;
	memcpy(cred->ssid, &buf[pos], ssid_len);
	cred->ssid_len = ssid_len;
	pos += ssid_len;

	uint8_t psk_len = buf[pos++];

	if (psk_len > WIFI_PROV_PSK_MAX_LEN) {
		return -EINVAL;
	}

	if (len < 1 + ssid_len + 1 + psk_len + 1) {
		return -EINVAL;
	}

	memcpy(cred->psk, &buf[pos], psk_len);
	cred->psk_len = psk_len;
	pos += psk_len;

	cred->security = buf[pos++];

	return 0;
}

int wifi_prov_msg_encode_status(enum wifi_prov_state state,
				const uint8_t ip_addr[4],
				uint8_t *buf, size_t buf_size)
{
	if (!buf) {
		return -EINVAL;
	}

	if (buf_size < 5) {
		return -ENOBUFS;
	}

	buf[0] = (uint8_t)state;

	if (ip_addr) {
		memcpy(&buf[1], ip_addr, 4);
	} else {
		memset(&buf[1], 0, 4);
	}

	return 5;
}

int wifi_prov_msg_decode_status(const uint8_t *buf, size_t len,
				enum wifi_prov_state *state,
				uint8_t ip_addr[4])
{
	if (!buf || !state || !ip_addr) {
		return -EINVAL;
	}

	if (len < 5) {
		return -EINVAL;
	}

	*state = (enum wifi_prov_state)buf[0];
	memcpy(ip_addr, &buf[1], 4);

	return 0;
}
