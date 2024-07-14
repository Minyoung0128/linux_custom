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

/*
* Display current L2P Map array
*/
static void display_index(void)
{
	unsigned long lba;
	void* ret;
	struct l2b_item* data;

	if(xa_empty(&dev->l2p_map)){
		pr_info("CSL : THERE IS NO ALLOCATE SECTOR");
		return;
	}

	pr_info("CSL : MAPPING INFO");
	pr_info("------------------------------------------------");

	xa_lock(&dev->l2p_map);
	xa_for_each(&dev->l2p_map, lba, ret)
	{
		data = (struct l2b_item*) ret;
		printk("LBA %lu		> 		PPN %d", data->lba, data->ppn);
	}
	
	pr_info("------------------------------------------------");
	xa_unlock(&dev->l2p_map);

}

static int read_from_file(char* filename, void* data, size_t size)
{	
	struct file* file;
	loff_t pos = 0;

	file = filp_open(filename, O_RDONLY, 0644);

	if(IS_ERR(file)|| file == NULL){
		pr_warn(FILE_OPEN_ERROR_MSG);
		return FAIL_EXIT;
	}
		
	// 여기는 파일이 있는 상태
	if(kernel_read(file, data, size, &pos) < 0){
		pr_warn(FILE_READ_ERROR_MSG);
		return FAIL_EXIT;
	}

	return SUCCESS_EXIT;
}


// 이미 저장되어 있는 csl dev 정보가 있으면 그걸 가져와 줌
// > XArray, list, Data 초기화를 수행
// > 없으면 각각 initialize만 해서 dev에 넣어주기 

static void csl_restore(struct csl_dev *dev){
	int i;

	unsigned int xa_entry_num = 0;
	unsigned int gc_entry_num;
	unsigned int *metadata_ptr;
	u8 *data_ptr;

	unsigned int offset;

	struct list_item* item;

	u8 *total_data;
	unsigned int total_data_size;

	pr_info("CSL : RESTORE START");

	// 1. offset, Xarray, list entry의 개수를 가져오고 전체 데이터를 읽어옴
	
	void* header_data = kmalloc(BACKUP_HEADER_SIZE, GFP_KERNEL);;
	
	if(IS_ERR(header_data)){
		pr_warn("MALLOC FAIL");
		goto nofile;
	}

	if(read_from_file(BACKUP_FILE_PATH, header_data, BACKUP_HEADER_SIZE)<0){
		pr_warn(FILE_READ_ERROR_MSG);
		goto nofile;
	}

	offset = *(unsigned int*)header_data;
	dev->offset = offset;

	xa_entry_num = *(unsigned int*)(header_data + OFFSET_SIZE);
	gc_entry_num = *(unsigned int*)(header_data + OFFSET_SIZE + sizeof(xa_entry_num));
	
	total_data_size = BACKUP_HEADER_SIZE + (xa_entry_num * XA_ENTRY_SIZE) + (gc_entry_num * GC_ENTRY_SIZE) + DEVICE_TOTAL_SIZE;
	
	total_data = kmalloc(total_data_size, GFP_KERNEL);

	if(read_from_file(BACKUP_FILE_PATH, total_data, total_data_size) < 0){
		pr_warn(FILE_READ_ERROR_MSG);
		goto nofile;
	}
	
	pr_info("There are %d XArray Entry, %d GC Entry > total data size is [%d] bytes", xa_entry_num, gc_entry_num, total_data_size);

	// 2. Xarray entry를 가져와 XArray에 저장
	
	struct l2b_item* l2b_item;

	pr_info("Start restore XArray data!");

	metadata_ptr = (unsigned int*)(total_data + BACKUP_HEADER_SIZE);
	
	xa_init(&dev->l2p_map);

	for(i = 0; i < xa_entry_num; i++){
		l2b_item = kmalloc(sizeof(struct l2b_item), GFP_KERNEL);
		l2b_item->lba = (unsigned long)(*metadata_ptr++);
		l2b_item->ppn = *metadata_ptr++;

		printk("XA lba : %lu | ppn : %d",l2b_item->lba, l2b_item->ppn);
		xa_store(&dev->l2p_map, l2b_item->lba, (void*)l2b_item, GFP_KERNEL);
	}

	pr_info("Complete to back up XArray data!");

	// 3. list index 복원
	
	pr_info("Start restore Garbage collection data!");

	INIT_LIST_HEAD(&dev->list);
	for(i = 0; i < gc_entry_num; i++){
		item = kmalloc(sizeof(struct list_item), GFP_KERNEL);
		item->sector = *metadata_ptr++;
		list_add_tail(&item->list_head, &dev->list);
		// printk("gc data : %d",item->sector);
	}
	
	pr_info("Complete to restore Garbage Collection data!");

	data_ptr = (u8*)metadata_ptr;
	memcpy(&data, data_ptr, DEVICE_TOTAL_SIZE);
	
	pr_info("CSL : RESTORE COMPLETE");
	display_index();
	return;

nofile:
	pr_warn(BACKUP_FAIL_MSG);
	xa_init(&dev->l2p_map);
	INIT_LIST_HEAD(&dev->list);
	return;

}

static int write_to_file(const char *filename, const void *data, size_t size) {
    struct file *file;
    loff_t pos = 0;

    file = filp_open(filename, O_WRONLY | O_CREAT, 0644);
    if (IS_ERR(file)) {
		pr_warn(FILE_OPEN_ERROR_MSG);
        return FAIL_EXIT;
    }

    if(kernel_write(file, data, size, &pos) < 0){
		pr_warn(FILE_WRITE_ERROR_MSG);
		return FAIL_EXIT;
	}

    return SUCCESS_EXIT;
}

static void csl_backup(void)
{
	
	pr_info("CSL : BACKUP START");
	display_index();
	unsigned int xa_entry_num=0;
	unsigned int gc_entry_num=0;
	unsigned int *metadata_ptr;

	void *data_ptr;
	struct list_item *litem;
	struct l2b_item* xa_item;

	u8 *total_data;
	unsigned int total_data_size = 0;

	// 1. XArray의 mapping data 개수 세오기
	void* xa_ret;
	unsigned long idx;
	if(!xa_empty(&dev->l2p_map)){
		xa_for_each(&dev->l2p_map, idx, xa_ret){
			xa_entry_num++;
		}
	}
	
	// 2. Garbage Collection 개수 count
	if(!list_empty(&dev->list)){
		gc_entry_num = list_count_nodes(&dev->list);
	}
	
	
	// 3. 배열 생성
	total_data_size = BACKUP_HEADER_SIZE + (xa_entry_num * XA_ENTRY_SIZE) + (gc_entry_num * GC_ENTRY_SIZE) + DEVICE_TOTAL_SIZE;
	total_data = kzalloc(total_data_size, GFP_KERNEL);
	printk("xa : %d, list : %d, total size : %d",xa_entry_num, gc_entry_num,total_data_size);
	
	if(IS_ERR(total_data) || total_data < 0 || total_data == NULL){
		pr_info("MALLOC ERROr");
		return;
	}
	
	// 3-1. header 정보 넣어주기

	*total_data++ = dev->offset;
	*total_data++ = xa_entry_num;
	*total_data++ = gc_entry_num;

	memcpy(total_data, &dev->offset, OFFSET_SIZE);
	memcpy(total_data + OFFSET_SIZE, &xa_entry_num, sizeof(xa_entry_num));
	memcpy(total_data + OFFSET_SIZE + sizeof(xa_entry_num), &gc_entry_num, sizeof(gc_entry_num));
	
	printk("make header");

	// 4. XArray 데이터 복사해오기 
	metadata_ptr = (unsigned int *)(total_data + BACKUP_HEADER_SIZE);

	xa_for_each(&dev->l2p_map, idx, xa_ret){
		xa_item = (struct l2b_item*)xa_ret;
		*metadata_ptr++ = (unsigned int)xa_item->lba;;
		*metadata_ptr++ = xa_item->ppn;
		pr_info("store xarray with lba %lu > ppn %d", xa_item->lba, xa_item->ppn);
	}
	
	printk("get xa data");

	// 5. GC Linked List 데이터 복사 
	metadata_ptr = (unsigned int*)(total_data + BACKUP_HEADER_SIZE + xa_entry_num * XA_ENTRY_SIZE);
	
	list_for_each_entry(litem, &dev->list, list_head){
		*metadata_ptr++ = litem->sector;
		pr_info("store gc data %d", litem->sector);
	}
	
	printk("get list data");
	data_ptr = total_data + BACKUP_HEADER_SIZE + (xa_entry_num * XA_ENTRY_SIZE) + (gc_entry_num * GC_ENTRY_SIZE);
    memcpy(data_ptr, &data, DEVICE_TOTAL_SIZE);

	if (write_to_file(BACKUP_FILE_PATH, total_data, total_data_size) < 0) {
        pr_warn(FILE_WRITE_ERROR_MSG);
		return;
    }

	pr_info("CSL : BACKUP COMPLETE");
	pr_info("There are %d XArray Entry, %d GC Entry > total data size is [%d] bytes", xa_entry_num, gc_entry_num, total_data_size);

    kfree(total_data);
}

static uint csl_gc(void)
{
	// device가 다 찬 경우 여기로 진입
	// garbage collection을 진행해서 새로운 ppn을 할당 

	struct list_item *entry;

	// pr_info("CSL : Start Garbage Collection");
	if(!list_empty(&dev->list))
	{
		entry = list_first_entry(&dev->list, struct list_item, list_head);
		list_del(&entry->list_head);
	
		// pr_info("CSL : %d sector collected", entry->sector);
		return entry->sector;
	}

	return -1;
}

static void csl_invalidate(uint ppn)
{
	struct list_item *item;

	// pr_info("CSL : Invalidata ppn [%d]", ppn);
	item = kmalloc(sizeof(struct list_item*), GFP_KERNEL);

	if(!item) {
		pr_warn("CSL : Fail To Allocate list item !");
		return;
		}

	item->sector = ppn;
	list_add_tail(&item->list_head, &dev->list);

	// pr_info("CSL : Success to Invalidate [%d]", ppn);

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
		int ppn_new = csl_gc();
		if(ppn_new < 0){
			return;
		}
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
		// pr_info("CSL : Start to Write");
		ret = xa_load(&dev->l2p_map, (unsigned long)start_sec);
		
		if(!ret){
			// address_val이 NULL > 이미 쓰여있는 데이터가 없으므로 invalidate하지 않아도 괜찮음
			l2b_item = kmalloc(sizeof(struct l2b_item), GFP_KERNEL);
			
			l2b_item->lba = start_sec;
			l2b_item->ppn = dev->offset;
			
			// pr_info("CSL : Allocate New page!");
			xa_store(&dev->l2p_map, l2b_item->lba, (void*)l2b_item, GFP_KERNEL);

			dev->offset+=num_sec;

			// printk(KERN_INFO "CLS : Start write LBA [%d] to PPN [%d]", l2b_item->lba, l2b_item->ppn);
			csl_write(l2b_item->ppn, buffer, num_sec);
			// printk(KERN_INFO "CLS : Finish Write LBA [%d] to PPN [%d]", l2b_item->lba, l2b_item->ppn);
			// display_index();
		}

		else{
			// 이미 할당받았던 자리가 있음 > 그거 invalidate 해주기
			l2b_item = (struct l2b_item*) ret;
			csl_invalidate(l2b_item->ppn);

			l2b_item->ppn = dev->offset; // 새로운 offset 할당
			
			dev->offset+=num_sec;
			
			// printk(KERN_INFO "CLS : Start write LBA [%d] to PPN [%d]", l2b_item->lba, l2b_item->ppn);
			csl_write(l2b_item->ppn, buffer, num_sec);
			// printk(KERN_INFO "CLS : Finish Write LBA [%d] to PPN [%d]", l2b_item->lba, l2b_item->ppn);
			// display_index();
		}
	}

	else {
		pr_info("CSL : Start to Read!");
		ret = xa_load(&dev->l2p_map, start_sec);
		if(!ret){
			printk(KERN_WARNING "PAGE FAULT!");
			return;
		}
		l2b_item = (struct l2b_item*) ret;
		// pr_info("CLS : Start Read LBA [%d] to PPN [%d]", l2b_item->lba, l2b_item->ppn);
		csl_read(l2b_item->ppn, buffer, num_sec);
		// pr_info("CLS : Finish Read LBA [%d] to PPN [%d]", l2b_item->lba, l2b_item->ppn);
	}
}


static void csl_get_request(struct request *rq)
{
	// request가 read, write인지 판별
	int isWrite = rq_data_dir(rq);

	sector_t start_sector = blk_rq_pos(rq);
	
	// pr_info("CSL : Request | isWrite = [%d] | start_sector = [%d] | sector_length : [%d] | byte_length : [%d]",isWrite,start_sector, sector_len, byte_len);

	struct bio_vec bvec;
	struct req_iterator iter;

	void* buffer;

	rq_for_each_segment(bvec, rq, iter){
		
		// request의 bio_vec을 가져와 파싱해줌
		// rq OR rq_cur?
		unsigned int num_sector = blk_rq_sectors(rq); 

		buffer = page_address(bvec.bv_page)+bvec.bv_offset;

		csl_transfer(dev, start_sector, num_sector, buffer, isWrite); // transfer로 들어가면 read or write를 실행

		start_sector += num_sector; 
	}
}

static blk_status_t csl_enqueue(struct blk_mq_hw_ctx *ctx, const struct blk_mq_queue_data *data){
	struct request *rq = data->rq;

	blk_mq_start_request(rq); // 커널에 device request를 시작한다고 알려주기 

	csl_get_request(rq);

	blk_mq_end_request(rq, BLK_STS_OK);

	return BLK_STS_OK;
}

static struct block_device_operations csl_fops = {
	.owner = THIS_MODULE,
	.open = csl_open,
	.release = csl_release,
	.ioctl = csl_ioctl
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

	mydev = kzalloc(sizeof(struct csl_dev), GFP_KERNEL); // kzmalloc로 커널 공간에 메모리 할당
	// GFP_KERNEL : 흔히 사용되는 flag로 메모리가 충분하지 않으면 sleep 상태가 된다. 

	if(!mydev){
		return NULL;
	}

	// printk(KERN_INFO "CSL : CSL DEVICE INIT LIST HEAD");
	// INIT_LIST_HEAD(&mydev->list);
	// printk(KERN_INFO "CSL : CSL DEVICE INIT LIST HEAD - FIN");
	// xa_init(&mydev->l2p_map);

	spin_lock_init(&mydev->csl_lock);

	// 2. tag set 할당

	mydev->tag_set.ops = &csl_mq_ops;
	mydev->tag_set.nr_hw_queues = 1;
	mydev->tag_set.queue_depth = 32;
	mydev->tag_set.numa_node = NUMA_NO_NODE;
	mydev->tag_set.cmd_size = 0;
	mydev->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
	mydev->tag_set.driver_data = mydev;

	error = blk_mq_alloc_tag_set(&mydev->tag_set);

	if(error){
		kfree(mydev);
		return NULL;
	}

	// 3. gendisk 할당
	
	disk = blk_mq_alloc_disk(&mydev->tag_set, &queue_limit, mydev->queue);
	
	disk->major = CSL_MAJOR;
	disk->fops = &csl_fops;
	disk->first_minor = DEV_FIRST_MINOR;
	disk->minors = DEV_MINORS;
	disk->private_data = mydev;

	snprintf(disk->disk_name, 32, "CSL");

	if(IS_ERR(disk)){
		blk_mq_free_tag_set(&mydev->tag_set);
		kfree(mydev);
		return NULL;
	}

	mydev->gdisk = disk;
	mydev->queue = disk->queue;

	error = add_disk(disk); 
	if(error){
		blk_mq_free_tag_set(&mydev->tag_set);
		kfree(mydev);
		return NULL;
	}
	set_capacity(mydev->gdisk, DEV_SECTOR_NUM);

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
	
	csl_restore(mydev);

	dev = mydev;
	
	printk(KERN_INFO "DEVICE : CSL is successfully initialized with major number %d\n",CSL_MAJOR);
	return 0;
}


static void csl_free(void)
{
	blk_mq_destroy_queue(dev->queue);
	xa_destroy(&dev->l2p_map);
	unregister_blkdev(CSL_MAJOR,DEV_NAME);
	blk_mq_free_tag_set(&dev->tag_set);

	struct list_head *e, *tmp;
	list_for_each_safe(e, tmp, &dev->list){
		list_del(e);
		kfree(list_entry(e, struct list_item, list_head));
	}
	return;
}
static void __exit csl_exit(void)
{
	csl_backup();
	del_gendisk(dev->gdisk);
	put_disk(dev->gdisk);

	csl_free();

	printk(KERN_INFO "DEVICE : CSL is successfully unregistered!\n");
	kfree(dev);
}

module_init(csl_init);
module_exit(csl_exit);

MODULE_AUTHOR("MinyoungKim");
MODULE_DESCRIPTION("Virtual Block Device Driver");
MODULE_LICENSE("GPL");
