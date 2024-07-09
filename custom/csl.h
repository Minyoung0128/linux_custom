
#include <linux/spinlock.h>
#include <linux/blk-mq.h>
#include <linux/xarray.h>
#include <linux/list.h>

#define DEV_NAME "CSL"
#define DEVICE_TOTAL_SIZE 16*1024*1024 // 16MB
#define QUEUE_LIMIT 16

#define SIZE_OF_SECTOR 512

#define DEV_SECTOR_NUM DEVICE_TOTAL_SIZE/SIZE_OF_SECTOR

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


