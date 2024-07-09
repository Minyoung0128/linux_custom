#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/blk_types.h>
#include <linux/err.h>

#include "csl.h"

static int CSL_MAJOR = 0; // save the major number of the device

struct csl_dev *dev;
static u8 data[DEVICE_TOTAL_SIZE];


struct queue_limits queue_limit = {
		.logical_block_size	= 512,
	};

static DEFINE_XARRAY(address_map);

/*
* Display current L2P Map array
*/
static void display_index(void)
{
	unsigned long lba = 0;

	xa_lock(&address_map);
	// xa_for_each()
	
	xa_unlock(&address_map);

}
static uint csl_gc(void)
{
	// device가 다 찬 경우 여기로 진입
	// garbage collection을 진행해서 새로운 ppn을 할당 

	struct list_item *entry;

	if(!list_empty(&dev->list))
	{
		entry = list_first_entry(&dev->list, struct list_item, list_head);
		list_del(&entry->list_head);
		return entry->sector;
	}

	return -1;
}

static void csl_invalidate(uint ppn)
{
	struct list_item *item;

	item = kmalloc(sizeof(struct list_item*), GFP_KERNEL);

	if(!item) return;

	item->sector = ppn;
	list_add_tail(&item->list_head, &dev->list);

}

static void csl_read(uint ppn, void* buf, uint num_sec){
	uint nbytes = num_sec * SECTOR_SIZE;

	if (ppn > DEV_SECTOR_NUM){
		printk(KERN_WARNING "Wrong Sector num!");
		return;	
	}

	memcpy(buf, &data[ppn*SECTOR_SIZE], nbytes);
}


static void csl_write(uint ppn, void* buf, uint num_sec){
	uint nbytes = num_sec * SECTOR_SIZE;

	if (ppn > DEV_SECTOR_NUM){
		// garbage collection 수행
		uint ppn_new = csl_gc();

		memcpy(&data[ppn_new*SECTOR_SIZE], buf, nbytes);
	}
	memcpy(&data[ppn*SECTOR_SIZE], buf, nbytes);
}

static int csl_open(struct gendisk *gdisk, fmode_t mode)
{
	printk("CSL Device Drive open !\n");
	return 0;
}

static void csl_release(struct gendisk *gd)
{
	printk("CSL Device Drive released!\n");
}

static int csl_ioctl(struct block_device *bdev, blk_mode_t mode, unsigned cmd, unsigned long arg)
{
	printk("CSL Device Drive ioctl!\n");
	return 0;
}

static void csl_transfer(struct csl_dev *dev, unsigned int start_sec, unsigned int num_sec, void* buffer, int isWrite){

	struct l2b_item* l2b_item;
	void* ret;

	if(isWrite){
		// start sec은 지금 logical address니까 PPN 변환이 필요
		pr_info("CSL : Start to Write");
		ret = xa_load(&address_map, (unsigned long)start_sec);
		
		if(!ret){
			// address_val이 NULL > 이미 쓰여있는 데이터가 없으므로 invalidate하지 않아도 괜찮음
			l2b_item = kmalloc(sizeof(struct l2b_item), GFP_KERNEL);

			l2b_item->lba = start_sec;
			l2b_item->ppn = dev->offset;
			
			pr_info("CSL : Allocate New page!");
			xa_store(&address_map, l2b_item->lba, (void*)l2b_item, GFP_KERNEL);
			
			printk(KERN_INFO "CLS : Start write LBA [%d] to PPN [%d]", l2b_item->lba, l2b_item->ppn);
			csl_write(l2b_item->ppn, buffer, num_sec);
			printk(KERN_INFO "CLS : Finish Write LBA [%d] to PPN [%d]", l2b_item->lba, l2b_item->ppn);
		}

		else{
			// 이미 할당받았던 자리가 있음 > 그거 invalidate 해주기
			l2b_item = (struct l2b_item*) ret;
			csl_invalidate(l2b_item->ppn);

			l2b_item->ppn = dev->offset; // 새로운 offset 할당
			
			dev->offset+=SIZE_OF_SECTOR;
			
			printk(KERN_INFO "CLS : Start write LBA [%d] to PPN [%d]", l2b_item->lba, l2b_item->ppn);
			csl_write(l2b_item->ppn, buffer, num_sec);
			printk(KERN_INFO "CLS : Finish Write LBA [%d] to PPN [%d]", l2b_item->lba, l2b_item->ppn);
		}
	}

	else {
		pr_info("CSL : Start to Read!");
		ret = xa_load(&address_map, start_sec);
		if(!ret){
			printk(KERN_WARNING "PAGE FAULT!");
			return;
		}
		l2b_item = (struct l2b_item*) ret;
		pr_info("CLS : Start Read LBA [%d] to PPN [%d]", l2b_item->lba, l2b_item->ppn);
		csl_read(l2b_item->ppn, buffer, num_sec);
		pr_info("CLS : Finish Read LBA [%d] to PPN [%d]", l2b_item->lba, l2b_item->ppn);
	}
}


static void csl_get_request(struct request *rq)
{
	// request가 read, write인지 판별
	int isWrite = rq_data_dir(rq);

	sector_t start_sector = blk_rq_pos(rq);
	unsigned int sector_len = blk_rq_sectors(rq);
	unsigned int byte_len = blk_rq_bytes(rq);
	
	pr_info("CSL : Request | isWrite = [%d] | start_sector = [%d] | sector_length : [%d] | byte_length : [%d]",isWrite,start_sector, sector_len, byte_len);

	struct bio_vec bvec;
	struct req_iterator iter;

	void* buffer;

	rq_for_each_segment(bvec, rq, iter){
		// request의 bio_vec을 가져와 파싱해줌.
		unsigned int num_sector = blk_rq_sectors(rq);

		buffer = page_address(bvec.bv_page)+bvec.bv_offset;

		csl_transfer(dev, start_sector, num_sector, buffer, isWrite); // transfer로 들어가면 read or write를 실행

		start_sector += num_sector; 
	}
}

static blk_status_t csl_enqueue(struct blk_mq_hw_ctx *ctx, const struct blk_mq_queue_data *data){
	struct request *rq = data->rq;
	
	pr_info("CSL : Start to Request");

	blk_mq_start_request(rq); // 커널에 device request를 시작한다고 알려주기 

	csl_get_request(rq);

	blk_mq_end_request(rq, BLK_STS_OK);

	return BLK_STS_OK;
}

static struct block_device_operations csl_fops = {
	.owner = THIS_MODULE,
	// .open = csl_open,
	// .release = csl_release,
	// .ioctl = csl_ioctl
};


static struct blk_mq_ops csl_mq_ops = {
	.queue_rq = csl_enqueue
};

static struct csl_dev *csl_alloc(void)
{

	// request queue랑 gendisk를 할당해주고 그 struct에 각각의 구조체를 연결해서 반환
	struct csl_dev *mydev;
	struct gendisk *disk;

	int error;

	printk(KERN_INFO "CSL : CSL DEVICE START TO ALLOCATE");

	mydev = kzalloc(sizeof(struct csl_dev), GFP_KERNEL); // kzmalloc로 커널 공간에 메모리 할당
	// GFP_KERNEL : 흔히 사용되는 flag로 메모리가 충분하지 않으면 sleep 상태가 된다. 

	if(!mydev){
		printk(KERN_WARNING "CSL : FAIL TO MALLOC TO DEVICE STRUCT!");
		return NULL;
	}

	printk(KERN_INFO "CSL : CSL DEVICE INIT LIST HEAD");
	INIT_LIST_HEAD(&mydev->list);
	printk(KERN_INFO "CSL : CSL DEVICE INIT LIST HEAD - FIN");


	printk(KERN_INFO "CSL : CSL DEVICE INIT LOCK");
	spin_lock_init(&mydev->csl_lock);
	printk(KERN_INFO "CSL : CSL DEVICE INIT LOCK - FIN");

	// 2. tag set 할당
	printk(KERN_INFO "CSL : START ALLOCATE TAG SET");

	mydev->tag_set.ops = &csl_mq_ops;
	mydev->tag_set.nr_hw_queues = 1;
	mydev->tag_set.queue_depth = 32;
	mydev->tag_set.numa_node = NUMA_NO_NODE;
	mydev->tag_set.cmd_size = 0;
	mydev->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
	mydev->tag_set.driver_data = mydev;

	error = blk_mq_alloc_tag_set(&mydev->tag_set);

	if(error){
		printk(KERN_WARNING "CSL : FAIL TO ALLCOCATE TAG SET");
		kfree(mydev);
		return NULL;
	}

	printk(KERN_INFO "CSL : FINISH ALLOCATE TAG SET");

	// 3. gendisk 할당
	printk(KERN_INFO "CSL : START ALLOCATE DISK");
	
	disk = blk_mq_alloc_disk(&mydev->tag_set, &queue_limit, mydev->queue);
	
	disk->major = CSL_MAJOR;
	disk->fops = &csl_fops;
	disk->first_minor = 0;
	disk->minors = 2;
	disk->private_data = mydev;

	snprintf(disk->disk_name, 32, "CSL");

	if(IS_ERR(disk)){
		printk(KERN_WARNING "CSL : FAIL TO ALLCOCATE DISK");
		blk_mq_free_tag_set(&mydev->tag_set);
		kfree(mydev);
		return NULL;
	}
	printk(KERN_INFO "CSL : FINISH ALLOCATE DISK");

	mydev->gdisk = disk;
	mydev->queue = disk->queue;

	printk(KERN_INFO "CSL : START ADD DISK");
	error = add_disk(disk); // 여기에서 뭔가 warning이 발생
	if(error){
		// 여기로 들어옴..
		// WARN_ON(!disk->minors) 에서 걸리는 듯 
		// 즉 major가 0이 아닌데 minors 가 0이다.. 이걸 설정해주자 
		printk(KERN_WARNING "CSL : FAIL TO ADD DISK with %d",error);
		blk_mq_free_tag_set(&mydev->tag_set);
		kfree(mydev);
		return NULL;
	}
	set_capacity(mydev->gdisk, DEV_SECTOR_NUM);

	printk(KERN_INFO "CSL : FINISH ADD DISK");

	return mydev;
}

static int __init csl_init(void)
{	
	
	printk(KERN_INFO "CSL : CSL INITIALIZE START");

	int result;
	
	struct csl_dev *mydev;

	result = register_blkdev(CSL_MAJOR, DEV_NAME);
	
	// error handling
	if(result < 0)	{
		printk(KERN_WARNING "CSL: Fail to get major number!\n");
		return result;
	}

	if(CSL_MAJOR == 0){
		CSL_MAJOR = result;
	}
	
	mydev = csl_alloc();

	if(!mydev){
		printk(KERN_WARNING "CSL: Fail to add disk!\n");
		return -1;
	}

	dev = mydev;
	
	printk(KERN_INFO "DEVICE : CSL is successfully initialized with major number %d\n",CSL_MAJOR);
	return 0;
}



static void __exit csl_exit(void)
{
	del_gendisk(dev->gdisk);
	put_disk(dev->gdisk);

	blk_mq_destroy_queue(dev->queue);
	
	unregister_blkdev(CSL_MAJOR,DEV_NAME);
	blk_mq_free_tag_set(&dev->tag_set);
	printk(KERN_INFO "DEVICE : CSL is successfully unregistered!\n");
	kfree(dev);
}

module_init(csl_init);
module_exit(csl_exit);

MODULE_AUTHOR("MinyoungKim");
MODULE_DESCRIPTION("Virtual Block Device Driver");
MODULE_LICENSE("GPL");
