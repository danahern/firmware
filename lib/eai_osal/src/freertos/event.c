#include <eai_osal/event.h>
#include "internal.h"

eai_osal_status_t eai_osal_event_create(eai_osal_event_t *event)
{
	if (event == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	event->_handle = xEventGroupCreate();
	if (event->_handle == NULL) {
		return EAI_OSAL_NO_MEMORY;
	}
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_event_destroy(eai_osal_event_t *event)
{
	if (event == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	if (event->_handle != NULL) {
		vEventGroupDelete(event->_handle);
		event->_handle = NULL;
	}
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_event_set(eai_osal_event_t *event, uint32_t bits)
{
	if (event == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	xEventGroupSetBits(event->_handle, (EventBits_t)bits);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_event_wait(eai_osal_event_t *event, uint32_t bits,
				      bool wait_all, uint32_t *actual,
				      uint32_t timeout_ms)
{
	if (event == NULL || bits == 0) {
		return EAI_OSAL_INVALID_PARAM;
	}

	EventBits_t result = xEventGroupWaitBits(
		event->_handle,
		(EventBits_t)bits,
		pdFALSE, /* xClearOnExit = no auto-clear */
		wait_all ? pdTRUE : pdFALSE,
		osal_ticks(timeout_ms));

	/* Check if the requested bits are actually set */
	if (wait_all) {
		if ((result & bits) != bits) {
			return EAI_OSAL_TIMEOUT;
		}
	} else {
		if ((result & bits) == 0) {
			return EAI_OSAL_TIMEOUT;
		}
	}

	if (actual != NULL) {
		*actual = (uint32_t)(result & bits);
	}
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_event_clear(eai_osal_event_t *event, uint32_t bits)
{
	if (event == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	xEventGroupClearBits(event->_handle, (EventBits_t)bits);
	return EAI_OSAL_OK;
}
