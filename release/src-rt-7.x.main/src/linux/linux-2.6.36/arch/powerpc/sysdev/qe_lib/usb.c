/*
 * QE USB routines
 *
 * Copyright (c) Freescale Semicondutor, Inc. 2006.
 *               Shlomi Gridish <gridish@freescale.com>
 *               Jerry Huang <Chang-Ming.Huang@freescale.com>
 * Copyright (c) MontaVista Software, Inc. 2008.
 *               Anton Vorontsov <avorontsov@ru.mvista.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <asm/immap_qe.h>
#include <asm/qe.h>

int qe_usb_clock_set(enum qe_clock clk, int rate)
{
	struct qe_mux __iomem *mux = &qe_immr->qmx;
	unsigned long flags;
	u32 val;

	switch (clk) {
	case QE_CLK3:  val = QE_CMXGCR_USBCS_CLK3;  break;
	case QE_CLK5:  val = QE_CMXGCR_USBCS_CLK5;  break;
	case QE_CLK7:  val = QE_CMXGCR_USBCS_CLK7;  break;
	case QE_CLK9:  val = QE_CMXGCR_USBCS_CLK9;  break;
	case QE_CLK13: val = QE_CMXGCR_USBCS_CLK13; break;
	case QE_CLK17: val = QE_CMXGCR_USBCS_CLK17; break;
	case QE_CLK19: val = QE_CMXGCR_USBCS_CLK19; break;
	case QE_CLK21: val = QE_CMXGCR_USBCS_CLK21; break;
	case QE_BRG9:  val = QE_CMXGCR_USBCS_BRG9;  break;
	case QE_BRG10: val = QE_CMXGCR_USBCS_BRG10; break;
	default:
		pr_err("%s: requested unknown clock %d\n", __func__, clk);
		return -EINVAL;
	}

	if (qe_clock_is_brg(clk))
		qe_setbrg(clk, rate, 1);

	spin_lock_irqsave(&cmxgcr_lock, flags);

	clrsetbits_be32(&mux->cmxgcr, QE_CMXGCR_USBCS, val);

	spin_unlock_irqrestore(&cmxgcr_lock, flags);

	return 0;
}
EXPORT_SYMBOL(qe_usb_clock_set);
