/*
 * arch/sh/boards/se/73180/irq.c
 *
 * Copyright (C) 2003 Takashi Kusuda <kusuda-takashi@hitachi-ul.co.jp>
 * Based on arch/sh/boards/se/7300/irq.c
 *
 * Modified for SH-Mobile SolutionEngine 73180 Support
 *              by YOSHII Takashi <yoshii-takashi@hitachi-ul.co.jp>
 *
 */

#include <linux/init.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/mach/se73180.h>

static int
irq2intreq(int irq)
{
	if (irq == 10)
		return 5;
	return 7 - (irq - 32);
}

static void
disable_intreq_irq(unsigned int irq)
{
	ctrl_outb(1 << (7 - irq2intreq(irq)), INTMSK0);
}

static void
enable_intreq_irq(unsigned int irq)
{
	ctrl_outb(1 << (7 - irq2intreq(irq)), INTMSKCLR0);
}

static void
mask_and_ack_intreq_irq(unsigned int irq)
{
	disable_intreq_irq(irq);
}

static unsigned int
startup_intreq_irq(unsigned int irq)
{
	enable_intreq_irq(irq);
	return 0;
}

static void
shutdown_intreq_irq(unsigned int irq)
{
	disable_intreq_irq(irq);
}

static void
end_intreq_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		enable_intreq_irq(irq);
}

static struct hw_interrupt_type intreq_irq_type = {
	.typename = "intreq",
	.startup = startup_intreq_irq,
	.shutdown = shutdown_intreq_irq,
	.enable = enable_intreq_irq,
	.disable = disable_intreq_irq,
	.ack = mask_and_ack_intreq_irq,
	.end = end_intreq_irq
};

void
make_intreq_irq(unsigned int irq)
{
	disable_irq_nosync(irq);
	irq_desc[irq].chip = &intreq_irq_type;
	disable_intreq_irq(irq);
}

int
shmse_irq_demux(int irq)
{
	if (irq == IRQ5_IRQ)
		return 10;
	return irq;
}

static struct ipr_data se73180_siof0_ipr_map[] = {
	{ SIOF0_IRQ, SIOF0_IPR_ADDR, SIOF0_IPR_POS, SIOF0_PRIORITY },
};
static struct ipr_data se73180_vpu_ipr_map[] = {
	{ VPU_IRQ, VPU_IPR_ADDR, VPU_IPR_POS, 8 },
};
static struct ipr_data se73180_other_ipr_map[] = {
	{ DMTE2_IRQ, DMA1_IPR_ADDR, DMA1_IPR_POS, DMA1_PRIORITY },
	{ DMTE3_IRQ, DMA1_IPR_ADDR, DMA1_IPR_POS, DMA1_PRIORITY },
	{ DMTE4_IRQ, DMA2_IPR_ADDR, DMA2_IPR_POS, DMA2_PRIORITY },
	{ IIC0_ALI_IRQ, IIC0_IPR_ADDR, IIC0_IPR_POS, IIC0_PRIORITY },
	{ IIC0_TACKI_IRQ, IIC0_IPR_ADDR, IIC0_IPR_POS, IIC0_PRIORITY },
	{ IIC0_WAITI_IRQ, IIC0_IPR_ADDR, IIC0_IPR_POS, IIC0_PRIORITY },
	{ IIC0_DTEI_IRQ, IIC0_IPR_ADDR, IIC0_IPR_POS, IIC0_PRIORITY },
	{ SIOF0_IRQ, SIOF0_IPR_ADDR, SIOF0_IPR_POS, SIOF0_PRIORITY },
	{ SIU_IRQ, SIU_IPR_ADDR, SIU_IPR_POS, SIU_PRIORITY },

	/* VIO interrupt */
	{ CEU_IRQ, VIO_IPR_ADDR, VIO_IPR_POS, VIO_PRIORITY },
	{ BEU_IRQ, VIO_IPR_ADDR, VIO_IPR_POS, VIO_PRIORITY },
	{ VEU_IRQ, VIO_IPR_ADDR, VIO_IPR_POS, VIO_PRIORITY },

	{ LCDC_IRQ, LCDC_IPR_ADDR, LCDC_IPR_POS, LCDC_PRIORITY },
};

/*
 * Initialize IRQ setting
 */
void __init
init_73180se_IRQ(void)
{
	make_ipr_irq(se73180_siof0_ipr_map, ARRAY_SIZE(se73180_siof0_ipr_map));

	ctrl_outw(0x2000, 0xb03fffec);	/* mrshpc irq enable */
	ctrl_outw(0x2000, 0xb07fffec);	/* mrshpc irq enable */
	ctrl_outl(3 << ((7 - 5) * 4), INTC_INTPRI0);	/* irq5 pri=3 */
	ctrl_outw(2 << ((7 - 5) * 2), INTC_ICR1);	/* low-level irq */
	make_intreq_irq(10);

	make_ipr_irq(se73180_vpu_ipr_map, ARRAY_SIZE(se73180_vpu_ipr_map));

	ctrl_outb(0x0f, INTC_IMCR5);	/* enable SCIF IRQ */

	make_ipr_irq(se73180_other_ipr_map, ARRAY_SIZE(se73180_other_ipr_map));

	ctrl_outw(0x2000, PA_MRSHPC + 0x0c);	/* mrshpc irq enable */
}
