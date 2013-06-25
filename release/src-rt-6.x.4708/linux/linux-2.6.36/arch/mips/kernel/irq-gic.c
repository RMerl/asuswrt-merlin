#undef DEBUG

#include <linux/bitmap.h>
#include <linux/init.h>
#include <linux/smp.h>

#include <asm/io.h>
#include <asm/gic.h>
#include <asm/gcmpregs.h>

#ifdef CONFIG_MIPS_SEAD3
#include <asm/mips-boards/sead3int.h>
#else
#include <asm/mips-boards/maltaint.h>
#endif

#include <asm/irq.h>
#include <linux/hardirq.h>
#include <asm-generic/bitops/find.h>
#include <asm/traps.h>

unsigned long _gic_base;
static unsigned int _irqbase;
static unsigned int gic_irq_flags[GIC_NUM_INTRS];
#define GIC_IRQ_FLAG_EDGE      0x0001

/* The index into this array is the vector # of the interrupt. */
static struct gic_shared_intr_map gic_shared_intr_map[GIC_NUM_INTRS];

struct gic_pcpu_mask pcpu_masks[NR_CPUS];
static struct gic_pending_regs pending_regs[NR_CPUS];
static struct gic_intrmask_regs intrmask_regs[NR_CPUS];

unsigned int gic_get_timer_pending(void)
{
	unsigned int vpe_pending;

	GICWRITE(GIC_REG(VPE_LOCAL, GIC_VPE_OTHER_ADDR), 0);
	GICREAD(GIC_REG(VPE_OTHER, GIC_VPE_PEND), vpe_pending);
	return vpe_pending & GIC_VPE_PEND_TIMER_MSK;
}

/* Helper function to enable the interrupt */
/* NOTE: the _irqbase have already been removed. */
void gic_enable_interrupt(int irq_vec)
{
#ifdef CONFIG_MIPS_SEAD3
	unsigned int i;
	unsigned int irq_source;

	/* enable all the interrupts associated with this vector */
	for (i = 0; i < gic_shared_intr_map[irq_vec].num_shared_intr; i++) {
		irq_source = gic_shared_intr_map[irq_vec].intr_list[i];
		GIC_SET_INTR_MASK(irq_source);
	}
	/* enable all local interrupts associated with this vector */
	if (gic_shared_intr_map[irq_vec].local_intr_mask) {
		GICWRITE(GIC_REG(VPE_LOCAL, GIC_VPE_OTHER_ADDR), 0);
		GICWRITE(GIC_REG(VPE_OTHER, GIC_VPE_SMASK), gic_shared_intr_map[irq_vec].local_intr_mask);
	}
#else
	GIC_SET_INTR_MASK(irq_vec);
#endif
}

/* Helper function to disable the interrupt */
/* NOTE: the _irqbase have already been removed. */
void gic_disable_interrupt(int irq_vec)
{
#ifdef CONFIG_MIPS_SEAD3
	unsigned int i;
	unsigned int irq_source;

	/* disable all the interrupts associated with this vector */
	for (i = 0; i < gic_shared_intr_map[irq_vec].num_shared_intr; i++) {
		irq_source = gic_shared_intr_map[irq_vec].intr_list[i];
		GIC_CLR_INTR_MASK(irq_source);
	}
	/* disable all local interrupts associated with this vector */
	if (gic_shared_intr_map[irq_vec].local_intr_mask) {
		GICWRITE(GIC_REG(VPE_LOCAL, GIC_VPE_OTHER_ADDR), 0);
		GICWRITE(GIC_REG(VPE_OTHER, GIC_VPE_RMASK), gic_shared_intr_map[irq_vec].local_intr_mask);
	}
#else
	GIC_CLR_INTR_MASK(irq_vec);
#endif
}

void gic_bind_eic_interrupt(int irq, int set)
{
	irq = irq - GIC_PIN_TO_VEC_OFFSET;   /* convert irq vector # to hw int # */

	/* set irq to use shadow set */
	GICWRITE(GIC_REG_ADDR(VPE_LOCAL, GIC_VPE_EIC_SS(irq)), set);
}

void gic_send_ipi(unsigned int intr)
{
	pr_debug("CPU%d: %s status %08x\n", smp_processor_id(), __func__,
		 read_c0_status());
	GICWRITE(GIC_REG(SHARED, GIC_SH_WEDGE), 0x80000000 | intr);
}

static void gic_eic_irq_dispatch(void)
{
	unsigned int cause = read_c0_cause();
	int irq;

	irq = (cause & ST0_IM) >> STATUSB_IP2;
	if (irq == 0)
		irq = -1;

	if (irq >= 0)
		do_IRQ(_irqbase + irq);
	else
		spurious_interrupt();
}

/* This is Malta specific and needs to be exported */
static void __init vpe_local_setup(unsigned int numvpes)
{
	int i;
	unsigned long timer_interrupt = GIC_INT_TMR, perf_interrupt = GIC_INT_PERFCTR;
	unsigned int vpe_ctl;

	if (cpu_has_veic) {
		/* GIC timer interrupt -> CPU HW Int X (vector X+2) -> map to pin X+2-1 (since GIC adds 1) */
		timer_interrupt += (GIC_CPU_TO_VEC_OFFSET - GIC_PIN_TO_VEC_OFFSET);
		/* GIC perfcnt interrupt -> CPU HW Int X (vector X+2) -> map to pin X+2-1 (since GIC adds 1) */
		perf_interrupt += (GIC_CPU_TO_VEC_OFFSET - GIC_PIN_TO_VEC_OFFSET);
	}

	/*
	 * Setup the default performance counter timer interrupts
	 * for all VPEs
	 */
	for (i = 0; i < numvpes; i++) {
		GICWRITE(GIC_REG(VPE_LOCAL, GIC_VPE_OTHER_ADDR), i);

		/* Are Interrupts locally routable? */
		GICREAD(GIC_REG(VPE_OTHER, GIC_VPE_CTL), vpe_ctl);
		if (vpe_ctl & GIC_VPE_CTL_TIMER_RTBL_MSK)
			GICWRITE(GIC_REG(VPE_OTHER, GIC_VPE_TIMER_MAP),
				 GIC_MAP_TO_PIN_MSK | timer_interrupt);
		if (cpu_has_veic) {
			set_vi_handler(timer_interrupt+GIC_PIN_TO_VEC_OFFSET, gic_eic_irq_dispatch);
			gic_shared_intr_map[timer_interrupt+GIC_PIN_TO_VEC_OFFSET].local_intr_mask |= GIC_VPE_RMASK_TIMER_MSK;
		}

		if (vpe_ctl & GIC_VPE_CTL_PERFCNT_RTBL_MSK)
			GICWRITE(GIC_REG(VPE_OTHER, GIC_VPE_PERFCTR_MAP),
				 GIC_MAP_TO_PIN_MSK | perf_interrupt);
		if (cpu_has_veic) {
			set_vi_handler(perf_interrupt+GIC_PIN_TO_VEC_OFFSET, gic_eic_irq_dispatch);
			gic_shared_intr_map[perf_interrupt+GIC_PIN_TO_VEC_OFFSET].local_intr_mask |= GIC_VPE_RMASK_PERFCNT_MSK;
		}
	}
}

unsigned int gic_get_int(void)
{
	unsigned int i;
	unsigned long *pending, *intrmask, *pcpu_mask;
	unsigned long *pending_abs, *intrmask_abs;

	/* Get per-cpu bitmaps */
	pending = pending_regs[smp_processor_id()].pending;
	intrmask = intrmask_regs[smp_processor_id()].intrmask;
	pcpu_mask = pcpu_masks[smp_processor_id()].pcpu_mask;

	pending_abs = (unsigned long *) GIC_REG_ABS_ADDR(SHARED,
							 GIC_SH_PEND_31_0_OFS);
	intrmask_abs = (unsigned long *) GIC_REG_ABS_ADDR(SHARED,
							  GIC_SH_MASK_31_0_OFS);

	for (i = 0; i < BITS_TO_LONGS(GIC_NUM_INTRS); i++) {
		GICREAD(*pending_abs, pending[i]);
		GICREAD(*intrmask_abs, intrmask[i]);
		pending_abs++;
		intrmask_abs++;
	}

	bitmap_and(pending, pending, intrmask, GIC_NUM_INTRS);
	bitmap_and(pending, pending, pcpu_mask, GIC_NUM_INTRS);

	i = find_first_bit(pending, GIC_NUM_INTRS);

	pr_debug("CPU%d: %s pend=%d\n", smp_processor_id(), __func__, i);

	return i;
}

static unsigned int gic_irq_startup(unsigned int irq)
{
	irq -= _irqbase;
	pr_debug("CPU%d: %s: irq%d\n", smp_processor_id(), __func__, irq);
	gic_enable_interrupt(irq);
	return 0;
}

static void gic_irq_ack(unsigned int irq)
{
	/* NOTE: this is called to mark the begining of an interrupt. */
	irq -= _irqbase;
	pr_debug("CPU%d: %s: irq%d\n", smp_processor_id(), __func__, irq);

	gic_disable_interrupt(irq);

#ifndef CONFIG_MIPS_SEAD3
	if (gic_irq_flags[irq] & GIC_IRQ_FLAG_EDGE)
		GICWRITE(GIC_REG(SHARED, GIC_SH_WEDGE), irq);
#endif
}

static void gic_mask_irq(unsigned int irq)
{
	irq -= _irqbase;
	pr_debug("CPU%d: %s: irq%d\n", smp_processor_id(), __func__, irq);
	gic_disable_interrupt(irq);
}

static void gic_unmask_irq(unsigned int irq)
{
	irq -= _irqbase;
	pr_debug("CPU%d: %s: irq%d\n", smp_processor_id(), __func__, irq);
	gic_enable_interrupt(irq);
}

static void gic_finish_irq(unsigned int irq)
{
#ifdef CONFIG_MIPS_SEAD3
	unsigned int i;
	unsigned int irq_source;

	irq -= _irqbase;

	/* clear edge detectors */
	for (i = 0; i < gic_shared_intr_map[irq].num_shared_intr; i++) {
		irq_source = gic_shared_intr_map[irq].intr_list[i];
		if (gic_irq_flags[irq_source] & GIC_IRQ_FLAG_EDGE)
			GICWRITE(GIC_REG(SHARED, GIC_SH_WEDGE), irq_source);
	}
#else
	irq -= _irqbase;
#endif

	pr_debug("CPU%d: %s: irq%d\n", smp_processor_id(), __func__, irq);
	/* enable interrupts */
	gic_enable_interrupt(irq);
}

#ifdef CONFIG_SMP

static DEFINE_SPINLOCK(gic_lock);

static int gic_set_affinity(unsigned int irq, const struct cpumask *cpumask)
{
	cpumask_t	tmp = CPU_MASK_NONE;
	unsigned long	flags;
	int		i;

	irq -= _irqbase;
	pr_debug("%s(%d) called\n", __func__, irq);
	cpumask_and(&tmp, cpumask, cpu_online_mask);
	if (cpus_empty(tmp))
		return -1;

	/* Assumption : cpumask refers to a single CPU */
	spin_lock_irqsave(&gic_lock, flags);
	for (;;) {
		/* Re-route this IRQ */
		GIC_SH_MAP_TO_VPE_SMASK(irq, first_cpu(tmp));

		/* Update the pcpu_masks */
		for (i = 0; i < NR_CPUS; i++)
			clear_bit(irq, pcpu_masks[i].pcpu_mask);
		set_bit(irq, pcpu_masks[first_cpu(tmp)].pcpu_mask);

	}
	cpumask_copy(irq_desc[irq].affinity, cpumask);
	spin_unlock_irqrestore(&gic_lock, flags);

	return 0;
}
#endif

static struct irq_chip gic_irq_controller = {
	.name		=	"MIPS GIC",
	.startup	=	gic_irq_startup,
	.ack		=	gic_irq_ack,
	.mask		=	gic_mask_irq,
	.mask_ack	=	gic_mask_irq,
	.unmask		=	gic_unmask_irq,
	.eoi		=	gic_finish_irq,
#ifdef CONFIG_SMP
	.set_affinity	=	gic_set_affinity,
#endif
};

static void __init gic_setup_intr(unsigned int intr, unsigned int cpu,
	unsigned int pin, unsigned int polarity, unsigned int trigtype,
	unsigned int flags)
{
	struct gic_shared_intr_map *map_ptr;

	/* Setup Intr to Pin mapping */
	if (pin & GIC_MAP_TO_NMI_MSK) {
		GICWRITE(GIC_REG_ADDR(SHARED, GIC_SH_MAP_TO_PIN(intr)), pin);
		for (cpu = 0; cpu < NR_CPUS; cpu += 32) {
			GICWRITE(GIC_REG_ADDR(SHARED,
					  GIC_SH_MAP_TO_VPE_REG_OFF(intr, cpu)),
				 0xffffffff);
		}
	} else {
		GICWRITE(GIC_REG_ADDR(SHARED, GIC_SH_MAP_TO_PIN(intr)),
			 GIC_MAP_TO_PIN_MSK | pin);
		/* Setup Intr to CPU mapping */
		GIC_SH_MAP_TO_VPE_SMASK(intr, cpu);
		if (cpu_has_veic) {
			set_vi_handler(pin+GIC_PIN_TO_VEC_OFFSET, gic_eic_irq_dispatch);
			map_ptr = &gic_shared_intr_map[pin+GIC_PIN_TO_VEC_OFFSET];
			if (map_ptr->num_shared_intr >= GIC_MAX_SHARED_INTR)
				BUG();
			map_ptr->intr_list[map_ptr->num_shared_intr++] = intr;
		}
	}

	/* Setup Intr Polarity */
	GIC_SET_POLARITY(intr, polarity);

	/* Setup Intr Trigger Type */
	GIC_SET_TRIGGER(intr, trigtype);

	/* Init Intr Masks */
	GIC_CLR_INTR_MASK(intr);
	/* Initialise per-cpu Interrupt software masks */
	if (flags & GIC_FLAG_IPI)
		set_bit(intr, pcpu_masks[cpu].pcpu_mask);
	if ((flags & GIC_FLAG_TRANSPARENT) && (cpu_has_veic == 0))
		GIC_SET_INTR_MASK(intr);
	if (trigtype == GIC_TRIG_EDGE)
		gic_irq_flags[intr] |= GIC_IRQ_FLAG_EDGE;
}

static void __init gic_basic_init(int numintrs, int numvpes,
			struct gic_intr_map *intrmap, int mapsize)
{
	unsigned int i, cpu;
	unsigned int pin_offset = 0;

	board_bind_eic_interrupt = &gic_bind_eic_interrupt;

	/* Setup defaults */
	for (i = 0; i < numintrs; i++) {
		GIC_SET_POLARITY(i, GIC_POL_POS);
		GIC_SET_TRIGGER(i, GIC_TRIG_LEVEL);
		GIC_CLR_INTR_MASK(i);
		if (i < GIC_NUM_INTRS) {
			gic_irq_flags[i] = 0;
			gic_shared_intr_map[i].num_shared_intr = 0;
			gic_shared_intr_map[i].local_intr_mask = 0;
		}
	}

	/* In EIC mode, the HW_INT# is offset by (2-1). */
	/* Need to subtract one because the GIC will add one (since 0=no intr). */
	if (cpu_has_veic)
		pin_offset = (GIC_CPU_TO_VEC_OFFSET - GIC_PIN_TO_VEC_OFFSET);

	/* Setup specifics */
	for (i = 0; i < mapsize; i++) {
		cpu = intrmap[i].cpunum;
		if (cpu == GIC_UNUSED)
			continue;
		if (cpu == 0 && i != 0 && intrmap[i].flags == 0)
			continue;
		gic_setup_intr(i,
			intrmap[i].cpunum,
			intrmap[i].pin + pin_offset,
			intrmap[i].polarity,
			intrmap[i].trigtype,
			intrmap[i].flags);
	}

	vpe_local_setup(numvpes);

#ifdef CONFIG_MIPS_SEAD3
	/* for non-eic mode, we want to setup the GIC in pass-through mode. */
	/* That is, as if the GIC don't exist. */
	if (cpu_has_veic) {
		for (i = _irqbase; i < (_irqbase + numintrs); i++)
			set_irq_chip_and_handler(i, &gic_irq_controller, handle_percpu_irq);
	}
#else
	for (i = _irqbase; i < (_irqbase + numintrs); i++)
		set_irq_chip(i, &gic_irq_controller);
#endif
}

void __init gic_init(unsigned long gic_base_addr,
		     unsigned long gic_addrspace_size,
		     struct gic_intr_map *intr_map, unsigned int intr_map_size,
		     unsigned int irqbase)
{
	unsigned int gicconfig;
	int numvpes, numintrs;

	_gic_base = (unsigned long) ioremap_nocache(gic_base_addr,
						    gic_addrspace_size);
	_irqbase = irqbase;

	GICREAD(GIC_REG(SHARED, GIC_SH_CONFIG), gicconfig);
	numintrs = (gicconfig & GIC_SH_CONFIG_NUMINTRS_MSK) >>
		   GIC_SH_CONFIG_NUMINTRS_SHF;
	numintrs = ((numintrs + 1) * 8);

	numvpes = (gicconfig & GIC_SH_CONFIG_NUMVPES_MSK) >>
		  GIC_SH_CONFIG_NUMVPES_SHF;
	numvpes = numvpes + 1;

	pr_debug("%s called\n", __func__);

	gic_basic_init(numintrs, numvpes, intr_map, intr_map_size);
}
