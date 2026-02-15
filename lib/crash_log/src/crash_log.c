#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/debug/coredump.h>
#include <zephyr/logging/log.h>
#include <crash_log.h>

LOG_MODULE_REGISTER(crash_log, LOG_LEVEL_INF);

#define COPY_BUF_SZ  128
#define HEX_LINE_SZ  64  /* hex chars per #CD: line */

bool crash_log_has_coredump(void)
{
	return coredump_query(COREDUMP_QUERY_HAS_STORED_DUMP, NULL) == 1;
}

int crash_log_clear(void)
{
	return coredump_cmd(COREDUMP_CMD_ERASE_STORED_DUMP, NULL);
}

int crash_log_emit(void)
{
	int size;
	uint8_t buf[COPY_BUF_SZ];
	char hex_line[HEX_LINE_SZ + 1];
	int hex_pos = 0;
	struct coredump_cmd_copy_arg copy = {
		.offset = 0,
		.buffer = buf,
		.length = COPY_BUF_SZ,
	};

	if (!crash_log_has_coredump()) {
		return -ENOENT;
	}

	size = coredump_query(COREDUMP_QUERY_GET_STORED_DUMP_SIZE, NULL);
	if (size <= 0) {
		return -ENODATA;
	}

	LOG_ERR("#CD:BEGIN#");

	while (size > 0) {
		if (size < COPY_BUF_SZ) {
			copy.length = size;
		}

		int ret = coredump_cmd(COREDUMP_CMD_COPY_STORED_DUMP, &copy);

		if (ret <= 0) {
			LOG_ERR("Failed to read coredump at offset %ld (ret=%d)",
				(long)copy.offset, ret);
			return -EIO;
		}

		for (int i = 0; i < ret; i++) {
			static const char hex[] = "0123456789abcdef";

			hex_line[hex_pos++] = hex[buf[i] >> 4];
			hex_line[hex_pos++] = hex[buf[i] & 0xf];

			if (hex_pos >= HEX_LINE_SZ) {
				hex_line[hex_pos] = '\0';
				LOG_ERR("#CD:%s", hex_line);
				hex_pos = 0;
			}
		}

		copy.offset += ret;
		size -= ret;
	}

	/* Flush remaining hex data */
	if (hex_pos > 0) {
		hex_line[hex_pos] = '\0';
		LOG_ERR("#CD:%s", hex_line);
	}

	LOG_ERR("#CD:END#");

	return 0;
}

#ifdef CONFIG_CRASH_LOG_AUTO_REPORT
static int crash_log_boot_check(void)
{
	if (crash_log_has_coredump()) {
		LOG_ERR("=== PREVIOUS CRASH DETECTED ===");
		LOG_ERR("Stored coredump found. Emitting via RTT...");
		crash_log_emit();
		LOG_ERR("=== END CRASH REPORT ===");
		LOG_INF("Use 'crash clear' or crash_log_clear() to erase.");
	} else {
		LOG_INF("No stored crash found. Clean boot.");
	}

	return 0;
}

SYS_INIT(crash_log_boot_check, APPLICATION, 99);
#endif /* CONFIG_CRASH_LOG_AUTO_REPORT */
