#include <eai_osal/queue.h>
#include "internal.h"

eai_osal_status_t eai_osal_queue_create(eai_osal_queue_t *queue, size_t msg_size,
					uint32_t max_msgs, void *buffer)
{
	if (queue == NULL || buffer == NULL || msg_size == 0 || max_msgs == 0) {
		return EAI_OSAL_INVALID_PARAM;
	}
	/*
	 * FreeRTOS xQueueCreate allocates its own storage.
	 * The caller-provided buffer is for Zephyr API compat — not used here.
	 */
	queue->_handle = xQueueCreate(max_msgs, msg_size);
	if (queue->_handle == NULL) {
		return EAI_OSAL_NO_MEMORY;
	}
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_queue_destroy(eai_osal_queue_t *queue)
{
	if (queue == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	if (queue->_handle != NULL) {
		vQueueDelete(queue->_handle);
		queue->_handle = NULL;
	}
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_queue_send(eai_osal_queue_t *queue, const void *msg,
				      uint32_t timeout_ms)
{
	if (queue == NULL || msg == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	if (xQueueSend(queue->_handle, msg, osal_ticks(timeout_ms)) == pdTRUE) {
		return EAI_OSAL_OK;
	}
	return EAI_OSAL_TIMEOUT;
}

eai_osal_status_t eai_osal_queue_recv(eai_osal_queue_t *queue, void *msg,
				      uint32_t timeout_ms)
{
	if (queue == NULL || msg == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	if (xQueueReceive(queue->_handle, msg, osal_ticks(timeout_ms)) == pdTRUE) {
		return EAI_OSAL_OK;
	}
	return EAI_OSAL_TIMEOUT;
}
