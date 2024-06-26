#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/fs.h>

int CLS_MAJOR= 0; // save the major number of the device

int cls_open(struct inode *inode, struct file *filp)
{
	printk("CLS Device Drive open !\n");
	return 0;
}

int cls_release(struct inode *inode, struct file *filp)
{
	printk("CLS Device Drive released!\n");
	return 0;
}

int cls_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg )
{
	printk("CLS Device Drive ioctl!\n");
	return 0;
}



static struct file_operations cls_fileops = {
	.open = cls_open,
	.release = cls_release,
};

int cls_init(void)
{
	int result;

	result = register_blkdev(CLS_MAJOR, "csl");
	
	// error handling
	if(result < 0)
	{
		printk(KERN_WARNING "CLS: Fail to get major number!\n");
		return result;
	}

	if( CLS_MAJOR == 0){
		CLS_MAJOR = result;
	}
	
	printk(KERN_INFO "DEVICE : CLS is successfully initialized with major number %d\n",CLS_MAJOR);
	return 0;
}



void cls_exit(void)
{
	unregister_blkdev(CLS_MAJOR,"cls");
	printk(KERN_INFO "DEVICE : CLS is successfully unregistered!\n");
	
}

module_init(cls_init);
module_exit(cls_exit);

MODULE_AUTHOR("Minyoung   kim");
MODULE_DESCRIPTION("Virtual Block Device Driver");
MODULE_LICENSE("@min0");
