#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/fs.h>

#define DEV_NAME "cls"
#define DEV_CAPACITY 1024*1024 // 1MB
#define QUEUE_LIMIT 16

static int CLS_MAJOR= 0; // save the major number of the device

/*typedef struct block_dev{
	sector_t capacity;
	unsigned char *data;
	struct gendisk gdisk*;
} block_dev_t;*/

struct queue_limits lim = {
	.max_sectors = 64
};



static void cls_request(struct request_queue *q)
{
	printk("CLS get request!");
	return;
}



static int cls_open(struct gendisk *gdisk, fmode_t mode)
{
	printk("CLS Device Drive open !\n");
	return 0;
}

static void cls_release(struct gendisk *gd)
{
	printk("CLS Device Drive released!\n");
}




static struct block_device_operations cls_fops = {
	.owner = THIS_MODULE,
	.open = cls_open,
	.release = cls_release,
};

/*static int blkdev_add_device(void){
	struct gendisk *disk;

	block_dev_t *dev = kzalloc(sizeof(block_dev_t), GFP_KERNEL);

	if(dev == NULL){
		prinkt(KERN_WARNING "CLS: kzalloc Error");
		return -1;
	}

	cls_dev = dev;

	if((disk = alloc_disk(1)) == NULL){
		prinkt(KERN_WARNING "CLS: disk alloc Error");
		return -1;
	}

	disk->
}*/
static int __init cls_init(void)
{
	int result;
	
	static struct gendisk *gdisk;
	result = register_blkdev(CLS_MAJOR, "cls");
	
	// error handling
	if(result < 0)	{
		printk(KERN_WARNING "CLS: Fail to get major number!\n");
		return result;
	}

	if(CLS_MAJOR == 0){
		CLS_MAJOR = result;
	}
	
	
	gdisk = blk_alloc_disk(&lim,NUMA_NO_NODE);
	if(!gdisk){
		printk(KERN_WARNING "CLS: Fail to allocate disk!\n");
		return -1;
	}
	
	sprintf(gdisk->disk_name,DEV_NAME);
	
	gdisk->major = CLS_MAJOR;
	gdisk->fops = &cls_fops;
	gdisk->first_minor = 0;
	set_capacity(gdisk, DEV_CAPACITY);
	
	int i = add_disk(gdisk);
	if(!i){
		printk(KERN_WARNING "CLS: Fail to add disk!\n");
		return -1;
	}
	printk(KERN_INFO "DEVICE : CLS is successfully initialized with major number %d\n",CLS_MAJOR);
	return 0;
}



static void __exit cls_exit(void)
{
	unregister_blkdev(CLS_MAJOR,DEV_NAME);
	printk(KERN_INFO "DEVICE : CLS is successfully unregistered!\n");
	
}

module_init(cls_init);
module_exit(cls_exit);

MODULE_AUTHOR("Minyoung   kim");
MODULE_DESCRIPTION("Virtual Block Device Driver");
MODULE_LICENSE("GPL");
