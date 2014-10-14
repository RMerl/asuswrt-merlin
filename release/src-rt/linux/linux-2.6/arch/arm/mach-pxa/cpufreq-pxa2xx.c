/*
 *  linux/arch/arm/mach-pxa/cpufreq-pxa2xx.c
 *
 *  Copyright (C) 2002,2003 Intrinsyc Software
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 *   31-Jul-2002 : Initial version [FB]
 *   29-Jan-2003 : added PXA255 support [FB]
 *   20-Apr-2003 : ported to v2.5 (Dustin McIntire, Sensoria Corp.)
 *
 * Note:
 *   This driver may change the memory bus clock rate, but will not do any
 *   platform specific access timing changes... for example if you have flash
 *   memory connected to CS0, you will need to register a platform specific
 *   notifier which will adjust the memory access strobes to maintain a
 *   minimum strobe width.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/io.h>

#include <mach/pxa2xx-regs.h>
#include <mach/smemc.h>

#ifdef DEBUG
static unsigned int freq_debug;
module_param(freq_debug, uint, 0);
MODULE_PARM_DESC(freq_debug, "Set the debug messages to on=1/off=0");
#else
#define freq_debug  0
#endif

static struct regulator *vcc_core;

static unsigned int pxa27x_maxfreq;
module_param(pxa27x_maxfreq, uint, 0);
MODULE_PARM_DESC(pxa27x_maxfreq, "Set the pxa27x maxfreq in MHz"
		 "(typically 624=>pxa270, 416=>pxa271, 520=>pxa272)");

typedef struct {
	unsigned int khz;
	unsigned int membus;
	unsigned int cccr;
	unsigned int div2;
	unsigned int cclkcfg;
	int vmin;
	int vmax;
} pxa_freqs_t;

/* Define the refresh period in mSec for the SDRAM and the number of rows */
#define SDRAM_TREF	64	/* standard 64ms SDRAM */
static unsigned int sdram_rows;

#define CCLKCFG_TURBO		0x1
#define CCLKCFG_FCS		0x2
#define CCLKCFG_HALFTURBO	0x4
#define CCLKCFG_FASTBUS		0x8
#define MDREFR_DB2_MASK		(MDREFR_K2DB2 | MDREFR_K1DB2)
#define MDREFR_DRI_MASK		0xFFF

#define MDCNFG_DRAC2(mdcnfg) (((mdcnfg) >> 21) & 0x3)
#define MDCNFG_DRAC0(mdcnfg) (((mdcnfg) >> 5) & 0x3)

/*
 * PXA255 definitions
 */
/* Use the run mode frequencies for the CPUFREQ_POLICY_PERFORMANCE policy */
#define CCLKCFG			CCLKCFG_TURBO | CCLKCFG_FCS

static pxa_freqs_t pxa255_run_freqs[] =
{
	/* CPU   MEMBUS  CCCR  DIV2 CCLKCFG	           run  turbo PXbus SDRAM */
	{ 99500,  99500, 0x121, 1,  CCLKCFG, -1, -1},	/*  99,   99,   50,   50  */
	{132700, 132700, 0x123, 1,  CCLKCFG, -1, -1},	/* 133,  133,   66,   66  */
	{199100,  99500, 0x141, 0,  CCLKCFG, -1, -1},	/* 199,  199,   99,   99  */
	{265400, 132700, 0x143, 1,  CCLKCFG, -1, -1},	/* 265,  265,  133,   66  */
	{331800, 165900, 0x145, 1,  CCLKCFG, -1, -1},	/* 331,  331,  166,   83  */
	{398100,  99500, 0x161, 0,  CCLKCFG, -1, -1},	/* 398,  398,  196,   99  */
};

/* Use the turbo mode frequencies for the CPUFREQ_POLICY_POWERSAVE policy */
static pxa_freqs_t pxa255_turbo_freqs[] =
{
	/* CPU   MEMBUS  CCCR  DIV2 CCLKCFG	   run  turbo PXbus SDRAM */
	{ 99500, 99500,  0x121, 1,  CCLKCFG, -1, -1},	/*  99,   99,   50,   50  */
	{199100, 99500,  0x221, 0,  CCLKCFG, -1, -1},	/*  99,  199,   50,   99  */
	{298500, 99500,  0x321, 0,  CCLKCFG, -1, -1},	/*  99,  287,   50,   99  */
	{298600, 99500,  0x1c1, 0,  CCLKCFG, -1, -1},	/* 199,  287,   99,   99  */
	{398100, 99500,  0x241, 0,  CCLKCFG, -1, -1},	/* 199,  398,   99,   99  */
};

#define NUM_PXA25x_RUN_FREQS ARRAY_SIZE(pxa255_run_freqs)
#define NUM_PXA25x_TURBO_FREQS ARRAY_SIZE(pxa255_turbo_freqs)

static struct cpufreq_frequency_table
	pxa255_run_freq_table[NUM_PXA25x_RUN_FREQS+1];
static struct cpufreq_frequency_table
	pxa255_turbo_freq_table[NUM_PXA25x_TURBO_FREQS+1];

static unsigned int pxa255_turbo_table;
module_param(pxa255_turbo_table, uint, 0);
MODULE_PARM_DESC(pxa255_turbo_table, "Selects the frequency table (0 = run table, !0 = turbo table)");

/*
 * PXA270 definitions
 *
 * For the PXA27x:
 * Control variables are A, L, 2N for CCCR; B, HT, T for CLKCFG.
 *
 * A = 0 => memory controller clock from table 3-7,
 * A = 1 => memory controller clock = system bus clock
 * Run mode frequency	= 13 MHz * L
 * Turbo mode frequency = 13 MHz * L * N
 * System bus frequency = 13 MHz * L / (B + 1)
 *
 * In CCCR:
 * A = 1
 * L = 16	  oscillator to run mode ratio
 * 2N = 6	  2 * (turbo mode to run mode ratio)
 *
 * In CCLKCFG:
 * B = 1	  Fast bus mode
 * HT = 0	  Half-Turbo mode
 * T = 1	  Turbo mode
 *
 * For now, just support some of the combinations in table 3-7 of
 * PXA27x Processor Family Developer's Manual to simplify frequency
 * change sequences.
 */
#define PXA27x_CCCR(A, L, N2) (A << 25 | N2 << 7 | L)
#define CCLKCFG2(B, HT, T) \
  (CCLKCFG_FCS | \
   ((B)  ? CCLKCFG_FASTBUS : 0) | \
   ((HT) ? CCLKCFG_HALFTURBO : 0) | \
   ((T)  ? CCLKCFG_TURBO : 0))

static pxa_freqs_t pxa27x_freqs[] = {
	{104000, 104000, PXA27x_CCCR(1,	 8, 2), 0, CCLKCFG2(1, 0, 1),  900000, 1705000 },
	{156000, 104000, PXA27x_CCCR(1,	 8, 3), 0, CCLKCFG2(1, 0, 1), 1000000, 1705000 },
	{208000, 208000, PXA27x_CCCR(0, 16, 2), 1, CCLKCFG2(0, 0, 1), 1180000, 1705000 },
	{312000, 208000, PXA27x_CCCR(1, 16, 3), 1, CCLKCFG2(1, 0, 1), 1250000, 1705000 },
	{416000, 208000, PXA27x_CCCR(1, 16, 4), 1, CCLKCFG2(1, 0, 1), 1350000, 1705000 },
	{520000, 208000, PXA27x_CCCR(1, 16, 5), 1, CCLKCFG2(1, 0, 1), 1450000, 1705000 },
	{624000, 208000, PXA27x_CCCR(1, 16, 6), 1, CCLKCFG2(1, 0, 1), 1550000, 1705000 }
};

#define NUM_PXA27x_FREQS ARRAY_SIZE(pxa27x_freqs)
static struct cpufreq_frequency_table
	pxa27x_freq_table[NUM_PXA27x_FREQS+1];

extern unsigned get_clk_frequency_khz(int info);

#ifdef CONFIG_REGULATOR

static int pxa_cpufreq_change_voltage(pxa_freqs_t *pxa_freq)
{
	int ret = 0;
	int vmin, vmax;

	if (!cpu_is_pxa27x())
		return 0;

	vmin = pxa_freq->vmin;
	vmax = pxa_freq->vmax;
	if ((vmin == -1) || (vmax == -1))
		return 0;

	ret = regulator_set_voltage(vcc_core, vmin, vmax);
	if (ret)
		pr_err("cpufreq: Failed to set vcc_core in [%dmV..%dmV]\n",
		       vmin, vmax);
	return ret;
}

static __init void pxa_cpufreq_init_voltages(void)
{
	vcc_core = regulator_get(NULL, "vcc_core");
	if (IS_ERR(vcc_core)) {
		pr_info("cpufreq: Didn't find vcc_core regulator\n");
		vcc_core = NULL;
	} else {
		pr_info("cpufreq: Found vcc_core regulator\n");
	}
}
#else
static int pxa_cpufreq_change_voltage(pxa_freqs_t *pxa_freq)
{
	return 0;
}

static __init void pxa_cpufreq_init_voltages(void) { }
#endif

static void find_freq_tables(struct cpufreq_frequency_table **freq_table,
			     pxa_freqs_t **pxa_freqs)
{
	if (cpu_is_pxa25x()) {
		if (!pxa255_turbo_table) {
			*pxa_freqs = pxa255_run_freqs;
			*freq_table = pxa255_run_freq_table;
		} else {
			*pxa_freqs = pxa255_turbo_freqs;
			*freq_table = pxa255_turbo_freq_table;
		}
	}
	if (cpu_is_pxa27x()) {
		*pxa_freqs = pxa27x_freqs;
		*freq_table = pxa27x_freq_table;
	}
}

static void pxa27x_guess_max_freq(void)
{
	if (!pxa27x_maxfreq) {
		pxa27x_maxfreq = 416000;
		printk(KERN_INFO "PXA CPU 27x max frequency not defined "
		       "(pxa27x_maxfreq), assuming pxa271 with %dkHz maxfreq\n",
		       pxa27x_maxfreq);
	} else {
		pxa27x_maxfreq *= 1000;
	}
}

static void init_sdram_rows(void)
{
	uint32_t mdcnfg = __raw_readl(MDCNFG);
	unsigned int drac2 = 0, drac0 = 0;

	if (mdcnfg & (MDCNFG_DE2 | MDCNFG_DE3))
		drac2 = MDCNFG_DRAC2(mdcnfg);

	if (mdcnfg & (MDCNFG_DE0 | MDCNFG_DE1))
		drac0 = MDCNFG_DRAC0(mdcnfg);

	sdram_rows = 1 << (11 + max(drac0, drac2));
}

static u32 mdrefr_dri(unsigned int freq)
{
	u32 interval = freq * SDRAM_TREF / sdram_rows;

	return (interval - (cpu_is_pxa27x() ? 31 : 0)) / 32;
}

/* find a valid frequency point */
static int pxa_verify_policy(struct cpufreq_policy *policy)
{
	struct cpufreq_frequency_table *pxa_freqs_table;
	pxa_freqs_t *pxa_freqs;
	int ret;

	find_freq_tables(&pxa_freqs_table, &pxa_freqs);
	ret = cpufreq_frequency_table_verify(policy, pxa_freqs_table);

	if (freq_debug)
		pr_debug("Verified CPU policy: %dKhz min to %dKhz max\n",
			 policy->min, policy->max);

	return ret;
}

static unsigned int pxa_cpufreq_get(unsigned int cpu)
{
	return get_clk_frequency_khz(0);
}

static int pxa_set_target(struct cpufreq_policy *policy,
			  unsigned int target_freq,
			  unsigned int relation)
{
	struct cpufreq_frequency_table *pxa_freqs_table;
	pxa_freqs_t *pxa_freq_settings;
	struct cpufreq_freqs freqs;
	unsigned int idx;
	unsigned long flags;
	unsigned int new_freq_cpu, new_freq_mem;
	unsigned int unused, preset_mdrefr, postset_mdrefr, cclkcfg;
	int ret = 0;

	/* Get the current policy */
	find_freq_tables(&pxa_freqs_table, &pxa_freq_settings);

	/* Lookup the next frequency */
	if (cpufreq_frequency_table_target(policy, pxa_freqs_table,
					   target_freq, relation, &idx)) {
		return -EINVAL;
	}

	new_freq_cpu = pxa_freq_settings[idx].khz;
	new_freq_mem = pxa_freq_settings[idx].membus;
	freqs.old = policy->cur;
	freqs.new = new_freq_cpu;
	freqs.cpu = policy->cpu;

	if (freq_debug)
		pr_debug("Changing CPU frequency to %d Mhz, (SDRAM %d Mhz)\n",
			 freqs.new / 1000, (pxa_freq_settings[idx].div2) ?
			 (new_freq_mem / 2000) : (new_freq_mem / 1000));

	if (vcc_core && freqs.new > freqs.old)
		ret = pxa_cpufreq_change_voltage(&pxa_freq_settings[idx]);
	if (ret)
		return ret;
	/*
	 * Tell everyone what we're about to do...
	 * you should add a notify client with any platform specific
	 * Vcc changing capability
	 */
	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

	/* Calculate the next MDREFR.  If we're slowing down the SDRAM clock
	 * we need to preset the smaller DRI before the change.	 If we're
	 * speeding up we need to set the larger DRI value after the change.
	 */
	preset_mdrefr = postset_mdrefr = __raw_readl(MDREFR);
	if ((preset_mdrefr & MDREFR_DRI_MASK) > mdrefr_dri(new_freq_mem)) {
		preset_mdrefr = (preset_mdrefr & ~MDREFR_DRI_MASK);
		preset_mdrefr |= mdrefr_dri(new_freq_mem);
	}
	postset_mdrefr =
		(postset_mdrefr & ~MDREFR_DRI_MASK) | mdrefr_dri(new_freq_mem);

	/* If we're dividing the memory clock by two for the SDRAM clock, this
	 * must be set prior to the change.  Clearing the divide must be done
	 * after the change.
	 */
	if (pxa_freq_settings[idx].div2) {
		preset_mdrefr  |= MDREFR_DB2_MASK;
		postset_mdrefr |= MDREFR_DB2_MASK;
	} else {
		postset_mdrefr &= ~MDREFR_DB2_MASK;
	}

	local_irq_save(flags);

	/* Set new the CCCR and prepare CCLKCFG */
	CCCR = pxa_freq_settings[idx].cccr;
	cclkcfg = pxa_freq_settings[idx].cclkcfg;

	asm volatile("							\n\
		ldr	r4, [%1]		/* load MDREFR */	\n\
		b	2f						\n\
		.align	5						\n\
1:									\n\
		str	%3, [%1]		/* preset the MDREFR */	\n\
		mcr	p14, 0, %2, c6, c0, 0	/* set CCLKCFG[FCS] */	\n\
		str	%4, [%1]		/* postset the MDREFR */ \n\
									\n\
		b	3f						\n\
2:		b	1b						\n\
3:		nop							\n\
	  "
		     : "=&r" (unused)
		     : "r" (MDREFR), "r" (cclkcfg),
		       "r" (preset_mdrefr), "r" (postset_mdrefr)
		     : "r4", "r5");
	local_irq_restore(flags);

	/*
	 * Tell everyone what we've just done...
	 * you should add a notify client with any platform specific
	 * SDRAM refresh timer adjustments
	 */
	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	/*
	 * Even if voltage setting fails, we don't report it, as the frequency
	 * change succeeded. The voltage reduction is not a critical failure,
	 * only power savings will suffer from this.
	 *
	 * Note: if the voltage change fails, and a return value is returned, a
	 * bug is triggered (seems a deadlock). Should anybody find out where,
	 * the "return 0" should become a "return ret".
	 */
	if (vcc_core && freqs.new < freqs.old)
		ret = pxa_cpufreq_change_voltage(&pxa_freq_settings[idx]);

	return 0;
}

static int pxa_cpufreq_init(struct cpufreq_policy *policy)
{
	int i;
	unsigned int freq;
	struct cpufreq_frequency_table *pxa255_freq_table;
	pxa_freqs_t *pxa255_freqs;

	/* try to guess pxa27x cpu */
	if (cpu_is_pxa27x())
		pxa27x_guess_max_freq();

	pxa_cpufreq_init_voltages();

	init_sdram_rows();

	/* set default policy and cpuinfo */
	policy->cpuinfo.transition_latency = 1000; /* FIXME: 1 ms, assumed */
	policy->cur = get_clk_frequency_khz(0);	   /* current freq */
	policy->min = policy->max = policy->cur;

	/* Generate pxa25x the run cpufreq_frequency_table struct */
	for (i = 0; i < NUM_PXA25x_RUN_FREQS; i++) {
		pxa255_run_freq_table[i].frequency = pxa255_run_freqs[i].khz;
		pxa255_run_freq_table[i].index = i;
	}
	pxa255_run_freq_table[i].frequency = CPUFREQ_TABLE_END;

	/* Generate pxa25x the turbo cpufreq_frequency_table struct */
	for (i = 0; i < NUM_PXA25x_TURBO_FREQS; i++) {
		pxa255_turbo_freq_table[i].frequency =
			pxa255_turbo_freqs[i].khz;
		pxa255_turbo_freq_table[i].index = i;
	}
	pxa255_turbo_freq_table[i].frequency = CPUFREQ_TABLE_END;

	pxa255_turbo_table = !!pxa255_turbo_table;

	/* Generate the pxa27x cpufreq_frequency_table struct */
	for (i = 0; i < NUM_PXA27x_FREQS; i++) {
		freq = pxa27x_freqs[i].khz;
		if (freq > pxa27x_maxfreq)
			break;
		pxa27x_freq_table[i].frequency = freq;
		pxa27x_freq_table[i].index = i;
	}
	pxa27x_freq_table[i].index = i;
	pxa27x_freq_table[i].frequency = CPUFREQ_TABLE_END;

	/*
	 * Set the policy's minimum and maximum frequencies from the tables
	 * just constructed.  This sets cpuinfo.mxx_freq, min and max.
	 */
	if (cpu_is_pxa25x()) {
		find_freq_tables(&pxa255_freq_table, &pxa255_freqs);
		pr_info("PXA255 cpufreq using %s frequency table\n",
			pxa255_turbo_table ? "turbo" : "run");
		cpufreq_frequency_table_cpuinfo(policy, pxa255_freq_table);
	}
	else if (cpu_is_pxa27x())
		cpufreq_frequency_table_cpuinfo(policy, pxa27x_freq_table);

	printk(KERN_INFO "PXA CPU frequency change support initialized\n");

	return 0;
}

static struct cpufreq_driver pxa_cpufreq_driver = {
	.verify	= pxa_verify_policy,
	.target	= pxa_set_target,
	.init	= pxa_cpufreq_init,
	.get	= pxa_cpufreq_get,
	.name	= "PXA2xx",
};

static int __init pxa_cpu_init(void)
{
	int ret = -ENODEV;
	if (cpu_is_pxa25x() || cpu_is_pxa27x())
		ret = cpufreq_register_driver(&pxa_cpufreq_driver);
	return ret;
}

static void __exit pxa_cpu_exit(void)
{
	cpufreq_unregister_driver(&pxa_cpufreq_driver);
}


MODULE_AUTHOR("Intrinsyc Software Inc.");
MODULE_DESCRIPTION("CPU frequency changing driver for the PXA architecture");
MODULE_LICENSE("GPL");
module_init(pxa_cpu_init);
module_exit(pxa_cpu_exit);
