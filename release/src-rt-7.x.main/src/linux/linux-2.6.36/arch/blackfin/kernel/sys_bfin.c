/*
 * contains various random system calls that have a non-standard
 * calling sequence on the Linux/Blackfin platform.
 *
 * Copyright 2004-2009 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later
 */

#include <linux/spinlock.h>
#include <linux/sem.h>
#include <linux/msg.h>
#include <linux/shm.h>
#include <linux/syscalls.h>
#include <linux/mman.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/ipc.h>
#include <linux/unistd.h>

#include <asm/cacheflush.h>
#include <asm/dma.h>

asmlinkage void *sys_sram_alloc(size_t size, unsigned long flags)
{
	return sram_alloc_with_lsl(size, flags);
}

asmlinkage int sys_sram_free(const void *addr)
{
	return sram_free_with_lsl(addr);
}

asmlinkage void *sys_dma_memcpy(void *dest, const void *src, size_t len)
{
	return safe_dma_memcpy(dest, src, len);
}

#if defined(CONFIG_FB) || defined(CONFIG_FB_MODULE)
#include <linux/fb.h>
unsigned long get_fb_unmapped_area(struct file *filp, unsigned long orig_addr,
	unsigned long len, unsigned long pgoff, unsigned long flags)
{
	struct fb_info *info = filp->private_data;
	return (unsigned long)info->screen_base;
}
EXPORT_SYMBOL(get_fb_unmapped_area);
#endif

/* Needed for legacy userspace atomic emulation */
static DEFINE_SPINLOCK(bfin_spinlock_lock);

#ifdef CONFIG_SYS_BFIN_SPINLOCK_L1
__attribute__((l1_text))
#endif
asmlinkage int sys_bfin_spinlock(int *p)
{
	int ret, tmp = 0;

	spin_lock(&bfin_spinlock_lock); /* This would also hold kernel preemption. */
	ret = get_user(tmp, p);
	if (likely(ret == 0)) {
		if (unlikely(tmp))
			ret = 1;
		else
			put_user(1, p);
	}
	spin_unlock(&bfin_spinlock_lock);

	return ret;
}
