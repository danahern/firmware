/**
 * STM32MP1 RPMsg Echo Client
 *
 * Linux userspace app that communicates with the M4 core via RPMsg.
 * Opens /dev/rpmsg_charN, sends a message, and reads the echo back.
 *
 * Prerequisites:
 * - M4 firmware loaded via remoteproc with RPMsg endpoint
 * - rpmsg_char kernel module loaded
 * - /dev/rpmsg_ctrlN device exists
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/rpmsg.h>

#define RPMSG_DEV "/dev/rpmsg_ctrl0"
#define MAX_MSG_SIZE 512

static void usage(const char *prog)
{
	fprintf(stderr, "Usage: %s [-d device] [-n count] message\n", prog);
	fprintf(stderr, "  -d device   RPMsg control device (default: %s)\n", RPMSG_DEV);
	fprintf(stderr, "  -n count    Number of echo rounds (default: 1)\n");
}

int main(int argc, char *argv[])
{
	const char *dev_path = RPMSG_DEV;
	int count = 1;
	int opt;

	while ((opt = getopt(argc, argv, "d:n:h")) != -1) {
		switch (opt) {
		case 'd':
			dev_path = optarg;
			break;
		case 'n':
			count = atoi(optarg);
			break;
		case 'h':
		default:
			usage(argv[0]);
			return (opt == 'h') ? 0 : 1;
		}
	}

	if (optind >= argc) {
		usage(argv[0]);
		return 1;
	}

	const char *message = argv[optind];

	printf("RPMsg Echo Client\n");
	printf("Device: %s\n", dev_path);
	printf("Message: \"%s\"\n", message);
	printf("Rounds: %d\n\n", count);

	/* Open RPMsg control device */
	int ctrl_fd = open(dev_path, O_RDWR);
	if (ctrl_fd < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", dev_path, strerror(errno));
		fprintf(stderr, "Is rpmsg_char module loaded? Is M4 firmware running?\n");
		return 1;
	}

	/* Create RPMsg endpoint */
	struct rpmsg_endpoint_info ept_info;
	memset(&ept_info, 0, sizeof(ept_info));
	strncpy(ept_info.name, "rpmsg-echo", sizeof(ept_info.name) - 1);
	ept_info.src = 0;  /* auto-assign */
	ept_info.dst = 0;

	int ret = ioctl(ctrl_fd, RPMSG_CREATE_EPT_IOCTL, &ept_info);
	if (ret < 0) {
		fprintf(stderr, "Failed to create RPMsg endpoint: %s\n", strerror(errno));
		close(ctrl_fd);
		return 1;
	}

	/* The ioctl returns the endpoint fd number via the name field trick,
	 * or we need to open /dev/rpmsgN. For simplicity, try /dev/rpmsg0 */
	close(ctrl_fd);

	int ept_fd = open("/dev/rpmsg0", O_RDWR);
	if (ept_fd < 0) {
		fprintf(stderr, "Failed to open /dev/rpmsg0: %s\n", strerror(errno));
		return 1;
	}

	/* Echo loop */
	char rx_buf[MAX_MSG_SIZE];
	size_t msg_len = strlen(message);

	for (int i = 0; i < count; i++) {
		ssize_t written = write(ept_fd, message, msg_len);
		if (written < 0) {
			fprintf(stderr, "Write failed: %s\n", strerror(errno));
			break;
		}

		ssize_t n = read(ept_fd, rx_buf, sizeof(rx_buf) - 1);
		if (n < 0) {
			fprintf(stderr, "Read failed: %s\n", strerror(errno));
			break;
		}
		rx_buf[n] = '\0';

		printf("[%d] TX: \"%s\" (%zd bytes) → RX: \"%s\" (%zd bytes)\n",
		       i + 1, message, written, rx_buf, n);
	}

	close(ept_fd);
	printf("\nDone.\n");
	return 0;
}
