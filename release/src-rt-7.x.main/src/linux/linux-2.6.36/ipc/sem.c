/*
 * linux/ipc/sem.c
 * Copyright (C) 1992 Krishna Balasubramanian
 * Copyright (C) 1995 Eric Schenk, Bruno Haible
 *
 * /proc/sysvipc/sem support (c) 1999 Dragos Acostachioaie <dragos@iname.com>
 *
 * SMP-threaded, sysctl's added
 * (c) 1999 Manfred Spraul <manfred@colorfullife.com>
 * Enforced range limit on SEM_UNDO
 * (c) 2001 Red Hat Inc
 * Lockless wakeup
 * (c) 2003 Manfred Spraul <manfred@colorfullife.com>
 * Further wakeup optimizations, documentation
 * (c) 2010 Manfred Spraul <manfred@colorfullife.com>
 *
 * support for audit of ipc object properties and permission changes
 * Dustin Kirkland <dustin.kirkland@us.ibm.com>
 *
 * namespaces support
 * OpenVZ, SWsoft Inc.
 * Pavel Emelianov <xemul@openvz.org>
 *
 * Implementation notes: (May 2010)
 * This file implements System V semaphores.
 *
 * User space visible behavior:
 * - FIFO ordering for semop() operations (just FIFO, not starvation
 *   protection)
 * - multiple semaphore operations that alter the same semaphore in
 *   one semop() are handled.
 * - sem_ctime (time of last semctl()) is updated in the IPC_SET, SETVAL and
 *   SETALL calls.
 * - two Linux specific semctl() commands: SEM_STAT, SEM_INFO.
 * - undo adjustments at process exit are limited to 0..SEMVMX.
 * - namespace are supported.
 * - SEMMSL, SEMMNS, SEMOPM and SEMMNI can be configured at runtine by writing
 *   to /proc/sys/kernel/sem.
 * - statistics about the usage are reported in /proc/sysvipc/sem.
 *
 * Internals:
 * - scalability:
 *   - all global variables are read-mostly.
 *   - semop() calls and semctl(RMID) are synchronized by RCU.
 *   - most operations do write operations (actually: spin_lock calls) to
 *     the per-semaphore array structure.
 *   Thus: Perfect SMP scaling between independent semaphore arrays.
 *         If multiple semaphores in one array are used, then cache line
 *         trashing on the semaphore array spinlock will limit the scaling.
 * - semncnt and semzcnt are calculated on demand in count_semncnt() and
 *   count_semzcnt()
 * - the task that performs a successful semop() scans the list of all
 *   sleeping tasks and completes any pending operations that can be fulfilled.
 *   Semaphores are actively given to waiting tasks (necessary for FIFO).
 *   (see update_queue())
 * - To improve the scalability, the actual wake-up calls are performed after
 *   dropping all locks. (see wake_up_sem_queue_prepare(),
 *   wake_up_sem_queue_do())
 * - All work is done by the waker, the woken up task does not have to do
 *   anything - not even acquiring a lock or dropping a refcount.
 * - A woken up task may not even touch the semaphore array anymore, it may
 *   have been destroyed already by a semctl(RMID).
 * - The synchronizations between wake-ups due to a timeout/signal and a
 *   wake-up due to a completed semaphore operation is achieved by using an
 *   intermediate state (IN_WAKEUP).
 * - UNDO values are stored in an array (one per process and per
 *   semaphore array, lazily allocated). For backwards compatibility, multiple
 *   modes for the UNDO variables are supported (per process, per thread)
 *   (see copy_semundo, CLONE_SYSVSEM)
 * - There are two lists of the pending operations: a per-array list
 *   and per-semaphore list (stored in the array). This allows to achieve FIFO
 *   ordering without always scanning all pending operations.
 *   The worst-case behavior is nevertheless O(N^2) for N wakeups.
 */

#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/time.h>
#include <linux/security.h>
#include <linux/syscalls.h>
#include <linux/audit.h>
#include <linux/capability.h>
#include <linux/seq_file.h>
#include <linux/rwsem.h>
#include <linux/nsproxy.h>
#include <linux/ipc_namespace.h>

#include <asm/uaccess.h>
#include "util.h"

#define sem_ids(ns)	((ns)->ids[IPC_SEM_IDS])

#define sem_unlock(sma)		ipc_unlock(&(sma)->sem_perm)
#define sem_checkid(sma, semid)	ipc_checkid(&sma->sem_perm, semid)

static int newary(struct ipc_namespace *, struct ipc_params *);
static void freeary(struct ipc_namespace *, struct kern_ipc_perm *);
#ifdef CONFIG_PROC_FS
static int sysvipc_sem_proc_show(struct seq_file *s, void *it);
#endif

#define SEMMSL_FAST	256 /* 512 bytes on stack */
#define SEMOPM_FAST	64  /* ~ 372 bytes on stack */

/*
 * linked list protection:
 *	sem_undo.id_next,
 *	sem_array.sem_pending{,last},
 *	sem_array.sem_undo: sem_lock() for read/write
 *	sem_undo.proc_next: only "current" is allowed to read/write that field.
 *	
 */

#define sc_semmsl	sem_ctls[0]
#define sc_semmns	sem_ctls[1]
#define sc_semopm	sem_ctls[2]
#define sc_semmni	sem_ctls[3]

void sem_init_ns(struct ipc_namespace *ns)
{
	ns->sc_semmsl = SEMMSL;
	ns->sc_semmns = SEMMNS;
	ns->sc_semopm = SEMOPM;
	ns->sc_semmni = SEMMNI;
	ns->used_sems = 0;
	ipc_init_ids(&ns->ids[IPC_SEM_IDS]);
}

#ifdef CONFIG_IPC_NS
void sem_exit_ns(struct ipc_namespace *ns)
{
	free_ipcs(ns, &sem_ids(ns), freeary);
	idr_destroy(&ns->ids[IPC_SEM_IDS].ipcs_idr);
}
#endif

void __init sem_init (void)
{
	sem_init_ns(&init_ipc_ns);
	ipc_init_proc_interface("sysvipc/sem",
				"       key      semid perms      nsems   uid   gid  cuid  cgid      otime      ctime\n",
				IPC_SEM_IDS, sysvipc_sem_proc_show);
}

/*
 * sem_lock_(check_) routines are called in the paths where the rw_mutex
 * is not held.
 */
static inline struct sem_array *sem_lock(struct ipc_namespace *ns, int id)
{
	struct kern_ipc_perm *ipcp = ipc_lock(&sem_ids(ns), id);

	if (IS_ERR(ipcp))
		return (struct sem_array *)ipcp;

	return container_of(ipcp, struct sem_array, sem_perm);
}

static inline struct sem_array *sem_lock_check(struct ipc_namespace *ns,
						int id)
{
	struct kern_ipc_perm *ipcp = ipc_lock_check(&sem_ids(ns), id);

	if (IS_ERR(ipcp))
		return (struct sem_array *)ipcp;

	return container_of(ipcp, struct sem_array, sem_perm);
}

static inline void sem_lock_and_putref(struct sem_array *sma)
{
	ipc_lock_by_ptr(&sma->sem_perm);
	ipc_rcu_putref(sma);
}

static inline void sem_getref_and_unlock(struct sem_array *sma)
{
	ipc_rcu_getref(sma);
	ipc_unlock(&(sma)->sem_perm);
}

static inline void sem_putref(struct sem_array *sma)
{
	ipc_lock_by_ptr(&sma->sem_perm);
	ipc_rcu_putref(sma);
	ipc_unlock(&(sma)->sem_perm);
}

static inline void sem_rmid(struct ipc_namespace *ns, struct sem_array *s)
{
	ipc_rmid(&sem_ids(ns), &s->sem_perm);
}

/*
 * Lockless wakeup algorithm:
 * Without the check/retry algorithm a lockless wakeup is possible:
 * - queue.status is initialized to -EINTR before blocking.
 * - wakeup is performed by
 *	* unlinking the queue entry from sma->sem_pending
 *	* setting queue.status to IN_WAKEUP
 *	  This is the notification for the blocked thread that a
 *	  result value is imminent.
 *	* call wake_up_process
 *	* set queue.status to the final value.
 * - the previously blocked thread checks queue.status:
 *   	* if it's IN_WAKEUP, then it must wait until the value changes
 *   	* if it's not -EINTR, then the operation was completed by
 *   	  update_queue. semtimedop can return queue.status without
 *   	  performing any operation on the sem array.
 *   	* otherwise it must acquire the spinlock and check what's up.
 *
 * The two-stage algorithm is necessary to protect against the following
 * races:
 * - if queue.status is set after wake_up_process, then the woken up idle
 *   thread could race forward and try (and fail) to acquire sma->lock
 *   before update_queue had a chance to set queue.status
 * - if queue.status is written before wake_up_process and if the
 *   blocked process is woken up by a signal between writing
 *   queue.status and the wake_up_process, then the woken up
 *   process could return from semtimedop and die by calling
 *   sys_exit before wake_up_process is called. Then wake_up_process
 *   will oops, because the task structure is already invalid.
 *   (yes, this happened on s390 with sysv msg).
 *
 */
#define IN_WAKEUP	1

/**
 * newary - Create a new semaphore set
 * @ns: namespace
 * @params: ptr to the structure that contains key, semflg and nsems
 *
 * Called with sem_ids.rw_mutex held (as a writer)
 */

static int newary(struct ipc_namespace *ns, struct ipc_params *params)
{
	int id;
	int retval;
	struct sem_array *sma;
	int size;
	key_t key = params->key;
	int nsems = params->u.nsems;
	int semflg = params->flg;
	int i;

	if (!nsems)
		return -EINVAL;
	if (ns->used_sems + nsems > ns->sc_semmns)
		return -ENOSPC;

	size = sizeof (*sma) + nsems * sizeof (struct sem);
	sma = ipc_rcu_alloc(size);
	if (!sma) {
		return -ENOMEM;
	}
	memset (sma, 0, size);

	sma->sem_perm.mode = (semflg & S_IRWXUGO);
	sma->sem_perm.key = key;

	sma->sem_perm.security = NULL;
	retval = security_sem_alloc(sma);
	if (retval) {
		ipc_rcu_putref(sma);
		return retval;
	}

	id = ipc_addid(&sem_ids(ns), &sma->sem_perm, ns->sc_semmni);
	if (id < 0) {
		security_sem_free(sma);
		ipc_rcu_putref(sma);
		return id;
	}
	ns->used_sems += nsems;

	sma->sem_base = (struct sem *) &sma[1];

	for (i = 0; i < nsems; i++)
		INIT_LIST_HEAD(&sma->sem_base[i].sem_pending);

	sma->complex_count = 0;
	INIT_LIST_HEAD(&sma->sem_pending);
	INIT_LIST_HEAD(&sma->list_id);
	sma->sem_nsems = nsems;
	sma->sem_ctime = get_seconds();
	sem_unlock(sma);

	return sma->sem_perm.id;
}


/*
 * Called with sem_ids.rw_mutex and ipcp locked.
 */
static inline int sem_security(struct kern_ipc_perm *ipcp, int semflg)
{
	struct sem_array *sma;

	sma = container_of(ipcp, struct sem_array, sem_perm);
	return security_sem_associate(sma, semflg);
}

/*
 * Called with sem_ids.rw_mutex and ipcp locked.
 */
static inline int sem_more_checks(struct kern_ipc_perm *ipcp,
				struct ipc_params *params)
{
	struct sem_array *sma;

	sma = container_of(ipcp, struct sem_array, sem_perm);
	if (params->u.nsems > sma->sem_nsems)
		return -EINVAL;

	return 0;
}

SYSCALL_DEFINE3(semget, key_t, key, int, nsems, int, semflg)
{
	struct ipc_namespace *ns;
	struct ipc_ops sem_ops;
	struct ipc_params sem_params;

	ns = current->nsproxy->ipc_ns;

	if (nsems < 0 || nsems > ns->sc_semmsl)
		return -EINVAL;

	sem_ops.getnew = newary;
	sem_ops.associate = sem_security;
	sem_ops.more_checks = sem_more_checks;

	sem_params.key = key;
	sem_params.flg = semflg;
	sem_params.u.nsems = nsems;

	return ipcget(ns, &sem_ids(ns), &sem_ops, &sem_params);
}

/*
 * Determine whether a sequence of semaphore operations would succeed
 * all at once. Return 0 if yes, 1 if need to sleep, else return error code.
 */

static int try_atomic_semop (struct sem_array * sma, struct sembuf * sops,
			     int nsops, struct sem_undo *un, int pid)
{
	int result, sem_op;
	struct sembuf *sop;
	struct sem * curr;

	for (sop = sops; sop < sops + nsops; sop++) {
		curr = sma->sem_base + sop->sem_num;
		sem_op = sop->sem_op;
		result = curr->semval;
  
		if (!sem_op && result)
			goto would_block;

		result += sem_op;
		if (result < 0)
			goto would_block;
		if (result > SEMVMX)
			goto out_of_range;
		if (sop->sem_flg & SEM_UNDO) {
			int undo = un->semadj[sop->sem_num] - sem_op;
			/*
	 		 *	Exceeding the undo range is an error.
			 */
			if (undo < (-SEMAEM - 1) || undo > SEMAEM)
				goto out_of_range;
		}
		curr->semval = result;
	}

	sop--;
	while (sop >= sops) {
		sma->sem_base[sop->sem_num].sempid = pid;
		if (sop->sem_flg & SEM_UNDO)
			un->semadj[sop->sem_num] -= sop->sem_op;
		sop--;
	}
	
	return 0;

out_of_range:
	result = -ERANGE;
	goto undo;

would_block:
	if (sop->sem_flg & IPC_NOWAIT)
		result = -EAGAIN;
	else
		result = 1;

undo:
	sop--;
	while (sop >= sops) {
		sma->sem_base[sop->sem_num].semval -= sop->sem_op;
		sop--;
	}

	return result;
}

/** wake_up_sem_queue_prepare(q, error): Prepare wake-up
 * @q: queue entry that must be signaled
 * @error: Error value for the signal
 *
 * Prepare the wake-up of the queue entry q.
 */
static void wake_up_sem_queue_prepare(struct list_head *pt,
				struct sem_queue *q, int error)
{
	if (list_empty(pt)) {
		/*
		 * Hold preempt off so that we don't get preempted and have the
		 * wakee busy-wait until we're scheduled back on.
		 */
		preempt_disable();
	}
	q->status = IN_WAKEUP;
	q->pid = error;

	list_add_tail(&q->simple_list, pt);
}

/**
 * wake_up_sem_queue_do(pt) - do the actual wake-up
 * @pt: list of tasks to be woken up
 *
 * Do the actual wake-up.
 * The function is called without any locks held, thus the semaphore array
 * could be destroyed already and the tasks can disappear as soon as the
 * status is set to the actual return code.
 */
static void wake_up_sem_queue_do(struct list_head *pt)
{
	struct sem_queue *q, *t;
	int did_something;

	did_something = !list_empty(pt);
	list_for_each_entry_safe(q, t, pt, simple_list) {
		wake_up_process(q->sleeper);
		/* q can disappear immediately after writing q->status. */
		smp_wmb();
		q->status = q->pid;
	}
	if (did_something)
		preempt_enable();
}

static void unlink_queue(struct sem_array *sma, struct sem_queue *q)
{
	list_del(&q->list);
	if (q->nsops == 1)
		list_del(&q->simple_list);
	else
		sma->complex_count--;
}

/** check_restart(sma, q)
 * @sma: semaphore array
 * @q: the operation that just completed
 *
 * update_queue is O(N^2) when it restarts scanning the whole queue of
 * waiting operations. Therefore this function checks if the restart is
 * really necessary. It is called after a previously waiting operation
 * was completed.
 */
static int check_restart(struct sem_array *sma, struct sem_queue *q)
{
	struct sem *curr;
	struct sem_queue *h;

	/* if the operation didn't modify the array, then no restart */
	if (q->alter == 0)
		return 0;

	/* pending complex operations are too difficult to analyse */
	if (sma->complex_count)
		return 1;

	/* we were a sleeping complex operation. Too difficult */
	if (q->nsops > 1)
		return 1;

	curr = sma->sem_base + q->sops[0].sem_num;

	/* No-one waits on this queue */
	if (list_empty(&curr->sem_pending))
		return 0;

	/* the new semaphore value */
	if (curr->semval) {
		/* It is impossible that someone waits for the new value:
		 * - q is a previously sleeping simple operation that
		 *   altered the array. It must be a decrement, because
		 *   simple increments never sleep.
		 * - The value is not 0, thus wait-for-zero won't proceed.
		 * - If there are older (higher priority) decrements
		 *   in the queue, then they have observed the original
		 *   semval value and couldn't proceed. The operation
		 *   decremented to value - thus they won't proceed either.
		 */
		BUG_ON(q->sops[0].sem_op >= 0);
		return 0;
	}
	/*
	 * semval is 0. Check if there are wait-for-zero semops.
	 * They must be the first entries in the per-semaphore simple queue
	 */
	h = list_first_entry(&curr->sem_pending, struct sem_queue, simple_list);
	BUG_ON(h->nsops != 1);
	BUG_ON(h->sops[0].sem_num != q->sops[0].sem_num);

	/* Yes, there is a wait-for-zero semop. Restart */
	if (h->sops[0].sem_op == 0)
		return 1;

	/* Again - no-one is waiting for the new value. */
	return 0;
}


/**
 * update_queue(sma, semnum): Look for tasks that can be completed.
 * @sma: semaphore array.
 * @semnum: semaphore that was modified.
 * @pt: list head for the tasks that must be woken up.
 *
 * update_queue must be called after a semaphore in a semaphore array
 * was modified. If multiple semaphore were modified, then @semnum
 * must be set to -1.
 * The tasks that must be woken up are added to @pt. The return code
 * is stored in q->pid.
 * The function return 1 if at least one semop was completed successfully.
 */
static int update_queue(struct sem_array *sma, int semnum, struct list_head *pt)
{
	struct sem_queue *q;
	struct list_head *walk;
	struct list_head *pending_list;
	int offset;
	int semop_completed = 0;

	/* if there are complex operations around, then knowing the semaphore
	 * that was modified doesn't help us. Assume that multiple semaphores
	 * were modified.
	 */
	if (sma->complex_count)
		semnum = -1;

	if (semnum == -1) {
		pending_list = &sma->sem_pending;
		offset = offsetof(struct sem_queue, list);
	} else {
		pending_list = &sma->sem_base[semnum].sem_pending;
		offset = offsetof(struct sem_queue, simple_list);
	}

again:
	walk = pending_list->next;
	while (walk != pending_list) {
		int error, restart;

		q = (struct sem_queue *)((char *)walk - offset);
		walk = walk->next;

		/* If we are scanning the single sop, per-semaphore list of
		 * one semaphore and that semaphore is 0, then it is not
		 * necessary to scan the "alter" entries: simple increments
		 * that affect only one entry succeed immediately and cannot
		 * be in the  per semaphore pending queue, and decrements
		 * cannot be successful if the value is already 0.
		 */
		if (semnum != -1 && sma->sem_base[semnum].semval == 0 &&
				q->alter)
			break;

		error = try_atomic_semop(sma, q->sops, q->nsops,
					 q->undo, q->pid);

		/* Does q->sleeper still need to sleep? */
		if (error > 0)
			continue;

		unlink_queue(sma, q);

		if (error) {
			restart = 0;
		} else {
			semop_completed = 1;
			restart = check_restart(sma, q);
		}

		wake_up_sem_queue_prepare(pt, q, error);
		if (restart)
			goto again;
	}
	return semop_completed;
}

/**
 * do_smart_update(sma, sops, nsops, otime, pt) - optimized update_queue
 * @sma: semaphore array
 * @sops: operations that were performed
 * @nsops: number of operations
 * @otime: force setting otime
 * @pt: list head of the tasks that must be woken up.
 *
 * do_smart_update() does the required called to update_queue, based on the
 * actual changes that were performed on the semaphore array.
 * Note that the function does not do the actual wake-up: the caller is
 * responsible for calling wake_up_sem_queue_do(@pt).
 * It is safe to perform this call after dropping all locks.
 */
static void do_smart_update(struct sem_array *sma, struct sembuf *sops, int nsops,
			int otime, struct list_head *pt)
{
	int i;

	if (sma->complex_count || sops == NULL) {
		if (update_queue(sma, -1, pt))
			otime = 1;
		goto done;
	}

	for (i = 0; i < nsops; i++) {
		if (sops[i].sem_op > 0 ||
			(sops[i].sem_op < 0 &&
				sma->sem_base[sops[i].sem_num].semval == 0))
			if (update_queue(sma, sops[i].sem_num, pt))
				otime = 1;
	}
done:
	if (otime)
		sma->sem_otime = get_seconds();
}


/* The following counts are associated to each semaphore:
 *   semncnt        number of tasks waiting on semval being nonzero
 *   semzcnt        number of tasks waiting on semval being zero
 * This model assumes that a task waits on exactly one semaphore.
 * Since semaphore operations are to be performed atomically, tasks actually
 * wait on a whole sequence of semaphores simultaneously.
 * The counts we return here are a rough approximation, but still
 * warrant that semncnt+semzcnt>0 if the task is on the pending queue.
 */
static int count_semncnt (struct sem_array * sma, ushort semnum)
{
	int semncnt;
	struct sem_queue * q;

	semncnt = 0;
	list_for_each_entry(q, &sma->sem_pending, list) {
		struct sembuf * sops = q->sops;
		int nsops = q->nsops;
		int i;
		for (i = 0; i < nsops; i++)
			if (sops[i].sem_num == semnum
			    && (sops[i].sem_op < 0)
			    && !(sops[i].sem_flg & IPC_NOWAIT))
				semncnt++;
	}
	return semncnt;
}

static int count_semzcnt (struct sem_array * sma, ushort semnum)
{
	int semzcnt;
	struct sem_queue * q;

	semzcnt = 0;
	list_for_each_entry(q, &sma->sem_pending, list) {
		struct sembuf * sops = q->sops;
		int nsops = q->nsops;
		int i;
		for (i = 0; i < nsops; i++)
			if (sops[i].sem_num == semnum
			    && (sops[i].sem_op == 0)
			    && !(sops[i].sem_flg & IPC_NOWAIT))
				semzcnt++;
	}
	return semzcnt;
}

static void free_un(struct rcu_head *head)
{
	struct sem_undo *un = container_of(head, struct sem_undo, rcu);
	kfree(un);
}

/* Free a semaphore set. freeary() is called with sem_ids.rw_mutex locked
 * as a writer and the spinlock for this semaphore set hold. sem_ids.rw_mutex
 * remains locked on exit.
 */
static void freeary(struct ipc_namespace *ns, struct kern_ipc_perm *ipcp)
{
	struct sem_undo *un, *tu;
	struct sem_queue *q, *tq;
	struct sem_array *sma = container_of(ipcp, struct sem_array, sem_perm);
	struct list_head tasks;

	/* Free the existing undo structures for this semaphore set.  */
	assert_spin_locked(&sma->sem_perm.lock);
	list_for_each_entry_safe(un, tu, &sma->list_id, list_id) {
		list_del(&un->list_id);
		spin_lock(&un->ulp->lock);
		un->semid = -1;
		list_del_rcu(&un->list_proc);
		spin_unlock(&un->ulp->lock);
		call_rcu(&un->rcu, free_un);
	}

	/* Wake up all pending processes and let them fail with EIDRM. */
	INIT_LIST_HEAD(&tasks);
	list_for_each_entry_safe(q, tq, &sma->sem_pending, list) {
		unlink_queue(sma, q);
		wake_up_sem_queue_prepare(&tasks, q, -EIDRM);
	}

	/* Remove the semaphore set from the IDR */
	sem_rmid(ns, sma);
	sem_unlock(sma);

	wake_up_sem_queue_do(&tasks);
	ns->used_sems -= sma->sem_nsems;
	security_sem_free(sma);
	ipc_rcu_putref(sma);
}

static unsigned long copy_semid_to_user(void __user *buf, struct semid64_ds *in, int version)
{
	switch(version) {
	case IPC_64:
		return copy_to_user(buf, in, sizeof(*in));
	case IPC_OLD:
	    {
		struct semid_ds out;

		memset(&out, 0, sizeof(out));

		ipc64_perm_to_ipc_perm(&in->sem_perm, &out.sem_perm);

		out.sem_otime	= in->sem_otime;
		out.sem_ctime	= in->sem_ctime;
		out.sem_nsems	= in->sem_nsems;

		return copy_to_user(buf, &out, sizeof(out));
	    }
	default:
		return -EINVAL;
	}
}

static int semctl_nolock(struct ipc_namespace *ns, int semid,
			 int cmd, int version, union semun arg)
{
	int err;
	struct sem_array *sma;

	switch(cmd) {
	case IPC_INFO:
	case SEM_INFO:
	{
		struct seminfo seminfo;
		int max_id;

		err = security_sem_semctl(NULL, cmd);
		if (err)
			return err;
		
		memset(&seminfo,0,sizeof(seminfo));
		seminfo.semmni = ns->sc_semmni;
		seminfo.semmns = ns->sc_semmns;
		seminfo.semmsl = ns->sc_semmsl;
		seminfo.semopm = ns->sc_semopm;
		seminfo.semvmx = SEMVMX;
		seminfo.semmnu = SEMMNU;
		seminfo.semmap = SEMMAP;
		seminfo.semume = SEMUME;
		down_read(&sem_ids(ns).rw_mutex);
		if (cmd == SEM_INFO) {
			seminfo.semusz = sem_ids(ns).in_use;
			seminfo.semaem = ns->used_sems;
		} else {
			seminfo.semusz = SEMUSZ;
			seminfo.semaem = SEMAEM;
		}
		max_id = ipc_get_maxid(&sem_ids(ns));
		up_read(&sem_ids(ns).rw_mutex);
		if (copy_to_user (arg.__buf, &seminfo, sizeof(struct seminfo))) 
			return -EFAULT;
		return (max_id < 0) ? 0: max_id;
	}
	case IPC_STAT:
	case SEM_STAT:
	{
		struct semid64_ds tbuf;
		int id;

		if (cmd == SEM_STAT) {
			sma = sem_lock(ns, semid);
			if (IS_ERR(sma))
				return PTR_ERR(sma);
			id = sma->sem_perm.id;
		} else {
			sma = sem_lock_check(ns, semid);
			if (IS_ERR(sma))
				return PTR_ERR(sma);
			id = 0;
		}

		err = -EACCES;
		if (ipcperms (&sma->sem_perm, S_IRUGO))
			goto out_unlock;

		err = security_sem_semctl(sma, cmd);
		if (err)
			goto out_unlock;

		memset(&tbuf, 0, sizeof(tbuf));

		kernel_to_ipc64_perm(&sma->sem_perm, &tbuf.sem_perm);
		tbuf.sem_otime  = sma->sem_otime;
		tbuf.sem_ctime  = sma->sem_ctime;
		tbuf.sem_nsems  = sma->sem_nsems;
		sem_unlock(sma);
		if (copy_semid_to_user (arg.buf, &tbuf, version))
			return -EFAULT;
		return id;
	}
	default:
		return -EINVAL;
	}
out_unlock:
	sem_unlock(sma);
	return err;
}

static int semctl_main(struct ipc_namespace *ns, int semid, int semnum,
		int cmd, int version, union semun arg)
{
	struct sem_array *sma;
	struct sem* curr;
	int err;
	ushort fast_sem_io[SEMMSL_FAST];
	ushort* sem_io = fast_sem_io;
	int nsems;
	struct list_head tasks;

	sma = sem_lock_check(ns, semid);
	if (IS_ERR(sma))
		return PTR_ERR(sma);

	INIT_LIST_HEAD(&tasks);
	nsems = sma->sem_nsems;

	err = -EACCES;
	if (ipcperms (&sma->sem_perm, (cmd==SETVAL||cmd==SETALL)?S_IWUGO:S_IRUGO))
		goto out_unlock;

	err = security_sem_semctl(sma, cmd);
	if (err)
		goto out_unlock;

	err = -EACCES;
	switch (cmd) {
	case GETALL:
	{
		ushort __user *array = arg.array;
		int i;

		if(nsems > SEMMSL_FAST) {
			sem_getref_and_unlock(sma);

			sem_io = ipc_alloc(sizeof(ushort)*nsems);
			if(sem_io == NULL) {
				sem_putref(sma);
				return -ENOMEM;
			}

			sem_lock_and_putref(sma);
			if (sma->sem_perm.deleted) {
				sem_unlock(sma);
				err = -EIDRM;
				goto out_free;
			}
		}

		for (i = 0; i < sma->sem_nsems; i++)
			sem_io[i] = sma->sem_base[i].semval;
		sem_unlock(sma);
		err = 0;
		if(copy_to_user(array, sem_io, nsems*sizeof(ushort)))
			err = -EFAULT;
		goto out_free;
	}
	case SETALL:
	{
		int i;
		struct sem_undo *un;

		sem_getref_and_unlock(sma);

		if(nsems > SEMMSL_FAST) {
			sem_io = ipc_alloc(sizeof(ushort)*nsems);
			if(sem_io == NULL) {
				sem_putref(sma);
				return -ENOMEM;
			}
		}

		if (copy_from_user (sem_io, arg.array, nsems*sizeof(ushort))) {
			sem_putref(sma);
			err = -EFAULT;
			goto out_free;
		}

		for (i = 0; i < nsems; i++) {
			if (sem_io[i] > SEMVMX) {
				sem_putref(sma);
				err = -ERANGE;
				goto out_free;
			}
		}
		sem_lock_and_putref(sma);
		if (sma->sem_perm.deleted) {
			sem_unlock(sma);
			err = -EIDRM;
			goto out_free;
		}

		for (i = 0; i < nsems; i++)
			sma->sem_base[i].semval = sem_io[i];

		assert_spin_locked(&sma->sem_perm.lock);
		list_for_each_entry(un, &sma->list_id, list_id) {
			for (i = 0; i < nsems; i++)
				un->semadj[i] = 0;
		}
		sma->sem_ctime = get_seconds();
		/* maybe some queued-up processes were waiting for this */
		do_smart_update(sma, NULL, 0, 0, &tasks);
		err = 0;
		goto out_unlock;
	}
	/* GETVAL, GETPID, GETNCTN, GETZCNT, SETVAL: fall-through */
	}
	err = -EINVAL;
	if(semnum < 0 || semnum >= nsems)
		goto out_unlock;

	curr = &sma->sem_base[semnum];

	switch (cmd) {
	case GETVAL:
		err = curr->semval;
		goto out_unlock;
	case GETPID:
		err = curr->sempid;
		goto out_unlock;
	case GETNCNT:
		err = count_semncnt(sma,semnum);
		goto out_unlock;
	case GETZCNT:
		err = count_semzcnt(sma,semnum);
		goto out_unlock;
	case SETVAL:
	{
		int val = arg.val;
		struct sem_undo *un;

		err = -ERANGE;
		if (val > SEMVMX || val < 0)
			goto out_unlock;

		assert_spin_locked(&sma->sem_perm.lock);
		list_for_each_entry(un, &sma->list_id, list_id)
			un->semadj[semnum] = 0;

		curr->semval = val;
		curr->sempid = task_tgid_vnr(current);
		sma->sem_ctime = get_seconds();
		/* maybe some queued-up processes were waiting for this */
		do_smart_update(sma, NULL, 0, 0, &tasks);
		err = 0;
		goto out_unlock;
	}
	}
out_unlock:
	sem_unlock(sma);
	wake_up_sem_queue_do(&tasks);

out_free:
	if(sem_io != fast_sem_io)
		ipc_free(sem_io, sizeof(ushort)*nsems);
	return err;
}

static inline unsigned long
copy_semid_from_user(struct semid64_ds *out, void __user *buf, int version)
{
	switch(version) {
	case IPC_64:
		if (copy_from_user(out, buf, sizeof(*out)))
			return -EFAULT;
		return 0;
	case IPC_OLD:
	    {
		struct semid_ds tbuf_old;

		if(copy_from_user(&tbuf_old, buf, sizeof(tbuf_old)))
			return -EFAULT;

		out->sem_perm.uid	= tbuf_old.sem_perm.uid;
		out->sem_perm.gid	= tbuf_old.sem_perm.gid;
		out->sem_perm.mode	= tbuf_old.sem_perm.mode;

		return 0;
	    }
	default:
		return -EINVAL;
	}
}

/*
 * This function handles some semctl commands which require the rw_mutex
 * to be held in write mode.
 * NOTE: no locks must be held, the rw_mutex is taken inside this function.
 */
static int semctl_down(struct ipc_namespace *ns, int semid,
		       int cmd, int version, union semun arg)
{
	struct sem_array *sma;
	int err;
	struct semid64_ds semid64;
	struct kern_ipc_perm *ipcp;

	if(cmd == IPC_SET) {
		if (copy_semid_from_user(&semid64, arg.buf, version))
			return -EFAULT;
	}

	ipcp = ipcctl_pre_down(&sem_ids(ns), semid, cmd, &semid64.sem_perm, 0);
	if (IS_ERR(ipcp))
		return PTR_ERR(ipcp);

	sma = container_of(ipcp, struct sem_array, sem_perm);

	err = security_sem_semctl(sma, cmd);
	if (err)
		goto out_unlock;

	switch(cmd){
	case IPC_RMID:
		freeary(ns, ipcp);
		goto out_up;
	case IPC_SET:
		ipc_update_perm(&semid64.sem_perm, ipcp);
		sma->sem_ctime = get_seconds();
		break;
	default:
		err = -EINVAL;
	}

out_unlock:
	sem_unlock(sma);
out_up:
	up_write(&sem_ids(ns).rw_mutex);
	return err;
}

SYSCALL_DEFINE(semctl)(int semid, int semnum, int cmd, union semun arg)
{
	int err = -EINVAL;
	int version;
	struct ipc_namespace *ns;

	if (semid < 0)
		return -EINVAL;

	version = ipc_parse_version(&cmd);
	ns = current->nsproxy->ipc_ns;

	switch(cmd) {
	case IPC_INFO:
	case SEM_INFO:
	case IPC_STAT:
	case SEM_STAT:
		err = semctl_nolock(ns, semid, cmd, version, arg);
		return err;
	case GETALL:
	case GETVAL:
	case GETPID:
	case GETNCNT:
	case GETZCNT:
	case SETVAL:
	case SETALL:
		err = semctl_main(ns,semid,semnum,cmd,version,arg);
		return err;
	case IPC_RMID:
	case IPC_SET:
		err = semctl_down(ns, semid, cmd, version, arg);
		return err;
	default:
		return -EINVAL;
	}
}
#ifdef CONFIG_HAVE_SYSCALL_WRAPPERS
asmlinkage long SyS_semctl(int semid, int semnum, int cmd, union semun arg)
{
	return SYSC_semctl((int) semid, (int) semnum, (int) cmd, arg);
}
SYSCALL_ALIAS(sys_semctl, SyS_semctl);
#endif

/* If the task doesn't already have a undo_list, then allocate one
 * here.  We guarantee there is only one thread using this undo list,
 * and current is THE ONE
 *
 * If this allocation and assignment succeeds, but later
 * portions of this code fail, there is no need to free the sem_undo_list.
 * Just let it stay associated with the task, and it'll be freed later
 * at exit time.
 *
 * This can block, so callers must hold no locks.
 */
static inline int get_undo_list(struct sem_undo_list **undo_listp)
{
	struct sem_undo_list *undo_list;

	undo_list = current->sysvsem.undo_list;
	if (!undo_list) {
		undo_list = kzalloc(sizeof(*undo_list), GFP_KERNEL);
		if (undo_list == NULL)
			return -ENOMEM;
		spin_lock_init(&undo_list->lock);
		atomic_set(&undo_list->refcnt, 1);
		INIT_LIST_HEAD(&undo_list->list_proc);

		current->sysvsem.undo_list = undo_list;
	}
	*undo_listp = undo_list;
	return 0;
}

static struct sem_undo *__lookup_undo(struct sem_undo_list *ulp, int semid)
{
	struct sem_undo *un;

	list_for_each_entry_rcu(un, &ulp->list_proc, list_proc) {
		if (un->semid == semid)
			return un;
	}
	return NULL;
}

static struct sem_undo *lookup_undo(struct sem_undo_list *ulp, int semid)
{
	struct sem_undo *un;

  	assert_spin_locked(&ulp->lock);

	un = __lookup_undo(ulp, semid);
	if (un) {
		list_del_rcu(&un->list_proc);
		list_add_rcu(&un->list_proc, &ulp->list_proc);
	}
	return un;
}

/**
 * find_alloc_undo - Lookup (and if not present create) undo array
 * @ns: namespace
 * @semid: semaphore array id
 *
 * The function looks up (and if not present creates) the undo structure.
 * The size of the undo structure depends on the size of the semaphore
 * array, thus the alloc path is not that straightforward.
 * Lifetime-rules: sem_undo is rcu-protected, on success, the function
 * performs a rcu_read_lock().
 */
static struct sem_undo *find_alloc_undo(struct ipc_namespace *ns, int semid)
{
	struct sem_array *sma;
	struct sem_undo_list *ulp;
	struct sem_undo *un, *new;
	int nsems;
	int error;

	error = get_undo_list(&ulp);
	if (error)
		return ERR_PTR(error);

	rcu_read_lock();
	spin_lock(&ulp->lock);
	un = lookup_undo(ulp, semid);
	spin_unlock(&ulp->lock);
	if (likely(un!=NULL))
		goto out;
	rcu_read_unlock();

	/* no undo structure around - allocate one. */
	/* step 1: figure out the size of the semaphore array */
	sma = sem_lock_check(ns, semid);
	if (IS_ERR(sma))
		return ERR_CAST(sma);

	nsems = sma->sem_nsems;
	sem_getref_and_unlock(sma);

	/* step 2: allocate new undo structure */
	new = kzalloc(sizeof(struct sem_undo) + sizeof(short)*nsems, GFP_KERNEL);
	if (!new) {
		sem_putref(sma);
		return ERR_PTR(-ENOMEM);
	}

	/* step 3: Acquire the lock on semaphore array */
	sem_lock_and_putref(sma);
	if (sma->sem_perm.deleted) {
		sem_unlock(sma);
		kfree(new);
		un = ERR_PTR(-EIDRM);
		goto out;
	}
	spin_lock(&ulp->lock);

	/*
	 * step 4: check for races: did someone else allocate the undo struct?
	 */
	un = lookup_undo(ulp, semid);
	if (un) {
		kfree(new);
		goto success;
	}
	/* step 5: initialize & link new undo structure */
	new->semadj = (short *) &new[1];
	new->ulp = ulp;
	new->semid = semid;
	assert_spin_locked(&ulp->lock);
	list_add_rcu(&new->list_proc, &ulp->list_proc);
	assert_spin_locked(&sma->sem_perm.lock);
	list_add(&new->list_id, &sma->list_id);
	un = new;

success:
	spin_unlock(&ulp->lock);
	rcu_read_lock();
	sem_unlock(sma);
out:
	return un;
}


/**
 * get_queue_result - Retrieve the result code from sem_queue
 * @q: Pointer to queue structure
 *
 * Retrieve the return code from the pending queue. If IN_WAKEUP is found in
 * q->status, then we must loop until the value is replaced with the final
 * value: This may happen if a task is woken up by an unrelated event (e.g.
 * signal) and in parallel the task is woken up by another task because it got
 * the requested semaphores.
 *
 * The function can be called with or without holding the semaphore spinlock.
 */
static int get_queue_result(struct sem_queue *q)
{
	int error;

	error = q->status;
	while (unlikely(error == IN_WAKEUP)) {
		cpu_relax();
		error = q->status;
	}

	return error;
}


SYSCALL_DEFINE4(semtimedop, int, semid, struct sembuf __user *, tsops,
		unsigned, nsops, const struct timespec __user *, timeout)
{
	int error = -EINVAL;
	struct sem_array *sma;
	struct sembuf fast_sops[SEMOPM_FAST];
	struct sembuf* sops = fast_sops, *sop;
	struct sem_undo *un;
	int undos = 0, alter = 0, max;
	struct sem_queue queue;
	unsigned long jiffies_left = 0;
	struct ipc_namespace *ns;
	struct list_head tasks;

	ns = current->nsproxy->ipc_ns;

	if (nsops < 1 || semid < 0)
		return -EINVAL;
	if (nsops > ns->sc_semopm)
		return -E2BIG;
	if(nsops > SEMOPM_FAST) {
		sops = kmalloc(sizeof(*sops)*nsops,GFP_KERNEL);
		if(sops==NULL)
			return -ENOMEM;
	}
	if (copy_from_user (sops, tsops, nsops * sizeof(*tsops))) {
		error=-EFAULT;
		goto out_free;
	}
	if (timeout) {
		struct timespec _timeout;
		if (copy_from_user(&_timeout, timeout, sizeof(*timeout))) {
			error = -EFAULT;
			goto out_free;
		}
		if (_timeout.tv_sec < 0 || _timeout.tv_nsec < 0 ||
			_timeout.tv_nsec >= 1000000000L) {
			error = -EINVAL;
			goto out_free;
		}
		jiffies_left = timespec_to_jiffies(&_timeout);
	}
	max = 0;
	for (sop = sops; sop < sops + nsops; sop++) {
		if (sop->sem_num >= max)
			max = sop->sem_num;
		if (sop->sem_flg & SEM_UNDO)
			undos = 1;
		if (sop->sem_op != 0)
			alter = 1;
	}

	if (undos) {
		un = find_alloc_undo(ns, semid);
		if (IS_ERR(un)) {
			error = PTR_ERR(un);
			goto out_free;
		}
	} else
		un = NULL;

	INIT_LIST_HEAD(&tasks);

	sma = sem_lock_check(ns, semid);
	if (IS_ERR(sma)) {
		if (un)
			rcu_read_unlock();
		error = PTR_ERR(sma);
		goto out_free;
	}

	/*
	 * semid identifiers are not unique - find_alloc_undo may have
	 * allocated an undo structure, it was invalidated by an RMID
	 * and now a new array with received the same id. Check and fail.
	 * This case can be detected checking un->semid. The existance of
	 * "un" itself is guaranteed by rcu.
	 */
	error = -EIDRM;
	if (un) {
		if (un->semid == -1) {
			rcu_read_unlock();
			goto out_unlock_free;
		} else {
			/*
			 * rcu lock can be released, "un" cannot disappear:
			 * - sem_lock is acquired, thus IPC_RMID is
			 *   impossible.
			 * - exit_sem is impossible, it always operates on
			 *   current (or a dead task).
			 */

			rcu_read_unlock();
		}
	}

	error = -EFBIG;
	if (max >= sma->sem_nsems)
		goto out_unlock_free;

	error = -EACCES;
	if (ipcperms(&sma->sem_perm, alter ? S_IWUGO : S_IRUGO))
		goto out_unlock_free;

	error = security_sem_semop(sma, sops, nsops, alter);
	if (error)
		goto out_unlock_free;

	error = try_atomic_semop (sma, sops, nsops, un, task_tgid_vnr(current));
	if (error <= 0) {
		if (alter && error == 0)
			do_smart_update(sma, sops, nsops, 1, &tasks);

		goto out_unlock_free;
	}

	/* We need to sleep on this operation, so we put the current
	 * task into the pending queue and go to sleep.
	 */
		
	queue.sops = sops;
	queue.nsops = nsops;
	queue.undo = un;
	queue.pid = task_tgid_vnr(current);
	queue.alter = alter;
	if (alter)
		list_add_tail(&queue.list, &sma->sem_pending);
	else
		list_add(&queue.list, &sma->sem_pending);

	if (nsops == 1) {
		struct sem *curr;
		curr = &sma->sem_base[sops->sem_num];

		if (alter)
			list_add_tail(&queue.simple_list, &curr->sem_pending);
		else
			list_add(&queue.simple_list, &curr->sem_pending);
	} else {
		INIT_LIST_HEAD(&queue.simple_list);
		sma->complex_count++;
	}

	queue.status = -EINTR;
	queue.sleeper = current;
	current->state = TASK_INTERRUPTIBLE;
	sem_unlock(sma);

	if (timeout)
		jiffies_left = schedule_timeout(jiffies_left);
	else
		schedule();

	error = get_queue_result(&queue);

	if (error != -EINTR) {
		/* fast path: update_queue already obtained all requested
		 * resources.
		 * Perform a smp_mb(): User space could assume that semop()
		 * is a memory barrier: Without the mb(), the cpu could
		 * speculatively read in user space stale data that was
		 * overwritten by the previous owner of the semaphore.
		 */
		smp_mb();

		goto out_free;
	}

	sma = sem_lock(ns, semid);
	if (IS_ERR(sma)) {
		error = -EIDRM;
		goto out_free;
	}

	error = get_queue_result(&queue);

	/*
	 * If queue.status != -EINTR we are woken up by another process
	 */

	if (error != -EINTR) {
		goto out_unlock_free;
	}

	/*
	 * If an interrupt occurred we have to clean up the queue
	 */
	if (timeout && jiffies_left == 0)
		error = -EAGAIN;
	unlink_queue(sma, &queue);

out_unlock_free:
	sem_unlock(sma);

	wake_up_sem_queue_do(&tasks);
out_free:
	if(sops != fast_sops)
		kfree(sops);
	return error;
}

SYSCALL_DEFINE3(semop, int, semid, struct sembuf __user *, tsops,
		unsigned, nsops)
{
	return sys_semtimedop(semid, tsops, nsops, NULL);
}

/* If CLONE_SYSVSEM is set, establish sharing of SEM_UNDO state between
 * parent and child tasks.
 */

int copy_semundo(unsigned long clone_flags, struct task_struct *tsk)
{
	struct sem_undo_list *undo_list;
	int error;

	if (clone_flags & CLONE_SYSVSEM) {
		error = get_undo_list(&undo_list);
		if (error)
			return error;
		atomic_inc(&undo_list->refcnt);
		tsk->sysvsem.undo_list = undo_list;
	} else 
		tsk->sysvsem.undo_list = NULL;

	return 0;
}

/*
 * add semadj values to semaphores, free undo structures.
 * undo structures are not freed when semaphore arrays are destroyed
 * so some of them may be out of date.
 * IMPLEMENTATION NOTE: There is some confusion over whether the
 * set of adjustments that needs to be done should be done in an atomic
 * manner or not. That is, if we are attempting to decrement the semval
 * should we queue up and wait until we can do so legally?
 * The original implementation attempted to do this (queue and wait).
 * The current implementation does not do so. The POSIX standard
 * and SVID should be consulted to determine what behavior is mandated.
 */
void exit_sem(struct task_struct *tsk)
{
	struct sem_undo_list *ulp;

	ulp = tsk->sysvsem.undo_list;
	if (!ulp)
		return;
	tsk->sysvsem.undo_list = NULL;

	if (!atomic_dec_and_test(&ulp->refcnt))
		return;

	for (;;) {
		struct sem_array *sma;
		struct sem_undo *un;
		struct list_head tasks;
		int semid;
		int i;

		rcu_read_lock();
		un = list_entry_rcu(ulp->list_proc.next,
				    struct sem_undo, list_proc);
		if (&un->list_proc == &ulp->list_proc)
			semid = -1;
		 else
			semid = un->semid;
		rcu_read_unlock();

		if (semid == -1)
			break;

		sma = sem_lock_check(tsk->nsproxy->ipc_ns, un->semid);

		/* exit_sem raced with IPC_RMID, nothing to do */
		if (IS_ERR(sma))
			continue;

		un = __lookup_undo(ulp, semid);
		if (un == NULL) {
			/* exit_sem raced with IPC_RMID+semget() that created
			 * exactly the same semid. Nothing to do.
			 */
			sem_unlock(sma);
			continue;
		}

		/* remove un from the linked lists */
		assert_spin_locked(&sma->sem_perm.lock);
		list_del(&un->list_id);

		spin_lock(&ulp->lock);
		list_del_rcu(&un->list_proc);
		spin_unlock(&ulp->lock);

		/* perform adjustments registered in un */
		for (i = 0; i < sma->sem_nsems; i++) {
			struct sem * semaphore = &sma->sem_base[i];
			if (un->semadj[i]) {
				semaphore->semval += un->semadj[i];
				/*
				 * Range checks of the new semaphore value,
				 * not defined by sus:
				 * - Some unices ignore the undo entirely
				 *   (e.g. HP UX 11i 11.22, Tru64 V5.1)
				 * - some cap the value (e.g. FreeBSD caps
				 *   at 0, but doesn't enforce SEMVMX)
				 *
				 * Linux caps the semaphore value, both at 0
				 * and at SEMVMX.
				 *
				 * 	Manfred <manfred@colorfullife.com>
				 */
				if (semaphore->semval < 0)
					semaphore->semval = 0;
				if (semaphore->semval > SEMVMX)
					semaphore->semval = SEMVMX;
				semaphore->sempid = task_tgid_vnr(current);
			}
		}
		/* maybe some queued-up processes were waiting for this */
		INIT_LIST_HEAD(&tasks);
		do_smart_update(sma, NULL, 0, 1, &tasks);
		sem_unlock(sma);
		wake_up_sem_queue_do(&tasks);

		call_rcu(&un->rcu, free_un);
	}
	kfree(ulp);
}

#ifdef CONFIG_PROC_FS
static int sysvipc_sem_proc_show(struct seq_file *s, void *it)
{
	struct sem_array *sma = it;

	return seq_printf(s,
			  "%10d %10d  %4o %10u %5u %5u %5u %5u %10lu %10lu\n",
			  sma->sem_perm.key,
			  sma->sem_perm.id,
			  sma->sem_perm.mode,
			  sma->sem_nsems,
			  sma->sem_perm.uid,
			  sma->sem_perm.gid,
			  sma->sem_perm.cuid,
			  sma->sem_perm.cgid,
			  sma->sem_otime,
			  sma->sem_ctime);
}
#endif
