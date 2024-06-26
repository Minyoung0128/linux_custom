#include <linux/module.h>
#include <linux/kernel.h>

// initialize module

int __init hello_init(void){
        printk(KERN_DEBUG "Hello, world!\n");
        return 0;
}


void __exit hello_exit(void){
        printk(KERN_DEBUG "Bye, world!\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_AUTHOR("Minyoung   kim");
MODULE_DESCRIPTION("Say hello to world");
MODULE_LICENSE("MINYOUNG");
