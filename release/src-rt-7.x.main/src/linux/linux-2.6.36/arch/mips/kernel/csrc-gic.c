/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2011 MIPS Technologies, Inc.
 */
#include <linux/clocksource.h>
#include <linux/init.h>

#include <asm/time.h>
#include <asm/gic.h>

static cycle_t gic_hpt_read(struct clocksource *cs)
{
	unsigned int hi, hi2, lo;

	do {
		GICREAD(GIC_REG(SHARED, GIC_SH_COUNTER_63_32), hi);
		GICREAD(GIC_REG(SHARED, GIC_SH_COUNTER_31_00), lo);
		GICREAD(GIC_REG(SHARED, GIC_SH_COUNTER_63_32), hi2);
	} while (hi2 != hi);

	return (((cycle_t) hi) << 32) + lo;
}

static struct clocksource gic_clocksource = {
	.name	= "GIC",
	.read	= gic_hpt_read,
	.flags	= CLOCK_SOURCE_IS_CONTINUOUS,
};

void __init gic_clocksource_init(void)
{
	unsigned int config, bits;

	if (!cpu_has_counter || !mips_hpt_frequency)
		BUG();

	/* Calculate the clocksource mask. */
	GICREAD(GIC_REG(SHARED, GIC_SH_CONFIG), config);
	bits = 32 + ((config & GIC_SH_CONFIG_COUNTBITS_MSK) >>
		(GIC_SH_CONFIG_COUNTBITS_SHF - 2));

	/* Set clocksource mask. */
	gic_clocksource.mask = CLOCKSOURCE_MASK(bits);

	/* Calculate a somewhat reasonable rating value. */
	gic_clocksource.rating = 200 + (mips_hpt_frequency * 2) / 10000000;

	clocksource_set_clock(&gic_clocksource, mips_hpt_frequency);
	clocksource_register(&gic_clocksource);
}
