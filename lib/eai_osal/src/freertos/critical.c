#include <eai_osal/critical.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*
 * ESP32 is dual-core — taskENTER_CRITICAL requires a spinlock (portMUX_TYPE).
 * A single static spinlock protects all OSAL critical sections.
 */
static portMUX_TYPE osal_spinlock = portMUX_INITIALIZER_UNLOCKED;

eai_osal_critical_key_t eai_osal_critical_enter(void)
{
	portENTER_CRITICAL(&osal_spinlock);
	return 0; /* key not used on ESP32 — spinlock handles nesting */
}

void eai_osal_critical_exit(eai_osal_critical_key_t key)
{
	(void)key;
	portEXIT_CRITICAL(&osal_spinlock);
}
