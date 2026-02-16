#ifndef EAI_OSAL_FREERTOS_INTERNAL_H
#define EAI_OSAL_FREERTOS_INTERNAL_H

#include "freertos/FreeRTOS.h"
#include <eai_osal/types.h>

static inline TickType_t osal_ticks(uint32_t ms)
{
	if (ms == EAI_OSAL_WAIT_FOREVER) {
		return portMAX_DELAY;
	}
	return pdMS_TO_TICKS(ms);
}

/*
 * Map FreeRTOS priority range.
 * OSAL: 0-31, higher = higher priority.
 * FreeRTOS: 0 to (configMAX_PRIORITIES-1), higher = higher priority.
 * Same direction, but different range. Scale linearly.
 */
static inline UBaseType_t osal_priority(uint8_t prio)
{
	if (prio > 31) {
		prio = 31;
	}
	/* Map 0-31 → 1 to (configMAX_PRIORITIES-1), reserving 0 for idle */
	return 1 + (prio * (configMAX_PRIORITIES - 2)) / 31;
}

#endif /* EAI_OSAL_FREERTOS_INTERNAL_H */
