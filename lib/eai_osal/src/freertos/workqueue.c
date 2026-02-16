#include <eai_osal/workqueue.h>
#include "internal.h"
#include <string.h>

/*
 * FreeRTOS work queue implementation.
 *
 * FreeRTOS has no native work queue. We implement it as:
 * - A FreeRTOS task that blocks on a queue
 * - Work items are {callback, arg} pairs sent to the queue
 * - Delayed work uses a FreeRTOS timer that enqueues on expiry
 *
 * The system work queue is lazily initialized on first use.
 */

#define WQ_DEPTH 16

struct wq_item {
	eai_osal_work_cb_t cb;
	void *arg;
};

/* ── System work queue (lazy init) ────────────────────────────────────── */

static eai_osal_workqueue_t sys_wq;
static bool sys_wq_ready;

static void wq_task(void *arg)
{
	eai_osal_workqueue_t *wq = (eai_osal_workqueue_t *)arg;
	struct wq_item item;

	for (;;) {
		if (xQueueReceive(wq->_queue, &item, portMAX_DELAY) == pdTRUE) {
			if (item.cb != NULL) {
				item.cb(item.arg);
			}
		}
	}
}

static eai_osal_workqueue_t *get_sys_wq(void)
{
	if (!sys_wq_ready) {
		sys_wq._queue = xQueueCreate(WQ_DEPTH, sizeof(struct wq_item));
		if (sys_wq._queue == NULL) {
			return NULL;
		}
		BaseType_t ret = xTaskCreate(wq_task, "sys_wq", 2048,
					     &sys_wq, tskIDLE_PRIORITY + 1,
					     &sys_wq._task);
		if (ret != pdPASS) {
			vQueueDelete(sys_wq._queue);
			sys_wq._queue = NULL;
			return NULL;
		}
		sys_wq_ready = true;
	}
	return &sys_wq;
}

/* ── Work item ────────────────────────────────────────────────────────── */

eai_osal_status_t eai_osal_work_init(eai_osal_work_t *work,
				     eai_osal_work_cb_t callback,
				     void *arg)
{
	if (work == NULL || callback == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	work->_cb = callback;
	work->_cb_arg = arg;
	return EAI_OSAL_OK;
}

static eai_osal_status_t submit_to_queue(QueueHandle_t q,
					 eai_osal_work_cb_t cb, void *arg)
{
	struct wq_item item = { .cb = cb, .arg = arg };

	if (xQueueSend(q, &item, 0) == pdTRUE) {
		return EAI_OSAL_OK;
	}
	return EAI_OSAL_ERROR;
}

eai_osal_status_t eai_osal_work_submit(eai_osal_work_t *work)
{
	if (work == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	eai_osal_workqueue_t *wq = get_sys_wq();

	if (wq == NULL) {
		return EAI_OSAL_ERROR;
	}
	return submit_to_queue(wq->_queue, work->_cb, work->_cb_arg);
}

eai_osal_status_t eai_osal_work_submit_to(eai_osal_work_t *work,
					  eai_osal_workqueue_t *wq)
{
	if (work == NULL || wq == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	return submit_to_queue(wq->_queue, work->_cb, work->_cb_arg);
}

/* ── Delayed work ─────────────────────────────────────────────────────── */

static void dwork_timer_cb(TimerHandle_t xTimer)
{
	eai_osal_dwork_t *dwork = (eai_osal_dwork_t *)pvTimerGetTimerID(xTimer);
	eai_osal_workqueue_t *wq = (eai_osal_workqueue_t *)dwork->_target_wq;

	if (wq == NULL) {
		wq = get_sys_wq();
	}
	if (wq != NULL) {
		submit_to_queue(wq->_queue, dwork->_cb, dwork->_cb_arg);
	}
}

eai_osal_status_t eai_osal_dwork_init(eai_osal_dwork_t *dwork,
				      eai_osal_work_cb_t callback,
				      void *arg)
{
	if (dwork == NULL || callback == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	dwork->_cb = callback;
	dwork->_cb_arg = arg;
	dwork->_target_wq = NULL;

	dwork->_timer = xTimerCreate("dwork",
				     pdMS_TO_TICKS(1000), /* placeholder */
				     pdFALSE,             /* one-shot */
				     dwork,               /* timer ID */
				     dwork_timer_cb);
	if (dwork->_timer == NULL) {
		return EAI_OSAL_NO_MEMORY;
	}
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_dwork_submit(eai_osal_dwork_t *dwork,
					uint32_t delay_ms)
{
	if (dwork == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	dwork->_target_wq = NULL;
	xTimerChangePeriod(dwork->_timer, pdMS_TO_TICKS(delay_ms), portMAX_DELAY);
	xTimerStart(dwork->_timer, portMAX_DELAY);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_dwork_submit_to(eai_osal_dwork_t *dwork,
					   eai_osal_workqueue_t *wq,
					   uint32_t delay_ms)
{
	if (dwork == NULL || wq == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	dwork->_target_wq = wq;
	xTimerChangePeriod(dwork->_timer, pdMS_TO_TICKS(delay_ms), portMAX_DELAY);
	xTimerStart(dwork->_timer, portMAX_DELAY);
	return EAI_OSAL_OK;
}

eai_osal_status_t eai_osal_dwork_cancel(eai_osal_dwork_t *dwork)
{
	if (dwork == NULL) {
		return EAI_OSAL_INVALID_PARAM;
	}
	if (xTimerStop(dwork->_timer, portMAX_DELAY) == pdTRUE) {
		return EAI_OSAL_OK;
	}
	return EAI_OSAL_ERROR;
}

/* ── Custom work queue ────────────────────────────────────────────────── */

eai_osal_status_t eai_osal_workqueue_create(eai_osal_workqueue_t *wq,
					    const char *name,
					    void *stack,
					    size_t stack_size,
					    uint8_t priority)
{
	if (wq == NULL || stack == NULL || stack_size == 0) {
		return EAI_OSAL_INVALID_PARAM;
	}

	wq->_queue = xQueueCreate(WQ_DEPTH, sizeof(struct wq_item));
	if (wq->_queue == NULL) {
		return EAI_OSAL_NO_MEMORY;
	}

	BaseType_t ret = xTaskCreate(wq_task,
				     name ? name : "wq",
				     stack_size / sizeof(StackType_t),
				     wq,
				     osal_priority(priority),
				     &wq->_task);
	if (ret != pdPASS) {
		vQueueDelete(wq->_queue);
		wq->_queue = NULL;
		return EAI_OSAL_NO_MEMORY;
	}

	return EAI_OSAL_OK;
}
