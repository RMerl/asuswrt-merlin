/* rwsem.c: R/W semaphores: contention handling functions
 *
 * Written by David Howells (dhowells@redhat.com).
 * Derived from arch/i386/kernel/semaphore.c
 */
#include <linux/rwsem.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/module.h>

/*
 * Initialize an rwsem:
 */
void __init_rwsem(struct rw_semaphore *sem, const char *name,
		  struct lock_class_key *key)
{
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	/*
	 * Make sure we are not reinitializing a held semaphore:
	 */
	debug_check_no_locks_freed((void *)sem, sizeof(*sem));
	lockdep_init_map(&sem->dep_map, name, key, 0);
#endif
	sem->count = RWSEM_UNLOCKED_VALUE;
	spin_lock_init(&sem->wait_lock);
	INIT_LIST_HEAD(&sem->wait_list);
}

EXPORT_SYMBOL(__init_rwsem);

struct rwsem_waiter {
	struct list_head list;
	struct task_struct *task;
	unsigned int flags;
#define RWSEM_WAITING_FOR_READ	0x00000001
#define RWSEM_WAITING_FOR_WRITE	0x00000002
};

/* Wake types for __rwsem_do_wake().  Note that RWSEM_WAKE_NO_ACTIVE and
 * RWSEM_WAKE_READ_OWNED imply that the spinlock must have been kept held
 * since the rwsem value was observed.
 */
#define RWSEM_WAKE_ANY        0 /* Wake whatever's at head of wait list */
#define RWSEM_WAKE_NO_ACTIVE  1 /* rwsem was observed with no active thread */
#define RWSEM_WAKE_READ_OWNED 2 /* rwsem was observed to be read owned */

/*
 * handle the lock release when processes blocked on it that can now run
 * - if we come here from up_xxxx(), then:
 *   - the 'active part' of count (&0x0000ffff) reached 0 (but may have changed)
 *   - the 'waiting part' of count (&0xffff0000) is -ve (and will still be so)
 * - there must be someone on the queue
 * - the spinlock must be held by the caller
 * - woken process blocks are discarded from the list after having task zeroed
 * - writers are only woken if downgrading is false
 */
static struct rw_semaphore *
__rwsem_do_wake(struct rw_semaphore *sem, int wake_type)
{
	struct rwsem_waiter *waiter;
	struct task_struct *tsk;
	struct list_head *next;
	signed long oldcount, woken, loop, adjustment;

	waiter = list_entry(sem->wait_list.next, struct rwsem_waiter, list);
	if (!(waiter->flags & RWSEM_WAITING_FOR_WRITE))
		goto readers_only;

	if (wake_type == RWSEM_WAKE_READ_OWNED)
		/* Another active reader was observed, so wakeup is not
		 * likely to succeed. Save the atomic op.
		 */
		goto out;

	/* There's a writer at the front of the queue - try to grant it the
	 * write lock.  However, we only wake this writer if we can transition
	 * the active part of the count from 0 -> 1
	 */
	adjustment = RWSEM_ACTIVE_WRITE_BIAS;
	if (waiter->list.next == &sem->wait_list)
		adjustment -= RWSEM_WAITING_BIAS;

 try_again_write:
	oldcount = rwsem_atomic_update(adjustment, sem) - adjustment;
	if (oldcount & RWSEM_ACTIVE_MASK)
		/* Someone grabbed the sem already */
		goto undo_write;

	/* We must be careful not to touch 'waiter' after we set ->task = NULL.
	 * It is an allocated on the waiter's stack and may become invalid at
	 * any time after that point (due to a wakeup from another source).
	 */
	list_del(&waiter->list);
	tsk = waiter->task;
	smp_mb();
	waiter->task = NULL;
	wake_up_process(tsk);
	put_task_struct(tsk);
	goto out;

 readers_only:
	/* If we come here from up_xxxx(), another thread might have reached
	 * rwsem_down_failed_common() before we acquired the spinlock and
	 * woken up a waiter, making it now active.  We prefer to check for
	 * this first in order to not spend too much time with the spinlock
	 * held if we're not going to be able to wake up readers in the end.
	 *
	 * Note that we do not need to update the rwsem count: any writer
	 * trying to acquire rwsem will run rwsem_down_write_failed() due
	 * to the waiting threads and block trying to acquire the spinlock.
	 *
	 * We use a dummy atomic update in order to acquire the cache line
	 * exclusively since we expect to succeed and run the final rwsem
	 * count adjustment pretty soon.
	 */
	if (wake_type == RWSEM_WAKE_ANY &&
	    rwsem_atomic_update(0, sem) < RWSEM_WAITING_BIAS)
		/* Someone grabbed the sem for write already */
		goto out;

	/* Grant an infinite number of read locks to the readers at the front
	 * of the queue.  Note we increment the 'active part' of the count by
	 * the number of readers before waking any processes up.
	 */
	woken = 0;
	do {
		woken++;

		if (waiter->list.next == &sem->wait_list)
			break;

		waiter = list_entry(waiter->list.next,
					struct rwsem_waiter, list);

	} while (waiter->flags & RWSEM_WAITING_FOR_READ);

	adjustment = woken * RWSEM_ACTIVE_READ_BIAS;
	if (waiter->flags & RWSEM_WAITING_FOR_READ)
		/* hit end of list above */
		adjustment -= RWSEM_WAITING_BIAS;

	rwsem_atomic_add(adjustment, sem);

	next = sem->wait_list.next;
	for (loop = woken; loop > 0; loop--) {
		waiter = list_entry(next, struct rwsem_waiter, list);
		next = waiter->list.next;
		tsk = waiter->task;
		smp_mb();
		waiter->task = NULL;
		wake_up_process(tsk);
		put_task_struct(tsk);
	}

	sem->wait_list.next = next;
	next->prev = &sem->wait_list;

 out:
	return sem;

	/* undo the change to the active count, but check for a transition
	 * 1->0 */
 undo_write:
	if (rwsem_atomic_update(-adjustment, sem) & RWSEM_ACTIVE_MASK)
		goto out;
	goto try_again_write;
}

/*
 * wait for a lock to be granted
 */
static struct rw_semaphore __sched *
rwsem_down_failed_common(struct rw_semaphore *sem,
			 unsigned int flags, signed long adjustment)
{
	struct rwsem_waiter waiter;
	struct task_struct *tsk = current;
	signed long count;

	set_task_state(tsk, TASK_UNINTERRUPTIBLE);

	/* set up my own style of waitqueue */
	spin_lock_irq(&sem->wait_lock);
	waiter.task = tsk;
	waiter.flags = flags;
	get_task_struct(tsk);

	if (list_empty(&sem->wait_list))
		adjustment += RWSEM_WAITING_BIAS;
	list_add_tail(&waiter.list, &sem->wait_list);

	/* we're now waiting on the lock, but no longer actively locking */
	count = rwsem_atomic_update(adjustment, sem);

	/* If there are no active locks, wake the front queued process(es) up.
	 *
	 * Alternatively, if we're called from a failed down_write(), there
	 * were already threads queued before us and there are no active
	 * writers, the lock must be read owned; so we try to wake any read
	 * locks that were queued ahead of us. */
	if (count == RWSEM_WAITING_BIAS)
		sem = __rwsem_do_wake(sem, RWSEM_WAKE_NO_ACTIVE);
	else if (count > RWSEM_WAITING_BIAS &&
		 adjustment == -RWSEM_ACTIVE_WRITE_BIAS)
		sem = __rwsem_do_wake(sem, RWSEM_WAKE_READ_OWNED);

	spin_unlock_irq(&sem->wait_lock);

	/* wait to be given the lock */
	for (;;) {
		if (!waiter.task)
			break;
		schedule();
		set_task_state(tsk, TASK_UNINTERRUPTIBLE);
	}

	tsk->state = TASK_RUNNING;

	return sem;
}

/*
 * wait for the read lock to be granted
 */
asmregparm struct rw_semaphore __sched *
rwsem_down_read_failed(struct rw_semaphore *sem)
{
	return rwsem_down_failed_common(sem, RWSEM_WAITING_FOR_READ,
					-RWSEM_ACTIVE_READ_BIAS);
}

/*
 * wait for the write lock to be granted
 */
asmregparm struct rw_semaphore __sched *
rwsem_down_write_failed(struct rw_semaphore *sem)
{
	return rwsem_down_failed_common(sem, RWSEM_WAITING_FOR_WRITE,
					-RWSEM_ACTIVE_WRITE_BIAS);
}

/*
 * handle waking up a waiter on the semaphore
 * - up_read/up_write has decremented the active part of count if we come here
 */
asmregparm struct rw_semaphore *rwsem_wake(struct rw_semaphore *sem)
{
	unsigned long flags;

	spin_lock_irqsave(&sem->wait_lock, flags);

	/* do nothing if list empty */
	if (!list_empty(&sem->wait_list))
		sem = __rwsem_do_wake(sem, RWSEM_WAKE_ANY);

	spin_unlock_irqrestore(&sem->wait_lock, flags);

	return sem;
}

/*
 * downgrade a write lock into a read lock
 * - caller incremented waiting part of count and discovered it still negative
 * - just wake up any readers at the front of the queue
 */
asmregparm struct rw_semaphore *rwsem_downgrade_wake(struct rw_semaphore *sem)
{
	unsigned long flags;

	spin_lock_irqsave(&sem->wait_lock, flags);

	/* do nothing if list empty */
	if (!list_empty(&sem->wait_list))
		sem = __rwsem_do_wake(sem, RWSEM_WAKE_READ_OWNED);

	spin_unlock_irqrestore(&sem->wait_lock, flags);

	return sem;
}

EXPORT_SYMBOL(rwsem_down_read_failed);
EXPORT_SYMBOL(rwsem_down_write_failed);
EXPORT_SYMBOL(rwsem_wake);
EXPORT_SYMBOL(rwsem_downgrade_wake);
