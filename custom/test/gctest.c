#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define BUF_LEN		512// 8 sector
#define DEV_NAME	"/dev/CSL"
#define DEV_SIZE 16*1024*1024 // 2048 sector니까 총 256번의 write_pattern을 수행 가능 
#define SECTOR_SIZE 512
#define START_SECTOR 10
#define END_SECTOR 200

#define ITER_NUM 100

void write_CSL(int fd, int iter,  int sector_num, char buf[]){

    off_t off = sector_num * SECTOR_SIZE;

    if(lseek(fd, off, SEEK_SET)<0){
        perror("lseeck error");
        return 0;
    }

    snprintf(buf, BUF_LEN, "WRITE TEST with iter %d,  SECTOR NUM %d\n", iter, sector_num);

    if(write(fd, buf, strlen(buf)+1)<0){
        perror("Write Error");
        return 0;
    }    
}
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

    for(int i=0; i < ITER_NUM; i++){
        for(int j=START_SECTOR;j<END_SECTOR;j++){
            write_CSL(fd,i, j, buf);
            off = j * SECTOR_SIZE;

            if (lseek(fd, off, SEEK_SET) < 0) {
                perror("lseek error");
                return 1;
            }

            memset(buf, 0, sizeof(buf)); // 버퍼 초기화
            if (read(fd, buf, sizeof(buf)) < 0) {
                perror("read error");
                return 1;
            }

        }
        printf("Write complete %dth\n",i);
    }
    close(fd);
    
    return 1; 
}
