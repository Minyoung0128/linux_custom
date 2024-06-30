#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/spinlock.h>
#include <linux/blk_types.h>
#include <linux/err.h>

#include "cls.h"

static int CLS_MAJOR = 0; // save the major number of the device
struct cls_dev *dev;

struct queue_limits queue_limit = {
		.logical_block_size	= 1024,
	};

// static void cls_request(struct request_queue *q)
// {
// 	printk("CLS get request!");
// 	return;
// }

static int cls_open(struct gendisk *gdisk, fmode_t mode)
{
	printk("CLS Device Drive open !\n");
	return 0;
}

static void cls_release(struct gendisk *gd)
{
	printk("CLS Device Drive released!\n");
}

static int cls_ioctl(struct block_device *bdev, blk_mode_t mode, unsigned cmd, unsigned long arg)
{
	printk("CLS Device Drive ioctl!\n");
	return 0;
}

// multi queue에 넣어주는 함수 
static blk_status_t cls_enqueue(struct blk_mq_hw_ctx *ctx, const struct blk_mq_queue_data *data){
	blk_status_t ret = 0;
	printk(KERN_INFO "PUT DATA TO CLS QUEUE!");
	return ret;
}

static struct block_device_operations cls_fops = {
	.owner = THIS_MODULE,
	// .open = cls_open,
	// .release = cls_release,
	// .ioctl = cls_ioctl
};


static struct blk_mq_ops cls_mq_ops = {
	.queue_rq = cls_enqueue
};

static struct cls_dev *cls_alloc(void)
{

	// request queue랑 gendisk를 할당해주고 그 struct에 각각의 구조체를 연결해서 반환
	struct cls_dev *mydev;
	struct gendisk *disk;

	int error;

	printk(KERN_INFO "CLS : CLS DEVICE START TO ALLOCATE");

	mydev = kzalloc(sizeof(*mydev), GFP_KERNEL); // kzmalloc로 커널 공간에 메모리 할당
	// GFP_KERNEL : 흔히 사용되는 flag로 메모리가 충분하지 않으면 sleep 상태가 된다. 

	if(!mydev){
		printk(KERN_WARNING "CLS : FAIL TO MALLOC TO DEVICE STRUCT!");
		return NULL;
	}

	spin_lock_init(&mydev->cls_lock);
	
	// 2. tag set 할당
	printk(KERN_INFO "CLS : START ALLOCATE TAG SET");

	mydev->tag_set.ops = &cls_mq_ops;
	mydev->tag_set.nr_hw_queues = 1;
	mydev->tag_set.queue_depth = 32;
	mydev->tag_set.numa_node = NUMA_NO_NODE;
	mydev->tag_set.cmd_size = 0;
	mydev->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
	mydev->tag_set.driver_data = mydev;

	error = blk_mq_alloc_tag_set(&mydev->tag_set);

	if(error){
		printk(KERN_WARNING "CLS : FAIL TO ALLCOCATE TAG SET");
		kfree(mydev);
		return NULL;
	}

	printk(KERN_INFO "CLS : FINISH ALLOCATE TAG SET");

	// 3. gendisk 할당
	printk(KERN_INFO "CLS : START ALLOCATE DISK");
	
	disk = blk_mq_alloc_disk(&mydev->tag_set, &queue_limit, mydev->queue);
	
	disk->major = CLS_MAJOR;
	disk->fops = &cls_fops;
	disk->first_minor = 0;
	disk->minors = 2;
	disk->private_data = mydev;

	snprintf(disk->disk_name, 32, "cls");
	// blk_mq_alloc_disk > queue allocate, 
	// alloc_disk_node를 호출 
	// alloc_disk_node : disk에 kzalloc_node를 해서 넣어주고, bio(block i/o) initialize
	// 
	// genhd.c의 alloc disk node를 호출
	// 그 함수에서 queue를 할당 

	if(IS_ERR(disk)){
		printk(KERN_WARNING "CLS : FAIL TO ALLCOCATE DISK");
		blk_mq_free_tag_set(&mydev->tag_set);
		kfree(mydev);
		return NULL;
	}
	printk(KERN_INFO "CLS : FINISH ALLOCATE DISK");

	mydev->gdisk = disk;
	mydev->queue = disk->queue;

	printk(KERN_INFO "CLS : START ADD DISK");
	error = add_disk(disk); // 여기에서 뭔가 warning이 발생
	if(error){
		// 여기로 들어옴..
		// WARN_ON(!disk->minors) 에서 걸리는 듯 
		// 즉 major가 0이 아닌데 minors 가 0이다.. 이걸 설정해주자 
		printk(KERN_WARNING "CLS : FAIL TO ADD DISK with %d",error);
		blk_mq_free_tag_set(&mydev->tag_set);
		kfree(mydev);
		return NULL;
	} 
	printk(KERN_INFO "CLS : FINISH ADD DISK");

	return mydev;
}

static int __init cls_init(void)
{
	int result;
	
	struct cls_dev *mydev;

	result = register_blkdev(CLS_MAJOR, "cls");
	
	// error handling
	if(result < 0)	{
		printk(KERN_WARNING "CLS: Fail to get major number!\n");
		return result;
	}

	if(CLS_MAJOR == 0){
		CLS_MAJOR = result;
	}
	
	mydev = cls_alloc();

	if(!mydev){
		printk(KERN_WARNING "CLS: Fail to add disk!\n");
		return -1;
	}

	dev = mydev;
	printk(KERN_INFO "DEVICE : CLS is successfully initialized with major number %d\n",CLS_MAJOR);
	return 0;
}



static void __exit cls_exit(void)
{
	put_disk(dev->gdisk);
	unregister_blkdev(CLS_MAJOR,BLKDEV_NAME);
	blk_mq_free_tag_set(&dev->tag_set);
	printk(KERN_INFO "DEVICE : CLS is successfully unregistered!\n");
	
}

module_init(cls_init);
module_exit(cls_exit);

MODULE_AUTHOR("MinyoungKim");
MODULE_DESCRIPTION("Virtual Block Device Driver");
MODULE_LICENSE("GPL");
