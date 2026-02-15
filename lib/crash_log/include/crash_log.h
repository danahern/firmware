#ifndef CRASH_LOG_H
#define CRASH_LOG_H

#include <stdbool.h>

/**
 * @brief Check if a stored coredump exists in flash.
 * @return true if a valid coredump is stored.
 */
bool crash_log_has_coredump(void);

/**
 * @brief Erase the stored coredump from flash.
 * @return 0 on success, negative errno on failure.
 */
int crash_log_clear(void);

/**
 * @brief Output stored coredump as #CD: lines via LOG_ERR.
 *
 * Reads the stored coredump from flash and emits it through
 * the logging subsystem in the same #CD: hex format that Zephyr
 * uses at crash time. This makes it capturable via RTT and
 * parseable by the analyze_coredump MCP tool.
 *
 * @return 0 on success, negative errno on failure.
 */
int crash_log_emit(void);

#endif /* CRASH_LOG_H */
