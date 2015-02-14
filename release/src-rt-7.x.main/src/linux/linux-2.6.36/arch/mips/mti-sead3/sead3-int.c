/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2000, 2001, 2004 MIPS Technologies, Inc.
 * Copyright (C) 2001 Ralf Baechle
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Routines for generic manipulation of the interrupts found on the MIPS
 * Malta board.
 * The interrupt controller is located in the South Bridge a PIIX4 device
 * with two internal 82C95 interrupt controllers.
 */
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel_stat.h>
#include <linux/kernel.h>
#include <linux/random.h>

#include <asm/traps.h>
#include <asm/irq_cpu.h>
#include <asm/irq_regs.h>
#include <asm/mips-boards/generic.h>
#include <asm/mips-boards/sead3int.h>
#include <asm/gic.h>

#define SEAD_CONFIG_GIC_PRESENT_SHF   (1)
#define SEAD_CONFIG_GIC_PRESENT_MSK   (1 << SEAD_CONFIG_GIC_PRESENT_SHF)
#define SEAD_CONFIG_BASE              (0x1B100110)
#define SEAD_CONFIG_SIZE              (4)

int gic_present;    /* global var => auto initialized to 0 */
static unsigned long sead3_config_reg;

/*
 * This table defines the setup for each external GIC interrupt
 * It is indexed by interrupt number
 */
#define GIC_CPU_NMI GIC_MAP_TO_NMI_MSK
static struct gic_intr_map gic_intr_map[GIC_NUM_INTRS] = {
	{ 0, GIC_CPU_INT4, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },
	{ 0, GIC_CPU_INT3, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },
	{ 0, GIC_CPU_INT2, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },
	{ 0, GIC_CPU_INT2, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },
	{ 0, GIC_CPU_INT1, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },
	{ 0, GIC_CPU_INT0, GIC_POL_POS, GIC_TRIG_LEVEL, GIC_FLAG_TRANSPARENT },
	{ X, X,            X,           X,              0 },
	{ X, X,            X,           X,              0 },
	{ X, X,            X,           X,              0 },
	{ X, X,            X,           X,              0 },
	{ X, X,            X,           X,              0 },
	{ X, X,            X,           X,              0 },
	{ X, X,            X,           X,              0 },
	/* The remainder of this table is initialised by fill_ipi_map */
};

/*
 * Version of ffs that only looks at bits 8..15
 */
static inline unsigned int irq_ffs(unsigned int pending)
{
#if defined(CONFIG_CPU_MIPS32) || defined(CONFIG_CPU_MIPS64)
	return fls(pending) - CAUSEB_IP - 1;
#else
	unsigned int a0 = 7;
	unsigned int t0;

	t0 = pending & 0xf000;
	t0 = t0 < 1;
	t0 = t0 << 2;
	a0 = a0 - t0;
	pending = pending << t0;

	t0 = pending & 0xc000;
	t0 = t0 < 1;
	t0 = t0 << 1;
	a0 = a0 - t0;
	pending = pending << t0;

	t0 = pending & 0x8000;
	t0 = t0 < 1;
	/* t0 = t0 << 2; */
	a0 = a0 - t0;
	/* pending = pending << t0; */

	return a0;
#endif
}

asmlinkage void plat_irq_dispatch(void)
{
	unsigned int pending = read_c0_cause() & read_c0_status() & ST0_IM;
	int irq;

	irq = irq_ffs(pending);

	if (irq >= 0)
		do_IRQ(MIPS_CPU_IRQ_BASE + irq);
	else
		spurious_interrupt();
}

void __init arch_init_irq(void)
{
	int i;

	if (!cpu_has_veic) {
		mips_cpu_irq_init();

		if (cpu_has_vint) {
			/* install generic handler */
			for (i = 0; i < 8; i++)
				set_vi_handler(i, plat_irq_dispatch);
		}
	}

	sead3_config_reg = (unsigned long)ioremap_nocache(SEAD_CONFIG_BASE, SEAD_CONFIG_SIZE);
	gic_present = (REG32(sead3_config_reg) & SEAD_CONFIG_GIC_PRESENT_MSK) >>
			SEAD_CONFIG_GIC_PRESENT_SHF;
	printk("GIC: %spresent\n", (gic_present) ? "" : "not ");
	printk("EIC: %s\n", (current_cpu_data.options & MIPS_CPU_VEIC) ? "on" : "off");

	if (gic_present) {
		gic_init(GIC_BASE_ADDR, GIC_ADDRSPACE_SZ, gic_intr_map,
				ARRAY_SIZE(gic_intr_map), MIPS_GIC_IRQ_BASE);
	}
}
