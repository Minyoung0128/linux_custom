#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/swap.h>
#include <linux/vmstat.h>
#include <linux/syscalls.h>
SYSCALL_DEFINE0(getmemutil)
{
	struct sysinfo i;

	si_meminfo(&i);
	si_swapinfo(&i);

	int mem_util = (10000*(i.totalram-i.freeram))/i.totalram;
	if(mem_util<0){
		return 0;
	}
	
	return mem_util;
}

