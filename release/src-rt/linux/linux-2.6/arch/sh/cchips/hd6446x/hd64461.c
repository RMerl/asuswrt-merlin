/*
 *	Copyright (C) 2000 YAEGASHI Takeshi
 *	Hitachi HD64461 companion chip support
 */

#include <linux/sched.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <asm/irq.h>
#include <asm/hd64461.h>

/* This belongs in cpu specific */
#define INTC_ICR1 0xA4140010UL

static void hd64461_mask_irq(struct irq_data *data)
{
	unsigned int irq = data->irq;
	unsigned short nimr;
	unsigned short mask = 1 << (irq - HD64461_IRQBASE);

	nimr = __raw_readw(HD64461_NIMR);
	nimr |= mask;
	__raw_writew(nimr, HD64461_NIMR);
}

static void hd64461_unmask_irq(struct irq_data *data)
{
	unsigned int irq = data->irq;
	unsigned short nimr;
	unsigned short mask = 1 << (irq - HD64461_IRQBASE);

	nimr = __raw_readw(HD64461_NIMR);
	nimr &= ~mask;
	__raw_writew(nimr, HD64461_NIMR);
}

static void hd64461_mask_and_ack_irq(struct irq_data *data)
{
	hd64461_mask_irq(data);

#ifdef CONFIG_HD64461_ENABLER
	if (data->irq == HD64461_IRQBASE + 13)
		__raw_writeb(0x00, HD64461_PCC1CSCR);
#endif
}

static struct irq_chip hd64461_irq_chip = {
	.name		= "HD64461-IRQ",
	.irq_mask	= hd64461_mask_irq,
	.irq_mask_ack	= hd64461_mask_and_ack_irq,
	.irq_unmask	= hd64461_unmask_irq,
};

static void hd64461_irq_demux(unsigned int irq, struct irq_desc *desc)
{
	unsigned short intv = __raw_readw(HD64461_NIRR);
	unsigned int ext_irq = HD64461_IRQBASE;

	intv &= (1 << HD64461_IRQ_NUM) - 1;

	for (; intv; intv >>= 1, ext_irq++) {
		if (!(intv & 1))
			continue;

		generic_handle_irq(ext_irq);
	}
}

int __init setup_hd64461(void)
{
	int i, nid = cpu_to_node(boot_cpu_data);

	if (!MACH_HD64461)
		return 0;

	printk(KERN_INFO
	       "HD64461 configured at 0x%x on irq %d(mapped into %d to %d)\n",
	       HD64461_IOBASE, CONFIG_HD64461_IRQ, HD64461_IRQBASE,
	       HD64461_IRQBASE + 15);

/* Should be at processor specific part.. */
#if defined(CONFIG_CPU_SUBTYPE_SH7709)
	__raw_writew(0x2240, INTC_ICR1);
#endif
	__raw_writew(0xffff, HD64461_NIMR);

	/*  IRQ 80 -> 95 belongs to HD64461  */
	for (i = HD64461_IRQBASE; i < HD64461_IRQBASE + 16; i++) {
		unsigned int irq;

		irq = create_irq_nr(i, nid);
		if (unlikely(irq == 0)) {
			pr_err("%s: failed hooking irq %d for HD64461\n",
			       __func__, i);
			return -EBUSY;
		}

		if (unlikely(irq != i)) {
			pr_err("%s: got irq %d but wanted %d, bailing.\n",
			       __func__, irq, i);
			destroy_irq(irq);
			return -EINVAL;
		}

		irq_set_chip_and_handler(i, &hd64461_irq_chip,
					 handle_level_irq);
	}

	irq_set_chained_handler(CONFIG_HD64461_IRQ, hd64461_irq_demux);
	irq_set_irq_type(CONFIG_HD64461_IRQ, IRQ_TYPE_LEVEL_LOW);

#ifdef CONFIG_HD64461_ENABLER
	printk(KERN_INFO "HD64461: enabling PCMCIA devices\n");
	__raw_writeb(0x4c, HD64461_PCC1CSCIER);
	__raw_writeb(0x00, HD64461_PCC1CSCR);
#endif

	return 0;
}

module_init(setup_hd64461);
