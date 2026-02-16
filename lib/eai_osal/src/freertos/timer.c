#include <eai_osal/timer.h>
#include "internal.h"

static void timer_trampoline(TimerHandle_t xTimer)
{
	eai_osal_timer_t *timer = (eai_osal_timer_t *)pvTimerGetTimerID(xTimer);

	if (timer->_cb != NULL) {
		timer->_cb(timer->_cb_arg);
	}
}

eai_osal_status_t eai_osal_timer_create(eai_osal_timer_t *timer,
					eai_osal_timer_cb_t callback,
					void *arg)
{
	if (timer == NULL || callback == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	timer->_cb = callback;
	timer->_cb_arg = arg;
	timer->_period_ms = 0;

	/* Create as one-shot; period set on start */
	timer->_handle = xTimerCreate("osal",
				      pdMS_TO_TICKS(1000), /* placeholder */
				      pdFALSE,             /* one-shot */
				      timer,               /* timer ID = our struct */
				      timer_trampoline);
	if (timer->_handle == NULL) {
		return EAI_OSAL_NO_MEMORY;
	}
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_timer_destroy(eai_osal_timer_t *timer)
{
	if (timer == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	if (timer->_handle != NULL) {
		xTimerDelete(timer->_handle, portMAX_DELAY);
		timer->_handle = NULL;
	}
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_timer_start(eai_osal_timer_t *timer,
				       uint32_t initial_ms,
				       uint32_t period_ms)
{
	if (timer == NULL || initial_ms == 0) {
		return EAI_OSAL_INVALID_PARAM;
	}

	timer->_period_ms = period_ms;

	/* Set the period to initial_ms for the first fire */
	xTimerChangePeriod(timer->_handle, pdMS_TO_TICKS(initial_ms), portMAX_DELAY);

	/* Set auto-reload based on whether this is periodic */
	if (period_ms > 0) {
		vTimerSetReloadMode(timer->_handle, pdTRUE);
	} else {
		vTimerSetReloadMode(timer->_handle, pdFALSE);
	}

	xTimerStart(timer->_handle, portMAX_DELAY);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_timer_stop(eai_osal_timer_t *timer)
{
	if (timer == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	xTimerStop(timer->_handle, portMAX_DELAY);
	return EAI_OSAL_OK;
}

bool eai_osal_timer_is_running(eai_osal_timer_t *timer)
{
	if (timer == NULL || timer->_handle == NULL) {
		return false;
	}
	return xTimerIsTimerActive(timer->_handle) != pdFALSE;
}
