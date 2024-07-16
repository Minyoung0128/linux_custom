#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define BUF_LEN      512 // 8 sector
#define DEV_NAME     "/dev/CSL"
#define SECTOR_SIZE  512
#define START_SECTOR 10
#define END_SECTOR   100

int main() {
    static char buf[BUF_LEN];
    int fd;
    off_t off;

    printf("Start to read test\n");

    if ((fd = open(DEV_NAME, O_RDONLY)) < 0) {
        perror("open error");
        return 1;
    }

    for (int i = START_SECTOR; i <= END_SECTOR; i++) {
        off = (off_t)i * SECTOR_SIZE;

        if (lseek(fd, off, SEEK_SET) < 0) {
            perror("lseek error");
            return 1;
        }

        memset(buf, 0, sizeof(buf)); // 버퍼 초기화
        if (read(fd, buf, sizeof(buf)) < 0) {
            perror("read error");
            return 1;
        }

        printf("Read from sector %d: %s\n", i, buf);
    }

    if (close(fd) != 0) {
        perror("close error");
    }

    printf("Read test complete\n");

    return 0;
}
