/*
* ARM A9 MPCORE Platform base
*/


#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include <linux/clockchips.h>
#include <linux/ioport.h>
#include <linux/cpumask.h>

#include <asm/mach/map.h>
#include <asm/smp_twd.h>
#include <asm/hardware/gic.h>

#include <plat/mpcore.h>
#include <mach/io_map.h>

#include <bcmutils.h>
#include <siutils.h>
#include <bcmdefs.h>
#include <bcmdevs.h>

/* Global SB handle */
extern si_t *bcm947xx_sih;
#define sih bcm947xx_sih

extern int _chipid;

/* Globals */
static void __iomem * periphbase;
extern void __iomem * twd_base;	/* declared in arch/arm/kernel/smp_twd.c */

void __iomem * scu_base_addr(void)
{
	if (BCM53573_CHIP(_chipid))
		return NULL;
	else
		return (periphbase + MPCORE_SCU_OFF);
}

/*
 * Local per-CPU timer support
 * Called from arch/arm/kernel/smp.c
 * Implemented in arch/arm/kernel/smp_twd.c
 * All that is needed is to set the base address in mpcore_init() and irq here.
 */
void __cpuinit local_timer_setup(struct clock_event_device *evt)
{
	unsigned int cpu = smp_processor_id();
	if (BCM53573_CHIP(_chipid)) {
		/* Not needed for BCM53573 */
		return;
	}
	printk(KERN_INFO "MPCORE Private timer setup CPU%u\n", cpu);

	evt->retries = 0;
	evt->irq = MPCORE_IRQ_LOCALTIMER;
	twd_timer_setup(evt);
}

void __init mpcore_map_io(void)
{
	struct map_desc desc;
	phys_addr_t base_addr;

	/*
	 * Cortex A9 Architecture Manual specifies this as a way to get
	 * MPCORE PERHIPHBASE address at run-time
	 */
	asm( "mrc p15,4,%0,c15,c0,0 @ Read Configuration Base Address Register"
		: "=&r" (base_addr) : : "cc" );

	printk(KERN_INFO "MPCORE found at %p\n", (void *)base_addr);

	/* Fix-map the entire PERIPHBASE 2*4K register block */
	desc.virtual = MPCORE_BASE_VA;
	desc.pfn = __phys_to_pfn(base_addr);
	desc.length = (BCM53573_CHIP(_chipid)) ? SZ_32K : SZ_8K;
	desc.type = MT_DEVICE;

	iotable_init(&desc, 1);

	periphbase = (void *) MPCORE_BASE_VA;
	if (BCM53573_CHIP(_chipid)) {
		twd_base = NULL;
	}
	else {
		/* Local timer code needs just the register base address */
		twd_base = periphbase + MPCORE_LTIMER_OFF;
	}
}

void __init mpcore_init_gic(void)
{
	printk(KERN_INFO "MPCORE GIC init\n");

	/* Init GIC interrupt distributor */
	gic_dist_init(0, periphbase + MPCORE_GIC_DIST_OFF, 1);

	/* Initialize the GIC CPU interface for the boot processor */
	if (BCM53573_CHIP(_chipid)) {
		gic_cpu_init(0, periphbase + MPCORE_GIC_CPUIF_OFF_CA7);
	} else {
		gic_cpu_init(0, periphbase + MPCORE_GIC_CPUIF_OFF);
	}
}

void __init mpcore_init_timer(unsigned long perphclk_freq)
{
	/* Init Global Timer */
	if (BCM53573_CHIP(sih->chip)) {
		mpcore_gtimer_init(NULL, perphclk_freq, MPCORE_IRQ_PPI1TIMER);
	} else {
		mpcore_gtimer_init(periphbase + MPCORE_GTIMER_OFF,
			perphclk_freq, MPCORE_IRQ_GLOBALTIMER);
	}
}

/*
 * For SMP - initialize GIC CPU interface for secondary cores
 */
void __cpuinit mpcore_cpu_init(void)
{
	if (BCM53573_CHIP(_chipid))
		return;
	/* Initialize the GIC CPU interface for the next processor */
	gic_cpu_init(0, periphbase + MPCORE_GIC_CPUIF_OFF);
}
