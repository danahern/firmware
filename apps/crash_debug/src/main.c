#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(crash_debug, LOG_LEVEL_INF);

/*
 * Crash debug demo app.
 *
 * Demonstrates automated crash analysis with the crash_log library:
 *   1. Boot — crash_log auto-checks for stored coredump (SYS_INIT)
 *   2. If previous crash found — emits #CD: data via RTT automatically
 *   3. After 5 seconds — triggers a NULL pointer write (MPU FAULT)
 *   4. Zephyr coredump subsystem stores crash to flash partition
 *   5. On next boot — step 1 detects and reports the crash
 *
 * Shell commands available via RTT:
 *   crash check  — check for stored crash
 *   crash info   — show crash PC, LR, fault reason
 *   crash dump   — output stored crash as #CD: lines
 *   crash clear  — erase stored crash
 *   board info   — board and firmware info
 *   board uptime — time since boot
 */

static void sensor_read_register(uint32_t reg_addr)
{
	volatile uint32_t *ptr = (volatile uint32_t *)0x0;

	LOG_INF("Reading sensor register 0x%x", reg_addr);
	*ptr = 0xDEAD; /* NULL pointer write -> HardFault */
}

static void sensor_process_data(const char *name)
{
	LOG_INF("Processing: %s", name);
	sensor_read_register(0xBEEF);
}

static void sensor_init_sequence(void)
{
	LOG_INF("Starting sensor init");
	sensor_process_data("accelerometer");
}

int main(void)
{
	LOG_INF("Crash debug app booted");
	LOG_INF("Shell available — try 'crash check' or 'board info'");
	LOG_INF("Crashing in 5 seconds...");

	k_sleep(K_SECONDS(5));

	LOG_INF("Triggering crash now!");
	sensor_init_sequence();

	return 0;
}
