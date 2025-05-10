#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <poll.h>

#define GPIO_DATA_OFFSET 0x00
#define GPIO_TRI_OFFSET 0x04
#define GPIO_DATA2_OFFSET 0x08
#define GPIO_TRI2_OFFSET 0x0C
#define GPIO_GLOBAL_IRQ 0x11C
#define GPIO_IRQ_CONTROL 0x128
#define GPIO_IRQ_STATUS 0x120
#define GPIO_MAP_SIZE 0x10000

/* Prototypes */
unsigned int get_memory_size(char *sysfs_path_file);
uint8_t wait_for_interrupt(int fd_int, void *gpio_ptr);
unsigned long reg_read(void *reg_base, unsigned long offset);
void reg_write(void *reg_base, unsigned long offset, unsigned long value);

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
    void *ptr_axi_gpio;
    uint32_t reenable = 1;
    unsigned int gpio_size = 0;
    unsigned int value = 0;

    int fd = open("/dev/uio0", O_RDWR);
    if (fd < 0) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    /* mmap the UIO devices */
    ptr_axi_gpio = mmap(NULL, GPIO_MAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

    // Global interrupt enable, Bit 31 = 1
    reg_write(ptr_axi_gpio, GPIO_GLOBAL_IRQ, 0x80000000);

    // Channel 1 Interrupt enable
    reg_write(ptr_axi_gpio, GPIO_IRQ_CONTROL, 1);

    // enable the interrupt controller thru the UIO subsystem
    write(fd, (void *)&reenable, sizeof(int));

    /* Print Interrupt Registers */
    value = reg_read(ptr_axi_gpio, GPIO_GLOBAL_IRQ);
    printf("%s: GIER: %08x\n", argv[0], value);
    value = reg_read(ptr_axi_gpio, GPIO_IRQ_CONTROL);
    printf("%s: IP_IER: %08x\n", argv[0], value);
    value = reg_read(ptr_axi_gpio, GPIO_IRQ_STATUS);
    printf("%s: IP_ISR: %08x\n", argv[0], value);

    /* Wait on interrupt */
    wait_for_interrupt(fd, ptr_axi_gpio);

    munmap(ptr_axi_gpio, gpio_size);
    close(fd);
    exit(EXIT_SUCCESS);
}

/**
 * @brief Wait for an interrupt on a file descriptor and handle it.
 *
 * @param fd_int File descriptor to wait on.
 * @param gpio_ptr Pointer to the base address of the GPIO device.
 * @return uint8_t The result of the poll operation.
 */
uint8_t wait_for_interrupt(int fd_int, void *gpio_ptr)
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
#ifdef COMMENT_OUT
		// channel 1 reading
		value = reg_read(gpio_ptr, GPIO_DATA_OFFSET);
		if ((value & 0x00000001) != 0) {
			do something
		}
#endif
		// anti rebond
		usleep(50000);

		// the interrupt occurred for the 1st GPIO channel so clear it
		reg = reg_read(gpio_ptr, GPIO_IRQ_STATUS);
		if (reg != 0) {
			printf("%s: Clearing ISR register:\n", __func__);
			reg_write(gpio_ptr, GPIO_IRQ_STATUS, 1);
                        usleep(50000); // anti rebond
                        value = reg_read(gpio_ptr, GPIO_IRQ_STATUS);
                        printf("%s: IP_ISR: %08x\n",__func__, value);
		}
		// re-enable the interrupt in the interrupt controller thru the
		// the UIO subsystem now that it's been handled
		write(fd_int, (void *)&reenable, sizeof(int));
	}
	return ret;
}

/**
 * @brief Write a value to a register at a specified offset from the base address.
 *
 * @param reg_base Base address of the register to write to.
 * @param offset Offset from the base address where the value should be written.
 * @param value The value to write to the register.
 */
void reg_write(void *reg_base, unsigned long offset, unsigned long value)
{
	*((unsigned *)(reg_base + offset)) = value;
}

/**
 * @brief Read a value from a register at a specified offset from the base address.
 *
 * @param reg_base Base address of the register to read from.
 * @param offset Offset from the base address where the value should be read.
 * @return unsigned long The value read from the register.
 */
unsigned long reg_read(void *reg_base, unsigned long offset)
{
	return *((unsigned *)(reg_base + offset));
}
