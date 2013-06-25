/*
 * Copyright (C) 2007, 2011 MIPS Technologies, Inc.
 *	All rights reserved.

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
 * Arbitrary Monitor interface
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/smp.h>

#include <asm/addrspace.h>
#include <asm/mips-boards/launch.h>
#include <asm/mipsmtregs.h>

int amon_cpu_avail(int cpu)
{
	struct cpulaunch *launch = (struct cpulaunch *)CKSEG0ADDR(CPULAUNCH);

	if (cpu < 0 || cpu >= NCPULAUNCH) {
		pr_debug("avail: cpu%d is out of range\n", cpu);
		return 0;
	}

	launch += cpu;
	if (!(launch->flags & LAUNCH_FREADY)) {
		pr_debug("avail: cpu%d is not ready\n", cpu);
		return 0;
	}
	if (launch->flags & (LAUNCH_FGO|LAUNCH_FGONE)) {
		pr_debug("avail: too late.. cpu%d is already gone\n", cpu);
		return 0;
	}

	return 1;
}

void amon_cpu_start(int cpu,
		    unsigned long pc, unsigned long sp,
		    unsigned long gp, unsigned long a0)
{
	volatile struct cpulaunch *launch =
		(struct cpulaunch  *)CKSEG0ADDR(CPULAUNCH);

	if (!amon_cpu_avail(cpu))
		return;
	if (cpu == smp_processor_id()) {
		pr_debug("launch: I am cpu%d!\n", cpu);
		return;
	}
	launch += cpu;

	pr_debug("launch: starting cpu%d\n", cpu);

	launch->pc = pc;
	launch->gp = gp;
	launch->sp = sp;
	launch->a0 = a0;

	smp_wmb();		/* Target must see parameters before go */
	launch->flags |= LAUNCH_FGO;
	smp_wmb();		/* Target must see go before we poll  */
	while ((launch->flags & LAUNCH_FGONE) == 0)
		;
	smp_rmb();		/* Target will be updating flags soon */
	pr_debug("launch: cpu%d gone!\n", cpu);
}

/*
 * Restart the CPU ready for amon_cpu_start again
 * A bit of a hack... it duplicates the CPU startup code that
 * lives somewhere in the boot monitor.
 */
void amon_cpu_dead(void)
{
	struct cpulaunch *launch;
	unsigned int r0;
	int cpufreq;

	launch = (struct cpulaunch *)CKSEG0ADDR(CPULAUNCH);
	launch += smp_processor_id();

	launch->pc = 0;
	launch->gp = 0;
	launch->sp = 0;
	launch->a0 = 0;
	launch->flags = LAUNCH_FREADY;

	cpufreq = 500000000;

	__asm__ __volatile__ (
	"	.set	push\n\t"
	"	.set	noreorder\n\t"
	"1:\n\t"
	"	lw	%[r0],%[LAUNCH_FLAGS](%[launch])\n\t"
	"	andi	%[r0],%[FGO]\n\t"
	"	bnez    %[r0],1f\n\t"
	"	 nop\n\t"
	"	mfc0	%[r0],$9\n\t"	/* Read Count */
	"	addu    %[r0],%[waitperiod]\n\t"
	"	mtc0	%[r0],$11\n\t"	/* Write Compare */
	"	wait\n\t"
	"	b	1b\n\t"
	"	 nop\n\t"
	"1:	lw	$ra,%[LAUNCH_PC](%[launch])\n\t"
	"	lw	$gp,%[LAUNCH_GP](%[launch])\n\t"
	"	lw	$sp,%[LAUNCH_SP](%[launch])\n\t"
	"	lw	$a0,%[LAUNCH_A0](%[launch])\n\t"
	"	move	$a1,$zero\n\t"
	"	move	$a2,$zero\n\t"
	"	move	$a3,$zero\n\t"
	"	lw	%[r0],%[LAUNCH_FLAGS](%[launch])\n\t"
	"	ori	%[r0],%[FGONE]\n\t"
	"	jr	$ra\n\t"
	"	 sw	%[r0],%[LAUNCH_FLAGS](%[launch])\n\t"
	"	.set	pop\n\t"
	: [r0] "=&r" (r0)
	: [launch] "r" (launch),
	  [FGONE] "i" (LAUNCH_FGONE),
	  [FGO] "i" (LAUNCH_FGO),
	  [LAUNCH_PC] "i" (offsetof(struct cpulaunch, pc)),
	  [LAUNCH_GP] "i" (offsetof(struct cpulaunch, gp)),
	  [LAUNCH_SP] "i" (offsetof(struct cpulaunch, sp)),
	  [LAUNCH_A0] "i" (offsetof(struct cpulaunch, a0)),
	  [LAUNCH_FLAGS] "i" (offsetof(struct cpulaunch, flags)),
	  [waitperiod] "i" ((cpufreq / 2) / 100)	/* delay of ~10ms  */
	: "ra","a0","a1","a2","a3","sp" /* ,"gp" */
	);
}
