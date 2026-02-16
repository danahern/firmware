#include <eai_osal/mutex.h>
#include "internal.h"

eai_osal_status_t eai_osal_mutex_create(eai_osal_mutex_t *mutex)
{
	if (mutex == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	mutex->_handle = xSemaphoreCreateRecursiveMutex();
	if (mutex->_handle == NULL) {
		return EAI_OSAL_NO_MEMORY;
	}
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_mutex_destroy(eai_osal_mutex_t *mutex)
{
	if (mutex == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	if (mutex->_handle != NULL) {
		vSemaphoreDelete(mutex->_handle);
		mutex->_handle = NULL;
	}
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_mutex_lock(eai_osal_mutex_t *mutex, uint32_t timeout_ms)
{
	if (mutex == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	if (xSemaphoreTakeRecursive(mutex->_handle, osal_ticks(timeout_ms)) == pdTRUE) {
		return EAI_OSAL_OK;
	}
	return EAI_OSAL_TIMEOUT;
}

eai_osal_status_t eai_osal_mutex_unlock(eai_osal_mutex_t *mutex)
{
	if (mutex == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	if (xSemaphoreGiveRecursive(mutex->_handle) == pdTRUE) {
		return EAI_OSAL_OK;
	}
	return EAI_OSAL_ERROR;
}
