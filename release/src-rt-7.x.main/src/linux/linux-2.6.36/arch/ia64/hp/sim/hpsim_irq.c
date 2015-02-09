/*
 * Platform dependent support for HP simulator.
 *
 * Copyright (C) 1998-2001 Hewlett-Packard Co
 * Copyright (C) 1998-2001 David Mosberger-Tang <davidm@hpl.hp.com>
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/irq.h>

static unsigned int
hpsim_irq_startup (unsigned int irq)
{
	return 0;
}

static void
hpsim_irq_noop (unsigned int irq)
{
}

static int
hpsim_set_affinity_noop(unsigned int a, const struct cpumask *b)
{
	return 0;
}

static struct irq_chip irq_type_hp_sim = {
	.name =		"hpsim",
	.startup =	hpsim_irq_startup,
	.shutdown =	hpsim_irq_noop,
	.enable =	hpsim_irq_noop,
	.disable =	hpsim_irq_noop,
	.ack =		hpsim_irq_noop,
	.end =		hpsim_irq_noop,
	.set_affinity =	hpsim_set_affinity_noop,
};

void __init
hpsim_irq_init (void)
{
	struct irq_desc *idesc;
	int i;

	for (i = 0; i < NR_IRQS; ++i) {
		idesc = irq_desc + i;
		if (idesc->chip == &no_irq_chip)
			idesc->chip = &irq_type_hp_sim;
	}
}
