#include <linux/module.h>
#include <linux/linkage.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/user.h>
#include <linux/elfcore.h>
#include <linux/in6.h>
#include <linux/interrupt.h>

#include <asm/setup.h>
#include <asm/machdep.h>
#include <asm/pgalloc.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/checksum.h>
#include <asm/current.h>

extern int dump_fpu(struct pt_regs *, elf_fpregset_t *);

/* platform dependent support */

EXPORT_SYMBOL(__ioremap);
EXPORT_SYMBOL(iounmap);
EXPORT_SYMBOL(dump_fpu);

EXPORT_SYMBOL(ip_fast_csum);

EXPORT_SYMBOL(kernel_thread);

/* Networking helper routines. */
EXPORT_SYMBOL(csum_partial_copy_nocheck);

/* The following are special because they're not called
   explicitly (the C compiler generates them).  Fortunately,
   their interface isn't gonna change any time soon now, so
   it's OK to leave it out of version control.  */
EXPORT_SYMBOL(memcpy);
EXPORT_SYMBOL(memset);

/*
 * libgcc functions - functions that are used internally by the
 * compiler...  (prototypes are not correct though, but that
 * doesn't really matter since they're not versioned).
 */
extern void __ashldi3(void);
extern void __ashrdi3(void);
extern void __divsi3(void);
extern void __lshrdi3(void);
extern void __modsi3(void);
extern void __muldi3(void);
extern void __mulsi3(void);
extern void __udivsi3(void);
extern void __umodsi3(void);

        /* gcc lib functions */
EXPORT_SYMBOL(__ashldi3);
EXPORT_SYMBOL(__ashrdi3);
EXPORT_SYMBOL(__divsi3);
EXPORT_SYMBOL(__lshrdi3);
EXPORT_SYMBOL(__modsi3);
EXPORT_SYMBOL(__muldi3);
EXPORT_SYMBOL(__mulsi3);
EXPORT_SYMBOL(__udivsi3);
EXPORT_SYMBOL(__umodsi3);

#ifdef CONFIG_COLDFIRE
extern unsigned int *dma_device_address;
extern unsigned long dma_base_addr, _ramend;
EXPORT_SYMBOL(dma_base_addr);
EXPORT_SYMBOL(dma_device_address);
EXPORT_SYMBOL(_ramend);

extern asmlinkage void trap(void);
extern void	*_ramvec;
EXPORT_SYMBOL(trap);
EXPORT_SYMBOL(_ramvec);
#endif /* CONFIG_COLDFIRE */
