
#include <linux/spinlock.h>
#include <linux/blk-mq.h>
#include <linux/xarray.h>
#include <linux/list.h>

#define DEV_NAME "CSL"
#define DEVICE_TOTAL_SIZE 16*1024*1024 // 16MB
#define QUEUE_LIMIT 16
#define DEV_FIRST_MINOR 0
#define DEV_MINORS 16

#define SIZE_OF_SECTOR 512
#define DEV_SECTOR_NUM DEVICE_TOTAL_SIZE/SIZE_OF_SECTOR
#define BACKUP_FILE_PATH "/dev/csl_backup"

/*
* RETURN VALUE 
*/
#define SUCCESS_EXIT 0
#define FAIL_EXIT -1

/*
* DEVICE BACKUP CONSTANT
*/
#define BACKUP_HEADER_SIZE 3 * sizeof(unsigned int)
#define OFFSET_SIZE sizeof(unsigned int)
#define XA_ENTRY_SIZE 2 * sizeof(unsigned int)
#define GC_ENTRY_SIZE sizeof(unsigned int)

/*
* ERROR MSG
*/
#define FILE_OPEN_ERROR_MSG "CSL : FAIL TO OPEN FILE"
#define FILE_READ_ERROR_MSG "CSL : FAIL TO READ FILE"
#define FILE_WRITE_ERROR_MSG "CSL : FAIL TO WRITE FILE"

#define MALLOC_ERROR_MSG "CSL : FAIL TO MALLOC SPACE"

#define BACKUP_FAIL_MSG "CSL : FAIL TO BACK UP CSL"

// 여기서 block device 관련 데이터를 다 다룸 

struct csl_dev{
	struct request_queue *queue;
	struct gendisk *gdisk;

    struct blk_mq_tag_set tag_set; // request queue의 tag set

	// atomic operation을 위한 lock
	spinlock_t csl_lock;

	// file write 시작 지점 -> sector 기준 
	unsigned int offset;

	// garbage collection을 위한 list 선언 
	struct list_head list;

	// XArray
	struct xarray l2p_map;

};

struct l2b_item{
	// 기준은 SECTOR
	unsigned long lba; // 이게 index가 되고 
	unsigned int ppn; // 이게 data가 되어서 저장
};

struct list_item{
	unsigned int sector;
	struct list_head list_head;
};


