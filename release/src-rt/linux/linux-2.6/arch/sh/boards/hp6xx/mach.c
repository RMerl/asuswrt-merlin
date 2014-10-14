/*
 * linux/arch/sh/boards/hp6xx/mach.c
 *
 * Copyright (C) 2000 Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Machine vector for the HP680
 */
#include <asm/machvec.h>
#include <asm/hd64461.h>
#include <asm/io.h>
#include <asm/irq.h>

struct sh_machine_vector mv_hp6xx __initmv = {
	.mv_nr_irqs = HD64461_IRQBASE + HD64461_IRQ_NUM,

	.mv_inb = hd64461_inb,
	.mv_inw = hd64461_inw,
	.mv_inl = hd64461_inl,
	.mv_outb = hd64461_outb,
	.mv_outw = hd64461_outw,
	.mv_outl = hd64461_outl,

	.mv_inb_p = hd64461_inb_p,
	.mv_inw_p = hd64461_inw,
	.mv_inl_p = hd64461_inl,
	.mv_outb_p = hd64461_outb_p,
	.mv_outw_p = hd64461_outw,
	.mv_outl_p = hd64461_outl,

	.mv_insb = hd64461_insb,
	.mv_insw = hd64461_insw,
	.mv_insl = hd64461_insl,
	.mv_outsb = hd64461_outsb,
	.mv_outsw = hd64461_outsw,
	.mv_outsl = hd64461_outsl,

	.mv_readw = hd64461_readw,
	.mv_writew = hd64461_writew,

	.mv_irq_demux = hd64461_irq_demux,
};

ALIAS_MV(hp6xx)
