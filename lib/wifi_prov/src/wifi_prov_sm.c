#include <eai_log/eai_log.h>
#include <errno.h>

#include <wifi_prov/wifi_prov.h>

EAI_LOG_MODULE_REGISTER(wifi_prov, EAI_LOG_LEVEL_INF);

static enum wifi_prov_state current_state = WIFI_PROV_STATE_IDLE;
static wifi_prov_state_cb_t state_cb;

void wifi_prov_sm_init(wifi_prov_state_cb_t callback)
{
	current_state = WIFI_PROV_STATE_IDLE;
	state_cb = callback;
}

enum wifi_prov_state wifi_prov_sm_get_state(void)
{
	return current_state;
}

static int transition(enum wifi_prov_state new_state)
{
	enum wifi_prov_state old = current_state;

	current_state = new_state;
	EAI_LOG_INF("State: %d -> %d", old, new_state);

	if (state_cb) {
		state_cb(old, new_state);
	}

	return 0;
}

int wifi_prov_sm_process_event(enum wifi_prov_event event)
{
	/* Factory reset always goes to IDLE */
	if (event == WIFI_PROV_EVT_FACTORY_RESET) {
		return transition(WIFI_PROV_STATE_IDLE);
	}

	switch (current_state) {
	case WIFI_PROV_STATE_IDLE:
		if (event == WIFI_PROV_EVT_SCAN_TRIGGER) {
			return transition(WIFI_PROV_STATE_SCANNING);
		}
		if (event == WIFI_PROV_EVT_CREDENTIALS_RX) {
			return transition(WIFI_PROV_STATE_PROVISIONING);
		}
		break;

	case WIFI_PROV_STATE_SCANNING:
		if (event == WIFI_PROV_EVT_SCAN_DONE) {
			return transition(WIFI_PROV_STATE_SCAN_COMPLETE);
		}
		break;

	case WIFI_PROV_STATE_SCAN_COMPLETE:
		if (event == WIFI_PROV_EVT_CREDENTIALS_RX) {
			return transition(WIFI_PROV_STATE_PROVISIONING);
		}
		if (event == WIFI_PROV_EVT_SCAN_TRIGGER) {
			return transition(WIFI_PROV_STATE_SCANNING);
		}
		break;

	case WIFI_PROV_STATE_PROVISIONING:
		if (event == WIFI_PROV_EVT_WIFI_CONNECTING) {
			return transition(WIFI_PROV_STATE_CONNECTING);
		}
		break;

	case WIFI_PROV_STATE_CONNECTING:
		if (event == WIFI_PROV_EVT_WIFI_CONNECTED) {
			return transition(WIFI_PROV_STATE_CONNECTED);
		}
		if (event == WIFI_PROV_EVT_WIFI_FAILED) {
			return transition(WIFI_PROV_STATE_IDLE);
		}
		break;

	case WIFI_PROV_STATE_CONNECTED:
		if (event == WIFI_PROV_EVT_WIFI_DISCONNECTED) {
			return transition(WIFI_PROV_STATE_IDLE);
		}
		if (event == WIFI_PROV_EVT_SCAN_TRIGGER) {
			return transition(WIFI_PROV_STATE_SCANNING);
		}
		break;
	}

	EAI_LOG_WRN("Invalid transition: state=%d event=%d", current_state, event);
	return -EPERM;
}
