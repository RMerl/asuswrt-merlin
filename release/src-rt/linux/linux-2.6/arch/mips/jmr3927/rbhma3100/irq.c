/*
 * Copyright 2001 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *              ahennessy@mvista.com
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2000-2001 Toshiba Corporation
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>

#include <asm/io.h>
#include <asm/mipsregs.h>
#include <asm/system.h>

#include <asm/processor.h>
#include <asm/jmr3927/jmr3927.h>

#if JMR3927_IRQ_END > NR_IRQS
#error JMR3927_IRQ_END > NR_IRQS
#endif

#define irc_dlevel	0
#define irc_elevel	1

static unsigned char irc_level[TX3927_NUM_IR] = {
	5, 5, 5, 5, 5, 5,	/* INT[5:0] */
	7, 7,			/* SIO */
	5, 5, 5, 0, 0,		/* DMA, PIO, PCI */
	6, 6, 6			/* TMR */
};

/*
 * CP0_STATUS is a thread's resource (saved/restored on context switch).
 * So disable_irq/enable_irq MUST handle IOC/IRC registers.
 */
static void mask_irq_ioc(unsigned int irq)
{
	/* 0: mask */
	unsigned int irq_nr = irq - JMR3927_IRQ_IOC;
	unsigned char imask = jmr3927_ioc_reg_in(JMR3927_IOC_INTM_ADDR);
	unsigned int bit = 1 << irq_nr;
	jmr3927_ioc_reg_out(imask & ~bit, JMR3927_IOC_INTM_ADDR);
	/* flush write buffer */
	(void)jmr3927_ioc_reg_in(JMR3927_IOC_REV_ADDR);
}
static void unmask_irq_ioc(unsigned int irq)
{
	/* 0: mask */
	unsigned int irq_nr = irq - JMR3927_IRQ_IOC;
	unsigned char imask = jmr3927_ioc_reg_in(JMR3927_IOC_INTM_ADDR);
	unsigned int bit = 1 << irq_nr;
	jmr3927_ioc_reg_out(imask | bit, JMR3927_IOC_INTM_ADDR);
	/* flush write buffer */
	(void)jmr3927_ioc_reg_in(JMR3927_IOC_REV_ADDR);
}

static void mask_irq_irc(unsigned int irq)
{
	unsigned int irq_nr = irq - JMR3927_IRQ_IRC;
	volatile unsigned long *ilrp = &tx3927_ircptr->ilr[irq_nr / 2];
	if (irq_nr & 1)
		*ilrp = (*ilrp & 0x00ff) | (irc_dlevel << 8);
	else
		*ilrp = (*ilrp & 0xff00) | irc_dlevel;
	/* update IRCSR */
	tx3927_ircptr->imr = 0;
	tx3927_ircptr->imr = irc_elevel;
	/* flush write buffer */
	(void)tx3927_ircptr->ssr;
}

static void unmask_irq_irc(unsigned int irq)
{
	unsigned int irq_nr = irq - JMR3927_IRQ_IRC;
	volatile unsigned long *ilrp = &tx3927_ircptr->ilr[irq_nr / 2];
	if (irq_nr & 1)
		*ilrp = (*ilrp & 0x00ff) | (irc_level[irq_nr] << 8);
	else
		*ilrp = (*ilrp & 0xff00) | irc_level[irq_nr];
	/* update IRCSR */
	tx3927_ircptr->imr = 0;
	tx3927_ircptr->imr = irc_elevel;
}

asmlinkage void plat_irq_dispatch(void)
{
	unsigned long cp0_cause = read_c0_cause();
	int irq;

	if ((cp0_cause & CAUSEF_IP7) == 0)
		return;
	irq = (cp0_cause >> CAUSEB_IP2) & 0x0f;

	do_IRQ(irq + JMR3927_IRQ_IRC);
}

static irqreturn_t jmr3927_ioc_interrupt(int irq, void *dev_id)
{
	unsigned char istat = jmr3927_ioc_reg_in(JMR3927_IOC_INTS2_ADDR);
	int i;

	for (i = 0; i < JMR3927_NR_IRQ_IOC; i++) {
		if (istat & (1 << i)) {
			irq = JMR3927_IRQ_IOC + i;
			do_IRQ(irq);
		}
	}
	return IRQ_HANDLED;
}

static struct irqaction ioc_action = {
	jmr3927_ioc_interrupt, 0, CPU_MASK_NONE, "IOC", NULL, NULL,
};

static irqreturn_t jmr3927_pcierr_interrupt(int irq, void *dev_id)
{
	printk(KERN_WARNING "PCI error interrupt (irq 0x%x).\n", irq);
	printk(KERN_WARNING "pcistat:%02x, lbstat:%04lx\n",
	       tx3927_pcicptr->pcistat, tx3927_pcicptr->lbstat);

	return IRQ_HANDLED;
}
static struct irqaction pcierr_action = {
	jmr3927_pcierr_interrupt, 0, CPU_MASK_NONE, "PCI error", NULL, NULL,
};

static void __init jmr3927_irq_init(void);

void __init arch_init_irq(void)
{
	/* Now, interrupt control disabled, */
	/* all IRC interrupts are masked, */
	/* all IRC interrupt mode are Low Active. */

	/* mask all IOC interrupts */
	jmr3927_ioc_reg_out(0, JMR3927_IOC_INTM_ADDR);
	/* setup IOC interrupt mode (SOFT:High Active, Others:Low Active) */
	jmr3927_ioc_reg_out(JMR3927_IOC_INTF_SOFT, JMR3927_IOC_INTP_ADDR);

	/* clear PCI Soft interrupts */
	jmr3927_ioc_reg_out(0, JMR3927_IOC_INTS1_ADDR);
	/* clear PCI Reset interrupts */
	jmr3927_ioc_reg_out(0, JMR3927_IOC_RESET_ADDR);

	/* enable interrupt control */
	tx3927_ircptr->cer = TX3927_IRCER_ICE;
	tx3927_ircptr->imr = irc_elevel;

	jmr3927_irq_init();

	/* setup IOC interrupt 1 (PCI, MODEM) */
	setup_irq(JMR3927_IRQ_IOCINT, &ioc_action);

#ifdef CONFIG_PCI
	setup_irq(JMR3927_IRQ_IRC_PCI, &pcierr_action);
#endif

	/* enable all CPU interrupt bits. */
	set_c0_status(ST0_IM);	/* IE bit is still 0. */
}

static struct irq_chip jmr3927_irq_ioc = {
	.name = "jmr3927_ioc",
	.ack = mask_irq_ioc,
	.mask = mask_irq_ioc,
	.mask_ack = mask_irq_ioc,
	.unmask = unmask_irq_ioc,
};

static struct irq_chip jmr3927_irq_irc = {
	.name = "jmr3927_irc",
	.ack = mask_irq_irc,
	.mask = mask_irq_irc,
	.mask_ack = mask_irq_irc,
	.unmask = unmask_irq_irc,
};

static void __init jmr3927_irq_init(void)
{
	u32 i;

	for (i = JMR3927_IRQ_IRC; i < JMR3927_IRQ_IRC + JMR3927_NR_IRQ_IRC; i++)
		set_irq_chip_and_handler(i, &jmr3927_irq_irc, handle_level_irq);
	for (i = JMR3927_IRQ_IOC; i < JMR3927_IRQ_IOC + JMR3927_NR_IRQ_IOC; i++)
		set_irq_chip_and_handler(i, &jmr3927_irq_ioc, handle_level_irq);
}
