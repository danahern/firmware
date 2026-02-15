#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/version.h>

static int cmd_device_info(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "Board:    %s", CONFIG_BOARD);
	shell_print(sh, "SOC:      %s", CONFIG_SOC);
	shell_print(sh, "Zephyr:   %s", KERNEL_VERSION_STRING);
	shell_print(sh, "Built:    %s %s", __DATE__, __TIME__);

	return 0;
}

static int cmd_device_uptime(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	int64_t uptime_ms = k_uptime_get();
	int secs = (int)(uptime_ms / 1000);
	int mins = secs / 60;
	int hours = mins / 60;

	shell_print(sh, "Uptime: %dh %dm %ds (%lld ms)",
		    hours, mins % 60, secs % 60, uptime_ms);

	return 0;
}

static int cmd_device_reset(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "Resetting...");
	k_msleep(100);
	sys_reboot(SYS_REBOOT_COLD);

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_board,
	SHELL_CMD(info, NULL, "Board, SOC, version, build date", cmd_device_info),
	SHELL_CMD(uptime, NULL, "Time since boot", cmd_device_uptime),
	SHELL_CMD(reset, NULL, "Cold reboot", cmd_device_reset),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(board, &sub_board, "Board info and management", NULL);
