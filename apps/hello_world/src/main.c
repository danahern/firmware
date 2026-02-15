#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(hello_world, LOG_LEVEL_INF);

int main(void)
{
	LOG_INF("hello_world booted");

	while (1) {
		k_sleep(K_FOREVER);
	}

	return 0;
}
