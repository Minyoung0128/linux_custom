#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define BUF_LEN		512 // 8 sector
#define DEV_NAME	"/dev/CSL"
#define DEV_SIZE 16*1024*1024 // 2048 sector니까 총 256번의 write_pattern을 수행 가능 
#define SECTOR_SIZE 512
#define START_SECTOR 10
#define END_SECTOR 100

int main()
{
    static char buf[BUF_LEN];
    int fd;
	off_t off = 512;
    int write_count = 0;

    printf("Start to test\n");

    if ((fd = open(DEV_NAME, O_RDWR)) < 0) {
		  perror("open error");
    }

    for (int i = START_SECTOR; i!=END_SECTOR; i++){
        off = (off_t) i*SECTOR_SIZE;

        if(lseek(fd, off, SEEK_SET)<0){
            perror("lseeck error");
            return 0;
        }

        snprintf(buf, 100, "WRITE TEST with SECTOR NUM %d\n", i);

        printf("write to %d : %s",i, buf);

        if(write(fd, buf, sizeof(buf))<0){
            perror("Write Error");
            return 0;
        }

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

    close(fd);
    
    return 1;

} 
