/* Modified by Broadcom Corp. Portions Copyright (c) Broadcom Corp, 2012. */
/*
 * kernel/workqueue.c - generic async execution with shared worker pool
 *
 * Copyright (C) 2002		Ingo Molnar
 *
 *   Derived from the taskqueue/keventd code by:
 *     David Woodhouse <dwmw2@infradead.org>
 *     Andrew Morton
 *     Kai Petzke <wpp@marie.physik.tu-berlin.de>
 *     Theodore Ts'o <tytso@mit.edu>
 *
 * Made to use alloc_percpu by Christoph Lameter.
 *
 * Copyright (C) 2010		SUSE Linux Products GmbH
 * Copyright (C) 2010		Tejun Heo <tj@kernel.org>
 *
 * This is the generic async execution mechanism.  Work items as are
 * executed in process context.  The worker pool is shared and
 * automatically managed.  There is one worker pool for each CPU and
 * one extra for works which are better served by workers which are
 * not bound to any specific CPU.
 *
 * Please read Documentation/workqueue.txt for details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/signal.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/notifier.h>
#include <linux/kthread.h>
#include <linux/hardirq.h>
#include <linux/mempolicy.h>
#include <linux/freezer.h>
#include <linux/kallsyms.h>
#include <linux/debug_locks.h>
#include <linux/lockdep.h>
#include <linux/idr.h>

#define CREATE_TRACE_POINTS
#include <trace/events/workqueue.h>

#include "workqueue_sched.h"

#if defined(CONFIG_BUZZZ)
#include <asm/buzzz.h>
#endif	/*  CONFIG_BUZZZ */

enum {
	/* global_cwq flags */
	GCWQ_MANAGE_WORKERS	= 1 << 0,	/* need to manage workers */
	GCWQ_MANAGING_WORKERS	= 1 << 1,	/* managing workers */
	GCWQ_DISASSOCIATED	= 1 << 2,	/* cpu can't serve workers */
	GCWQ_FREEZING		= 1 << 3,	/* freeze in progress */
	GCWQ_HIGHPRI_PENDING	= 1 << 4,	/* highpri works on queue */

	/* worker flags */
	WORKER_STARTED		= 1 << 0,	/* started */
	WORKER_DIE		= 1 << 1,	/* die die die */
	WORKER_IDLE		= 1 << 2,	/* is idle */
	WORKER_PREP		= 1 << 3,	/* preparing to run works */
	WORKER_ROGUE		= 1 << 4,	/* not bound to any cpu */
	WORKER_REBIND		= 1 << 5,	/* mom is home, come back */
	WORKER_CPU_INTENSIVE	= 1 << 6,	/* cpu intensive */
	WORKER_UNBOUND		= 1 << 7,	/* worker is unbound */

	WORKER_NOT_RUNNING	= WORKER_PREP | WORKER_ROGUE | WORKER_REBIND |
				  WORKER_CPU_INTENSIVE | WORKER_UNBOUND,

	/* gcwq->trustee_state */
	TRUSTEE_START		= 0,		/* start */
	TRUSTEE_IN_CHARGE	= 1,		/* trustee in charge of gcwq */
	TRUSTEE_BUTCHER		= 2,		/* butcher workers */
	TRUSTEE_RELEASE		= 3,		/* release workers */
	TRUSTEE_DONE		= 4,		/* trustee is done */

	BUSY_WORKER_HASH_ORDER	= 6,		/* 64 pointers */
	BUSY_WORKER_HASH_SIZE	= 1 << BUSY_WORKER_HASH_ORDER,
	BUSY_WORKER_HASH_MASK	= BUSY_WORKER_HASH_SIZE - 1,

	MAX_IDLE_WORKERS_RATIO	= 4,		/* 1/4 of busy can be idle */
	IDLE_WORKER_TIMEOUT	= 300 * HZ,	/* keep idle ones for 5 mins */

	MAYDAY_INITIAL_TIMEOUT	= HZ / 100,	/* call for help after 10ms */
	MAYDAY_INTERVAL		= HZ / 10,	/* and then every 100ms */
	CREATE_COOLDOWN		= HZ,		/* time to breath after fail */
	TRUSTEE_COOLDOWN	= HZ / 10,	/* for trustee draining */

	/*
	 * Rescue workers are used only on emergencies and shared by
	 * all cpus.  Give -20.
	 */
	RESCUER_NICE_LEVEL	= -20,
};

/*
 * Structure fields follow one of the following exclusion rules.
 *
 * I: Modifiable by initialization/destruction paths and read-only for
 *    everyone else.
 *
 * P: Preemption protected.  Disabling preemption is enough and should
 *    only be modified and accessed from the local cpu.
 *
 * L: gcwq->lock protected.  Access with gcwq->lock held.
 *
 * X: During normal operation, modification requires gcwq->lock and
 *    should be done only from local cpu.  Either disabling preemption
 *    on local cpu or grabbing gcwq->lock is enough for read access.
 *    If GCWQ_DISASSOCIATED is set, it's identical to L.
 *
 * F: wq->flush_mutex protected.
 *
 * W: workqueue_lock protected.
 */

struct global_cwq;

/*
 * The poor guys doing the actual heavy lifting.  All on-duty workers
 * are either serving the manager role, on idle list or on busy hash.
 */
struct worker {
	/* on idle list while idle, on busy hash table while busy */
	union {
		struct list_head	entry;	/* L: while idle */
		struct hlist_node	hentry;	/* L: while busy */
	};

	struct work_struct	*current_work;	/* L: work being processed */
	struct cpu_workqueue_struct *current_cwq; /* L: current_work's cwq */
	struct list_head	scheduled;	/* L: scheduled works */
	struct task_struct	*task;		/* I: worker task */
	struct global_cwq	*gcwq;		/* I: the associated gcwq */
	/* 64 bytes boundary on 64bit, 32 on 32bit */
	unsigned long		last_active;	/* L: last active timestamp */
	unsigned int		flags;		/* X: flags */
	int			id;		/* I: worker id */
	struct work_struct	rebind_work;	/* L: rebind worker to cpu */
};

/*
 * Global per-cpu workqueue.  There's one and only one for each cpu
 * and all works are queued and processed here regardless of their
 * target workqueues.
 */
struct global_cwq {
	spinlock_t		lock;		/* the gcwq lock */
	struct list_head	worklist;	/* L: list of pending works */
	unsigned int		cpu;		/* I: the associated cpu */
	unsigned int		flags;		/* L: GCWQ_* flags */

	int			nr_workers;	/* L: total number of workers */
	int			nr_idle;	/* L: currently idle ones */

	/* workers are chained either in the idle_list or busy_hash */
	struct list_head	idle_list;	/* X: list of idle workers */
	struct hlist_head	busy_hash[BUSY_WORKER_HASH_SIZE];
						/* L: hash of busy workers */

	struct timer_list	idle_timer;	/* L: worker idle timeout */
	struct timer_list	mayday_timer;	/* L: SOS timer for dworkers */

	struct ida		worker_ida;	/* L: for worker IDs */

	struct task_struct	*trustee;	/* L: for gcwq shutdown */
	unsigned int		trustee_state;	/* L: trustee state */
	wait_queue_head_t	trustee_wait;	/* trustee wait */
	struct worker		*first_idle;	/* L: first idle worker */
} ____cacheline_aligned_in_smp;

/*
 * The per-CPU workqueue.  The lower WORK_STRUCT_FLAG_BITS of
 * work_struct->data are used for flags and thus cwqs need to be
 * aligned at two's power of the number of flag bits.
 */
struct cpu_workqueue_struct {
	struct global_cwq	*gcwq;		/* I: the associated gcwq */
	struct workqueue_struct *wq;		/* I: the owning workqueue */
	int			work_color;	/* L: current color */
	int			flush_color;	/* L: flushing color */
	int			nr_in_flight[WORK_NR_COLORS];
						/* L: nr of in_flight works */
	int			nr_active;	/* L: nr of active works */
	int			max_active;	/* L: max active works */
	struct list_head	delayed_works;	/* L: delayed works */
};

/*
 * Structure used to wait for workqueue flush.
 */
struct wq_flusher {
	struct list_head	list;		/* F: list of flushers */
	int			flush_color;	/* F: flush color waiting for */
	struct completion	done;		/* flush completion */
};

/*
 * All cpumasks are assumed to be always set on UP and thus can't be
 * used to determine whether there's something to be done.
 */
#ifdef CONFIG_SMP
typedef cpumask_var_t mayday_mask_t;
#define mayday_test_and_set_cpu(cpu, mask)	\
	cpumask_test_and_set_cpu((cpu), (mask))
#define mayday_clear_cpu(cpu, mask)		cpumask_clear_cpu((cpu), (mask))
#define for_each_mayday_cpu(cpu, mask)		for_each_cpu((cpu), (mask))
#define alloc_mayday_mask(maskp, gfp)		zalloc_cpumask_var((maskp), (gfp))
#define free_mayday_mask(mask)			free_cpumask_var((mask))
#else
typedef unsigned long mayday_mask_t;
#define mayday_test_and_set_cpu(cpu, mask)	test_and_set_bit(0, &(mask))
#define mayday_clear_cpu(cpu, mask)		clear_bit(0, &(mask))
#define for_each_mayday_cpu(cpu, mask)		if ((cpu) = 0, (mask))
#define alloc_mayday_mask(maskp, gfp)		true
#define free_mayday_mask(mask)			do { } while (0)
#endif

/*
 * The externally visible workqueue abstraction is an array of
 * per-CPU workqueues:
 */
struct workqueue_struct {
	unsigned int		flags;		/* I: WQ_* flags */
	union {
		struct cpu_workqueue_struct __percpu	*pcpu;
		struct cpu_workqueue_struct		*single;
		unsigned long				v;
	} cpu_wq;				/* I: cwq's */
	struct list_head	list;		/* W: list of all workqueues */

	struct mutex		flush_mutex;	/* protects wq flushing */
	int			work_color;	/* F: current work color */
	int			flush_color;	/* F: current flush color */
	atomic_t		nr_cwqs_to_flush; /* flush in progress */
	struct wq_flusher	*first_flusher;	/* F: first flusher */
	struct list_head	flusher_queue;	/* F: flush waiters */
	struct list_head	flusher_overflow; /* F: flush overflow list */

	mayday_mask_t		mayday_mask;	/* cpus requesting rescue */
	struct worker		*rescuer;	/* I: rescue worker */

	int			saved_max_active; /* W: saved cwq max_active */
	const char		*name;		/* I: workqueue name */
#ifdef CONFIG_LOCKDEP
	struct lockdep_map	lockdep_map;
#endif
};

struct workqueue_struct *system_wq __read_mostly;
struct workqueue_struct *system_long_wq __read_mostly;
struct workqueue_struct *system_nrt_wq __read_mostly;
struct workqueue_struct *system_unbound_wq __read_mostly;
EXPORT_SYMBOL_GPL(system_wq);
EXPORT_SYMBOL_GPL(system_long_wq);
EXPORT_SYMBOL_GPL(system_nrt_wq);
EXPORT_SYMBOL_GPL(system_unbound_wq);

#define for_each_busy_worker(worker, i, pos, gcwq)			\
	for (i = 0; i < BUSY_WORKER_HASH_SIZE; i++)			\
		hlist_for_each_entry(worker, pos, &gcwq->busy_hash[i], hentry)

static inline int __next_gcwq_cpu(int cpu, const struct cpumask *mask,
				  unsigned int sw)
{
	if (cpu < nr_cpu_ids) {
		if (sw & 1) {
			cpu = cpumask_next(cpu, mask);
			if (cpu < nr_cpu_ids)
				return cpu;
		}
		if (sw & 2)
			return WORK_CPU_UNBOUND;
	}
	return WORK_CPU_NONE;
}

static inline int __next_wq_cpu(int cpu, const struct cpumask *mask,
				struct workqueue_struct *wq)
{
	return __next_gcwq_cpu(cpu, mask, !(wq->flags & WQ_UNBOUND) ? 1 : 2);
}

/*
 * CPU iterators
 *
 * An extra gcwq is defined for an invalid cpu number
 * (WORK_CPU_UNBOUND) to host workqueues which are not bound to any
 * specific CPU.  The following iterators are similar to
 * for_each_*_cpu() iterators but also considers the unbound gcwq.
 *
 * for_each_gcwq_cpu()		: possible CPUs + WORK_CPU_UNBOUND
 * for_each_online_gcwq_cpu()	: online CPUs + WORK_CPU_UNBOUND
 * for_each_cwq_cpu()		: possible CPUs for bound workqueues,
 *				  WORK_CPU_UNBOUND for unbound workqueues
 */
#define for_each_gcwq_cpu(cpu)						\
	for ((cpu) = __next_gcwq_cpu(-1, cpu_possible_mask, 3);		\
	     (cpu) < WORK_CPU_NONE;					\
	     (cpu) = __next_gcwq_cpu((cpu), cpu_possible_mask, 3))

#define for_each_online_gcwq_cpu(cpu)					\
	for ((cpu) = __next_gcwq_cpu(-1, cpu_online_mask, 3);		\
	     (cpu) < WORK_CPU_NONE;					\
	     (cpu) = __next_gcwq_cpu((cpu), cpu_online_mask, 3))

#define for_each_cwq_cpu(cpu, wq)					\
	for ((cpu) = __next_wq_cpu(-1, cpu_possible_mask, (wq));	\
	     (cpu) < WORK_CPU_NONE;					\
	     (cpu) = __next_wq_cpu((cpu), cpu_possible_mask, (wq)))

#ifdef CONFIG_LOCKDEP
/**
 * in_workqueue_context() - in context of specified workqueue?
 * @wq: the workqueue of interest
 *
 * Checks lockdep state to see if the current task is executing from
 * within a workqueue item.  This function exists only if lockdep is
 * enabled.
 */
int in_workqueue_context(struct workqueue_struct *wq)
{
	return lock_is_held(&wq->lockdep_map);
}
#endif

#ifdef CONFIG_DEBUG_OBJECTS_WORK

static struct debug_obj_descr work_debug_descr;

/*
 * fixup_init is called when:
 * - an active object is initialized
 */
static int work_fixup_init(void *addr, enum debug_obj_state state)
{
	struct work_struct *work = addr;

	switch (state) {
	case ODEBUG_STATE_ACTIVE:
		cancel_work_sync(work);
		debug_object_init(work, &work_debug_descr);
		return 1;
	default:
		return 0;
	}
}

/*
 * fixup_activate is called when:
 * - an active object is activated
 * - an unknown object is activated (might be a statically initialized object)
 */
static int work_fixup_activate(void *addr, enum debug_obj_state state)
{
	struct work_struct *work = addr;

	switch (state) {

	case ODEBUG_STATE_NOTAVAILABLE:
		/*
		 * This is not really a fixup. The work struct was
		 * statically initialized. We just make sure that it
		 * is tracked in the object tracker.
		 */
		if (test_bit(WORK_STRUCT_STATIC_BIT, work_data_bits(work))) {
			debug_object_init(work, &work_debug_descr);
			debug_object_activate(work, &work_debug_descr);
			return 0;
		}
		WARN_ON_ONCE(1);
		return 0;

	case ODEBUG_STATE_ACTIVE:
		WARN_ON(1);

	default:
		return 0;
	}
}

/*
 * fixup_free is called when:
 * - an active object is freed
 */
static int work_fixup_free(void *addr, enum debug_obj_state state)
{
	struct work_struct *work = addr;

	switch (state) {
	case ODEBUG_STATE_ACTIVE:
		cancel_work_sync(work);
		debug_object_free(work, &work_debug_descr);
		return 1;
	default:
		return 0;
	}
}

static struct debug_obj_descr work_debug_descr = {
	.name		= "work_struct",
	.fixup_init	= work_fixup_init,
	.fixup_activate	= work_fixup_activate,
	.fixup_free	= work_fixup_free,
};

static inline void debug_work_activate(struct work_struct *work)
{
	debug_object_activate(work, &work_debug_descr);
}

static inline void debug_work_deactivate(struct work_struct *work)
{
	debug_object_deactivate(work, &work_debug_descr);
}

void __init_work(struct work_struct *work, int onstack)
{
	if (onstack)
		debug_object_init_on_stack(work, &work_debug_descr);
	else
		debug_object_init(work, &work_debug_descr);
}
EXPORT_SYMBOL_GPL(__init_work);

void destroy_work_on_stack(struct work_struct *work)
{
	debug_object_free(work, &work_debug_descr);
}
EXPORT_SYMBOL_GPL(destroy_work_on_stack);

#else
static inline void debug_work_activate(struct work_struct *work) { }
static inline void debug_work_deactivate(struct work_struct *work) { }
#endif

/* Serializes the accesses to the list of workqueues. */
static DEFINE_SPINLOCK(workqueue_lock);
static LIST_HEAD(workqueues);
static bool workqueue_freezing;		/* W: have wqs started freezing? */

/*
 * The almighty global cpu workqueues.  nr_running is the only field
 * which is expected to be used frequently by other cpus via
 * try_to_wake_up().  Put it in a separate cacheline.
 */
static DEFINE_PER_CPU(struct global_cwq, global_cwq);
static DEFINE_PER_CPU_SHARED_ALIGNED(atomic_t, gcwq_nr_running);

/*
 * Global cpu workqueue and nr_running counter for unbound gcwq.  The
 * gcwq is always online, has GCWQ_DISASSOCIATED set, and all its
 * workers have WORKER_UNBOUND set.
 */
static struct global_cwq unbound_global_cwq;
static atomic_t unbound_gcwq_nr_running = ATOMIC_INIT(0);	/* always 0 */

static int worker_thread(void *__worker);

static struct global_cwq *get_gcwq(unsigned int cpu)
{
	if (cpu != WORK_CPU_UNBOUND)
		return &per_cpu(global_cwq, cpu);
	else
		return &unbound_global_cwq;
}

static atomic_t *get_gcwq_nr_running(unsigned int cpu)
{
	if (cpu != WORK_CPU_UNBOUND)
		return &per_cpu(gcwq_nr_running, cpu);
	else
		return &unbound_gcwq_nr_running;
}

static struct cpu_workqueue_struct *get_cwq(unsigned int cpu,
					    struct workqueue_struct *wq)
{
	if (!(wq->flags & WQ_UNBOUND)) {
		if (likely(cpu < nr_cpu_ids)) {
#ifdef CONFIG_SMP
			return per_cpu_ptr(wq->cpu_wq.pcpu, cpu);
#else
			return wq->cpu_wq.single;
#endif
		}
	} else if (likely(cpu == WORK_CPU_UNBOUND))
		return wq->cpu_wq.single;
	return NULL;
}

static unsigned int work_color_to_flags(int color)
{
	return color << WORK_STRUCT_COLOR_SHIFT;
}

static int get_work_color(struct work_struct *work)
{
	return (*work_data_bits(work) >> WORK_STRUCT_COLOR_SHIFT) &
		((1 << WORK_STRUCT_COLOR_BITS) - 1);
}

static int work_next_color(int color)
{
	return (color + 1) % WORK_NR_COLORS;
}

/*
 * A work's data points to the cwq with WORK_STRUCT_CWQ set while the
 * work is on queue.  Once execution starts, WORK_STRUCT_CWQ is
 * cleared and the work data contains the cpu number it was last on.
 *
 * set_work_{cwq|cpu}() and clear_work_data() can be used to set the
 * cwq, cpu or clear work->data.  These functions should only be
 * called while the work is owned - ie. while the PENDING bit is set.
 *
 * get_work_[g]cwq() can be used to obtain the gcwq or cwq
 * corresponding to a work.  gcwq is available once the work has been
 * queued anywhere after initialization.  cwq is available only from
 * queueing until execution starts.
 */
static inline void set_work_data(struct work_struct *work, unsigned long data,
				 unsigned long flags)
{
	BUG_ON(!work_pending(work));
	atomic_long_set(&work->data, data | flags | work_static(work));
}

static void set_work_cwq(struct work_struct *work,
			 struct cpu_workqueue_struct *cwq,
			 unsigned long extra_flags)
{
	set_work_data(work, (unsigned long)cwq,
		      WORK_STRUCT_PENDING | WORK_STRUCT_CWQ | extra_flags);
}

static void set_work_cpu(struct work_struct *work, unsigned int cpu)
{
	set_work_data(work, cpu << WORK_STRUCT_FLAG_BITS, WORK_STRUCT_PENDING);
}

static void clear_work_data(struct work_struct *work)
{
	set_work_data(work, WORK_STRUCT_NO_CPU, 0);
}

static struct cpu_workqueue_struct *get_work_cwq(struct work_struct *work)
{
	unsigned long data = atomic_long_read(&work->data);

	if (data & WORK_STRUCT_CWQ)
		return (void *)(data & WORK_STRUCT_WQ_DATA_MASK);
	else
		return NULL;
}

static struct global_cwq *get_work_gcwq(struct work_struct *work)
{
	unsigned long data = atomic_long_read(&work->data);
	unsigned int cpu;

	if (data & WORK_STRUCT_CWQ)
		return ((struct cpu_workqueue_struct *)
			(data & WORK_STRUCT_WQ_DATA_MASK))->gcwq;

	cpu = data >> WORK_STRUCT_FLAG_BITS;
	if (cpu == WORK_CPU_NONE)
		return NULL;

	BUG_ON(cpu >= nr_cpu_ids && cpu != WORK_CPU_UNBOUND);
	return get_gcwq(cpu);
}

/*
 * Policy functions.  These define the policies on how the global
 * worker pool is managed.  Unless noted otherwise, these functions
 * assume that they're being called with gcwq->lock held.
 */

static bool __need_more_worker(struct global_cwq *gcwq)
{
	return !atomic_read(get_gcwq_nr_running(gcwq->cpu)) ||
		gcwq->flags & GCWQ_HIGHPRI_PENDING;
}

/*
 * Need to wake up a worker?  Called from anything but currently
 * running workers.
 */
static bool need_more_worker(struct global_cwq *gcwq)
{
	return !list_empty(&gcwq->worklist) && __need_more_worker(gcwq);
}

/* Can I start working?  Called from busy but !running workers. */
static bool may_start_working(struct global_cwq *gcwq)
{
	return gcwq->nr_idle;
}

/* Do I need to keep working?  Called from currently running workers. */
static bool keep_working(struct global_cwq *gcwq)
{
	atomic_t *nr_running = get_gcwq_nr_running(gcwq->cpu);

	return !list_empty(&gcwq->worklist) && atomic_read(nr_running) <= 1;
}

/* Do we need a new worker?  Called from manager. */
static bool need_to_create_worker(struct global_cwq *gcwq)
{
	return need_more_worker(gcwq) && !may_start_working(gcwq);
}

/* Do I need to be the manager? */
static bool need_to_manage_workers(struct global_cwq *gcwq)
{
	return need_to_create_worker(gcwq) || gcwq->flags & GCWQ_MANAGE_WORKERS;
}

/* Do we have too many workers and should some go away? */
static bool too_many_workers(struct global_cwq *gcwq)
{
	bool managing = gcwq->flags & GCWQ_MANAGING_WORKERS;
	int nr_idle = gcwq->nr_idle + managing; /* manager is considered idle */
	int nr_busy = gcwq->nr_workers - nr_idle;

	return nr_idle > 2 && (nr_idle - 2) * MAX_IDLE_WORKERS_RATIO >= nr_busy;
}

/*
 * Wake up functions.
 */

/* Return the first worker.  Safe with preemption disabled */
static struct worker *first_worker(struct global_cwq *gcwq)
{
	if (unlikely(list_empty(&gcwq->idle_list)))
		return NULL;

	return list_first_entry(&gcwq->idle_list, struct worker, entry);
}

/**
 * wake_up_worker - wake up an idle worker
 * @gcwq: gcwq to wake worker for
 *
 * Wake up the first idle worker of @gcwq.
 *
 * CONTEXT:
 * spin_lock_irq(gcwq->lock).
 */
static void wake_up_worker(struct global_cwq *gcwq)
{
	struct worker *worker = first_worker(gcwq);

	if (likely(worker))
		wake_up_process(worker->task);
}

/**
 * wq_worker_waking_up - a worker is waking up
 * @task: task waking up
 * @cpu: CPU @task is waking up to
 *
 * This function is called during try_to_wake_up() when a worker is
 * being awoken.
 *
 * CONTEXT:
 * spin_lock_irq(rq->lock)
 */
void wq_worker_waking_up(struct task_struct *task, unsigned int cpu)
{
	struct worker *worker = kthread_data(task);

	if (likely(!(worker->flags & WORKER_NOT_RUNNING)))
		atomic_inc(get_gcwq_nr_running(cpu));
}

/**
 * wq_worker_sleeping - a worker is going to sleep
 * @task: task going to sleep
 * @cpu: CPU in question, must be the current CPU number
 *
 * This function is called during schedule() when a busy worker is
 * going to sleep.  Worker on the same cpu can be woken up by
 * returning pointer to its task.
 *
 * CONTEXT:
 * spin_lock_irq(rq->lock)
 *
 * RETURNS:
 * Worker task on @cpu to wake up, %NULL if none.
 */
struct task_struct *wq_worker_sleeping(struct task_struct *task,
				       unsigned int cpu)
{
	struct worker *worker = kthread_data(task), *to_wakeup = NULL;
	struct global_cwq *gcwq = get_gcwq(cpu);
	atomic_t *nr_running = get_gcwq_nr_running(cpu);

	if (unlikely(worker->flags & WORKER_NOT_RUNNING))
		return NULL;

	/* this can only happen on the local cpu */
	BUG_ON(cpu != raw_smp_processor_id());

	/*
	 * The counterpart of the following dec_and_test, implied mb,
	 * worklist not empty test sequence is in insert_work().
	 * Please read comment there.
	 *
	 * NOT_RUNNING is clear.  This means that trustee is not in
	 * charge and we're running on the local cpu w/ rq lock held
	 * and preemption disabled, which in turn means that none else
	 * could be manipulating idle_list, so dereferencing idle_list
	 * without gcwq lock is safe.
	 */
	if (atomic_dec_and_test(nr_running) && !list_empty(&gcwq->worklist))
		to_wakeup = first_worker(gcwq);
	return to_wakeup ? to_wakeup->task : NULL;
}

/**
 * worker_set_flags - set worker flags and adjust nr_running accordingly
 * @worker: self
 * @flags: flags to set
 * @wakeup: wakeup an idle worker if necessary
 *
 * Set @flags in @worker->flags and adjust nr_running accordingly.  If
 * nr_running becomes zero and @wakeup is %true, an idle worker is
 * woken up.
 *
 * CONTEXT:
 * spin_lock_irq(gcwq->lock)
 */
static inline void worker_set_flags(struct worker *worker, unsigned int flags,
				    bool wakeup)
{
	struct global_cwq *gcwq = worker->gcwq;

	WARN_ON_ONCE(worker->task != current);

	/*
	 * If transitioning into NOT_RUNNING, adjust nr_running and
	 * wake up an idle worker as necessary if requested by
	 * @wakeup.
	 */
	if ((flags & WORKER_NOT_RUNNING) &&
	    !(worker->flags & WORKER_NOT_RUNNING)) {
		atomic_t *nr_running = get_gcwq_nr_running(gcwq->cpu);

		if (wakeup) {
			if (atomic_dec_and_test(nr_running) &&
			    !list_empty(&gcwq->worklist))
				wake_up_worker(gcwq);
		} else
			atomic_dec(nr_running);
	}

	worker->flags |= flags;
}

/**
 * worker_clr_flags - clear worker flags and adjust nr_running accordingly
 * @worker: self
 * @flags: flags to clear
 *
 * Clear @flags in @worker->flags and adjust nr_running accordingly.
 *
 * CONTEXT:
 * spin_lock_irq(gcwq->lock)
 */
static inline void worker_clr_flags(struct worker *worker, unsigned int flags)
{
	struct global_cwq *gcwq = worker->gcwq;
	unsigned int oflags = worker->flags;

	WARN_ON_ONCE(worker->task != current);

	worker->flags &= ~flags;

	/* if transitioning out of NOT_RUNNING, increment nr_running */
	if ((flags & WORKER_NOT_RUNNING) && (oflags & WORKER_NOT_RUNNING))
		if (!(worker->flags & WORKER_NOT_RUNNING))
			atomic_inc(get_gcwq_nr_running(gcwq->cpu));
}

/**
 * busy_worker_head - return the busy hash head for a work
 * @gcwq: gcwq of interest
 * @work: work to be hashed
 *
 * Return hash head of @gcwq for @work.
 *
 * CONTEXT:
 * spin_lock_irq(gcwq->lock).
 *
 * RETURNS:
 * Pointer to the hash head.
 */
static struct hlist_head *busy_worker_head(struct global_cwq *gcwq,
					   struct work_struct *work)
{
	const int base_shift = ilog2(sizeof(struct work_struct));
	unsigned long v = (unsigned long)work;

	/* simple shift and fold hash, do we need something better? */
	v >>= base_shift;
	v += v >> BUSY_WORKER_HASH_ORDER;
	v &= BUSY_WORKER_HASH_MASK;

	return &gcwq->busy_hash[v];
}

/**
 * __find_worker_executing_work - find worker which is executing a work
 * @gcwq: gcwq of interest
 * @bwh: hash head as returned by busy_worker_head()
 * @work: work to find worker for
 *
 * Find a worker which is executing @work on @gcwq.  @bwh should be
 * the hash head obtained by calling busy_worker_head() with the same
 * work.
 *
 * CONTEXT:
 * spin_lock_irq(gcwq->lock).
 *
 * RETURNS:
 * Pointer to worker which is executing @work if found, NULL
 * otherwise.
 */
static struct worker *__find_worker_executing_work(struct global_cwq *gcwq,
						   struct hlist_head *bwh,
						   struct work_struct *work)
{
	struct worker *worker;
	struct hlist_node *tmp;

	hlist_for_each_entry(worker, tmp, bwh, hentry)
		if (worker->current_work == work)
			return worker;
	return NULL;
}

/**
 * find_worker_executing_work - find worker which is executing a work
 * @gcwq: gcwq of interest
 * @work: work to find worker for
 *
 * Find a worker which is executing @work on @gcwq.  This function is
 * identical to __find_worker_executing_work() except that this
 * function calculates @bwh itself.
 *
 * CONTEXT:
 * spin_lock_irq(gcwq->lock).
 *
 * RETURNS:
 * Pointer to worker which is executing @work if found, NULL
 * otherwise.
 */
static struct worker *find_worker_executing_work(struct global_cwq *gcwq,
						 struct work_struct *work)
{
	return __find_worker_executing_work(gcwq, busy_worker_head(gcwq, work),
					    work);
}

/**
 * gcwq_determine_ins_pos - find insertion position
 * @gcwq: gcwq of interest
 * @cwq: cwq a work is being queued for
 *
 * A work for @cwq is about to be queued on @gcwq, determine insertion
 * position for the work.  If @cwq is for HIGHPRI wq, the work is
 * queued at the head of the queue but in FIFO order with respect to
 * other HIGHPRI works; otherwise, at the end of the queue.  This
 * function also sets GCWQ_HIGHPRI_PENDING flag to hint @gcwq that
 * there are HIGHPRI works pending.
 *
 * CONTEXT:
 * spin_lock_irq(gcwq->lock).
 *
 * RETURNS:
 * Pointer to inserstion position.
 */
static inline struct list_head *gcwq_determine_ins_pos(struct global_cwq *gcwq,
					       struct cpu_workqueue_struct *cwq)
{
	struct work_struct *twork;

	if (likely(!(cwq->wq->flags & WQ_HIGHPRI)))
		return &gcwq->worklist;

	list_for_each_entry(twork, &gcwq->worklist, entry) {
		struct cpu_workqueue_struct *tcwq = get_work_cwq(twork);

		if (!(tcwq->wq->flags & WQ_HIGHPRI))
			break;
	}

	gcwq->flags |= GCWQ_HIGHPRI_PENDING;
	return &twork->entry;
}

/**
 * insert_work - insert a work into gcwq
 * @cwq: cwq @work belongs to
 * @work: work to insert
 * @head: insertion point
 * @extra_flags: extra WORK_STRUCT_* flags to set
 *
 * Insert @work which belongs to @cwq into @gcwq after @head.
 * @extra_flags is or'd to work_struct flags.
 *
 * CONTEXT:
 * spin_lock_irq(gcwq->lock).
 */
static void insert_work(struct cpu_workqueue_struct *cwq,
			struct work_struct *work, struct list_head *head,
			unsigned int extra_flags)
{
	struct global_cwq *gcwq = cwq->gcwq;

	/* we own @work, set data and link */
	set_work_cwq(work, cwq, extra_flags);

	/*
	 * Ensure that we get the right work->data if we see the
	 * result of list_add() below, see try_to_grab_pending().
	 */
	smp_wmb();

	list_add_tail(&work->entry, head);

	/*
	 * Ensure either worker_sched_deactivated() sees the above
	 * list_add_tail() or we see zero nr_running to avoid workers
	 * lying around lazily while there are works to be processed.
	 */
	smp_mb();

	if (__need_more_worker(gcwq))
		wake_up_worker(gcwq);
}

static void __queue_work(unsigned int cpu, struct workqueue_struct *wq,
			 struct work_struct *work)
{
	struct global_cwq *gcwq;
	struct cpu_workqueue_struct *cwq;
	struct list_head *worklist;
	unsigned int work_flags;
	unsigned long flags;

	debug_work_activate(work);

	if (WARN_ON_ONCE(wq->flags & WQ_DYING))
		return;

	/* determine gcwq to use */
	if (!(wq->flags & WQ_UNBOUND)) {
		struct global_cwq *last_gcwq;

		if (unlikely(cpu == WORK_CPU_UNBOUND))
			cpu = raw_smp_processor_id();

		/*
		 * It's multi cpu.  If @wq is non-reentrant and @work
		 * was previously on a different cpu, it might still
		 * be running there, in which case the work needs to
		 * be queued on that cpu to guarantee non-reentrance.
		 */
		gcwq = get_gcwq(cpu);
		if (wq->flags & WQ_NON_REENTRANT &&
		    (last_gcwq = get_work_gcwq(work)) && last_gcwq != gcwq) {
			struct worker *worker;

			spin_lock_irqsave(&last_gcwq->lock, flags);

			worker = find_worker_executing_work(last_gcwq, work);

			if (worker && worker->current_cwq->wq == wq)
				gcwq = last_gcwq;
			else {
				/* meh... not running there, queue here */
				spin_unlock_irqrestore(&last_gcwq->lock, flags);
				spin_lock_irqsave(&gcwq->lock, flags);
			}
		} else
			spin_lock_irqsave(&gcwq->lock, flags);
	} else {
		gcwq = get_gcwq(WORK_CPU_UNBOUND);
		spin_lock_irqsave(&gcwq->lock, flags);
	}

	/* gcwq determined, get cwq and queue */
	cwq = get_cwq(gcwq->cpu, wq);

	BUG_ON(!list_empty(&work->entry));

	cwq->nr_in_flight[cwq->work_color]++;
	work_flags = work_color_to_flags(cwq->work_color);

	if (likely(cwq->nr_active < cwq->max_active)) {
		cwq->nr_active++;
		worklist = gcwq_determine_ins_pos(gcwq, cwq);
	} else {
		work_flags |= WORK_STRUCT_DELAYED;
		worklist = &cwq->delayed_works;
	}

	insert_work(cwq, work, worklist, work_flags);

	spin_unlock_irqrestore(&gcwq->lock, flags);
}

/**
 * queue_work - queue work on a workqueue
 * @wq: workqueue to use
 * @work: work to queue
 *
 * Returns 0 if @work was already on a queue, non-zero otherwise.
 *
 * We queue the work to the CPU on which it was submitted, but if the CPU dies
 * it can be processed by another CPU.
 */
int queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
	int ret;

	ret = queue_work_on(get_cpu(), wq, work);
	put_cpu();

	return ret;
}
EXPORT_SYMBOL_GPL(queue_work);

/**
 * queue_work_on - queue work on specific cpu
 * @cpu: CPU number to execute work on
 * @wq: workqueue to use
 * @work: work to queue
 *
 * Returns 0 if @work was already on a queue, non-zero otherwise.
 *
 * We queue the work to a specific CPU, the caller must ensure it
 * can't go away.
 */
int
queue_work_on(int cpu, struct workqueue_struct *wq, struct work_struct *work)
{
	int ret = 0;

	if (!test_and_set_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work))) {
		__queue_work(cpu, wq, work);
		ret = 1;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(queue_work_on);

static void delayed_work_timer_fn(unsigned long __data)
{
	struct delayed_work *dwork = (struct delayed_work *)__data;
	struct cpu_workqueue_struct *cwq = get_work_cwq(&dwork->work);

	__queue_work(smp_processor_id(), cwq->wq, &dwork->work);
}

/**
 * queue_delayed_work - queue work on a workqueue after delay
 * @wq: workqueue to use
 * @dwork: delayable work to queue
 * @delay: number of jiffies to wait before queueing
 *
 * Returns 0 if @work was already on a queue, non-zero otherwise.
 */
int queue_delayed_work(struct workqueue_struct *wq,
			struct delayed_work *dwork, unsigned long delay)
{
	if (delay == 0)
		return queue_work(wq, &dwork->work);

	return queue_delayed_work_on(-1, wq, dwork, delay);
}
EXPORT_SYMBOL_GPL(queue_delayed_work);

/**
 * queue_delayed_work_on - queue work on specific CPU after delay
 * @cpu: CPU number to execute work on
 * @wq: workqueue to use
 * @dwork: work to queue
 * @delay: number of jiffies to wait before queueing
 *
 * Returns 0 if @work was already on a queue, non-zero otherwise.
 */
int queue_delayed_work_on(int cpu, struct workqueue_struct *wq,
			struct delayed_work *dwork, unsigned long delay)
{
	int ret = 0;
	struct timer_list *timer = &dwork->timer;
	struct work_struct *work = &dwork->work;

	if (!test_and_set_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work))) {
		unsigned int lcpu;

		BUG_ON(timer_pending(timer));
		BUG_ON(!list_empty(&work->entry));

		timer_stats_timer_set_start_info(&dwork->timer);

		/*
		 * This stores cwq for the moment, for the timer_fn.
		 * Note that the work's gcwq is preserved to allow
		 * reentrance detection for delayed works.
		 */
		if (!(wq->flags & WQ_UNBOUND)) {
			struct global_cwq *gcwq = get_work_gcwq(work);

			if (gcwq && gcwq->cpu != WORK_CPU_UNBOUND)
				lcpu = gcwq->cpu;
			else
				lcpu = raw_smp_processor_id();
		} else
			lcpu = WORK_CPU_UNBOUND;

		set_work_cwq(work, get_cwq(lcpu, wq), 0);

		timer->expires = jiffies + delay;
		timer->data = (unsigned long)dwork;
		timer->function = delayed_work_timer_fn;

		if (unlikely(cpu >= 0))
			add_timer_on(timer, cpu);
		else
			add_timer(timer);
		ret = 1;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(queue_delayed_work_on);

/**
 * worker_enter_idle - enter idle state
 * @worker: worker which is entering idle state
 *
 * @worker is entering idle state.  Update stats and idle timer if
 * necessary.
 *
 * LOCKING:
 * spin_lock_irq(gcwq->lock).
 */
static void worker_enter_idle(struct worker *worker)
{
	struct global_cwq *gcwq = worker->gcwq;

	BUG_ON(worker->flags & WORKER_IDLE);
	BUG_ON(!list_empty(&worker->entry) &&
	       (worker->hentry.next || worker->hentry.pprev));

	/* can't use worker_set_flags(), also called from start_worker() */
	worker->flags |= WORKER_IDLE;
	gcwq->nr_idle++;
	worker->last_active = jiffies;

	/* idle_list is LIFO */
	list_add(&worker->entry, &gcwq->idle_list);

	if (likely(!(worker->flags & WORKER_ROGUE))) {
		if (too_many_workers(gcwq) && !timer_pending(&gcwq->idle_timer))
			mod_timer(&gcwq->idle_timer,
				  jiffies + IDLE_WORKER_TIMEOUT);
	} else
		wake_up_all(&gcwq->trustee_wait);

	/* sanity check nr_running */
	WARN_ON_ONCE(gcwq->nr_workers == gcwq->nr_idle &&
		     atomic_read(get_gcwq_nr_running(gcwq->cpu)));
}

/**
 * worker_leave_idle - leave idle state
 * @worker: worker which is leaving idle state
 *
 * @worker is leaving idle state.  Update stats.
 *
 * LOCKING:
 * spin_lock_irq(gcwq->lock).
 */
static void worker_leave_idle(struct worker *worker)
{
	struct global_cwq *gcwq = worker->gcwq;

	BUG_ON(!(worker->flags & WORKER_IDLE));
	worker_clr_flags(worker, WORKER_IDLE);
	gcwq->nr_idle--;
	list_del_init(&worker->entry);
}

/**
 * worker_maybe_bind_and_lock - bind worker to its cpu if possible and lock gcwq
 * @worker: self
 *
 * Works which are scheduled while the cpu is online must at least be
 * scheduled to a worker which is bound to the cpu so that if they are
 * flushed from cpu callbacks while cpu is going down, they are
 * guaranteed to execute on the cpu.
 *
 * This function is to be used by rogue workers and rescuers to bind
 * themselves to the target cpu and may race with cpu going down or
 * coming online.  kthread_bind() can't be used because it may put the
 * worker to already dead cpu and set_cpus_allowed_ptr() can't be used
 * verbatim as it's best effort and blocking and gcwq may be
 * [dis]associated in the meantime.
 *
 * This function tries set_cpus_allowed() and locks gcwq and verifies
 * the binding against GCWQ_DISASSOCIATED which is set during
 * CPU_DYING and cleared during CPU_ONLINE, so if the worker enters
 * idle state or fetches works without dropping lock, it can guarantee
 * the scheduling requirement described in the first paragraph.
 *
 * CONTEXT:
 * Might sleep.  Called without any lock but returns with gcwq->lock
 * held.
 *
 * RETURNS:
 * %true if the associated gcwq is online (@worker is successfully
 * bound), %false if offline.
 */
static bool worker_maybe_bind_and_lock(struct worker *worker)
__acquires(&gcwq->lock)
{
	struct global_cwq *gcwq = worker->gcwq;
	struct task_struct *task = worker->task;

	while (true) {
		/*
		 * The following call may fail, succeed or succeed
		 * without actually migrating the task to the cpu if
		 * it races with cpu hotunplug operation.  Verify
		 * against GCWQ_DISASSOCIATED.
		 */
		if (!(gcwq->flags & GCWQ_DISASSOCIATED))
			set_cpus_allowed_ptr(task, get_cpu_mask(gcwq->cpu));

		spin_lock_irq(&gcwq->lock);
		if (gcwq->flags & GCWQ_DISASSOCIATED)
			return false;
		if (task_cpu(task) == gcwq->cpu &&
		    cpumask_equal(&current->cpus_allowed,
				  get_cpu_mask(gcwq->cpu)))
			return true;
		spin_unlock_irq(&gcwq->lock);

		/* CPU has come up inbetween, retry migration */
		cpu_relax();
	}
}

/*
 * Function for worker->rebind_work used to rebind rogue busy workers
 * to the associated cpu which is coming back online.  This is
 * scheduled by cpu up but can race with other cpu hotplug operations
 * and may be executed twice without intervening cpu down.
 */
static void worker_rebind_fn(struct work_struct *work)
{
	struct worker *worker = container_of(work, struct worker, rebind_work);
	struct global_cwq *gcwq = worker->gcwq;

	if (worker_maybe_bind_and_lock(worker))
		worker_clr_flags(worker, WORKER_REBIND);

	spin_unlock_irq(&gcwq->lock);
}

static struct worker *alloc_worker(void)
{
	struct worker *worker;

	worker = kzalloc(sizeof(*worker), GFP_KERNEL);
	if (worker) {
		INIT_LIST_HEAD(&worker->entry);
		INIT_LIST_HEAD(&worker->scheduled);
		INIT_WORK(&worker->rebind_work, worker_rebind_fn);
		/* on creation a worker is in !idle && prep state */
		worker->flags = WORKER_PREP;
	}
	return worker;
}

/**
 * create_worker - create a new workqueue worker
 * @gcwq: gcwq the new worker will belong to
 * @bind: whether to set affinity to @cpu or not
 *
 * Create a new worker which is bound to @gcwq.  The returned worker
 * can be started by calling start_worker() or destroyed using
 * destroy_worker().
 *
 * CONTEXT:
 * Might sleep.  Does GFP_KERNEL allocations.
 *
 * RETURNS:
 * Pointer to the newly created worker.
 */
static struct worker *create_worker(struct global_cwq *gcwq, bool bind)
{
	bool on_unbound_cpu = gcwq->cpu == WORK_CPU_UNBOUND;
	struct worker *worker = NULL;
	int id = -1;

	spin_lock_irq(&gcwq->lock);
	while (ida_get_new(&gcwq->worker_ida, &id)) {
		spin_unlock_irq(&gcwq->lock);
		if (!ida_pre_get(&gcwq->worker_ida, GFP_KERNEL))
			goto fail;
		spin_lock_irq(&gcwq->lock);
	}
	spin_unlock_irq(&gcwq->lock);

	worker = alloc_worker();
	if (!worker)
		goto fail;

	worker->gcwq = gcwq;
	worker->id = id;

	if (!on_unbound_cpu)
		worker->task = kthread_create(worker_thread, worker,
					      "kworker/%u:%d", gcwq->cpu, id);
	else
		worker->task = kthread_create(worker_thread, worker,
					      "kworker/u:%d", id);
	if (IS_ERR(worker->task))
		goto fail;

	/*
	 * A rogue worker will become a regular one if CPU comes
	 * online later on.  Make sure every worker has
	 * PF_THREAD_BOUND set.
	 */
	if (bind && !on_unbound_cpu)
		kthread_bind(worker->task, gcwq->cpu);
	else {
		worker->task->flags |= PF_THREAD_BOUND;
		if (on_unbound_cpu)
			worker->flags |= WORKER_UNBOUND;
	}

	return worker;
fail:
	if (id >= 0) {
		spin_lock_irq(&gcwq->lock);
		ida_remove(&gcwq->worker_ida, id);
		spin_unlock_irq(&gcwq->lock);
	}
	kfree(worker);
	return NULL;
}

/**
 * start_worker - start a newly created worker
 * @worker: worker to start
 *
 * Make the gcwq aware of @worker and start it.
 *
 * CONTEXT:
 * spin_lock_irq(gcwq->lock).
 */
static void start_worker(struct worker *worker)
{
	worker->flags |= WORKER_STARTED;
	worker->gcwq->nr_workers++;
	worker_enter_idle(worker);
	wake_up_process(worker->task);
}

/**
 * destroy_worker - destroy a workqueue worker
 * @worker: worker to be destroyed
 *
 * Destroy @worker and adjust @gcwq stats accordingly.
 *
 * CONTEXT:
 * spin_lock_irq(gcwq->lock) which is released and regrabbed.
 */
static void destroy_worker(struct worker *worker)
{
	struct global_cwq *gcwq = worker->gcwq;
	int id = worker->id;

	/* sanity check frenzy */
	BUG_ON(worker->current_work);
	BUG_ON(!list_empty(&worker->scheduled));

	if (worker->flags & WORKER_STARTED)
		gcwq->nr_workers--;
	if (worker->flags & WORKER_IDLE)
		gcwq->nr_idle--;

	list_del_init(&worker->entry);
	worker->flags |= WORKER_DIE;

	spin_unlock_irq(&gcwq->lock);

	kthread_stop(worker->task);
	kfree(worker);

	spin_lock_irq(&gcwq->lock);
	ida_remove(&gcwq->worker_ida, id);
}

static void idle_worker_timeout(unsigned long __gcwq)
{
	struct global_cwq *gcwq = (void *)__gcwq;

	spin_lock_irq(&gcwq->lock);

	if (too_many_workers(gcwq)) {
		struct worker *worker;
		unsigned long expires;

		/* idle_list is kept in LIFO order, check the last one */
		worker = list_entry(gcwq->idle_list.prev, struct worker, entry);
		expires = worker->last_active + IDLE_WORKER_TIMEOUT;

		if (time_before(jiffies, expires))
			mod_timer(&gcwq->idle_timer, expires);
		else {
			/* it's been idle for too long, wake up manager */
			gcwq->flags |= GCWQ_MANAGE_WORKERS;
			wake_up_worker(gcwq);
		}
	}

	spin_unlock_irq(&gcwq->lock);
}

static bool send_mayday(struct work_struct *work)
{
	struct cpu_workqueue_struct *cwq = get_work_cwq(work);
	struct workqueue_struct *wq = cwq->wq;
	unsigned int cpu;

	if (!(wq->flags & WQ_RESCUER))
		return false;

	/* mayday mayday mayday */
	cpu = cwq->gcwq->cpu;
	/* WORK_CPU_UNBOUND can't be set in cpumask, use cpu 0 instead */
	if (cpu == WORK_CPU_UNBOUND)
		cpu = 0;
	if (!mayday_test_and_set_cpu(cpu, wq->mayday_mask))
		wake_up_process(wq->rescuer->task);
	return true;
}

static void gcwq_mayday_timeout(unsigned long __gcwq)
{
	struct global_cwq *gcwq = (void *)__gcwq;
	struct work_struct *work;

	spin_lock_irq(&gcwq->lock);

	if (need_to_create_worker(gcwq)) {
		/*
		 * We've been trying to create a new worker but
		 * haven't been successful.  We might be hitting an
		 * allocation deadlock.  Send distress signals to
		 * rescuers.
		 */
		list_for_each_entry(work, &gcwq->worklist, entry)
			send_mayday(work);
	}

	spin_unlock_irq(&gcwq->lock);

	mod_timer(&gcwq->mayday_timer, jiffies + MAYDAY_INTERVAL);
}

/**
 * maybe_create_worker - create a new worker if necessary
 * @gcwq: gcwq to create a new worker for
 *
 * Create a new worker for @gcwq if necessary.  @gcwq is guaranteed to
 * have at least one idle worker on return from this function.  If
 * creating a new worker takes longer than MAYDAY_INTERVAL, mayday is
 * sent to all rescuers with works scheduled on @gcwq to resolve
 * possible allocation deadlock.
 *
 * On return, need_to_create_worker() is guaranteed to be false and
 * may_start_working() true.
 *
 * LOCKING:
 * spin_lock_irq(gcwq->lock) which may be released and regrabbed
 * multiple times.  Does GFP_KERNEL allocations.  Called only from
 * manager.
 *
 * RETURNS:
 * false if no action was taken and gcwq->lock stayed locked, true
 * otherwise.
 */
static bool maybe_create_worker(struct global_cwq *gcwq)
__releases(&gcwq->lock)
__acquires(&gcwq->lock)
{
	if (!need_to_create_worker(gcwq))
		return false;
restart:
	spin_unlock_irq(&gcwq->lock);

	/* if we don't make progress in MAYDAY_INITIAL_TIMEOUT, call for help */
	mod_timer(&gcwq->mayday_timer, jiffies + MAYDAY_INITIAL_TIMEOUT);

	while (true) {
		struct worker *worker;

		worker = create_worker(gcwq, true);
		if (worker) {
			del_timer_sync(&gcwq->mayday_timer);
			spin_lock_irq(&gcwq->lock);
			start_worker(worker);
			BUG_ON(need_to_create_worker(gcwq));
			return true;
		}

		if (!need_to_create_worker(gcwq))
			break;

		__set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(CREATE_COOLDOWN);

		if (!need_to_create_worker(gcwq))
			break;
	}

	del_timer_sync(&gcwq->mayday_timer);
	spin_lock_irq(&gcwq->lock);
	if (need_to_create_worker(gcwq))
		goto restart;
	return true;
}

/**
 * maybe_destroy_worker - destroy workers which have been idle for a while
 * @gcwq: gcwq to destroy workers for
 *
 * Destroy @gcwq workers which have been idle for longer than
 * IDLE_WORKER_TIMEOUT.
 *
 * LOCKING:
 * spin_lock_irq(gcwq->lock) which may be released and regrabbed
 * multiple times.  Called only from manager.
 *
 * RETURNS:
 * false if no action was taken and gcwq->lock stayed locked, true
 * otherwise.
 */
static bool maybe_destroy_workers(struct global_cwq *gcwq)
{
	bool ret = false;

	while (too_many_workers(gcwq)) {
		struct worker *worker;
		unsigned long expires;

		worker = list_entry(gcwq->idle_list.prev, struct worker, entry);
		expires = worker->last_active + IDLE_WORKER_TIMEOUT;

		if (time_before(jiffies, expires)) {
			mod_timer(&gcwq->idle_timer, expires);
			break;
		}

		destroy_worker(worker);
		ret = true;
	}

	return ret;
}

/**
 * manage_workers - manage worker pool
 * @worker: self
 *
 * Assume the manager role and manage gcwq worker pool @worker belongs
 * to.  At any given time, there can be only zero or one manager per
 * gcwq.  The exclusion is handled automatically by this function.
 *
 * The caller can safely start processing works on false return.  On
 * true return, it's guaranteed that need_to_create_worker() is false
 * and may_start_working() is true.
 *
 * CONTEXT:
 * spin_lock_irq(gcwq->lock) which may be released and regrabbed
 * multiple times.  Does GFP_KERNEL allocations.
 *
 * RETURNS:
 * false if no action was taken and gcwq->lock stayed locked, true if
 * some action was taken.
 */
static bool manage_workers(struct worker *worker)
{
	struct global_cwq *gcwq = worker->gcwq;
	bool ret = false;

	if (gcwq->flags & GCWQ_MANAGING_WORKERS)
		return ret;

	gcwq->flags &= ~GCWQ_MANAGE_WORKERS;
	gcwq->flags |= GCWQ_MANAGING_WORKERS;

	/*
	 * Destroy and then create so that may_start_working() is true
	 * on return.
	 */
	ret |= maybe_destroy_workers(gcwq);
	ret |= maybe_create_worker(gcwq);

	gcwq->flags &= ~GCWQ_MANAGING_WORKERS;

	/*
	 * The trustee might be waiting to take over the manager
	 * position, tell it we're done.
	 */
	if (unlikely(gcwq->trustee))
		wake_up_all(&gcwq->trustee_wait);

	return ret;
}

/**
 * move_linked_works - move linked works to a list
 * @work: start of series of works to be scheduled
 * @head: target list to append @work to
 * @nextp: out paramter for nested worklist walking
 *
 * Schedule linked works starting from @work to @head.  Work series to
 * be scheduled starts at @work and includes any consecutive work with
 * WORK_STRUCT_LINKED set in its predecessor.
 *
 * If @nextp is not NULL, it's updated to point to the next work of
 * the last scheduled work.  This allows move_linked_works() to be
 * nested inside outer list_for_each_entry_safe().
 *
 * CONTEXT:
 * spin_lock_irq(gcwq->lock).
 */
static void move_linked_works(struct work_struct *work, struct list_head *head,
			      struct work_struct **nextp)
{
	struct work_struct *n;

	/*
	 * Linked worklist will always end before the end of the list,
	 * use NULL for list head.
	 */
	list_for_each_entry_safe_from(work, n, NULL, entry) {
		list_move_tail(&work->entry, head);
		if (!(*work_data_bits(work) & WORK_STRUCT_LINKED))
			break;
	}

	/*
	 * If we're already inside safe list traversal and have moved
	 * multiple works to the scheduled queue, the next position
	 * needs to be updated.
	 */
	if (nextp)
		*nextp = n;
}

static void cwq_activate_first_delayed(struct cpu_workqueue_struct *cwq)
{
	struct work_struct *work = list_first_entry(&cwq->delayed_works,
						    struct work_struct, entry);
	struct list_head *pos = gcwq_determine_ins_pos(cwq->gcwq, cwq);

	move_linked_works(work, pos, NULL);
	__clear_bit(WORK_STRUCT_DELAYED_BIT, work_data_bits(work));
	cwq->nr_active++;
}

/**
 * cwq_dec_nr_in_flight - decrement cwq's nr_in_flight
 * @cwq: cwq of interest
 * @color: color of work which left the queue
 * @delayed: for a delayed work
 *
 * A work either has completed or is removed from pending queue,
 * decrement nr_in_flight of its cwq and handle workqueue flushing.
 *
 * CONTEXT:
 * spin_lock_irq(gcwq->lock).
 */
static void cwq_dec_nr_in_flight(struct cpu_workqueue_struct *cwq, int color,
				 bool delayed)
{
	/* ignore uncolored works */
	if (color == WORK_NO_COLOR)
		return;

	cwq->nr_in_flight[color]--;

	if (!delayed) {
		cwq->nr_active--;
		if (!list_empty(&cwq->delayed_works)) {
			/* one down, submit a delayed one */
			if (cwq->nr_active < cwq->max_active)
				cwq_activate_first_delayed(cwq);
		}
	}

	/* is flush in progress and are we at the flushing tip? */
	if (likely(cwq->flush_color != color))
		return;

	/* are there still in-flight works? */
	if (cwq->nr_in_flight[color])
		return;

	/* this cwq is done, clear flush_color */
	cwq->flush_color = -1;

	/*
	 * If this was the last cwq, wake up the first flusher.  It
	 * will handle the rest.
	 */
	if (atomic_dec_and_test(&cwq->wq->nr_cwqs_to_flush))
		complete(&cwq->wq->first_flusher->done);
}

/**
 * process_one_work - process single work
 * @worker: self
 * @work: work to process
 *
 * Process @work.  This function contains all the logics necessary to
 * process a single work including synchronization against and
 * interaction with other workers on the same cpu, queueing and
 * flushing.  As long as context requirement is met, any worker can
 * call this function to process a work.
 *
 * CONTEXT:
 * spin_lock_irq(gcwq->lock) which is released and regrabbed.
 */
static void process_one_work(struct worker *worker, struct work_struct *work)
__releases(&gcwq->lock)
__acquires(&gcwq->lock)
{
	struct cpu_workqueue_struct *cwq = get_work_cwq(work);
	struct global_cwq *gcwq = cwq->gcwq;
	struct hlist_head *bwh = busy_worker_head(gcwq, work);
	bool cpu_intensive = cwq->wq->flags & WQ_CPU_INTENSIVE;
	work_func_t f = work->func;
	int work_color;
	struct worker *collision;
#ifdef CONFIG_LOCKDEP
	/*
	 * It is permissible to free the struct work_struct from
	 * inside the function that is called from it, this we need to
	 * take into account for lockdep too.  To avoid bogus "held
	 * lock freed" warnings as well as problems when looking into
	 * work->lockdep_map, make a copy and use that here.
	 */
	struct lockdep_map lockdep_map = work->lockdep_map;
#endif
	/*
	 * A single work shouldn't be executed concurrently by
	 * multiple workers on a single cpu.  Check whether anyone is
	 * already processing the work.  If so, defer the work to the
	 * currently executing one.
	 */
	collision = __find_worker_executing_work(gcwq, bwh, work);
	if (unlikely(collision)) {
		move_linked_works(work, &collision->scheduled, NULL);
		return;
	}

	/* claim and process */
	debug_work_deactivate(work);
	hlist_add_head(&worker->hentry, bwh);
	worker->current_work = work;
	worker->current_cwq = cwq;
	work_color = get_work_color(work);

	/* record the current cpu number in the work data and dequeue */
	set_work_cpu(work, gcwq->cpu);
	list_del_init(&work->entry);

	/*
	 * If HIGHPRI_PENDING, check the next work, and, if HIGHPRI,
	 * wake up another worker; otherwise, clear HIGHPRI_PENDING.
	 */
	if (unlikely(gcwq->flags & GCWQ_HIGHPRI_PENDING)) {
		struct work_struct *nwork = list_first_entry(&gcwq->worklist,
						struct work_struct, entry);

		if (!list_empty(&gcwq->worklist) &&
		    get_work_cwq(nwork)->wq->flags & WQ_HIGHPRI)
			wake_up_worker(gcwq);
		else
			gcwq->flags &= ~GCWQ_HIGHPRI_PENDING;
	}

	/*
	 * CPU intensive works don't participate in concurrency
	 * management.  They're the scheduler's responsibility.
	 */
	if (unlikely(cpu_intensive))
		worker_set_flags(worker, WORKER_CPU_INTENSIVE, true);

	spin_unlock_irq(&gcwq->lock);

	work_clear_pending(work);
	lock_map_acquire(&cwq->wq->lockdep_map);
	lock_map_acquire(&lockdep_map);
	trace_workqueue_execute_start(work);

#if defined(BUZZZ_KEVT_LVL) && (BUZZZ_KEVT_LVL >= 1)
	buzzz_kevt_log1(BUZZZ_KEVT_ID_WORKQ_ENTRY, (int)f);
#endif	/* BUZZZ_KEVT_LVL */

	f(work);

#if defined(BUZZZ_KEVT_LVL) && (BUZZZ_KEVT_LVL >= 1)
	buzzz_kevt_log1(BUZZZ_KEVT_ID_WORKQ_EXIT, (int)f);
#endif	/* BUZZZ_KEVT_LVL */

	/*
	 * While we must be careful to not use "work" after this, the trace
	 * point will only record its address.
	 */
	trace_workqueue_execute_end(work);
	lock_map_release(&lockdep_map);
	lock_map_release(&cwq->wq->lockdep_map);

	if (unlikely(in_atomic() || lockdep_depth(current) > 0)) {
		printk(KERN_ERR "BUG: workqueue leaked lock or atomic: "
		       "%s/0x%08x/%d\n",
		       current->comm, preempt_count(), task_pid_nr(current));
		printk(KERN_ERR "    last function: ");
		print_symbol("%s\n", (unsigned long)f);
		debug_show_held_locks(current);
		dump_stack();
	}

	spin_lock_irq(&gcwq->lock);

	/* clear cpu intensive status */
	if (unlikely(cpu_intensive))
		worker_clr_flags(worker, WORKER_CPU_INTENSIVE);

	/* we're done with it, release */
	hlist_del_init(&worker->hentry);
	worker->current_work = NULL;
	worker->current_cwq = NULL;
	cwq_dec_nr_in_flight(cwq, work_color, false);
}

/**
 * process_scheduled_works - process scheduled works
 * @worker: self
 *
 * Process all scheduled works.  Please note that the scheduled list
 * may change while processing a work, so this function repeatedly
 * fetches a work from the top and executes it.
 *
 * CONTEXT:
 * spin_lock_irq(gcwq->lock) which may be released and regrabbed
 * multiple times.
 */
static void process_scheduled_works(struct worker *worker)
{
	while (!list_empty(&worker->scheduled)) {
		struct work_struct *work = list_first_entry(&worker->scheduled,
						struct work_struct, entry);
		process_one_work(worker, work);
	}
}

/**
 * worker_thread - the worker thread function
 * @__worker: self
 *
 * The gcwq worker thread function.  There's a single dynamic pool of
 * these per each cpu.  These workers process all works regardless of
 * their specific target workqueue.  The only exception is works which
 * belong to workqueues with a rescuer which will be explained in
 * rescuer_thread().
 */
static int worker_thread(void *__worker)
{
	struct worker *worker = __worker;
	struct global_cwq *gcwq = worker->gcwq;

	/* tell the scheduler that this is a workqueue worker */
	worker->task->flags |= PF_WQ_WORKER;
woke_up:
	spin_lock_irq(&gcwq->lock);

	/* DIE can be set only while we're idle, checking here is enough */
	if (worker->flags & WORKER_DIE) {
		spin_unlock_irq(&gcwq->lock);
		worker->task->flags &= ~PF_WQ_WORKER;
		return 0;
	}

	worker_leave_idle(worker);
recheck:
	/* no more worker necessary? */
	if (!need_more_worker(gcwq))
		goto sleep;

	/* do we need to manage? */
	if (unlikely(!may_start_working(gcwq)) && manage_workers(worker))
		goto recheck;

	/*
	 * ->scheduled list can only be filled while a worker is
	 * preparing to process a work or actually processing it.
	 * Make sure nobody diddled with it while I was sleeping.
	 */
	BUG_ON(!list_empty(&worker->scheduled));

	/*
	 * When control reaches this point, we're guaranteed to have
	 * at least one idle worker or that someone else has already
	 * assumed the manager role.
	 */
	worker_clr_flags(worker, WORKER_PREP);

	do {
		struct work_struct *work =
			list_first_entry(&gcwq->worklist,
					 struct work_struct, entry);

		if (likely(!(*work_data_bits(work) & WORK_STRUCT_LINKED))) {
			/* optimization path, not strictly necessary */
			process_one_work(worker, work);
			if (unlikely(!list_empty(&worker->scheduled)))
				process_scheduled_works(worker);
		} else {
			move_linked_works(work, &worker->scheduled, NULL);
			process_scheduled_works(worker);
		}
	} while (keep_working(gcwq));

	worker_set_flags(worker, WORKER_PREP, false);
sleep:
	if (unlikely(need_to_manage_workers(gcwq)) && manage_workers(worker))
		goto recheck;

	/*
	 * gcwq->lock is held and there's no work to process and no
	 * need to manage, sleep.  Workers are woken up only while
	 * holding gcwq->lock or from local cpu, so setting the
	 * current state before releasing gcwq->lock is enough to
	 * prevent losing any event.
	 */
	worker_enter_idle(worker);
	__set_current_state(TASK_INTERRUPTIBLE);
	spin_unlock_irq(&gcwq->lock);
	schedule();
	goto woke_up;
}

/**
 * rescuer_thread - the rescuer thread function
 * @__wq: the associated workqueue
 *
 * Workqueue rescuer thread function.  There's one rescuer for each
 * workqueue which has WQ_RESCUER set.
 *
 * Regular work processing on a gcwq may block trying to create a new
 * worker which uses GFP_KERNEL allocation which has slight chance of
 * developing into deadlock if some works currently on the same queue
 * need to be processed to satisfy the GFP_KERNEL allocation.  This is
 * the problem rescuer solves.
 *
 * When such condition is possible, the gcwq summons rescuers of all
 * workqueues which have works queued on the gcwq and let them process
 * those works so that forward progress can be guaranteed.
 *
 * This should happen rarely.
 */
static int rescuer_thread(void *__wq)
{
	struct workqueue_struct *wq = __wq;
	struct worker *rescuer = wq->rescuer;
	struct list_head *scheduled = &rescuer->scheduled;
	bool is_unbound = wq->flags & WQ_UNBOUND;
	unsigned int cpu;

	set_user_nice(current, RESCUER_NICE_LEVEL);
repeat:
	set_current_state(TASK_INTERRUPTIBLE);

	if (kthread_should_stop())
		return 0;

	/*
	 * See whether any cpu is asking for help.  Unbounded
	 * workqueues use cpu 0 in mayday_mask for CPU_UNBOUND.
	 */
	for_each_mayday_cpu(cpu, wq->mayday_mask) {
		unsigned int tcpu = is_unbound ? WORK_CPU_UNBOUND : cpu;
		struct cpu_workqueue_struct *cwq = get_cwq(tcpu, wq);
		struct global_cwq *gcwq = cwq->gcwq;
		struct work_struct *work, *n;

		__set_current_state(TASK_RUNNING);
		mayday_clear_cpu(cpu, wq->mayday_mask);

		/* migrate to the target cpu if possible */
		rescuer->gcwq = gcwq;
		worker_maybe_bind_and_lock(rescuer);

		/*
		 * Slurp in all works issued via this workqueue and
		 * process'em.
		 */
		BUG_ON(!list_empty(&rescuer->scheduled));
		list_for_each_entry_safe(work, n, &gcwq->worklist, entry)
			if (get_work_cwq(work) == cwq)
				move_linked_works(work, scheduled, &n);

		process_scheduled_works(rescuer);
		spin_unlock_irq(&gcwq->lock);
	}

	schedule();
	goto repeat;
}

struct wq_barrier {
	struct work_struct	work;
	struct completion	done;
};

static void wq_barrier_func(struct work_struct *work)
{
	struct wq_barrier *barr = container_of(work, struct wq_barrier, work);
	complete(&barr->done);
}

/**
 * insert_wq_barrier - insert a barrier work
 * @cwq: cwq to insert barrier into
 * @barr: wq_barrier to insert
 * @target: target work to attach @barr to
 * @worker: worker currently executing @target, NULL if @target is not executing
 *
 * @barr is linked to @target such that @barr is completed only after
 * @target finishes execution.  Please note that the ordering
 * guarantee is observed only with respect to @target and on the local
 * cpu.
 *
 * Currently, a queued barrier can't be canceled.  This is because
 * try_to_grab_pending() can't determine whether the work to be
 * grabbed is at the head of the queue and thus can't clear LINKED
 * flag of the previous work while there must be a valid next work
 * after a work with LINKED flag set.
 *
 * Note that when @worker is non-NULL, @target may be modified
 * underneath us, so we can't reliably determine cwq from @target.
 *
 * CONTEXT:
 * spin_lock_irq(gcwq->lock).
 */
static void insert_wq_barrier(struct cpu_workqueue_struct *cwq,
			      struct wq_barrier *barr,
			      struct work_struct *target, struct worker *worker)
{
	struct list_head *head;
	unsigned int linked = 0;

	/*
	 * debugobject calls are safe here even with gcwq->lock locked
	 * as we know for sure that this will not trigger any of the
	 * checks and call back into the fixup functions where we
	 * might deadlock.
	 */
	INIT_WORK_ON_STACK(&barr->work, wq_barrier_func);
	__set_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(&barr->work));
	init_completion(&barr->done);

	/*
	 * If @target is currently being executed, schedule the
	 * barrier to the worker; otherwise, put it after @target.
	 */
	if (worker)
		head = worker->scheduled.next;
	else {
		unsigned long *bits = work_data_bits(target);

		head = target->entry.next;
		/* there can already be other linked works, inherit and set */
		linked = *bits & WORK_STRUCT_LINKED;
		__set_bit(WORK_STRUCT_LINKED_BIT, bits);
	}

	debug_work_activate(&barr->work);
	insert_work(cwq, &barr->work, head,
		    work_color_to_flags(WORK_NO_COLOR) | linked);
}

/**
 * flush_workqueue_prep_cwqs - prepare cwqs for workqueue flushing
 * @wq: workqueue being flushed
 * @flush_color: new flush color, < 0 for no-op
 * @work_color: new work color, < 0 for no-op
 *
 * Prepare cwqs for workqueue flushing.
 *
 * If @flush_color is non-negative, flush_color on all cwqs should be
 * -1.  If no cwq has in-flight commands at the specified color, all
 * cwq->flush_color's stay at -1 and %false is returned.  If any cwq
 * has in flight commands, its cwq->flush_color is set to
 * @flush_color, @wq->nr_cwqs_to_flush is updated accordingly, cwq
 * wakeup logic is armed and %true is returned.
 *
 * The caller should have initialized @wq->first_flusher prior to
 * calling this function with non-negative @flush_color.  If
 * @flush_color is negative, no flush color update is done and %false
 * is returned.
 *
 * If @work_color is non-negative, all cwqs should have the same
 * work_color which is previous to @work_color and all will be
 * advanced to @work_color.
 *
 * CONTEXT:
 * mutex_lock(wq->flush_mutex).
 *
 * RETURNS:
 * %true if @flush_color >= 0 and there's something to flush.  %false
 * otherwise.
 */
static bool flush_workqueue_prep_cwqs(struct workqueue_struct *wq,
				      int flush_color, int work_color)
{
	bool wait = false;
	unsigned int cpu;

	if (flush_color >= 0) {
		BUG_ON(atomic_read(&wq->nr_cwqs_to_flush));
		atomic_set(&wq->nr_cwqs_to_flush, 1);
	}

	for_each_cwq_cpu(cpu, wq) {
		struct cpu_workqueue_struct *cwq = get_cwq(cpu, wq);
		struct global_cwq *gcwq = cwq->gcwq;

		spin_lock_irq(&gcwq->lock);

		if (flush_color >= 0) {
			BUG_ON(cwq->flush_color != -1);

			if (cwq->nr_in_flight[flush_color]) {
				cwq->flush_color = flush_color;
				atomic_inc(&wq->nr_cwqs_to_flush);
				wait = true;
			}
		}

		if (work_color >= 0) {
			BUG_ON(work_color != work_next_color(cwq->work_color));
			cwq->work_color = work_color;
		}

		spin_unlock_irq(&gcwq->lock);
	}

	if (flush_color >= 0 && atomic_dec_and_test(&wq->nr_cwqs_to_flush))
		complete(&wq->first_flusher->done);

	return wait;
}

/**
 * flush_workqueue - ensure that any scheduled work has run to completion.
 * @wq: workqueue to flush
 *
 * Forces execution of the workqueue and blocks until its completion.
 * This is typically used in driver shutdown handlers.
 *
 * We sleep until all works which were queued on entry have been handled,
 * but we are not livelocked by new incoming ones.
 */
void flush_workqueue(struct workqueue_struct *wq)
{
	struct wq_flusher this_flusher = {
		.list = LIST_HEAD_INIT(this_flusher.list),
		.flush_color = -1,
		.done = COMPLETION_INITIALIZER_ONSTACK(this_flusher.done),
	};
	int next_color;

	lock_map_acquire(&wq->lockdep_map);
	lock_map_release(&wq->lockdep_map);

	mutex_lock(&wq->flush_mutex);

	/*
	 * Start-to-wait phase
	 */
	next_color = work_next_color(wq->work_color);

	if (next_color != wq->flush_color) {
		/*
		 * Color space is not full.  The current work_color
		 * becomes our flush_color and work_color is advanced
		 * by one.
		 */
		BUG_ON(!list_empty(&wq->flusher_overflow));
		this_flusher.flush_color = wq->work_color;
		wq->work_color = next_color;

		if (!wq->first_flusher) {
			/* no flush in progress, become the first flusher */
			BUG_ON(wq->flush_color != this_flusher.flush_color);

			wq->first_flusher = &this_flusher;

			if (!flush_workqueue_prep_cwqs(wq, wq->flush_color,
						       wq->work_color)) {
				/* nothing to flush, done */
				wq->flush_color = next_color;
				wq->first_flusher = NULL;
				goto out_unlock;
			}
		} else {
			/* wait in queue */
			BUG_ON(wq->flush_color == this_flusher.flush_color);
			list_add_tail(&this_flusher.list, &wq->flusher_queue);
			flush_workqueue_prep_cwqs(wq, -1, wq->work_color);
		}
	} else {
		/*
		 * Oops, color space is full, wait on overflow queue.
		 * The next flush completion will assign us
		 * flush_color and transfer to flusher_queue.
		 */
		list_add_tail(&this_flusher.list, &wq->flusher_overflow);
	}

	mutex_unlock(&wq->flush_mutex);

	wait_for_completion(&this_flusher.done);

	/*
	 * Wake-up-and-cascade phase
	 *
	 * First flushers are responsible for cascading flushes and
	 * handling overflow.  Non-first flushers can simply return.
	 */
	if (wq->first_flusher != &this_flusher)
		return;

	mutex_lock(&wq->flush_mutex);

	/* we might have raced, check again with mutex held */
	if (wq->first_flusher != &this_flusher)
		goto out_unlock;

	wq->first_flusher = NULL;

	BUG_ON(!list_empty(&this_flusher.list));
	BUG_ON(wq->flush_color != this_flusher.flush_color);

	while (true) {
		struct wq_flusher *next, *tmp;

		/* complete all the flushers sharing the current flush color */
		list_for_each_entry_safe(next, tmp, &wq->flusher_queue, list) {
			if (next->flush_color != wq->flush_color)
				break;
			list_del_init(&next->list);
			complete(&next->done);
		}

		BUG_ON(!list_empty(&wq->flusher_overflow) &&
		       wq->flush_color != work_next_color(wq->work_color));

		/* this flush_color is finished, advance by one */
		wq->flush_color = work_next_color(wq->flush_color);

		/* one color has been freed, handle overflow queue */
		if (!list_empty(&wq->flusher_overflow)) {
			/*
			 * Assign the same color to all overflowed
			 * flushers, advance work_color and append to
			 * flusher_queue.  This is the start-to-wait
			 * phase for these overflowed flushers.
			 */
			list_for_each_entry(tmp, &wq->flusher_overflow, list)
				tmp->flush_color = wq->work_color;

			wq->work_color = work_next_color(wq->work_color);

			list_splice_tail_init(&wq->flusher_overflow,
					      &wq->flusher_queue);
			flush_workqueue_prep_cwqs(wq, -1, wq->work_color);
		}

		if (list_empty(&wq->flusher_queue)) {
			BUG_ON(wq->flush_color != wq->work_color);
			break;
		}

		/*
		 * Need to flush more colors.  Make the next flusher
		 * the new first flusher and arm cwqs.
		 */
		BUG_ON(wq->flush_color == wq->work_color);
		BUG_ON(wq->flush_color != next->flush_color);

		list_del_init(&next->list);
		wq->first_flusher = next;

		if (flush_workqueue_prep_cwqs(wq, wq->flush_color, -1))
			break;

		/*
		 * Meh... this color is already done, clear first
		 * flusher and repeat cascading.
		 */
		wq->first_flusher = NULL;
	}

out_unlock:
	mutex_unlock(&wq->flush_mutex);
}
EXPORT_SYMBOL_GPL(flush_workqueue);

/**
 * flush_work - block until a work_struct's callback has terminated
 * @work: the work which is to be flushed
 *
 * Returns false if @work has already terminated.
 *
 * It is expected that, prior to calling flush_work(), the caller has
 * arranged for the work to not be requeued, otherwise it doesn't make
 * sense to use this function.
 */
int flush_work(struct work_struct *work)
{
	struct worker *worker = NULL;
	struct global_cwq *gcwq;
	struct cpu_workqueue_struct *cwq;
	struct wq_barrier barr;

	might_sleep();
	gcwq = get_work_gcwq(work);
	if (!gcwq)
		return 0;

	spin_lock_irq(&gcwq->lock);
	if (!list_empty(&work->entry)) {
		/*
		 * See the comment near try_to_grab_pending()->smp_rmb().
		 * If it was re-queued to a different gcwq under us, we
		 * are not going to wait.
		 */
		smp_rmb();
		cwq = get_work_cwq(work);
		if (unlikely(!cwq || gcwq != cwq->gcwq))
			goto already_gone;
	} else {
		worker = find_worker_executing_work(gcwq, work);
		if (!worker)
			goto already_gone;
		cwq = worker->current_cwq;
	}

	insert_wq_barrier(cwq, &barr, work, worker);
	spin_unlock_irq(&gcwq->lock);

	lock_map_acquire(&cwq->wq->lockdep_map);
	lock_map_release(&cwq->wq->lockdep_map);

	wait_for_completion(&barr.done);
	destroy_work_on_stack(&barr.work);
	return 1;
already_gone:
	spin_unlock_irq(&gcwq->lock);
	return 0;
}
EXPORT_SYMBOL_GPL(flush_work);

/*
 * Upon a successful return (>= 0), the caller "owns" WORK_STRUCT_PENDING bit,
 * so this work can't be re-armed in any way.
 */
static int try_to_grab_pending(struct work_struct *work)
{
	struct global_cwq *gcwq;
	int ret = -1;

	if (!test_and_set_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work)))
		return 0;

	/*
	 * The queueing is in progress, or it is already queued. Try to
	 * steal it from ->worklist without clearing WORK_STRUCT_PENDING.
	 */
	gcwq = get_work_gcwq(work);
	if (!gcwq)
		return ret;

	spin_lock_irq(&gcwq->lock);
	if (!list_empty(&work->entry)) {
		/*
		 * This work is queued, but perhaps we locked the wrong gcwq.
		 * In that case we must see the new value after rmb(), see
		 * insert_work()->wmb().
		 */
		smp_rmb();
		if (gcwq == get_work_gcwq(work)) {
			debug_work_deactivate(work);
			list_del_init(&work->entry);
			cwq_dec_nr_in_flight(get_work_cwq(work),
				get_work_color(work),
				*work_data_bits(work) & WORK_STRUCT_DELAYED);
			ret = 1;
		}
	}
	spin_unlock_irq(&gcwq->lock);

	return ret;
}

static void wait_on_cpu_work(struct global_cwq *gcwq, struct work_struct *work)
{
	struct wq_barrier barr;
	struct worker *worker;

	spin_lock_irq(&gcwq->lock);

	worker = find_worker_executing_work(gcwq, work);
	if (unlikely(worker))
		insert_wq_barrier(worker->current_cwq, &barr, work, worker);

	spin_unlock_irq(&gcwq->lock);

	if (unlikely(worker)) {
		wait_for_completion(&barr.done);
		destroy_work_on_stack(&barr.work);
	}
}

static void wait_on_work(struct work_struct *work)
{
	int cpu;

	might_sleep();

	lock_map_acquire(&work->lockdep_map);
	lock_map_release(&work->lockdep_map);

	for_each_gcwq_cpu(cpu)
		wait_on_cpu_work(get_gcwq(cpu), work);
}

static int __cancel_work_timer(struct work_struct *work,
				struct timer_list* timer)
{
	int ret;

	do {
		ret = (timer && likely(del_timer(timer)));
		if (!ret)
			ret = try_to_grab_pending(work);
		wait_on_work(work);
	} while (unlikely(ret < 0));

	clear_work_data(work);
	return ret;
}

/**
 * cancel_work_sync - block until a work_struct's callback has terminated
 * @work: the work which is to be flushed
 *
 * Returns true if @work was pending.
 *
 * cancel_work_sync() will cancel the work if it is queued. If the work's
 * callback appears to be running, cancel_work_sync() will block until it
 * has completed.
 *
 * It is possible to use this function if the work re-queues itself. It can
 * cancel the work even if it migrates to another workqueue, however in that
 * case it only guarantees that work->func() has completed on the last queued
 * workqueue.
 *
 * cancel_work_sync(&delayed_work->work) should be used only if ->timer is not
 * pending, otherwise it goes into a busy-wait loop until the timer expires.
 *
 * The caller must ensure that workqueue_struct on which this work was last
 * queued can't be destroyed before this function returns.
 */
int cancel_work_sync(struct work_struct *work)
{
	return __cancel_work_timer(work, NULL);
}
EXPORT_SYMBOL_GPL(cancel_work_sync);

/**
 * cancel_delayed_work_sync - reliably kill off a delayed work.
 * @dwork: the delayed work struct
 *
 * Returns true if @dwork was pending.
 *
 * It is possible to use this function if @dwork rearms itself via queue_work()
 * or queue_delayed_work(). See also the comment for cancel_work_sync().
 */
int cancel_delayed_work_sync(struct delayed_work *dwork)
{
	return __cancel_work_timer(&dwork->work, &dwork->timer);
}
EXPORT_SYMBOL(cancel_delayed_work_sync);

/**
 * schedule_work - put work task in global workqueue
 * @work: job to be done
 *
 * Returns zero if @work was already on the kernel-global workqueue and
 * non-zero otherwise.
 *
 * This puts a job in the kernel-global workqueue if it was not already
 * queued and leaves it in the same position on the kernel-global
 * workqueue otherwise.
 */
int schedule_work(struct work_struct *work)
{
	return queue_work(system_wq, work);
}
EXPORT_SYMBOL(schedule_work);

/*
 * schedule_work_on - put work task on a specific cpu
 * @cpu: cpu to put the work task on
 * @work: job to be done
 *
 * This puts a job on a specific cpu
 */
int schedule_work_on(int cpu, struct work_struct *work)
{
	return queue_work_on(cpu, system_wq, work);
}
EXPORT_SYMBOL(schedule_work_on);

/**
 * schedule_delayed_work - put work task in global workqueue after delay
 * @dwork: job to be done
 * @delay: number of jiffies to wait or 0 for immediate execution
 *
 * After waiting for a given time this puts a job in the kernel-global
 * workqueue.
 */
int schedule_delayed_work(struct delayed_work *dwork,
					unsigned long delay)
{
	return queue_delayed_work(system_wq, dwork, delay);
}
EXPORT_SYMBOL(schedule_delayed_work);

/**
 * flush_delayed_work - block until a dwork_struct's callback has terminated
 * @dwork: the delayed work which is to be flushed
 *
 * Any timeout is cancelled, and any pending work is run immediately.
 */
void flush_delayed_work(struct delayed_work *dwork)
{
	if (del_timer_sync(&dwork->timer)) {
		__queue_work(get_cpu(), get_work_cwq(&dwork->work)->wq,
			     &dwork->work);
		put_cpu();
	}
	flush_work(&dwork->work);
}
EXPORT_SYMBOL(flush_delayed_work);

/**
 * schedule_delayed_work_on - queue work in global workqueue on CPU after delay
 * @cpu: cpu to use
 * @dwork: job to be done
 * @delay: number of jiffies to wait
 *
 * After waiting for a given time this puts a job in the kernel-global
 * workqueue on the specified CPU.
 */
int schedule_delayed_work_on(int cpu,
			struct delayed_work *dwork, unsigned long delay)
{
	return queue_delayed_work_on(cpu, system_wq, dwork, delay);
}
EXPORT_SYMBOL(schedule_delayed_work_on);

/**
 * schedule_on_each_cpu - call a function on each online CPU from keventd
 * @func: the function to call
 *
 * Returns zero on success.
 * Returns -ve errno on failure.
 *
 * schedule_on_each_cpu() is very slow.
 */
int schedule_on_each_cpu(work_func_t func)
{
	int cpu;
	struct work_struct __percpu *works;

	works = alloc_percpu(struct work_struct);
	if (!works)
		return -ENOMEM;

	get_online_cpus();

	for_each_online_cpu(cpu) {
		struct work_struct *work = per_cpu_ptr(works, cpu);

		INIT_WORK(work, func);
		schedule_work_on(cpu, work);
	}

	for_each_online_cpu(cpu)
		flush_work(per_cpu_ptr(works, cpu));

	put_online_cpus();
	free_percpu(works);
	return 0;
}

/**
 * flush_scheduled_work - ensure that any scheduled work has run to completion.
 *
 * Forces execution of the kernel-global workqueue and blocks until its
 * completion.
 *
 * Think twice before calling this function!  It's very easy to get into
 * trouble if you don't take great care.  Either of the following situations
 * will lead to deadlock:
 *
 *	One of the work items currently on the workqueue needs to acquire
 *	a lock held by your code or its caller.
 *
 *	Your code is running in the context of a work routine.
 *
 * They will be detected by lockdep when they occur, but the first might not
 * occur very often.  It depends on what work items are on the workqueue and
 * what locks they need, which you have no control over.
 *
 * In most situations flushing the entire workqueue is overkill; you merely
 * need to know that a particular work item isn't queued and isn't running.
 * In such cases you should use cancel_delayed_work_sync() or
 * cancel_work_sync() instead.
 */
void flush_scheduled_work(void)
{
	flush_workqueue(system_wq);
}
EXPORT_SYMBOL(flush_scheduled_work);

/**
 * execute_in_process_context - reliably execute the routine with user context
 * @fn:		the function to execute
 * @ew:		guaranteed storage for the execute work structure (must
 *		be available when the work executes)
 *
 * Executes the function immediately if process context is available,
 * otherwise schedules the function for delayed execution.
 *
 * Returns:	0 - function was executed
 *		1 - function was scheduled for execution
 */
int execute_in_process_context(work_func_t fn, struct execute_work *ew)
{
	if (!in_interrupt()) {
		fn(&ew->work);
		return 0;
	}

	INIT_WORK(&ew->work, fn);
	schedule_work(&ew->work);

	return 1;
}
EXPORT_SYMBOL_GPL(execute_in_process_context);

int keventd_up(void)
{
	return system_wq != NULL;
}

static int alloc_cwqs(struct workqueue_struct *wq)
{
	/*
	 * cwqs are forced aligned according to WORK_STRUCT_FLAG_BITS.
	 * Make sure that the alignment isn't lower than that of
	 * unsigned long long.
	 */
	const size_t size = sizeof(struct cpu_workqueue_struct);
	const size_t align = max_t(size_t, 1 << WORK_STRUCT_FLAG_BITS,
				   __alignof__(unsigned long long));
#ifdef CONFIG_SMP
	bool percpu = !(wq->flags & WQ_UNBOUND);
#else
	bool percpu = false;
#endif

	if (percpu)
		wq->cpu_wq.pcpu = __alloc_percpu(size, align);
	else {
		void *ptr;

		/*
		 * Allocate enough room to align cwq and put an extra
		 * pointer at the end pointing back to the originally
		 * allocated pointer which will be used for free.
		 */
		ptr = kzalloc(size + align + sizeof(void *), GFP_KERNEL);
		if (ptr) {
			wq->cpu_wq.single = PTR_ALIGN(ptr, align);
			*(void **)(wq->cpu_wq.single + 1) = ptr;
		}
	}

	/* just in case, make sure it's actually aligned */
	BUG_ON(!IS_ALIGNED(wq->cpu_wq.v, align));
	return wq->cpu_wq.v ? 0 : -ENOMEM;
}

static void free_cwqs(struct workqueue_struct *wq)
{
#ifdef CONFIG_SMP
	bool percpu = !(wq->flags & WQ_UNBOUND);
#else
	bool percpu = false;
#endif

	if (percpu)
		free_percpu(wq->cpu_wq.pcpu);
	else if (wq->cpu_wq.single) {
		/* the pointer to free is stored right after the cwq */
		kfree(*(void **)(wq->cpu_wq.single + 1));
	}
}

static int wq_clamp_max_active(int max_active, unsigned int flags,
			       const char *name)
{
	int lim = flags & WQ_UNBOUND ? WQ_UNBOUND_MAX_ACTIVE : WQ_MAX_ACTIVE;

	if (max_active < 1 || max_active > lim)
		printk(KERN_WARNING "workqueue: max_active %d requested for %s "
		       "is out of range, clamping between %d and %d\n",
		       max_active, name, 1, lim);

	return clamp_val(max_active, 1, lim);
}

struct workqueue_struct *__alloc_workqueue_key(const char *name,
					       unsigned int flags,
					       int max_active,
					       struct lock_class_key *key,
					       const char *lock_name)
{
	struct workqueue_struct *wq;
	unsigned int cpu;

	/*
	 * Unbound workqueues aren't concurrency managed and should be
	 * dispatched to workers immediately.
	 */
	if (flags & WQ_UNBOUND)
		flags |= WQ_HIGHPRI;

	max_active = max_active ?: WQ_DFL_ACTIVE;
	max_active = wq_clamp_max_active(max_active, flags, name);

	wq = kzalloc(sizeof(*wq), GFP_KERNEL);
	if (!wq)
		goto err;

	wq->flags = flags;
	wq->saved_max_active = max_active;
	mutex_init(&wq->flush_mutex);
	atomic_set(&wq->nr_cwqs_to_flush, 0);
	INIT_LIST_HEAD(&wq->flusher_queue);
	INIT_LIST_HEAD(&wq->flusher_overflow);

	wq->name = name;
	lockdep_init_map(&wq->lockdep_map, lock_name, key, 0);
	INIT_LIST_HEAD(&wq->list);

	if (alloc_cwqs(wq) < 0)
		goto err;

	for_each_cwq_cpu(cpu, wq) {
		struct cpu_workqueue_struct *cwq = get_cwq(cpu, wq);
		struct global_cwq *gcwq = get_gcwq(cpu);

		BUG_ON((unsigned long)cwq & WORK_STRUCT_FLAG_MASK);
		cwq->gcwq = gcwq;
		cwq->wq = wq;
		cwq->flush_color = -1;
		cwq->max_active = max_active;
		INIT_LIST_HEAD(&cwq->delayed_works);
	}

	if (flags & WQ_RESCUER) {
		struct worker *rescuer;

		if (!alloc_mayday_mask(&wq->mayday_mask, GFP_KERNEL))
			goto err;

		wq->rescuer = rescuer = alloc_worker();
		if (!rescuer)
			goto err;

		rescuer->task = kthread_create(rescuer_thread, wq, "%s", name);
		if (IS_ERR(rescuer->task))
			goto err;

		rescuer->task->flags |= PF_THREAD_BOUND;
		wake_up_process(rescuer->task);
	}

	/*
	 * workqueue_lock protects global freeze state and workqueues
	 * list.  Grab it, set max_active accordingly and add the new
	 * workqueue to workqueues list.
	 */
	spin_lock(&workqueue_lock);

	if (workqueue_freezing && wq->flags & WQ_FREEZEABLE)
		for_each_cwq_cpu(cpu, wq)
			get_cwq(cpu, wq)->max_active = 0;

	list_add(&wq->list, &workqueues);

	spin_unlock(&workqueue_lock);

	return wq;
err:
	if (wq) {
		free_cwqs(wq);
		free_mayday_mask(wq->mayday_mask);
		kfree(wq->rescuer);
		kfree(wq);
	}
	return NULL;
}
EXPORT_SYMBOL_GPL(__alloc_workqueue_key);

/**
 * destroy_workqueue - safely terminate a workqueue
 * @wq: target workqueue
 *
 * Safely destroy a workqueue. All work currently pending will be done first.
 */
void destroy_workqueue(struct workqueue_struct *wq)
{
	unsigned int cpu;

	wq->flags |= WQ_DYING;
	flush_workqueue(wq);

	/*
	 * wq list is used to freeze wq, remove from list after
	 * flushing is complete in case freeze races us.
	 */
	spin_lock(&workqueue_lock);
	list_del(&wq->list);
	spin_unlock(&workqueue_lock);

	/* sanity check */
	for_each_cwq_cpu(cpu, wq) {
		struct cpu_workqueue_struct *cwq = get_cwq(cpu, wq);
		int i;

		for (i = 0; i < WORK_NR_COLORS; i++)
			BUG_ON(cwq->nr_in_flight[i]);
		BUG_ON(cwq->nr_active);
		BUG_ON(!list_empty(&cwq->delayed_works));
	}

	if (wq->flags & WQ_RESCUER) {
		kthread_stop(wq->rescuer->task);
		free_mayday_mask(wq->mayday_mask);
		kfree(wq->rescuer);
	}

	free_cwqs(wq);
	kfree(wq);
}
EXPORT_SYMBOL_GPL(destroy_workqueue);

/**
 * workqueue_set_max_active - adjust max_active of a workqueue
 * @wq: target workqueue
 * @max_active: new max_active value.
 *
 * Set max_active of @wq to @max_active.
 *
 * CONTEXT:
 * Don't call from IRQ context.
 */
void workqueue_set_max_active(struct workqueue_struct *wq, int max_active)
{
	unsigned int cpu;

	max_active = wq_clamp_max_active(max_active, wq->flags, wq->name);

	spin_lock(&workqueue_lock);

	wq->saved_max_active = max_active;

	for_each_cwq_cpu(cpu, wq) {
		struct global_cwq *gcwq = get_gcwq(cpu);

		spin_lock_irq(&gcwq->lock);

		if (!(wq->flags & WQ_FREEZEABLE) ||
		    !(gcwq->flags & GCWQ_FREEZING))
			get_cwq(gcwq->cpu, wq)->max_active = max_active;

		spin_unlock_irq(&gcwq->lock);
	}

	spin_unlock(&workqueue_lock);
}
EXPORT_SYMBOL_GPL(workqueue_set_max_active);

/**
 * workqueue_congested - test whether a workqueue is congested
 * @cpu: CPU in question
 * @wq: target workqueue
 *
 * Test whether @wq's cpu workqueue for @cpu is congested.  There is
 * no synchronization around this function and the test result is
 * unreliable and only useful as advisory hints or for debugging.
 *
 * RETURNS:
 * %true if congested, %false otherwise.
 */
bool workqueue_congested(unsigned int cpu, struct workqueue_struct *wq)
{
	struct cpu_workqueue_struct *cwq = get_cwq(cpu, wq);

	return !list_empty(&cwq->delayed_works);
}
EXPORT_SYMBOL_GPL(workqueue_congested);

/**
 * work_cpu - return the last known associated cpu for @work
 * @work: the work of interest
 *
 * RETURNS:
 * CPU number if @work was ever queued.  WORK_CPU_NONE otherwise.
 */
unsigned int work_cpu(struct work_struct *work)
{
	struct global_cwq *gcwq = get_work_gcwq(work);

	return gcwq ? gcwq->cpu : WORK_CPU_NONE;
}
EXPORT_SYMBOL_GPL(work_cpu);

/**
 * work_busy - test whether a work is currently pending or running
 * @work: the work to be tested
 *
 * Test whether @work is currently pending or running.  There is no
 * synchronization around this function and the test result is
 * unreliable and only useful as advisory hints or for debugging.
 * Especially for reentrant wqs, the pending state might hide the
 * running state.
 *
 * RETURNS:
 * OR'd bitmask of WORK_BUSY_* bits.
 */
unsigned int work_busy(struct work_struct *work)
{
	struct global_cwq *gcwq = get_work_gcwq(work);
	unsigned long flags;
	unsigned int ret = 0;

	if (!gcwq)
		return false;

	spin_lock_irqsave(&gcwq->lock, flags);

	if (work_pending(work))
		ret |= WORK_BUSY_PENDING;
	if (find_worker_executing_work(gcwq, work))
		ret |= WORK_BUSY_RUNNING;

	spin_unlock_irqrestore(&gcwq->lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(work_busy);

/*
 * CPU hotplug.
 *
 * There are two challenges in supporting CPU hotplug.  Firstly, there
 * are a lot of assumptions on strong associations among work, cwq and
 * gcwq which make migrating pending and scheduled works very
 * difficult to implement without impacting hot paths.  Secondly,
 * gcwqs serve mix of short, long and very long running works making
 * blocked draining impractical.
 *
 * This is solved by allowing a gcwq to be detached from CPU, running
 * it with unbound (rogue) workers and allowing it to be reattached
 * later if the cpu comes back online.  A separate thread is created
 * to govern a gcwq in such state and is called the trustee of the
 * gcwq.
 *
 * Trustee states and their descriptions.
 *
 * START	Command state used on startup.  On CPU_DOWN_PREPARE, a
 *		new trustee is started with this state.
 *
 * IN_CHARGE	Once started, trustee will enter this state after
 *		assuming the manager role and making all existing
 *		workers rogue.  DOWN_PREPARE waits for trustee to
 *		enter this state.  After reaching IN_CHARGE, trustee
 *		tries to execute the pending worklist until it's empty
 *		and the state is set to BUTCHER, or the state is set
 *		to RELEASE.
 *
 * BUTCHER	Command state which is set by the cpu callback after
 *		the cpu has went down.  Once this state is set trustee
 *		knows that there will be no new works on the worklist
 *		and once the worklist is empty it can proceed to
 *		killing idle workers.
 *
 * RELEASE	Command state which is set by the cpu callback if the
 *		cpu down has been canceled or it has come online
 *		again.  After recognizing this state, trustee stops
 *		trying to drain or butcher and clears ROGUE, rebinds
 *		all remaining workers back to the cpu and releases
 *		manager role.
 *
 * DONE		Trustee will enter this state after BUTCHER or RELEASE
 *		is complete.
 *
 *          trustee                 CPU                draining
 *         took over                down               complete
 * START -----------> IN_CHARGE -----------> BUTCHER -----------> DONE
 *                        |                     |                  ^
 *                        | CPU is back online  v   return workers |
 *                         ----------------> RELEASE --------------
 */

/**
 * trustee_wait_event_timeout - timed event wait for trustee
 * @cond: condition to wait for
 * @timeout: timeout in jiffies
 *
 * wait_event_timeout() for trustee to use.  Handles locking and
 * checks for RELEASE request.
 *
 * CONTEXT:
 * spin_lock_irq(gcwq->lock) which may be released and regrabbed
 * multiple times.  To be used by trustee.
 *
 * RETURNS:
 * Positive indicating left time if @cond is satisfied, 0 if timed
 * out, -1 if canceled.
 */
#define trustee_wait_event_timeout(cond, timeout) ({			\
	long __ret = (timeout);						\
	while (!((cond) || (gcwq->trustee_state == TRUSTEE_RELEASE)) &&	\
	       __ret) {							\
		spin_unlock_irq(&gcwq->lock);				\
		__wait_event_timeout(gcwq->trustee_wait, (cond) ||	\
			(gcwq->trustee_state == TRUSTEE_RELEASE),	\
			__ret);						\
		spin_lock_irq(&gcwq->lock);				\
	}								\
	gcwq->trustee_state == TRUSTEE_RELEASE ? -1 : (__ret);		\
})

/**
 * trustee_wait_event - event wait for trustee
 * @cond: condition to wait for
 *
 * wait_event() for trustee to use.  Automatically handles locking and
 * checks for CANCEL request.
 *
 * CONTEXT:
 * spin_lock_irq(gcwq->lock) which may be released and regrabbed
 * multiple times.  To be used by trustee.
 *
 * RETURNS:
 * 0 if @cond is satisfied, -1 if canceled.
 */
#define trustee_wait_event(cond) ({					\
	long __ret1;							\
	__ret1 = trustee_wait_event_timeout(cond, MAX_SCHEDULE_TIMEOUT);\
	__ret1 < 0 ? -1 : 0;						\
})

static int __cpuinit trustee_thread(void *__gcwq)
{
	struct global_cwq *gcwq = __gcwq;
	struct worker *worker;
	struct work_struct *work;
	struct hlist_node *pos;
	long rc;
	int i;

	BUG_ON(gcwq->cpu != smp_processor_id());

	spin_lock_irq(&gcwq->lock);
	/*
	 * Claim the manager position and make all workers rogue.
	 * Trustee must be bound to the target cpu and can't be
	 * cancelled.
	 */
	BUG_ON(gcwq->cpu != smp_processor_id());
	rc = trustee_wait_event(!(gcwq->flags & GCWQ_MANAGING_WORKERS));
	BUG_ON(rc < 0);

	gcwq->flags |= GCWQ_MANAGING_WORKERS;

	list_for_each_entry(worker, &gcwq->idle_list, entry)
		worker->flags |= WORKER_ROGUE;

	for_each_busy_worker(worker, i, pos, gcwq)
		worker->flags |= WORKER_ROGUE;

	/*
	 * Call schedule() so that we cross rq->lock and thus can
	 * guarantee sched callbacks see the rogue flag.  This is
	 * necessary as scheduler callbacks may be invoked from other
	 * cpus.
	 */
	spin_unlock_irq(&gcwq->lock);
	schedule();
	spin_lock_irq(&gcwq->lock);

	/*
	 * Sched callbacks are disabled now.  Zap nr_running.  After
	 * this, nr_running stays zero and need_more_worker() and
	 * keep_working() are always true as long as the worklist is
	 * not empty.
	 */
	atomic_set(get_gcwq_nr_running(gcwq->cpu), 0);

	spin_unlock_irq(&gcwq->lock);
	del_timer_sync(&gcwq->idle_timer);
	spin_lock_irq(&gcwq->lock);

	/*
	 * We're now in charge.  Notify and proceed to drain.  We need
	 * to keep the gcwq running during the whole CPU down
	 * procedure as other cpu hotunplug callbacks may need to
	 * flush currently running tasks.
	 */
	gcwq->trustee_state = TRUSTEE_IN_CHARGE;
	wake_up_all(&gcwq->trustee_wait);

	/*
	 * The original cpu is in the process of dying and may go away
	 * anytime now.  When that happens, we and all workers would
	 * be migrated to other cpus.  Try draining any left work.  We
	 * want to get it over with ASAP - spam rescuers, wake up as
	 * many idlers as necessary and create new ones till the
	 * worklist is empty.  Note that if the gcwq is frozen, there
	 * may be frozen works in freezeable cwqs.  Don't declare
	 * completion while frozen.
	 */
	while (gcwq->nr_workers != gcwq->nr_idle ||
	       gcwq->flags & GCWQ_FREEZING ||
	       gcwq->trustee_state == TRUSTEE_IN_CHARGE) {
		int nr_works = 0;

		list_for_each_entry(work, &gcwq->worklist, entry) {
			send_mayday(work);
			nr_works++;
		}

		list_for_each_entry(worker, &gcwq->idle_list, entry) {
			if (!nr_works--)
				break;
			wake_up_process(worker->task);
		}

		if (need_to_create_worker(gcwq)) {
			spin_unlock_irq(&gcwq->lock);
			worker = create_worker(gcwq, false);
			spin_lock_irq(&gcwq->lock);
			if (worker) {
				worker->flags |= WORKER_ROGUE;
				start_worker(worker);
			}
		}

		/* give a breather */
		if (trustee_wait_event_timeout(false, TRUSTEE_COOLDOWN) < 0)
			break;
	}

	/*
	 * Either all works have been scheduled and cpu is down, or
	 * cpu down has already been canceled.  Wait for and butcher
	 * all workers till we're canceled.
	 */
	do {
		rc = trustee_wait_event(!list_empty(&gcwq->idle_list));
		while (!list_empty(&gcwq->idle_list))
			destroy_worker(list_first_entry(&gcwq->idle_list,
							struct worker, entry));
	} while (gcwq->nr_workers && rc >= 0);

	/*
	 * At this point, either draining has completed and no worker
	 * is left, or cpu down has been canceled or the cpu is being
	 * brought back up.  There shouldn't be any idle one left.
	 * Tell the remaining busy ones to rebind once it finishes the
	 * currently scheduled works by scheduling the rebind_work.
	 */
	WARN_ON(!list_empty(&gcwq->idle_list));

	for_each_busy_worker(worker, i, pos, gcwq) {
		struct work_struct *rebind_work = &worker->rebind_work;

		/*
		 * Rebind_work may race with future cpu hotplug
		 * operations.  Use a separate flag to mark that
		 * rebinding is scheduled.
		 */
		worker->flags |= WORKER_REBIND;
		worker->flags &= ~WORKER_ROGUE;

		/* queue rebind_work, wq doesn't matter, use the default one */
		if (test_and_set_bit(WORK_STRUCT_PENDING_BIT,
				     work_data_bits(rebind_work)))
			continue;

		debug_work_activate(rebind_work);
		insert_work(get_cwq(gcwq->cpu, system_wq), rebind_work,
			    worker->scheduled.next,
			    work_color_to_flags(WORK_NO_COLOR));
	}

	/* relinquish manager role */
	gcwq->flags &= ~GCWQ_MANAGING_WORKERS;

	/* notify completion */
	gcwq->trustee = NULL;
	gcwq->trustee_state = TRUSTEE_DONE;
	wake_up_all(&gcwq->trustee_wait);
	spin_unlock_irq(&gcwq->lock);
	return 0;
}

/**
 * wait_trustee_state - wait for trustee to enter the specified state
 * @gcwq: gcwq the trustee of interest belongs to
 * @state: target state to wait for
 *
 * Wait for the trustee to reach @state.  DONE is already matched.
 *
 * CONTEXT:
 * spin_lock_irq(gcwq->lock) which may be released and regrabbed
 * multiple times.  To be used by cpu_callback.
 */
static void __cpuinit wait_trustee_state(struct global_cwq *gcwq, int state)
__releases(&gcwq->lock)
__acquires(&gcwq->lock)
{
	if (!(gcwq->trustee_state == state ||
	      gcwq->trustee_state == TRUSTEE_DONE)) {
		spin_unlock_irq(&gcwq->lock);
		__wait_event(gcwq->trustee_wait,
			     gcwq->trustee_state == state ||
			     gcwq->trustee_state == TRUSTEE_DONE);
		spin_lock_irq(&gcwq->lock);
	}
}

static int __devinit workqueue_cpu_callback(struct notifier_block *nfb,
						unsigned long action,
						void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;
	struct global_cwq *gcwq = get_gcwq(cpu);
	struct task_struct *new_trustee = NULL;
	struct worker *uninitialized_var(new_worker);
	unsigned long flags;

	action &= ~CPU_TASKS_FROZEN;

	switch (action) {
	case CPU_DOWN_PREPARE:
		new_trustee = kthread_create(trustee_thread, gcwq,
					     "workqueue_trustee/%d\n", cpu);
		if (IS_ERR(new_trustee))
			return notifier_from_errno(PTR_ERR(new_trustee));
		kthread_bind(new_trustee, cpu);
		/* fall through */
	case CPU_UP_PREPARE:
		BUG_ON(gcwq->first_idle);
		new_worker = create_worker(gcwq, false);
		if (!new_worker) {
			if (new_trustee)
				kthread_stop(new_trustee);
			return NOTIFY_BAD;
		}
	}

	/* some are called w/ irq disabled, don't disturb irq status */
	spin_lock_irqsave(&gcwq->lock, flags);

	switch (action) {
	case CPU_DOWN_PREPARE:
		/* initialize trustee and tell it to acquire the gcwq */
		BUG_ON(gcwq->trustee || gcwq->trustee_state != TRUSTEE_DONE);
		gcwq->trustee = new_trustee;
		gcwq->trustee_state = TRUSTEE_START;
		wake_up_process(gcwq->trustee);
		wait_trustee_state(gcwq, TRUSTEE_IN_CHARGE);
		/* fall through */
	case CPU_UP_PREPARE:
		BUG_ON(gcwq->first_idle);
		gcwq->first_idle = new_worker;
		break;

	case CPU_DYING:
		/*
		 * Before this, the trustee and all workers except for
		 * the ones which are still executing works from
		 * before the last CPU down must be on the cpu.  After
		 * this, they'll all be diasporas.
		 */
		gcwq->flags |= GCWQ_DISASSOCIATED;
		break;

	case CPU_POST_DEAD:
		gcwq->trustee_state = TRUSTEE_BUTCHER;
		/* fall through */
	case CPU_UP_CANCELED:
		destroy_worker(gcwq->first_idle);
		gcwq->first_idle = NULL;
		break;

	case CPU_DOWN_FAILED:
	case CPU_ONLINE:
		gcwq->flags &= ~GCWQ_DISASSOCIATED;
		if (gcwq->trustee_state != TRUSTEE_DONE) {
			gcwq->trustee_state = TRUSTEE_RELEASE;
			wake_up_process(gcwq->trustee);
			wait_trustee_state(gcwq, TRUSTEE_DONE);
		}

		/*
		 * Trustee is done and there might be no worker left.
		 * Put the first_idle in and request a real manager to
		 * take a look.
		 */
		spin_unlock_irq(&gcwq->lock);
		kthread_bind(gcwq->first_idle->task, cpu);
		spin_lock_irq(&gcwq->lock);
		gcwq->flags |= GCWQ_MANAGE_WORKERS;
		start_worker(gcwq->first_idle);
		gcwq->first_idle = NULL;
		break;
	}

	spin_unlock_irqrestore(&gcwq->lock, flags);

	return notifier_from_errno(0);
}

#ifdef CONFIG_SMP

struct work_for_cpu {
	struct completion completion;
	long (*fn)(void *);
	void *arg;
	long ret;
};

static int do_work_for_cpu(void *_wfc)
{
	struct work_for_cpu *wfc = _wfc;
	wfc->ret = wfc->fn(wfc->arg);
	complete(&wfc->completion);
	return 0;
}

/**
 * work_on_cpu - run a function in user context on a particular cpu
 * @cpu: the cpu to run on
 * @fn: the function to run
 * @arg: the function arg
 *
 * This will return the value @fn returns.
 * It is up to the caller to ensure that the cpu doesn't go offline.
 * The caller must not hold any locks which would prevent @fn from completing.
 */
long work_on_cpu(unsigned int cpu, long (*fn)(void *), void *arg)
{
	struct task_struct *sub_thread;
	struct work_for_cpu wfc = {
		.completion = COMPLETION_INITIALIZER_ONSTACK(wfc.completion),
		.fn = fn,
		.arg = arg,
	};

	sub_thread = kthread_create(do_work_for_cpu, &wfc, "work_for_cpu");
	if (IS_ERR(sub_thread))
		return PTR_ERR(sub_thread);
	kthread_bind(sub_thread, cpu);
	wake_up_process(sub_thread);
	wait_for_completion(&wfc.completion);
	return wfc.ret;
}
EXPORT_SYMBOL_GPL(work_on_cpu);
#endif /* CONFIG_SMP */

#ifdef CONFIG_FREEZER

/**
 * freeze_workqueues_begin - begin freezing workqueues
 *
 * Start freezing workqueues.  After this function returns, all
 * freezeable workqueues will queue new works to their frozen_works
 * list instead of gcwq->worklist.
 *
 * CONTEXT:
 * Grabs and releases workqueue_lock and gcwq->lock's.
 */
void freeze_workqueues_begin(void)
{
	unsigned int cpu;

	spin_lock(&workqueue_lock);

	BUG_ON(workqueue_freezing);
	workqueue_freezing = true;

	for_each_gcwq_cpu(cpu) {
		struct global_cwq *gcwq = get_gcwq(cpu);
		struct workqueue_struct *wq;

		spin_lock_irq(&gcwq->lock);

		BUG_ON(gcwq->flags & GCWQ_FREEZING);
		gcwq->flags |= GCWQ_FREEZING;

		list_for_each_entry(wq, &workqueues, list) {
			struct cpu_workqueue_struct *cwq = get_cwq(cpu, wq);

			if (cwq && wq->flags & WQ_FREEZEABLE)
				cwq->max_active = 0;
		}

		spin_unlock_irq(&gcwq->lock);
	}

	spin_unlock(&workqueue_lock);
}

/**
 * freeze_workqueues_busy - are freezeable workqueues still busy?
 *
 * Check whether freezing is complete.  This function must be called
 * between freeze_workqueues_begin() and thaw_workqueues().
 *
 * CONTEXT:
 * Grabs and releases workqueue_lock.
 *
 * RETURNS:
 * %true if some freezeable workqueues are still busy.  %false if
 * freezing is complete.
 */
bool freeze_workqueues_busy(void)
{
	unsigned int cpu;
	bool busy = false;

	spin_lock(&workqueue_lock);

	BUG_ON(!workqueue_freezing);

	for_each_gcwq_cpu(cpu) {
		struct workqueue_struct *wq;
		/*
		 * nr_active is monotonically decreasing.  It's safe
		 * to peek without lock.
		 */
		list_for_each_entry(wq, &workqueues, list) {
			struct cpu_workqueue_struct *cwq = get_cwq(cpu, wq);

			if (!cwq || !(wq->flags & WQ_FREEZEABLE))
				continue;

			BUG_ON(cwq->nr_active < 0);
			if (cwq->nr_active) {
				busy = true;
				goto out_unlock;
			}
		}
	}
out_unlock:
	spin_unlock(&workqueue_lock);
	return busy;
}

/**
 * thaw_workqueues - thaw workqueues
 *
 * Thaw workqueues.  Normal queueing is restored and all collected
 * frozen works are transferred to their respective gcwq worklists.
 *
 * CONTEXT:
 * Grabs and releases workqueue_lock and gcwq->lock's.
 */
void thaw_workqueues(void)
{
	unsigned int cpu;

	spin_lock(&workqueue_lock);

	if (!workqueue_freezing)
		goto out_unlock;

	for_each_gcwq_cpu(cpu) {
		struct global_cwq *gcwq = get_gcwq(cpu);
		struct workqueue_struct *wq;

		spin_lock_irq(&gcwq->lock);

		BUG_ON(!(gcwq->flags & GCWQ_FREEZING));
		gcwq->flags &= ~GCWQ_FREEZING;

		list_for_each_entry(wq, &workqueues, list) {
			struct cpu_workqueue_struct *cwq = get_cwq(cpu, wq);

			if (!cwq || !(wq->flags & WQ_FREEZEABLE))
				continue;

			/* restore max_active and repopulate worklist */
			cwq->max_active = wq->saved_max_active;

			while (!list_empty(&cwq->delayed_works) &&
			       cwq->nr_active < cwq->max_active)
				cwq_activate_first_delayed(cwq);
		}

		wake_up_worker(gcwq);

		spin_unlock_irq(&gcwq->lock);
	}

	workqueue_freezing = false;
out_unlock:
	spin_unlock(&workqueue_lock);
}
#endif /* CONFIG_FREEZER */

static int __init init_workqueues(void)
{
	unsigned int cpu;
	int i;

	cpu_notifier(workqueue_cpu_callback, CPU_PRI_WORKQUEUE);

	/* initialize gcwqs */
	for_each_gcwq_cpu(cpu) {
		struct global_cwq *gcwq = get_gcwq(cpu);

		spin_lock_init(&gcwq->lock);
		INIT_LIST_HEAD(&gcwq->worklist);
		gcwq->cpu = cpu;
		gcwq->flags |= GCWQ_DISASSOCIATED;

		INIT_LIST_HEAD(&gcwq->idle_list);
		for (i = 0; i < BUSY_WORKER_HASH_SIZE; i++)
			INIT_HLIST_HEAD(&gcwq->busy_hash[i]);

		init_timer_deferrable(&gcwq->idle_timer);
		gcwq->idle_timer.function = idle_worker_timeout;
		gcwq->idle_timer.data = (unsigned long)gcwq;

		setup_timer(&gcwq->mayday_timer, gcwq_mayday_timeout,
			    (unsigned long)gcwq);

		ida_init(&gcwq->worker_ida);

		gcwq->trustee_state = TRUSTEE_DONE;
		init_waitqueue_head(&gcwq->trustee_wait);
	}

	/* create the initial worker */
	for_each_online_gcwq_cpu(cpu) {
		struct global_cwq *gcwq = get_gcwq(cpu);
		struct worker *worker;

		if (cpu != WORK_CPU_UNBOUND)
			gcwq->flags &= ~GCWQ_DISASSOCIATED;
		worker = create_worker(gcwq, true);
		BUG_ON(!worker);
		spin_lock_irq(&gcwq->lock);
		start_worker(worker);
		spin_unlock_irq(&gcwq->lock);
	}

	system_wq = alloc_workqueue("events", 0, 0);
	system_long_wq = alloc_workqueue("events_long", 0, 0);
	system_nrt_wq = alloc_workqueue("events_nrt", WQ_NON_REENTRANT, 0);
	system_unbound_wq = alloc_workqueue("events_unbound", WQ_UNBOUND,
					    WQ_UNBOUND_MAX_ACTIVE);
	BUG_ON(!system_wq || !system_long_wq || !system_nrt_wq);
	return 0;
}
early_initcall(init_workqueues);
