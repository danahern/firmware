#include <zephyr/ztest.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_dummy.h>
#include <string.h>

static const struct shell *sh;

static void *device_shell_setup(void)
{
	sh = shell_backend_dummy_get_ptr();
	k_msleep(100);
	return NULL;
}

static void device_shell_before(void *fixture)
{
	ARG_UNUSED(fixture);
	shell_backend_dummy_clear_output(sh);
}

ZTEST(device_shell, test_info_command)
{
	size_t size;

	int ret = shell_execute_cmd(sh, "board info");
	const char *output = shell_backend_dummy_get_output(sh, &size);

	zassert_equal(ret, 0, "board info returned %d", ret);
	zassert_true(size > 0, "Expected output from board info");
	zassert_not_null(strstr(output, "Board:"),
			 "Output should contain 'Board:'");
	zassert_not_null(strstr(output, "Zephyr:"),
			 "Output should contain 'Zephyr:'");
}

ZTEST(device_shell, test_uptime_command)
{
	size_t size;

	int ret = shell_execute_cmd(sh, "board uptime");
	const char *output = shell_backend_dummy_get_output(sh, &size);

	zassert_equal(ret, 0, "board uptime returned %d", ret);
	zassert_true(size > 0, "Expected output");
	zassert_not_null(strstr(output, "Uptime:"),
			 "Output should contain 'Uptime:'");
}

ZTEST(device_shell, test_invalid_subcommand)
{
	int ret = shell_execute_cmd(sh, "board bogus");

	zassert_not_equal(ret, 0,
			  "Invalid subcommand should return error");
}

ZTEST_SUITE(device_shell, NULL, device_shell_setup, device_shell_before,
	    NULL, NULL);
