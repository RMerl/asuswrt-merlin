/*
 * kernel/time/sched_debug.c
 *
 * Print the CFS rbtree
 *
 * Copyright(C) 2007, Red Hat, Inc., Ingo Molnar
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>

/*
 * This allows printing both to /proc/sched_debug and
 * to the console
 */
#define SEQ_printf(m, x...)			\
 do {						\
	if (m)					\
		seq_printf(m, x);		\
	else					\
		printk(x);			\
 } while (0)

/*
 * Ease the printing of nsec fields:
 */
static long long nsec_high(unsigned long long nsec)
{
	if ((long long)nsec < 0) {
		nsec = -nsec;
		do_div(nsec, 1000000);
		return -nsec;
	}
	do_div(nsec, 1000000);

	return nsec;
}

static unsigned long nsec_low(unsigned long long nsec)
{
	if ((long long)nsec < 0)
		nsec = -nsec;

	return do_div(nsec, 1000000);
}

#define SPLIT_NS(x) nsec_high(x), nsec_low(x)

#ifdef CONFIG_FAIR_GROUP_SCHED
static void print_cfs_group_stats(struct seq_file *m, int cpu,
		struct task_group *tg)
{
	struct sched_entity *se = tg->se[cpu];
	if (!se)
		return;

#define P(F) \
	SEQ_printf(m, "  .%-30s: %lld\n", #F, (long long)F)
#define PN(F) \
	SEQ_printf(m, "  .%-30s: %lld.%06ld\n", #F, SPLIT_NS((long long)F))

	PN(se->exec_start);
	PN(se->vruntime);
	PN(se->sum_exec_runtime);
#ifdef CONFIG_SCHEDSTATS
	PN(se->statistics.wait_start);
	PN(se->statistics.sleep_start);
	PN(se->statistics.block_start);
	PN(se->statistics.sleep_max);
	PN(se->statistics.block_max);
	PN(se->statistics.exec_max);
	PN(se->statistics.slice_max);
	PN(se->statistics.wait_max);
	PN(se->statistics.wait_sum);
	P(se->statistics.wait_count);
#endif
	P(se->load.weight);
#undef PN
#undef P
}
#endif

static void
print_task(struct seq_file *m, struct rq *rq, struct task_struct *p)
{
	if (rq->curr == p)
		SEQ_printf(m, "R");
	else
		SEQ_printf(m, " ");

	SEQ_printf(m, "%15s %5d %9Ld.%06ld %9Ld %5d ",
		p->comm, p->pid,
		SPLIT_NS(p->se.vruntime),
		(long long)(p->nvcsw + p->nivcsw),
		p->prio);
#ifdef CONFIG_SCHEDSTATS
	SEQ_printf(m, "%9Ld.%06ld %9Ld.%06ld %9Ld.%06ld",
		SPLIT_NS(p->se.vruntime),
		SPLIT_NS(p->se.sum_exec_runtime),
		SPLIT_NS(p->se.statistics.sum_sleep_runtime));
#else
	SEQ_printf(m, "%15Ld %15Ld %15Ld.%06ld %15Ld.%06ld %15Ld.%06ld",
		0LL, 0LL, 0LL, 0L, 0LL, 0L, 0LL, 0L);
#endif

#ifdef CONFIG_CGROUP_SCHED
	{
		char path[64];

		rcu_read_lock();
		cgroup_path(task_group(p)->css.cgroup, path, sizeof(path));
		rcu_read_unlock();
		SEQ_printf(m, " %s", path);
	}
#endif
	SEQ_printf(m, "\n");
}

static void print_rq(struct seq_file *m, struct rq *rq, int rq_cpu)
{
	struct task_struct *g, *p;
	unsigned long flags;

	SEQ_printf(m,
	"\nrunnable tasks:\n"
	"            task   PID         tree-key  switches  prio"
	"     exec-runtime         sum-exec        sum-sleep\n"
	"------------------------------------------------------"
	"----------------------------------------------------\n");

	read_lock_irqsave(&tasklist_lock, flags);

	do_each_thread(g, p) {
		if (!p->se.on_rq || task_cpu(p) != rq_cpu)
			continue;

		print_task(m, rq, p);
	} while_each_thread(g, p);

	read_unlock_irqrestore(&tasklist_lock, flags);
}

#if defined(CONFIG_CGROUP_SCHED) && (defined(CONFIG_FAIR_GROUP_SCHED) || \
	defined(CONFIG_RT_GROUP_SCHED))
static void task_group_path(struct task_group *tg, char *buf, int buflen)
{
	/* may be NULL if the underlying cgroup isn't fully-created yet */
	if (!tg->css.cgroup) {
		buf[0] = '\0';
		return;
	}
	cgroup_path(tg->css.cgroup, buf, buflen);
}
#endif

void print_cfs_rq(struct seq_file *m, int cpu, struct cfs_rq *cfs_rq)
{
	s64 MIN_vruntime = -1, min_vruntime, max_vruntime = -1,
		spread, rq0_min_vruntime, spread0;
	struct rq *rq = cpu_rq(cpu);
	struct sched_entity *last;
	unsigned long flags;

#if defined(CONFIG_CGROUP_SCHED) && defined(CONFIG_FAIR_GROUP_SCHED)
	char path[128];
	struct task_group *tg = cfs_rq->tg;

	task_group_path(tg, path, sizeof(path));

	SEQ_printf(m, "\ncfs_rq[%d]:%s\n", cpu, path);
#else
	SEQ_printf(m, "\ncfs_rq[%d]:\n", cpu);
#endif
	SEQ_printf(m, "  .%-30s: %Ld.%06ld\n", "exec_clock",
			SPLIT_NS(cfs_rq->exec_clock));

	raw_spin_lock_irqsave(&rq->lock, flags);
	if (cfs_rq->rb_leftmost)
		MIN_vruntime = (__pick_next_entity(cfs_rq))->vruntime;
	last = __pick_last_entity(cfs_rq);
	if (last)
		max_vruntime = last->vruntime;
	min_vruntime = cfs_rq->min_vruntime;
	rq0_min_vruntime = cpu_rq(0)->cfs.min_vruntime;
	raw_spin_unlock_irqrestore(&rq->lock, flags);
	SEQ_printf(m, "  .%-30s: %Ld.%06ld\n", "MIN_vruntime",
			SPLIT_NS(MIN_vruntime));
	SEQ_printf(m, "  .%-30s: %Ld.%06ld\n", "min_vruntime",
			SPLIT_NS(min_vruntime));
	SEQ_printf(m, "  .%-30s: %Ld.%06ld\n", "max_vruntime",
			SPLIT_NS(max_vruntime));
	spread = max_vruntime - MIN_vruntime;
	SEQ_printf(m, "  .%-30s: %Ld.%06ld\n", "spread",
			SPLIT_NS(spread));
	spread0 = min_vruntime - rq0_min_vruntime;
	SEQ_printf(m, "  .%-30s: %Ld.%06ld\n", "spread0",
			SPLIT_NS(spread0));
	SEQ_printf(m, "  .%-30s: %ld\n", "nr_running", cfs_rq->nr_running);
	SEQ_printf(m, "  .%-30s: %ld\n", "load", cfs_rq->load.weight);

	SEQ_printf(m, "  .%-30s: %d\n", "nr_spread_over",
			cfs_rq->nr_spread_over);
#ifdef CONFIG_FAIR_GROUP_SCHED
#ifdef CONFIG_SMP
	SEQ_printf(m, "  .%-30s: %lu\n", "shares", cfs_rq->shares);
#endif
	print_cfs_group_stats(m, cpu, cfs_rq->tg);
#endif
}

void print_rt_rq(struct seq_file *m, int cpu, struct rt_rq *rt_rq)
{
#if defined(CONFIG_CGROUP_SCHED) && defined(CONFIG_RT_GROUP_SCHED)
	char path[128];
	struct task_group *tg = rt_rq->tg;

	task_group_path(tg, path, sizeof(path));

	SEQ_printf(m, "\nrt_rq[%d]:%s\n", cpu, path);
#else
	SEQ_printf(m, "\nrt_rq[%d]:\n", cpu);
#endif


#define P(x) \
	SEQ_printf(m, "  .%-30s: %Ld\n", #x, (long long)(rt_rq->x))
#define PN(x) \
	SEQ_printf(m, "  .%-30s: %Ld.%06ld\n", #x, SPLIT_NS(rt_rq->x))

	P(rt_nr_running);
	P(rt_throttled);
	PN(rt_time);
	PN(rt_runtime);

#undef PN
#undef P
}

static void print_cpu(struct seq_file *m, int cpu)
{
	struct rq *rq = cpu_rq(cpu);

#ifdef CONFIG_X86
	{
		unsigned int freq = cpu_khz ? : 1;

		SEQ_printf(m, "\ncpu#%d, %u.%03u MHz\n",
			   cpu, freq / 1000, (freq % 1000));
	}
#else
	SEQ_printf(m, "\ncpu#%d\n", cpu);
#endif

#define P(x) \
	SEQ_printf(m, "  .%-30s: %Ld\n", #x, (long long)(rq->x))
#define PN(x) \
	SEQ_printf(m, "  .%-30s: %Ld.%06ld\n", #x, SPLIT_NS(rq->x))

	P(nr_running);
	SEQ_printf(m, "  .%-30s: %lu\n", "load",
		   rq->load.weight);
	P(nr_switches);
	P(nr_load_updates);
	P(nr_uninterruptible);
	PN(next_balance);
	P(curr->pid);
	PN(clock);
	P(cpu_load[0]);
	P(cpu_load[1]);
	P(cpu_load[2]);
	P(cpu_load[3]);
	P(cpu_load[4]);
#undef P
#undef PN

#ifdef CONFIG_SCHEDSTATS
#define P(n) SEQ_printf(m, "  .%-30s: %d\n", #n, rq->n);
#define P64(n) SEQ_printf(m, "  .%-30s: %Ld\n", #n, rq->n);

	P(yld_count);

	P(sched_switch);
	P(sched_count);
	P(sched_goidle);
#ifdef CONFIG_SMP
	P64(avg_idle);
#endif

	P(ttwu_count);
	P(ttwu_local);

	P(bkl_count);

#undef P
#endif
	print_cfs_stats(m, cpu);
	print_rt_stats(m, cpu);

	print_rq(m, rq, cpu);
}

static const char *sched_tunable_scaling_names[] = {
	"none",
	"logaritmic",
	"linear"
};

static int sched_debug_show(struct seq_file *m, void *v)
{
	u64 now = ktime_to_ns(ktime_get());
	int cpu;

	SEQ_printf(m, "Sched Debug Version: v0.09, %s %.*s\n",
		init_utsname()->release,
		(int)strcspn(init_utsname()->version, " "),
		init_utsname()->version);

	SEQ_printf(m, "now at %Lu.%06ld msecs\n", SPLIT_NS(now));

#define P(x) \
	SEQ_printf(m, "  .%-40s: %Ld\n", #x, (long long)(x))
#define PN(x) \
	SEQ_printf(m, "  .%-40s: %Ld.%06ld\n", #x, SPLIT_NS(x))
	P(jiffies);
	PN(sysctl_sched_latency);
	PN(sysctl_sched_min_granularity);
	PN(sysctl_sched_wakeup_granularity);
	P(sysctl_sched_child_runs_first);
	P(sysctl_sched_features);
#undef PN
#undef P

	SEQ_printf(m, "  .%-40s: %d (%s)\n", "sysctl_sched_tunable_scaling",
		sysctl_sched_tunable_scaling,
		sched_tunable_scaling_names[sysctl_sched_tunable_scaling]);

	for_each_online_cpu(cpu)
		print_cpu(m, cpu);

	SEQ_printf(m, "\n");

	return 0;
}

static void sysrq_sched_debug_show(void)
{
	sched_debug_show(NULL, NULL);
}

static int sched_debug_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, sched_debug_show, NULL);
}

static const struct file_operations sched_debug_fops = {
	.open		= sched_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init init_sched_debug_procfs(void)
{
	struct proc_dir_entry *pe;

	pe = proc_create("sched_debug", 0444, NULL, &sched_debug_fops);
	if (!pe)
		return -ENOMEM;
	return 0;
}

__initcall(init_sched_debug_procfs);

void proc_sched_show_task(struct task_struct *p, struct seq_file *m)
{
	unsigned long nr_switches;

	SEQ_printf(m, "%s (%d, #threads: %d)\n", p->comm, p->pid,
						get_nr_threads(p));
	SEQ_printf(m,
		"---------------------------------------------------------\n");
#define __P(F) \
	SEQ_printf(m, "%-35s:%21Ld\n", #F, (long long)F)
#define P(F) \
	SEQ_printf(m, "%-35s:%21Ld\n", #F, (long long)p->F)
#define __PN(F) \
	SEQ_printf(m, "%-35s:%14Ld.%06ld\n", #F, SPLIT_NS((long long)F))
#define PN(F) \
	SEQ_printf(m, "%-35s:%14Ld.%06ld\n", #F, SPLIT_NS((long long)p->F))

	PN(se.exec_start);
	PN(se.vruntime);
	PN(se.sum_exec_runtime);

	nr_switches = p->nvcsw + p->nivcsw;

#ifdef CONFIG_SCHEDSTATS
	PN(se.statistics.wait_start);
	PN(se.statistics.sleep_start);
	PN(se.statistics.block_start);
	PN(se.statistics.sleep_max);
	PN(se.statistics.block_max);
	PN(se.statistics.exec_max);
	PN(se.statistics.slice_max);
	PN(se.statistics.wait_max);
	PN(se.statistics.wait_sum);
	P(se.statistics.wait_count);
	PN(se.statistics.iowait_sum);
	P(se.statistics.iowait_count);
	P(sched_info.bkl_count);
	P(se.nr_migrations);
	P(se.statistics.nr_migrations_cold);
	P(se.statistics.nr_failed_migrations_affine);
	P(se.statistics.nr_failed_migrations_running);
	P(se.statistics.nr_failed_migrations_hot);
	P(se.statistics.nr_forced_migrations);
	P(se.statistics.nr_wakeups);
	P(se.statistics.nr_wakeups_sync);
	P(se.statistics.nr_wakeups_migrate);
	P(se.statistics.nr_wakeups_local);
	P(se.statistics.nr_wakeups_remote);
	P(se.statistics.nr_wakeups_affine);
	P(se.statistics.nr_wakeups_affine_attempts);
	P(se.statistics.nr_wakeups_passive);
	P(se.statistics.nr_wakeups_idle);

	{
		u64 avg_atom, avg_per_cpu;

		avg_atom = p->se.sum_exec_runtime;
		if (nr_switches)
			do_div(avg_atom, nr_switches);
		else
			avg_atom = -1LL;

		avg_per_cpu = p->se.sum_exec_runtime;
		if (p->se.nr_migrations) {
			avg_per_cpu = div64_u64(avg_per_cpu,
						p->se.nr_migrations);
		} else {
			avg_per_cpu = -1LL;
		}

		__PN(avg_atom);
		__PN(avg_per_cpu);
	}
#endif
	__P(nr_switches);
	SEQ_printf(m, "%-35s:%21Ld\n",
		   "nr_voluntary_switches", (long long)p->nvcsw);
	SEQ_printf(m, "%-35s:%21Ld\n",
		   "nr_involuntary_switches", (long long)p->nivcsw);

	P(se.load.weight);
	P(policy);
	P(prio);
#undef PN
#undef __PN
#undef P
#undef __P

	{
		unsigned int this_cpu = raw_smp_processor_id();
		u64 t0, t1;

		t0 = cpu_clock(this_cpu);
		t1 = cpu_clock(this_cpu);
		SEQ_printf(m, "%-35s:%21Ld\n",
			   "clock-delta", (long long)(t1-t0));
	}
}

void proc_sched_set_task(struct task_struct *p)
{
#ifdef CONFIG_SCHEDSTATS
	memset(&p->se.statistics, 0, sizeof(p->se.statistics));
#endif
}
