#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define BUF_LEN		512 // 8 sector
#define DEV_NAME	"/dev/CSL"
#define DEV_SIZE 1024*1024 // 2048 sector니까 총 256번의 write_pattern을 수행 가능 
#define SECTOR_SIZE 512


int main()
{
    static char buf[BUF_LEN];
    int fd;
	off_t off = 1024*100;
    int write_count = 0;

    printf("Start to test\n");

    if ((fd = open(DEV_NAME, O_RDWR)) < 0) {
		  perror("open error");
    }

    if(lseek(fd, off, SEEK_SET)<0){
        perror("lseeck error");
        return 0;
    }

    if(write(fd, "Say Hello To world", strlen("Say Hello To world"))<0 ){
        perror("Write Error");
        return 0;
    }

    if(lseek(fd, off, SEEK_SET)<0){
        perror("lseeck error");
        return 0;
    }

    if (read(fd, buf, BUF_LEN) < 0) {
        perror("read error");
    } else {
        printf("Read from offset %ld:\n%s\n", off, buf);
    }

    // offset 초기화
    // if(lseek(fd, 0, SEEK_SET)<0){
    //     perror("lseeck error");
    //     return 0;
    // }

    // if(write(fd, "Say Hello To world Again", strlen("Say Hello To world Again"))<0){
    //     perror("Write error");
    //     return 0;   
    // }
    // if(lseek(fd, off, SEEK_SET)<0){
    //     perror("lseeck error");
    //     return 0;
    // }
    // if (read(fd, buf, BUF_LEN) < 0) {
    //     perror("read error");
    // } else {
    //     printf("Read from offset %ld:\n%s\n", off, buf);
    // }

    printf("Test Complete\n");
    

    return 0;
}