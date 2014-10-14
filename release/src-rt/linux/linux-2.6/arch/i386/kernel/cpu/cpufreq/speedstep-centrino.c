/*
 * cpufreq driver for Enhanced SpeedStep, as found in Intel's Pentium
 * M (part of the Centrino chipset).
 *
 * Since the original Pentium M, most new Intel CPUs support Enhanced
 * SpeedStep.
 *
 * Despite the "SpeedStep" in the name, this is almost entirely unlike
 * traditional SpeedStep.
 *
 * Modelled on speedstep.c
 *
 * Copyright (C) 2003 Jeremy Fitzhardinge <jeremy@goop.org>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/sched.h>	/* current */
#include <linux/delay.h>
#include <linux/compiler.h>

#ifdef CONFIG_X86_SPEEDSTEP_CENTRINO_ACPI
#include <linux/acpi.h>
#include <linux/dmi.h>
#include <acpi/processor.h>
#endif

#include <asm/msr.h>
#include <asm/processor.h>
#include <asm/cpufeature.h>

#define PFX		"speedstep-centrino: "
#define MAINTAINER	"cpufreq@lists.linux.org.uk"

#define dprintk(msg...) cpufreq_debug_printk(CPUFREQ_DEBUG_DRIVER, "speedstep-centrino", msg)

#define INTEL_MSR_RANGE	(0xffff)

struct cpu_id
{
	__u8	x86;            /* CPU family */
	__u8	x86_model;	/* model */
	__u8	x86_mask;	/* stepping */
};

enum {
	CPU_BANIAS,
	CPU_DOTHAN_A1,
	CPU_DOTHAN_A2,
	CPU_DOTHAN_B0,
	CPU_MP4HT_D0,
	CPU_MP4HT_E0,
};

static const struct cpu_id cpu_ids[] = {
	[CPU_BANIAS]	= { 6,  9, 5 },
	[CPU_DOTHAN_A1]	= { 6, 13, 1 },
	[CPU_DOTHAN_A2]	= { 6, 13, 2 },
	[CPU_DOTHAN_B0]	= { 6, 13, 6 },
	[CPU_MP4HT_D0]	= {15,  3, 4 },
	[CPU_MP4HT_E0]	= {15,  4, 1 },
};
#define N_IDS	ARRAY_SIZE(cpu_ids)

struct cpu_model
{
	const struct cpu_id *cpu_id;
	const char	*model_name;
	unsigned	max_freq; /* max clock in kHz */

	struct cpufreq_frequency_table *op_points; /* clock/voltage pairs */
};
static int centrino_verify_cpu_id(const struct cpuinfo_x86 *c, const struct cpu_id *x);

/* Operating points for current CPU */
static struct cpu_model *centrino_model[NR_CPUS];
static const struct cpu_id *centrino_cpu[NR_CPUS];

static struct cpufreq_driver centrino_driver;

#ifdef CONFIG_X86_SPEEDSTEP_CENTRINO_TABLE

/* Computes the correct form for IA32_PERF_CTL MSR for a particular
   frequency/voltage operating point; frequency in MHz, volts in mV.
   This is stored as "index" in the structure. */
#define OP(mhz, mv)							\
	{								\
		.frequency = (mhz) * 1000,				\
		.index = (((mhz)/100) << 8) | ((mv - 700) / 16)		\
	}

/*
 * These voltage tables were derived from the Intel Pentium M
 * datasheet, document 25261202.pdf, Table 5.  I have verified they
 * are consistent with my IBM ThinkPad X31, which has a 1.3GHz Pentium
 * M.
 */

/* Ultra Low Voltage Intel Pentium M processor 900MHz (Banias) */
static struct cpufreq_frequency_table banias_900[] =
{
	OP(600,  844),
	OP(800,  988),
	OP(900, 1004),
	{ .frequency = CPUFREQ_TABLE_END }
};

/* Ultra Low Voltage Intel Pentium M processor 1000MHz (Banias) */
static struct cpufreq_frequency_table banias_1000[] =
{
	OP(600,   844),
	OP(800,   972),
	OP(900,   988),
	OP(1000, 1004),
	{ .frequency = CPUFREQ_TABLE_END }
};

/* Low Voltage Intel Pentium M processor 1.10GHz (Banias) */
static struct cpufreq_frequency_table banias_1100[] =
{
	OP( 600,  956),
	OP( 800, 1020),
	OP( 900, 1100),
	OP(1000, 1164),
	OP(1100, 1180),
	{ .frequency = CPUFREQ_TABLE_END }
};


/* Low Voltage Intel Pentium M processor 1.20GHz (Banias) */
static struct cpufreq_frequency_table banias_1200[] =
{
	OP( 600,  956),
	OP( 800, 1004),
	OP( 900, 1020),
	OP(1000, 1100),
	OP(1100, 1164),
	OP(1200, 1180),
	{ .frequency = CPUFREQ_TABLE_END }
};

/* Intel Pentium M processor 1.30GHz (Banias) */
static struct cpufreq_frequency_table banias_1300[] =
{
	OP( 600,  956),
	OP( 800, 1260),
	OP(1000, 1292),
	OP(1200, 1356),
	OP(1300, 1388),
	{ .frequency = CPUFREQ_TABLE_END }
};

/* Intel Pentium M processor 1.40GHz (Banias) */
static struct cpufreq_frequency_table banias_1400[] =
{
	OP( 600,  956),
	OP( 800, 1180),
	OP(1000, 1308),
	OP(1200, 1436),
	OP(1400, 1484),
	{ .frequency = CPUFREQ_TABLE_END }
};

/* Intel Pentium M processor 1.50GHz (Banias) */
static struct cpufreq_frequency_table banias_1500[] =
{
	OP( 600,  956),
	OP( 800, 1116),
	OP(1000, 1228),
	OP(1200, 1356),
	OP(1400, 1452),
	OP(1500, 1484),
	{ .frequency = CPUFREQ_TABLE_END }
};

/* Intel Pentium M processor 1.60GHz (Banias) */
static struct cpufreq_frequency_table banias_1600[] =
{
	OP( 600,  956),
	OP( 800, 1036),
	OP(1000, 1164),
	OP(1200, 1276),
	OP(1400, 1420),
	OP(1600, 1484),
	{ .frequency = CPUFREQ_TABLE_END }
};

/* Intel Pentium M processor 1.70GHz (Banias) */
static struct cpufreq_frequency_table banias_1700[] =
{
	OP( 600,  956),
	OP( 800, 1004),
	OP(1000, 1116),
	OP(1200, 1228),
	OP(1400, 1308),
	OP(1700, 1484),
	{ .frequency = CPUFREQ_TABLE_END }
};
#undef OP

#define _BANIAS(cpuid, max, name)	\
{	.cpu_id		= cpuid,	\
	.model_name	= "Intel(R) Pentium(R) M processor " name "MHz", \
	.max_freq	= (max)*1000,	\
	.op_points	= banias_##max,	\
}
#define BANIAS(max)	_BANIAS(&cpu_ids[CPU_BANIAS], max, #max)

/* CPU models, their operating frequency range, and freq/voltage
   operating points */
static struct cpu_model models[] =
{
	_BANIAS(&cpu_ids[CPU_BANIAS], 900, " 900"),
	BANIAS(1000),
	BANIAS(1100),
	BANIAS(1200),
	BANIAS(1300),
	BANIAS(1400),
	BANIAS(1500),
	BANIAS(1600),
	BANIAS(1700),

	/* NULL model_name is a wildcard */
	{ &cpu_ids[CPU_DOTHAN_A1], NULL, 0, NULL },
	{ &cpu_ids[CPU_DOTHAN_A2], NULL, 0, NULL },
	{ &cpu_ids[CPU_DOTHAN_B0], NULL, 0, NULL },
	{ &cpu_ids[CPU_MP4HT_D0], NULL, 0, NULL },
	{ &cpu_ids[CPU_MP4HT_E0], NULL, 0, NULL },

	{ NULL, }
};
#undef _BANIAS
#undef BANIAS

static int centrino_cpu_init_table(struct cpufreq_policy *policy)
{
	struct cpuinfo_x86 *cpu = &cpu_data[policy->cpu];
	struct cpu_model *model;

	for(model = models; model->cpu_id != NULL; model++)
		if (centrino_verify_cpu_id(cpu, model->cpu_id) &&
		    (model->model_name == NULL ||
		     strcmp(cpu->x86_model_id, model->model_name) == 0))
			break;

	if (model->cpu_id == NULL) {
		/* No match at all */
		dprintk("no support for CPU model \"%s\": "
		       "send /proc/cpuinfo to " MAINTAINER "\n",
		       cpu->x86_model_id);
		return -ENOENT;
	}

	if (model->op_points == NULL) {
		/* Matched a non-match */
		dprintk("no table support for CPU model \"%s\"\n",
		       cpu->x86_model_id);
#ifndef CONFIG_X86_SPEEDSTEP_CENTRINO_ACPI
		dprintk("try compiling with CONFIG_X86_SPEEDSTEP_CENTRINO_ACPI enabled\n");
#endif
		return -ENOENT;
	}

	centrino_model[policy->cpu] = model;

	dprintk("found \"%s\": max frequency: %dkHz\n",
	       model->model_name, model->max_freq);

	return 0;
}

#else
static inline int centrino_cpu_init_table(struct cpufreq_policy *policy) { return -ENODEV; }
#endif /* CONFIG_X86_SPEEDSTEP_CENTRINO_TABLE */

static int centrino_verify_cpu_id(const struct cpuinfo_x86 *c, const struct cpu_id *x)
{
	if ((c->x86 == x->x86) &&
	    (c->x86_model == x->x86_model) &&
	    (c->x86_mask == x->x86_mask))
		return 1;
	return 0;
}

/* To be called only after centrino_model is initialized */
static unsigned extract_clock(unsigned msr, unsigned int cpu, int failsafe)
{
	int i;

	/*
	 * Extract clock in kHz from PERF_CTL value
	 * for centrino, as some DSDTs are buggy.
	 * Ideally, this can be done using the acpi_data structure.
	 */
	if ((centrino_cpu[cpu] == &cpu_ids[CPU_BANIAS]) ||
	    (centrino_cpu[cpu] == &cpu_ids[CPU_DOTHAN_A1]) ||
	    (centrino_cpu[cpu] == &cpu_ids[CPU_DOTHAN_B0])) {
		msr = (msr >> 8) & 0xff;
		return msr * 100000;
	}

	if ((!centrino_model[cpu]) || (!centrino_model[cpu]->op_points))
		return 0;

	msr &= 0xffff;
	for (i=0;centrino_model[cpu]->op_points[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (msr == centrino_model[cpu]->op_points[i].index)
			return centrino_model[cpu]->op_points[i].frequency;
	}
	if (failsafe)
		return centrino_model[cpu]->op_points[i-1].frequency;
	else
		return 0;
}

/* Return the current CPU frequency in kHz */
static unsigned int get_cur_freq(unsigned int cpu)
{
	unsigned l, h;
	unsigned clock_freq;
	cpumask_t saved_mask;

	saved_mask = current->cpus_allowed;
	set_cpus_allowed(current, cpumask_of_cpu(cpu));
	if (smp_processor_id() != cpu)
		return 0;

	rdmsr(MSR_IA32_PERF_STATUS, l, h);
	clock_freq = extract_clock(l, cpu, 0);

	if (unlikely(clock_freq == 0)) {
		/*
		 * On some CPUs, we can see transient MSR values (which are
		 * not present in _PSS), while CPU is doing some automatic
		 * P-state transition (like TM2). Get the last freq set 
		 * in PERF_CTL.
		 */
		rdmsr(MSR_IA32_PERF_CTL, l, h);
		clock_freq = extract_clock(l, cpu, 1);
	}

	set_cpus_allowed(current, saved_mask);
	return clock_freq;
}


#ifdef CONFIG_X86_SPEEDSTEP_CENTRINO_ACPI

static struct acpi_processor_performance *acpi_perf_data[NR_CPUS];

/*
 * centrino_cpu_early_init_acpi - Do the preregistering with ACPI P-States
 * library
 *
 * Before doing the actual init, we need to do _PSD related setup whenever
 * supported by the BIOS. These are handled by this early_init routine.
 */
static int centrino_cpu_early_init_acpi(void)
{
	unsigned int	i, j;
	struct acpi_processor_performance	*data;

	for_each_possible_cpu(i) {
		data = kzalloc(sizeof(struct acpi_processor_performance), 
				GFP_KERNEL);
		if (!data) {
			for_each_possible_cpu(j) {
				kfree(acpi_perf_data[j]);
				acpi_perf_data[j] = NULL;
			}
			return (-ENOMEM);
		}
		acpi_perf_data[i] = data;
	}

	acpi_processor_preregister_performance(acpi_perf_data);
	return 0;
}


#ifdef CONFIG_SMP
/*
 * Some BIOSes do SW_ANY coordination internally, either set it up in hw
 * or do it in BIOS firmware and won't inform about it to OS. If not
 * detected, this has a side effect of making CPU run at a different speed
 * than OS intended it to run at. Detect it and handle it cleanly.
 */
static int bios_with_sw_any_bug;
static int sw_any_bug_found(struct dmi_system_id *d)
{
	bios_with_sw_any_bug = 1;
	return 0;
}

static struct dmi_system_id sw_any_bug_dmi_table[] = {
	{
		.callback = sw_any_bug_found,
		.ident = "Supermicro Server X6DLP",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Supermicro"),
			DMI_MATCH(DMI_BIOS_VERSION, "080010"),
			DMI_MATCH(DMI_PRODUCT_NAME, "X6DLP"),
		},
	},
	{ }
};
#endif

/*
 * centrino_cpu_init_acpi - register with ACPI P-States library
 *
 * Register with the ACPI P-States library (part of drivers/acpi/processor.c)
 * in order to determine correct frequency and voltage pairings by reading
 * the _PSS of the ACPI DSDT or SSDT tables.
 */
static int centrino_cpu_init_acpi(struct cpufreq_policy *policy)
{
	unsigned long			cur_freq;
	int				result = 0, i;
	unsigned int			cpu = policy->cpu;
	struct acpi_processor_performance	*p;

	p = acpi_perf_data[cpu];

	/* register with ACPI core */
	if (acpi_processor_register_performance(p, cpu)) {
		dprintk(PFX "obtaining ACPI data failed\n");
		return -EIO;
	}

	policy->shared_type = p->shared_type;
	/*
	 * Will let policy->cpus know about dependency only when software 
	 * coordination is required.
	 */
	if (policy->shared_type == CPUFREQ_SHARED_TYPE_ALL ||
	    policy->shared_type == CPUFREQ_SHARED_TYPE_ANY) {
		policy->cpus = p->shared_cpu_map;
	}

#ifdef CONFIG_SMP
	dmi_check_system(sw_any_bug_dmi_table);
	if (bios_with_sw_any_bug && cpus_weight(policy->cpus) == 1) {
		policy->shared_type = CPUFREQ_SHARED_TYPE_ALL;
		policy->cpus = cpu_core_map[cpu];
	}
#endif

	/* verify the acpi_data */
	if (p->state_count <= 1) {
		dprintk("No P-States\n");
		result = -ENODEV;
		goto err_unreg;
	}

	if ((p->control_register.space_id != ACPI_ADR_SPACE_FIXED_HARDWARE) ||
	    (p->status_register.space_id != ACPI_ADR_SPACE_FIXED_HARDWARE)) {
		dprintk("Invalid control/status registers (%x - %x)\n",
			p->control_register.space_id, p->status_register.space_id);
		result = -EIO;
		goto err_unreg;
	}

	for (i=0; i<p->state_count; i++) {
		if ((p->states[i].control & INTEL_MSR_RANGE) !=
		    (p->states[i].status & INTEL_MSR_RANGE)) {
			dprintk("Different MSR bits in control (%llu) and status (%llu)\n",
				p->states[i].control, p->states[i].status);
			result = -EINVAL;
			goto err_unreg;
		}

		if (!p->states[i].core_frequency) {
			dprintk("Zero core frequency for state %u\n", i);
			result = -EINVAL;
			goto err_unreg;
		}

		if (p->states[i].core_frequency > p->states[0].core_frequency) {
			dprintk("P%u has larger frequency (%llu) than P0 (%llu), skipping\n", i,
				p->states[i].core_frequency, p->states[0].core_frequency);
			p->states[i].core_frequency = 0;
			continue;
		}
	}

	centrino_model[cpu] = kzalloc(sizeof(struct cpu_model), GFP_KERNEL);
	if (!centrino_model[cpu]) {
		result = -ENOMEM;
		goto err_unreg;
	}

	centrino_model[cpu]->model_name=NULL;
	centrino_model[cpu]->max_freq = p->states[0].core_frequency * 1000;
	centrino_model[cpu]->op_points =  kmalloc(sizeof(struct cpufreq_frequency_table) *
					     (p->state_count + 1), GFP_KERNEL);
        if (!centrino_model[cpu]->op_points) {
                result = -ENOMEM;
                goto err_kfree;
        }

        for (i=0; i<p->state_count; i++) {
		centrino_model[cpu]->op_points[i].index = p->states[i].control & INTEL_MSR_RANGE;
		centrino_model[cpu]->op_points[i].frequency = p->states[i].core_frequency * 1000;
		dprintk("adding state %i with frequency %u and control value %04x\n", 
			i, centrino_model[cpu]->op_points[i].frequency, centrino_model[cpu]->op_points[i].index);
	}
	centrino_model[cpu]->op_points[p->state_count].frequency = CPUFREQ_TABLE_END;

	cur_freq = get_cur_freq(cpu);

	for (i=0; i<p->state_count; i++) {
		if (!p->states[i].core_frequency) {
			dprintk("skipping state %u\n", i);
			centrino_model[cpu]->op_points[i].frequency = CPUFREQ_ENTRY_INVALID;
			continue;
		}
		
		if (extract_clock(centrino_model[cpu]->op_points[i].index, cpu, 0) !=
		    (centrino_model[cpu]->op_points[i].frequency)) {
			dprintk("Invalid encoded frequency (%u vs. %u)\n",
				extract_clock(centrino_model[cpu]->op_points[i].index, cpu, 0),
				centrino_model[cpu]->op_points[i].frequency);
			result = -EINVAL;
			goto err_kfree_all;
		}

		if (cur_freq == centrino_model[cpu]->op_points[i].frequency)
			p->state = i;
	}

	/* notify BIOS that we exist */
	acpi_processor_notify_smm(THIS_MODULE);
	printk("speedstep-centrino with X86_SPEEDSTEP_CENTRINO_ACPI "
	       "config is deprecated.\n "
	       "Use X86_ACPI_CPUFREQ (acpi-cpufreq) instead.\n" );

	return 0;

 err_kfree_all:
	kfree(centrino_model[cpu]->op_points);
 err_kfree:
	kfree(centrino_model[cpu]);
 err_unreg:
	acpi_processor_unregister_performance(p, cpu);
	dprintk(PFX "invalid ACPI data\n");
	return (result);
}
#else
static inline int centrino_cpu_init_acpi(struct cpufreq_policy *policy) { return -ENODEV; }
static inline int centrino_cpu_early_init_acpi(void) { return 0; }
#endif

static int centrino_cpu_init(struct cpufreq_policy *policy)
{
	struct cpuinfo_x86 *cpu = &cpu_data[policy->cpu];
	unsigned freq;
	unsigned l, h;
	int ret;
	int i;

	/* Only Intel makes Enhanced Speedstep-capable CPUs */
	if (cpu->x86_vendor != X86_VENDOR_INTEL || !cpu_has(cpu, X86_FEATURE_EST))
		return -ENODEV;

	if (cpu_has(cpu, X86_FEATURE_CONSTANT_TSC))
		centrino_driver.flags |= CPUFREQ_CONST_LOOPS;

	if (centrino_cpu_init_acpi(policy)) {
		if (policy->cpu != 0)
			return -ENODEV;

		for (i = 0; i < N_IDS; i++)
			if (centrino_verify_cpu_id(cpu, &cpu_ids[i]))
				break;

		if (i != N_IDS)
			centrino_cpu[policy->cpu] = &cpu_ids[i];

		if (!centrino_cpu[policy->cpu]) {
			dprintk("found unsupported CPU with "
			"Enhanced SpeedStep: send /proc/cpuinfo to "
			MAINTAINER "\n");
			return -ENODEV;
		}

		if (centrino_cpu_init_table(policy)) {
			return -ENODEV;
		}
	}

	/* Check to see if Enhanced SpeedStep is enabled, and try to
	   enable it if not. */
	rdmsr(MSR_IA32_MISC_ENABLE, l, h);

	if (!(l & (1<<16))) {
		l |= (1<<16);
		dprintk("trying to enable Enhanced SpeedStep (%x)\n", l);
		wrmsr(MSR_IA32_MISC_ENABLE, l, h);

		/* check to see if it stuck */
		rdmsr(MSR_IA32_MISC_ENABLE, l, h);
		if (!(l & (1<<16))) {
			printk(KERN_INFO PFX "couldn't enable Enhanced SpeedStep\n");
			return -ENODEV;
		}
	}

	freq = get_cur_freq(policy->cpu);

	policy->governor = CPUFREQ_DEFAULT_GOVERNOR;
	policy->cpuinfo.transition_latency = 10000; /* 10uS transition latency */
	policy->cur = freq;

	dprintk("centrino_cpu_init: cur=%dkHz\n", policy->cur);

	ret = cpufreq_frequency_table_cpuinfo(policy, centrino_model[policy->cpu]->op_points);
	if (ret)
		return (ret);

	cpufreq_frequency_table_get_attr(centrino_model[policy->cpu]->op_points, policy->cpu);

	return 0;
}

static int centrino_cpu_exit(struct cpufreq_policy *policy)
{
	unsigned int cpu = policy->cpu;

	if (!centrino_model[cpu])
		return -ENODEV;

	cpufreq_frequency_table_put_attr(cpu);

#ifdef CONFIG_X86_SPEEDSTEP_CENTRINO_ACPI
	if (!centrino_model[cpu]->model_name) {
		static struct acpi_processor_performance *p;

		if (acpi_perf_data[cpu]) {
			p = acpi_perf_data[cpu];
			dprintk("unregistering and freeing ACPI data\n");
			acpi_processor_unregister_performance(p, cpu);
			kfree(centrino_model[cpu]->op_points);
			kfree(centrino_model[cpu]);
		}
	}
#endif

	centrino_model[cpu] = NULL;

	return 0;
}

/**
 * centrino_verify - verifies a new CPUFreq policy
 * @policy: new policy
 *
 * Limit must be within this model's frequency range at least one
 * border included.
 */
static int centrino_verify (struct cpufreq_policy *policy)
{
	return cpufreq_frequency_table_verify(policy, centrino_model[policy->cpu]->op_points);
}

/**
 * centrino_setpolicy - set a new CPUFreq policy
 * @policy: new policy
 * @target_freq: the target frequency
 * @relation: how that frequency relates to achieved frequency (CPUFREQ_RELATION_L or CPUFREQ_RELATION_H)
 *
 * Sets a new CPUFreq policy.
 */
static int centrino_target (struct cpufreq_policy *policy,
			    unsigned int target_freq,
			    unsigned int relation)
{
	unsigned int    newstate = 0;
	unsigned int	msr, oldmsr = 0, h = 0, cpu = policy->cpu;
	struct cpufreq_freqs	freqs;
	cpumask_t		online_policy_cpus;
	cpumask_t		saved_mask;
	cpumask_t		set_mask;
	cpumask_t		covered_cpus;
	int			retval = 0;
	unsigned int		j, k, first_cpu, tmp;

	if (unlikely(centrino_model[cpu] == NULL))
		return -ENODEV;

	if (unlikely(cpufreq_frequency_table_target(policy,
			centrino_model[cpu]->op_points,
			target_freq,
			relation,
			&newstate))) {
		return -EINVAL;
	}

#ifdef CONFIG_HOTPLUG_CPU
	/* cpufreq holds the hotplug lock, so we are safe from here on */
	cpus_and(online_policy_cpus, cpu_online_map, policy->cpus);
#else
	online_policy_cpus = policy->cpus;
#endif

	saved_mask = current->cpus_allowed;
	first_cpu = 1;
	cpus_clear(covered_cpus);
	for_each_cpu_mask(j, online_policy_cpus) {
		/*
		 * Support for SMP systems.
		 * Make sure we are running on CPU that wants to change freq
		 */
		cpus_clear(set_mask);
		if (policy->shared_type == CPUFREQ_SHARED_TYPE_ANY)
			cpus_or(set_mask, set_mask, online_policy_cpus);
		else
			cpu_set(j, set_mask);

		set_cpus_allowed(current, set_mask);
		preempt_disable();
		if (unlikely(!cpu_isset(smp_processor_id(), set_mask))) {
			dprintk("couldn't limit to CPUs in this domain\n");
			retval = -EAGAIN;
			if (first_cpu) {
				/* We haven't started the transition yet. */
				goto migrate_end;
			}
			preempt_enable();
			break;
		}

		msr = centrino_model[cpu]->op_points[newstate].index;

		if (first_cpu) {
			rdmsr(MSR_IA32_PERF_CTL, oldmsr, h);
			if (msr == (oldmsr & 0xffff)) {
				dprintk("no change needed - msr was and needs "
					"to be %x\n", oldmsr);
				retval = 0;
				goto migrate_end;
			}

			freqs.old = extract_clock(oldmsr, cpu, 0);
			freqs.new = extract_clock(msr, cpu, 0);

			dprintk("target=%dkHz old=%d new=%d msr=%04x\n",
				target_freq, freqs.old, freqs.new, msr);

			for_each_cpu_mask(k, online_policy_cpus) {
				freqs.cpu = k;
				cpufreq_notify_transition(&freqs,
					CPUFREQ_PRECHANGE);
			}

			first_cpu = 0;
			/* all but 16 LSB are reserved, treat them with care */
			oldmsr &= ~0xffff;
			msr &= 0xffff;
			oldmsr |= msr;
		}

		wrmsr(MSR_IA32_PERF_CTL, oldmsr, h);
		if (policy->shared_type == CPUFREQ_SHARED_TYPE_ANY) {
			preempt_enable();
			break;
		}

		cpu_set(j, covered_cpus);
		preempt_enable();
	}

	for_each_cpu_mask(k, online_policy_cpus) {
		freqs.cpu = k;
		cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);
	}

	if (unlikely(retval)) {
		/*
		 * We have failed halfway through the frequency change.
		 * We have sent callbacks to policy->cpus and
		 * MSRs have already been written on coverd_cpus.
		 * Best effort undo..
		 */

		if (!cpus_empty(covered_cpus)) {
			for_each_cpu_mask(j, covered_cpus) {
				set_cpus_allowed(current, cpumask_of_cpu(j));
				wrmsr(MSR_IA32_PERF_CTL, oldmsr, h);
			}
		}

		tmp = freqs.new;
		freqs.new = freqs.old;
		freqs.old = tmp;
		for_each_cpu_mask(j, online_policy_cpus) {
			freqs.cpu = j;
			cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);
			cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);
		}
	}
	set_cpus_allowed(current, saved_mask);
	return 0;

migrate_end:
	preempt_enable();
	set_cpus_allowed(current, saved_mask);
	return 0;
}

static struct freq_attr* centrino_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver centrino_driver = {
	.name		= "centrino", /* should be speedstep-centrino,
					 but there's a 16 char limit */
	.init		= centrino_cpu_init,
	.exit		= centrino_cpu_exit,
	.verify		= centrino_verify,
	.target		= centrino_target,
	.get		= get_cur_freq,
	.attr           = centrino_attr,
	.owner		= THIS_MODULE,
};


/**
 * centrino_init - initializes the Enhanced SpeedStep CPUFreq driver
 *
 * Initializes the Enhanced SpeedStep support. Returns -ENODEV on
 * unsupported devices, -ENOENT if there's no voltage table for this
 * particular CPU model, -EINVAL on problems during initiatization,
 * and zero on success.
 *
 * This is quite picky.  Not only does the CPU have to advertise the
 * "est" flag in the cpuid capability flags, we look for a specific
 * CPU model and stepping, and we need to have the exact model name in
 * our voltage tables.  That is, be paranoid about not releasing
 * someone's valuable magic smoke.
 */
static int __init centrino_init(void)
{
	struct cpuinfo_x86 *cpu = cpu_data;

	if (!cpu_has(cpu, X86_FEATURE_EST))
		return -ENODEV;

	centrino_cpu_early_init_acpi();

	return cpufreq_register_driver(&centrino_driver);
}

static void __exit centrino_exit(void)
{
#ifdef CONFIG_X86_SPEEDSTEP_CENTRINO_ACPI
	unsigned int j;
#endif
	
	cpufreq_unregister_driver(&centrino_driver);

#ifdef CONFIG_X86_SPEEDSTEP_CENTRINO_ACPI
	for_each_possible_cpu(j) {
		kfree(acpi_perf_data[j]);
		acpi_perf_data[j] = NULL;
	}
#endif
}

MODULE_AUTHOR ("Jeremy Fitzhardinge <jeremy@goop.org>");
MODULE_DESCRIPTION ("Enhanced SpeedStep driver for Intel Pentium M processors.");
MODULE_LICENSE ("GPL");

late_initcall(centrino_init);
module_exit(centrino_exit);
