#include <zephyr/ztest.h>
#include <zephyr/debug/coredump.h>
#include <crash_log.h>

/*
 * crash_log API tests.
 *
 * These test the C API on a clean boot (no stored coredump).
 * Testing with an actual stored coredump requires triggering a fault
 * and rebooting, which is better done as a hardware integration test.
 */

ZTEST(crash_log, test_no_coredump_on_clean_boot)
{
	zassert_false(crash_log_has_coredump(),
		      "No coredump should exist on fresh boot");
}

ZTEST(crash_log, test_emit_returns_enoent_when_empty)
{
	int ret = crash_log_emit();

	zassert_equal(ret, -ENOENT,
		      "crash_log_emit should return -ENOENT when no dump, got %d",
		      ret);
}

ZTEST(crash_log, test_clear_succeeds_when_empty)
{
	int ret = crash_log_clear();

	zassert_equal(ret, 0,
		      "crash_log_clear should succeed even when empty, got %d",
		      ret);
}

ZTEST(crash_log, test_still_empty_after_clear)
{
	crash_log_clear();

	zassert_false(crash_log_has_coredump(),
		      "No coredump should exist after clear");
}

ZTEST_SUITE(crash_log, NULL, NULL, NULL, NULL, NULL);
