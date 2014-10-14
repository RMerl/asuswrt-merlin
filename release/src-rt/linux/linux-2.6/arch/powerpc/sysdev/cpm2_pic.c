/*
 * Platform information definitions.
 *
 * Copied from arch/ppc/syslib/cpm2_pic.c with minor subsequent updates
 * to make in work in arch/powerpc/. Original (c) belongs to Dan Malek.
 *
 * Author:  Vitaly Bordug <vbordug@ru.mvista.com>
 *
 * 1999-2001 (c) Dan Malek <dan@embeddedalley.com>
 * 2006 (c) MontaVista Software, Inc.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

/* The CPM2 internal interrupt controller.  It is usually
 * the only interrupt controller.
 * There are two 32-bit registers (high/low) for up to 64
 * possible interrupts.
 *
 * Now, the fun starts.....Interrupt Numbers DO NOT MAP
 * in a simple arithmetic fashion to mask or pending registers.
 * That is, interrupt 4 does not map to bit position 4.
 * We create two tables, indexed by vector number, to indicate
 * which register to use and which bit in the register to use.
 */

#include <linux/stddef.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/irq.h>

#include <asm/immap_cpm2.h>
#include <asm/mpc8260.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/fs_pd.h>

#include "cpm2_pic.h"

/* External IRQS */
#define CPM2_IRQ_EXT1		19
#define CPM2_IRQ_EXT7		25

/* Port C IRQS */
#define CPM2_IRQ_PORTC15	48
#define CPM2_IRQ_PORTC0		63

static intctl_cpm2_t __iomem *cpm2_intctl;

static struct irq_host *cpm2_pic_host;
#define NR_MASK_WORDS   ((NR_IRQS + 31) / 32)
static unsigned long ppc_cached_irq_mask[NR_MASK_WORDS];

static const u_char irq_to_siureg[] = {
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

/* bit numbers do not match the docs, these are precomputed so the bit for
 * a given irq is (1 << irq_to_siubit[irq]) */
static const u_char irq_to_siubit[] = {
	 0, 15, 14, 13, 12, 11, 10,  9,
	 8,  7,  6,  5,  4,  3,  2,  1,
	 2,  1,  0, 14, 13, 12, 11, 10,
	 9,  8,  7,  6,  5,  4,  3,  0,
	31, 30, 29, 28, 27, 26, 25, 24,
	23, 22, 21, 20, 19, 18, 17, 16,
	16, 17, 18, 19, 20, 21, 22, 23,
	24, 25, 26, 27, 28, 29, 30, 31,
};

static void cpm2_mask_irq(struct irq_data *d)
{
	int	bit, word;
	unsigned int irq_nr = virq_to_hw(d->irq);

	bit = irq_to_siubit[irq_nr];
	word = irq_to_siureg[irq_nr];

	ppc_cached_irq_mask[word] &= ~(1 << bit);
	out_be32(&cpm2_intctl->ic_simrh + word, ppc_cached_irq_mask[word]);
}

static void cpm2_unmask_irq(struct irq_data *d)
{
	int	bit, word;
	unsigned int irq_nr = virq_to_hw(d->irq);

	bit = irq_to_siubit[irq_nr];
	word = irq_to_siureg[irq_nr];

	ppc_cached_irq_mask[word] |= 1 << bit;
	out_be32(&cpm2_intctl->ic_simrh + word, ppc_cached_irq_mask[word]);
}

static void cpm2_ack(struct irq_data *d)
{
	int	bit, word;
	unsigned int irq_nr = virq_to_hw(d->irq);

	bit = irq_to_siubit[irq_nr];
	word = irq_to_siureg[irq_nr];

	out_be32(&cpm2_intctl->ic_sipnrh + word, 1 << bit);
}

static void cpm2_end_irq(struct irq_data *d)
{
	int	bit, word;
	unsigned int irq_nr = virq_to_hw(d->irq);

	bit = irq_to_siubit[irq_nr];
	word = irq_to_siureg[irq_nr];

	ppc_cached_irq_mask[word] |= 1 << bit;
	out_be32(&cpm2_intctl->ic_simrh + word, ppc_cached_irq_mask[word]);

	/*
	 * Work around large numbers of spurious IRQs on PowerPC 82xx
	 * systems.
	 */
	mb();
}

static int cpm2_set_irq_type(struct irq_data *d, unsigned int flow_type)
{
	unsigned int src = virq_to_hw(d->irq);
	unsigned int vold, vnew, edibit;

	/* Port C interrupts are either IRQ_TYPE_EDGE_FALLING or
	 * IRQ_TYPE_EDGE_BOTH (default).  All others are IRQ_TYPE_EDGE_FALLING
	 * or IRQ_TYPE_LEVEL_LOW (default)
	 */
	if (src >= CPM2_IRQ_PORTC15 && src <= CPM2_IRQ_PORTC0) {
		if (flow_type == IRQ_TYPE_NONE)
			flow_type = IRQ_TYPE_EDGE_BOTH;

		if (flow_type != IRQ_TYPE_EDGE_BOTH &&
		    flow_type != IRQ_TYPE_EDGE_FALLING)
			goto err_sense;
	} else {
		if (flow_type == IRQ_TYPE_NONE)
			flow_type = IRQ_TYPE_LEVEL_LOW;

		if (flow_type & (IRQ_TYPE_EDGE_RISING | IRQ_TYPE_LEVEL_HIGH))
			goto err_sense;
	}

	irqd_set_trigger_type(d, flow_type);
	if (flow_type & IRQ_TYPE_LEVEL_LOW)
		__irq_set_handler_locked(d->irq, handle_level_irq);
	else
		__irq_set_handler_locked(d->irq, handle_edge_irq);

	/* internal IRQ senses are LEVEL_LOW
	 * EXT IRQ and Port C IRQ senses are programmable
	 */
	if (src >= CPM2_IRQ_EXT1 && src <= CPM2_IRQ_EXT7)
			edibit = (14 - (src - CPM2_IRQ_EXT1));
	else
		if (src >= CPM2_IRQ_PORTC15 && src <= CPM2_IRQ_PORTC0)
			edibit = (31 - (CPM2_IRQ_PORTC0 - src));
		else
			return (flow_type & IRQ_TYPE_LEVEL_LOW) ?
				IRQ_SET_MASK_OK_NOCOPY : -EINVAL;

	vold = in_be32(&cpm2_intctl->ic_siexr);

	if ((flow_type & IRQ_TYPE_SENSE_MASK) == IRQ_TYPE_EDGE_FALLING)
		vnew = vold | (1 << edibit);
	else
		vnew = vold & ~(1 << edibit);

	if (vold != vnew)
		out_be32(&cpm2_intctl->ic_siexr, vnew);
	return IRQ_SET_MASK_OK_NOCOPY;

err_sense:
	pr_err("CPM2 PIC: sense type 0x%x not supported\n", flow_type);
	return -EINVAL;
}

static struct irq_chip cpm2_pic = {
	.name = "CPM2 SIU",
	.irq_mask = cpm2_mask_irq,
	.irq_unmask = cpm2_unmask_irq,
	.irq_ack = cpm2_ack,
	.irq_eoi = cpm2_end_irq,
	.irq_set_type = cpm2_set_irq_type,
	.flags = IRQCHIP_EOI_IF_HANDLED,
};

unsigned int cpm2_get_irq(void)
{
	int irq;
	unsigned long bits;

       /* For CPM2, read the SIVEC register and shift the bits down
         * to get the irq number.         */
        bits = in_be32(&cpm2_intctl->ic_sivec);
        irq = bits >> 26;

	if (irq == 0)
		return(-1);
	return irq_linear_revmap(cpm2_pic_host, irq);
}

static int cpm2_pic_host_map(struct irq_host *h, unsigned int virq,
			  irq_hw_number_t hw)
{
	pr_debug("cpm2_pic_host_map(%d, 0x%lx)\n", virq, hw);

	irq_set_status_flags(virq, IRQ_LEVEL);
	irq_set_chip_and_handler(virq, &cpm2_pic, handle_level_irq);
	return 0;
}

static int cpm2_pic_host_xlate(struct irq_host *h, struct device_node *ct,
			    const u32 *intspec, unsigned int intsize,
			    irq_hw_number_t *out_hwirq, unsigned int *out_flags)
{
	*out_hwirq = intspec[0];
	if (intsize > 1)
		*out_flags = intspec[1];
	else
		*out_flags = IRQ_TYPE_NONE;
	return 0;
}

static struct irq_host_ops cpm2_pic_host_ops = {
	.map = cpm2_pic_host_map,
	.xlate = cpm2_pic_host_xlate,
};

void cpm2_pic_init(struct device_node *node)
{
	int i;

	cpm2_intctl = cpm2_map(im_intctl);

	/* Clear the CPM IRQ controller, in case it has any bits set
	 * from the bootloader
	 */

	/* Mask out everything */

	out_be32(&cpm2_intctl->ic_simrh, 0x00000000);
	out_be32(&cpm2_intctl->ic_simrl, 0x00000000);

	wmb();

	/* Ack everything */
	out_be32(&cpm2_intctl->ic_sipnrh, 0xffffffff);
	out_be32(&cpm2_intctl->ic_sipnrl, 0xffffffff);
	wmb();

	/* Dummy read of the vector */
	i = in_be32(&cpm2_intctl->ic_sivec);
	rmb();

	/* Initialize the default interrupt mapping priorities,
	 * in case the boot rom changed something on us.
	 */
	out_be16(&cpm2_intctl->ic_sicr, 0);
	out_be32(&cpm2_intctl->ic_scprrh, 0x05309770);
	out_be32(&cpm2_intctl->ic_scprrl, 0x05309770);

	/* create a legacy host */
	cpm2_pic_host = irq_alloc_host(node, IRQ_HOST_MAP_LINEAR,
				       64, &cpm2_pic_host_ops, 64);
	if (cpm2_pic_host == NULL) {
		printk(KERN_ERR "CPM2 PIC: failed to allocate irq host!\n");
		return;
	}
}
