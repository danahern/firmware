#ifndef EAI_OSAL_TYPES_H
#define EAI_OSAL_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
	EAI_OSAL_OK            =  0,
	EAI_OSAL_ERROR         = -1,
	EAI_OSAL_TIMEOUT       = -2,
	EAI_OSAL_NO_MEMORY     = -3,
	EAI_OSAL_INVALID_PARAM = -4,
} eai_osal_status_t;

#define EAI_OSAL_WAIT_FOREVER UINT32_MAX
#define EAI_OSAL_NO_WAIT      0

/** Timer callback. Invoked from timer context (ISR on some backends). */
typedef void (*eai_osal_timer_cb_t)(void *arg);

/** Thread entry point. */
typedef void (*eai_osal_thread_entry_t)(void *arg);

/** Work item callback. Invoked from the work queue's thread context. */
typedef void (*eai_osal_work_cb_t)(void *arg);

/* Backend type dispatch — pulls in eai_osal_mutex_t, eai_osal_sem_t, etc. */
#if defined(CONFIG_EAI_OSAL_BACKEND_ZEPHYR)
#include "../../src/zephyr/types.h"
#elif defined(CONFIG_EAI_OSAL_BACKEND_FREERTOS)
#include "../../src/freertos/types.h"
#else
#error "No EAI OSAL backend selected"
#endif

#endif /* EAI_OSAL_TYPES_H */
