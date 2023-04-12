#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

int main() {
    char *device_path = "/dev/mapper/mydedup"; // replace with your device path
    int fd = open(device_path, O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Failed to open device: %s\n", strerror(errno));
        return 1;
    }
    
    int len = 1024*12;
    char *data = (char*)malloc(len);
    ssize_t num_bytes_written = write(fd, data, len);
    if (num_bytes_written == -1) {
        fprintf(stderr, "Failed to write to device: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    printf("Wrote %zd bytes to device\n", num_bytes_written);
    close(fd);
    return 0;
}
