#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/debug/coredump.h>
#include <crash_log.h>

#define COPY_BUF_SZ 128

static int cmd_crash_check(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (crash_log_has_coredump()) {
		int size = coredump_query(COREDUMP_QUERY_GET_STORED_DUMP_SIZE,
					  NULL);
		shell_print(sh, "CRASH STORED (%d bytes)", size);
		shell_print(sh, "Use 'crash dump' to output, "
			    "'crash clear' to erase.");
	} else {
		shell_print(sh, "No stored crash.");
	}

	return 0;
}

static int cmd_crash_info(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (!crash_log_has_coredump()) {
		shell_print(sh, "No stored crash.");
		return 0;
	}

	/* Read the coredump header (12 bytes) + arch block header (5 bytes)
	 * + registers to get crash PC, LR, and fault reason.
	 *
	 * Zephyr coredump format:
	 *   Header:  ZE + version(2) + tgt_code(2) + ptr_bits(1) + flag(1) + reason(4)
	 *   Arch:    'A' + version(2) + data_len(2) + registers...
	 *   Regs:    R0,R1,R2,R3,R12,LR,PC,xPSR,SP [,R4-R11 in v2]
	 */
	uint8_t buf[COPY_BUF_SZ];
	struct coredump_cmd_copy_arg copy = {
		.offset = 0,
		.buffer = buf,
		.length = COPY_BUF_SZ,
	};

	int ret = coredump_cmd(COREDUMP_CMD_COPY_STORED_DUMP, &copy);

	if (ret != 0) {
		shell_print(sh, "Failed to read coredump: %d", ret);
		return 0;
	}

	/* Parse header */
	if (buf[0] != 'Z' || buf[1] != 'E') {
		shell_print(sh, "Invalid coredump header");
		return 0;
	}

	uint16_t hdr_ver = buf[2] | (buf[3] << 8);
	uint16_t tgt_code = buf[4] | (buf[5] << 8);
	uint32_t reason = buf[8] | (buf[9] << 8) |
			  (buf[10] << 16) | (buf[11] << 24);

	static const char *const reason_str[] = {
		"CPU exception",
		"Spurious IRQ",
		"Stack check fail",
		"Kernel oops",
		"Kernel panic",
	};
	const char *reason_name = (reason < ARRAY_SIZE(reason_str))
				  ? reason_str[reason] : "Unknown";

	static const char *const tgt_str[] = {
		"Unknown", "x86", "x86_64",
		"ARM Cortex-M", "RISC-V", "Xtensa", "ARM64",
	};
	const char *tgt_name = (tgt_code < ARRAY_SIZE(tgt_str))
			       ? tgt_str[tgt_code] : "Unknown";

	shell_print(sh, "Crash info (coredump v%u):", hdr_ver);
	shell_print(sh, "  Target:  %s", tgt_name);
	shell_print(sh, "  Reason:  %s (%u)", reason_name, reason);

	/* Parse arch block registers if ARM Cortex-M (tgt_code == 3) */
	if (tgt_code == 3) {
		/* Arch block starts at offset 12 */
		int arch_off = 12;

		if (buf[arch_off] != 'A') {
			shell_print(sh, "  (no arch block found)");
			return 0;
		}

		/* Skip arch header: id(1) + version(2) + data_len(2) = 5 */
		int reg_off = arch_off + 5;

		/* Registers: R0,R1,R2,R3,R12,LR,PC,xPSR,SP */
		uint32_t *regs = (uint32_t *)&buf[reg_off];

		shell_print(sh, "  PC:      0x%08x", regs[6]);
		shell_print(sh, "  LR:      0x%08x", regs[5]);
		shell_print(sh, "  SP:      0x%08x", regs[8]);
		shell_print(sh, "  R0:      0x%08x (arg0/fault addr)",
			    regs[0]);
		shell_print(sh, "  R2:      0x%08x", regs[2]);
	}

	int size = coredump_query(COREDUMP_QUERY_GET_STORED_DUMP_SIZE, NULL);

	shell_print(sh, "  Size:    %d bytes", size);
	shell_print(sh, "Use 'crash dump' for full #CD: output.");

	return 0;
}

static int cmd_crash_dump(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (!crash_log_has_coredump()) {
		shell_print(sh, "No stored crash.");
		return 0;
	}

	shell_print(sh, "Emitting stored coredump via logging...");
	int ret = crash_log_emit();

	if (ret == 0) {
		shell_print(sh, "Done. Capture RTT output and pass to "
			    "analyze_coredump.");
	} else {
		shell_print(sh, "Failed: %d", ret);
	}

	return 0;
}

static int cmd_crash_clear(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	int ret = crash_log_clear();

	if (ret == 0) {
		shell_print(sh, "Stored coredump erased.");
	} else {
		shell_print(sh, "Failed to erase: %d", ret);
	}

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_crash,
	SHELL_CMD(check, NULL, "Check for stored crash", cmd_crash_check),
	SHELL_CMD(info, NULL, "Show crash summary (PC, LR, reason)",
		  cmd_crash_info),
	SHELL_CMD(dump, NULL, "Output stored crash as #CD: lines",
		  cmd_crash_dump),
	SHELL_CMD(clear, NULL, "Erase stored crash from flash",
		  cmd_crash_clear),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(crash, &sub_crash, "Crash log management", NULL);
