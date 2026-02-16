#ifndef EAI_OSAL_FREERTOS_TYPES_H
#define EAI_OSAL_FREERTOS_TYPES_H

/*
 * Internal header — included only via include/eai_osal/types.h.
 * eai_osal_timer_cb_t, eai_osal_thread_entry_t, and eai_osal_work_cb_t
 * are already typedef'd by the time this file is processed.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"

typedef struct { SemaphoreHandle_t _handle; } eai_osal_mutex_t;

typedef struct {
	SemaphoreHandle_t _handle;
	uint32_t _limit;
} eai_osal_sem_t;

typedef struct {
	TaskHandle_t _handle;
	SemaphoreHandle_t _join_sem;
	eai_osal_thread_entry_t _entry;
	void *_entry_arg;
} eai_osal_thread_t;

typedef struct {
	QueueHandle_t _handle;
} eai_osal_queue_t;

typedef struct {
	TimerHandle_t _handle;
	eai_osal_timer_cb_t _cb;
	void *_cb_arg;
	uint32_t _period_ms;
} eai_osal_timer_t;

typedef struct { EventGroupHandle_t _handle; } eai_osal_event_t;

typedef unsigned int eai_osal_critical_key_t;

/* Work item — submitted to a work queue (task + queue pattern) */
typedef struct {
	eai_osal_work_cb_t _cb;
	void *_cb_arg;
} eai_osal_work_t;

/* Delayed work — uses a timer to defer submission */
typedef struct {
	eai_osal_work_cb_t _cb;
	void *_cb_arg;
	TimerHandle_t _timer;
	void *_target_wq; /* eai_osal_workqueue_t*, NULL = system */
} eai_osal_dwork_t;

/* Work queue — task that processes {cb, arg} items from a FreeRTOS queue */
typedef struct {
	TaskHandle_t _task;
	QueueHandle_t _queue;
} eai_osal_workqueue_t;

/*
 * Thread stacks on FreeRTOS/ESP-IDF are allocated by the kernel.
 * These macros exist for API compatibility — the stack array is not
 * actually used. xTaskCreate allocates internally on ESP-IDF.
 */
#define EAI_OSAL_THREAD_STACK_DEFINE(name, size) \
	static uint8_t name[size] __attribute__((unused))
#define EAI_OSAL_THREAD_STACK_SIZEOF(name) sizeof(name)

#endif /* EAI_OSAL_FREERTOS_TYPES_H */
