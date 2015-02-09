/*  sun4m_irq.c
 *  arch/sparc/kernel/sun4m_irq.c:
 *
 *  djhr: Hacked out of irq.c into a CPU dependent version.
 *
 *  Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 *  Copyright (C) 1995 Miguel de Icaza (miguel@nuclecu.unam.mx)
 *  Copyright (C) 1995 Pete A. Zaitcev (zaitcev@yahoo.com)
 *  Copyright (C) 1996 Dave Redman (djhr@tadpole.co.uk)
 */

#include <linux/errno.h>
#include <linux/linkage.h>
#include <linux/kernel_stat.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/smp.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include <asm/ptrace.h>
#include <asm/processor.h>
#include <asm/system.h>
#include <asm/psr.h>
#include <asm/vaddrs.h>
#include <asm/timer.h>
#include <asm/openprom.h>
#include <asm/oplib.h>
#include <asm/traps.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/smp.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/cacheflush.h>

#include "irq.h"

struct sun4m_irq_percpu {
	u32		pending;
	u32		clear;
	u32		set;
};

struct sun4m_irq_global {
	u32		pending;
	u32		mask;
	u32		mask_clear;
	u32		mask_set;
	u32		interrupt_target;
};

/* Code in entry.S needs to get at these register mappings.  */
struct sun4m_irq_percpu __iomem *sun4m_irq_percpu[SUN4M_NCPUS];
struct sun4m_irq_global __iomem *sun4m_irq_global;

/* Dave Redman (djhr@tadpole.co.uk)
 * The sun4m interrupt registers.
 */
#define SUN4M_INT_ENABLE  	0x80000000
#define SUN4M_INT_E14     	0x00000080
#define SUN4M_INT_E10     	0x00080000

#define SUN4M_HARD_INT(x)	(0x000000001 << (x))
#define SUN4M_SOFT_INT(x)	(0x000010000 << (x))

#define	SUN4M_INT_MASKALL	0x80000000	  /* mask all interrupts */
#define	SUN4M_INT_MODULE_ERR	0x40000000	  /* module error */
#define	SUN4M_INT_M2S_WRITE_ERR	0x20000000	  /* write buffer error */
#define	SUN4M_INT_ECC_ERR	0x10000000	  /* ecc memory error */
#define	SUN4M_INT_VME_ERR	0x08000000	  /* vme async error */
#define	SUN4M_INT_FLOPPY	0x00400000	  /* floppy disk */
#define	SUN4M_INT_MODULE	0x00200000	  /* module interrupt */
#define	SUN4M_INT_VIDEO		0x00100000	  /* onboard video */
#define	SUN4M_INT_REALTIME	0x00080000	  /* system timer */
#define	SUN4M_INT_SCSI		0x00040000	  /* onboard scsi */
#define	SUN4M_INT_AUDIO		0x00020000	  /* audio/isdn */
#define	SUN4M_INT_ETHERNET	0x00010000	  /* onboard ethernet */
#define	SUN4M_INT_SERIAL	0x00008000	  /* serial ports */
#define	SUN4M_INT_KBDMS		0x00004000	  /* keyboard/mouse */
#define	SUN4M_INT_SBUSBITS	0x00003F80	  /* sbus int bits */
#define	SUN4M_INT_VMEBITS	0x0000007F	  /* vme int bits */

#define	SUN4M_INT_ERROR		(SUN4M_INT_MODULE_ERR |    \
				 SUN4M_INT_M2S_WRITE_ERR | \
				 SUN4M_INT_ECC_ERR |       \
				 SUN4M_INT_VME_ERR)

#define SUN4M_INT_SBUS(x)	(1 << (x+7))
#define SUN4M_INT_VME(x)	(1 << (x))

/* Interrupt levels used by OBP */
#define	OBP_INT_LEVEL_SOFT	0x10
#define	OBP_INT_LEVEL_ONBOARD	0x20
#define	OBP_INT_LEVEL_SBUS	0x30
#define	OBP_INT_LEVEL_VME	0x40


static unsigned long irq_mask[0x50] = {
	/* SMP */
	0,  SUN4M_SOFT_INT(1),
	SUN4M_SOFT_INT(2),  SUN4M_SOFT_INT(3),
	SUN4M_SOFT_INT(4),  SUN4M_SOFT_INT(5),
	SUN4M_SOFT_INT(6),  SUN4M_SOFT_INT(7),
	SUN4M_SOFT_INT(8),  SUN4M_SOFT_INT(9),
	SUN4M_SOFT_INT(10), SUN4M_SOFT_INT(11),
	SUN4M_SOFT_INT(12), SUN4M_SOFT_INT(13),
	SUN4M_SOFT_INT(14), SUN4M_SOFT_INT(15),
	/* soft */
	0,  SUN4M_SOFT_INT(1),
	SUN4M_SOFT_INT(2),  SUN4M_SOFT_INT(3),
	SUN4M_SOFT_INT(4),  SUN4M_SOFT_INT(5),
	SUN4M_SOFT_INT(6),  SUN4M_SOFT_INT(7),
	SUN4M_SOFT_INT(8),  SUN4M_SOFT_INT(9),
	SUN4M_SOFT_INT(10), SUN4M_SOFT_INT(11),
	SUN4M_SOFT_INT(12), SUN4M_SOFT_INT(13),
	SUN4M_SOFT_INT(14), SUN4M_SOFT_INT(15),
	/* onboard */
	0, 0, 0, 0,
	SUN4M_INT_SCSI,  0, SUN4M_INT_ETHERNET, 0,
	SUN4M_INT_VIDEO, SUN4M_INT_MODULE,
	SUN4M_INT_REALTIME, SUN4M_INT_FLOPPY,
	(SUN4M_INT_SERIAL | SUN4M_INT_KBDMS),
	SUN4M_INT_AUDIO, 0, SUN4M_INT_MODULE_ERR,
	/* sbus */
	0, 0, SUN4M_INT_SBUS(0), SUN4M_INT_SBUS(1),
	0, SUN4M_INT_SBUS(2), 0, SUN4M_INT_SBUS(3),
	0, SUN4M_INT_SBUS(4), 0, SUN4M_INT_SBUS(5),
	0, SUN4M_INT_SBUS(6), 0, 0,
	/* vme */
	0, 0, SUN4M_INT_VME(0), SUN4M_INT_VME(1),
	0, SUN4M_INT_VME(2), 0, SUN4M_INT_VME(3),
	0, SUN4M_INT_VME(4), 0, SUN4M_INT_VME(5),
	0, SUN4M_INT_VME(6), 0, 0
};

static unsigned long sun4m_get_irqmask(unsigned int irq)
{
	unsigned long mask;
    
	if (irq < 0x50)
		mask = irq_mask[irq];
	else
		mask = 0;

	if (!mask)
		printk(KERN_ERR "sun4m_get_irqmask: IRQ%d has no valid mask!\n",
		       irq);

	return mask;
}

static void sun4m_disable_irq(unsigned int irq_nr)
{
	unsigned long mask, flags;
	int cpu = smp_processor_id();

	mask = sun4m_get_irqmask(irq_nr);
	local_irq_save(flags);
	if (irq_nr > 15)
		sbus_writel(mask, &sun4m_irq_global->mask_set);
	else
		sbus_writel(mask, &sun4m_irq_percpu[cpu]->set);
	local_irq_restore(flags);    
}

static void sun4m_enable_irq(unsigned int irq_nr)
{
	unsigned long mask, flags;
	int cpu = smp_processor_id();

        if (irq_nr != 0x0b) {
		mask = sun4m_get_irqmask(irq_nr);
		local_irq_save(flags);
		if (irq_nr > 15)
			sbus_writel(mask, &sun4m_irq_global->mask_clear);
		else
			sbus_writel(mask, &sun4m_irq_percpu[cpu]->clear);
		local_irq_restore(flags);    
	} else {
		local_irq_save(flags);
		sbus_writel(SUN4M_INT_FLOPPY, &sun4m_irq_global->mask_clear);
		local_irq_restore(flags);
	}
}

static unsigned long cpu_pil_to_imask[16] = {
/*0*/	0x00000000,
/*1*/	0x00000000,
/*2*/	SUN4M_INT_SBUS(0) | SUN4M_INT_VME(0),
/*3*/	SUN4M_INT_SBUS(1) | SUN4M_INT_VME(1),
/*4*/	SUN4M_INT_SCSI,
/*5*/	SUN4M_INT_SBUS(2) | SUN4M_INT_VME(2),
/*6*/	SUN4M_INT_ETHERNET,
/*7*/	SUN4M_INT_SBUS(3) | SUN4M_INT_VME(3),
/*8*/	SUN4M_INT_VIDEO,
/*9*/	SUN4M_INT_SBUS(4) | SUN4M_INT_VME(4) | SUN4M_INT_MODULE_ERR,
/*10*/	SUN4M_INT_REALTIME,
/*11*/	SUN4M_INT_SBUS(5) | SUN4M_INT_VME(5) | SUN4M_INT_FLOPPY,
/*12*/	SUN4M_INT_SERIAL  | SUN4M_INT_KBDMS,
/*13*/	SUN4M_INT_SBUS(6) | SUN4M_INT_VME(6) | SUN4M_INT_AUDIO,
/*14*/	SUN4M_INT_E14,
/*15*/	SUN4M_INT_ERROR
};

/* We assume the caller has disabled local interrupts when these are called,
 * or else very bizarre behavior will result.
 */
static void sun4m_disable_pil_irq(unsigned int pil)
{
	sbus_writel(cpu_pil_to_imask[pil], &sun4m_irq_global->mask_set);
}

static void sun4m_enable_pil_irq(unsigned int pil)
{
	sbus_writel(cpu_pil_to_imask[pil], &sun4m_irq_global->mask_clear);
}

#ifdef CONFIG_SMP
static void sun4m_send_ipi(int cpu, int level)
{
	unsigned long mask = sun4m_get_irqmask(level);
	sbus_writel(mask, &sun4m_irq_percpu[cpu]->set);
}

static void sun4m_clear_ipi(int cpu, int level)
{
	unsigned long mask = sun4m_get_irqmask(level);
	sbus_writel(mask, &sun4m_irq_percpu[cpu]->clear);
}

static void sun4m_set_udt(int cpu)
{
	sbus_writel(cpu, &sun4m_irq_global->interrupt_target);
}
#endif

struct sun4m_timer_percpu {
	u32		l14_limit;
	u32		l14_count;
	u32		l14_limit_noclear;
	u32		user_timer_start_stop;
};

static struct sun4m_timer_percpu __iomem *timers_percpu[SUN4M_NCPUS];

struct sun4m_timer_global {
	u32		l10_limit;
	u32		l10_count;
	u32		l10_limit_noclear;
	u32		reserved;
	u32		timer_config;
};

static struct sun4m_timer_global __iomem *timers_global;

#define TIMER_IRQ  	(OBP_INT_LEVEL_ONBOARD | 10)

unsigned int lvl14_resolution = (((1000000/HZ) + 1) << 10);

static void sun4m_clear_clock_irq(void)
{
	sbus_readl(&timers_global->l10_limit);
}

void sun4m_nmi(struct pt_regs *regs)
{
	unsigned long afsr, afar, si;

	printk(KERN_ERR "Aieee: sun4m NMI received!\n");
	__asm__ __volatile__("mov 0x500, %%g1\n\t"
			     "lda [%%g1] 0x4, %0\n\t"
			     "mov 0x600, %%g1\n\t"
			     "lda [%%g1] 0x4, %1\n\t" :
			     "=r" (afsr), "=r" (afar));
	printk(KERN_ERR "afsr=%08lx afar=%08lx\n", afsr, afar);
	si = sbus_readl(&sun4m_irq_global->pending);
	printk(KERN_ERR "si=%08lx\n", si);
	if (si & SUN4M_INT_MODULE_ERR)
		printk(KERN_ERR "Module async error\n");
	if (si & SUN4M_INT_M2S_WRITE_ERR)
		printk(KERN_ERR "MBus/SBus async error\n");
	if (si & SUN4M_INT_ECC_ERR)
		printk(KERN_ERR "ECC memory error\n");
	if (si & SUN4M_INT_VME_ERR)
		printk(KERN_ERR "VME async error\n");
	printk(KERN_ERR "you lose buddy boy...\n");
	show_regs(regs);
	prom_halt();
}

/* Exported for sun4m_smp.c */
void sun4m_clear_profile_irq(int cpu)
{
	sbus_readl(&timers_percpu[cpu]->l14_limit);
}

static void sun4m_load_profile_irq(int cpu, unsigned int limit)
{
	sbus_writel(limit, &timers_percpu[cpu]->l14_limit);
}

static void __init sun4m_init_timers(irq_handler_t counter_fn)
{
	struct device_node *dp = of_find_node_by_name(NULL, "counter");
	int i, err, len, num_cpu_timers;
	const u32 *addr;

	if (!dp) {
		printk(KERN_ERR "sun4m_init_timers: No 'counter' node.\n");
		return;
	}

	addr = of_get_property(dp, "address", &len);
	of_node_put(dp);
	if (!addr) {
		printk(KERN_ERR "sun4m_init_timers: No 'address' prop.\n");
		return;
	}

	num_cpu_timers = (len / sizeof(u32)) - 1;
	for (i = 0; i < num_cpu_timers; i++) {
		timers_percpu[i] = (void __iomem *)
			(unsigned long) addr[i];
	}
	timers_global = (void __iomem *)
		(unsigned long) addr[num_cpu_timers];

	sbus_writel((((1000000/HZ) + 1) << 10), &timers_global->l10_limit);

	master_l10_counter = &timers_global->l10_count;

	err = request_irq(TIMER_IRQ, counter_fn,
			  (IRQF_DISABLED | SA_STATIC_ALLOC), "timer", NULL);
	if (err) {
		printk(KERN_ERR "sun4m_init_timers: Register IRQ error %d.\n",
			err);
		return;
	}

	for (i = 0; i < num_cpu_timers; i++)
		sbus_writel(0, &timers_percpu[i]->l14_limit);
	if (num_cpu_timers == 4)
		sbus_writel(SUN4M_INT_E14, &sun4m_irq_global->mask_set);

#ifdef CONFIG_SMP
	{
		unsigned long flags;
		extern unsigned long lvl14_save[4];
		struct tt_entry *trap_table = &sparc_ttable[SP_TRAP_IRQ1 + (14 - 1)];

		/* For SMP we use the level 14 ticker, however the bootup code
		 * has copied the firmware's level 14 vector into the boot cpu's
		 * trap table, we must fix this now or we get squashed.
		 */
		local_irq_save(flags);
		trap_table->inst_one = lvl14_save[0];
		trap_table->inst_two = lvl14_save[1];
		trap_table->inst_three = lvl14_save[2];
		trap_table->inst_four = lvl14_save[3];
		local_flush_cache_all();
		local_irq_restore(flags);
	}
#endif
}

void __init sun4m_init_IRQ(void)
{
	struct device_node *dp = of_find_node_by_name(NULL, "interrupt");
	int len, i, mid, num_cpu_iregs;
	const u32 *addr;

	if (!dp) {
		printk(KERN_ERR "sun4m_init_IRQ: No 'interrupt' node.\n");
		return;
	}

	addr = of_get_property(dp, "address", &len);
	of_node_put(dp);
	if (!addr) {
		printk(KERN_ERR "sun4m_init_IRQ: No 'address' prop.\n");
		return;
	}

	num_cpu_iregs = (len / sizeof(u32)) - 1;
	for (i = 0; i < num_cpu_iregs; i++) {
		sun4m_irq_percpu[i] = (void __iomem *)
			(unsigned long) addr[i];
	}
	sun4m_irq_global = (void __iomem *)
		(unsigned long) addr[num_cpu_iregs];

	local_irq_disable();

	sbus_writel(~SUN4M_INT_MASKALL, &sun4m_irq_global->mask_set);
	for (i = 0; !cpu_find_by_instance(i, NULL, &mid); i++)
		sbus_writel(~0x17fff, &sun4m_irq_percpu[mid]->clear);

	if (num_cpu_iregs == 4)
		sbus_writel(0, &sun4m_irq_global->interrupt_target);

	BTFIXUPSET_CALL(enable_irq, sun4m_enable_irq, BTFIXUPCALL_NORM);
	BTFIXUPSET_CALL(disable_irq, sun4m_disable_irq, BTFIXUPCALL_NORM);
	BTFIXUPSET_CALL(enable_pil_irq, sun4m_enable_pil_irq, BTFIXUPCALL_NORM);
	BTFIXUPSET_CALL(disable_pil_irq, sun4m_disable_pil_irq, BTFIXUPCALL_NORM);
	BTFIXUPSET_CALL(clear_clock_irq, sun4m_clear_clock_irq, BTFIXUPCALL_NORM);
	BTFIXUPSET_CALL(load_profile_irq, sun4m_load_profile_irq, BTFIXUPCALL_NORM);
	sparc_init_timers = sun4m_init_timers;
#ifdef CONFIG_SMP
	BTFIXUPSET_CALL(set_cpu_int, sun4m_send_ipi, BTFIXUPCALL_NORM);
	BTFIXUPSET_CALL(clear_cpu_int, sun4m_clear_ipi, BTFIXUPCALL_NORM);
	BTFIXUPSET_CALL(set_irq_udt, sun4m_set_udt, BTFIXUPCALL_NORM);
#endif

	/* Cannot enable interrupts until OBP ticker is disabled. */
}
