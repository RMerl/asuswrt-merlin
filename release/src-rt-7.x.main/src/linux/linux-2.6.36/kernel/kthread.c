/* Kernel thread helper functions.
 *   Copyright (C) 2004 IBM Corporation, Rusty Russell.
 *
 * Creation is done via kthreadd, so that we get a clean environment
 * even if we're invoked from userspace (think modprobe, hotplug cpu,
 * etc.).
 */
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/err.h>
#include <linux/cpuset.h>
#include <linux/unistd.h>
#include <linux/file.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/freezer.h>
#include <trace/events/sched.h>

static DEFINE_SPINLOCK(kthread_create_lock);
static LIST_HEAD(kthread_create_list);
struct task_struct *kthreadd_task;

struct kthread_create_info
{
	/* Information passed to kthread() from kthreadd. */
	int (*threadfn)(void *data);
	void *data;

	/* Result passed back to kthread_create() from kthreadd. */
	struct task_struct *result;
	struct completion done;

	struct list_head list;
};

struct kthread {
	int should_stop;
	void *data;
	struct completion exited;
};

#define to_kthread(tsk)	\
	container_of((tsk)->vfork_done, struct kthread, exited)

/**
 * kthread_should_stop - should this kthread return now?
 *
 * When someone calls kthread_stop() on your kthread, it will be woken
 * and this will return true.  You should then return, and your return
 * value will be passed through to kthread_stop().
 */
int kthread_should_stop(void)
{
	return to_kthread(current)->should_stop;
}
EXPORT_SYMBOL(kthread_should_stop);

/**
 * kthread_data - return data value specified on kthread creation
 * @task: kthread task in question
 *
 * Return the data value specified when kthread @task was created.
 * The caller is responsible for ensuring the validity of @task when
 * calling this function.
 */
void *kthread_data(struct task_struct *task)
{
	return to_kthread(task)->data;
}

static int kthread(void *_create)
{
	/* Copy data: it's on kthread's stack */
	struct kthread_create_info *create = _create;
	int (*threadfn)(void *data) = create->threadfn;
	void *data = create->data;
	struct kthread self;
	int ret;

	self.should_stop = 0;
	self.data = data;
	init_completion(&self.exited);
	current->vfork_done = &self.exited;

	/* OK, tell user we're spawned, wait for stop or wakeup */
	__set_current_state(TASK_UNINTERRUPTIBLE);
	create->result = current;
	complete(&create->done);
	schedule();

	ret = -EINTR;
	if (!self.should_stop)
		ret = threadfn(data);

	/* we can't just return, we must preserve "self" on stack */
	do_exit(ret);
}

static void create_kthread(struct kthread_create_info *create)
{
	int pid;

	/* We want our own signal handler (we take no signals by default). */
	pid = kernel_thread(kthread, create, CLONE_FS | CLONE_FILES | SIGCHLD);
	if (pid < 0) {
		create->result = ERR_PTR(pid);
		complete(&create->done);
	}
}

/**
 * kthread_create - create a kthread.
 * @threadfn: the function to run until signal_pending(current).
 * @data: data ptr for @threadfn.
 * @namefmt: printf-style name for the thread.
 *
 * Description: This helper function creates and names a kernel
 * thread.  The thread will be stopped: use wake_up_process() to start
 * it.  See also kthread_run().
 *
 * When woken, the thread will run @threadfn() with @data as its
 * argument. @threadfn() can either call do_exit() directly if it is a
 * standalone thread for which noone will call kthread_stop(), or
 * return when 'kthread_should_stop()' is true (which means
 * kthread_stop() has been called).  The return value should be zero
 * or a negative error number; it will be passed to kthread_stop().
 *
 * Returns a task_struct or ERR_PTR(-ENOMEM).
 */
struct task_struct *kthread_create(int (*threadfn)(void *data),
				   void *data,
				   const char namefmt[],
				   ...)
{
	struct kthread_create_info create;

	create.threadfn = threadfn;
	create.data = data;
	init_completion(&create.done);

	spin_lock(&kthread_create_lock);
	list_add_tail(&create.list, &kthread_create_list);
	spin_unlock(&kthread_create_lock);

	wake_up_process(kthreadd_task);
	wait_for_completion(&create.done);

	if (!IS_ERR(create.result)) {
		struct sched_param param = { .sched_priority = 0 };
		va_list args;

		va_start(args, namefmt);
		vsnprintf(create.result->comm, sizeof(create.result->comm),
			  namefmt, args);
		va_end(args);
		/*
		 * root may have changed our (kthreadd's) priority or CPU mask.
		 * The kernel thread should not inherit these properties.
		 */
		sched_setscheduler_nocheck(create.result, SCHED_NORMAL, &param);
		set_cpus_allowed_ptr(create.result, cpu_all_mask);
	}
	return create.result;
}
EXPORT_SYMBOL(kthread_create);

/**
 * kthread_bind - bind a just-created kthread to a cpu.
 * @p: thread created by kthread_create().
 * @cpu: cpu (might not be online, must be possible) for @k to run on.
 *
 * Description: This function is equivalent to set_cpus_allowed(),
 * except that @cpu doesn't need to be online, and the thread must be
 * stopped (i.e., just returned from kthread_create()).
 */
void kthread_bind(struct task_struct *p, unsigned int cpu)
{
	/* Must have done schedule() in kthread() before we set_task_cpu */
	if (!wait_task_inactive(p, TASK_UNINTERRUPTIBLE)) {
		WARN_ON(1);
		return;
	}

	p->cpus_allowed = cpumask_of_cpu(cpu);
	p->rt.nr_cpus_allowed = 1;
	p->flags |= PF_THREAD_BOUND;
}
EXPORT_SYMBOL(kthread_bind);

/**
 * kthread_stop - stop a thread created by kthread_create().
 * @k: thread created by kthread_create().
 *
 * Sets kthread_should_stop() for @k to return true, wakes it, and
 * waits for it to exit. This can also be called after kthread_create()
 * instead of calling wake_up_process(): the thread will exit without
 * calling threadfn().
 *
 * If threadfn() may call do_exit() itself, the caller must ensure
 * task_struct can't go away.
 *
 * Returns the result of threadfn(), or %-EINTR if wake_up_process()
 * was never called.
 */
int kthread_stop(struct task_struct *k)
{
	struct kthread *kthread;
	int ret;

	trace_sched_kthread_stop(k);
	get_task_struct(k);

	kthread = to_kthread(k);
	barrier(); /* it might have exited */
	if (k->vfork_done != NULL) {
		kthread->should_stop = 1;
		wake_up_process(k);
		wait_for_completion(&kthread->exited);
	}
	ret = k->exit_code;

	put_task_struct(k);
	trace_sched_kthread_stop_ret(ret);

	return ret;
}
EXPORT_SYMBOL(kthread_stop);

int kthreadd(void *unused)
{
	struct task_struct *tsk = current;

	/* Setup a clean context for our children to inherit. */
	set_task_comm(tsk, "kthreadd");
	ignore_signals(tsk);
	set_cpus_allowed_ptr(tsk, cpu_all_mask);
	set_mems_allowed(node_states[N_HIGH_MEMORY]);

	current->flags |= PF_NOFREEZE | PF_FREEZER_NOSIG;

	for (;;) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (list_empty(&kthread_create_list))
			schedule();
		__set_current_state(TASK_RUNNING);

		spin_lock(&kthread_create_lock);
		while (!list_empty(&kthread_create_list)) {
			struct kthread_create_info *create;

			create = list_entry(kthread_create_list.next,
					    struct kthread_create_info, list);
			list_del_init(&create->list);
			spin_unlock(&kthread_create_lock);

			create_kthread(create);

			spin_lock(&kthread_create_lock);
		}
		spin_unlock(&kthread_create_lock);
	}

	return 0;
}

/**
 * kthread_worker_fn - kthread function to process kthread_worker
 * @worker_ptr: pointer to initialized kthread_worker
 *
 * This function can be used as @threadfn to kthread_create() or
 * kthread_run() with @worker_ptr argument pointing to an initialized
 * kthread_worker.  The started kthread will process work_list until
 * the it is stopped with kthread_stop().  A kthread can also call
 * this function directly after extra initialization.
 *
 * Different kthreads can be used for the same kthread_worker as long
 * as there's only one kthread attached to it at any given time.  A
 * kthread_worker without an attached kthread simply collects queued
 * kthread_works.
 */
int kthread_worker_fn(void *worker_ptr)
{
	struct kthread_worker *worker = worker_ptr;
	struct kthread_work *work;

	WARN_ON(worker->task);
	worker->task = current;
repeat:
	set_current_state(TASK_INTERRUPTIBLE);	/* mb paired w/ kthread_stop */

	if (kthread_should_stop()) {
		__set_current_state(TASK_RUNNING);
		spin_lock_irq(&worker->lock);
		worker->task = NULL;
		spin_unlock_irq(&worker->lock);
		return 0;
	}

	work = NULL;
	spin_lock_irq(&worker->lock);
	if (!list_empty(&worker->work_list)) {
		work = list_first_entry(&worker->work_list,
					struct kthread_work, node);
		list_del_init(&work->node);
	}
	spin_unlock_irq(&worker->lock);

	if (work) {
		__set_current_state(TASK_RUNNING);
		work->func(work);
		smp_wmb();	/* wmb worker-b0 paired with flush-b1 */
		work->done_seq = work->queue_seq;
		smp_mb();	/* mb worker-b1 paired with flush-b0 */
		if (atomic_read(&work->flushing))
			wake_up_all(&work->done);
	} else if (!freezing(current))
		schedule();

	try_to_freeze();
	goto repeat;
}
EXPORT_SYMBOL_GPL(kthread_worker_fn);

/**
 * queue_kthread_work - queue a kthread_work
 * @worker: target kthread_worker
 * @work: kthread_work to queue
 *
 * Queue @work to work processor @task for async execution.  @task
 * must have been created with kthread_worker_create().  Returns %true
 * if @work was successfully queued, %false if it was already pending.
 */
bool queue_kthread_work(struct kthread_worker *worker,
			struct kthread_work *work)
{
	bool ret = false;
	unsigned long flags;

	spin_lock_irqsave(&worker->lock, flags);
	if (list_empty(&work->node)) {
		list_add_tail(&work->node, &worker->work_list);
		work->queue_seq++;
		if (likely(worker->task))
			wake_up_process(worker->task);
		ret = true;
	}
	spin_unlock_irqrestore(&worker->lock, flags);
	return ret;
}
EXPORT_SYMBOL_GPL(queue_kthread_work);

/**
 * flush_kthread_work - flush a kthread_work
 * @work: work to flush
 *
 * If @work is queued or executing, wait for it to finish execution.
 */
void flush_kthread_work(struct kthread_work *work)
{
	int seq = work->queue_seq;

	atomic_inc(&work->flushing);

	/*
	 * mb flush-b0 paired with worker-b1, to make sure either
	 * worker sees the above increment or we see done_seq update.
	 */
	smp_mb__after_atomic_inc();

	/* A - B <= 0 tests whether B is in front of A regardless of overflow */
	wait_event(work->done, seq - work->done_seq <= 0);
	atomic_dec(&work->flushing);

	/*
	 * rmb flush-b1 paired with worker-b0, to make sure our caller
	 * sees every change made by work->func().
	 */
	smp_mb__after_atomic_dec();
}
EXPORT_SYMBOL_GPL(flush_kthread_work);

struct kthread_flush_work {
	struct kthread_work	work;
	struct completion	done;
};

static void kthread_flush_work_fn(struct kthread_work *work)
{
	struct kthread_flush_work *fwork =
		container_of(work, struct kthread_flush_work, work);
	complete(&fwork->done);
}

/**
 * flush_kthread_worker - flush all current works on a kthread_worker
 * @worker: worker to flush
 *
 * Wait until all currently executing or pending works on @worker are
 * finished.
 */
void flush_kthread_worker(struct kthread_worker *worker)
{
	struct kthread_flush_work fwork = {
		KTHREAD_WORK_INIT(fwork.work, kthread_flush_work_fn),
		COMPLETION_INITIALIZER_ONSTACK(fwork.done),
	};

	queue_kthread_work(worker, &fwork.work);
	wait_for_completion(&fwork.done);
}
EXPORT_SYMBOL_GPL(flush_kthread_worker);
