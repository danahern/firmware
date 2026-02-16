#include <eai_osal/thread.h>
#include "internal.h"

static void thread_trampoline(void *arg)
{
	eai_osal_thread_t *thread = (eai_osal_thread_t *)arg;

	thread->_entry(thread->_entry_arg);

	/* Signal join semaphore before self-deleting */
	if (thread->_join_sem != NULL) {
		xSemaphoreGive(thread->_join_sem);
	}
	vTaskDelete(NULL);
}

eai_osal_status_t eai_osal_thread_create(eai_osal_thread_t *thread,
					 const char *name,
					 eai_osal_thread_entry_t entry,
					 void *arg,
					 void *stack,
					 size_t stack_size,
					 uint8_t priority)
{
	if (thread == NULL || entry == NULL || stack == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	if (priority > 31) {
		return EAI_OSAL_INVALID_PARAM;
	}

	thread->_entry = entry;
	thread->_entry_arg = arg;

	/* Create join semaphore (binary, starts at 0) */
	thread->_join_sem = xSemaphoreCreateBinary();
	if (thread->_join_sem == NULL) {
		return EAI_OSAL_NO_MEMORY;
	}

	/*
	 * ESP-IDF xTaskCreate allocates the stack internally.
	 * The stack parameter is for API compatibility but the size
	 * (in bytes) is passed to FreeRTOS which converts to words.
	 */
	BaseType_t ret = xTaskCreate(thread_trampoline,
				     name ? name : "osal",
				     stack_size / sizeof(StackType_t),
				     thread,
				     osal_priority(priority),
				     &thread->_handle);
	if (ret != pdPASS) {
		vSemaphoreDelete(thread->_join_sem);
		thread->_join_sem = NULL;
		return EAI_OSAL_NO_MEMORY;
	}

	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_thread_join(eai_osal_thread_t *thread,
				       uint32_t timeout_ms)
{
	if (thread == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	if (thread->_join_sem == NULL) {
		return EAI_OSAL_ERROR;
	}
	if (xSemaphoreTake(thread->_join_sem, osal_ticks(timeout_ms)) == pdTRUE) {
		vSemaphoreDelete(thread->_join_sem);
		thread->_join_sem = NULL;
		return EAI_OSAL_OK;
	}
	return EAI_OSAL_TIMEOUT;
}

void eai_osal_thread_sleep(uint32_t ms)
{
	vTaskDelay(pdMS_TO_TICKS(ms));
}

void eai_osal_thread_yield(void)
{
	taskYIELD();
}
