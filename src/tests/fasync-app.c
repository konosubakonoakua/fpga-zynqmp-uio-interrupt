#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#define DEVICE "/dev/zynq_irq_0"

volatile sig_atomic_t got_signal = 0;

void sigio_handler(int sig) {
    got_signal = 1;
}

int main() {
        int cnt = 0;
    int fd = open(DEVICE, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    signal(SIGIO, sigio_handler);
    fcntl(fd, F_SETOWN, getpid());
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | FASYNC);

    printf("Waiting for SIGIO...\n");
    while (1) {
        if (got_signal) {
                        printf("got signal %d\n", ++cnt);
            got_signal = 0;
        }
        sleep(0.1);
    }

    close(fd);
    return 0;
}
