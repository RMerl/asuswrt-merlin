/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2004, 05, 06 by Ralf Baechle
 * Copyright (C) 2005 by MIPS Technologies, Inc.
 */
#include <linux/cpumask.h>
#include <linux/oprofile.h>
#include <linux/interrupt.h>
#include <linux/smp.h>
#include <asm/irq_regs.h>

#include "op_impl.h"

#define M_PERFCTL_EXL			(1UL      <<  0)
#define M_PERFCTL_KERNEL		(1UL      <<  1)
#define M_PERFCTL_SUPERVISOR		(1UL      <<  2)
#define M_PERFCTL_USER			(1UL      <<  3)
#define M_PERFCTL_INTERRUPT_ENABLE	(1UL      <<  4)
#define M_PERFCTL_EVENT(event)		(((event) & 0x3ff)  << 5)
#define M_PERFCTL_VPEID(vpe)		((vpe)    << 16)
#define M_PERFCTL_MT_EN(filter)		((filter) << 20)
#define    M_TC_EN_ALL			M_PERFCTL_MT_EN(0)
#define    M_TC_EN_VPE			M_PERFCTL_MT_EN(1)
#define    M_TC_EN_TC			M_PERFCTL_MT_EN(2)
#define M_PERFCTL_TCID(tcid)		((tcid)   << 22)
#define M_PERFCTL_WIDE			(1UL      << 30)
#define M_PERFCTL_MORE			(1UL      << 31)

#define M_COUNTER_OVERFLOW		(1UL      << 31)

static int (*save_perf_irq)(void);

#ifdef CONFIG_MIPS_MT_SMP
static int cpu_has_mipsmt_pertccounters;
#define WHAT		(M_TC_EN_VPE | \
			 M_PERFCTL_VPEID(cpu_data[smp_processor_id()].vpe_id))
#define vpe_id()	(cpu_has_mipsmt_pertccounters ? \
			0 : cpu_data[smp_processor_id()].vpe_id)

/*
 * The number of bits to shift to convert between counters per core and
 * counters per VPE.  There is no reasonable interface atm to obtain the
 * number of VPEs used by Linux and in the 34K this number is fixed to two
 * anyways so we hardcore a few things here for the moment.  The way it's
 * done here will ensure that oprofile VSMP kernel will run right on a lesser
 * core like a 24K also or with maxcpus=1.
 */
static inline unsigned int vpe_shift(void)
{
	if (num_possible_cpus() > 1)
		return 1;

	return 0;
}

#else

#define WHAT		0
#define vpe_id()	0

static inline unsigned int vpe_shift(void)
{
	return 0;
}

#endif

static inline unsigned int counters_total_to_per_cpu(unsigned int counters)
{
	return counters >> vpe_shift();
}

static inline unsigned int counters_per_cpu_to_total(unsigned int counters)
{
	return counters << vpe_shift();
}

#define __define_perf_accessors(r, n, np)				\
									\
static inline unsigned int r_c0_ ## r ## n(void)			\
{									\
	unsigned int cpu = vpe_id();					\
									\
	switch (cpu) {							\
	case 0:								\
		return read_c0_ ## r ## n();				\
	case 1:								\
		return read_c0_ ## r ## np();				\
	default:							\
		BUG();							\
	}								\
	return 0;							\
}									\
									\
static inline void w_c0_ ## r ## n(unsigned int value)			\
{									\
	unsigned int cpu = vpe_id();					\
									\
	switch (cpu) {							\
	case 0:								\
		write_c0_ ## r ## n(value);				\
		return;							\
	case 1:								\
		write_c0_ ## r ## np(value);				\
		return;							\
	default:							\
		BUG();							\
	}								\
	return;								\
}									\

__define_perf_accessors(perfcntr, 0, 2)
__define_perf_accessors(perfcntr, 1, 3)
__define_perf_accessors(perfcntr, 2, 0)
__define_perf_accessors(perfcntr, 3, 1)

__define_perf_accessors(perfctrl, 0, 2)
__define_perf_accessors(perfctrl, 1, 3)
__define_perf_accessors(perfctrl, 2, 0)
__define_perf_accessors(perfctrl, 3, 1)

struct op_mips_model op_model_mipsxx_ops;

static struct mipsxx_register_config {
	unsigned int control[4];
	unsigned int counter[4];
} reg;

/* Compute all of the registers in preparation for enabling profiling.  */

static void mipsxx_reg_setup(struct op_counter_config *ctr)
{
	unsigned int counters = op_model_mipsxx_ops.num_counters;
	int i;

	/* Compute the performance counter control word.  */
	for (i = 0; i < counters; i++) {
		reg.control[i] = 0;
		reg.counter[i] = 0;

		if (!ctr[i].enabled)
			continue;

		reg.control[i] = M_PERFCTL_EVENT(ctr[i].event) |
		                 M_PERFCTL_INTERRUPT_ENABLE;
		if (ctr[i].kernel)
			reg.control[i] |= M_PERFCTL_KERNEL;
		if (ctr[i].user)
			reg.control[i] |= M_PERFCTL_USER;
		if (ctr[i].exl)
			reg.control[i] |= M_PERFCTL_EXL;
		reg.counter[i] = 0x80000000 - ctr[i].count;
	}
}

/* Program all of the registers in preparation for enabling profiling.  */

static void mipsxx_cpu_setup(void *args)
{
	unsigned int counters = op_model_mipsxx_ops.num_counters;

	switch (counters) {
	case 4:
		w_c0_perfctrl3(0);
		w_c0_perfcntr3(reg.counter[3]);
	case 3:
		w_c0_perfctrl2(0);
		w_c0_perfcntr2(reg.counter[2]);
	case 2:
		w_c0_perfctrl1(0);
		w_c0_perfcntr1(reg.counter[1]);
	case 1:
		w_c0_perfctrl0(0);
		w_c0_perfcntr0(reg.counter[0]);
	}
}

/* Start all counters on current CPU */
static void mipsxx_cpu_start(void *args)
{
	unsigned int counters = op_model_mipsxx_ops.num_counters;

	switch (counters) {
	case 4:
		w_c0_perfctrl3(WHAT | reg.control[3]);
	case 3:
		w_c0_perfctrl2(WHAT | reg.control[2]);
	case 2:
		w_c0_perfctrl1(WHAT | reg.control[1]);
	case 1:
		w_c0_perfctrl0(WHAT | reg.control[0]);
	}
}

/* Stop all counters on current CPU */
static void mipsxx_cpu_stop(void *args)
{
	unsigned int counters = op_model_mipsxx_ops.num_counters;

	switch (counters) {
	case 4:
		w_c0_perfctrl3(0);
	case 3:
		w_c0_perfctrl2(0);
	case 2:
		w_c0_perfctrl1(0);
	case 1:
		w_c0_perfctrl0(0);
	}
}

static int mipsxx_perfcount_handler(void)
{
	unsigned int counters = op_model_mipsxx_ops.num_counters;
	unsigned int control;
	unsigned int counter;
	int handled = IRQ_NONE;

	if (cpu_has_mips_r2 && !(read_c0_cause() & (1 << 26)))
		return handled;

	switch (counters) {
#define HANDLE_COUNTER(n)						\
	case n + 1:							\
		control = r_c0_perfctrl ## n();				\
		counter = r_c0_perfcntr ## n();				\
		if ((control & M_PERFCTL_INTERRUPT_ENABLE) &&		\
		    (counter & M_COUNTER_OVERFLOW)) {			\
			oprofile_add_sample(get_irq_regs(), n);		\
			w_c0_perfcntr ## n(reg.counter[n]);		\
			handled = IRQ_HANDLED;				\
		}
	HANDLE_COUNTER(3)
	HANDLE_COUNTER(2)
	HANDLE_COUNTER(1)
	HANDLE_COUNTER(0)
	}

	return handled;
}

#define M_CONFIG1_PC	(1 << 4)

static inline int __n_counters(void)
{
	if (!(read_c0_config1() & M_CONFIG1_PC))
		return 0;
	if (!(read_c0_perfctrl0() & M_PERFCTL_MORE))
		return 1;
	if (!(read_c0_perfctrl1() & M_PERFCTL_MORE))
		return 2;
	if (!(read_c0_perfctrl2() & M_PERFCTL_MORE))
		return 3;

	return 4;
}

static inline int n_counters(void)
{
	int counters;

	switch (current_cpu_type()) {
	case CPU_R10000:
		counters = 2;
		break;

	case CPU_R12000:
	case CPU_R14000:
		counters = 4;
		break;

	default:
		counters = __n_counters();
	}

	return counters;
}

static void reset_counters(void *arg)
{
	int counters = (int)(long)arg;
	switch (counters) {
	case 4:
		w_c0_perfctrl3(0);
		w_c0_perfcntr3(0);
	case 3:
		w_c0_perfctrl2(0);
		w_c0_perfcntr2(0);
	case 2:
		w_c0_perfctrl1(0);
		w_c0_perfcntr1(0);
	case 1:
		w_c0_perfctrl0(0);
		w_c0_perfcntr0(0);
	}
}

static int __init mipsxx_init(void)
{
	int counters;

	counters = n_counters();
	if (counters == 0) {
		printk(KERN_ERR "Oprofile: CPU has no performance counters\n");
		return -ENODEV;
	}

#ifdef CONFIG_MIPS_MT_SMP
	cpu_has_mipsmt_pertccounters = read_c0_config7() & (1<<19);
	if (!cpu_has_mipsmt_pertccounters)
		counters = counters_total_to_per_cpu(counters);
#endif
	on_each_cpu(reset_counters, (void *)(long)counters, 1);

	op_model_mipsxx_ops.num_counters = counters;
	switch (current_cpu_type()) {
	case CPU_14K:
		op_model_mipsxx_ops.cpu_type = "mips/14K";
		break;

	case CPU_14KE:
		op_model_mipsxx_ops.cpu_type = "mips/14KE";
		break;

	case CPU_20KC:
		op_model_mipsxx_ops.cpu_type = "mips/20K";
		break;

	case CPU_24K:
		op_model_mipsxx_ops.cpu_type = "mips/24K";
		break;

	case CPU_25KF:
		op_model_mipsxx_ops.cpu_type = "mips/25K";
		break;

	case CPU_1004K:
		op_model_mipsxx_ops.cpu_type = "mips/1004K";
		break;

	case CPU_34K:
		op_model_mipsxx_ops.cpu_type = "mips/34K";
		break;

	case CPU_1074K:
	case CPU_74K:
		op_model_mipsxx_ops.cpu_type = "mips/74K";
		break;

	case CPU_5KC:
		op_model_mipsxx_ops.cpu_type = "mips/5K";
		break;

	case CPU_R10000:
		if ((current_cpu_data.processor_id & 0xff) == 0x20)
			op_model_mipsxx_ops.cpu_type = "mips/r10000-v2.x";
		else
			op_model_mipsxx_ops.cpu_type = "mips/r10000";
		break;

	case CPU_R12000:
	case CPU_R14000:
		op_model_mipsxx_ops.cpu_type = "mips/r12000";
		break;

	case CPU_SB1:
	case CPU_SB1A:
		op_model_mipsxx_ops.cpu_type = "mips/sb1";
		break;

	default:
		printk(KERN_ERR "Profiling unsupported for this CPU\n");

		return -ENODEV;
	}

	save_perf_irq = perf_irq;
	perf_irq = mipsxx_perfcount_handler;

	return 0;
}

static void mipsxx_exit(void)
{
	int counters = op_model_mipsxx_ops.num_counters;

	counters = counters_per_cpu_to_total(counters);
	on_each_cpu(reset_counters, (void *)(long)counters, 1);

	perf_irq = save_perf_irq;
}

struct op_mips_model op_model_mipsxx_ops = {
	.reg_setup	= mipsxx_reg_setup,
	.cpu_setup	= mipsxx_cpu_setup,
	.init		= mipsxx_init,
	.exit		= mipsxx_exit,
	.cpu_start	= mipsxx_cpu_start,
	.cpu_stop	= mipsxx_cpu_stop,
};
