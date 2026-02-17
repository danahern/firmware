/**
 * STM32MP1 A7 Hello World
 *
 * Simple Linux userspace app for the Cortex-A7 core.
 * Cross-compiled with Buildroot's arm-linux-gnueabihf-gcc.
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/utsname.h>

int main(void)
{
	struct utsname info;

	printf("Hello from STM32MP1 Cortex-A7!\n");

	if (uname(&info) == 0) {
		printf("Kernel:  %s %s\n", info.sysname, info.release);
		printf("Machine: %s\n", info.machine);
	}

	printf("PID: %d\n", getpid());

	return 0;
}
