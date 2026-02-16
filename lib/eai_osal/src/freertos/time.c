#include <eai_osal/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

uint32_t eai_osal_time_get_ms(void)
{
	return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

uint64_t eai_osal_time_get_ticks(void)
{
	return (uint64_t)xTaskGetTickCount();
}

uint32_t eai_osal_time_ticks_to_ms(uint64_t ticks)
{
	return (uint32_t)(ticks * portTICK_PERIOD_MS);
}
