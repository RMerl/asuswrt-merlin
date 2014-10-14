/*
 * linux/arch/h8300/kernel/sys_h8300.c
 *
 * This file contains various random system calls that
 * have a non-standard calling sequence on the H8/300
 * platform.
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/sem.h>
#include <linux/msg.h>
#include <linux/shm.h>
#include <linux/stat.h>
#include <linux/syscalls.h>
#include <linux/mman.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/ipc.h>

#include <asm/setup.h>
#include <asm/uaccess.h>
#include <asm/cachectl.h>
#include <asm/traps.h>
#include <asm/unistd.h>

/* sys_cacheflush -- no support.  */
asmlinkage int
sys_cacheflush (unsigned long addr, int scope, int cache, unsigned long len)
{
	return -EINVAL;
}

asmlinkage int sys_getpagesize(void)
{
	return PAGE_SIZE;
}

#if defined(CONFIG_SYSCALL_PRINT)
asmlinkage void syscall_print(void *dummy,...)
{
	struct pt_regs *regs = (struct pt_regs *) ((unsigned char *)&dummy-4);
	printk("call %06lx:%ld 1:%08lx,2:%08lx,3:%08lx,ret:%08lx\n",
               ((regs->pc)&0xffffff)-2,regs->orig_er0,regs->er1,regs->er2,regs->er3,regs->er0);
}
#endif

/*
 * Do a system call from kernel instead of calling sys_execve so we
 * end up with proper pt_regs.
 */
int kernel_execve(const char *filename,
		  const char *const argv[],
		  const char *const envp[])
{
	register long res __asm__("er0");
	register const char *const *_c __asm__("er3") = envp;
	register const char *const *_b __asm__("er2") = argv;
	register const char * _a __asm__("er1") = filename;
	__asm__ __volatile__ ("mov.l %1,er0\n\t"
			"trapa	#0\n\t"
			: "=r" (res)
			: "g" (__NR_execve),
			  "g" (_a),
			  "g" (_b),
			  "g" (_c)
			: "cc", "memory");
	return res;
}


