/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * arch/sh64/kernel/irq_cayman.c
 *
 * SH-5 Cayman Interrupt Support
 *
 * This file handles the board specific parts of the Cayman interrupt system
 *
 * Copyright (C) 2002 Stuart Menefy
 */

#include <asm/irq.h>
#include <asm/page.h>
#include <asm/io.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/signal.h>
#include <asm/cayman.h>

unsigned long epld_virt;

#define EPLD_BASE        0x04002000
#define EPLD_STATUS_BASE (epld_virt + 0x10)
#define EPLD_MASK_BASE   (epld_virt + 0x20)

/* Note the SMSC SuperIO chip and SMSC LAN chip interrupts are all muxed onto
   the same SH-5 interrupt */

static irqreturn_t cayman_interrupt_smsc(int irq, void *dev_id)
{
        printk(KERN_INFO "CAYMAN: spurious SMSC interrupt\n");
	return IRQ_NONE;
}

static irqreturn_t cayman_interrupt_pci2(int irq, void *dev_id)
{
        printk(KERN_INFO "CAYMAN: spurious PCI interrupt, IRQ %d\n", irq);
	return IRQ_NONE;
}

static struct irqaction cayman_action_smsc = {
	.name		= "Cayman SMSC Mux",
	.handler	= cayman_interrupt_smsc,
	.flags		= IRQF_DISABLED,
};

static struct irqaction cayman_action_pci2 = {
	.name		= "Cayman PCI2 Mux",
	.handler	= cayman_interrupt_pci2,
	.flags		= IRQF_DISABLED,
};

static void enable_cayman_irq(unsigned int irq)
{
	unsigned long flags;
	unsigned long mask;
	unsigned int reg;
	unsigned char bit;

	irq -= START_EXT_IRQS;
	reg = EPLD_MASK_BASE + ((irq / 8) << 2);
	bit = 1<<(irq % 8);
	local_irq_save(flags);
	mask = ctrl_inl(reg);
	mask |= bit;
	ctrl_outl(mask, reg);
	local_irq_restore(flags);
}

void disable_cayman_irq(unsigned int irq)
{
	unsigned long flags;
	unsigned long mask;
	unsigned int reg;
	unsigned char bit;

	irq -= START_EXT_IRQS;
	reg = EPLD_MASK_BASE + ((irq / 8) << 2);
	bit = 1<<(irq % 8);
	local_irq_save(flags);
	mask = ctrl_inl(reg);
	mask &= ~bit;
	ctrl_outl(mask, reg);
	local_irq_restore(flags);
}

static void ack_cayman_irq(unsigned int irq)
{
	disable_cayman_irq(irq);
}

static void end_cayman_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED|IRQ_INPROGRESS)))
		enable_cayman_irq(irq);
}

static unsigned int startup_cayman_irq(unsigned int irq)
{
	enable_cayman_irq(irq);
	return 0; /* never anything pending */
}

static void shutdown_cayman_irq(unsigned int irq)
{
	disable_cayman_irq(irq);
}

struct hw_interrupt_type cayman_irq_type = {
	.typename	= "Cayman-IRQ",
	.startup	= startup_cayman_irq,
	.shutdown	= shutdown_cayman_irq,
	.enable		= enable_cayman_irq,
	.disable	= disable_cayman_irq,
	.ack		= ack_cayman_irq,
	.end		= end_cayman_irq,
};

int cayman_irq_demux(int evt)
{
	int irq = intc_evt_to_irq[evt];

	if (irq == SMSC_IRQ) {
		unsigned long status;
		int i;

		status = ctrl_inl(EPLD_STATUS_BASE) &
			 ctrl_inl(EPLD_MASK_BASE) & 0xff;
		if (status == 0) {
			irq = -1;
		} else {
			for (i=0; i<8; i++) {
				if (status & (1<<i))
					break;
			}
			irq = START_EXT_IRQS + i;
		}
	}

	if (irq == PCI2_IRQ) {
		unsigned long status;
		int i;

		status = ctrl_inl(EPLD_STATUS_BASE + 3 * sizeof(u32)) &
			 ctrl_inl(EPLD_MASK_BASE + 3 * sizeof(u32)) & 0xff;
		if (status == 0) {
			irq = -1;
		} else {
			for (i=0; i<8; i++) {
				if (status & (1<<i))
					break;
			}
			irq = START_EXT_IRQS + (3 * 8) + i;
		}
	}

	return irq;
}

#if defined(CONFIG_PROC_FS) && defined(CONFIG_SYSCTL)
int cayman_irq_describe(char* p, int irq)
{
	if (irq < NR_INTC_IRQS) {
		return intc_irq_describe(p, irq);
	} else if (irq < NR_INTC_IRQS + 8) {
		return sprintf(p, "(SMSC %d)", irq - NR_INTC_IRQS);
	} else if ((irq >= NR_INTC_IRQS + 24) && (irq < NR_INTC_IRQS + 32)) {
		return sprintf(p, "(PCI2 %d)", irq - (NR_INTC_IRQS + 24));
	}

	return 0;
}
#endif

void init_cayman_irq(void)
{
	int i;

	epld_virt = onchip_remap(EPLD_BASE, 1024, "EPLD");
	if (!epld_virt) {
		printk(KERN_ERR "Cayman IRQ: Unable to remap EPLD\n");
		return;
	}

	for (i=0; i<NR_EXT_IRQS; i++) {
		irq_desc[START_EXT_IRQS + i].chip = &cayman_irq_type;
	}

	/* Setup the SMSC interrupt */
	setup_irq(SMSC_IRQ, &cayman_action_smsc);
	setup_irq(PCI2_IRQ, &cayman_action_pci2);
}
