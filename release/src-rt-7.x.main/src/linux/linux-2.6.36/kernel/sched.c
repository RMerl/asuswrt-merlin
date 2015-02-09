/* Modified by Broadcom Corp. Portions Copyright (c) Broadcom Corp, 2012. */
/*
 *  kernel/sched.c
 *
 *  Kernel scheduler and related syscalls
 *
 *  Copyright (C) 1991-2002  Linus Torvalds
 *
 *  1996-12-23  Modified by Dave Grothe to fix bugs in semaphores and
 *		make semaphores SMP safe
 *  1998-11-19	Implemented schedule_timeout() and related stuff
 *		by Andrea Arcangeli
 *  2002-01-04	New ultra-scalable O(1) scheduler by Ingo Molnar:
 *		hybrid priority-list and round-robin design with
 *		an array-switch method of distributing timeslices
 *		and per-CPU runqueues.  Cleanups and useful suggestions
 *		by Davide Libenzi, preemptible kernel bits by Robert Love.
 *  2003-09-03	Interactivity tuning by Con Kolivas.
 *  2004-04-02	Scheduler domains code by Nick Piggin
 *  2007-04-15  Work begun on replacing all interactivity tuning with a
 *              fair scheduling design by Con Kolivas.
 *  2007-05-05  Load balancing (smp-nice) and other improvements
 *              by Peter Williams
 *  2007-05-06  Interactivity improvements to CFS by Mike Galbraith
 *  2007-07-01  Group scheduling enhancements by Srivatsa Vaddagiri
 *  2007-11-29  RT balancing improvements by Steven Rostedt, Gregory Haskins,
 *              Thomas Gleixner, Mike Kravetz
 */

#include <linux/mm.h>
#include <linux/module.h>
#include <linux/nmi.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/highmem.h>
#include <linux/smp_lock.h>
#include <asm/mmu_context.h>
#include <linux/interrupt.h>
#include <linux/capability.h>
#include <linux/completion.h>
#include <linux/kernel_stat.h>
#include <linux/debug_locks.h>
#include <linux/perf_event.h>
#include <linux/security.h>
#include <linux/notifier.h>
#include <linux/profile.h>
#include <linux/freezer.h>
#include <linux/vmalloc.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/pid_namespace.h>
#include <linux/smp.h>
#include <linux/threads.h>
#include <linux/timer.h>
#include <linux/rcupdate.h>
#include <linux/cpu.h>
#include <linux/cpuset.h>
#include <linux/percpu.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/stop_machine.h>
#include <linux/sysctl.h>
#include <linux/syscalls.h>
#include <linux/times.h>
#include <linux/tsacct_kern.h>
#include <linux/kprobes.h>
#include <linux/delayacct.h>
#include <linux/unistd.h>
#include <linux/pagemap.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/debugfs.h>
#include <linux/ctype.h>
#include <linux/ftrace.h>
#include <linux/slab.h>

#include <asm/tlb.h>
#include <asm/irq_regs.h>

#include "sched_cpupri.h"
#include "workqueue_sched.h"

#define CREATE_TRACE_POINTS
#include <trace/events/sched.h>

#if defined(CONFIG_BUZZZ)
#include <asm/buzzz.h>
/* These global varaibles are needed to hold prev, next tasks to log context
 * switch as stack will be invalid after context_switch.
 * Also per-cpu macros are not needed as these variables are accessed
 * only inside pre-emption disabled code.
 */
#if defined(BUZZZ_KEVT_LVL) && (BUZZZ_KEVT_LVL >= 1)
struct task_struct *buzzz_prev[NR_CPUS];
struct task_struct *buzzz_next[NR_CPUS];
#endif	/* BUZZZ_KEVT_LVL */
#endif	/* CONFIG_BUZZZ */
/*
 * Convert user-nice values [ -20 ... 0 ... 19 ]
 * to static priority [ MAX_RT_PRIO..MAX_PRIO-1 ],
 * and back.
 */
#define NICE_TO_PRIO(nice)	(MAX_RT_PRIO + (nice) + 20)
#define PRIO_TO_NICE(prio)	((prio) - MAX_RT_PRIO - 20)
#define TASK_NICE(p)		PRIO_TO_NICE((p)->static_prio)

/*
 * 'User priority' is the nice value converted to something we
 * can work with better when scaling various scheduler parameters,
 * it's a [ 0 ... 39 ] range.
 */
#define USER_PRIO(p)		((p)-MAX_RT_PRIO)
#define TASK_USER_PRIO(p)	USER_PRIO((p)->static_prio)
#define MAX_USER_PRIO		(USER_PRIO(MAX_PRIO))

/*
 * Helpers for converting nanosecond timing to jiffy resolution
 */
#define NS_TO_JIFFIES(TIME)	((unsigned long)(TIME) / (NSEC_PER_SEC / HZ))

#define NICE_0_LOAD		SCHED_LOAD_SCALE
#define NICE_0_SHIFT		SCHED_LOAD_SHIFT

/*
 * These are the 'tuning knobs' of the scheduler:
 *
 * default timeslice is 100 msecs (used only for SCHED_RR tasks).
 * Timeslices get refilled after they expire.
 */
#define DEF_TIMESLICE		(100 * HZ / 1000)

/*
 * single value that denotes runtime == period, ie unlimited time.
 */
#define RUNTIME_INF	((u64)~0ULL)

static inline int rt_policy(int policy)
{
	if (unlikely(policy == SCHED_FIFO || policy == SCHED_RR))
		return 1;
	return 0;
}

static inline int task_has_rt_policy(struct task_struct *p)
{
	return rt_policy(p->policy);
}

/*
 * This is the priority-queue data structure of the RT scheduling class:
 */
struct rt_prio_array {
	DECLARE_BITMAP(bitmap, MAX_RT_PRIO+1); /* include 1 bit for delimiter */
	struct list_head queue[MAX_RT_PRIO];
};

struct rt_bandwidth {
	/* nests inside the rq lock: */
	raw_spinlock_t		rt_runtime_lock;
	ktime_t			rt_period;
	u64			rt_runtime;
	struct hrtimer		rt_period_timer;
};

static struct rt_bandwidth def_rt_bandwidth;

static int do_sched_rt_period_timer(struct rt_bandwidth *rt_b, int overrun);

static enum hrtimer_restart sched_rt_period_timer(struct hrtimer *timer)
{
	struct rt_bandwidth *rt_b =
		container_of(timer, struct rt_bandwidth, rt_period_timer);
	ktime_t now;
	int overrun;
	int idle = 0;

	for (;;) {
		now = hrtimer_cb_get_time(timer);
		overrun = hrtimer_forward(timer, now, rt_b->rt_period);

		if (!overrun)
			break;

		idle = do_sched_rt_period_timer(rt_b, overrun);
	}

	return idle ? HRTIMER_NORESTART : HRTIMER_RESTART;
}

static
void init_rt_bandwidth(struct rt_bandwidth *rt_b, u64 period, u64 runtime)
{
	rt_b->rt_period = ns_to_ktime(period);
	rt_b->rt_runtime = runtime;

	raw_spin_lock_init(&rt_b->rt_runtime_lock);

	hrtimer_init(&rt_b->rt_period_timer,
			CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	rt_b->rt_period_timer.function = sched_rt_period_timer;
}

static inline int rt_bandwidth_enabled(void)
{
	return sysctl_sched_rt_runtime >= 0;
}

static void start_rt_bandwidth(struct rt_bandwidth *rt_b)
{
	ktime_t now;

	if (!rt_bandwidth_enabled() || rt_b->rt_runtime == RUNTIME_INF)
		return;

	if (hrtimer_active(&rt_b->rt_period_timer))
		return;

	raw_spin_lock(&rt_b->rt_runtime_lock);
	for (;;) {
		unsigned long delta;
		ktime_t soft, hard;

		if (hrtimer_active(&rt_b->rt_period_timer))
			break;

		now = hrtimer_cb_get_time(&rt_b->rt_period_timer);
		hrtimer_forward(&rt_b->rt_period_timer, now, rt_b->rt_period);

		soft = hrtimer_get_softexpires(&rt_b->rt_period_timer);
		hard = hrtimer_get_expires(&rt_b->rt_period_timer);
		delta = ktime_to_ns(ktime_sub(hard, soft));
		__hrtimer_start_range_ns(&rt_b->rt_period_timer, soft, delta,
				HRTIMER_MODE_ABS_PINNED, 0);
	}
	raw_spin_unlock(&rt_b->rt_runtime_lock);
}

#ifdef CONFIG_RT_GROUP_SCHED
static void destroy_rt_bandwidth(struct rt_bandwidth *rt_b)
{
	hrtimer_cancel(&rt_b->rt_period_timer);
}
#endif

/*
 * sched_domains_mutex serializes calls to arch_init_sched_domains,
 * detach_destroy_domains and partition_sched_domains.
 */
static DEFINE_MUTEX(sched_domains_mutex);

#ifdef CONFIG_CGROUP_SCHED

#include <linux/cgroup.h>

struct cfs_rq;

static LIST_HEAD(task_groups);

/* task group related information */
struct task_group {
	struct cgroup_subsys_state css;

#ifdef CONFIG_FAIR_GROUP_SCHED
	/* schedulable entities of this group on each cpu */
	struct sched_entity **se;
	/* runqueue "owned" by this group on each cpu */
	struct cfs_rq **cfs_rq;
	unsigned long shares;
#endif

#ifdef CONFIG_RT_GROUP_SCHED
	struct sched_rt_entity **rt_se;
	struct rt_rq **rt_rq;

	struct rt_bandwidth rt_bandwidth;
#endif

	struct rcu_head rcu;
	struct list_head list;

	struct task_group *parent;
	struct list_head siblings;
	struct list_head children;
};

#define root_task_group init_task_group

/* task_group_lock serializes add/remove of task groups and also changes to
 * a task group's cpu shares.
 */
static DEFINE_SPINLOCK(task_group_lock);

#ifdef CONFIG_FAIR_GROUP_SCHED

#ifdef CONFIG_SMP
static int root_task_group_empty(void)
{
	return list_empty(&root_task_group.children);
}
#endif

# define INIT_TASK_GROUP_LOAD	NICE_0_LOAD

/*
 * A weight of 0 or 1 can cause arithmetics problems.
 * A weight of a cfs_rq is the sum of weights of which entities
 * are queued on this cfs_rq, so a weight of a entity should not be
 * too large, so as the shares value of a task group.
 * (The default weight is 1024 - so there's no practical
 *  limitation from this.)
 */
#define MIN_SHARES	2
#define MAX_SHARES	(1UL << 18)

static int init_task_group_load = INIT_TASK_GROUP_LOAD;
#endif

/* Default task group.
 *	Every task in system belong to this group at bootup.
 */
struct task_group init_task_group;

#endif	/* CONFIG_CGROUP_SCHED */

/* CFS-related fields in a runqueue */
struct cfs_rq {
	struct load_weight load;
	unsigned long nr_running;

	u64 exec_clock;
	u64 min_vruntime;

	struct rb_root tasks_timeline;
	struct rb_node *rb_leftmost;

	struct list_head tasks;
	struct list_head *balance_iterator;

	/*
	 * 'curr' points to currently running entity on this cfs_rq.
	 * It is set to NULL otherwise (i.e when none are currently running).
	 */
	struct sched_entity *curr, *next, *last;

	unsigned int nr_spread_over;

#ifdef CONFIG_FAIR_GROUP_SCHED
	struct rq *rq;	/* cpu runqueue to which this cfs_rq is attached */

	/*
	 * leaf cfs_rqs are those that hold tasks (lowest schedulable entity in
	 * a hierarchy). Non-leaf lrqs hold other higher schedulable entities
	 * (like users, containers etc.)
	 *
	 * leaf_cfs_rq_list ties together list of leaf cfs_rq's in a cpu. This
	 * list is used during load balance.
	 */
	struct list_head leaf_cfs_rq_list;
	struct task_group *tg;	/* group that "owns" this runqueue */

#ifdef CONFIG_SMP
	/*
	 * the part of load.weight contributed by tasks
	 */
	unsigned long task_weight;

	/*
	 *   h_load = weight * f(tg)
	 *
	 * Where f(tg) is the recursive weight fraction assigned to
	 * this group.
	 */
	unsigned long h_load;

	/*
	 * this cpu's part of tg->shares
	 */
	unsigned long shares;

	/*
	 * load.weight at the time we set shares
	 */
	unsigned long rq_weight;
#endif
#endif
};

/* Real-Time classes' related field in a runqueue: */
struct rt_rq {
	struct rt_prio_array active;
	unsigned long rt_nr_running;
#if defined CONFIG_SMP || defined CONFIG_RT_GROUP_SCHED
	struct {
		int curr; /* highest queued rt task prio */
#ifdef CONFIG_SMP
		int next; /* next highest */
#endif
	} highest_prio;
#endif
#ifdef CONFIG_SMP
	unsigned long rt_nr_migratory;
	unsigned long rt_nr_total;
	int overloaded;
	struct plist_head pushable_tasks;
#endif
	int rt_throttled;
	u64 rt_time;
	u64 rt_runtime;
	/* Nests inside the rq lock: */
	raw_spinlock_t rt_runtime_lock;

#ifdef CONFIG_RT_GROUP_SCHED
	unsigned long rt_nr_boosted;

	struct rq *rq;
	struct list_head leaf_rt_rq_list;
	struct task_group *tg;
#endif
};

#ifdef CONFIG_SMP

/*
 * We add the notion of a root-domain which will be used to define per-domain
 * variables. Each exclusive cpuset essentially defines an island domain by
 * fully partitioning the member cpus from any other cpuset. Whenever a new
 * exclusive cpuset is created, we also create and attach a new root-domain
 * object.
 *
 */
struct root_domain {
	atomic_t refcount;
	cpumask_var_t span;
	cpumask_var_t online;

	/*
	 * The "RT overload" flag: it gets set if a CPU has more than
	 * one runnable RT task.
	 */
	cpumask_var_t rto_mask;
	atomic_t rto_count;
#ifdef CONFIG_SMP
	struct cpupri cpupri;
#endif
};

/*
 * By default the system creates a single root-domain with all cpus as
 * members (mimicking the global state we have today).
 */
static struct root_domain def_root_domain;

#endif

/*
 * This is the main, per-CPU runqueue data structure.
 *
 * Locking rule: those places that want to lock multiple runqueues
 * (such as the load balancing or the thread migration code), lock
 * acquire operations must be ordered by ascending &runqueue.
 */
struct rq {
	/* runqueue lock: */
	raw_spinlock_t lock;

	/*
	 * nr_running and cpu_load should be in the same cacheline because
	 * remote CPUs use both these fields when doing load calculation.
	 */
	unsigned long nr_running;
	#define CPU_LOAD_IDX_MAX 5
	unsigned long cpu_load[CPU_LOAD_IDX_MAX];
	unsigned long last_load_update_tick;
#ifdef CONFIG_NO_HZ
	u64 nohz_stamp;
	unsigned char nohz_balance_kick;
#endif
	unsigned int skip_clock_update;

	/* capture load from *all* tasks on this cpu: */
	struct load_weight load;
	unsigned long nr_load_updates;
	u64 nr_switches;

	struct cfs_rq cfs;
	struct rt_rq rt;

#ifdef CONFIG_FAIR_GROUP_SCHED
	/* list of leaf cfs_rq on this cpu: */
	struct list_head leaf_cfs_rq_list;
#endif
#ifdef CONFIG_RT_GROUP_SCHED
	struct list_head leaf_rt_rq_list;
#endif

	/*
	 * This is part of a global counter where only the total sum
	 * over all CPUs matters. A task can increase this counter on
	 * one CPU and if it got migrated afterwards it may decrease
	 * it on another CPU. Always updated under the runqueue lock:
	 */
	unsigned long nr_uninterruptible;

	struct task_struct *curr, *idle;
	unsigned long next_balance;
	struct mm_struct *prev_mm;

	u64 clock;

	atomic_t nr_iowait;

#ifdef CONFIG_SMP
	struct root_domain *rd;
	struct sched_domain *sd;

	unsigned long cpu_power;

	unsigned char idle_at_tick;
	/* For active balancing */
	int post_schedule;
	int active_balance;
	int push_cpu;
	struct cpu_stop_work active_balance_work;
	/* cpu of this runqueue: */
	int cpu;
	int online;

	unsigned long avg_load_per_task;

	u64 rt_avg;
	u64 age_stamp;
	u64 idle_stamp;
	u64 avg_idle;
#endif

	/* calc_load related fields */
	unsigned long calc_load_update;
	long calc_load_active;

#ifdef CONFIG_SCHED_HRTICK
#ifdef CONFIG_SMP
	int hrtick_csd_pending;
	struct call_single_data hrtick_csd;
#endif
	struct hrtimer hrtick_timer;
#endif

#ifdef CONFIG_SCHEDSTATS
	/* latency stats */
	struct sched_info rq_sched_info;
	unsigned long long rq_cpu_time;
	/* could above be rq->cfs_rq.exec_clock + rq->rt_rq.rt_runtime ? */

	/* sys_sched_yield() stats */
	unsigned int yld_count;

	/* schedule() stats */
	unsigned int sched_switch;
	unsigned int sched_count;
	unsigned int sched_goidle;

	/* try_to_wake_up() stats */
	unsigned int ttwu_count;
	unsigned int ttwu_local;

	/* BKL stats */
	unsigned int bkl_count;
#endif
};

static DEFINE_PER_CPU_SHARED_ALIGNED(struct rq, runqueues);

static inline
void check_preempt_curr(struct rq *rq, struct task_struct *p, int flags)
{
	rq->curr->sched_class->check_preempt_curr(rq, p, flags);

	/*
	 * A queue event has occurred, and we're going to schedule.  In
	 * this case, we can save a useless back to back clock update.
	 */
	if (rq->curr->se.on_rq && test_tsk_need_resched(rq->curr))
		rq->skip_clock_update = 1;
}

static inline int cpu_of(struct rq *rq)
{
#ifdef CONFIG_SMP
	return rq->cpu;
#else
	return 0;
#endif
}

#define rcu_dereference_check_sched_domain(p) \
	rcu_dereference_check((p), \
			      rcu_read_lock_sched_held() || \
			      lockdep_is_held(&sched_domains_mutex))

/*
 * The domain tree (rq->sd) is protected by RCU's quiescent state transition.
 * See detach_destroy_domains: synchronize_sched for details.
 *
 * The domain tree of any CPU may only be accessed from within
 * preempt-disabled sections.
 */
#define for_each_domain(cpu, __sd) \
	for (__sd = rcu_dereference_check_sched_domain(cpu_rq(cpu)->sd); __sd; __sd = __sd->parent)

#define cpu_rq(cpu)		(&per_cpu(runqueues, (cpu)))
#define this_rq()		(&__get_cpu_var(runqueues))
#define task_rq(p)		cpu_rq(task_cpu(p))
#define cpu_curr(cpu)		(cpu_rq(cpu)->curr)
#define raw_rq()		(&__raw_get_cpu_var(runqueues))

#ifdef CONFIG_CGROUP_SCHED

/*
 * Return the group to which this tasks belongs.
 *
 * We use task_subsys_state_check() and extend the RCU verification
 * with lockdep_is_held(&task_rq(p)->lock) because cpu_cgroup_attach()
 * holds that lock for each task it moves into the cgroup. Therefore
 * by holding that lock, we pin the task to the current cgroup.
 */
static inline struct task_group *task_group(struct task_struct *p)
{
	struct cgroup_subsys_state *css;

	css = task_subsys_state_check(p, cpu_cgroup_subsys_id,
			lockdep_is_held(&task_rq(p)->lock));
	return container_of(css, struct task_group, css);
}

/* Change a task's cfs_rq and parent entity if it moves across CPUs/groups */
static inline void set_task_rq(struct task_struct *p, unsigned int cpu)
{
#ifdef CONFIG_FAIR_GROUP_SCHED
	p->se.cfs_rq = task_group(p)->cfs_rq[cpu];
	p->se.parent = task_group(p)->se[cpu];
#endif

#ifdef CONFIG_RT_GROUP_SCHED
	p->rt.rt_rq  = task_group(p)->rt_rq[cpu];
	p->rt.parent = task_group(p)->rt_se[cpu];
#endif
}

#else /* CONFIG_CGROUP_SCHED */

static inline void set_task_rq(struct task_struct *p, unsigned int cpu) { }
static inline struct task_group *task_group(struct task_struct *p)
{
	return NULL;
}

#endif /* CONFIG_CGROUP_SCHED */

inline void update_rq_clock(struct rq *rq)
{
	if (!rq->skip_clock_update)
		rq->clock = sched_clock_cpu(cpu_of(rq));
}

/*
 * Tunables that become constants when CONFIG_SCHED_DEBUG is off:
 */
#ifdef CONFIG_SCHED_DEBUG
# define const_debug __read_mostly
#else
# define const_debug static const
#endif

/**
 * runqueue_is_locked
 * @cpu: the processor in question.
 *
 * Returns true if the current cpu runqueue is locked.
 * This interface allows printk to be called with the runqueue lock
 * held and know whether or not it is OK to wake up the klogd.
 */
int runqueue_is_locked(int cpu)
{
	return raw_spin_is_locked(&cpu_rq(cpu)->lock);
}

/*
 * Debugging: various feature bits
 */

#define SCHED_FEAT(name, enabled)	\
	__SCHED_FEAT_##name ,

enum {
#include "sched_features.h"
};

#undef SCHED_FEAT

#define SCHED_FEAT(name, enabled)	\
	(1UL << __SCHED_FEAT_##name) * enabled |

const_debug unsigned int sysctl_sched_features =
#include "sched_features.h"
	0;

#undef SCHED_FEAT

#ifdef CONFIG_SCHED_DEBUG
#define SCHED_FEAT(name, enabled)	\
	#name ,

static __read_mostly char *sched_feat_names[] = {
#include "sched_features.h"
	NULL
};

#undef SCHED_FEAT

static int sched_feat_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; sched_feat_names[i]; i++) {
		if (!(sysctl_sched_features & (1UL << i)))
			seq_puts(m, "NO_");
		seq_printf(m, "%s ", sched_feat_names[i]);
	}
	seq_puts(m, "\n");

	return 0;
}

static ssize_t
sched_feat_write(struct file *filp, const char __user *ubuf,
		size_t cnt, loff_t *ppos)
{
	char buf[64];
	char *cmp;
	int neg = 0;
	int i;

	if (cnt > 63)
		cnt = 63;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;
	cmp = strstrip(buf);

	if (strncmp(buf, "NO_", 3) == 0) {
		neg = 1;
		cmp += 3;
	}

	for (i = 0; sched_feat_names[i]; i++) {
		if (strcmp(cmp, sched_feat_names[i]) == 0) {
			if (neg)
				sysctl_sched_features &= ~(1UL << i);
			else
				sysctl_sched_features |= (1UL << i);
			break;
		}
	}

	if (!sched_feat_names[i])
		return -EINVAL;

	*ppos += cnt;

	return cnt;
}

static int sched_feat_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, sched_feat_show, NULL);
}

static const struct file_operations sched_feat_fops = {
	.open		= sched_feat_open,
	.write		= sched_feat_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static __init int sched_init_debug(void)
{
	debugfs_create_file("sched_features", 0644, NULL, NULL,
			&sched_feat_fops);

	return 0;
}
late_initcall(sched_init_debug);

#endif

#define sched_feat(x) (sysctl_sched_features & (1UL << __SCHED_FEAT_##x))

/*
 * Number of tasks to iterate in a single balance run.
 * Limited because this is done with IRQs disabled.
 */
const_debug unsigned int sysctl_sched_nr_migrate = 32;

/*
 * ratelimit for updating the group shares.
 * default: 0.25ms
 */
unsigned int sysctl_sched_shares_ratelimit = 250000;
unsigned int normalized_sysctl_sched_shares_ratelimit = 250000;

/*
 * Inject some fuzzyness into changing the per-cpu group shares
 * this avoids remote rq-locks at the expense of fairness.
 * default: 4
 */
unsigned int sysctl_sched_shares_thresh = 4;

/*
 * period over which we average the RT time consumption, measured
 * in ms.
 *
 * default: 1s
 */
const_debug unsigned int sysctl_sched_time_avg = MSEC_PER_SEC;

/*
 * period over which we measure -rt task cpu usage in us.
 * default: 1s
 */
unsigned int sysctl_sched_rt_period = 1000000;

static __read_mostly int scheduler_running;

/*
 * part of the period that we allow rt tasks to run in us.
 * default: 0.95s
 */
int sysctl_sched_rt_runtime = 950000;

static inline u64 global_rt_period(void)
{
	return (u64)sysctl_sched_rt_period * NSEC_PER_USEC;
}

static inline u64 global_rt_runtime(void)
{
	if (sysctl_sched_rt_runtime < 0)
		return RUNTIME_INF;

	return (u64)sysctl_sched_rt_runtime * NSEC_PER_USEC;
}

#ifndef prepare_arch_switch
# define prepare_arch_switch(next)	do { } while (0)
#endif
#ifndef finish_arch_switch
# define finish_arch_switch(prev)	do { } while (0)
#endif

static inline int task_current(struct rq *rq, struct task_struct *p)
{
	return rq->curr == p;
}

#ifndef __ARCH_WANT_UNLOCKED_CTXSW
static inline int task_running(struct rq *rq, struct task_struct *p)
{
	return task_current(rq, p);
}

static inline void prepare_lock_switch(struct rq *rq, struct task_struct *next)
{
}

static inline void finish_lock_switch(struct rq *rq, struct task_struct *prev)
{
#ifdef CONFIG_DEBUG_SPINLOCK
	/* this is a valid case when another task releases the spinlock */
	rq->lock.owner = current;
#endif
	/*
	 * If we are tracking spinlock dependencies then we have to
	 * fix up the runqueue lock - which gets 'carried over' from
	 * prev into current:
	 */
	spin_acquire(&rq->lock.dep_map, 0, 0, _THIS_IP_);

	raw_spin_unlock_irq(&rq->lock);
}

#else /* __ARCH_WANT_UNLOCKED_CTXSW */
static inline int task_running(struct rq *rq, struct task_struct *p)
{
#ifdef CONFIG_SMP
	return p->oncpu;
#else
	return task_current(rq, p);
#endif
}

static inline void prepare_lock_switch(struct rq *rq, struct task_struct *next)
{
#ifdef CONFIG_SMP
	/*
	 * We can optimise this out completely for !SMP, because the
	 * SMP rebalancing from interrupt is the only thing that cares
	 * here.
	 */
	next->oncpu = 1;
#endif
#ifdef __ARCH_WANT_INTERRUPTS_ON_CTXSW
	raw_spin_unlock_irq(&rq->lock);
#else
	raw_spin_unlock(&rq->lock);
#endif
}

static inline void finish_lock_switch(struct rq *rq, struct task_struct *prev)
{
#ifdef CONFIG_SMP
	/*
	 * After ->oncpu is cleared, the task can be moved to a different CPU.
	 * We must ensure this doesn't happen until the switch is completely
	 * finished.
	 */
	smp_wmb();
	prev->oncpu = 0;
#endif
#ifndef __ARCH_WANT_INTERRUPTS_ON_CTXSW
	local_irq_enable();
#endif
}
#endif /* __ARCH_WANT_UNLOCKED_CTXSW */

/*
 * Check whether the task is waking, we use this to synchronize ->cpus_allowed
 * against ttwu().
 */
static inline int task_is_waking(struct task_struct *p)
{
	return unlikely(p->state == TASK_WAKING);
}

/*
 * __task_rq_lock - lock the runqueue a given task resides on.
 * Must be called interrupts disabled.
 */
static inline struct rq *__task_rq_lock(struct task_struct *p)
	__acquires(rq->lock)
{
	struct rq *rq;

	for (;;) {
		rq = task_rq(p);
		raw_spin_lock(&rq->lock);
		if (likely(rq == task_rq(p)))
			return rq;
		raw_spin_unlock(&rq->lock);
	}
}

/*
 * task_rq_lock - lock the runqueue a given task resides on and disable
 * interrupts. Note the ordering: we can safely lookup the task_rq without
 * explicitly disabling preemption.
 */
static struct rq *task_rq_lock(struct task_struct *p, unsigned long *flags)
	__acquires(rq->lock)
{
	struct rq *rq;

	for (;;) {
		local_irq_save(*flags);
		rq = task_rq(p);
		raw_spin_lock(&rq->lock);
		if (likely(rq == task_rq(p)))
			return rq;
		raw_spin_unlock_irqrestore(&rq->lock, *flags);
	}
}

static void __task_rq_unlock(struct rq *rq)
	__releases(rq->lock)
{
	raw_spin_unlock(&rq->lock);
}

static inline void task_rq_unlock(struct rq *rq, unsigned long *flags)
	__releases(rq->lock)
{
	raw_spin_unlock_irqrestore(&rq->lock, *flags);
}

/*
 * this_rq_lock - lock this runqueue and disable interrupts.
 */
static struct rq *this_rq_lock(void)
	__acquires(rq->lock)
{
	struct rq *rq;

	local_irq_disable();
	rq = this_rq();
	raw_spin_lock(&rq->lock);

	return rq;
}

#ifdef CONFIG_SCHED_HRTICK
/*
 * Use HR-timers to deliver accurate preemption points.
 *
 * Its all a bit involved since we cannot program an hrt while holding the
 * rq->lock. So what we do is store a state in in rq->hrtick_* and ask for a
 * reschedule event.
 *
 * When we get rescheduled we reprogram the hrtick_timer outside of the
 * rq->lock.
 */

/*
 * Use hrtick when:
 *  - enabled by features
 *  - hrtimer is actually high res
 */
static inline int hrtick_enabled(struct rq *rq)
{
	if (!sched_feat(HRTICK))
		return 0;
	if (!cpu_active(cpu_of(rq)))
		return 0;
	return hrtimer_is_hres_active(&rq->hrtick_timer);
}

static void hrtick_clear(struct rq *rq)
{
	if (hrtimer_active(&rq->hrtick_timer))
		hrtimer_cancel(&rq->hrtick_timer);
}

/*
 * High-resolution timer tick.
 * Runs from hardirq context with interrupts disabled.
 */
static enum hrtimer_restart hrtick(struct hrtimer *timer)
{
	struct rq *rq = container_of(timer, struct rq, hrtick_timer);

	WARN_ON_ONCE(cpu_of(rq) != smp_processor_id());

#if defined(BUZZZ_KEVT_LVL) && (BUZZZ_KEVT_LVL >= 2)
	buzzz_kevt_log0(BUZZZ_KEVT_ID_SCHED_HRTICK);
#endif	/* BUZZZ_KEVT_LVL */

	raw_spin_lock(&rq->lock);
	update_rq_clock(rq);
	rq->curr->sched_class->task_tick(rq, rq->curr, 1);
	raw_spin_unlock(&rq->lock);

	return HRTIMER_NORESTART;
}

#ifdef CONFIG_SMP
/*
 * called from hardirq (IPI) context
 */
static void __hrtick_start(void *arg)
{
	struct rq *rq = arg;

	raw_spin_lock(&rq->lock);
	hrtimer_restart(&rq->hrtick_timer);
	rq->hrtick_csd_pending = 0;
	raw_spin_unlock(&rq->lock);
}

/*
 * Called to set the hrtick timer state.
 *
 * called with rq->lock held and irqs disabled
 */
static void hrtick_start(struct rq *rq, u64 delay)
{
	struct hrtimer *timer = &rq->hrtick_timer;
	ktime_t time = ktime_add_ns(timer->base->get_time(), delay);

	hrtimer_set_expires(timer, time);

	if (rq == this_rq()) {
		hrtimer_restart(timer);
	} else if (!rq->hrtick_csd_pending) {
		__smp_call_function_single(cpu_of(rq), &rq->hrtick_csd, 0);
		rq->hrtick_csd_pending = 1;
	}
}

static int
hotplug_hrtick(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
	int cpu = (int)(long)hcpu;

	switch (action) {
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		hrtick_clear(cpu_rq(cpu));
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

static __init void init_hrtick(void)
{
	hotcpu_notifier(hotplug_hrtick, 0);
}
#else
/*
 * Called to set the hrtick timer state.
 *
 * called with rq->lock held and irqs disabled
 */
static void hrtick_start(struct rq *rq, u64 delay)
{
	__hrtimer_start_range_ns(&rq->hrtick_timer, ns_to_ktime(delay), 0,
			HRTIMER_MODE_REL_PINNED, 0);
}

static inline void init_hrtick(void)
{
}
#endif /* CONFIG_SMP */

static void init_rq_hrtick(struct rq *rq)
{
#ifdef CONFIG_SMP
	rq->hrtick_csd_pending = 0;

	rq->hrtick_csd.flags = 0;
	rq->hrtick_csd.func = __hrtick_start;
	rq->hrtick_csd.info = rq;
#endif

	hrtimer_init(&rq->hrtick_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	rq->hrtick_timer.function = hrtick;
}
#else	/* CONFIG_SCHED_HRTICK */
static inline void hrtick_clear(struct rq *rq)
{
}

static inline void init_rq_hrtick(struct rq *rq)
{
}

static inline void init_hrtick(void)
{
}
#endif	/* CONFIG_SCHED_HRTICK */

/*
 * resched_task - mark a task 'to be rescheduled now'.
 *
 * On UP this means the setting of the need_resched flag, on SMP it
 * might also involve a cross-CPU call to trigger the scheduler on
 * the target CPU.
 */
#ifdef CONFIG_SMP

#ifndef tsk_is_polling
#define tsk_is_polling(t) test_tsk_thread_flag(t, TIF_POLLING_NRFLAG)
#endif

static void resched_task(struct task_struct *p)
{
	int cpu;

	assert_raw_spin_locked(&task_rq(p)->lock);

	if (test_tsk_need_resched(p))
		return;

	set_tsk_need_resched(p);

	cpu = task_cpu(p);
	if (cpu == smp_processor_id())
		return;

	/* NEED_RESCHED must be visible before we test polling */
	smp_mb();
	if (!tsk_is_polling(p))
		smp_send_reschedule(cpu);
}

static void resched_cpu(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long flags;

	if (!raw_spin_trylock_irqsave(&rq->lock, flags))
		return;
	resched_task(cpu_curr(cpu));
	raw_spin_unlock_irqrestore(&rq->lock, flags);
}

#ifdef CONFIG_NO_HZ
/*
 * In the semi idle case, use the nearest busy cpu for migrating timers
 * from an idle cpu.  This is good for power-savings.
 *
 * We don't do similar optimization for completely idle system, as
 * selecting an idle cpu will add more delays to the timers than intended
 * (as that cpu's timer base may not be uptodate wrt jiffies etc).
 */
int get_nohz_timer_target(void)
{
	int cpu = smp_processor_id();
	int i;
	struct sched_domain *sd;

	for_each_domain(cpu, sd) {
		for_each_cpu(i, sched_domain_span(sd))
			if (!idle_cpu(i))
				return i;
	}
	return cpu;
}
/*
 * When add_timer_on() enqueues a timer into the timer wheel of an
 * idle CPU then this timer might expire before the next timer event
 * which is scheduled to wake up that CPU. In case of a completely
 * idle system the next event might even be infinite time into the
 * future. wake_up_idle_cpu() ensures that the CPU is woken up and
 * leaves the inner idle loop so the newly added timer is taken into
 * account when the CPU goes back to idle and evaluates the timer
 * wheel for the next timer event.
 */
void wake_up_idle_cpu(int cpu)
{
	struct rq *rq = cpu_rq(cpu);

	if (cpu == smp_processor_id())
		return;

	/*
	 * This is safe, as this function is called with the timer
	 * wheel base lock of (cpu) held. When the CPU is on the way
	 * to idle and has not yet set rq->curr to idle then it will
	 * be serialized on the timer wheel base lock and take the new
	 * timer into account automatically.
	 */
	if (rq->curr != rq->idle)
		return;

	/*
	 * We can set TIF_RESCHED on the idle task of the other CPU
	 * lockless. The worst case is that the other CPU runs the
	 * idle task through an additional NOOP schedule()
	 */
	set_tsk_need_resched(rq->idle);

	/* NEED_RESCHED must be visible before we test polling */
	smp_mb();
	if (!tsk_is_polling(rq->idle))
		smp_send_reschedule(cpu);
}

#endif /* CONFIG_NO_HZ */

static u64 sched_avg_period(void)
{
	return (u64)sysctl_sched_time_avg * NSEC_PER_MSEC / 2;
}

static void sched_avg_update(struct rq *rq)
{
	s64 period = sched_avg_period();

	while ((s64)(rq->clock - rq->age_stamp) > period) {
		/*
		 * Inline assembly required to prevent the compiler
		 * optimising this loop into a divmod call.
		 * See __iter_div_u64_rem() for another example of this.
		 */
		asm("" : "+rm" (rq->age_stamp));
		rq->age_stamp += period;
		rq->rt_avg /= 2;
	}
}

static void sched_rt_avg_update(struct rq *rq, u64 rt_delta)
{
	rq->rt_avg += rt_delta;
	sched_avg_update(rq);
}

#else /* !CONFIG_SMP */
static void resched_task(struct task_struct *p)
{
	assert_raw_spin_locked(&task_rq(p)->lock);
	set_tsk_need_resched(p);
}

static void sched_rt_avg_update(struct rq *rq, u64 rt_delta)
{
}

static void sched_avg_update(struct rq *rq)
{
}
#endif /* CONFIG_SMP */

#if BITS_PER_LONG == 32
# define WMULT_CONST	(~0UL)
#else
# define WMULT_CONST	(1UL << 32)
#endif

#define WMULT_SHIFT	32

/*
 * Shift right and round:
 */
#define SRR(x, y) (((x) + (1UL << ((y) - 1))) >> (y))

/*
 * delta *= weight / lw
 */
static unsigned long
calc_delta_mine(unsigned long delta_exec, unsigned long weight,
		struct load_weight *lw)
{
	u64 tmp;

	if (!lw->inv_weight) {
		if (BITS_PER_LONG > 32 && unlikely(lw->weight >= WMULT_CONST))
			lw->inv_weight = 1;
		else
			lw->inv_weight = 1 + (WMULT_CONST-lw->weight/2)
				/ (lw->weight+1);
	}

	tmp = (u64)delta_exec * weight;
	/*
	 * Check whether we'd overflow the 64-bit multiplication:
	 */
	if (unlikely(tmp > WMULT_CONST))
		tmp = SRR(SRR(tmp, WMULT_SHIFT/2) * lw->inv_weight,
			WMULT_SHIFT/2);
	else
		tmp = SRR(tmp * lw->inv_weight, WMULT_SHIFT);

	return (unsigned long)min(tmp, (u64)(unsigned long)LONG_MAX);
}

static inline void update_load_add(struct load_weight *lw, unsigned long inc)
{
	lw->weight += inc;
	lw->inv_weight = 0;
}

static inline void update_load_sub(struct load_weight *lw, unsigned long dec)
{
	lw->weight -= dec;
	lw->inv_weight = 0;
}

/*
 * To aid in avoiding the subversion of "niceness" due to uneven distribution
 * of tasks with abnormal "nice" values across CPUs the contribution that
 * each task makes to its run queue's load is weighted according to its
 * scheduling class and "nice" value. For SCHED_NORMAL tasks this is just a
 * scaled version of the new time slice allocation that they receive on time
 * slice expiry etc.
 */

#define WEIGHT_IDLEPRIO                3
#define WMULT_IDLEPRIO         1431655765

/*
 * Nice levels are multiplicative, with a gentle 10% change for every
 * nice level changed. I.e. when a CPU-bound task goes from nice 0 to
 * nice 1, it will get ~10% less CPU time than another CPU-bound task
 * that remained on nice 0.
 *
 * The "10% effect" is relative and cumulative: from _any_ nice level,
 * if you go up 1 level, it's -10% CPU usage, if you go down 1 level
 * it's +10% CPU usage. (to achieve that we use a multiplier of 1.25.
 * If a task goes up by ~10% and another task goes down by ~10% then
 * the relative distance between them is ~25%.)
 */
static const int prio_to_weight[40] = {
 /* -20 */     88761,     71755,     56483,     46273,     36291,
 /* -15 */     29154,     23254,     18705,     14949,     11916,
 /* -10 */      9548,      7620,      6100,      4904,      3906,
 /*  -5 */      3121,      2501,      1991,      1586,      1277,
 /*   0 */      1024,       820,       655,       526,       423,
 /*   5 */       335,       272,       215,       172,       137,
 /*  10 */       110,        87,        70,        56,        45,
 /*  15 */        36,        29,        23,        18,        15,
};

/*
 * Inverse (2^32/x) values of the prio_to_weight[] array, precalculated.
 *
 * In cases where the weight does not change often, we can use the
 * precalculated inverse to speed up arithmetics by turning divisions
 * into multiplications:
 */
static const u32 prio_to_wmult[40] = {
 /* -20 */     48388,     59856,     76040,     92818,    118348,
 /* -15 */    147320,    184698,    229616,    287308,    360437,
 /* -10 */    449829,    563644,    704093,    875809,   1099582,
 /*  -5 */   1376151,   1717300,   2157191,   2708050,   3363326,
 /*   0 */   4194304,   5237765,   6557202,   8165337,  10153587,
 /*   5 */  12820798,  15790321,  19976592,  24970740,  31350126,
 /*  10 */  39045157,  49367440,  61356676,  76695844,  95443717,
 /*  15 */ 119304647, 148102320, 186737708, 238609294, 286331153,
};

/* Time spent by the tasks of the cpu accounting group executing in ... */
enum cpuacct_stat_index {
	CPUACCT_STAT_USER,	/* ... user mode */
	CPUACCT_STAT_SYSTEM,	/* ... kernel mode */

	CPUACCT_STAT_NSTATS,
};

#ifdef CONFIG_CGROUP_CPUACCT
static void cpuacct_charge(struct task_struct *tsk, u64 cputime);
static void cpuacct_update_stats(struct task_struct *tsk,
		enum cpuacct_stat_index idx, cputime_t val);
#else
static inline void cpuacct_charge(struct task_struct *tsk, u64 cputime) {}
static inline void cpuacct_update_stats(struct task_struct *tsk,
		enum cpuacct_stat_index idx, cputime_t val) {}
#endif

static inline void inc_cpu_load(struct rq *rq, unsigned long load)
{
	update_load_add(&rq->load, load);
}

static inline void dec_cpu_load(struct rq *rq, unsigned long load)
{
	update_load_sub(&rq->load, load);
}

#if (defined(CONFIG_SMP) && defined(CONFIG_FAIR_GROUP_SCHED)) || \
	defined(CONFIG_RT_GROUP_SCHED)
typedef int (*tg_visitor)(struct task_group *, void *);

/*
 * Iterate the full tree, calling @down when first entering a node and @up when
 * leaving it for the final time.
 */
static int walk_tg_tree(tg_visitor down, tg_visitor up, void *data)
{
	struct task_group *parent, *child;
	int ret;

	rcu_read_lock();
	parent = &root_task_group;
down:
	ret = (*down)(parent, data);
	if (ret)
		goto out_unlock;
	list_for_each_entry_rcu(child, &parent->children, siblings) {
		parent = child;
		goto down;

up:
		continue;
	}
	ret = (*up)(parent, data);
	if (ret)
		goto out_unlock;

	child = parent;
	parent = parent->parent;
	if (parent)
		goto up;
out_unlock:
	rcu_read_unlock();

	return ret;
}

static int tg_nop(struct task_group *tg, void *data)
{
	return 0;
}
#endif

#ifdef CONFIG_SMP
/* Used instead of source_load when we know the type == 0 */
static unsigned long weighted_cpuload(const int cpu)
{
	return cpu_rq(cpu)->load.weight;
}

/*
 * Return a low guess at the load of a migration-source cpu weighted
 * according to the scheduling class and "nice" value.
 *
 * We want to under-estimate the load of migration sources, to
 * balance conservatively.
 */
static unsigned long source_load(int cpu, int type)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long total = weighted_cpuload(cpu);

	if (type == 0 || !sched_feat(LB_BIAS))
		return total;

	return min(rq->cpu_load[type-1], total);
}

/*
 * Return a high guess at the load of a migration-target cpu weighted
 * according to the scheduling class and "nice" value.
 */
static unsigned long target_load(int cpu, int type)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long total = weighted_cpuload(cpu);

	if (type == 0 || !sched_feat(LB_BIAS))
		return total;

	return max(rq->cpu_load[type-1], total);
}

static unsigned long power_of(int cpu)
{
	return cpu_rq(cpu)->cpu_power;
}

static int task_hot(struct task_struct *p, u64 now, struct sched_domain *sd);

static unsigned long cpu_avg_load_per_task(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long nr_running = ACCESS_ONCE(rq->nr_running);

	if (nr_running)
		rq->avg_load_per_task = rq->load.weight / nr_running;
	else
		rq->avg_load_per_task = 0;

	return rq->avg_load_per_task;
}

#ifdef CONFIG_FAIR_GROUP_SCHED

static __read_mostly unsigned long __percpu *update_shares_data;

static void __set_se_shares(struct sched_entity *se, unsigned long shares);

/*
 * Calculate and set the cpu's group shares.
 */
static void update_group_shares_cpu(struct task_group *tg, int cpu,
				    unsigned long sd_shares,
				    unsigned long sd_rq_weight,
				    unsigned long *usd_rq_weight)
{
	unsigned long shares, rq_weight;
	int boost = 0;

	rq_weight = usd_rq_weight[cpu];
	if (!rq_weight) {
		boost = 1;
		rq_weight = NICE_0_LOAD;
	}

	/*
	 *             \Sum_j shares_j * rq_weight_i
	 * shares_i =  -----------------------------
	 *                  \Sum_j rq_weight_j
	 */
	shares = (sd_shares * rq_weight) / sd_rq_weight;
	shares = clamp_t(unsigned long, shares, MIN_SHARES, MAX_SHARES);

	if (abs(shares - tg->se[cpu]->load.weight) >
			sysctl_sched_shares_thresh) {
		struct rq *rq = cpu_rq(cpu);
		unsigned long flags;

		raw_spin_lock_irqsave(&rq->lock, flags);
		tg->cfs_rq[cpu]->rq_weight = boost ? 0 : rq_weight;
		tg->cfs_rq[cpu]->shares = boost ? 0 : shares;
		__set_se_shares(tg->se[cpu], shares);
		raw_spin_unlock_irqrestore(&rq->lock, flags);
	}
}

/*
 * Re-compute the task group their per cpu shares over the given domain.
 * This needs to be done in a bottom-up fashion because the rq weight of a
 * parent group depends on the shares of its child groups.
 */
static int tg_shares_up(struct task_group *tg, void *data)
{
	unsigned long weight, rq_weight = 0, sum_weight = 0, shares = 0;
	unsigned long *usd_rq_weight;
	struct sched_domain *sd = data;
	unsigned long flags;
	int i;

	if (!tg->se[0])
		return 0;

	local_irq_save(flags);
	usd_rq_weight = per_cpu_ptr(update_shares_data, smp_processor_id());

	for_each_cpu(i, sched_domain_span(sd)) {
		weight = tg->cfs_rq[i]->load.weight;
		usd_rq_weight[i] = weight;

		rq_weight += weight;
		/*
		 * If there are currently no tasks on the cpu pretend there
		 * is one of average load so that when a new task gets to
		 * run here it will not get delayed by group starvation.
		 */
		if (!weight)
			weight = NICE_0_LOAD;

		sum_weight += weight;
		shares += tg->cfs_rq[i]->shares;
	}

	if (!rq_weight)
		rq_weight = sum_weight;

	if ((!shares && rq_weight) || shares > tg->shares)
		shares = tg->shares;

	if (!sd->parent || !(sd->parent->flags & SD_LOAD_BALANCE))
		shares = tg->shares;

	for_each_cpu(i, sched_domain_span(sd))
		update_group_shares_cpu(tg, i, shares, rq_weight, usd_rq_weight);

	local_irq_restore(flags);

	return 0;
}

/*
 * Compute the cpu's hierarchical load factor for each task group.
 * This needs to be done in a top-down fashion because the load of a child
 * group is a fraction of its parents load.
 */
static int tg_load_down(struct task_group *tg, void *data)
{
	unsigned long load;
	long cpu = (long)data;

	if (!tg->parent) {
		load = cpu_rq(cpu)->load.weight;
	} else {
		load = tg->parent->cfs_rq[cpu]->h_load;
		load *= tg->cfs_rq[cpu]->shares;
		load /= tg->parent->cfs_rq[cpu]->load.weight + 1;
	}

	tg->cfs_rq[cpu]->h_load = load;

	return 0;
}

static void update_shares(struct sched_domain *sd)
{
	s64 elapsed;
	u64 now;

	if (root_task_group_empty())
		return;

	now = local_clock();
	elapsed = now - sd->last_update;

	if (elapsed >= (s64)(u64)sysctl_sched_shares_ratelimit) {
		sd->last_update = now;
		walk_tg_tree(tg_nop, tg_shares_up, sd);
	}
}

static void update_h_load(long cpu)
{
	walk_tg_tree(tg_load_down, tg_nop, (void *)cpu);
}

#else

static inline void update_shares(struct sched_domain *sd)
{
}

#endif

#ifdef CONFIG_PREEMPT

static void double_rq_lock(struct rq *rq1, struct rq *rq2);

/*
 * fair double_lock_balance: Safely acquires both rq->locks in a fair
 * way at the expense of forcing extra atomic operations in all
 * invocations.  This assures that the double_lock is acquired using the
 * same underlying policy as the spinlock_t on this architecture, which
 * reduces latency compared to the unfair variant below.  However, it
 * also adds more overhead and therefore may reduce throughput.
 */
static inline int _double_lock_balance(struct rq *this_rq, struct rq *busiest)
	__releases(this_rq->lock)
	__acquires(busiest->lock)
	__acquires(this_rq->lock)
{
	raw_spin_unlock(&this_rq->lock);
	double_rq_lock(this_rq, busiest);

	return 1;
}

#else
/*
 * Unfair double_lock_balance: Optimizes throughput at the expense of
 * latency by eliminating extra atomic operations when the locks are
 * already in proper order on entry.  This favors lower cpu-ids and will
 * grant the double lock to lower cpus over higher ids under contention,
 * regardless of entry order into the function.
 */
static int _double_lock_balance(struct rq *this_rq, struct rq *busiest)
	__releases(this_rq->lock)
	__acquires(busiest->lock)
	__acquires(this_rq->lock)
{
	int ret = 0;

	if (unlikely(!raw_spin_trylock(&busiest->lock))) {
		if (busiest < this_rq) {
			raw_spin_unlock(&this_rq->lock);
			raw_spin_lock(&busiest->lock);
			raw_spin_lock_nested(&this_rq->lock,
					      SINGLE_DEPTH_NESTING);
			ret = 1;
		} else
			raw_spin_lock_nested(&busiest->lock,
					      SINGLE_DEPTH_NESTING);
	}
	return ret;
}

#endif /* CONFIG_PREEMPT */

/*
 * double_lock_balance - lock the busiest runqueue, this_rq is locked already.
 */
static int double_lock_balance(struct rq *this_rq, struct rq *busiest)
{
	if (unlikely(!irqs_disabled())) {
		/* printk() doesn't work good under rq->lock */
		raw_spin_unlock(&this_rq->lock);
		BUG_ON(1);
	}

	return _double_lock_balance(this_rq, busiest);
}

static inline void double_unlock_balance(struct rq *this_rq, struct rq *busiest)
	__releases(busiest->lock)
{
	raw_spin_unlock(&busiest->lock);
	lock_set_subclass(&this_rq->lock.dep_map, 0, _RET_IP_);
}

/*
 * double_rq_lock - safely lock two runqueues
 *
 * Note this does not disable interrupts like task_rq_lock,
 * you need to do so manually before calling.
 */
static void double_rq_lock(struct rq *rq1, struct rq *rq2)
	__acquires(rq1->lock)
	__acquires(rq2->lock)
{
	BUG_ON(!irqs_disabled());
	if (rq1 == rq2) {
		raw_spin_lock(&rq1->lock);
		__acquire(rq2->lock);	/* Fake it out ;) */
	} else {
		if (rq1 < rq2) {
			raw_spin_lock(&rq1->lock);
			raw_spin_lock_nested(&rq2->lock, SINGLE_DEPTH_NESTING);
		} else {
			raw_spin_lock(&rq2->lock);
			raw_spin_lock_nested(&rq1->lock, SINGLE_DEPTH_NESTING);
		}
	}
}

/*
 * double_rq_unlock - safely unlock two runqueues
 *
 * Note this does not restore interrupts like task_rq_unlock,
 * you need to do so manually after calling.
 */
static void double_rq_unlock(struct rq *rq1, struct rq *rq2)
	__releases(rq1->lock)
	__releases(rq2->lock)
{
	raw_spin_unlock(&rq1->lock);
	if (rq1 != rq2)
		raw_spin_unlock(&rq2->lock);
	else
		__release(rq2->lock);
}

#endif

#ifdef CONFIG_FAIR_GROUP_SCHED
static void cfs_rq_set_shares(struct cfs_rq *cfs_rq, unsigned long shares)
{
#ifdef CONFIG_SMP
	cfs_rq->shares = shares;
#endif
}
#endif

static void calc_load_account_idle(struct rq *this_rq);
static void update_sysctl(void);
static int get_update_sysctl_factor(void);
static void update_cpu_load(struct rq *this_rq);

static inline void __set_task_cpu(struct task_struct *p, unsigned int cpu)
{
	set_task_rq(p, cpu);
#ifdef CONFIG_SMP
	/*
	 * After ->cpu is set up to a new value, task_rq_lock(p, ...) can be
	 * successfuly executed on another CPU. We must ensure that updates of
	 * per-task data have been completed by this moment.
	 */
	smp_wmb();
	task_thread_info(p)->cpu = cpu;
#endif
}

static const struct sched_class rt_sched_class;

#define sched_class_highest (&rt_sched_class)
#define for_each_class(class) \
   for (class = sched_class_highest; class; class = class->next)

#include "sched_stats.h"

static void inc_nr_running(struct rq *rq)
{
	rq->nr_running++;
}

static void dec_nr_running(struct rq *rq)
{
	rq->nr_running--;
}

static void set_load_weight(struct task_struct *p)
{
	/*
	 * SCHED_IDLE tasks get minimal weight:
	 */
	if (p->policy == SCHED_IDLE) {
		p->se.load.weight = WEIGHT_IDLEPRIO;
		p->se.load.inv_weight = WMULT_IDLEPRIO;
		return;
	}

	p->se.load.weight = prio_to_weight[p->static_prio - MAX_RT_PRIO];
	p->se.load.inv_weight = prio_to_wmult[p->static_prio - MAX_RT_PRIO];
}

static void enqueue_task(struct rq *rq, struct task_struct *p, int flags)
{
	update_rq_clock(rq);
	sched_info_queued(p);
	p->sched_class->enqueue_task(rq, p, flags);
	p->se.on_rq = 1;
}

static void dequeue_task(struct rq *rq, struct task_struct *p, int flags)
{
	update_rq_clock(rq);
	sched_info_dequeued(p);
	p->sched_class->dequeue_task(rq, p, flags);
	p->se.on_rq = 0;
}

/*
 * activate_task - move a task to the runqueue.
 */
static void activate_task(struct rq *rq, struct task_struct *p, int flags)
{
	if (task_contributes_to_load(p))
		rq->nr_uninterruptible--;

	enqueue_task(rq, p, flags);
	inc_nr_running(rq);
}

/*
 * deactivate_task - remove a task from the runqueue.
 */
static void deactivate_task(struct rq *rq, struct task_struct *p, int flags)
{
	if (task_contributes_to_load(p))
		rq->nr_uninterruptible++;

	dequeue_task(rq, p, flags);
	dec_nr_running(rq);
}

#include "sched_idletask.c"
#include "sched_fair.c"
#include "sched_rt.c"
#ifdef CONFIG_SCHED_DEBUG
# include "sched_debug.c"
#endif

/*
 * __normal_prio - return the priority that is based on the static prio
 */
static inline int __normal_prio(struct task_struct *p)
{
	return p->static_prio;
}

/*
 * Calculate the expected normal priority: i.e. priority
 * without taking RT-inheritance into account. Might be
 * boosted by interactivity modifiers. Changes upon fork,
 * setprio syscalls, and whenever the interactivity
 * estimator recalculates.
 */
static inline int normal_prio(struct task_struct *p)
{
	int prio;

	if (task_has_rt_policy(p))
		prio = MAX_RT_PRIO-1 - p->rt_priority;
	else
		prio = __normal_prio(p);
	return prio;
}

/*
 * Calculate the current priority, i.e. the priority
 * taken into account by the scheduler. This value might
 * be boosted by RT tasks, or might be boosted by
 * interactivity modifiers. Will be RT if the task got
 * RT-boosted. If not then it returns p->normal_prio.
 */
static int effective_prio(struct task_struct *p)
{
	p->normal_prio = normal_prio(p);
	/*
	 * If we are RT tasks or we were boosted to RT priority,
	 * keep the priority unchanged. Otherwise, update priority
	 * to the normal priority:
	 */
	if (!rt_prio(p->prio))
		return p->normal_prio;
	return p->prio;
}

/**
 * task_curr - is this task currently executing on a CPU?
 * @p: the task in question.
 */
inline int task_curr(const struct task_struct *p)
{
	return cpu_curr(task_cpu(p)) == p;
}

static inline void check_class_changed(struct rq *rq, struct task_struct *p,
				       const struct sched_class *prev_class,
				       int oldprio, int running)
{
	if (prev_class != p->sched_class) {
		if (prev_class->switched_from)
			prev_class->switched_from(rq, p, running);
		p->sched_class->switched_to(rq, p, running);
	} else
		p->sched_class->prio_changed(rq, p, oldprio, running);
}

#ifdef CONFIG_SMP
/*
 * Is this task likely cache-hot:
 */
static int
task_hot(struct task_struct *p, u64 now, struct sched_domain *sd)
{
	s64 delta;

	if (p->sched_class != &fair_sched_class)
		return 0;

	/*
	 * Buddy candidates are cache hot:
	 */
	if (sched_feat(CACHE_HOT_BUDDY) && this_rq()->nr_running &&
			(&p->se == cfs_rq_of(&p->se)->next ||
			 &p->se == cfs_rq_of(&p->se)->last))
		return 1;

	if (sysctl_sched_migration_cost == -1)
		return 1;
	if (sysctl_sched_migration_cost == 0)
		return 0;

	delta = now - p->se.exec_start;

	return delta < (s64)sysctl_sched_migration_cost;
}

void set_task_cpu(struct task_struct *p, unsigned int new_cpu)
{
#ifdef CONFIG_SCHED_DEBUG
	/*
	 * We should never call set_task_cpu() on a blocked task,
	 * ttwu() will sort out the placement.
	 */
	WARN_ON_ONCE(p->state != TASK_RUNNING && p->state != TASK_WAKING &&
			!(task_thread_info(p)->preempt_count & PREEMPT_ACTIVE));
#endif

	trace_sched_migrate_task(p, new_cpu);

	if (task_cpu(p) != new_cpu) {
		p->se.nr_migrations++;
		perf_sw_event(PERF_COUNT_SW_CPU_MIGRATIONS, 1, 1, NULL, 0);
	}

	__set_task_cpu(p, new_cpu);
}

struct migration_arg {
	struct task_struct *task;
	int dest_cpu;
};

static int migration_cpu_stop(void *data);

/*
 * The task's runqueue lock must be held.
 * Returns true if you have to wait for migration thread.
 */
static bool migrate_task(struct task_struct *p, int dest_cpu)
{
	struct rq *rq = task_rq(p);

	/*
	 * If the task is not on a runqueue (and not running), then
	 * the next wake-up will properly place the task.
	 */
	return p->se.on_rq || task_running(rq, p);
}

/*
 * wait_task_inactive - wait for a thread to unschedule.
 *
 * If @match_state is nonzero, it's the @p->state value just checked and
 * not expected to change.  If it changes, i.e. @p might have woken up,
 * then return zero.  When we succeed in waiting for @p to be off its CPU,
 * we return a positive number (its total switch count).  If a second call
 * a short while later returns the same number, the caller can be sure that
 * @p has remained unscheduled the whole time.
 *
 * The caller must ensure that the task *will* unschedule sometime soon,
 * else this function might spin for a *long* time. This function can't
 * be called with interrupts off, or it may introduce deadlock with
 * smp_call_function() if an IPI is sent by the same process we are
 * waiting to become inactive.
 */
unsigned long wait_task_inactive(struct task_struct *p, long match_state)
{
	unsigned long flags;
	int running, on_rq;
	unsigned long ncsw;
	struct rq *rq;

	for (;;) {
		/*
		 * We do the initial early heuristics without holding
		 * any task-queue locks at all. We'll only try to get
		 * the runqueue lock when things look like they will
		 * work out!
		 */
		rq = task_rq(p);

		/*
		 * If the task is actively running on another CPU
		 * still, just relax and busy-wait without holding
		 * any locks.
		 *
		 * NOTE! Since we don't hold any locks, it's not
		 * even sure that "rq" stays as the right runqueue!
		 * But we don't care, since "task_running()" will
		 * return false if the runqueue has changed and p
		 * is actually now running somewhere else!
		 */
		while (task_running(rq, p)) {
			if (match_state && unlikely(p->state != match_state))
				return 0;
			cpu_relax();
		}

		/*
		 * Ok, time to look more closely! We need the rq
		 * lock now, to be *sure*. If we're wrong, we'll
		 * just go back and repeat.
		 */
		rq = task_rq_lock(p, &flags);
		trace_sched_wait_task(p);
		running = task_running(rq, p);
		on_rq = p->se.on_rq;
		ncsw = 0;
		if (!match_state || p->state == match_state)
			ncsw = p->nvcsw | LONG_MIN; /* sets MSB */
		task_rq_unlock(rq, &flags);

		/*
		 * If it changed from the expected state, bail out now.
		 */
		if (unlikely(!ncsw))
			break;

		/*
		 * Was it really running after all now that we
		 * checked with the proper locks actually held?
		 *
		 * Oops. Go back and try again..
		 */
		if (unlikely(running)) {
			cpu_relax();
			continue;
		}

		/*
		 * It's not enough that it's not actively running,
		 * it must be off the runqueue _entirely_, and not
		 * preempted!
		 *
		 * So if it was still runnable (but just not actively
		 * running right now), it's preempted, and we should
		 * yield - it could be a while.
		 */
		if (unlikely(on_rq)) {
			schedule_timeout_uninterruptible(1);
			continue;
		}

		/*
		 * Ahh, all good. It wasn't running, and it wasn't
		 * runnable, which means that it will never become
		 * running in the future either. We're all done!
		 */
		break;
	}

	return ncsw;
}

/***
 * kick_process - kick a running thread to enter/exit the kernel
 * @p: the to-be-kicked thread
 *
 * Cause a process which is running on another CPU to enter
 * kernel-mode, without any delay. (to get signals handled.)
 *
 * NOTE: this function doesnt have to take the runqueue lock,
 * because all it wants to ensure is that the remote task enters
 * the kernel. If the IPI races and the task has been migrated
 * to another CPU then no harm is done and the purpose has been
 * achieved as well.
 */
void kick_process(struct task_struct *p)
{
	int cpu;

	preempt_disable();
	cpu = task_cpu(p);
	if ((cpu != smp_processor_id()) && task_curr(p))
		smp_send_reschedule(cpu);
	preempt_enable();
}
EXPORT_SYMBOL_GPL(kick_process);
#endif /* CONFIG_SMP */

/**
 * task_oncpu_function_call - call a function on the cpu on which a task runs
 * @p:		the task to evaluate
 * @func:	the function to be called
 * @info:	the function call argument
 *
 * Calls the function @func when the task is currently running. This might
 * be on the current CPU, which just calls the function directly
 */
void task_oncpu_function_call(struct task_struct *p,
			      void (*func) (void *info), void *info)
{
	int cpu;

	preempt_disable();
	cpu = task_cpu(p);
	if (task_curr(p))
		smp_call_function_single(cpu, func, info, 1);
	preempt_enable();
}

#ifdef CONFIG_SMP
/*
 * ->cpus_allowed is protected by either TASK_WAKING or rq->lock held.
 */
static int select_fallback_rq(int cpu, struct task_struct *p)
{
	int dest_cpu;
	const struct cpumask *nodemask = cpumask_of_node(cpu_to_node(cpu));

	/* Look for allowed, online CPU in same node. */
	for_each_cpu_and(dest_cpu, nodemask, cpu_active_mask)
		if (cpumask_test_cpu(dest_cpu, &p->cpus_allowed))
			return dest_cpu;

	/* Any allowed, online CPU? */
	dest_cpu = cpumask_any_and(&p->cpus_allowed, cpu_active_mask);
	if (dest_cpu < nr_cpu_ids)
		return dest_cpu;

	/* No more Mr. Nice Guy. */
	if (unlikely(dest_cpu >= nr_cpu_ids)) {
		dest_cpu = cpuset_cpus_allowed_fallback(p);
		/*
		 * Don't tell them about moving exiting tasks or
		 * kernel threads (both mm NULL), since they never
		 * leave kernel.
		 */
		if (p->mm && printk_ratelimit()) {
			printk(KERN_INFO "process %d (%s) no "
			       "longer affine to cpu%d\n",
			       task_pid_nr(p), p->comm, cpu);
		}
	}

	return dest_cpu;
}

/*
 * The caller (fork, wakeup) owns TASK_WAKING, ->cpus_allowed is stable.
 */
static inline
int select_task_rq(struct rq *rq, struct task_struct *p, int sd_flags, int wake_flags)
{
	int cpu = p->sched_class->select_task_rq(rq, p, sd_flags, wake_flags);

	/*
	 * In order not to call set_task_cpu() on a blocking task we need
	 * to rely on ttwu() to place the task on a valid ->cpus_allowed
	 * cpu.
	 *
	 * Since this is common to all placement strategies, this lives here.
	 *
	 * [ this allows ->select_task() to simply return task_cpu(p) and
	 *   not worry about this generic constraint ]
	 */
	if (unlikely(!cpumask_test_cpu(cpu, &p->cpus_allowed) ||
		     !cpu_online(cpu)))
		cpu = select_fallback_rq(task_cpu(p), p);

	return cpu;
}

static void update_avg(u64 *avg, u64 sample)
{
	s64 diff = sample - *avg;
	*avg += diff >> 3;
}
#endif

static inline void ttwu_activate(struct task_struct *p, struct rq *rq,
				 bool is_sync, bool is_migrate, bool is_local,
				 unsigned long en_flags)
{
	schedstat_inc(p, se.statistics.nr_wakeups);
	if (is_sync)
		schedstat_inc(p, se.statistics.nr_wakeups_sync);
	if (is_migrate)
		schedstat_inc(p, se.statistics.nr_wakeups_migrate);
	if (is_local)
		schedstat_inc(p, se.statistics.nr_wakeups_local);
	else
		schedstat_inc(p, se.statistics.nr_wakeups_remote);

	activate_task(rq, p, en_flags);
}

static inline void ttwu_post_activation(struct task_struct *p, struct rq *rq,
					int wake_flags, bool success)
{
	trace_sched_wakeup(p, success);
	check_preempt_curr(rq, p, wake_flags);

	p->state = TASK_RUNNING;
#ifdef CONFIG_SMP
	if (p->sched_class->task_woken)
		p->sched_class->task_woken(rq, p);

	if (unlikely(rq->idle_stamp)) {
		u64 delta = rq->clock - rq->idle_stamp;
		u64 max = 2*sysctl_sched_migration_cost;

		if (delta > max)
			rq->avg_idle = max;
		else
			update_avg(&rq->avg_idle, delta);
		rq->idle_stamp = 0;
	}
#endif
	/* if a worker is waking up, notify workqueue */
	if ((p->flags & PF_WQ_WORKER) && success)
		wq_worker_waking_up(p, cpu_of(rq));
}

/**
 * try_to_wake_up - wake up a thread
 * @p: the thread to be awakened
 * @state: the mask of task states that can be woken
 * @wake_flags: wake modifier flags (WF_*)
 *
 * Put it on the run-queue if it's not already there. The "current"
 * thread is always on the run-queue (except when the actual
 * re-schedule is in progress), and as such you're allowed to do
 * the simpler "current->state = TASK_RUNNING" to mark yourself
 * runnable without the overhead of this.
 *
 * Returns %true if @p was woken up, %false if it was already running
 * or @state didn't match @p's state.
 */
static int try_to_wake_up(struct task_struct *p, unsigned int state,
			  int wake_flags)
{
	int cpu, orig_cpu, this_cpu, success = 0;
	unsigned long flags;
	unsigned long en_flags = ENQUEUE_WAKEUP;
	struct rq *rq;

	this_cpu = get_cpu();

	smp_wmb();
	rq = task_rq_lock(p, &flags);
	if (!(p->state & state))
		goto out;

	if (p->se.on_rq)
		goto out_running;

	cpu = task_cpu(p);
	orig_cpu = cpu;

#ifdef CONFIG_SMP
	if (unlikely(task_running(rq, p)))
		goto out_activate;

	/*
	 * In order to handle concurrent wakeups and release the rq->lock
	 * we put the task in TASK_WAKING state.
	 *
	 * First fix up the nr_uninterruptible count:
	 */
	if (task_contributes_to_load(p)) {
		if (likely(cpu_online(orig_cpu)))
			rq->nr_uninterruptible--;
		else
			this_rq()->nr_uninterruptible--;
	}
	p->state = TASK_WAKING;

	if (p->sched_class->task_waking) {
		p->sched_class->task_waking(rq, p);
		en_flags |= ENQUEUE_WAKING;
	}

	cpu = select_task_rq(rq, p, SD_BALANCE_WAKE, wake_flags);
	if (cpu != orig_cpu)
		set_task_cpu(p, cpu);
	__task_rq_unlock(rq);

	rq = cpu_rq(cpu);
	raw_spin_lock(&rq->lock);

	/*
	 * We migrated the task without holding either rq->lock, however
	 * since the task is not on the task list itself, nobody else
	 * will try and migrate the task, hence the rq should match the
	 * cpu we just moved it to.
	 */
	WARN_ON(task_cpu(p) != cpu);
	WARN_ON(p->state != TASK_WAKING);

#ifdef CONFIG_SCHEDSTATS
	schedstat_inc(rq, ttwu_count);
	if (cpu == this_cpu)
		schedstat_inc(rq, ttwu_local);
	else {
		struct sched_domain *sd;
		for_each_domain(this_cpu, sd) {
			if (cpumask_test_cpu(cpu, sched_domain_span(sd))) {
				schedstat_inc(sd, ttwu_wake_remote);
				break;
			}
		}
	}
#endif /* CONFIG_SCHEDSTATS */

out_activate:
#endif /* CONFIG_SMP */
	ttwu_activate(p, rq, wake_flags & WF_SYNC, orig_cpu != cpu,
		      cpu == this_cpu, en_flags);
	success = 1;
out_running:
	ttwu_post_activation(p, rq, wake_flags, success);
out:
	task_rq_unlock(rq, &flags);
	put_cpu();

	return success;
}

/**
 * try_to_wake_up_local - try to wake up a local task with rq lock held
 * @p: the thread to be awakened
 *
 * Put @p on the run-queue if it's not alredy there.  The caller must
 * ensure that this_rq() is locked, @p is bound to this_rq() and not
 * the current task.  this_rq() stays locked over invocation.
 */
static void try_to_wake_up_local(struct task_struct *p)
{
	struct rq *rq = task_rq(p);
	bool success = false;

	BUG_ON(rq != this_rq());
	BUG_ON(p == current);
	lockdep_assert_held(&rq->lock);

	if (!(p->state & TASK_NORMAL))
		return;

	if (!p->se.on_rq) {
		if (likely(!task_running(rq, p))) {
			schedstat_inc(rq, ttwu_count);
			schedstat_inc(rq, ttwu_local);
		}
		ttwu_activate(p, rq, false, false, true, ENQUEUE_WAKEUP);
		success = true;
	}
	ttwu_post_activation(p, rq, 0, success);
}

/**
 * wake_up_process - Wake up a specific process
 * @p: The process to be woken up.
 *
 * Attempt to wake up the nominated process and move it to the set of runnable
 * processes.  Returns 1 if the process was woken up, 0 if it was already
 * running.
 *
 * It may be assumed that this function implies a write memory barrier before
 * changing the task state if and only if any tasks are woken up.
 */
int wake_up_process(struct task_struct *p)
{
	return try_to_wake_up(p, TASK_ALL, 0);
}
EXPORT_SYMBOL(wake_up_process);

int wake_up_state(struct task_struct *p, unsigned int state)
{
	return try_to_wake_up(p, state, 0);
}

/*
 * Perform scheduler related setup for a newly forked process p.
 * p is forked by current.
 *
 * __sched_fork() is basic setup used by init_idle() too:
 */
static void __sched_fork(struct task_struct *p)
{
	p->se.exec_start		= 0;
	p->se.sum_exec_runtime		= 0;
	p->se.prev_sum_exec_runtime	= 0;
	p->se.nr_migrations		= 0;

#ifdef CONFIG_SCHEDSTATS
	memset(&p->se.statistics, 0, sizeof(p->se.statistics));
#endif

	INIT_LIST_HEAD(&p->rt.run_list);
	p->se.on_rq = 0;
	INIT_LIST_HEAD(&p->se.group_node);

#ifdef CONFIG_PREEMPT_NOTIFIERS
	INIT_HLIST_HEAD(&p->preempt_notifiers);
#endif
}

/*
 * fork()/clone()-time setup:
 */
void sched_fork(struct task_struct *p, int clone_flags)
{
	int cpu = get_cpu();

	__sched_fork(p);
	/*
	 * We mark the process as running here. This guarantees that
	 * nobody will actually run it, and a signal or other external
	 * event cannot wake it up and insert it on the runqueue either.
	 */
	p->state = TASK_RUNNING;

	/*
	 * Revert to default priority/policy on fork if requested.
	 */
	if (unlikely(p->sched_reset_on_fork)) {
		if (p->policy == SCHED_FIFO || p->policy == SCHED_RR) {
			p->policy = SCHED_NORMAL;
			p->normal_prio = p->static_prio;
		}

		if (PRIO_TO_NICE(p->static_prio) < 0) {
			p->static_prio = NICE_TO_PRIO(0);
			p->normal_prio = p->static_prio;
			set_load_weight(p);
		}

		/*
		 * We don't need the reset flag anymore after the fork. It has
		 * fulfilled its duty:
		 */
		p->sched_reset_on_fork = 0;
	}

	/*
	 * Make sure we do not leak PI boosting priority to the child.
	 */
	p->prio = current->normal_prio;

	if (!rt_prio(p->prio))
		p->sched_class = &fair_sched_class;

	if (p->sched_class->task_fork)
		p->sched_class->task_fork(p);

	/*
	 * The child is not yet in the pid-hash so no cgroup attach races,
	 * and the cgroup is pinned to this child due to cgroup_fork()
	 * is ran before sched_fork().
	 *
	 * Silence PROVE_RCU.
	 */
	rcu_read_lock();
	set_task_cpu(p, cpu);
	rcu_read_unlock();

#if defined(CONFIG_SCHEDSTATS) || defined(CONFIG_TASK_DELAY_ACCT)
	if (likely(sched_info_on()))
		memset(&p->sched_info, 0, sizeof(p->sched_info));
#endif
#if defined(CONFIG_SMP) && defined(__ARCH_WANT_UNLOCKED_CTXSW)
	p->oncpu = 0;
#endif
#ifdef CONFIG_PREEMPT
	/* Want to start with kernel preemption disabled. */
	task_thread_info(p)->preempt_count = 1;
#endif
	plist_node_init(&p->pushable_tasks, MAX_PRIO);

	put_cpu();
}

/*
 * wake_up_new_task - wake up a newly created task for the first time.
 *
 * This function will do some initial scheduler statistics housekeeping
 * that must be done for every newly created context, then puts the task
 * on the runqueue and wakes it.
 */
void wake_up_new_task(struct task_struct *p, unsigned long clone_flags)
{
	unsigned long flags;
	struct rq *rq;
	int cpu __maybe_unused = get_cpu();

#ifdef CONFIG_SMP
	rq = task_rq_lock(p, &flags);
	p->state = TASK_WAKING;

	/*
	 * Fork balancing, do it here and not earlier because:
	 *  - cpus_allowed can change in the fork path
	 *  - any previously selected cpu might disappear through hotplug
	 *
	 * We set TASK_WAKING so that select_task_rq() can drop rq->lock
	 * without people poking at ->cpus_allowed.
	 */
	cpu = select_task_rq(rq, p, SD_BALANCE_FORK, 0);
	set_task_cpu(p, cpu);

	p->state = TASK_RUNNING;
	task_rq_unlock(rq, &flags);
#endif

	rq = task_rq_lock(p, &flags);
	activate_task(rq, p, 0);
	trace_sched_wakeup_new(p, 1);
	check_preempt_curr(rq, p, WF_FORK);
#ifdef CONFIG_SMP
	if (p->sched_class->task_woken)
		p->sched_class->task_woken(rq, p);
#endif
	task_rq_unlock(rq, &flags);
	put_cpu();
}

#ifdef CONFIG_PREEMPT_NOTIFIERS

/**
 * preempt_notifier_register - tell me when current is being preempted & rescheduled
 * @notifier: notifier struct to register
 */
void preempt_notifier_register(struct preempt_notifier *notifier)
{
	hlist_add_head(&notifier->link, &current->preempt_notifiers);
}
EXPORT_SYMBOL_GPL(preempt_notifier_register);

/**
 * preempt_notifier_unregister - no longer interested in preemption notifications
 * @notifier: notifier struct to unregister
 *
 * This is safe to call from within a preemption notifier.
 */
void preempt_notifier_unregister(struct preempt_notifier *notifier)
{
	hlist_del(&notifier->link);
}
EXPORT_SYMBOL_GPL(preempt_notifier_unregister);

static void fire_sched_in_preempt_notifiers(struct task_struct *curr)
{
	struct preempt_notifier *notifier;
	struct hlist_node *node;

	hlist_for_each_entry(notifier, node, &curr->preempt_notifiers, link)
		notifier->ops->sched_in(notifier, raw_smp_processor_id());
}

static void
fire_sched_out_preempt_notifiers(struct task_struct *curr,
				 struct task_struct *next)
{
	struct preempt_notifier *notifier;
	struct hlist_node *node;

	hlist_for_each_entry(notifier, node, &curr->preempt_notifiers, link)
		notifier->ops->sched_out(notifier, next);
}

#else /* !CONFIG_PREEMPT_NOTIFIERS */

static void fire_sched_in_preempt_notifiers(struct task_struct *curr)
{
}

static void
fire_sched_out_preempt_notifiers(struct task_struct *curr,
				 struct task_struct *next)
{
}

#endif /* CONFIG_PREEMPT_NOTIFIERS */

/**
 * prepare_task_switch - prepare to switch tasks
 * @rq: the runqueue preparing to switch
 * @prev: the current task that is being switched out
 * @next: the task we are going to switch to.
 *
 * This is called with the rq lock held and interrupts off. It must
 * be paired with a subsequent finish_task_switch after the context
 * switch.
 *
 * prepare_task_switch sets up locking and calls architecture specific
 * hooks.
 */
static inline void
prepare_task_switch(struct rq *rq, struct task_struct *prev,
		    struct task_struct *next)
{
	fire_sched_out_preempt_notifiers(prev, next);
	prepare_lock_switch(rq, next);
	prepare_arch_switch(next);
}

/**
 * finish_task_switch - clean up after a task-switch
 * @rq: runqueue associated with task-switch
 * @prev: the thread we just switched away from.
 *
 * finish_task_switch must be called after the context switch, paired
 * with a prepare_task_switch call before the context switch.
 * finish_task_switch will reconcile locking set up by prepare_task_switch,
 * and do any other architecture-specific cleanup actions.
 *
 * Note that we may have delayed dropping an mm in context_switch(). If
 * so, we finish that here outside of the runqueue lock. (Doing it
 * with the lock held can cause deadlocks; see schedule() for
 * details.)
 */
static void finish_task_switch(struct rq *rq, struct task_struct *prev)
	__releases(rq->lock)
{
	struct mm_struct *mm = rq->prev_mm;
	long prev_state;

	rq->prev_mm = NULL;

	/*
	 * A task struct has one reference for the use as "current".
	 * If a task dies, then it sets TASK_DEAD in tsk->state and calls
	 * schedule one last time. The schedule call will never return, and
	 * the scheduled task must drop that reference.
	 * The test for TASK_DEAD must occur while the runqueue locks are
	 * still held, otherwise prev could be scheduled on another cpu, die
	 * there before we look at prev->state, and then the reference would
	 * be dropped twice.
	 *		Manfred Spraul <manfred@colorfullife.com>
	 */
	prev_state = prev->state;
	finish_arch_switch(prev);
#ifdef __ARCH_WANT_INTERRUPTS_ON_CTXSW
	local_irq_disable();
#endif /* __ARCH_WANT_INTERRUPTS_ON_CTXSW */
	perf_event_task_sched_in(current);
#ifdef __ARCH_WANT_INTERRUPTS_ON_CTXSW
	local_irq_enable();
#endif /* __ARCH_WANT_INTERRUPTS_ON_CTXSW */
	finish_lock_switch(rq, prev);

	fire_sched_in_preempt_notifiers(current);
	if (mm)
		mmdrop(mm);
	if (unlikely(prev_state == TASK_DEAD)) {
		/*
		 * Remove function-return probe instances associated with this
		 * task and put them back on the free list.
		 */
		kprobe_flush_task(prev);
		put_task_struct(prev);
	}
}

#ifdef CONFIG_SMP

/* assumes rq->lock is held */
static inline void pre_schedule(struct rq *rq, struct task_struct *prev)
{
	if (prev->sched_class->pre_schedule)
		prev->sched_class->pre_schedule(rq, prev);
}

/* rq->lock is NOT held, but preemption is disabled */
static inline void post_schedule(struct rq *rq)
{
	if (rq->post_schedule) {
		unsigned long flags;

		raw_spin_lock_irqsave(&rq->lock, flags);
		if (rq->curr->sched_class->post_schedule)
			rq->curr->sched_class->post_schedule(rq);
		raw_spin_unlock_irqrestore(&rq->lock, flags);

		rq->post_schedule = 0;
	}
}

#else

static inline void pre_schedule(struct rq *rq, struct task_struct *p)
{
}

static inline void post_schedule(struct rq *rq)
{
}

#endif

/**
 * schedule_tail - first thing a freshly forked thread must call.
 * @prev: the thread we just switched away from.
 */
asmlinkage void schedule_tail(struct task_struct *prev)
	__releases(rq->lock)
{
	struct rq *rq = this_rq();

	finish_task_switch(rq, prev);

	post_schedule(rq);

#ifdef __ARCH_WANT_UNLOCKED_CTXSW
	/* In this case, finish_task_switch does not reenable preemption */
	preempt_enable();
#endif
	if (current->set_child_tid)
		put_user(task_pid_vnr(current), current->set_child_tid);
}

/*
 * context_switch - switch to the new MM and the new
 * thread's register state.
 */
static inline void
context_switch(struct rq *rq, struct task_struct *prev,
	       struct task_struct *next)
{
	struct mm_struct *mm, *oldmm;

	prepare_task_switch(rq, prev, next);
	trace_sched_switch(prev, next);
	mm = next->mm;
	oldmm = prev->active_mm;
	/*
	 * For paravirt, this is coupled with an exit in switch_to to
	 * combine the page table reload and the switch backend into
	 * one hypercall.
	 */
	arch_start_context_switch(prev);

	if (likely(!mm)) {
		next->active_mm = oldmm;
		atomic_inc(&oldmm->mm_count);
		enter_lazy_tlb(oldmm, next);
	} else
		switch_mm(oldmm, mm, next);

	if (likely(!prev->mm)) {
		prev->active_mm = NULL;
		rq->prev_mm = oldmm;
	}
	/*
	 * Since the runqueue lock will be released by the next
	 * task (which is an invalid locking op but in the case
	 * of the scheduler it's an obvious special-case), so we
	 * do an early lockdep release here:
	 */
#ifndef __ARCH_WANT_UNLOCKED_CTXSW
	spin_release(&rq->lock.dep_map, 1, _THIS_IP_);
#endif

	/* Here we just switch the register state and the stack. */
	switch_to(prev, next, prev);

	barrier();
	/*
	 * this_rq must be evaluated again because prev may have moved
	 * CPUs since it called schedule(), thus the 'rq' on its stack
	 * frame will be invalid.
	 */
	finish_task_switch(this_rq(), prev);
}

/*
 * nr_running, nr_uninterruptible and nr_context_switches:
 *
 * externally visible scheduler statistics: current number of runnable
 * threads, current number of uninterruptible-sleeping threads, total
 * number of context switches performed since bootup.
 */
unsigned long nr_running(void)
{
	unsigned long i, sum = 0;

	for_each_online_cpu(i)
		sum += cpu_rq(i)->nr_running;

	return sum;
}

unsigned long nr_uninterruptible(void)
{
	unsigned long i, sum = 0;

	for_each_possible_cpu(i)
		sum += cpu_rq(i)->nr_uninterruptible;

	/*
	 * Since we read the counters lockless, it might be slightly
	 * inaccurate. Do not allow it to go below zero though:
	 */
	if (unlikely((long)sum < 0))
		sum = 0;

	return sum;
}

unsigned long long nr_context_switches(void)
{
	int i;
	unsigned long long sum = 0;

	for_each_possible_cpu(i)
		sum += cpu_rq(i)->nr_switches;

	return sum;
}

unsigned long nr_iowait(void)
{
	unsigned long i, sum = 0;

	for_each_possible_cpu(i)
		sum += atomic_read(&cpu_rq(i)->nr_iowait);

	return sum;
}

unsigned long nr_iowait_cpu(int cpu)
{
	struct rq *this = cpu_rq(cpu);
	return atomic_read(&this->nr_iowait);
}

unsigned long this_cpu_load(void)
{
	struct rq *this = this_rq();
	return this->cpu_load[0];
}


/* Variables and functions for calc_load */
static atomic_long_t calc_load_tasks;
static unsigned long calc_load_update;
unsigned long avenrun[3];
EXPORT_SYMBOL(avenrun);

static long calc_load_fold_active(struct rq *this_rq)
{
	long nr_active, delta = 0;

	nr_active = this_rq->nr_running;
	nr_active += (long) this_rq->nr_uninterruptible;

	if (nr_active != this_rq->calc_load_active) {
		delta = nr_active - this_rq->calc_load_active;
		this_rq->calc_load_active = nr_active;
	}

	return delta;
}

static unsigned long
calc_load(unsigned long load, unsigned long exp, unsigned long active)
{
	load *= exp;
	load += active * (FIXED_1 - exp);
	load += 1UL << (FSHIFT - 1);
	return load >> FSHIFT;
}

#ifdef CONFIG_NO_HZ
/*
 * For NO_HZ we delay the active fold to the next LOAD_FREQ update.
 *
 * When making the ILB scale, we should try to pull this in as well.
 */
static atomic_long_t calc_load_tasks_idle;

static void calc_load_account_idle(struct rq *this_rq)
{
	long delta;

	delta = calc_load_fold_active(this_rq);
	if (delta)
		atomic_long_add(delta, &calc_load_tasks_idle);
}

static long calc_load_fold_idle(void)
{
	long delta = 0;

	/*
	 * Its got a race, we don't care...
	 */
	if (atomic_long_read(&calc_load_tasks_idle))
		delta = atomic_long_xchg(&calc_load_tasks_idle, 0);

	return delta;
}

/**
 * fixed_power_int - compute: x^n, in O(log n) time
 *
 * @x:         base of the power
 * @frac_bits: fractional bits of @x
 * @n:         power to raise @x to.
 *
 * By exploiting the relation between the definition of the natural power
 * function: x^n := x*x*...*x (x multiplied by itself for n times), and
 * the binary encoding of numbers used by computers: n := \Sum n_i * 2^i,
 * (where: n_i \elem {0, 1}, the binary vector representing n),
 * we find: x^n := x^(\Sum n_i * 2^i) := \Prod x^(n_i * 2^i), which is
 * of course trivially computable in O(log_2 n), the length of our binary
 * vector.
 */
static unsigned long
fixed_power_int(unsigned long x, unsigned int frac_bits, unsigned int n)
{
	unsigned long result = 1UL << frac_bits;

	if (n) for (;;) {
		if (n & 1) {
			result *= x;
			result += 1UL << (frac_bits - 1);
			result >>= frac_bits;
		}
		n >>= 1;
		if (!n)
			break;
		x *= x;
		x += 1UL << (frac_bits - 1);
		x >>= frac_bits;
	}

	return result;
}

/*
 * a1 = a0 * e + a * (1 - e)
 *
 * a2 = a1 * e + a * (1 - e)
 *    = (a0 * e + a * (1 - e)) * e + a * (1 - e)
 *    = a0 * e^2 + a * (1 - e) * (1 + e)
 *
 * a3 = a2 * e + a * (1 - e)
 *    = (a0 * e^2 + a * (1 - e) * (1 + e)) * e + a * (1 - e)
 *    = a0 * e^3 + a * (1 - e) * (1 + e + e^2)
 *
 *  ...
 *
 * an = a0 * e^n + a * (1 - e) * (1 + e + ... + e^n-1) [1]
 *    = a0 * e^n + a * (1 - e) * (1 - e^n)/(1 - e)
 *    = a0 * e^n + a * (1 - e^n)
 *
 * [1] application of the geometric series:
 *
 *              n         1 - x^(n+1)
 *     S_n := \Sum x^i = -------------
 *             i=0          1 - x
 */
static unsigned long
calc_load_n(unsigned long load, unsigned long exp,
	    unsigned long active, unsigned int n)
{

	return calc_load(load, fixed_power_int(exp, FSHIFT, n), active);
}

/*
 * NO_HZ can leave us missing all per-cpu ticks calling
 * calc_load_account_active(), but since an idle CPU folds its delta into
 * calc_load_tasks_idle per calc_load_account_idle(), all we need to do is fold
 * in the pending idle delta if our idle period crossed a load cycle boundary.
 *
 * Once we've updated the global active value, we need to apply the exponential
 * weights adjusted to the number of cycles missed.
 */
static void calc_global_nohz(unsigned long ticks)
{
	long delta, active, n;

	if (time_before(jiffies, calc_load_update))
		return;

	/*
	 * If we crossed a calc_load_update boundary, make sure to fold
	 * any pending idle changes, the respective CPUs might have
	 * missed the tick driven calc_load_account_active() update
	 * due to NO_HZ.
	 */
	delta = calc_load_fold_idle();
	if (delta)
		atomic_long_add(delta, &calc_load_tasks);

	/*
	 * If we were idle for multiple load cycles, apply them.
	 */
	if (ticks >= LOAD_FREQ) {
		n = ticks / LOAD_FREQ;

		active = atomic_long_read(&calc_load_tasks);
		active = active > 0 ? active * FIXED_1 : 0;

		avenrun[0] = calc_load_n(avenrun[0], EXP_1, active, n);
		avenrun[1] = calc_load_n(avenrun[1], EXP_5, active, n);
		avenrun[2] = calc_load_n(avenrun[2], EXP_15, active, n);

		calc_load_update += n * LOAD_FREQ;
	}

	/*
	 * Its possible the remainder of the above division also crosses
	 * a LOAD_FREQ period, the regular check in calc_global_load()
	 * which comes after this will take care of that.
	 *
	 * Consider us being 11 ticks before a cycle completion, and us
	 * sleeping for 4*LOAD_FREQ + 22 ticks, then the above code will
	 * age us 4 cycles, and the test in calc_global_load() will
	 * pick up the final one.
	 */
}
#else
static void calc_load_account_idle(struct rq *this_rq)
{
}

static inline long calc_load_fold_idle(void)
{
	return 0;
}

static void calc_global_nohz(unsigned long ticks)
{
}
#endif

/**
 * get_avenrun - get the load average array
 * @loads:	pointer to dest load array
 * @offset:	offset to add
 * @shift:	shift count to shift the result left
 *
 * These values are estimates at best, so no need for locking.
 */
void get_avenrun(unsigned long *loads, unsigned long offset, int shift)
{
	loads[0] = (avenrun[0] + offset) << shift;
	loads[1] = (avenrun[1] + offset) << shift;
	loads[2] = (avenrun[2] + offset) << shift;
}

/*
 * calc_load - update the avenrun load estimates 10 ticks after the
 * CPUs have updated calc_load_tasks.
 */
void calc_global_load(unsigned long ticks)
{
	long active;

	calc_global_nohz(ticks);

	if (time_before(jiffies, calc_load_update + 10))
		return;

	active = atomic_long_read(&calc_load_tasks);
	active = active > 0 ? active * FIXED_1 : 0;

	avenrun[0] = calc_load(avenrun[0], EXP_1, active);
	avenrun[1] = calc_load(avenrun[1], EXP_5, active);
	avenrun[2] = calc_load(avenrun[2], EXP_15, active);

	calc_load_update += LOAD_FREQ;
}

/*
 * Called from update_cpu_load() to periodically update this CPU's
 * active count.
 */
static void calc_load_account_active(struct rq *this_rq)
{
	long delta;

	if (time_before(jiffies, this_rq->calc_load_update))
		return;

	delta  = calc_load_fold_active(this_rq);
	delta += calc_load_fold_idle();
	if (delta)
		atomic_long_add(delta, &calc_load_tasks);

	this_rq->calc_load_update += LOAD_FREQ;
}

/*
 * The exact cpuload at various idx values, calculated at every tick would be
 * load = (2^idx - 1) / 2^idx * load + 1 / 2^idx * cur_load
 *
 * If a cpu misses updates for n-1 ticks (as it was idle) and update gets called
 * on nth tick when cpu may be busy, then we have:
 * load = ((2^idx - 1) / 2^idx)^(n-1) * load
 * load = (2^idx - 1) / 2^idx) * load + 1 / 2^idx * cur_load
 *
 * decay_load_missed() below does efficient calculation of
 * load = ((2^idx - 1) / 2^idx)^(n-1) * load
 * avoiding 0..n-1 loop doing load = ((2^idx - 1) / 2^idx) * load
 *
 * The calculation is approximated on a 128 point scale.
 * degrade_zero_ticks is the number of ticks after which load at any
 * particular idx is approximated to be zero.
 * degrade_factor is a precomputed table, a row for each load idx.
 * Each column corresponds to degradation factor for a power of two ticks,
 * based on 128 point scale.
 * Example:
 * row 2, col 3 (=12) says that the degradation at load idx 2 after
 * 8 ticks is 12/128 (which is an approximation of exact factor 3^8/4^8).
 *
 * With this power of 2 load factors, we can degrade the load n times
 * by looking at 1 bits in n and doing as many mult/shift instead of
 * n mult/shifts needed by the exact degradation.
 */
#define DEGRADE_SHIFT		7
static const unsigned char
		degrade_zero_ticks[CPU_LOAD_IDX_MAX] = {0, 8, 32, 64, 128};
static const unsigned char
		degrade_factor[CPU_LOAD_IDX_MAX][DEGRADE_SHIFT + 1] = {
					{0, 0, 0, 0, 0, 0, 0, 0},
					{64, 32, 8, 0, 0, 0, 0, 0},
					{96, 72, 40, 12, 1, 0, 0},
					{112, 98, 75, 43, 15, 1, 0},
					{120, 112, 98, 76, 45, 16, 2} };

/*
 * Update cpu_load for any missed ticks, due to tickless idle. The backlog
 * would be when CPU is idle and so we just decay the old load without
 * adding any new load.
 */
static unsigned long
decay_load_missed(unsigned long load, unsigned long missed_updates, int idx)
{
	int j = 0;

	if (!missed_updates)
		return load;

	if (missed_updates >= degrade_zero_ticks[idx])
		return 0;

	if (idx == 1)
		return load >> missed_updates;

	while (missed_updates) {
		if (missed_updates % 2)
			load = (load * degrade_factor[idx][j]) >> DEGRADE_SHIFT;

		missed_updates >>= 1;
		j++;
	}
	return load;
}

/*
 * Update rq->cpu_load[] statistics. This function is usually called every
 * scheduler tick (TICK_NSEC). With tickless idle this will not be called
 * every tick. We fix it up based on jiffies.
 */
static void update_cpu_load(struct rq *this_rq)
{
	unsigned long this_load = this_rq->load.weight;
	unsigned long curr_jiffies = jiffies;
	unsigned long pending_updates;
	int i, scale;

	this_rq->nr_load_updates++;

	/* Avoid repeated calls on same jiffy, when moving in and out of idle */
	if (curr_jiffies == this_rq->last_load_update_tick)
		return;

	pending_updates = curr_jiffies - this_rq->last_load_update_tick;
	this_rq->last_load_update_tick = curr_jiffies;

	/* Update our load: */
	this_rq->cpu_load[0] = this_load; /* Fasttrack for idx 0 */
	for (i = 1, scale = 2; i < CPU_LOAD_IDX_MAX; i++, scale += scale) {
		unsigned long old_load, new_load;

		/* scale is effectively 1 << i now, and >> i divides by scale */

		old_load = this_rq->cpu_load[i];
		old_load = decay_load_missed(old_load, pending_updates - 1, i);
		new_load = this_load;
		/*
		 * Round up the averaging division if load is increasing. This
		 * prevents us from getting stuck on 9 if the load is 10, for
		 * example.
		 */
		if (new_load > old_load)
			new_load += scale - 1;

		this_rq->cpu_load[i] = (old_load * (scale - 1) + new_load) >> i;
	}

	sched_avg_update(this_rq);
}

static void update_cpu_load_active(struct rq *this_rq)
{
	update_cpu_load(this_rq);

	calc_load_account_active(this_rq);
}

#ifdef CONFIG_SMP

/*
 * sched_exec - execve() is a valuable balancing opportunity, because at
 * this point the task has the smallest effective memory and cache footprint.
 */
void sched_exec(void)
{
	struct task_struct *p = current;
	unsigned long flags;
	struct rq *rq;
	int dest_cpu;

	rq = task_rq_lock(p, &flags);
	dest_cpu = p->sched_class->select_task_rq(rq, p, SD_BALANCE_EXEC, 0);
	if (dest_cpu == smp_processor_id())
		goto unlock;

	/*
	 * select_task_rq() can race against ->cpus_allowed
	 */
	if (cpumask_test_cpu(dest_cpu, &p->cpus_allowed) &&
	    likely(cpu_active(dest_cpu)) && migrate_task(p, dest_cpu)) {
		struct migration_arg arg = { p, dest_cpu };

		task_rq_unlock(rq, &flags);
		stop_one_cpu(cpu_of(rq), migration_cpu_stop, &arg);
		return;
	}
unlock:
	task_rq_unlock(rq, &flags);
}

#endif

DEFINE_PER_CPU(struct kernel_stat, kstat);

EXPORT_PER_CPU_SYMBOL(kstat);

/*
 * Return any ns on the sched_clock that have not yet been accounted in
 * @p in case that task is currently running.
 *
 * Called with task_rq_lock() held on @rq.
 */
static u64 do_task_delta_exec(struct task_struct *p, struct rq *rq)
{
	u64 ns = 0;

	if (task_current(rq, p)) {
		update_rq_clock(rq);
		ns = rq->clock - p->se.exec_start;
		if ((s64)ns < 0)
			ns = 0;
	}

	return ns;
}

unsigned long long task_delta_exec(struct task_struct *p)
{
	unsigned long flags;
	struct rq *rq;
	u64 ns = 0;

	rq = task_rq_lock(p, &flags);
	ns = do_task_delta_exec(p, rq);
	task_rq_unlock(rq, &flags);

	return ns;
}

/*
 * Return accounted runtime for the task.
 * In case the task is currently running, return the runtime plus current's
 * pending runtime that have not been accounted yet.
 */
unsigned long long task_sched_runtime(struct task_struct *p)
{
	unsigned long flags;
	struct rq *rq;
	u64 ns = 0;

	rq = task_rq_lock(p, &flags);
	ns = p->se.sum_exec_runtime + do_task_delta_exec(p, rq);
	task_rq_unlock(rq, &flags);

	return ns;
}

/*
 * Return sum_exec_runtime for the thread group.
 * In case the task is currently running, return the sum plus current's
 * pending runtime that have not been accounted yet.
 *
 * Note that the thread group might have other running tasks as well,
 * so the return value not includes other pending runtime that other
 * running tasks might have.
 */
unsigned long long thread_group_sched_runtime(struct task_struct *p)
{
	struct task_cputime totals;
	unsigned long flags;
	struct rq *rq;
	u64 ns;

	rq = task_rq_lock(p, &flags);
	thread_group_cputime(p, &totals);
	ns = totals.sum_exec_runtime + do_task_delta_exec(p, rq);
	task_rq_unlock(rq, &flags);

	return ns;
}

/*
 * Account user cpu time to a process.
 * @p: the process that the cpu time gets accounted to
 * @cputime: the cpu time spent in user space since the last update
 * @cputime_scaled: cputime scaled by cpu frequency
 */
void account_user_time(struct task_struct *p, cputime_t cputime,
		       cputime_t cputime_scaled)
{
	struct cpu_usage_stat *cpustat = &kstat_this_cpu.cpustat;
	cputime64_t tmp;

	/* Add user time to process. */
	p->utime = cputime_add(p->utime, cputime);
	p->utimescaled = cputime_add(p->utimescaled, cputime_scaled);
	account_group_user_time(p, cputime);

	/* Add user time to cpustat. */
	tmp = cputime_to_cputime64(cputime);
	if (TASK_NICE(p) > 0)
		cpustat->nice = cputime64_add(cpustat->nice, tmp);
	else
		cpustat->user = cputime64_add(cpustat->user, tmp);

	cpuacct_update_stats(p, CPUACCT_STAT_USER, cputime);
	/* Account for user time used */
	acct_update_integrals(p);
}

/*
 * Account guest cpu time to a process.
 * @p: the process that the cpu time gets accounted to
 * @cputime: the cpu time spent in virtual machine since the last update
 * @cputime_scaled: cputime scaled by cpu frequency
 */
static void account_guest_time(struct task_struct *p, cputime_t cputime,
			       cputime_t cputime_scaled)
{
	cputime64_t tmp;
	struct cpu_usage_stat *cpustat = &kstat_this_cpu.cpustat;

	tmp = cputime_to_cputime64(cputime);

	/* Add guest time to process. */
	p->utime = cputime_add(p->utime, cputime);
	p->utimescaled = cputime_add(p->utimescaled, cputime_scaled);
	account_group_user_time(p, cputime);
	p->gtime = cputime_add(p->gtime, cputime);

	/* Add guest time to cpustat. */
	if (TASK_NICE(p) > 0) {
		cpustat->nice = cputime64_add(cpustat->nice, tmp);
		cpustat->guest_nice = cputime64_add(cpustat->guest_nice, tmp);
	} else {
		cpustat->user = cputime64_add(cpustat->user, tmp);
		cpustat->guest = cputime64_add(cpustat->guest, tmp);
	}
}

/*
 * Account system cpu time to a process.
 * @p: the process that the cpu time gets accounted to
 * @hardirq_offset: the offset to subtract from hardirq_count()
 * @cputime: the cpu time spent in kernel space since the last update
 * @cputime_scaled: cputime scaled by cpu frequency
 */
void account_system_time(struct task_struct *p, int hardirq_offset,
			 cputime_t cputime, cputime_t cputime_scaled)
{
	struct cpu_usage_stat *cpustat = &kstat_this_cpu.cpustat;
	cputime64_t tmp;

	if ((p->flags & PF_VCPU) && (irq_count() - hardirq_offset == 0)) {
		account_guest_time(p, cputime, cputime_scaled);
		return;
	}

	/* Add system time to process. */
	p->stime = cputime_add(p->stime, cputime);
	p->stimescaled = cputime_add(p->stimescaled, cputime_scaled);
	account_group_system_time(p, cputime);

	/* Add system time to cpustat. */
	tmp = cputime_to_cputime64(cputime);
	if (hardirq_count() - hardirq_offset)
		cpustat->irq = cputime64_add(cpustat->irq, tmp);
	else if (softirq_count())
		cpustat->softirq = cputime64_add(cpustat->softirq, tmp);
	else
		cpustat->system = cputime64_add(cpustat->system, tmp);

	cpuacct_update_stats(p, CPUACCT_STAT_SYSTEM, cputime);

	/* Account for system time used */
	acct_update_integrals(p);
}

/*
 * Account for involuntary wait time.
 * @steal: the cpu time spent in involuntary wait
 */
void account_steal_time(cputime_t cputime)
{
	struct cpu_usage_stat *cpustat = &kstat_this_cpu.cpustat;
	cputime64_t cputime64 = cputime_to_cputime64(cputime);

	cpustat->steal = cputime64_add(cpustat->steal, cputime64);
}

/*
 * Account for idle time.
 * @cputime: the cpu time spent in idle wait
 */
void account_idle_time(cputime_t cputime)
{
	struct cpu_usage_stat *cpustat = &kstat_this_cpu.cpustat;
	cputime64_t cputime64 = cputime_to_cputime64(cputime);
	struct rq *rq = this_rq();

	if (atomic_read(&rq->nr_iowait) > 0)
		cpustat->iowait = cputime64_add(cpustat->iowait, cputime64);
	else
		cpustat->idle = cputime64_add(cpustat->idle, cputime64);
}

#ifndef CONFIG_VIRT_CPU_ACCOUNTING

/*
 * Account a single tick of cpu time.
 * @p: the process that the cpu time gets accounted to
 * @user_tick: indicates if the tick is a user or a system tick
 */
void account_process_tick(struct task_struct *p, int user_tick)
{
	cputime_t one_jiffy_scaled = cputime_to_scaled(cputime_one_jiffy);
	struct rq *rq = this_rq();

	if (user_tick)
		account_user_time(p, cputime_one_jiffy, one_jiffy_scaled);
	else if ((p != rq->idle) || (irq_count() != HARDIRQ_OFFSET))
		account_system_time(p, HARDIRQ_OFFSET, cputime_one_jiffy,
				    one_jiffy_scaled);
	else
		account_idle_time(cputime_one_jiffy);
}

/*
 * Account multiple ticks of steal time.
 * @p: the process from which the cpu time has been stolen
 * @ticks: number of stolen ticks
 */
void account_steal_ticks(unsigned long ticks)
{
	account_steal_time(jiffies_to_cputime(ticks));
}

/*
 * Account multiple ticks of idle time.
 * @ticks: number of stolen ticks
 */
void account_idle_ticks(unsigned long ticks)
{
	account_idle_time(jiffies_to_cputime(ticks));
}

#endif

/*
 * Use precise platform statistics if available:
 */
#ifdef CONFIG_VIRT_CPU_ACCOUNTING
void task_times(struct task_struct *p, cputime_t *ut, cputime_t *st)
{
	*ut = p->utime;
	*st = p->stime;
}

void thread_group_times(struct task_struct *p, cputime_t *ut, cputime_t *st)
{
	struct task_cputime cputime;

	thread_group_cputime(p, &cputime);

	*ut = cputime.utime;
	*st = cputime.stime;
}
#else

#ifndef nsecs_to_cputime
# define nsecs_to_cputime(__nsecs)	nsecs_to_jiffies(__nsecs)
#endif

void task_times(struct task_struct *p, cputime_t *ut, cputime_t *st)
{
	cputime_t rtime, utime = p->utime, total = cputime_add(utime, p->stime);

	/*
	 * Use CFS's precise accounting:
	 */
	rtime = nsecs_to_cputime(p->se.sum_exec_runtime);

	if (total) {
		u64 temp = rtime;

		temp *= utime;
		do_div(temp, total);
		utime = (cputime_t)temp;
	} else
		utime = rtime;

	/*
	 * Compare with previous values, to keep monotonicity:
	 */
	p->prev_utime = max(p->prev_utime, utime);
	p->prev_stime = max(p->prev_stime, cputime_sub(rtime, p->prev_utime));

	*ut = p->prev_utime;
	*st = p->prev_stime;
}

/*
 * Must be called with siglock held.
 */
void thread_group_times(struct task_struct *p, cputime_t *ut, cputime_t *st)
{
	struct signal_struct *sig = p->signal;
	struct task_cputime cputime;
	cputime_t rtime, utime, total;

	thread_group_cputime(p, &cputime);

	total = cputime_add(cputime.utime, cputime.stime);
	rtime = nsecs_to_cputime(cputime.sum_exec_runtime);

	if (total) {
		u64 temp = rtime;

		temp *= cputime.utime;
		do_div(temp, total);
		utime = (cputime_t)temp;
	} else
		utime = rtime;

	sig->prev_utime = max(sig->prev_utime, utime);
	sig->prev_stime = max(sig->prev_stime,
			      cputime_sub(rtime, sig->prev_utime));

	*ut = sig->prev_utime;
	*st = sig->prev_stime;
}
#endif

/*
 * This function gets called by the timer code, with HZ frequency.
 * We call it with interrupts disabled.
 *
 * It also gets called by the fork code, when changing the parent's
 * timeslices.
 */
void scheduler_tick(void)
{
	int cpu = smp_processor_id();
	struct rq *rq = cpu_rq(cpu);
	struct task_struct *curr = rq->curr;

#if defined(BUZZZ_KEVT_LVL) && (BUZZZ_KEVT_LVL >= 1)
	buzzz_kevt_log1(BUZZZ_KEVT_ID_SCHED_TICK, jiffies);
#endif	/* BUZZZ_KEVT_LVL */

	sched_clock_tick();

	raw_spin_lock(&rq->lock);
	update_rq_clock(rq);
	update_cpu_load_active(rq);
	curr->sched_class->task_tick(rq, curr, 0);
	raw_spin_unlock(&rq->lock);

	perf_event_task_tick(curr);

#ifdef CONFIG_SMP
	rq->idle_at_tick = idle_cpu(cpu);
	trigger_load_balance(rq, cpu);
#endif
}

notrace unsigned long get_parent_ip(unsigned long addr)
{
	if (in_lock_functions(addr)) {
		addr = CALLER_ADDR2;
		if (in_lock_functions(addr))
			addr = CALLER_ADDR3;
	}
	return addr;
}

#if defined(CONFIG_PREEMPT) && (defined(CONFIG_DEBUG_PREEMPT) || \
	defined(CONFIG_PREEMPT_TRACER))

void __kprobes add_preempt_count(int val)
{
#ifdef CONFIG_DEBUG_PREEMPT
	/*
	 * Underflow?
	 */
	if (DEBUG_LOCKS_WARN_ON((preempt_count() < 0)))
		return;
#endif
	preempt_count() += val;
#ifdef CONFIG_DEBUG_PREEMPT
	/*
	 * Spinlock count overflowing soon?
	 */
	DEBUG_LOCKS_WARN_ON((preempt_count() & PREEMPT_MASK) >=
				PREEMPT_MASK - 10);
#endif
	if (preempt_count() == val)
		trace_preempt_off(CALLER_ADDR0, get_parent_ip(CALLER_ADDR1));
}
EXPORT_SYMBOL(add_preempt_count);

void __kprobes sub_preempt_count(int val)
{
#ifdef CONFIG_DEBUG_PREEMPT
	/*
	 * Underflow?
	 */
	if (DEBUG_LOCKS_WARN_ON(val > preempt_count()))
		return;
	/*
	 * Is the spinlock portion underflowing?
	 */
	if (DEBUG_LOCKS_WARN_ON((val < PREEMPT_MASK) &&
			!(preempt_count() & PREEMPT_MASK)))
		return;
#endif

	if (preempt_count() == val)
		trace_preempt_on(CALLER_ADDR0, get_parent_ip(CALLER_ADDR1));
	preempt_count() -= val;
}
EXPORT_SYMBOL(sub_preempt_count);

#endif

/*
 * Print scheduling while atomic bug:
 */
static noinline void __schedule_bug(struct task_struct *prev)
{
	struct pt_regs *regs = get_irq_regs();

	printk(KERN_ERR "BUG: scheduling while atomic: %s/%d/0x%08x\n",
		prev->comm, prev->pid, preempt_count());

	debug_show_held_locks(prev);
	print_modules();
	if (irqs_disabled())
		print_irqtrace_events(prev);

	if (regs)
		show_regs(regs);
	else
		dump_stack();
}

/*
 * Various schedule()-time debugging checks and statistics:
 */
static inline void schedule_debug(struct task_struct *prev)
{
	/*
	 * Test if we are atomic. Since do_exit() needs to call into
	 * schedule() atomically, we ignore that path for now.
	 * Otherwise, whine if we are scheduling when we should not be.
	 */
	if (unlikely(in_atomic_preempt_off() && !prev->exit_state))
		__schedule_bug(prev);

	profile_hit(SCHED_PROFILING, __builtin_return_address(0));

	schedstat_inc(this_rq(), sched_count);
#ifdef CONFIG_SCHEDSTATS
	if (unlikely(prev->lock_depth >= 0)) {
		schedstat_inc(this_rq(), bkl_count);
		schedstat_inc(prev, sched_info.bkl_count);
	}
#endif
}

static void put_prev_task(struct rq *rq, struct task_struct *prev)
{
	if (prev->se.on_rq)
		update_rq_clock(rq);
	prev->sched_class->put_prev_task(rq, prev);
}

/*
 * Pick up the highest-prio task:
 */
static inline struct task_struct *
pick_next_task(struct rq *rq)
{
	const struct sched_class *class;
	struct task_struct *p;

	/*
	 * Optimization: we know that if all tasks are in
	 * the fair class we can call that function directly:
	 */
	if (likely(rq->nr_running == rq->cfs.nr_running)) {
		p = fair_sched_class.pick_next_task(rq);
		if (likely(p))
			return p;
	}

	class = sched_class_highest;
	for ( ; ; ) {
		p = class->pick_next_task(rq);
		if (p)
			return p;
		/*
		 * Will never be NULL as the idle class always
		 * returns a non-NULL p:
		 */
		class = class->next;
	}
}

/*
 * schedule() is the main scheduler function.
 */
asmlinkage void __sched schedule(void)
{
	struct task_struct *prev, *next;
	unsigned long *switch_count;
	struct rq *rq;
	int cpu;

need_resched:
	preempt_disable();
	cpu = smp_processor_id();
	rq = cpu_rq(cpu);
	rcu_note_context_switch(cpu);
	prev = rq->curr;

	release_kernel_lock(prev);
need_resched_nonpreemptible:

	schedule_debug(prev);

	if (sched_feat(HRTICK))
		hrtick_clear(rq);

	raw_spin_lock_irq(&rq->lock);

	switch_count = &prev->nivcsw;
	if (prev->state && !(preempt_count() & PREEMPT_ACTIVE)) {
		if (unlikely(signal_pending_state(prev->state, prev))) {
			prev->state = TASK_RUNNING;
		} else {
			/*
			 * If a worker is going to sleep, notify and
			 * ask workqueue whether it wants to wake up a
			 * task to maintain concurrency.  If so, wake
			 * up the task.
			 */
			if (prev->flags & PF_WQ_WORKER) {
				struct task_struct *to_wakeup;

				to_wakeup = wq_worker_sleeping(prev, cpu);
				if (to_wakeup)
					try_to_wake_up_local(to_wakeup);
			}
			deactivate_task(rq, prev, DEQUEUE_SLEEP);
		}
		switch_count = &prev->nvcsw;
	}

	pre_schedule(rq, prev);

	if (unlikely(!rq->nr_running))
		idle_balance(cpu, rq);

	put_prev_task(rq, prev);
	next = pick_next_task(rq);
	clear_tsk_need_resched(prev);
	rq->skip_clock_update = 0;

	if (likely(prev != next)) {
		sched_info_switch(prev, next);
		perf_event_task_sched_out(prev, next);

		rq->nr_switches++;
		rq->curr = next;
		++*switch_count;

#if defined(BUZZZ_KEVT_LVL) && (BUZZZ_KEVT_LVL >= 1)
		buzzz_prev[cpu] = prev;
		buzzz_next[cpu] = next;
#endif	/* BUZZZ_KEVT_LVL */

		context_switch(rq, prev, next); /* unlocks the rq */
		/*
		 * The context switch have flipped the stack from under us
		 * and restored the local variables which were saved when
		 * this task called schedule() in the past. prev == current
		 * is still correct, but it can be moved to another cpu/rq.
		 */
		cpu = smp_processor_id();
		rq = cpu_rq(cpu);

#if defined(BUZZZ_KEVT_LVL) && (BUZZZ_KEVT_LVL >= 1)
		buzzz_kevt_log2(BUZZZ_KEVT_ID_SCHEDULE,
			            (uint32_t)buzzz_prev[cpu], (uint32_t)buzzz_next[cpu]);
#endif	/* BUZZZ_KEVT_LVL */

	} else
		raw_spin_unlock_irq(&rq->lock);

	post_schedule(rq);

	if (unlikely(reacquire_kernel_lock(prev)))
		goto need_resched_nonpreemptible;

	preempt_enable_no_resched();
	if (need_resched())
		goto need_resched;
}
EXPORT_SYMBOL(schedule);

#ifdef CONFIG_MUTEX_SPIN_ON_OWNER
/*
 * Look out! "owner" is an entirely speculative pointer
 * access and not reliable.
 */
int mutex_spin_on_owner(struct mutex *lock, struct thread_info *owner)
{
	unsigned int cpu;
	struct rq *rq;

	if (!sched_feat(OWNER_SPIN))
		return 0;

#ifdef CONFIG_DEBUG_PAGEALLOC
	/*
	 * Need to access the cpu field knowing that
	 * DEBUG_PAGEALLOC could have unmapped it if
	 * the mutex owner just released it and exited.
	 */
	if (probe_kernel_address(&owner->cpu, cpu))
		return 0;
#else
	cpu = owner->cpu;
#endif

	/*
	 * Even if the access succeeded (likely case),
	 * the cpu field may no longer be valid.
	 */
	if (cpu >= nr_cpumask_bits)
		return 0;

	/*
	 * We need to validate that we can do a
	 * get_cpu() and that we have the percpu area.
	 */
	if (!cpu_online(cpu))
		return 0;

	rq = cpu_rq(cpu);

	for (;;) {
		/*
		 * Owner changed, break to re-assess state.
		 */
		if (lock->owner != owner) {
			/*
			 * If the lock has switched to a different owner,
			 * we likely have heavy contention. Return 0 to quit
			 * optimistic spinning and not contend further:
			 */
			if (lock->owner)
				return 0;
			break;
		}

		/*
		 * Is that owner really running on that cpu?
		 */
		if (task_thread_info(rq->curr) != owner || need_resched())
			return 0;

		cpu_relax();
	}

	return 1;
}
#endif

#ifdef CONFIG_PREEMPT
/*
 * this is the entry point to schedule() from in-kernel preemption
 * off of preempt_enable. Kernel preemptions off return from interrupt
 * occur there and call schedule directly.
 */
asmlinkage void __sched notrace preempt_schedule(void)
{
	struct thread_info *ti = current_thread_info();

	/*
	 * If there is a non-zero preempt_count or interrupts are disabled,
	 * we do not want to preempt the current task. Just return..
	 */
	if (likely(ti->preempt_count || irqs_disabled()))
		return;

	do {
		add_preempt_count_notrace(PREEMPT_ACTIVE);
		schedule();
		sub_preempt_count_notrace(PREEMPT_ACTIVE);

		/*
		 * Check again in case we missed a preemption opportunity
		 * between schedule and now.
		 */
		barrier();
	} while (need_resched());
}
EXPORT_SYMBOL(preempt_schedule);

/*
 * this is the entry point to schedule() from kernel preemption
 * off of irq context.
 * Note, that this is called and return with irqs disabled. This will
 * protect us against recursive calling from irq.
 */
asmlinkage void __sched preempt_schedule_irq(void)
{
	struct thread_info *ti = current_thread_info();

	/* Catch callers which need to be fixed */
	BUG_ON(ti->preempt_count || !irqs_disabled());

	do {
		add_preempt_count(PREEMPT_ACTIVE);
		local_irq_enable();
		schedule();
		local_irq_disable();
		sub_preempt_count(PREEMPT_ACTIVE);

		/*
		 * Check again in case we missed a preemption opportunity
		 * between schedule and now.
		 */
		barrier();
	} while (need_resched());
}

#endif /* CONFIG_PREEMPT */

int default_wake_function(wait_queue_t *curr, unsigned mode, int wake_flags,
			  void *key)
{
	return try_to_wake_up(curr->private, mode, wake_flags);
}
EXPORT_SYMBOL(default_wake_function);

/*
 * The core wakeup function. Non-exclusive wakeups (nr_exclusive == 0) just
 * wake everything up. If it's an exclusive wakeup (nr_exclusive == small +ve
 * number) then we wake all the non-exclusive tasks and one exclusive task.
 *
 * There are circumstances in which we can try to wake a task which has already
 * started to run but is not in state TASK_RUNNING. try_to_wake_up() returns
 * zero in this (rare) case, and we handle it by continuing to scan the queue.
 */
static void __wake_up_common(wait_queue_head_t *q, unsigned int mode,
			int nr_exclusive, int wake_flags, void *key)
{
	wait_queue_t *curr, *next;

	list_for_each_entry_safe(curr, next, &q->task_list, task_list) {
		unsigned flags = curr->flags;

		if (curr->func(curr, mode, wake_flags, key) &&
				(flags & WQ_FLAG_EXCLUSIVE) && !--nr_exclusive)
			break;
	}
}

/**
 * __wake_up - wake up threads blocked on a waitqueue.
 * @q: the waitqueue
 * @mode: which threads
 * @nr_exclusive: how many wake-one or wake-many threads to wake up
 * @key: is directly passed to the wakeup function
 *
 * It may be assumed that this function implies a write memory barrier before
 * changing the task state if and only if any tasks are woken up.
 */
void __wake_up(wait_queue_head_t *q, unsigned int mode,
			int nr_exclusive, void *key)
{
	unsigned long flags;

	spin_lock_irqsave(&q->lock, flags);
	__wake_up_common(q, mode, nr_exclusive, 0, key);
	spin_unlock_irqrestore(&q->lock, flags);
}
EXPORT_SYMBOL(__wake_up);

/*
 * Same as __wake_up but called with the spinlock in wait_queue_head_t held.
 */
void __wake_up_locked(wait_queue_head_t *q, unsigned int mode)
{
	__wake_up_common(q, mode, 1, 0, NULL);
}
EXPORT_SYMBOL_GPL(__wake_up_locked);

void __wake_up_locked_key(wait_queue_head_t *q, unsigned int mode, void *key)
{
	__wake_up_common(q, mode, 1, 0, key);
}

/**
 * __wake_up_sync_key - wake up threads blocked on a waitqueue.
 * @q: the waitqueue
 * @mode: which threads
 * @nr_exclusive: how many wake-one or wake-many threads to wake up
 * @key: opaque value to be passed to wakeup targets
 *
 * The sync wakeup differs that the waker knows that it will schedule
 * away soon, so while the target thread will be woken up, it will not
 * be migrated to another CPU - ie. the two threads are 'synchronized'
 * with each other. This can prevent needless bouncing between CPUs.
 *
 * On UP it can prevent extra preemption.
 *
 * It may be assumed that this function implies a write memory barrier before
 * changing the task state if and only if any tasks are woken up.
 */
void __wake_up_sync_key(wait_queue_head_t *q, unsigned int mode,
			int nr_exclusive, void *key)
{
	unsigned long flags;
	int wake_flags = WF_SYNC;

	if (unlikely(!q))
		return;

	if (unlikely(!nr_exclusive))
		wake_flags = 0;

	spin_lock_irqsave(&q->lock, flags);
	__wake_up_common(q, mode, nr_exclusive, wake_flags, key);
	spin_unlock_irqrestore(&q->lock, flags);
}
EXPORT_SYMBOL_GPL(__wake_up_sync_key);

/*
 * __wake_up_sync - see __wake_up_sync_key()
 */
void __wake_up_sync(wait_queue_head_t *q, unsigned int mode, int nr_exclusive)
{
	__wake_up_sync_key(q, mode, nr_exclusive, NULL);
}
EXPORT_SYMBOL_GPL(__wake_up_sync);	/* For internal use only */

/**
 * complete: - signals a single thread waiting on this completion
 * @x:  holds the state of this particular completion
 *
 * This will wake up a single thread waiting on this completion. Threads will be
 * awakened in the same order in which they were queued.
 *
 * See also complete_all(), wait_for_completion() and related routines.
 *
 * It may be assumed that this function implies a write memory barrier before
 * changing the task state if and only if any tasks are woken up.
 */
void complete(struct completion *x)
{
	unsigned long flags;

	spin_lock_irqsave(&x->wait.lock, flags);
	x->done++;
	__wake_up_common(&x->wait, TASK_NORMAL, 1, 0, NULL);
	spin_unlock_irqrestore(&x->wait.lock, flags);
}
EXPORT_SYMBOL(complete);

/**
 * complete_all: - signals all threads waiting on this completion
 * @x:  holds the state of this particular completion
 *
 * This will wake up all threads waiting on this particular completion event.
 *
 * It may be assumed that this function implies a write memory barrier before
 * changing the task state if and only if any tasks are woken up.
 */
void complete_all(struct completion *x)
{
	unsigned long flags;

	spin_lock_irqsave(&x->wait.lock, flags);
	x->done += UINT_MAX/2;
	__wake_up_common(&x->wait, TASK_NORMAL, 0, 0, NULL);
	spin_unlock_irqrestore(&x->wait.lock, flags);
}
EXPORT_SYMBOL(complete_all);

static inline long __sched
do_wait_for_common(struct completion *x, long timeout, int state)
{
	if (!x->done) {
		DECLARE_WAITQUEUE(wait, current);

		__add_wait_queue_tail_exclusive(&x->wait, &wait);
		do {
			if (signal_pending_state(state, current)) {
				timeout = -ERESTARTSYS;
				break;
			}
			__set_current_state(state);
			spin_unlock_irq(&x->wait.lock);
			timeout = schedule_timeout(timeout);
			spin_lock_irq(&x->wait.lock);
		} while (!x->done && timeout);
		__remove_wait_queue(&x->wait, &wait);
		if (!x->done)
			return timeout;
	}
	x->done--;
	return timeout ?: 1;
}

static long __sched
wait_for_common(struct completion *x, long timeout, int state)
{
	might_sleep();

	spin_lock_irq(&x->wait.lock);
	timeout = do_wait_for_common(x, timeout, state);
	spin_unlock_irq(&x->wait.lock);
	return timeout;
}

/**
 * wait_for_completion: - waits for completion of a task
 * @x:  holds the state of this particular completion
 *
 * This waits to be signaled for completion of a specific task. It is NOT
 * interruptible and there is no timeout.
 *
 * See also similar routines (i.e. wait_for_completion_timeout()) with timeout
 * and interrupt capability. Also see complete().
 */
void __sched wait_for_completion(struct completion *x)
{
	wait_for_common(x, MAX_SCHEDULE_TIMEOUT, TASK_UNINTERRUPTIBLE);
}
EXPORT_SYMBOL(wait_for_completion);

/**
 * wait_for_completion_timeout: - waits for completion of a task (w/timeout)
 * @x:  holds the state of this particular completion
 * @timeout:  timeout value in jiffies
 *
 * This waits for either a completion of a specific task to be signaled or for a
 * specified timeout to expire. The timeout is in jiffies. It is not
 * interruptible.
 */
unsigned long __sched
wait_for_completion_timeout(struct completion *x, unsigned long timeout)
{
	return wait_for_common(x, timeout, TASK_UNINTERRUPTIBLE);
}
EXPORT_SYMBOL(wait_for_completion_timeout);

/**
 * wait_for_completion_interruptible: - waits for completion of a task (w/intr)
 * @x:  holds the state of this particular completion
 *
 * This waits for completion of a specific task to be signaled. It is
 * interruptible.
 */
int __sched wait_for_completion_interruptible(struct completion *x)
{
	long t = wait_for_common(x, MAX_SCHEDULE_TIMEOUT, TASK_INTERRUPTIBLE);
	if (t == -ERESTARTSYS)
		return t;
	return 0;
}
EXPORT_SYMBOL(wait_for_completion_interruptible);

/**
 * wait_for_completion_interruptible_timeout: - waits for completion (w/(to,intr))
 * @x:  holds the state of this particular completion
 * @timeout:  timeout value in jiffies
 *
 * This waits for either a completion of a specific task to be signaled or for a
 * specified timeout to expire. It is interruptible. The timeout is in jiffies.
 */
unsigned long __sched
wait_for_completion_interruptible_timeout(struct completion *x,
					  unsigned long timeout)
{
	return wait_for_common(x, timeout, TASK_INTERRUPTIBLE);
}
EXPORT_SYMBOL(wait_for_completion_interruptible_timeout);

/**
 * wait_for_completion_killable: - waits for completion of a task (killable)
 * @x:  holds the state of this particular completion
 *
 * This waits to be signaled for completion of a specific task. It can be
 * interrupted by a kill signal.
 */
int __sched wait_for_completion_killable(struct completion *x)
{
	long t = wait_for_common(x, MAX_SCHEDULE_TIMEOUT, TASK_KILLABLE);
	if (t == -ERESTARTSYS)
		return t;
	return 0;
}
EXPORT_SYMBOL(wait_for_completion_killable);

/**
 * wait_for_completion_killable_timeout: - waits for completion of a task (w/(to,killable))
 * @x:  holds the state of this particular completion
 * @timeout:  timeout value in jiffies
 *
 * This waits for either a completion of a specific task to be
 * signaled or for a specified timeout to expire. It can be
 * interrupted by a kill signal. The timeout is in jiffies.
 */
unsigned long __sched
wait_for_completion_killable_timeout(struct completion *x,
				     unsigned long timeout)
{
	return wait_for_common(x, timeout, TASK_KILLABLE);
}
EXPORT_SYMBOL(wait_for_completion_killable_timeout);

/**
 *	try_wait_for_completion - try to decrement a completion without blocking
 *	@x:	completion structure
 *
 *	Returns: 0 if a decrement cannot be done without blocking
 *		 1 if a decrement succeeded.
 *
 *	If a completion is being used as a counting completion,
 *	attempt to decrement the counter without blocking. This
 *	enables us to avoid waiting if the resource the completion
 *	is protecting is not available.
 */
bool try_wait_for_completion(struct completion *x)
{
	unsigned long flags;
	int ret = 1;

	spin_lock_irqsave(&x->wait.lock, flags);
	if (!x->done)
		ret = 0;
	else
		x->done--;
	spin_unlock_irqrestore(&x->wait.lock, flags);
	return ret;
}
EXPORT_SYMBOL(try_wait_for_completion);

/**
 *	completion_done - Test to see if a completion has any waiters
 *	@x:	completion structure
 *
 *	Returns: 0 if there are waiters (wait_for_completion() in progress)
 *		 1 if there are no waiters.
 *
 */
bool completion_done(struct completion *x)
{
	unsigned long flags;
	int ret = 1;

	spin_lock_irqsave(&x->wait.lock, flags);
	if (!x->done)
		ret = 0;
	spin_unlock_irqrestore(&x->wait.lock, flags);
	return ret;
}
EXPORT_SYMBOL(completion_done);

static long __sched
sleep_on_common(wait_queue_head_t *q, int state, long timeout)
{
	unsigned long flags;
	wait_queue_t wait;

	init_waitqueue_entry(&wait, current);

	__set_current_state(state);

	spin_lock_irqsave(&q->lock, flags);
	__add_wait_queue(q, &wait);
	spin_unlock(&q->lock);
	timeout = schedule_timeout(timeout);
	spin_lock_irq(&q->lock);
	__remove_wait_queue(q, &wait);
	spin_unlock_irqrestore(&q->lock, flags);

	return timeout;
}

void __sched interruptible_sleep_on(wait_queue_head_t *q)
{
	sleep_on_common(q, TASK_INTERRUPTIBLE, MAX_SCHEDULE_TIMEOUT);
}
EXPORT_SYMBOL(interruptible_sleep_on);

long __sched
interruptible_sleep_on_timeout(wait_queue_head_t *q, long timeout)
{
	return sleep_on_common(q, TASK_INTERRUPTIBLE, timeout);
}
EXPORT_SYMBOL(interruptible_sleep_on_timeout);

void __sched sleep_on(wait_queue_head_t *q)
{
	sleep_on_common(q, TASK_UNINTERRUPTIBLE, MAX_SCHEDULE_TIMEOUT);
}
EXPORT_SYMBOL(sleep_on);

long __sched sleep_on_timeout(wait_queue_head_t *q, long timeout)
{
	return sleep_on_common(q, TASK_UNINTERRUPTIBLE, timeout);
}
EXPORT_SYMBOL(sleep_on_timeout);

#ifdef CONFIG_RT_MUTEXES

/*
 * rt_mutex_setprio - set the current priority of a task
 * @p: task
 * @prio: prio value (kernel-internal form)
 *
 * This function changes the 'effective' priority of a task. It does
 * not touch ->normal_prio like __setscheduler().
 *
 * Used by the rt_mutex code to implement priority inheritance logic.
 */
void rt_mutex_setprio(struct task_struct *p, int prio)
{
	unsigned long flags;
	int oldprio, on_rq, running;
	struct rq *rq;
	const struct sched_class *prev_class;

	BUG_ON(prio < 0 || prio > MAX_PRIO);

	rq = task_rq_lock(p, &flags);

	oldprio = p->prio;
	prev_class = p->sched_class;
	on_rq = p->se.on_rq;
	running = task_current(rq, p);
	if (on_rq)
		dequeue_task(rq, p, 0);
	if (running)
		p->sched_class->put_prev_task(rq, p);

	if (rt_prio(prio))
		p->sched_class = &rt_sched_class;
	else
		p->sched_class = &fair_sched_class;

	p->prio = prio;

	if (running)
		p->sched_class->set_curr_task(rq);
	if (on_rq) {
		enqueue_task(rq, p, oldprio < prio ? ENQUEUE_HEAD : 0);

		check_class_changed(rq, p, prev_class, oldprio, running);
	}
	task_rq_unlock(rq, &flags);
}

#endif

void set_user_nice(struct task_struct *p, long nice)
{
	int old_prio, delta, on_rq;
	unsigned long flags;
	struct rq *rq;

	if (TASK_NICE(p) == nice || nice < -20 || nice > 19)
		return;
	/*
	 * We have to be careful, if called from sys_setpriority(),
	 * the task might be in the middle of scheduling on another CPU.
	 */
	rq = task_rq_lock(p, &flags);
	/*
	 * The RT priorities are set via sched_setscheduler(), but we still
	 * allow the 'normal' nice value to be set - but as expected
	 * it wont have any effect on scheduling until the task is
	 * SCHED_FIFO/SCHED_RR:
	 */
	if (task_has_rt_policy(p)) {
		p->static_prio = NICE_TO_PRIO(nice);
		goto out_unlock;
	}
	on_rq = p->se.on_rq;
	if (on_rq)
		dequeue_task(rq, p, 0);

	p->static_prio = NICE_TO_PRIO(nice);
	set_load_weight(p);
	old_prio = p->prio;
	p->prio = effective_prio(p);
	delta = p->prio - old_prio;

	if (on_rq) {
		enqueue_task(rq, p, 0);
		/*
		 * If the task increased its priority or is running and
		 * lowered its priority, then reschedule its CPU:
		 */
		if (delta < 0 || (delta > 0 && task_running(rq, p)))
			resched_task(rq->curr);
	}
out_unlock:
	task_rq_unlock(rq, &flags);
}
EXPORT_SYMBOL(set_user_nice);

/*
 * can_nice - check if a task can reduce its nice value
 * @p: task
 * @nice: nice value
 */
int can_nice(const struct task_struct *p, const int nice)
{
	/* convert nice value [19,-20] to rlimit style value [1,40] */
	int nice_rlim = 20 - nice;

	return (nice_rlim <= task_rlimit(p, RLIMIT_NICE) ||
		capable(CAP_SYS_NICE));
}

#ifdef __ARCH_WANT_SYS_NICE

/*
 * sys_nice - change the priority of the current process.
 * @increment: priority increment
 *
 * sys_setpriority is a more generic, but much slower function that
 * does similar things.
 */
SYSCALL_DEFINE1(nice, int, increment)
{
	long nice, retval;

	/*
	 * Setpriority might change our priority at the same moment.
	 * We don't have to worry. Conceptually one call occurs first
	 * and we have a single winner.
	 */
	if (increment < -40)
		increment = -40;
	if (increment > 40)
		increment = 40;

	nice = TASK_NICE(current) + increment;
	if (nice < -20)
		nice = -20;
	if (nice > 19)
		nice = 19;

	if (increment < 0 && !can_nice(current, nice))
		return -EPERM;

	retval = security_task_setnice(current, nice);
	if (retval)
		return retval;

	set_user_nice(current, nice);
	return 0;
}

#endif

/**
 * task_prio - return the priority value of a given task.
 * @p: the task in question.
 *
 * This is the priority value as seen by users in /proc.
 * RT tasks are offset by -200. Normal tasks are centered
 * around 0, value goes from -16 to +15.
 */
int task_prio(const struct task_struct *p)
{
	return p->prio - MAX_RT_PRIO;
}

/**
 * task_nice - return the nice value of a given task.
 * @p: the task in question.
 */
int task_nice(const struct task_struct *p)
{
	return TASK_NICE(p);
}
EXPORT_SYMBOL(task_nice);

/**
 * idle_cpu - is a given cpu idle currently?
 * @cpu: the processor in question.
 */
int idle_cpu(int cpu)
{
	return cpu_curr(cpu) == cpu_rq(cpu)->idle;
}

/**
 * idle_task - return the idle task for a given cpu.
 * @cpu: the processor in question.
 */
struct task_struct *idle_task(int cpu)
{
	return cpu_rq(cpu)->idle;
}

/**
 * find_process_by_pid - find a process with a matching PID value.
 * @pid: the pid in question.
 */
static struct task_struct *find_process_by_pid(pid_t pid)
{
	return pid ? find_task_by_vpid(pid) : current;
}

/* Actually do priority change: must hold rq lock. */
static void
__setscheduler(struct rq *rq, struct task_struct *p, int policy, int prio)
{
	BUG_ON(p->se.on_rq);

	p->policy = policy;
	p->rt_priority = prio;
	p->normal_prio = normal_prio(p);
	/* we are holding p->pi_lock already */
	p->prio = rt_mutex_getprio(p);
	if (rt_prio(p->prio))
		p->sched_class = &rt_sched_class;
	else
		p->sched_class = &fair_sched_class;
	set_load_weight(p);
}

/*
 * check the target process has a UID that matches the current process's
 */
static bool check_same_owner(struct task_struct *p)
{
	const struct cred *cred = current_cred(), *pcred;
	bool match;

	rcu_read_lock();
	pcred = __task_cred(p);
	match = (cred->euid == pcred->euid ||
		 cred->euid == pcred->uid);
	rcu_read_unlock();
	return match;
}

static int __sched_setscheduler(struct task_struct *p, int policy,
				struct sched_param *param, bool user)
{
	int retval, oldprio, oldpolicy = -1, on_rq, running;
	unsigned long flags;
	const struct sched_class *prev_class;
	struct rq *rq;
	int reset_on_fork;

	/* may grab non-irq protected spin_locks */
	BUG_ON(in_interrupt());
recheck:
	/* double check policy once rq lock held */
	if (policy < 0) {
		reset_on_fork = p->sched_reset_on_fork;
		policy = oldpolicy = p->policy;
	} else {
		reset_on_fork = !!(policy & SCHED_RESET_ON_FORK);
		policy &= ~SCHED_RESET_ON_FORK;

		if (policy != SCHED_FIFO && policy != SCHED_RR &&
				policy != SCHED_NORMAL && policy != SCHED_BATCH &&
				policy != SCHED_IDLE)
			return -EINVAL;
	}

	/*
	 * Valid priorities for SCHED_FIFO and SCHED_RR are
	 * 1..MAX_USER_RT_PRIO-1, valid priority for SCHED_NORMAL,
	 * SCHED_BATCH and SCHED_IDLE is 0.
	 */
	if (param->sched_priority < 0 ||
	    (p->mm && param->sched_priority > MAX_USER_RT_PRIO-1) ||
	    (!p->mm && param->sched_priority > MAX_RT_PRIO-1))
		return -EINVAL;
	if (rt_policy(policy) != (param->sched_priority != 0))
		return -EINVAL;

	/*
	 * Allow unprivileged RT tasks to decrease priority:
	 */
	if (user && !capable(CAP_SYS_NICE)) {
		if (rt_policy(policy)) {
			unsigned long rlim_rtprio =
					task_rlimit(p, RLIMIT_RTPRIO);

			/* can't set/change the rt policy */
			if (policy != p->policy && !rlim_rtprio)
				return -EPERM;

			/* can't increase priority */
			if (param->sched_priority > p->rt_priority &&
			    param->sched_priority > rlim_rtprio)
				return -EPERM;
		}
		/*
		 * Like positive nice levels, dont allow tasks to
		 * move out of SCHED_IDLE either:
		 */
		if (p->policy == SCHED_IDLE && policy != SCHED_IDLE)
			return -EPERM;

		/* can't change other user's priorities */
		if (!check_same_owner(p))
			return -EPERM;

		/* Normal users shall not reset the sched_reset_on_fork flag */
		if (p->sched_reset_on_fork && !reset_on_fork)
			return -EPERM;
	}

	if (user) {
		retval = security_task_setscheduler(p, policy, param);
		if (retval)
			return retval;
	}

	/*
	 * make sure no PI-waiters arrive (or leave) while we are
	 * changing the priority of the task:
	 */
	raw_spin_lock_irqsave(&p->pi_lock, flags);
	/*
	 * To be able to change p->policy safely, the apropriate
	 * runqueue lock must be held.
	 */
	rq = __task_rq_lock(p);

#ifdef CONFIG_RT_GROUP_SCHED
	if (user) {
		/*
		 * Do not allow realtime tasks into groups that have no runtime
		 * assigned.
		 */
		if (rt_bandwidth_enabled() && rt_policy(policy) &&
				task_group(p)->rt_bandwidth.rt_runtime == 0) {
			__task_rq_unlock(rq);
			raw_spin_unlock_irqrestore(&p->pi_lock, flags);
			return -EPERM;
		}
	}
#endif

	/* recheck policy now with rq lock held */
	if (unlikely(oldpolicy != -1 && oldpolicy != p->policy)) {
		policy = oldpolicy = -1;
		__task_rq_unlock(rq);
		raw_spin_unlock_irqrestore(&p->pi_lock, flags);
		goto recheck;
	}
	on_rq = p->se.on_rq;
	running = task_current(rq, p);
	if (on_rq)
		deactivate_task(rq, p, 0);
	if (running)
		p->sched_class->put_prev_task(rq, p);

	p->sched_reset_on_fork = reset_on_fork;

	oldprio = p->prio;
	prev_class = p->sched_class;
	__setscheduler(rq, p, policy, param->sched_priority);

	if (running)
		p->sched_class->set_curr_task(rq);
	if (on_rq) {
		activate_task(rq, p, 0);

		check_class_changed(rq, p, prev_class, oldprio, running);
	}
	__task_rq_unlock(rq);
	raw_spin_unlock_irqrestore(&p->pi_lock, flags);

	rt_mutex_adjust_pi(p);

	return 0;
}

/**
 * sched_setscheduler - change the scheduling policy and/or RT priority of a thread.
 * @p: the task in question.
 * @policy: new policy.
 * @param: structure containing the new RT priority.
 *
 * NOTE that the task may be already dead.
 */
int sched_setscheduler(struct task_struct *p, int policy,
		       struct sched_param *param)
{
	return __sched_setscheduler(p, policy, param, true);
}
EXPORT_SYMBOL_GPL(sched_setscheduler);

/**
 * sched_setscheduler_nocheck - change the scheduling policy and/or RT priority of a thread from kernelspace.
 * @p: the task in question.
 * @policy: new policy.
 * @param: structure containing the new RT priority.
 *
 * Just like sched_setscheduler, only don't bother checking if the
 * current context has permission.  For example, this is needed in
 * stop_machine(): we create temporary high priority worker threads,
 * but our caller might not have that capability.
 */
int sched_setscheduler_nocheck(struct task_struct *p, int policy,
			       struct sched_param *param)
{
	return __sched_setscheduler(p, policy, param, false);
}

static int
do_sched_setscheduler(pid_t pid, int policy, struct sched_param __user *param)
{
	struct sched_param lparam;
	struct task_struct *p;
	int retval;

	if (!param || pid < 0)
		return -EINVAL;
	if (copy_from_user(&lparam, param, sizeof(struct sched_param)))
		return -EFAULT;

	rcu_read_lock();
	retval = -ESRCH;
	p = find_process_by_pid(pid);
	if (p != NULL)
		retval = sched_setscheduler(p, policy, &lparam);
	rcu_read_unlock();

	return retval;
}

/**
 * sys_sched_setscheduler - set/change the scheduler policy and RT priority
 * @pid: the pid in question.
 * @policy: new policy.
 * @param: structure containing the new RT priority.
 */
SYSCALL_DEFINE3(sched_setscheduler, pid_t, pid, int, policy,
		struct sched_param __user *, param)
{
	/* negative values for policy are not valid */
	if (policy < 0)
		return -EINVAL;

	return do_sched_setscheduler(pid, policy, param);
}

/**
 * sys_sched_setparam - set/change the RT priority of a thread
 * @pid: the pid in question.
 * @param: structure containing the new RT priority.
 */
SYSCALL_DEFINE2(sched_setparam, pid_t, pid, struct sched_param __user *, param)
{
	return do_sched_setscheduler(pid, -1, param);
}

/**
 * sys_sched_getscheduler - get the policy (scheduling class) of a thread
 * @pid: the pid in question.
 */
SYSCALL_DEFINE1(sched_getscheduler, pid_t, pid)
{
	struct task_struct *p;
	int retval;

	if (pid < 0)
		return -EINVAL;

	retval = -ESRCH;
	rcu_read_lock();
	p = find_process_by_pid(pid);
	if (p) {
		retval = security_task_getscheduler(p);
		if (!retval)
			retval = p->policy
				| (p->sched_reset_on_fork ? SCHED_RESET_ON_FORK : 0);
	}
	rcu_read_unlock();
	return retval;
}

/**
 * sys_sched_getparam - get the RT priority of a thread
 * @pid: the pid in question.
 * @param: structure containing the RT priority.
 */
SYSCALL_DEFINE2(sched_getparam, pid_t, pid, struct sched_param __user *, param)
{
	struct sched_param lp;
	struct task_struct *p;
	int retval;

	if (!param || pid < 0)
		return -EINVAL;

	rcu_read_lock();
	p = find_process_by_pid(pid);
	retval = -ESRCH;
	if (!p)
		goto out_unlock;

	retval = security_task_getscheduler(p);
	if (retval)
		goto out_unlock;

	lp.sched_priority = p->rt_priority;
	rcu_read_unlock();

	/*
	 * This one might sleep, we cannot do it with a spinlock held ...
	 */
	retval = copy_to_user(param, &lp, sizeof(*param)) ? -EFAULT : 0;

	return retval;

out_unlock:
	rcu_read_unlock();
	return retval;
}

long sched_setaffinity(pid_t pid, const struct cpumask *in_mask)
{
	cpumask_var_t cpus_allowed, new_mask;
	struct task_struct *p;
	int retval;

	get_online_cpus();
	rcu_read_lock();

	p = find_process_by_pid(pid);
	if (!p) {
		rcu_read_unlock();
		put_online_cpus();
		return -ESRCH;
	}

	/* Prevent p going away */
	get_task_struct(p);
	rcu_read_unlock();

	if (!alloc_cpumask_var(&cpus_allowed, GFP_KERNEL)) {
		retval = -ENOMEM;
		goto out_put_task;
	}
	if (!alloc_cpumask_var(&new_mask, GFP_KERNEL)) {
		retval = -ENOMEM;
		goto out_free_cpus_allowed;
	}
	retval = -EPERM;
	if (!check_same_owner(p) && !capable(CAP_SYS_NICE))
		goto out_unlock;

	retval = security_task_setscheduler(p, 0, NULL);
	if (retval)
		goto out_unlock;

	cpuset_cpus_allowed(p, cpus_allowed);
	cpumask_and(new_mask, in_mask, cpus_allowed);
 again:
	retval = set_cpus_allowed_ptr(p, new_mask);

	if (!retval) {
		cpuset_cpus_allowed(p, cpus_allowed);
		if (!cpumask_subset(new_mask, cpus_allowed)) {
			/*
			 * We must have raced with a concurrent cpuset
			 * update. Just reset the cpus_allowed to the
			 * cpuset's cpus_allowed
			 */
			cpumask_copy(new_mask, cpus_allowed);
			goto again;
		}
	}
out_unlock:
	free_cpumask_var(new_mask);
out_free_cpus_allowed:
	free_cpumask_var(cpus_allowed);
out_put_task:
	put_task_struct(p);
	put_online_cpus();
	return retval;
}

static int get_user_cpu_mask(unsigned long __user *user_mask_ptr, unsigned len,
			     struct cpumask *new_mask)
{
	if (len < cpumask_size())
		cpumask_clear(new_mask);
	else if (len > cpumask_size())
		len = cpumask_size();

	return copy_from_user(new_mask, user_mask_ptr, len) ? -EFAULT : 0;
}

/**
 * sys_sched_setaffinity - set the cpu affinity of a process
 * @pid: pid of the process
 * @len: length in bytes of the bitmask pointed to by user_mask_ptr
 * @user_mask_ptr: user-space pointer to the new cpu mask
 */
SYSCALL_DEFINE3(sched_setaffinity, pid_t, pid, unsigned int, len,
		unsigned long __user *, user_mask_ptr)
{
	cpumask_var_t new_mask;
	int retval;

	if (!alloc_cpumask_var(&new_mask, GFP_KERNEL))
		return -ENOMEM;

	retval = get_user_cpu_mask(user_mask_ptr, len, new_mask);
	if (retval == 0)
		retval = sched_setaffinity(pid, new_mask);
	free_cpumask_var(new_mask);
	return retval;
}

long sched_getaffinity(pid_t pid, struct cpumask *mask)
{
	struct task_struct *p;
	unsigned long flags;
	struct rq *rq;
	int retval;

	get_online_cpus();
	rcu_read_lock();

	retval = -ESRCH;
	p = find_process_by_pid(pid);
	if (!p)
		goto out_unlock;

	retval = security_task_getscheduler(p);
	if (retval)
		goto out_unlock;

	rq = task_rq_lock(p, &flags);
	cpumask_and(mask, &p->cpus_allowed, cpu_online_mask);
	task_rq_unlock(rq, &flags);

out_unlock:
	rcu_read_unlock();
	put_online_cpus();

	return retval;
}

/**
 * sys_sched_getaffinity - get the cpu affinity of a process
 * @pid: pid of the process
 * @len: length in bytes of the bitmask pointed to by user_mask_ptr
 * @user_mask_ptr: user-space pointer to hold the current cpu mask
 */
SYSCALL_DEFINE3(sched_getaffinity, pid_t, pid, unsigned int, len,
		unsigned long __user *, user_mask_ptr)
{
	int ret;
	cpumask_var_t mask;

	if ((len * BITS_PER_BYTE) < nr_cpu_ids)
		return -EINVAL;
	if (len & (sizeof(unsigned long)-1))
		return -EINVAL;

	if (!alloc_cpumask_var(&mask, GFP_KERNEL))
		return -ENOMEM;

	ret = sched_getaffinity(pid, mask);
	if (ret == 0) {
		size_t retlen = min_t(size_t, len, cpumask_size());

		if (copy_to_user(user_mask_ptr, mask, retlen))
			ret = -EFAULT;
		else
			ret = retlen;
	}
	free_cpumask_var(mask);

	return ret;
}

/**
 * sys_sched_yield - yield the current processor to other threads.
 *
 * This function yields the current CPU to other tasks. If there are no
 * other threads running on this CPU then this function will return.
 */
SYSCALL_DEFINE0(sched_yield)
{
	struct rq *rq = this_rq_lock();

	schedstat_inc(rq, yld_count);
	current->sched_class->yield_task(rq);

	/*
	 * Since we are going to call schedule() anyway, there's
	 * no need to preempt or enable interrupts:
	 */
	__release(rq->lock);
	spin_release(&rq->lock.dep_map, 1, _THIS_IP_);
	do_raw_spin_unlock(&rq->lock);
	preempt_enable_no_resched();

	schedule();

	return 0;
}

static inline int should_resched(void)
{
	return need_resched() && !(preempt_count() & PREEMPT_ACTIVE);
}

static void __cond_resched(void)
{
	add_preempt_count(PREEMPT_ACTIVE);
	schedule();
	sub_preempt_count(PREEMPT_ACTIVE);
}

int __sched _cond_resched(void)
{
	if (should_resched()) {
		__cond_resched();
		return 1;
	}
	return 0;
}
EXPORT_SYMBOL(_cond_resched);

/*
 * __cond_resched_lock() - if a reschedule is pending, drop the given lock,
 * call schedule, and on return reacquire the lock.
 *
 * This works OK both with and without CONFIG_PREEMPT. We do strange low-level
 * operations here to prevent schedule() from being called twice (once via
 * spin_unlock(), once by hand).
 */
int __cond_resched_lock(spinlock_t *lock)
{
	int resched = should_resched();
	int ret = 0;

	lockdep_assert_held(lock);

	if (spin_needbreak(lock) || resched) {
		spin_unlock(lock);
		if (resched)
			__cond_resched();
		else
			cpu_relax();
		ret = 1;
		spin_lock(lock);
	}
	return ret;
}
EXPORT_SYMBOL(__cond_resched_lock);

int __sched __cond_resched_softirq(void)
{
	BUG_ON(!in_softirq());

	if (should_resched()) {
		local_bh_enable();
		__cond_resched();
		local_bh_disable();
		return 1;
	}
	return 0;
}
EXPORT_SYMBOL(__cond_resched_softirq);

/**
 * yield - yield the current processor to other threads.
 *
 * This is a shortcut for kernel-space yielding - it marks the
 * thread runnable and calls sys_sched_yield().
 */
void __sched yield(void)
{
	set_current_state(TASK_RUNNING);
	sys_sched_yield();
}
EXPORT_SYMBOL(yield);

/*
 * This task is about to go to sleep on IO. Increment rq->nr_iowait so
 * that process accounting knows that this is a task in IO wait state.
 */
void __sched io_schedule(void)
{
	struct rq *rq = raw_rq();

	delayacct_blkio_start();
	atomic_inc(&rq->nr_iowait);
	current->in_iowait = 1;
	schedule();
	current->in_iowait = 0;
	atomic_dec(&rq->nr_iowait);
	delayacct_blkio_end();
}
EXPORT_SYMBOL(io_schedule);

long __sched io_schedule_timeout(long timeout)
{
	struct rq *rq = raw_rq();
	long ret;

	delayacct_blkio_start();
	atomic_inc(&rq->nr_iowait);
	current->in_iowait = 1;
	ret = schedule_timeout(timeout);
	current->in_iowait = 0;
	atomic_dec(&rq->nr_iowait);
	delayacct_blkio_end();
	return ret;
}

/**
 * sys_sched_get_priority_max - return maximum RT priority.
 * @policy: scheduling class.
 *
 * this syscall returns the maximum rt_priority that can be used
 * by a given scheduling class.
 */
SYSCALL_DEFINE1(sched_get_priority_max, int, policy)
{
	int ret = -EINVAL;

	switch (policy) {
	case SCHED_FIFO:
	case SCHED_RR:
		ret = MAX_USER_RT_PRIO-1;
		break;
	case SCHED_NORMAL:
	case SCHED_BATCH:
	case SCHED_IDLE:
		ret = 0;
		break;
	}
	return ret;
}

/**
 * sys_sched_get_priority_min - return minimum RT priority.
 * @policy: scheduling class.
 *
 * this syscall returns the minimum rt_priority that can be used
 * by a given scheduling class.
 */
SYSCALL_DEFINE1(sched_get_priority_min, int, policy)
{
	int ret = -EINVAL;

	switch (policy) {
	case SCHED_FIFO:
	case SCHED_RR:
		ret = 1;
		break;
	case SCHED_NORMAL:
	case SCHED_BATCH:
	case SCHED_IDLE:
		ret = 0;
	}
	return ret;
}

/**
 * sys_sched_rr_get_interval - return the default timeslice of a process.
 * @pid: pid of the process.
 * @interval: userspace pointer to the timeslice value.
 *
 * this syscall writes the default timeslice value of a given process
 * into the user-space timespec buffer. A value of '0' means infinity.
 */
SYSCALL_DEFINE2(sched_rr_get_interval, pid_t, pid,
		struct timespec __user *, interval)
{
	struct task_struct *p;
	unsigned int time_slice;
	unsigned long flags;
	struct rq *rq;
	int retval;
	struct timespec t;

	if (pid < 0)
		return -EINVAL;

	retval = -ESRCH;
	rcu_read_lock();
	p = find_process_by_pid(pid);
	if (!p)
		goto out_unlock;

	retval = security_task_getscheduler(p);
	if (retval)
		goto out_unlock;

	rq = task_rq_lock(p, &flags);
	time_slice = p->sched_class->get_rr_interval(rq, p);
	task_rq_unlock(rq, &flags);

	rcu_read_unlock();
	jiffies_to_timespec(time_slice, &t);
	retval = copy_to_user(interval, &t, sizeof(t)) ? -EFAULT : 0;
	return retval;

out_unlock:
	rcu_read_unlock();
	return retval;
}

static const char stat_nam[] = TASK_STATE_TO_CHAR_STR;

void sched_show_task(struct task_struct *p)
{
	unsigned long free = 0;
	unsigned state;

	state = p->state ? __ffs(p->state) + 1 : 0;
	printk(KERN_INFO "%-13.13s %c", p->comm,
		state < sizeof(stat_nam) - 1 ? stat_nam[state] : '?');
#if BITS_PER_LONG == 32
	if (state == TASK_RUNNING)
		printk(KERN_CONT " running  ");
	else
		printk(KERN_CONT " %08lx ", thread_saved_pc(p));
#else
	if (state == TASK_RUNNING)
		printk(KERN_CONT "  running task    ");
	else
		printk(KERN_CONT " %016lx ", thread_saved_pc(p));
#endif
#ifdef CONFIG_DEBUG_STACK_USAGE
	free = stack_not_used(p);
#endif
	printk(KERN_CONT "%5lu %5d %6d 0x%08lx\n", free,
		task_pid_nr(p), task_pid_nr(p->real_parent),
		(unsigned long)task_thread_info(p)->flags);

	show_stack(p, NULL);
}

void show_state_filter(unsigned long state_filter)
{
	struct task_struct *g, *p;

#if BITS_PER_LONG == 32
	printk(KERN_INFO
		"  task                PC stack   pid father\n");
#else
	printk(KERN_INFO
		"  task                        PC stack   pid father\n");
#endif
	read_lock(&tasklist_lock);
	do_each_thread(g, p) {
		/*
		 * reset the NMI-timeout, listing all files on a slow
		 * console might take alot of time:
		 */
		touch_nmi_watchdog();
		if (!state_filter || (p->state & state_filter))
			sched_show_task(p);
	} while_each_thread(g, p);

	touch_all_softlockup_watchdogs();

#ifdef CONFIG_SCHED_DEBUG
	sysrq_sched_debug_show();
#endif
	read_unlock(&tasklist_lock);
	/*
	 * Only show locks if all tasks are dumped:
	 */
	if (!state_filter)
		debug_show_all_locks();
}

void __cpuinit init_idle_bootup_task(struct task_struct *idle)
{
	idle->sched_class = &idle_sched_class;
}

/**
 * init_idle - set up an idle thread for a given CPU
 * @idle: task in question
 * @cpu: cpu the idle task belongs to
 *
 * NOTE: this function does not set the idle thread's NEED_RESCHED
 * flag, to make booting more robust.
 */
void __cpuinit init_idle(struct task_struct *idle, int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long flags;

	raw_spin_lock_irqsave(&rq->lock, flags);

	__sched_fork(idle);
	idle->state = TASK_RUNNING;
	idle->se.exec_start = sched_clock();

	cpumask_copy(&idle->cpus_allowed, cpumask_of(cpu));
	/*
	 * We're having a chicken and egg problem, even though we are
	 * holding rq->lock, the cpu isn't yet set to this cpu so the
	 * lockdep check in task_group() will fail.
	 *
	 * Similar case to sched_fork(). / Alternatively we could
	 * use task_rq_lock() here and obtain the other rq->lock.
	 *
	 * Silence PROVE_RCU
	 */
	rcu_read_lock();
	__set_task_cpu(idle, cpu);
	rcu_read_unlock();

	rq->curr = rq->idle = idle;
#if defined(CONFIG_SMP) && defined(__ARCH_WANT_UNLOCKED_CTXSW)
	idle->oncpu = 1;
#endif
	raw_spin_unlock_irqrestore(&rq->lock, flags);

	/* Set the preempt count _outside_ the spinlocks! */
#if defined(CONFIG_PREEMPT)
	task_thread_info(idle)->preempt_count = (idle->lock_depth >= 0);
#else
	task_thread_info(idle)->preempt_count = 0;
#endif
	/*
	 * The idle tasks have their own, simple scheduling class:
	 */
	idle->sched_class = &idle_sched_class;
	ftrace_graph_init_task(idle);
}

/*
 * In a system that switches off the HZ timer nohz_cpu_mask
 * indicates which cpus entered this state. This is used
 * in the rcu update to wait only for active cpus. For system
 * which do not switch off the HZ timer nohz_cpu_mask should
 * always be CPU_BITS_NONE.
 */
cpumask_var_t nohz_cpu_mask;

/*
 * Increase the granularity value when there are more CPUs,
 * because with more CPUs the 'effective latency' as visible
 * to users decreases. But the relationship is not linear,
 * so pick a second-best guess by going with the log2 of the
 * number of CPUs.
 *
 * This idea comes from the SD scheduler of Con Kolivas:
 */
static int get_update_sysctl_factor(void)
{
	unsigned int cpus = min_t(int, num_online_cpus(), 8);
	unsigned int factor;

	switch (sysctl_sched_tunable_scaling) {
	case SCHED_TUNABLESCALING_NONE:
		factor = 1;
		break;
	case SCHED_TUNABLESCALING_LINEAR:
		factor = cpus;
		break;
	case SCHED_TUNABLESCALING_LOG:
	default:
		factor = 1 + ilog2(cpus);
		break;
	}

	return factor;
}

static void update_sysctl(void)
{
	unsigned int factor = get_update_sysctl_factor();

#define SET_SYSCTL(name) \
	(sysctl_##name = (factor) * normalized_sysctl_##name)
	SET_SYSCTL(sched_min_granularity);
	SET_SYSCTL(sched_latency);
	SET_SYSCTL(sched_wakeup_granularity);
	SET_SYSCTL(sched_shares_ratelimit);
#undef SET_SYSCTL
}

static inline void sched_init_granularity(void)
{
	update_sysctl();
}

#ifdef CONFIG_SMP
/*
 * This is how migration works:
 *
 * 1) we invoke migration_cpu_stop() on the target CPU using
 *    stop_one_cpu().
 * 2) stopper starts to run (implicitly forcing the migrated thread
 *    off the CPU)
 * 3) it checks whether the migrated task is still in the wrong runqueue.
 * 4) if it's in the wrong runqueue then the migration thread removes
 *    it and puts it into the right queue.
 * 5) stopper completes and stop_one_cpu() returns and the migration
 *    is done.
 */

/*
 * Change a given task's CPU affinity. Migrate the thread to a
 * proper CPU and schedule it away if the CPU it's executing on
 * is removed from the allowed bitmask.
 *
 * NOTE: the caller must have a valid reference to the task, the
 * task must not exit() & deallocate itself prematurely. The
 * call is not atomic; no spinlocks may be held.
 */
int set_cpus_allowed_ptr(struct task_struct *p, const struct cpumask *new_mask)
{
	unsigned long flags;
	struct rq *rq;
	unsigned int dest_cpu;
	int ret = 0;

	/*
	 * Serialize against TASK_WAKING so that ttwu() and wunt() can
	 * drop the rq->lock and still rely on ->cpus_allowed.
	 */
again:
	while (task_is_waking(p))
		cpu_relax();
	rq = task_rq_lock(p, &flags);
	if (task_is_waking(p)) {
		task_rq_unlock(rq, &flags);
		goto again;
	}

	if (!cpumask_intersects(new_mask, cpu_active_mask)) {
		ret = -EINVAL;
		goto out;
	}

	if (unlikely((p->flags & PF_THREAD_BOUND) && p != current &&
		     !cpumask_equal(&p->cpus_allowed, new_mask))) {
		ret = -EINVAL;
		goto out;
	}

	if (p->sched_class->set_cpus_allowed)
		p->sched_class->set_cpus_allowed(p, new_mask);
	else {
		cpumask_copy(&p->cpus_allowed, new_mask);
		p->rt.nr_cpus_allowed = cpumask_weight(new_mask);
	}

	/* Can the task run on the task's current CPU? If so, we're done */
	if (cpumask_test_cpu(task_cpu(p), new_mask))
		goto out;

	dest_cpu = cpumask_any_and(cpu_active_mask, new_mask);
	if (migrate_task(p, dest_cpu)) {
		struct migration_arg arg = { p, dest_cpu };
		/* Need help from migration thread: drop lock and wait. */
		task_rq_unlock(rq, &flags);
		stop_one_cpu(cpu_of(rq), migration_cpu_stop, &arg);
		tlb_migrate_finish(p->mm);
		return 0;
	}
out:
	task_rq_unlock(rq, &flags);

	return ret;
}
EXPORT_SYMBOL_GPL(set_cpus_allowed_ptr);

/*
 * Move (not current) task off this cpu, onto dest cpu. We're doing
 * this because either it can't run here any more (set_cpus_allowed()
 * away from this CPU, or CPU going down), or because we're
 * attempting to rebalance this task on exec (sched_exec).
 *
 * So we race with normal scheduler movements, but that's OK, as long
 * as the task is no longer on this CPU.
 *
 * Returns non-zero if task was successfully migrated.
 */
static int __migrate_task(struct task_struct *p, int src_cpu, int dest_cpu)
{
	struct rq *rq_dest, *rq_src;
	int ret = 0;

	if (unlikely(!cpu_active(dest_cpu)))
		return ret;

	rq_src = cpu_rq(src_cpu);
	rq_dest = cpu_rq(dest_cpu);

	double_rq_lock(rq_src, rq_dest);
	/* Already moved. */
	if (task_cpu(p) != src_cpu)
		goto done;
	/* Affinity changed (again). */
	if (!cpumask_test_cpu(dest_cpu, &p->cpus_allowed))
		goto fail;

	/*
	 * If we're not on a rq, the next wake-up will ensure we're
	 * placed properly.
	 */
	if (p->se.on_rq) {
		deactivate_task(rq_src, p, 0);
		set_task_cpu(p, dest_cpu);
		activate_task(rq_dest, p, 0);
		check_preempt_curr(rq_dest, p, 0);
	}
done:
	ret = 1;
fail:
	double_rq_unlock(rq_src, rq_dest);
	return ret;
}

/*
 * migration_cpu_stop - this will be executed by a highprio stopper thread
 * and performs thread migration by bumping thread off CPU then
 * 'pushing' onto another runqueue.
 */
static int migration_cpu_stop(void *data)
{
	struct migration_arg *arg = data;

	/*
	 * The original target cpu might have gone down and we might
	 * be on another cpu but it doesn't matter.
	 */
	local_irq_disable();
	__migrate_task(arg->task, raw_smp_processor_id(), arg->dest_cpu);
	local_irq_enable();
	return 0;
}

#ifdef CONFIG_HOTPLUG_CPU
/*
 * Figure out where task on dead CPU should go, use force if necessary.
 */
void move_task_off_dead_cpu(int dead_cpu, struct task_struct *p)
{
	struct rq *rq = cpu_rq(dead_cpu);
	int needs_cpu, uninitialized_var(dest_cpu);
	unsigned long flags;

	local_irq_save(flags);

	raw_spin_lock(&rq->lock);
	needs_cpu = (task_cpu(p) == dead_cpu) && (p->state != TASK_WAKING);
	if (needs_cpu)
		dest_cpu = select_fallback_rq(dead_cpu, p);
	raw_spin_unlock(&rq->lock);
	/*
	 * It can only fail if we race with set_cpus_allowed(),
	 * in the racer should migrate the task anyway.
	 */
	if (needs_cpu)
		__migrate_task(p, dead_cpu, dest_cpu);
	local_irq_restore(flags);
}

/*
 * While a dead CPU has no uninterruptible tasks queued at this point,
 * it might still have a nonzero ->nr_uninterruptible counter, because
 * for performance reasons the counter is not stricly tracking tasks to
 * their home CPUs. So we just add the counter to another CPU's counter,
 * to keep the global sum constant after CPU-down:
 */
static void migrate_nr_uninterruptible(struct rq *rq_src)
{
	struct rq *rq_dest = cpu_rq(cpumask_any(cpu_active_mask));
	unsigned long flags;

	local_irq_save(flags);
	double_rq_lock(rq_src, rq_dest);
	rq_dest->nr_uninterruptible += rq_src->nr_uninterruptible;
	rq_src->nr_uninterruptible = 0;
	double_rq_unlock(rq_src, rq_dest);
	local_irq_restore(flags);
}

/* Run through task list and migrate tasks from the dead cpu. */
static void migrate_live_tasks(int src_cpu)
{
	struct task_struct *p, *t;

	read_lock(&tasklist_lock);

	do_each_thread(t, p) {
		if (p == current)
			continue;

		if (task_cpu(p) == src_cpu)
			move_task_off_dead_cpu(src_cpu, p);
	} while_each_thread(t, p);

	read_unlock(&tasklist_lock);
}

/*
 * Schedules idle task to be the next runnable task on current CPU.
 * It does so by boosting its priority to highest possible.
 * Used by CPU offline code.
 */
void sched_idle_next(void)
{
	int this_cpu = smp_processor_id();
	struct rq *rq = cpu_rq(this_cpu);
	struct task_struct *p = rq->idle;
	unsigned long flags;

	/* cpu has to be offline */
	BUG_ON(cpu_online(this_cpu));

	/*
	 * Strictly not necessary since rest of the CPUs are stopped by now
	 * and interrupts disabled on the current cpu.
	 */
	raw_spin_lock_irqsave(&rq->lock, flags);

	__setscheduler(rq, p, SCHED_FIFO, MAX_RT_PRIO-1);

	activate_task(rq, p, 0);

	raw_spin_unlock_irqrestore(&rq->lock, flags);
}

/*
 * Ensures that the idle task is using init_mm right before its cpu goes
 * offline.
 */
void idle_task_exit(void)
{
	struct mm_struct *mm = current->active_mm;

	BUG_ON(cpu_online(smp_processor_id()));

	if (mm != &init_mm)
		switch_mm(mm, &init_mm, current);
	mmdrop(mm);
}

/* called under rq->lock with disabled interrupts */
static void migrate_dead(unsigned int dead_cpu, struct task_struct *p)
{
	struct rq *rq = cpu_rq(dead_cpu);

	/* Must be exiting, otherwise would be on tasklist. */
	BUG_ON(!p->exit_state);

	/* Cannot have done final schedule yet: would have vanished. */
	BUG_ON(p->state == TASK_DEAD);

	get_task_struct(p);

	/*
	 * Drop lock around migration; if someone else moves it,
	 * that's OK. No task can be added to this CPU, so iteration is
	 * fine.
	 */
	raw_spin_unlock_irq(&rq->lock);
	move_task_off_dead_cpu(dead_cpu, p);
	raw_spin_lock_irq(&rq->lock);

	put_task_struct(p);
}

/* release_task() removes task from tasklist, so we won't find dead tasks. */
static void migrate_dead_tasks(unsigned int dead_cpu)
{
	struct rq *rq = cpu_rq(dead_cpu);
	struct task_struct *next;

	for ( ; ; ) {
		if (!rq->nr_running)
			break;
		next = pick_next_task(rq);
		if (!next)
			break;
		next->sched_class->put_prev_task(rq, next);
		migrate_dead(dead_cpu, next);

	}
}

/*
 * remove the tasks which were accounted by rq from calc_load_tasks.
 */
static void calc_global_load_remove(struct rq *rq)
{
	atomic_long_sub(rq->calc_load_active, &calc_load_tasks);
	rq->calc_load_active = 0;
}
#endif /* CONFIG_HOTPLUG_CPU */

#if defined(CONFIG_SCHED_DEBUG) && defined(CONFIG_SYSCTL)

static struct ctl_table sd_ctl_dir[] = {
	{
		.procname	= "sched_domain",
		.mode		= 0555,
	},
	{}
};

static struct ctl_table sd_ctl_root[] = {
	{
		.procname	= "kernel",
		.mode		= 0555,
		.child		= sd_ctl_dir,
	},
	{}
};

static struct ctl_table *sd_alloc_ctl_entry(int n)
{
	struct ctl_table *entry =
		kcalloc(n, sizeof(struct ctl_table), GFP_KERNEL);

	return entry;
}

static void sd_free_ctl_entry(struct ctl_table **tablep)
{
	struct ctl_table *entry;

	/*
	 * In the intermediate directories, both the child directory and
	 * procname are dynamically allocated and could fail but the mode
	 * will always be set. In the lowest directory the names are
	 * static strings and all have proc handlers.
	 */
	for (entry = *tablep; entry->mode; entry++) {
		if (entry->child)
			sd_free_ctl_entry(&entry->child);
		if (entry->proc_handler == NULL)
			kfree(entry->procname);
	}

	kfree(*tablep);
	*tablep = NULL;
}

static void
set_table_entry(struct ctl_table *entry,
		const char *procname, void *data, int maxlen,
		mode_t mode, proc_handler *proc_handler)
{
	entry->procname = procname;
	entry->data = data;
	entry->maxlen = maxlen;
	entry->mode = mode;
	entry->proc_handler = proc_handler;
}

static struct ctl_table *
sd_alloc_ctl_domain_table(struct sched_domain *sd)
{
	struct ctl_table *table = sd_alloc_ctl_entry(13);

	if (table == NULL)
		return NULL;

	set_table_entry(&table[0], "min_interval", &sd->min_interval,
		sizeof(long), 0644, proc_doulongvec_minmax);
	set_table_entry(&table[1], "max_interval", &sd->max_interval,
		sizeof(long), 0644, proc_doulongvec_minmax);
	set_table_entry(&table[2], "busy_idx", &sd->busy_idx,
		sizeof(int), 0644, proc_dointvec_minmax);
	set_table_entry(&table[3], "idle_idx", &sd->idle_idx,
		sizeof(int), 0644, proc_dointvec_minmax);
	set_table_entry(&table[4], "newidle_idx", &sd->newidle_idx,
		sizeof(int), 0644, proc_dointvec_minmax);
	set_table_entry(&table[5], "wake_idx", &sd->wake_idx,
		sizeof(int), 0644, proc_dointvec_minmax);
	set_table_entry(&table[6], "forkexec_idx", &sd->forkexec_idx,
		sizeof(int), 0644, proc_dointvec_minmax);
	set_table_entry(&table[7], "busy_factor", &sd->busy_factor,
		sizeof(int), 0644, proc_dointvec_minmax);
	set_table_entry(&table[8], "imbalance_pct", &sd->imbalance_pct,
		sizeof(int), 0644, proc_dointvec_minmax);
	set_table_entry(&table[9], "cache_nice_tries",
		&sd->cache_nice_tries,
		sizeof(int), 0644, proc_dointvec_minmax);
	set_table_entry(&table[10], "flags", &sd->flags,
		sizeof(int), 0644, proc_dointvec_minmax);
	set_table_entry(&table[11], "name", sd->name,
		CORENAME_MAX_SIZE, 0444, proc_dostring);
	/* &table[12] is terminator */

	return table;
}

static ctl_table *sd_alloc_ctl_cpu_table(int cpu)
{
	struct ctl_table *entry, *table;
	struct sched_domain *sd;
	int domain_num = 0, i;
	char buf[32];

	for_each_domain(cpu, sd)
		domain_num++;
	entry = table = sd_alloc_ctl_entry(domain_num + 1);
	if (table == NULL)
		return NULL;

	i = 0;
	for_each_domain(cpu, sd) {
		snprintf(buf, 32, "domain%d", i);
		entry->procname = kstrdup(buf, GFP_KERNEL);
		entry->mode = 0555;
		entry->child = sd_alloc_ctl_domain_table(sd);
		entry++;
		i++;
	}
	return table;
}

static struct ctl_table_header *sd_sysctl_header;
static void register_sched_domain_sysctl(void)
{
	int i, cpu_num = num_possible_cpus();
	struct ctl_table *entry = sd_alloc_ctl_entry(cpu_num + 1);
	char buf[32];

	WARN_ON(sd_ctl_dir[0].child);
	sd_ctl_dir[0].child = entry;

	if (entry == NULL)
		return;

	for_each_possible_cpu(i) {
		snprintf(buf, 32, "cpu%d", i);
		entry->procname = kstrdup(buf, GFP_KERNEL);
		entry->mode = 0555;
		entry->child = sd_alloc_ctl_cpu_table(i);
		entry++;
	}

	WARN_ON(sd_sysctl_header);
	sd_sysctl_header = register_sysctl_table(sd_ctl_root);
}

/* may be called multiple times per register */
static void unregister_sched_domain_sysctl(void)
{
	if (sd_sysctl_header)
		unregister_sysctl_table(sd_sysctl_header);
	sd_sysctl_header = NULL;
	if (sd_ctl_dir[0].child)
		sd_free_ctl_entry(&sd_ctl_dir[0].child);
}
#else
static void register_sched_domain_sysctl(void)
{
}
static void unregister_sched_domain_sysctl(void)
{
}
#endif

static void set_rq_online(struct rq *rq)
{
	if (!rq->online) {
		const struct sched_class *class;

		cpumask_set_cpu(rq->cpu, rq->rd->online);
		rq->online = 1;

		for_each_class(class) {
			if (class->rq_online)
				class->rq_online(rq);
		}
	}
}

static void set_rq_offline(struct rq *rq)
{
	if (rq->online) {
		const struct sched_class *class;

		for_each_class(class) {
			if (class->rq_offline)
				class->rq_offline(rq);
		}

		cpumask_clear_cpu(rq->cpu, rq->rd->online);
		rq->online = 0;
	}
}

/*
 * migration_call - callback that gets triggered when a CPU is added.
 * Here we can start up the necessary migration thread for the new CPU.
 */
static int __cpuinit
migration_call(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
	int cpu = (long)hcpu;
	unsigned long flags;
	struct rq *rq = cpu_rq(cpu);

	switch (action) {

	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
		rq->calc_load_update = calc_load_update;
		break;

	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		/* Update our root-domain */
		raw_spin_lock_irqsave(&rq->lock, flags);
		if (rq->rd) {
			BUG_ON(!cpumask_test_cpu(cpu, rq->rd->span));

			set_rq_online(rq);
		}
		raw_spin_unlock_irqrestore(&rq->lock, flags);
		break;

#ifdef CONFIG_HOTPLUG_CPU
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		migrate_live_tasks(cpu);
		/* Idle task back to normal (off runqueue, low prio) */
		raw_spin_lock_irq(&rq->lock);
		deactivate_task(rq, rq->idle, 0);
		__setscheduler(rq, rq->idle, SCHED_NORMAL, 0);
		rq->idle->sched_class = &idle_sched_class;
		migrate_dead_tasks(cpu);
		raw_spin_unlock_irq(&rq->lock);
		migrate_nr_uninterruptible(rq);
		BUG_ON(rq->nr_running != 0);
		calc_global_load_remove(rq);
		break;

	case CPU_DYING:
	case CPU_DYING_FROZEN:
		/* Update our root-domain */
		raw_spin_lock_irqsave(&rq->lock, flags);
		if (rq->rd) {
			BUG_ON(!cpumask_test_cpu(cpu, rq->rd->span));
			set_rq_offline(rq);
		}
		raw_spin_unlock_irqrestore(&rq->lock, flags);
		break;
#endif
	}
	return NOTIFY_OK;
}

/*
 * Register at high priority so that task migration (migrate_all_tasks)
 * happens before everything else.  This has to be lower priority than
 * the notifier in the perf_event subsystem, though.
 */
static struct notifier_block __cpuinitdata migration_notifier = {
	.notifier_call = migration_call,
	.priority = CPU_PRI_MIGRATION,
};

static int __cpuinit sched_cpu_active(struct notifier_block *nfb,
				      unsigned long action, void *hcpu)
{
	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_ONLINE:
	case CPU_DOWN_FAILED:
		set_cpu_active((long)hcpu, true);
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
}

static int __cpuinit sched_cpu_inactive(struct notifier_block *nfb,
					unsigned long action, void *hcpu)
{
	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_DOWN_PREPARE:
		set_cpu_active((long)hcpu, false);
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
}

static int __init migration_init(void)
{
	void *cpu = (void *)(long)smp_processor_id();
	int err;

	/* Initialize migration for the boot CPU */
	err = migration_call(&migration_notifier, CPU_UP_PREPARE, cpu);
	BUG_ON(err == NOTIFY_BAD);
	migration_call(&migration_notifier, CPU_ONLINE, cpu);
	register_cpu_notifier(&migration_notifier);

	/* Register cpu active notifiers */
	cpu_notifier(sched_cpu_active, CPU_PRI_SCHED_ACTIVE);
	cpu_notifier(sched_cpu_inactive, CPU_PRI_SCHED_INACTIVE);

	return 0;
}
early_initcall(migration_init);
#endif

#ifdef CONFIG_SMP

#ifdef CONFIG_SCHED_DEBUG

static __read_mostly int sched_domain_debug_enabled;

static int __init sched_domain_debug_setup(char *str)
{
	sched_domain_debug_enabled = 1;

	return 0;
}
early_param("sched_debug", sched_domain_debug_setup);

static int sched_domain_debug_one(struct sched_domain *sd, int cpu, int level,
				  struct cpumask *groupmask)
{
	struct sched_group *group = sd->groups;
	char str[256];

	cpulist_scnprintf(str, sizeof(str), sched_domain_span(sd));
	cpumask_clear(groupmask);

	printk(KERN_DEBUG "%*s domain %d: ", level, "", level);

	if (!(sd->flags & SD_LOAD_BALANCE)) {
		printk("does not load-balance\n");
		if (sd->parent)
			printk(KERN_ERR "ERROR: !SD_LOAD_BALANCE domain"
					" has parent");
		return -1;
	}

	printk(KERN_CONT "span %s level %s\n", str, sd->name);

	if (!cpumask_test_cpu(cpu, sched_domain_span(sd))) {
		printk(KERN_ERR "ERROR: domain->span does not contain "
				"CPU%d\n", cpu);
	}
	if (!cpumask_test_cpu(cpu, sched_group_cpus(group))) {
		printk(KERN_ERR "ERROR: domain->groups does not contain"
				" CPU%d\n", cpu);
	}

	printk(KERN_DEBUG "%*s groups:", level + 1, "");
	do {
		if (!group) {
			printk("\n");
			printk(KERN_ERR "ERROR: group is NULL\n");
			break;
		}

		if (!group->cpu_power) {
			printk(KERN_CONT "\n");
			printk(KERN_ERR "ERROR: domain->cpu_power not "
					"set\n");
			break;
		}

		if (!cpumask_weight(sched_group_cpus(group))) {
			printk(KERN_CONT "\n");
			printk(KERN_ERR "ERROR: empty group\n");
			break;
		}

		if (cpumask_intersects(groupmask, sched_group_cpus(group))) {
			printk(KERN_CONT "\n");
			printk(KERN_ERR "ERROR: repeated CPUs\n");
			break;
		}

		cpumask_or(groupmask, groupmask, sched_group_cpus(group));

		cpulist_scnprintf(str, sizeof(str), sched_group_cpus(group));

		printk(KERN_CONT " %s", str);
		if (group->cpu_power != SCHED_LOAD_SCALE) {
			printk(KERN_CONT " (cpu_power = %d)",
				group->cpu_power);
		}

		group = group->next;
	} while (group != sd->groups);
	printk(KERN_CONT "\n");

	if (!cpumask_equal(sched_domain_span(sd), groupmask))
		printk(KERN_ERR "ERROR: groups don't span domain->span\n");

	if (sd->parent &&
	    !cpumask_subset(groupmask, sched_domain_span(sd->parent)))
		printk(KERN_ERR "ERROR: parent span is not a superset "
			"of domain->span\n");
	return 0;
}

static void sched_domain_debug(struct sched_domain *sd, int cpu)
{
	cpumask_var_t groupmask;
	int level = 0;

	if (!sched_domain_debug_enabled)
		return;

	if (!sd) {
		printk(KERN_DEBUG "CPU%d attaching NULL sched-domain.\n", cpu);
		return;
	}

	printk(KERN_DEBUG "CPU%d attaching sched-domain:\n", cpu);

	if (!alloc_cpumask_var(&groupmask, GFP_KERNEL)) {
		printk(KERN_DEBUG "Cannot load-balance (out of memory)\n");
		return;
	}

	for (;;) {
		if (sched_domain_debug_one(sd, cpu, level, groupmask))
			break;
		level++;
		sd = sd->parent;
		if (!sd)
			break;
	}
	free_cpumask_var(groupmask);
}
#else /* !CONFIG_SCHED_DEBUG */
# define sched_domain_debug(sd, cpu) do { } while (0)
#endif /* CONFIG_SCHED_DEBUG */

static int sd_degenerate(struct sched_domain *sd)
{
	if (cpumask_weight(sched_domain_span(sd)) == 1)
		return 1;

	/* Following flags need at least 2 groups */
	if (sd->flags & (SD_LOAD_BALANCE |
			 SD_BALANCE_NEWIDLE |
			 SD_BALANCE_FORK |
			 SD_BALANCE_EXEC |
			 SD_SHARE_CPUPOWER |
			 SD_SHARE_PKG_RESOURCES)) {
		if (sd->groups != sd->groups->next)
			return 0;
	}

	/* Following flags don't use groups */
	if (sd->flags & (SD_WAKE_AFFINE))
		return 0;

	return 1;
}

static int
sd_parent_degenerate(struct sched_domain *sd, struct sched_domain *parent)
{
	unsigned long cflags = sd->flags, pflags = parent->flags;

	if (sd_degenerate(parent))
		return 1;

	if (!cpumask_equal(sched_domain_span(sd), sched_domain_span(parent)))
		return 0;

	/* Flags needing groups don't count if only 1 group in parent */
	if (parent->groups == parent->groups->next) {
		pflags &= ~(SD_LOAD_BALANCE |
				SD_BALANCE_NEWIDLE |
				SD_BALANCE_FORK |
				SD_BALANCE_EXEC |
				SD_SHARE_CPUPOWER |
				SD_SHARE_PKG_RESOURCES);
		if (nr_node_ids == 1)
			pflags &= ~SD_SERIALIZE;
	}
	if (~cflags & pflags)
		return 0;

	return 1;
}

static void free_rootdomain(struct root_domain *rd)
{
	synchronize_sched();

	cpupri_cleanup(&rd->cpupri);

	free_cpumask_var(rd->rto_mask);
	free_cpumask_var(rd->online);
	free_cpumask_var(rd->span);
	kfree(rd);
}

static void rq_attach_root(struct rq *rq, struct root_domain *rd)
{
	struct root_domain *old_rd = NULL;
	unsigned long flags;

	raw_spin_lock_irqsave(&rq->lock, flags);

	if (rq->rd) {
		old_rd = rq->rd;

		if (cpumask_test_cpu(rq->cpu, old_rd->online))
			set_rq_offline(rq);

		cpumask_clear_cpu(rq->cpu, old_rd->span);

		/*
		 * If we dont want to free the old_rt yet then
		 * set old_rd to NULL to skip the freeing later
		 * in this function:
		 */
		if (!atomic_dec_and_test(&old_rd->refcount))
			old_rd = NULL;
	}

	atomic_inc(&rd->refcount);
	rq->rd = rd;

	cpumask_set_cpu(rq->cpu, rd->span);
	if (cpumask_test_cpu(rq->cpu, cpu_active_mask))
		set_rq_online(rq);

	raw_spin_unlock_irqrestore(&rq->lock, flags);

	if (old_rd)
		free_rootdomain(old_rd);
}

static int init_rootdomain(struct root_domain *rd)
{
	memset(rd, 0, sizeof(*rd));

	if (!alloc_cpumask_var(&rd->span, GFP_KERNEL))
		goto out;
	if (!alloc_cpumask_var(&rd->online, GFP_KERNEL))
		goto free_span;
	if (!alloc_cpumask_var(&rd->rto_mask, GFP_KERNEL))
		goto free_online;

	if (cpupri_init(&rd->cpupri) != 0)
		goto free_rto_mask;
	return 0;

free_rto_mask:
	free_cpumask_var(rd->rto_mask);
free_online:
	free_cpumask_var(rd->online);
free_span:
	free_cpumask_var(rd->span);
out:
	return -ENOMEM;
}

static void init_defrootdomain(void)
{
	init_rootdomain(&def_root_domain);

	atomic_set(&def_root_domain.refcount, 1);
}

static struct root_domain *alloc_rootdomain(void)
{
	struct root_domain *rd;

	rd = kmalloc(sizeof(*rd), GFP_KERNEL);
	if (!rd)
		return NULL;

	if (init_rootdomain(rd) != 0) {
		kfree(rd);
		return NULL;
	}

	return rd;
}

/*
 * Attach the domain 'sd' to 'cpu' as its base domain. Callers must
 * hold the hotplug lock.
 */
static void
cpu_attach_domain(struct sched_domain *sd, struct root_domain *rd, int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	struct sched_domain *tmp;

	for (tmp = sd; tmp; tmp = tmp->parent)
		tmp->span_weight = cpumask_weight(sched_domain_span(tmp));

	/* Remove the sched domains which do not contribute to scheduling. */
	for (tmp = sd; tmp; ) {
		struct sched_domain *parent = tmp->parent;
		if (!parent)
			break;

		if (sd_parent_degenerate(tmp, parent)) {
			tmp->parent = parent->parent;
			if (parent->parent)
				parent->parent->child = tmp;
		} else
			tmp = tmp->parent;
	}

	if (sd && sd_degenerate(sd)) {
		sd = sd->parent;
		if (sd)
			sd->child = NULL;
	}

	sched_domain_debug(sd, cpu);

	rq_attach_root(rq, rd);
	rcu_assign_pointer(rq->sd, sd);
}

/* cpus with isolated domains */
static cpumask_var_t cpu_isolated_map;

/* Setup the mask of cpus configured for isolated domains */
static int __init isolated_cpu_setup(char *str)
{
	alloc_bootmem_cpumask_var(&cpu_isolated_map);
	cpulist_parse(str, cpu_isolated_map);
	return 1;
}

__setup("isolcpus=", isolated_cpu_setup);

/*
 * init_sched_build_groups takes the cpumask we wish to span, and a pointer
 * to a function which identifies what group(along with sched group) a CPU
 * belongs to. The return value of group_fn must be a >= 0 and < nr_cpu_ids
 * (due to the fact that we keep track of groups covered with a struct cpumask).
 *
 * init_sched_build_groups will build a circular linked list of the groups
 * covered by the given span, and will set each group's ->cpumask correctly,
 * and ->cpu_power to 0.
 */
static void
init_sched_build_groups(const struct cpumask *span,
			const struct cpumask *cpu_map,
			int (*group_fn)(int cpu, const struct cpumask *cpu_map,
					struct sched_group **sg,
					struct cpumask *tmpmask),
			struct cpumask *covered, struct cpumask *tmpmask)
{
	struct sched_group *first = NULL, *last = NULL;
	int i;

	cpumask_clear(covered);

	for_each_cpu(i, span) {
		struct sched_group *sg;
		int group = group_fn(i, cpu_map, &sg, tmpmask);
		int j;

		if (cpumask_test_cpu(i, covered))
			continue;

		cpumask_clear(sched_group_cpus(sg));
		sg->cpu_power = 0;

		for_each_cpu(j, span) {
			if (group_fn(j, cpu_map, NULL, tmpmask) != group)
				continue;

			cpumask_set_cpu(j, covered);
			cpumask_set_cpu(j, sched_group_cpus(sg));
		}
		if (!first)
			first = sg;
		if (last)
			last->next = sg;
		last = sg;
	}
	last->next = first;
}

#define SD_NODES_PER_DOMAIN 16

#ifdef CONFIG_NUMA

/**
 * find_next_best_node - find the next node to include in a sched_domain
 * @node: node whose sched_domain we're building
 * @used_nodes: nodes already in the sched_domain
 *
 * Find the next node to include in a given scheduling domain. Simply
 * finds the closest node not already in the @used_nodes map.
 *
 * Should use nodemask_t.
 */
static int find_next_best_node(int node, nodemask_t *used_nodes)
{
	int i, n, val, min_val, best_node = 0;

	min_val = INT_MAX;

	for (i = 0; i < nr_node_ids; i++) {
		/* Start at @node */
		n = (node + i) % nr_node_ids;

		if (!nr_cpus_node(n))
			continue;

		/* Skip already used nodes */
		if (node_isset(n, *used_nodes))
			continue;

		/* Simple min distance search */
		val = node_distance(node, n);

		if (val < min_val) {
			min_val = val;
			best_node = n;
		}
	}

	node_set(best_node, *used_nodes);
	return best_node;
}

/**
 * sched_domain_node_span - get a cpumask for a node's sched_domain
 * @node: node whose cpumask we're constructing
 * @span: resulting cpumask
 *
 * Given a node, construct a good cpumask for its sched_domain to span. It
 * should be one that prevents unnecessary balancing, but also spreads tasks
 * out optimally.
 */
static void sched_domain_node_span(int node, struct cpumask *span)
{
	nodemask_t used_nodes;
	int i;

	cpumask_clear(span);
	nodes_clear(used_nodes);

	cpumask_or(span, span, cpumask_of_node(node));
	node_set(node, used_nodes);

	for (i = 1; i < SD_NODES_PER_DOMAIN; i++) {
		int next_node = find_next_best_node(node, &used_nodes);

		cpumask_or(span, span, cpumask_of_node(next_node));
	}
}
#endif /* CONFIG_NUMA */

int sched_smt_power_savings = 0, sched_mc_power_savings = 0;

/*
 * The cpus mask in sched_group and sched_domain hangs off the end.
 *
 * ( See the the comments in include/linux/sched.h:struct sched_group
 *   and struct sched_domain. )
 */
struct static_sched_group {
	struct sched_group sg;
	DECLARE_BITMAP(cpus, CONFIG_NR_CPUS);
};

struct static_sched_domain {
	struct sched_domain sd;
	DECLARE_BITMAP(span, CONFIG_NR_CPUS);
};

struct s_data {
#ifdef CONFIG_NUMA
	int			sd_allnodes;
	cpumask_var_t		domainspan;
	cpumask_var_t		covered;
	cpumask_var_t		notcovered;
#endif
	cpumask_var_t		nodemask;
	cpumask_var_t		this_sibling_map;
	cpumask_var_t		this_core_map;
	cpumask_var_t		send_covered;
	cpumask_var_t		tmpmask;
	struct sched_group	**sched_group_nodes;
	struct root_domain	*rd;
};

enum s_alloc {
	sa_sched_groups = 0,
	sa_rootdomain,
	sa_tmpmask,
	sa_send_covered,
	sa_this_core_map,
	sa_this_sibling_map,
	sa_nodemask,
	sa_sched_group_nodes,
#ifdef CONFIG_NUMA
	sa_notcovered,
	sa_covered,
	sa_domainspan,
#endif
	sa_none,
};

/*
 * SMT sched-domains:
 */
#ifdef CONFIG_SCHED_SMT
static DEFINE_PER_CPU(struct static_sched_domain, cpu_domains);
static DEFINE_PER_CPU(struct static_sched_group, sched_groups);

static int
cpu_to_cpu_group(int cpu, const struct cpumask *cpu_map,
		 struct sched_group **sg, struct cpumask *unused)
{
	if (sg)
		*sg = &per_cpu(sched_groups, cpu).sg;
	return cpu;
}
#endif /* CONFIG_SCHED_SMT */

/*
 * multi-core sched-domains:
 */
#ifdef CONFIG_SCHED_MC
static DEFINE_PER_CPU(struct static_sched_domain, core_domains);
static DEFINE_PER_CPU(struct static_sched_group, sched_group_core);
#endif /* CONFIG_SCHED_MC */

#if defined(CONFIG_SCHED_MC) && defined(CONFIG_SCHED_SMT)
static int
cpu_to_core_group(int cpu, const struct cpumask *cpu_map,
		  struct sched_group **sg, struct cpumask *mask)
{
	int group;

	cpumask_and(mask, topology_thread_cpumask(cpu), cpu_map);
	group = cpumask_first(mask);
	if (sg)
		*sg = &per_cpu(sched_group_core, group).sg;
	return group;
}
#elif defined(CONFIG_SCHED_MC)
static int
cpu_to_core_group(int cpu, const struct cpumask *cpu_map,
		  struct sched_group **sg, struct cpumask *unused)
{
	if (sg)
		*sg = &per_cpu(sched_group_core, cpu).sg;
	return cpu;
}
#endif

static DEFINE_PER_CPU(struct static_sched_domain, phys_domains);
static DEFINE_PER_CPU(struct static_sched_group, sched_group_phys);

static int
cpu_to_phys_group(int cpu, const struct cpumask *cpu_map,
		  struct sched_group **sg, struct cpumask *mask)
{
	int group;
#ifdef CONFIG_SCHED_MC
	cpumask_and(mask, cpu_coregroup_mask(cpu), cpu_map);
	group = cpumask_first(mask);
#elif defined(CONFIG_SCHED_SMT)
	cpumask_and(mask, topology_thread_cpumask(cpu), cpu_map);
	group = cpumask_first(mask);
#else
	group = cpu;
#endif
	if (sg)
		*sg = &per_cpu(sched_group_phys, group).sg;
	return group;
}

#ifdef CONFIG_NUMA
/*
 * The init_sched_build_groups can't handle what we want to do with node
 * groups, so roll our own. Now each node has its own list of groups which
 * gets dynamically allocated.
 */
static DEFINE_PER_CPU(struct static_sched_domain, node_domains);
static struct sched_group ***sched_group_nodes_bycpu;

static DEFINE_PER_CPU(struct static_sched_domain, allnodes_domains);
static DEFINE_PER_CPU(struct static_sched_group, sched_group_allnodes);

static int cpu_to_allnodes_group(int cpu, const struct cpumask *cpu_map,
				 struct sched_group **sg,
				 struct cpumask *nodemask)
{
	int group;

	cpumask_and(nodemask, cpumask_of_node(cpu_to_node(cpu)), cpu_map);
	group = cpumask_first(nodemask);

	if (sg)
		*sg = &per_cpu(sched_group_allnodes, group).sg;
	return group;
}

static void init_numa_sched_groups_power(struct sched_group *group_head)
{
	struct sched_group *sg = group_head;
	int j;

	if (!sg)
		return;
	do {
		for_each_cpu(j, sched_group_cpus(sg)) {
			struct sched_domain *sd;

			sd = &per_cpu(phys_domains, j).sd;
			if (j != group_first_cpu(sd->groups)) {
				/*
				 * Only add "power" once for each
				 * physical package.
				 */
				continue;
			}

			sg->cpu_power += sd->groups->cpu_power;
		}
		sg = sg->next;
	} while (sg != group_head);
}

static int build_numa_sched_groups(struct s_data *d,
				   const struct cpumask *cpu_map, int num)
{
	struct sched_domain *sd;
	struct sched_group *sg, *prev;
	int n, j;

	cpumask_clear(d->covered);
	cpumask_and(d->nodemask, cpumask_of_node(num), cpu_map);
	if (cpumask_empty(d->nodemask)) {
		d->sched_group_nodes[num] = NULL;
		goto out;
	}

	sched_domain_node_span(num, d->domainspan);
	cpumask_and(d->domainspan, d->domainspan, cpu_map);

	sg = kmalloc_node(sizeof(struct sched_group) + cpumask_size(),
			  GFP_KERNEL, num);
	if (!sg) {
		printk(KERN_WARNING "Can not alloc domain group for node %d\n",
		       num);
		return -ENOMEM;
	}
	d->sched_group_nodes[num] = sg;

	for_each_cpu(j, d->nodemask) {
		sd = &per_cpu(node_domains, j).sd;
		sd->groups = sg;
	}

	sg->cpu_power = 0;
	cpumask_copy(sched_group_cpus(sg), d->nodemask);
	sg->next = sg;
	cpumask_or(d->covered, d->covered, d->nodemask);

	prev = sg;
	for (j = 0; j < nr_node_ids; j++) {
		n = (num + j) % nr_node_ids;
		cpumask_complement(d->notcovered, d->covered);
		cpumask_and(d->tmpmask, d->notcovered, cpu_map);
		cpumask_and(d->tmpmask, d->tmpmask, d->domainspan);
		if (cpumask_empty(d->tmpmask))
			break;
		cpumask_and(d->tmpmask, d->tmpmask, cpumask_of_node(n));
		if (cpumask_empty(d->tmpmask))
			continue;
		sg = kmalloc_node(sizeof(struct sched_group) + cpumask_size(),
				  GFP_KERNEL, num);
		if (!sg) {
			printk(KERN_WARNING
			       "Can not alloc domain group for node %d\n", j);
			return -ENOMEM;
		}
		sg->cpu_power = 0;
		cpumask_copy(sched_group_cpus(sg), d->tmpmask);
		sg->next = prev->next;
		cpumask_or(d->covered, d->covered, d->tmpmask);
		prev->next = sg;
		prev = sg;
	}
out:
	return 0;
}
#endif /* CONFIG_NUMA */

#ifdef CONFIG_NUMA
/* Free memory allocated for various sched_group structures */
static void free_sched_groups(const struct cpumask *cpu_map,
			      struct cpumask *nodemask)
{
	int cpu, i;

	for_each_cpu(cpu, cpu_map) {
		struct sched_group **sched_group_nodes
			= sched_group_nodes_bycpu[cpu];

		if (!sched_group_nodes)
			continue;

		for (i = 0; i < nr_node_ids; i++) {
			struct sched_group *oldsg, *sg = sched_group_nodes[i];

			cpumask_and(nodemask, cpumask_of_node(i), cpu_map);
			if (cpumask_empty(nodemask))
				continue;

			if (sg == NULL)
				continue;
			sg = sg->next;
next_sg:
			oldsg = sg;
			sg = sg->next;
			kfree(oldsg);
			if (oldsg != sched_group_nodes[i])
				goto next_sg;
		}
		kfree(sched_group_nodes);
		sched_group_nodes_bycpu[cpu] = NULL;
	}
}
#else /* !CONFIG_NUMA */
static void free_sched_groups(const struct cpumask *cpu_map,
			      struct cpumask *nodemask)
{
}
#endif /* CONFIG_NUMA */

/*
 * Initialize sched groups cpu_power.
 *
 * cpu_power indicates the capacity of sched group, which is used while
 * distributing the load between different sched groups in a sched domain.
 * Typically cpu_power for all the groups in a sched domain will be same unless
 * there are asymmetries in the topology. If there are asymmetries, group
 * having more cpu_power will pickup more load compared to the group having
 * less cpu_power.
 */
static void init_sched_groups_power(int cpu, struct sched_domain *sd)
{
	struct sched_domain *child;
	struct sched_group *group;
	long power;
	int weight;

	WARN_ON(!sd || !sd->groups);

	if (cpu != group_first_cpu(sd->groups))
		return;

	child = sd->child;

	sd->groups->cpu_power = 0;

	if (!child) {
		power = SCHED_LOAD_SCALE;
		weight = cpumask_weight(sched_domain_span(sd));
		/*
		 * SMT siblings share the power of a single core.
		 * Usually multiple threads get a better yield out of
		 * that one core than a single thread would have,
		 * reflect that in sd->smt_gain.
		 */
		if ((sd->flags & SD_SHARE_CPUPOWER) && weight > 1) {
			power *= sd->smt_gain;
			power /= weight;
			power >>= SCHED_LOAD_SHIFT;
		}
		sd->groups->cpu_power += power;
		return;
	}

	/*
	 * Add cpu_power of each child group to this groups cpu_power.
	 */
	group = child->groups;
	do {
		sd->groups->cpu_power += group->cpu_power;
		group = group->next;
	} while (group != child->groups);
}

/*
 * Initializers for schedule domains
 * Non-inlined to reduce accumulated stack pressure in build_sched_domains()
 */

#ifdef CONFIG_SCHED_DEBUG
# define SD_INIT_NAME(sd, type)		sd->name = #type
#else
# define SD_INIT_NAME(sd, type)		do { } while (0)
#endif

#define	SD_INIT(sd, type)	sd_init_##type(sd)

#define SD_INIT_FUNC(type)	\
static noinline void sd_init_##type(struct sched_domain *sd)	\
{								\
	memset(sd, 0, sizeof(*sd));				\
	*sd = SD_##type##_INIT;					\
	sd->level = SD_LV_##type;				\
	SD_INIT_NAME(sd, type);					\
}

SD_INIT_FUNC(CPU)
#ifdef CONFIG_NUMA
 SD_INIT_FUNC(ALLNODES)
 SD_INIT_FUNC(NODE)
#endif
#ifdef CONFIG_SCHED_SMT
 SD_INIT_FUNC(SIBLING)
#endif
#ifdef CONFIG_SCHED_MC
 SD_INIT_FUNC(MC)
#endif

static int default_relax_domain_level = -1;

static int __init setup_relax_domain_level(char *str)
{
	unsigned long val;

	val = simple_strtoul(str, NULL, 0);
	if (val < SD_LV_MAX)
		default_relax_domain_level = val;

	return 1;
}
__setup("relax_domain_level=", setup_relax_domain_level);

static void set_domain_attribute(struct sched_domain *sd,
				 struct sched_domain_attr *attr)
{
	int request;

	if (!attr || attr->relax_domain_level < 0) {
		if (default_relax_domain_level < 0)
			return;
		else
			request = default_relax_domain_level;
	} else
		request = attr->relax_domain_level;
	if (request < sd->level) {
		/* turn off idle balance on this domain */
		sd->flags &= ~(SD_BALANCE_WAKE|SD_BALANCE_NEWIDLE);
	} else {
		/* turn on idle balance on this domain */
		sd->flags |= (SD_BALANCE_WAKE|SD_BALANCE_NEWIDLE);
	}
}

static void __free_domain_allocs(struct s_data *d, enum s_alloc what,
				 const struct cpumask *cpu_map)
{
	switch (what) {
	case sa_sched_groups:
		free_sched_groups(cpu_map, d->tmpmask); /* fall through */
		d->sched_group_nodes = NULL;
	case sa_rootdomain:
		free_rootdomain(d->rd); /* fall through */
	case sa_tmpmask:
		free_cpumask_var(d->tmpmask); /* fall through */
	case sa_send_covered:
		free_cpumask_var(d->send_covered); /* fall through */
	case sa_this_core_map:
		free_cpumask_var(d->this_core_map); /* fall through */
	case sa_this_sibling_map:
		free_cpumask_var(d->this_sibling_map); /* fall through */
	case sa_nodemask:
		free_cpumask_var(d->nodemask); /* fall through */
	case sa_sched_group_nodes:
#ifdef CONFIG_NUMA
		kfree(d->sched_group_nodes); /* fall through */
	case sa_notcovered:
		free_cpumask_var(d->notcovered); /* fall through */
	case sa_covered:
		free_cpumask_var(d->covered); /* fall through */
	case sa_domainspan:
		free_cpumask_var(d->domainspan); /* fall through */
#endif
	case sa_none:
		break;
	}
}

static enum s_alloc __visit_domain_allocation_hell(struct s_data *d,
						   const struct cpumask *cpu_map)
{
#ifdef CONFIG_NUMA
	if (!alloc_cpumask_var(&d->domainspan, GFP_KERNEL))
		return sa_none;
	if (!alloc_cpumask_var(&d->covered, GFP_KERNEL))
		return sa_domainspan;
	if (!alloc_cpumask_var(&d->notcovered, GFP_KERNEL))
		return sa_covered;
	/* Allocate the per-node list of sched groups */
	d->sched_group_nodes = kcalloc(nr_node_ids,
				      sizeof(struct sched_group *), GFP_KERNEL);
	if (!d->sched_group_nodes) {
		printk(KERN_WARNING "Can not alloc sched group node list\n");
		return sa_notcovered;
	}
	sched_group_nodes_bycpu[cpumask_first(cpu_map)] = d->sched_group_nodes;
#endif
	if (!alloc_cpumask_var(&d->nodemask, GFP_KERNEL))
		return sa_sched_group_nodes;
	if (!alloc_cpumask_var(&d->this_sibling_map, GFP_KERNEL))
		return sa_nodemask;
	if (!alloc_cpumask_var(&d->this_core_map, GFP_KERNEL))
		return sa_this_sibling_map;
	if (!alloc_cpumask_var(&d->send_covered, GFP_KERNEL))
		return sa_this_core_map;
	if (!alloc_cpumask_var(&d->tmpmask, GFP_KERNEL))
		return sa_send_covered;
	d->rd = alloc_rootdomain();
	if (!d->rd) {
		printk(KERN_WARNING "Cannot alloc root domain\n");
		return sa_tmpmask;
	}
	return sa_rootdomain;
}

static struct sched_domain *__build_numa_sched_domains(struct s_data *d,
	const struct cpumask *cpu_map, struct sched_domain_attr *attr, int i)
{
	struct sched_domain *sd = NULL;
#ifdef CONFIG_NUMA
	struct sched_domain *parent;

	d->sd_allnodes = 0;
	if (cpumask_weight(cpu_map) >
	    SD_NODES_PER_DOMAIN * cpumask_weight(d->nodemask)) {
		sd = &per_cpu(allnodes_domains, i).sd;
		SD_INIT(sd, ALLNODES);
		set_domain_attribute(sd, attr);
		cpumask_copy(sched_domain_span(sd), cpu_map);
		cpu_to_allnodes_group(i, cpu_map, &sd->groups, d->tmpmask);
		d->sd_allnodes = 1;
	}
	parent = sd;

	sd = &per_cpu(node_domains, i).sd;
	SD_INIT(sd, NODE);
	set_domain_attribute(sd, attr);
	sched_domain_node_span(cpu_to_node(i), sched_domain_span(sd));
	sd->parent = parent;
	if (parent)
		parent->child = sd;
	cpumask_and(sched_domain_span(sd), sched_domain_span(sd), cpu_map);
#endif
	return sd;
}

static struct sched_domain *__build_cpu_sched_domain(struct s_data *d,
	const struct cpumask *cpu_map, struct sched_domain_attr *attr,
	struct sched_domain *parent, int i)
{
	struct sched_domain *sd;
	sd = &per_cpu(phys_domains, i).sd;
	SD_INIT(sd, CPU);
	set_domain_attribute(sd, attr);
	cpumask_copy(sched_domain_span(sd), d->nodemask);
	sd->parent = parent;
	if (parent)
		parent->child = sd;
	cpu_to_phys_group(i, cpu_map, &sd->groups, d->tmpmask);
	return sd;
}

static struct sched_domain *__build_mc_sched_domain(struct s_data *d,
	const struct cpumask *cpu_map, struct sched_domain_attr *attr,
	struct sched_domain *parent, int i)
{
	struct sched_domain *sd = parent;
#ifdef CONFIG_SCHED_MC
	sd = &per_cpu(core_domains, i).sd;
	SD_INIT(sd, MC);
	set_domain_attribute(sd, attr);
	cpumask_and(sched_domain_span(sd), cpu_map, cpu_coregroup_mask(i));
	sd->parent = parent;
	parent->child = sd;
	cpu_to_core_group(i, cpu_map, &sd->groups, d->tmpmask);
#endif
	return sd;
}

static struct sched_domain *__build_smt_sched_domain(struct s_data *d,
	const struct cpumask *cpu_map, struct sched_domain_attr *attr,
	struct sched_domain *parent, int i)
{
	struct sched_domain *sd = parent;
#ifdef CONFIG_SCHED_SMT
	sd = &per_cpu(cpu_domains, i).sd;
	SD_INIT(sd, SIBLING);
	set_domain_attribute(sd, attr);
	cpumask_and(sched_domain_span(sd), cpu_map, topology_thread_cpumask(i));
	sd->parent = parent;
	parent->child = sd;
	cpu_to_cpu_group(i, cpu_map, &sd->groups, d->tmpmask);
#endif
	return sd;
}

static void build_sched_groups(struct s_data *d, enum sched_domain_level l,
			       const struct cpumask *cpu_map, int cpu)
{
	switch (l) {
#ifdef CONFIG_SCHED_SMT
	case SD_LV_SIBLING: /* set up CPU (sibling) groups */
		cpumask_and(d->this_sibling_map, cpu_map,
			    topology_thread_cpumask(cpu));
		if (cpu == cpumask_first(d->this_sibling_map))
			init_sched_build_groups(d->this_sibling_map, cpu_map,
						&cpu_to_cpu_group,
						d->send_covered, d->tmpmask);
		break;
#endif
#ifdef CONFIG_SCHED_MC
	case SD_LV_MC: /* set up multi-core groups */
		cpumask_and(d->this_core_map, cpu_map, cpu_coregroup_mask(cpu));
		if (cpu == cpumask_first(d->this_core_map))
			init_sched_build_groups(d->this_core_map, cpu_map,
						&cpu_to_core_group,
						d->send_covered, d->tmpmask);
		break;
#endif
	case SD_LV_CPU: /* set up physical groups */
		cpumask_and(d->nodemask, cpumask_of_node(cpu), cpu_map);
		if (!cpumask_empty(d->nodemask))
			init_sched_build_groups(d->nodemask, cpu_map,
						&cpu_to_phys_group,
						d->send_covered, d->tmpmask);
		break;
#ifdef CONFIG_NUMA
	case SD_LV_ALLNODES:
		init_sched_build_groups(cpu_map, cpu_map, &cpu_to_allnodes_group,
					d->send_covered, d->tmpmask);
		break;
#endif
	default:
		break;
	}
}

/*
 * Build sched domains for a given set of cpus and attach the sched domains
 * to the individual cpus
 */
static int __build_sched_domains(const struct cpumask *cpu_map,
				 struct sched_domain_attr *attr)
{
	enum s_alloc alloc_state = sa_none;
	struct s_data d;
	struct sched_domain *sd;
	int i;
#ifdef CONFIG_NUMA
	d.sd_allnodes = 0;
#endif

	alloc_state = __visit_domain_allocation_hell(&d, cpu_map);
	if (alloc_state != sa_rootdomain)
		goto error;
	alloc_state = sa_sched_groups;

	/*
	 * Set up domains for cpus specified by the cpu_map.
	 */
	for_each_cpu(i, cpu_map) {
		cpumask_and(d.nodemask, cpumask_of_node(cpu_to_node(i)),
			    cpu_map);

		sd = __build_numa_sched_domains(&d, cpu_map, attr, i);
		sd = __build_cpu_sched_domain(&d, cpu_map, attr, sd, i);
		sd = __build_mc_sched_domain(&d, cpu_map, attr, sd, i);
		sd = __build_smt_sched_domain(&d, cpu_map, attr, sd, i);
	}

	for_each_cpu(i, cpu_map) {
		build_sched_groups(&d, SD_LV_SIBLING, cpu_map, i);
		build_sched_groups(&d, SD_LV_MC, cpu_map, i);
	}

	/* Set up physical groups */
	for (i = 0; i < nr_node_ids; i++)
		build_sched_groups(&d, SD_LV_CPU, cpu_map, i);

#ifdef CONFIG_NUMA
	/* Set up node groups */
	if (d.sd_allnodes)
		build_sched_groups(&d, SD_LV_ALLNODES, cpu_map, 0);

	for (i = 0; i < nr_node_ids; i++)
		if (build_numa_sched_groups(&d, cpu_map, i))
			goto error;
#endif

	/* Calculate CPU power for physical packages and nodes */
#ifdef CONFIG_SCHED_SMT
	for_each_cpu(i, cpu_map) {
		sd = &per_cpu(cpu_domains, i).sd;
		init_sched_groups_power(i, sd);
	}
#endif
#ifdef CONFIG_SCHED_MC
	for_each_cpu(i, cpu_map) {
		sd = &per_cpu(core_domains, i).sd;
		init_sched_groups_power(i, sd);
	}
#endif

	for_each_cpu(i, cpu_map) {
		sd = &per_cpu(phys_domains, i).sd;
		init_sched_groups_power(i, sd);
	}

#ifdef CONFIG_NUMA
	for (i = 0; i < nr_node_ids; i++)
		init_numa_sched_groups_power(d.sched_group_nodes[i]);

	if (d.sd_allnodes) {
		struct sched_group *sg;

		cpu_to_allnodes_group(cpumask_first(cpu_map), cpu_map, &sg,
								d.tmpmask);
		init_numa_sched_groups_power(sg);
	}
#endif

	/* Attach the domains */
	for_each_cpu(i, cpu_map) {
#ifdef CONFIG_SCHED_SMT
		sd = &per_cpu(cpu_domains, i).sd;
#elif defined(CONFIG_SCHED_MC)
		sd = &per_cpu(core_domains, i).sd;
#else
		sd = &per_cpu(phys_domains, i).sd;
#endif
		cpu_attach_domain(sd, d.rd, i);
	}

	d.sched_group_nodes = NULL; /* don't free this we still need it */
	__free_domain_allocs(&d, sa_tmpmask, cpu_map);
	return 0;

error:
	__free_domain_allocs(&d, alloc_state, cpu_map);
	return -ENOMEM;
}

static int build_sched_domains(const struct cpumask *cpu_map)
{
	return __build_sched_domains(cpu_map, NULL);
}

static cpumask_var_t *doms_cur;	/* current sched domains */
static int ndoms_cur;		/* number of sched domains in 'doms_cur' */
static struct sched_domain_attr *dattr_cur;
				/* attribues of custom domains in 'doms_cur' */

/*
 * Special case: If a kmalloc of a doms_cur partition (array of
 * cpumask) fails, then fallback to a single sched domain,
 * as determined by the single cpumask fallback_doms.
 */
static cpumask_var_t fallback_doms;

/*
 * arch_update_cpu_topology lets virtualized architectures update the
 * cpu core maps. It is supposed to return 1 if the topology changed
 * or 0 if it stayed the same.
 */
int __attribute__((weak)) arch_update_cpu_topology(void)
{
	return 0;
}

cpumask_var_t *alloc_sched_domains(unsigned int ndoms)
{
	int i;
	cpumask_var_t *doms;

	doms = kmalloc(sizeof(*doms) * ndoms, GFP_KERNEL);
	if (!doms)
		return NULL;
	for (i = 0; i < ndoms; i++) {
		if (!alloc_cpumask_var(&doms[i], GFP_KERNEL)) {
			free_sched_domains(doms, i);
			return NULL;
		}
	}
	return doms;
}

void free_sched_domains(cpumask_var_t doms[], unsigned int ndoms)
{
	unsigned int i;
	for (i = 0; i < ndoms; i++)
		free_cpumask_var(doms[i]);
	kfree(doms);
}

/*
 * Set up scheduler domains and groups. Callers must hold the hotplug lock.
 * For now this just excludes isolated cpus, but could be used to
 * exclude other special cases in the future.
 */
static int arch_init_sched_domains(const struct cpumask *cpu_map)
{
	int err;

	arch_update_cpu_topology();
	ndoms_cur = 1;
	doms_cur = alloc_sched_domains(ndoms_cur);
	if (!doms_cur)
		doms_cur = &fallback_doms;
	cpumask_andnot(doms_cur[0], cpu_map, cpu_isolated_map);
	dattr_cur = NULL;
	err = build_sched_domains(doms_cur[0]);
	register_sched_domain_sysctl();

	return err;
}

static void arch_destroy_sched_domains(const struct cpumask *cpu_map,
				       struct cpumask *tmpmask)
{
	free_sched_groups(cpu_map, tmpmask);
}

/*
 * Detach sched domains from a group of cpus specified in cpu_map
 * These cpus will now be attached to the NULL domain
 */
static void detach_destroy_domains(const struct cpumask *cpu_map)
{
	/* Save because hotplug lock held. */
	static DECLARE_BITMAP(tmpmask, CONFIG_NR_CPUS);
	int i;

	for_each_cpu(i, cpu_map)
		cpu_attach_domain(NULL, &def_root_domain, i);
	synchronize_sched();
	arch_destroy_sched_domains(cpu_map, to_cpumask(tmpmask));
}

/* handle null as "default" */
static int dattrs_equal(struct sched_domain_attr *cur, int idx_cur,
			struct sched_domain_attr *new, int idx_new)
{
	struct sched_domain_attr tmp;

	/* fast path */
	if (!new && !cur)
		return 1;

	tmp = SD_ATTR_INIT;
	return !memcmp(cur ? (cur + idx_cur) : &tmp,
			new ? (new + idx_new) : &tmp,
			sizeof(struct sched_domain_attr));
}

/*
 * Partition sched domains as specified by the 'ndoms_new'
 * cpumasks in the array doms_new[] of cpumasks. This compares
 * doms_new[] to the current sched domain partitioning, doms_cur[].
 * It destroys each deleted domain and builds each new domain.
 *
 * 'doms_new' is an array of cpumask_var_t's of length 'ndoms_new'.
 * The masks don't intersect (don't overlap.) We should setup one
 * sched domain for each mask. CPUs not in any of the cpumasks will
 * not be load balanced. If the same cpumask appears both in the
 * current 'doms_cur' domains and in the new 'doms_new', we can leave
 * it as it is.
 *
 * The passed in 'doms_new' should be allocated using
 * alloc_sched_domains.  This routine takes ownership of it and will
 * free_sched_domains it when done with it. If the caller failed the
 * alloc call, then it can pass in doms_new == NULL && ndoms_new == 1,
 * and partition_sched_domains() will fallback to the single partition
 * 'fallback_doms', it also forces the domains to be rebuilt.
 *
 * If doms_new == NULL it will be replaced with cpu_online_mask.
 * ndoms_new == 0 is a special case for destroying existing domains,
 * and it will not create the default domain.
 *
 * Call with hotplug lock held
 */
void partition_sched_domains(int ndoms_new, cpumask_var_t doms_new[],
			     struct sched_domain_attr *dattr_new)
{
	int i, j, n;
	int new_topology;

	mutex_lock(&sched_domains_mutex);

	/* always unregister in case we don't destroy any domains */
	unregister_sched_domain_sysctl();

	/* Let architecture update cpu core mappings. */
	new_topology = arch_update_cpu_topology();

	n = doms_new ? ndoms_new : 0;

	/* Destroy deleted domains */
	for (i = 0; i < ndoms_cur; i++) {
		for (j = 0; j < n && !new_topology; j++) {
			if (cpumask_equal(doms_cur[i], doms_new[j])
			    && dattrs_equal(dattr_cur, i, dattr_new, j))
				goto match1;
		}
		/* no match - a current sched domain not in new doms_new[] */
		detach_destroy_domains(doms_cur[i]);
match1:
		;
	}

	if (doms_new == NULL) {
		ndoms_cur = 0;
		doms_new = &fallback_doms;
		cpumask_andnot(doms_new[0], cpu_active_mask, cpu_isolated_map);
		WARN_ON_ONCE(dattr_new);
	}

	/* Build new domains */
	for (i = 0; i < ndoms_new; i++) {
		for (j = 0; j < ndoms_cur && !new_topology; j++) {
			if (cpumask_equal(doms_new[i], doms_cur[j])
			    && dattrs_equal(dattr_new, i, dattr_cur, j))
				goto match2;
		}
		/* no match - add a new doms_new */
		__build_sched_domains(doms_new[i],
					dattr_new ? dattr_new + i : NULL);
match2:
		;
	}

	/* Remember the new sched domains */
	if (doms_cur != &fallback_doms)
		free_sched_domains(doms_cur, ndoms_cur);
	kfree(dattr_cur);	/* kfree(NULL) is safe */
	doms_cur = doms_new;
	dattr_cur = dattr_new;
	ndoms_cur = ndoms_new;

	register_sched_domain_sysctl();

	mutex_unlock(&sched_domains_mutex);
}

#if defined(CONFIG_SCHED_MC) || defined(CONFIG_SCHED_SMT)
static void arch_reinit_sched_domains(void)
{
	get_online_cpus();

	/* Destroy domains first to force the rebuild */
	partition_sched_domains(0, NULL, NULL);

	rebuild_sched_domains();
	put_online_cpus();
}

static ssize_t sched_power_savings_store(const char *buf, size_t count, int smt)
{
	unsigned int level = 0;

	if (sscanf(buf, "%u", &level) != 1)
		return -EINVAL;

	/*
	 * level is always be positive so don't check for
	 * level < POWERSAVINGS_BALANCE_NONE which is 0
	 * What happens on 0 or 1 byte write,
	 * need to check for count as well?
	 */

	if (level >= MAX_POWERSAVINGS_BALANCE_LEVELS)
		return -EINVAL;

	if (smt)
		sched_smt_power_savings = level;
	else
		sched_mc_power_savings = level;

	arch_reinit_sched_domains();

	return count;
}

#ifdef CONFIG_SCHED_MC
static ssize_t sched_mc_power_savings_show(struct sysdev_class *class,
					   struct sysdev_class_attribute *attr,
					   char *page)
{
	return sprintf(page, "%u\n", sched_mc_power_savings);
}
static ssize_t sched_mc_power_savings_store(struct sysdev_class *class,
					    struct sysdev_class_attribute *attr,
					    const char *buf, size_t count)
{
	return sched_power_savings_store(buf, count, 0);
}
static SYSDEV_CLASS_ATTR(sched_mc_power_savings, 0644,
			 sched_mc_power_savings_show,
			 sched_mc_power_savings_store);
#endif

#ifdef CONFIG_SCHED_SMT
static ssize_t sched_smt_power_savings_show(struct sysdev_class *dev,
					    struct sysdev_class_attribute *attr,
					    char *page)
{
	return sprintf(page, "%u\n", sched_smt_power_savings);
}
static ssize_t sched_smt_power_savings_store(struct sysdev_class *dev,
					     struct sysdev_class_attribute *attr,
					     const char *buf, size_t count)
{
	return sched_power_savings_store(buf, count, 1);
}
static SYSDEV_CLASS_ATTR(sched_smt_power_savings, 0644,
		   sched_smt_power_savings_show,
		   sched_smt_power_savings_store);
#endif

int __init sched_create_sysfs_power_savings_entries(struct sysdev_class *cls)
{
	int err = 0;

#ifdef CONFIG_SCHED_SMT
	if (smt_capable())
		err = sysfs_create_file(&cls->kset.kobj,
					&attr_sched_smt_power_savings.attr);
#endif
#ifdef CONFIG_SCHED_MC
	if (!err && mc_capable())
		err = sysfs_create_file(&cls->kset.kobj,
					&attr_sched_mc_power_savings.attr);
#endif
	return err;
}
#endif /* CONFIG_SCHED_MC || CONFIG_SCHED_SMT */

/*
 * Update cpusets according to cpu_active mask.  If cpusets are
 * disabled, cpuset_update_active_cpus() becomes a simple wrapper
 * around partition_sched_domains().
 */
static int cpuset_cpu_active(struct notifier_block *nfb, unsigned long action,
			     void *hcpu)
{
	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_ONLINE:
	case CPU_DOWN_FAILED:
		cpuset_update_active_cpus();
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
}

static int cpuset_cpu_inactive(struct notifier_block *nfb, unsigned long action,
			       void *hcpu)
{
	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_DOWN_PREPARE:
		cpuset_update_active_cpus();
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
}

static int update_runtime(struct notifier_block *nfb,
				unsigned long action, void *hcpu)
{
	int cpu = (int)(long)hcpu;

	switch (action) {
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		disable_runtime(cpu_rq(cpu));
		return NOTIFY_OK;

	case CPU_DOWN_FAILED:
	case CPU_DOWN_FAILED_FROZEN:
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		enable_runtime(cpu_rq(cpu));
		return NOTIFY_OK;

	default:
		return NOTIFY_DONE;
	}
}

void __init sched_init_smp(void)
{
	cpumask_var_t non_isolated_cpus;

	alloc_cpumask_var(&non_isolated_cpus, GFP_KERNEL);
	alloc_cpumask_var(&fallback_doms, GFP_KERNEL);

#if defined(CONFIG_NUMA)
	sched_group_nodes_bycpu = kzalloc(nr_cpu_ids * sizeof(void **),
								GFP_KERNEL);
	BUG_ON(sched_group_nodes_bycpu == NULL);
#endif
	get_online_cpus();
	mutex_lock(&sched_domains_mutex);
	arch_init_sched_domains(cpu_active_mask);
	cpumask_andnot(non_isolated_cpus, cpu_possible_mask, cpu_isolated_map);
	if (cpumask_empty(non_isolated_cpus))
		cpumask_set_cpu(smp_processor_id(), non_isolated_cpus);
	mutex_unlock(&sched_domains_mutex);
	put_online_cpus();

	hotcpu_notifier(cpuset_cpu_active, CPU_PRI_CPUSET_ACTIVE);
	hotcpu_notifier(cpuset_cpu_inactive, CPU_PRI_CPUSET_INACTIVE);

	/* RT runtime code needs to handle some hotplug events */
	hotcpu_notifier(update_runtime, 0);

	init_hrtick();

	/* Move init over to a non-isolated CPU */
	if (set_cpus_allowed_ptr(current, non_isolated_cpus) < 0)
		BUG();
	sched_init_granularity();
	free_cpumask_var(non_isolated_cpus);

	init_sched_rt_class();
}
#else
void __init sched_init_smp(void)
{
	sched_init_granularity();
}
#endif /* CONFIG_SMP */

const_debug unsigned int sysctl_timer_migration = 1;

int in_sched_functions(unsigned long addr)
{
	return in_lock_functions(addr) ||
		(addr >= (unsigned long)__sched_text_start
		&& addr < (unsigned long)__sched_text_end);
}

static void init_cfs_rq(struct cfs_rq *cfs_rq, struct rq *rq)
{
	cfs_rq->tasks_timeline = RB_ROOT;
	INIT_LIST_HEAD(&cfs_rq->tasks);
#ifdef CONFIG_FAIR_GROUP_SCHED
	cfs_rq->rq = rq;
#endif
	cfs_rq->min_vruntime = (u64)(-(1LL << 20));
}

static void init_rt_rq(struct rt_rq *rt_rq, struct rq *rq)
{
	struct rt_prio_array *array;
	int i;

	array = &rt_rq->active;
	for (i = 0; i < MAX_RT_PRIO; i++) {
		INIT_LIST_HEAD(array->queue + i);
		__clear_bit(i, array->bitmap);
	}
	/* delimiter for bitsearch: */
	__set_bit(MAX_RT_PRIO, array->bitmap);

#if defined CONFIG_SMP || defined CONFIG_RT_GROUP_SCHED
	rt_rq->highest_prio.curr = MAX_RT_PRIO;
#ifdef CONFIG_SMP
	rt_rq->highest_prio.next = MAX_RT_PRIO;
#endif
#endif
#ifdef CONFIG_SMP
	rt_rq->rt_nr_migratory = 0;
	rt_rq->overloaded = 0;
	plist_head_init_raw(&rt_rq->pushable_tasks, &rq->lock);
#endif

	rt_rq->rt_time = 0;
	rt_rq->rt_throttled = 0;
	rt_rq->rt_runtime = 0;
	raw_spin_lock_init(&rt_rq->rt_runtime_lock);

#ifdef CONFIG_RT_GROUP_SCHED
	rt_rq->rt_nr_boosted = 0;
	rt_rq->rq = rq;
#endif
}

#ifdef CONFIG_FAIR_GROUP_SCHED
static void init_tg_cfs_entry(struct task_group *tg, struct cfs_rq *cfs_rq,
				struct sched_entity *se, int cpu, int add,
				struct sched_entity *parent)
{
	struct rq *rq = cpu_rq(cpu);
	tg->cfs_rq[cpu] = cfs_rq;
	init_cfs_rq(cfs_rq, rq);
	cfs_rq->tg = tg;
	if (add)
		list_add(&cfs_rq->leaf_cfs_rq_list, &rq->leaf_cfs_rq_list);

	tg->se[cpu] = se;
	/* se could be NULL for init_task_group */
	if (!se)
		return;

	if (!parent)
		se->cfs_rq = &rq->cfs;
	else
		se->cfs_rq = parent->my_q;

	se->my_q = cfs_rq;
	se->load.weight = tg->shares;
	se->load.inv_weight = 0;
	se->parent = parent;
}
#endif

#ifdef CONFIG_RT_GROUP_SCHED
static void init_tg_rt_entry(struct task_group *tg, struct rt_rq *rt_rq,
		struct sched_rt_entity *rt_se, int cpu, int add,
		struct sched_rt_entity *parent)
{
	struct rq *rq = cpu_rq(cpu);

	tg->rt_rq[cpu] = rt_rq;
	init_rt_rq(rt_rq, rq);
	rt_rq->tg = tg;
	rt_rq->rt_runtime = tg->rt_bandwidth.rt_runtime;
	if (add)
		list_add(&rt_rq->leaf_rt_rq_list, &rq->leaf_rt_rq_list);

	tg->rt_se[cpu] = rt_se;
	if (!rt_se)
		return;

	if (!parent)
		rt_se->rt_rq = &rq->rt;
	else
		rt_se->rt_rq = parent->my_q;

	rt_se->my_q = rt_rq;
	rt_se->parent = parent;
	INIT_LIST_HEAD(&rt_se->run_list);
}
#endif

void __init sched_init(void)
{
	int i, j;
	unsigned long alloc_size = 0, ptr;

#ifdef CONFIG_FAIR_GROUP_SCHED
	alloc_size += 2 * nr_cpu_ids * sizeof(void **);
#endif
#ifdef CONFIG_RT_GROUP_SCHED
	alloc_size += 2 * nr_cpu_ids * sizeof(void **);
#endif
#ifdef CONFIG_CPUMASK_OFFSTACK
	alloc_size += num_possible_cpus() * cpumask_size();
#endif
	if (alloc_size) {
		ptr = (unsigned long)kzalloc(alloc_size, GFP_NOWAIT);

#ifdef CONFIG_FAIR_GROUP_SCHED
		init_task_group.se = (struct sched_entity **)ptr;
		ptr += nr_cpu_ids * sizeof(void **);

		init_task_group.cfs_rq = (struct cfs_rq **)ptr;
		ptr += nr_cpu_ids * sizeof(void **);

#endif /* CONFIG_FAIR_GROUP_SCHED */
#ifdef CONFIG_RT_GROUP_SCHED
		init_task_group.rt_se = (struct sched_rt_entity **)ptr;
		ptr += nr_cpu_ids * sizeof(void **);

		init_task_group.rt_rq = (struct rt_rq **)ptr;
		ptr += nr_cpu_ids * sizeof(void **);

#endif /* CONFIG_RT_GROUP_SCHED */
#ifdef CONFIG_CPUMASK_OFFSTACK
		for_each_possible_cpu(i) {
			per_cpu(load_balance_tmpmask, i) = (void *)ptr;
			ptr += cpumask_size();
		}
#endif /* CONFIG_CPUMASK_OFFSTACK */
	}

#ifdef CONFIG_SMP
	init_defrootdomain();
#endif

	init_rt_bandwidth(&def_rt_bandwidth,
			global_rt_period(), global_rt_runtime());

#ifdef CONFIG_RT_GROUP_SCHED
	init_rt_bandwidth(&init_task_group.rt_bandwidth,
			global_rt_period(), global_rt_runtime());
#endif /* CONFIG_RT_GROUP_SCHED */

#ifdef CONFIG_CGROUP_SCHED
	list_add(&init_task_group.list, &task_groups);
	INIT_LIST_HEAD(&init_task_group.children);

#endif /* CONFIG_CGROUP_SCHED */

#if defined CONFIG_FAIR_GROUP_SCHED && defined CONFIG_SMP
	update_shares_data = __alloc_percpu(nr_cpu_ids * sizeof(unsigned long),
					    __alignof__(unsigned long));
#endif
	for_each_possible_cpu(i) {
		struct rq *rq;

		rq = cpu_rq(i);
		raw_spin_lock_init(&rq->lock);
		rq->nr_running = 0;
		rq->calc_load_active = 0;
		rq->calc_load_update = jiffies + LOAD_FREQ;
		init_cfs_rq(&rq->cfs, rq);
		init_rt_rq(&rq->rt, rq);
#ifdef CONFIG_FAIR_GROUP_SCHED
		init_task_group.shares = init_task_group_load;
		INIT_LIST_HEAD(&rq->leaf_cfs_rq_list);
#ifdef CONFIG_CGROUP_SCHED
		/*
		 * How much cpu bandwidth does init_task_group get?
		 *
		 * In case of task-groups formed thr' the cgroup filesystem, it
		 * gets 100% of the cpu resources in the system. This overall
		 * system cpu resource is divided among the tasks of
		 * init_task_group and its child task-groups in a fair manner,
		 * based on each entity's (task or task-group's) weight
		 * (se->load.weight).
		 *
		 * In other words, if init_task_group has 10 tasks of weight
		 * 1024) and two child groups A0 and A1 (of weight 1024 each),
		 * then A0's share of the cpu resource is:
		 *
		 *	A0's bandwidth = 1024 / (10*1024 + 1024 + 1024) = 8.33%
		 *
		 * We achieve this by letting init_task_group's tasks sit
		 * directly in rq->cfs (i.e init_task_group->se[] = NULL).
		 */
		init_tg_cfs_entry(&init_task_group, &rq->cfs, NULL, i, 1, NULL);
#endif
#endif /* CONFIG_FAIR_GROUP_SCHED */

		rq->rt.rt_runtime = def_rt_bandwidth.rt_runtime;
#ifdef CONFIG_RT_GROUP_SCHED
		INIT_LIST_HEAD(&rq->leaf_rt_rq_list);
#ifdef CONFIG_CGROUP_SCHED
		init_tg_rt_entry(&init_task_group, &rq->rt, NULL, i, 1, NULL);
#endif
#endif

		for (j = 0; j < CPU_LOAD_IDX_MAX; j++)
			rq->cpu_load[j] = 0;

		rq->last_load_update_tick = jiffies;

#ifdef CONFIG_SMP
		rq->sd = NULL;
		rq->rd = NULL;
		rq->cpu_power = SCHED_LOAD_SCALE;
		rq->post_schedule = 0;
		rq->active_balance = 0;
		rq->next_balance = jiffies;
		rq->push_cpu = 0;
		rq->cpu = i;
		rq->online = 0;
		rq->idle_stamp = 0;
		rq->avg_idle = 2*sysctl_sched_migration_cost;
		rq_attach_root(rq, &def_root_domain);
#ifdef CONFIG_NO_HZ
		rq->nohz_balance_kick = 0;
		init_sched_softirq_csd(&per_cpu(remote_sched_softirq_cb, i));
#endif
#endif
		init_rq_hrtick(rq);
		atomic_set(&rq->nr_iowait, 0);
	}

	set_load_weight(&init_task);

#ifdef CONFIG_PREEMPT_NOTIFIERS
	INIT_HLIST_HEAD(&init_task.preempt_notifiers);
#endif

#ifdef CONFIG_SMP
	open_softirq(SCHED_SOFTIRQ, run_rebalance_domains);
#endif

#ifdef CONFIG_RT_MUTEXES
	plist_head_init_raw(&init_task.pi_waiters, &init_task.pi_lock);
#endif

	/*
	 * The boot idle thread does lazy MMU switching as well:
	 */
	atomic_inc(&init_mm.mm_count);
	enter_lazy_tlb(&init_mm, current);

	/*
	 * Make us the idle thread. Technically, schedule() should not be
	 * called from this thread, however somewhere below it might be,
	 * but because we are the idle thread, we just pick up running again
	 * when this runqueue becomes "idle".
	 */
	init_idle(current, smp_processor_id());

	calc_load_update = jiffies + LOAD_FREQ;

	/*
	 * During early bootup we pretend to be a normal task:
	 */
	current->sched_class = &fair_sched_class;

	/* Allocate the nohz_cpu_mask if CONFIG_CPUMASK_OFFSTACK */
	zalloc_cpumask_var(&nohz_cpu_mask, GFP_NOWAIT);
#ifdef CONFIG_SMP
#ifdef CONFIG_NO_HZ
	zalloc_cpumask_var(&nohz.idle_cpus_mask, GFP_NOWAIT);
	alloc_cpumask_var(&nohz.grp_idle_mask, GFP_NOWAIT);
	atomic_set(&nohz.load_balancer, nr_cpu_ids);
	atomic_set(&nohz.first_pick_cpu, nr_cpu_ids);
	atomic_set(&nohz.second_pick_cpu, nr_cpu_ids);
#endif
	/* May be allocated at isolcpus cmdline parse time */
	if (cpu_isolated_map == NULL)
		zalloc_cpumask_var(&cpu_isolated_map, GFP_NOWAIT);
#endif /* SMP */

	perf_event_init();

	scheduler_running = 1;
}

#ifdef CONFIG_DEBUG_SPINLOCK_SLEEP
static inline int preempt_count_equals(int preempt_offset)
{
	int nested = (preempt_count() & ~PREEMPT_ACTIVE) + rcu_preempt_depth();

	return (nested == PREEMPT_INATOMIC_BASE + preempt_offset);
}

void __might_sleep(const char *file, int line, int preempt_offset)
{
#ifdef in_atomic
	static unsigned long prev_jiffy;	/* ratelimiting */

	if ((preempt_count_equals(preempt_offset) && !irqs_disabled()) ||
	    system_state != SYSTEM_RUNNING || oops_in_progress)
		return;
	if (time_before(jiffies, prev_jiffy + HZ) && prev_jiffy)
		return;
	prev_jiffy = jiffies;

	printk(KERN_ERR
		"BUG: sleeping function called from invalid context at %s:%d\n",
			file, line);
	printk(KERN_ERR
		"in_atomic(): %d, irqs_disabled(): %d, pid: %d, name: %s\n",
			in_atomic(), irqs_disabled(),
			current->pid, current->comm);

	debug_show_held_locks(current);
	if (irqs_disabled())
		print_irqtrace_events(current);
	dump_stack();
#endif
}
EXPORT_SYMBOL(__might_sleep);
#endif

#ifdef CONFIG_MAGIC_SYSRQ
static void normalize_task(struct rq *rq, struct task_struct *p)
{
	int on_rq;

	on_rq = p->se.on_rq;
	if (on_rq)
		deactivate_task(rq, p, 0);
	__setscheduler(rq, p, SCHED_NORMAL, 0);
	if (on_rq) {
		activate_task(rq, p, 0);
		resched_task(rq->curr);
	}
}

void normalize_rt_tasks(void)
{
	struct task_struct *g, *p;
	unsigned long flags;
	struct rq *rq;

	read_lock_irqsave(&tasklist_lock, flags);
	do_each_thread(g, p) {
		/*
		 * Only normalize user tasks:
		 */
		if (!p->mm)
			continue;

		p->se.exec_start		= 0;
#ifdef CONFIG_SCHEDSTATS
		p->se.statistics.wait_start	= 0;
		p->se.statistics.sleep_start	= 0;
		p->se.statistics.block_start	= 0;
#endif

		if (!rt_task(p)) {
			/*
			 * Renice negative nice level userspace
			 * tasks back to 0:
			 */
			if (TASK_NICE(p) < 0 && p->mm)
				set_user_nice(p, 0);
			continue;
		}

		raw_spin_lock(&p->pi_lock);
		rq = __task_rq_lock(p);

		normalize_task(rq, p);

		__task_rq_unlock(rq);
		raw_spin_unlock(&p->pi_lock);
	} while_each_thread(g, p);

	read_unlock_irqrestore(&tasklist_lock, flags);
}

#endif /* CONFIG_MAGIC_SYSRQ */

#if defined(CONFIG_IA64) || defined(CONFIG_KGDB_KDB)
/*
 * These functions are only useful for the IA64 MCA handling, or kdb.
 *
 * They can only be called when the whole system has been
 * stopped - every CPU needs to be quiescent, and no scheduling
 * activity can take place. Using them for anything else would
 * be a serious bug, and as a result, they aren't even visible
 * under any other configuration.
 */

/**
 * curr_task - return the current task for a given cpu.
 * @cpu: the processor in question.
 *
 * ONLY VALID WHEN THE WHOLE SYSTEM IS STOPPED!
 */
struct task_struct *curr_task(int cpu)
{
	return cpu_curr(cpu);
}

#endif /* defined(CONFIG_IA64) || defined(CONFIG_KGDB_KDB) */

#ifdef CONFIG_IA64
/**
 * set_curr_task - set the current task for a given cpu.
 * @cpu: the processor in question.
 * @p: the task pointer to set.
 *
 * Description: This function must only be used when non-maskable interrupts
 * are serviced on a separate stack. It allows the architecture to switch the
 * notion of the current task on a cpu in a non-blocking manner. This function
 * must be called with all CPU's synchronized, and interrupts disabled, the
 * and caller must save the original value of the current task (see
 * curr_task() above) and restore that value before reenabling interrupts and
 * re-starting the system.
 *
 * ONLY VALID WHEN THE WHOLE SYSTEM IS STOPPED!
 */
void set_curr_task(int cpu, struct task_struct *p)
{
	cpu_curr(cpu) = p;
}

#endif

#ifdef CONFIG_FAIR_GROUP_SCHED
static void free_fair_sched_group(struct task_group *tg)
{
	int i;

	for_each_possible_cpu(i) {
		if (tg->cfs_rq)
			kfree(tg->cfs_rq[i]);
		if (tg->se)
			kfree(tg->se[i]);
	}

	kfree(tg->cfs_rq);
	kfree(tg->se);
}

static
int alloc_fair_sched_group(struct task_group *tg, struct task_group *parent)
{
	struct cfs_rq *cfs_rq;
	struct sched_entity *se;
	struct rq *rq;
	int i;

	tg->cfs_rq = kzalloc(sizeof(cfs_rq) * nr_cpu_ids, GFP_KERNEL);
	if (!tg->cfs_rq)
		goto err;
	tg->se = kzalloc(sizeof(se) * nr_cpu_ids, GFP_KERNEL);
	if (!tg->se)
		goto err;

	tg->shares = NICE_0_LOAD;

	for_each_possible_cpu(i) {
		rq = cpu_rq(i);

		cfs_rq = kzalloc_node(sizeof(struct cfs_rq),
				      GFP_KERNEL, cpu_to_node(i));
		if (!cfs_rq)
			goto err;

		se = kzalloc_node(sizeof(struct sched_entity),
				  GFP_KERNEL, cpu_to_node(i));
		if (!se)
			goto err_free_rq;

		init_tg_cfs_entry(tg, cfs_rq, se, i, 0, parent->se[i]);
	}

	return 1;

 err_free_rq:
	kfree(cfs_rq);
 err:
	return 0;
}

static inline void register_fair_sched_group(struct task_group *tg, int cpu)
{
	list_add_rcu(&tg->cfs_rq[cpu]->leaf_cfs_rq_list,
			&cpu_rq(cpu)->leaf_cfs_rq_list);
}

static inline void unregister_fair_sched_group(struct task_group *tg, int cpu)
{
	list_del_rcu(&tg->cfs_rq[cpu]->leaf_cfs_rq_list);
}
#else /* !CONFG_FAIR_GROUP_SCHED */
static inline void free_fair_sched_group(struct task_group *tg)
{
}

static inline
int alloc_fair_sched_group(struct task_group *tg, struct task_group *parent)
{
	return 1;
}

static inline void register_fair_sched_group(struct task_group *tg, int cpu)
{
}

static inline void unregister_fair_sched_group(struct task_group *tg, int cpu)
{
}
#endif /* CONFIG_FAIR_GROUP_SCHED */

#ifdef CONFIG_RT_GROUP_SCHED
static void free_rt_sched_group(struct task_group *tg)
{
	int i;

	destroy_rt_bandwidth(&tg->rt_bandwidth);

	for_each_possible_cpu(i) {
		if (tg->rt_rq)
			kfree(tg->rt_rq[i]);
		if (tg->rt_se)
			kfree(tg->rt_se[i]);
	}

	kfree(tg->rt_rq);
	kfree(tg->rt_se);
}

static
int alloc_rt_sched_group(struct task_group *tg, struct task_group *parent)
{
	struct rt_rq *rt_rq;
	struct sched_rt_entity *rt_se;
	struct rq *rq;
	int i;

	tg->rt_rq = kzalloc(sizeof(rt_rq) * nr_cpu_ids, GFP_KERNEL);
	if (!tg->rt_rq)
		goto err;
	tg->rt_se = kzalloc(sizeof(rt_se) * nr_cpu_ids, GFP_KERNEL);
	if (!tg->rt_se)
		goto err;

	init_rt_bandwidth(&tg->rt_bandwidth,
			ktime_to_ns(def_rt_bandwidth.rt_period), 0);

	for_each_possible_cpu(i) {
		rq = cpu_rq(i);

		rt_rq = kzalloc_node(sizeof(struct rt_rq),
				     GFP_KERNEL, cpu_to_node(i));
		if (!rt_rq)
			goto err;

		rt_se = kzalloc_node(sizeof(struct sched_rt_entity),
				     GFP_KERNEL, cpu_to_node(i));
		if (!rt_se)
			goto err_free_rq;

		init_tg_rt_entry(tg, rt_rq, rt_se, i, 0, parent->rt_se[i]);
	}

	return 1;

 err_free_rq:
	kfree(rt_rq);
 err:
	return 0;
}

static inline void register_rt_sched_group(struct task_group *tg, int cpu)
{
	list_add_rcu(&tg->rt_rq[cpu]->leaf_rt_rq_list,
			&cpu_rq(cpu)->leaf_rt_rq_list);
}

static inline void unregister_rt_sched_group(struct task_group *tg, int cpu)
{
	list_del_rcu(&tg->rt_rq[cpu]->leaf_rt_rq_list);
}
#else /* !CONFIG_RT_GROUP_SCHED */
static inline void free_rt_sched_group(struct task_group *tg)
{
}

static inline
int alloc_rt_sched_group(struct task_group *tg, struct task_group *parent)
{
	return 1;
}

static inline void register_rt_sched_group(struct task_group *tg, int cpu)
{
}

static inline void unregister_rt_sched_group(struct task_group *tg, int cpu)
{
}
#endif /* CONFIG_RT_GROUP_SCHED */

#ifdef CONFIG_CGROUP_SCHED
static void free_sched_group(struct task_group *tg)
{
	free_fair_sched_group(tg);
	free_rt_sched_group(tg);
	kfree(tg);
}

/* allocate runqueue etc for a new task group */
struct task_group *sched_create_group(struct task_group *parent)
{
	struct task_group *tg;
	unsigned long flags;
	int i;

	tg = kzalloc(sizeof(*tg), GFP_KERNEL);
	if (!tg)
		return ERR_PTR(-ENOMEM);

	if (!alloc_fair_sched_group(tg, parent))
		goto err;

	if (!alloc_rt_sched_group(tg, parent))
		goto err;

	spin_lock_irqsave(&task_group_lock, flags);
	for_each_possible_cpu(i) {
		register_fair_sched_group(tg, i);
		register_rt_sched_group(tg, i);
	}
	list_add_rcu(&tg->list, &task_groups);

	WARN_ON(!parent); /* root should already exist */

	tg->parent = parent;
	INIT_LIST_HEAD(&tg->children);
	list_add_rcu(&tg->siblings, &parent->children);
	spin_unlock_irqrestore(&task_group_lock, flags);

	return tg;

err:
	free_sched_group(tg);
	return ERR_PTR(-ENOMEM);
}

/* rcu callback to free various structures associated with a task group */
static void free_sched_group_rcu(struct rcu_head *rhp)
{
	/* now it should be safe to free those cfs_rqs */
	free_sched_group(container_of(rhp, struct task_group, rcu));
}

/* Destroy runqueue etc associated with a task group */
void sched_destroy_group(struct task_group *tg)
{
	unsigned long flags;
	int i;

	spin_lock_irqsave(&task_group_lock, flags);
	for_each_possible_cpu(i) {
		unregister_fair_sched_group(tg, i);
		unregister_rt_sched_group(tg, i);
	}
	list_del_rcu(&tg->list);
	list_del_rcu(&tg->siblings);
	spin_unlock_irqrestore(&task_group_lock, flags);

	/* wait for possible concurrent references to cfs_rqs complete */
	call_rcu(&tg->rcu, free_sched_group_rcu);
}

/* change task's runqueue when it moves between groups.
 *	The caller of this function should have put the task in its new group
 *	by now. This function just updates tsk->se.cfs_rq and tsk->se.parent to
 *	reflect its new group.
 */
void sched_move_task(struct task_struct *tsk)
{
	int on_rq, running;
	unsigned long flags;
	struct rq *rq;

	rq = task_rq_lock(tsk, &flags);

	running = task_current(rq, tsk);
	on_rq = tsk->se.on_rq;

	if (on_rq)
		dequeue_task(rq, tsk, 0);
	if (unlikely(running))
		tsk->sched_class->put_prev_task(rq, tsk);

	set_task_rq(tsk, task_cpu(tsk));

#ifdef CONFIG_FAIR_GROUP_SCHED
	if (tsk->sched_class->moved_group)
		tsk->sched_class->moved_group(tsk, on_rq);
#endif

	if (unlikely(running))
		tsk->sched_class->set_curr_task(rq);
	if (on_rq)
		enqueue_task(rq, tsk, 0);

	task_rq_unlock(rq, &flags);
}
#endif /* CONFIG_CGROUP_SCHED */

#ifdef CONFIG_FAIR_GROUP_SCHED
static void __set_se_shares(struct sched_entity *se, unsigned long shares)
{
	struct cfs_rq *cfs_rq = se->cfs_rq;
	int on_rq;

	on_rq = se->on_rq;
	if (on_rq)
		dequeue_entity(cfs_rq, se, 0);

	se->load.weight = shares;
	se->load.inv_weight = 0;

	if (on_rq)
		enqueue_entity(cfs_rq, se, 0);
}

static void set_se_shares(struct sched_entity *se, unsigned long shares)
{
	struct cfs_rq *cfs_rq = se->cfs_rq;
	struct rq *rq = cfs_rq->rq;
	unsigned long flags;

	raw_spin_lock_irqsave(&rq->lock, flags);
	__set_se_shares(se, shares);
	raw_spin_unlock_irqrestore(&rq->lock, flags);
}

static DEFINE_MUTEX(shares_mutex);

int sched_group_set_shares(struct task_group *tg, unsigned long shares)
{
	int i;
	unsigned long flags;

	/*
	 * We can't change the weight of the root cgroup.
	 */
	if (!tg->se[0])
		return -EINVAL;

	if (shares < MIN_SHARES)
		shares = MIN_SHARES;
	else if (shares > MAX_SHARES)
		shares = MAX_SHARES;

	mutex_lock(&shares_mutex);
	if (tg->shares == shares)
		goto done;

	spin_lock_irqsave(&task_group_lock, flags);
	for_each_possible_cpu(i)
		unregister_fair_sched_group(tg, i);
	list_del_rcu(&tg->siblings);
	spin_unlock_irqrestore(&task_group_lock, flags);

	/* wait for any ongoing reference to this group to finish */
	synchronize_sched();

	/*
	 * Now we are free to modify the group's share on each cpu
	 * w/o tripping rebalance_share or load_balance_fair.
	 */
	tg->shares = shares;
	for_each_possible_cpu(i) {
		/*
		 * force a rebalance
		 */
		cfs_rq_set_shares(tg->cfs_rq[i], 0);
		set_se_shares(tg->se[i], shares);
	}

	/*
	 * Enable load balance activity on this group, by inserting it back on
	 * each cpu's rq->leaf_cfs_rq_list.
	 */
	spin_lock_irqsave(&task_group_lock, flags);
	for_each_possible_cpu(i)
		register_fair_sched_group(tg, i);
	list_add_rcu(&tg->siblings, &tg->parent->children);
	spin_unlock_irqrestore(&task_group_lock, flags);
done:
	mutex_unlock(&shares_mutex);
	return 0;
}

unsigned long sched_group_shares(struct task_group *tg)
{
	return tg->shares;
}
#endif

#ifdef CONFIG_RT_GROUP_SCHED
/*
 * Ensure that the real time constraints are schedulable.
 */
static DEFINE_MUTEX(rt_constraints_mutex);

static unsigned long to_ratio(u64 period, u64 runtime)
{
	if (runtime == RUNTIME_INF)
		return 1ULL << 20;

	return div64_u64(runtime << 20, period);
}

/* Must be called with tasklist_lock held */
static inline int tg_has_rt_tasks(struct task_group *tg)
{
	struct task_struct *g, *p;

	do_each_thread(g, p) {
		if (rt_task(p) && rt_rq_of_se(&p->rt)->tg == tg)
			return 1;
	} while_each_thread(g, p);

	return 0;
}

struct rt_schedulable_data {
	struct task_group *tg;
	u64 rt_period;
	u64 rt_runtime;
};

static int tg_schedulable(struct task_group *tg, void *data)
{
	struct rt_schedulable_data *d = data;
	struct task_group *child;
	unsigned long total, sum = 0;
	u64 period, runtime;

	period = ktime_to_ns(tg->rt_bandwidth.rt_period);
	runtime = tg->rt_bandwidth.rt_runtime;

	if (tg == d->tg) {
		period = d->rt_period;
		runtime = d->rt_runtime;
	}

	/*
	 * Cannot have more runtime than the period.
	 */
	if (runtime > period && runtime != RUNTIME_INF)
		return -EINVAL;

	/*
	 * Ensure we don't starve existing RT tasks.
	 */
	if (rt_bandwidth_enabled() && !runtime && tg_has_rt_tasks(tg))
		return -EBUSY;

	total = to_ratio(period, runtime);

	/*
	 * Nobody can have more than the global setting allows.
	 */
	if (total > to_ratio(global_rt_period(), global_rt_runtime()))
		return -EINVAL;

	/*
	 * The sum of our children's runtime should not exceed our own.
	 */
	list_for_each_entry_rcu(child, &tg->children, siblings) {
		period = ktime_to_ns(child->rt_bandwidth.rt_period);
		runtime = child->rt_bandwidth.rt_runtime;

		if (child == d->tg) {
			period = d->rt_period;
			runtime = d->rt_runtime;
		}

		sum += to_ratio(period, runtime);
	}

	if (sum > total)
		return -EINVAL;

	return 0;
}

static int __rt_schedulable(struct task_group *tg, u64 period, u64 runtime)
{
	struct rt_schedulable_data data = {
		.tg = tg,
		.rt_period = period,
		.rt_runtime = runtime,
	};

	return walk_tg_tree(tg_schedulable, tg_nop, &data);
}

static int tg_set_bandwidth(struct task_group *tg,
		u64 rt_period, u64 rt_runtime)
{
	int i, err = 0;

	mutex_lock(&rt_constraints_mutex);
	read_lock(&tasklist_lock);
	err = __rt_schedulable(tg, rt_period, rt_runtime);
	if (err)
		goto unlock;

	raw_spin_lock_irq(&tg->rt_bandwidth.rt_runtime_lock);
	tg->rt_bandwidth.rt_period = ns_to_ktime(rt_period);
	tg->rt_bandwidth.rt_runtime = rt_runtime;

	for_each_possible_cpu(i) {
		struct rt_rq *rt_rq = tg->rt_rq[i];

		raw_spin_lock(&rt_rq->rt_runtime_lock);
		rt_rq->rt_runtime = rt_runtime;
		raw_spin_unlock(&rt_rq->rt_runtime_lock);
	}
	raw_spin_unlock_irq(&tg->rt_bandwidth.rt_runtime_lock);
 unlock:
	read_unlock(&tasklist_lock);
	mutex_unlock(&rt_constraints_mutex);

	return err;
}

int sched_group_set_rt_runtime(struct task_group *tg, long rt_runtime_us)
{
	u64 rt_runtime, rt_period;

	rt_period = ktime_to_ns(tg->rt_bandwidth.rt_period);
	rt_runtime = (u64)rt_runtime_us * NSEC_PER_USEC;
	if (rt_runtime_us < 0)
		rt_runtime = RUNTIME_INF;

	return tg_set_bandwidth(tg, rt_period, rt_runtime);
}

long sched_group_rt_runtime(struct task_group *tg)
{
	u64 rt_runtime_us;

	if (tg->rt_bandwidth.rt_runtime == RUNTIME_INF)
		return -1;

	rt_runtime_us = tg->rt_bandwidth.rt_runtime;
	do_div(rt_runtime_us, NSEC_PER_USEC);
	return rt_runtime_us;
}

int sched_group_set_rt_period(struct task_group *tg, long rt_period_us)
{
	u64 rt_runtime, rt_period;

	rt_period = (u64)rt_period_us * NSEC_PER_USEC;
	rt_runtime = tg->rt_bandwidth.rt_runtime;

	if (rt_period == 0)
		return -EINVAL;

	return tg_set_bandwidth(tg, rt_period, rt_runtime);
}

long sched_group_rt_period(struct task_group *tg)
{
	u64 rt_period_us;

	rt_period_us = ktime_to_ns(tg->rt_bandwidth.rt_period);
	do_div(rt_period_us, NSEC_PER_USEC);
	return rt_period_us;
}

static int sched_rt_global_constraints(void)
{
	u64 runtime, period;
	int ret = 0;

	if (sysctl_sched_rt_period <= 0)
		return -EINVAL;

	runtime = global_rt_runtime();
	period = global_rt_period();

	/*
	 * Sanity check on the sysctl variables.
	 */
	if (runtime > period && runtime != RUNTIME_INF)
		return -EINVAL;

	mutex_lock(&rt_constraints_mutex);
	read_lock(&tasklist_lock);
	ret = __rt_schedulable(NULL, 0, 0);
	read_unlock(&tasklist_lock);
	mutex_unlock(&rt_constraints_mutex);

	return ret;
}

int sched_rt_can_attach(struct task_group *tg, struct task_struct *tsk)
{
	/* Don't accept realtime tasks when there is no way for them to run */
	if (rt_task(tsk) && tg->rt_bandwidth.rt_runtime == 0)
		return 0;

	return 1;
}

#else /* !CONFIG_RT_GROUP_SCHED */
static int sched_rt_global_constraints(void)
{
	unsigned long flags;
	int i;

	if (sysctl_sched_rt_period <= 0)
		return -EINVAL;

	/*
	 * There's always some RT tasks in the root group
	 * -- migration, kstopmachine etc..
	 */
	if (sysctl_sched_rt_runtime == 0)
		return -EBUSY;

	raw_spin_lock_irqsave(&def_rt_bandwidth.rt_runtime_lock, flags);
	for_each_possible_cpu(i) {
		struct rt_rq *rt_rq = &cpu_rq(i)->rt;

		raw_spin_lock(&rt_rq->rt_runtime_lock);
		rt_rq->rt_runtime = global_rt_runtime();
		raw_spin_unlock(&rt_rq->rt_runtime_lock);
	}
	raw_spin_unlock_irqrestore(&def_rt_bandwidth.rt_runtime_lock, flags);

	return 0;
}
#endif /* CONFIG_RT_GROUP_SCHED */

int sched_rt_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp,
		loff_t *ppos)
{
	int ret;
	int old_period, old_runtime;
	static DEFINE_MUTEX(mutex);

	mutex_lock(&mutex);
	old_period = sysctl_sched_rt_period;
	old_runtime = sysctl_sched_rt_runtime;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);

	if (!ret && write) {
		ret = sched_rt_global_constraints();
		if (ret) {
			sysctl_sched_rt_period = old_period;
			sysctl_sched_rt_runtime = old_runtime;
		} else {
			def_rt_bandwidth.rt_runtime = global_rt_runtime();
			def_rt_bandwidth.rt_period =
				ns_to_ktime(global_rt_period());
		}
	}
	mutex_unlock(&mutex);

	return ret;
}

#ifdef CONFIG_CGROUP_SCHED

/* return corresponding task_group object of a cgroup */
static inline struct task_group *cgroup_tg(struct cgroup *cgrp)
{
	return container_of(cgroup_subsys_state(cgrp, cpu_cgroup_subsys_id),
			    struct task_group, css);
}

static struct cgroup_subsys_state *
cpu_cgroup_create(struct cgroup_subsys *ss, struct cgroup *cgrp)
{
	struct task_group *tg, *parent;

	if (!cgrp->parent) {
		/* This is early initialization for the top cgroup */
		return &init_task_group.css;
	}

	parent = cgroup_tg(cgrp->parent);
	tg = sched_create_group(parent);
	if (IS_ERR(tg))
		return ERR_PTR(-ENOMEM);

	return &tg->css;
}

static void
cpu_cgroup_destroy(struct cgroup_subsys *ss, struct cgroup *cgrp)
{
	struct task_group *tg = cgroup_tg(cgrp);

	sched_destroy_group(tg);
}

static int
cpu_cgroup_can_attach_task(struct cgroup *cgrp, struct task_struct *tsk)
{
#ifdef CONFIG_RT_GROUP_SCHED
	if (!sched_rt_can_attach(cgroup_tg(cgrp), tsk))
		return -EINVAL;
#else
	/* We don't support RT-tasks being in separate groups */
	if (tsk->sched_class != &fair_sched_class)
		return -EINVAL;
#endif
	return 0;
}

static int
cpu_cgroup_can_attach(struct cgroup_subsys *ss, struct cgroup *cgrp,
		      struct task_struct *tsk, bool threadgroup)
{
	int retval = cpu_cgroup_can_attach_task(cgrp, tsk);
	if (retval)
		return retval;
	if (threadgroup) {
		struct task_struct *c;
		rcu_read_lock();
		list_for_each_entry_rcu(c, &tsk->thread_group, thread_group) {
			retval = cpu_cgroup_can_attach_task(cgrp, c);
			if (retval) {
				rcu_read_unlock();
				return retval;
			}
		}
		rcu_read_unlock();
	}
	return 0;
}

static void
cpu_cgroup_attach(struct cgroup_subsys *ss, struct cgroup *cgrp,
		  struct cgroup *old_cont, struct task_struct *tsk,
		  bool threadgroup)
{
	sched_move_task(tsk);
	if (threadgroup) {
		struct task_struct *c;
		rcu_read_lock();
		list_for_each_entry_rcu(c, &tsk->thread_group, thread_group) {
			sched_move_task(c);
		}
		rcu_read_unlock();
	}
}

#ifdef CONFIG_FAIR_GROUP_SCHED
static int cpu_shares_write_u64(struct cgroup *cgrp, struct cftype *cftype,
				u64 shareval)
{
	return sched_group_set_shares(cgroup_tg(cgrp), shareval);
}

static u64 cpu_shares_read_u64(struct cgroup *cgrp, struct cftype *cft)
{
	struct task_group *tg = cgroup_tg(cgrp);

	return (u64) tg->shares;
}
#endif /* CONFIG_FAIR_GROUP_SCHED */

#ifdef CONFIG_RT_GROUP_SCHED
static int cpu_rt_runtime_write(struct cgroup *cgrp, struct cftype *cft,
				s64 val)
{
	return sched_group_set_rt_runtime(cgroup_tg(cgrp), val);
}

static s64 cpu_rt_runtime_read(struct cgroup *cgrp, struct cftype *cft)
{
	return sched_group_rt_runtime(cgroup_tg(cgrp));
}

static int cpu_rt_period_write_uint(struct cgroup *cgrp, struct cftype *cftype,
		u64 rt_period_us)
{
	return sched_group_set_rt_period(cgroup_tg(cgrp), rt_period_us);
}

static u64 cpu_rt_period_read_uint(struct cgroup *cgrp, struct cftype *cft)
{
	return sched_group_rt_period(cgroup_tg(cgrp));
}
#endif /* CONFIG_RT_GROUP_SCHED */

static struct cftype cpu_files[] = {
#ifdef CONFIG_FAIR_GROUP_SCHED
	{
		.name = "shares",
		.read_u64 = cpu_shares_read_u64,
		.write_u64 = cpu_shares_write_u64,
	},
#endif
#ifdef CONFIG_RT_GROUP_SCHED
	{
		.name = "rt_runtime_us",
		.read_s64 = cpu_rt_runtime_read,
		.write_s64 = cpu_rt_runtime_write,
	},
	{
		.name = "rt_period_us",
		.read_u64 = cpu_rt_period_read_uint,
		.write_u64 = cpu_rt_period_write_uint,
	},
#endif
};

static int cpu_cgroup_populate(struct cgroup_subsys *ss, struct cgroup *cont)
{
	return cgroup_add_files(cont, ss, cpu_files, ARRAY_SIZE(cpu_files));
}

struct cgroup_subsys cpu_cgroup_subsys = {
	.name		= "cpu",
	.create		= cpu_cgroup_create,
	.destroy	= cpu_cgroup_destroy,
	.can_attach	= cpu_cgroup_can_attach,
	.attach		= cpu_cgroup_attach,
	.populate	= cpu_cgroup_populate,
	.subsys_id	= cpu_cgroup_subsys_id,
	.early_init	= 1,
};

#endif	/* CONFIG_CGROUP_SCHED */

#ifdef CONFIG_CGROUP_CPUACCT

/*
 * CPU accounting code for task groups.
 *
 * Based on the work by Paul Menage (menage@google.com) and Balbir Singh
 * (balbir@in.ibm.com).
 */

/* track cpu usage of a group of tasks and its child groups */
struct cpuacct {
	struct cgroup_subsys_state css;
	/* cpuusage holds pointer to a u64-type object on every cpu */
	u64 __percpu *cpuusage;
	struct percpu_counter cpustat[CPUACCT_STAT_NSTATS];
	struct cpuacct *parent;
};

struct cgroup_subsys cpuacct_subsys;

/* return cpu accounting group corresponding to this container */
static inline struct cpuacct *cgroup_ca(struct cgroup *cgrp)
{
	return container_of(cgroup_subsys_state(cgrp, cpuacct_subsys_id),
			    struct cpuacct, css);
}

/* return cpu accounting group to which this task belongs */
static inline struct cpuacct *task_ca(struct task_struct *tsk)
{
	return container_of(task_subsys_state(tsk, cpuacct_subsys_id),
			    struct cpuacct, css);
}

/* create a new cpu accounting group */
static struct cgroup_subsys_state *cpuacct_create(
	struct cgroup_subsys *ss, struct cgroup *cgrp)
{
	struct cpuacct *ca = kzalloc(sizeof(*ca), GFP_KERNEL);
	int i;

	if (!ca)
		goto out;

	ca->cpuusage = alloc_percpu(u64);
	if (!ca->cpuusage)
		goto out_free_ca;

	for (i = 0; i < CPUACCT_STAT_NSTATS; i++)
		if (percpu_counter_init(&ca->cpustat[i], 0))
			goto out_free_counters;

	if (cgrp->parent)
		ca->parent = cgroup_ca(cgrp->parent);

	return &ca->css;

out_free_counters:
	while (--i >= 0)
		percpu_counter_destroy(&ca->cpustat[i]);
	free_percpu(ca->cpuusage);
out_free_ca:
	kfree(ca);
out:
	return ERR_PTR(-ENOMEM);
}

/* destroy an existing cpu accounting group */
static void
cpuacct_destroy(struct cgroup_subsys *ss, struct cgroup *cgrp)
{
	struct cpuacct *ca = cgroup_ca(cgrp);
	int i;

	for (i = 0; i < CPUACCT_STAT_NSTATS; i++)
		percpu_counter_destroy(&ca->cpustat[i]);
	free_percpu(ca->cpuusage);
	kfree(ca);
}

static u64 cpuacct_cpuusage_read(struct cpuacct *ca, int cpu)
{
	u64 *cpuusage = per_cpu_ptr(ca->cpuusage, cpu);
	u64 data;

#ifndef CONFIG_64BIT
	/*
	 * Take rq->lock to make 64-bit read safe on 32-bit platforms.
	 */
	raw_spin_lock_irq(&cpu_rq(cpu)->lock);
	data = *cpuusage;
	raw_spin_unlock_irq(&cpu_rq(cpu)->lock);
#else
	data = *cpuusage;
#endif

	return data;
}

static void cpuacct_cpuusage_write(struct cpuacct *ca, int cpu, u64 val)
{
	u64 *cpuusage = per_cpu_ptr(ca->cpuusage, cpu);

#ifndef CONFIG_64BIT
	/*
	 * Take rq->lock to make 64-bit write safe on 32-bit platforms.
	 */
	raw_spin_lock_irq(&cpu_rq(cpu)->lock);
	*cpuusage = val;
	raw_spin_unlock_irq(&cpu_rq(cpu)->lock);
#else
	*cpuusage = val;
#endif
}

/* return total cpu usage (in nanoseconds) of a group */
static u64 cpuusage_read(struct cgroup *cgrp, struct cftype *cft)
{
	struct cpuacct *ca = cgroup_ca(cgrp);
	u64 totalcpuusage = 0;
	int i;

	for_each_present_cpu(i)
		totalcpuusage += cpuacct_cpuusage_read(ca, i);

	return totalcpuusage;
}

static int cpuusage_write(struct cgroup *cgrp, struct cftype *cftype,
								u64 reset)
{
	struct cpuacct *ca = cgroup_ca(cgrp);
	int err = 0;
	int i;

	if (reset) {
		err = -EINVAL;
		goto out;
	}

	for_each_present_cpu(i)
		cpuacct_cpuusage_write(ca, i, 0);

out:
	return err;
}

static int cpuacct_percpu_seq_read(struct cgroup *cgroup, struct cftype *cft,
				   struct seq_file *m)
{
	struct cpuacct *ca = cgroup_ca(cgroup);
	u64 percpu;
	int i;

	for_each_present_cpu(i) {
		percpu = cpuacct_cpuusage_read(ca, i);
		seq_printf(m, "%llu ", (unsigned long long) percpu);
	}
	seq_printf(m, "\n");
	return 0;
}

static const char *cpuacct_stat_desc[] = {
	[CPUACCT_STAT_USER] = "user",
	[CPUACCT_STAT_SYSTEM] = "system",
};

static int cpuacct_stats_show(struct cgroup *cgrp, struct cftype *cft,
		struct cgroup_map_cb *cb)
{
	struct cpuacct *ca = cgroup_ca(cgrp);
	int i;

	for (i = 0; i < CPUACCT_STAT_NSTATS; i++) {
		s64 val = percpu_counter_read(&ca->cpustat[i]);
		val = cputime64_to_clock_t(val);
		cb->fill(cb, cpuacct_stat_desc[i], val);
	}
	return 0;
}

static struct cftype files[] = {
	{
		.name = "usage",
		.read_u64 = cpuusage_read,
		.write_u64 = cpuusage_write,
	},
	{
		.name = "usage_percpu",
		.read_seq_string = cpuacct_percpu_seq_read,
	},
	{
		.name = "stat",
		.read_map = cpuacct_stats_show,
	},
};

static int cpuacct_populate(struct cgroup_subsys *ss, struct cgroup *cgrp)
{
	return cgroup_add_files(cgrp, ss, files, ARRAY_SIZE(files));
}

/*
 * charge this task's execution time to its accounting group.
 *
 * called with rq->lock held.
 */
static void cpuacct_charge(struct task_struct *tsk, u64 cputime)
{
	struct cpuacct *ca;
	int cpu;

	if (unlikely(!cpuacct_subsys.active))
		return;

	cpu = task_cpu(tsk);

	rcu_read_lock();

	ca = task_ca(tsk);

	for (; ca; ca = ca->parent) {
		u64 *cpuusage = per_cpu_ptr(ca->cpuusage, cpu);
		*cpuusage += cputime;
	}

	rcu_read_unlock();
}

/*
 * When CONFIG_VIRT_CPU_ACCOUNTING is enabled one jiffy can be very large
 * in cputime_t units. As a result, cpuacct_update_stats calls
 * percpu_counter_add with values large enough to always overflow the
 * per cpu batch limit causing bad SMP scalability.
 *
 * To fix this we scale percpu_counter_batch by cputime_one_jiffy so we
 * batch the same amount of time with CONFIG_VIRT_CPU_ACCOUNTING disabled
 * and enabled. We cap it at INT_MAX which is the largest allowed batch value.
 */
#ifdef CONFIG_SMP
#define CPUACCT_BATCH	\
	min_t(long, percpu_counter_batch * cputime_one_jiffy, INT_MAX)
#else
#define CPUACCT_BATCH	0
#endif

/*
 * Charge the system/user time to the task's accounting group.
 */
static void cpuacct_update_stats(struct task_struct *tsk,
		enum cpuacct_stat_index idx, cputime_t val)
{
	struct cpuacct *ca;
	int batch = CPUACCT_BATCH;

	if (unlikely(!cpuacct_subsys.active))
		return;

	rcu_read_lock();
	ca = task_ca(tsk);

	do {
		__percpu_counter_add(&ca->cpustat[idx], val, batch);
		ca = ca->parent;
	} while (ca);
	rcu_read_unlock();
}

struct cgroup_subsys cpuacct_subsys = {
	.name = "cpuacct",
	.create = cpuacct_create,
	.destroy = cpuacct_destroy,
	.populate = cpuacct_populate,
	.subsys_id = cpuacct_subsys_id,
};
#endif	/* CONFIG_CGROUP_CPUACCT */

#ifndef CONFIG_SMP

void synchronize_sched_expedited(void)
{
	barrier();
}
EXPORT_SYMBOL_GPL(synchronize_sched_expedited);

#else /* #ifndef CONFIG_SMP */

static atomic_t synchronize_sched_expedited_count = ATOMIC_INIT(0);

static int synchronize_sched_expedited_cpu_stop(void *data)
{
	/*
	 * There must be a full memory barrier on each affected CPU
	 * between the time that try_stop_cpus() is called and the
	 * time that it returns.
	 *
	 * In the current initial implementation of cpu_stop, the
	 * above condition is already met when the control reaches
	 * this point and the following smp_mb() is not strictly
	 * necessary.  Do smp_mb() anyway for documentation and
	 * robustness against future implementation changes.
	 */
	smp_mb(); /* See above comment block. */
	return 0;
}

/*
 * Wait for an rcu-sched grace period to elapse, but use "big hammer"
 * approach to force grace period to end quickly.  This consumes
 * significant time on all CPUs, and is thus not recommended for
 * any sort of common-case code.
 *
 * Note that it is illegal to call this function while holding any
 * lock that is acquired by a CPU-hotplug notifier.  Failing to
 * observe this restriction will result in deadlock.
 */
void synchronize_sched_expedited(void)
{
	int snap, trycount = 0;

	smp_mb();  /* ensure prior mod happens before capturing snap. */
	snap = atomic_read(&synchronize_sched_expedited_count) + 1;
	get_online_cpus();
	while (try_stop_cpus(cpu_online_mask,
			     synchronize_sched_expedited_cpu_stop,
			     NULL) == -EAGAIN) {
		put_online_cpus();
		if (trycount++ < 10)
			udelay(trycount * num_online_cpus());
		else {
			synchronize_sched();
			return;
		}
		if (atomic_read(&synchronize_sched_expedited_count) - snap > 0) {
			smp_mb(); /* ensure test happens before caller kfree */
			return;
		}
		get_online_cpus();
	}
	atomic_inc(&synchronize_sched_expedited_count);
	smp_mb__after_atomic_inc(); /* ensure post-GP actions seen after GP. */
	put_online_cpus();
}
EXPORT_SYMBOL_GPL(synchronize_sched_expedited);

#endif /* #else #ifndef CONFIG_SMP */
