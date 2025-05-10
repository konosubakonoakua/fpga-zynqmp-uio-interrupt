#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <poll.h>

uint8_t wait_for_interrupt(int fd_int);

/**
 * @brief Entry point of the program. It opens a file descriptor, enables global and channel interrupts,
 * reads interrupt registers, waits for an interrupt, and cleans up before exiting.
 *
 * @param argc The count of command-line arguments.
 * @param argv The array of command-line arguments.
 * @return int The exit status of the program.
 */
int main(int argc, char *argv[])
{
    uint32_t reenable = 1;
    unsigned int value = 0;

    int fd = open("/dev/uio1", O_RDWR);
    if (fd < 0) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    // enable the interrupt controller thru the UIO subsystem
    write(fd, (void *)&reenable, sizeof(int));


    /* Wait on interrupt */
	while(1)
		wait_for_interrupt(fd);

    close(fd);
    exit(EXIT_SUCCESS);
}

uint8_t wait_for_interrupt(int fd_int)
{
	static unsigned int count = 0, bntd_flag = 0, bntu_flag = 0;
	int pending = 0;
	int reenable = 1;
	int flag_end = 0;
	unsigned int reg;
	unsigned int value;

	// block (timeout for poll) on the file waiting for an interrupt
	struct pollfd fds = {
        .fd = fd_int,
        .events = POLLIN,
        };

	int ret = poll(&fds, 1, -1); // block wait
	printf("%s: ret = %d\n", __func__, ret);
	if (ret >= 1) {
		// &reenable -> &pending
		read(fd_int, (void *)&reenable, sizeof(int));

		count++;
		printf("fixed timer interrupt %d!\n", count);

		// anti rebond
		usleep(50000);

		// re-enable the interrupt in the interrupt controller thru the
		// the UIO subsystem now that it's been handled
		write(fd_int, (void *)&reenable, sizeof(int));
	}
	return ret;
}

