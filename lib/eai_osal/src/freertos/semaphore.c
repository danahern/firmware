#include <eai_osal/semaphore.h>
#include "internal.h"

eai_osal_status_t eai_osal_sem_create(eai_osal_sem_t *sem, uint32_t initial,
				      uint32_t limit)
{
	if (sem == NULL || limit == 0) {
		return EAI_OSAL_INVALID_PARAM;
	}
	sem->_limit = limit;
	sem->_handle = xSemaphoreCreateCounting(limit, initial);
	if (sem->_handle == NULL) {
		return EAI_OSAL_NO_MEMORY;
	}
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_sem_destroy(eai_osal_sem_t *sem)
{
	if (sem == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	if (sem->_handle != NULL) {
		vSemaphoreDelete(sem->_handle);
		sem->_handle = NULL;
	}
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_sem_give(eai_osal_sem_t *sem)
{
	if (sem == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	if (xSemaphoreGive(sem->_handle) == pdTRUE) {
		return EAI_OSAL_OK;
	}
	/* Semaphore at limit — give failed */
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_sem_take(eai_osal_sem_t *sem, uint32_t timeout_ms)
{
	if (sem == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	if (xSemaphoreTake(sem->_handle, osal_ticks(timeout_ms)) == pdTRUE) {
		return EAI_OSAL_OK;
	}
	return EAI_OSAL_TIMEOUT;
}
