/*
 *  linux/arch/m32r/semaphore.c
 *    orig : i386 2.6.4
 *
 *  M32R semaphore implementation.
 *
 *	Copyright (c) 2002 - 2004 Hitoshi Yamamoto
 */

/*
 * i386 semaphore implementation.
 *
 * (C) Copyright 1999 Linus Torvalds
 *
 * Portions Copyright 1999 Red Hat, Inc.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 * rw semaphores implemented November 1999 by Benjamin LaHaise <bcrl@kvack.org>
 */
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/init.h>
#include <asm/semaphore.h>

/*
 * Semaphores are implemented using a two-way counter:
 * The "count" variable is decremented for each process
 * that tries to acquire the semaphore, while the "sleeping"
 * variable is a count of such acquires.
 *
 * Notably, the inline "up()" and "down()" functions can
 * efficiently test if they need to do any extra work (up
 * needs to do something only if count was negative before
 * the increment operation.
 *
 * "sleeping" and the contention routine ordering is protected
 * by the spinlock in the semaphore's waitqueue head.
 *
 * Note that these functions are only called when there is
 * contention on the lock, and as such all this is the
 * "non-critical" part of the whole semaphore business. The
 * critical part is the inline stuff in <asm/semaphore.h>
 * where we want to avoid any extra jumps and calls.
 */

/*
 * Logic:
 *  - only on a boundary condition do we need to care. When we go
 *    from a negative count to a non-negative, we wake people up.
 *  - when we go from a non-negative count to a negative do we
 *    (a) synchronize with the "sleeper" count and (b) make sure
 *    that we're on the wakeup list before we synchronize so that
 *    we cannot lose wakeup events.
 */

asmlinkage void __up(struct semaphore *sem)
{
	wake_up(&sem->wait);
}

asmlinkage void __sched __down(struct semaphore * sem)
{
	struct task_struct *tsk = current;
	DECLARE_WAITQUEUE(wait, tsk);
	unsigned long flags;

	tsk->state = TASK_UNINTERRUPTIBLE;
	spin_lock_irqsave(&sem->wait.lock, flags);
	add_wait_queue_exclusive_locked(&sem->wait, &wait);

	sem->sleepers++;
	for (;;) {
		int sleepers = sem->sleepers;

		/*
		 * Add "everybody else" into it. They aren't
		 * playing, because we own the spinlock in
		 * the wait_queue_head.
		 */
		if (!atomic_add_negative(sleepers - 1, &sem->count)) {
			sem->sleepers = 0;
			break;
		}
		sem->sleepers = 1;	/* us - see -1 above */
		spin_unlock_irqrestore(&sem->wait.lock, flags);

		schedule();

		spin_lock_irqsave(&sem->wait.lock, flags);
		tsk->state = TASK_UNINTERRUPTIBLE;
	}
	remove_wait_queue_locked(&sem->wait, &wait);
	wake_up_locked(&sem->wait);
	spin_unlock_irqrestore(&sem->wait.lock, flags);
	tsk->state = TASK_RUNNING;
}

asmlinkage int __sched __down_interruptible(struct semaphore * sem)
{
	int retval = 0;
	struct task_struct *tsk = current;
	DECLARE_WAITQUEUE(wait, tsk);
	unsigned long flags;

	tsk->state = TASK_INTERRUPTIBLE;
	spin_lock_irqsave(&sem->wait.lock, flags);
	add_wait_queue_exclusive_locked(&sem->wait, &wait);

	sem->sleepers++;
	for (;;) {
		int sleepers = sem->sleepers;

		/*
		 * With signals pending, this turns into
		 * the trylock failure case - we won't be
		 * sleeping, and we* can't get the lock as
		 * it has contention. Just correct the count
		 * and exit.
		 */
		if (signal_pending(current)) {
			retval = -EINTR;
			sem->sleepers = 0;
			atomic_add(sleepers, &sem->count);
			break;
		}

		/*
		 * Add "everybody else" into it. They aren't
		 * playing, because we own the spinlock in
		 * wait_queue_head. The "-1" is because we're
		 * still hoping to get the semaphore.
		 */
		if (!atomic_add_negative(sleepers - 1, &sem->count)) {
			sem->sleepers = 0;
			break;
		}
		sem->sleepers = 1;	/* us - see -1 above */
		spin_unlock_irqrestore(&sem->wait.lock, flags);

		schedule();

		spin_lock_irqsave(&sem->wait.lock, flags);
		tsk->state = TASK_INTERRUPTIBLE;
	}
	remove_wait_queue_locked(&sem->wait, &wait);
	wake_up_locked(&sem->wait);
	spin_unlock_irqrestore(&sem->wait.lock, flags);

	tsk->state = TASK_RUNNING;
	return retval;
}

/*
 * Trylock failed - make sure we correct for
 * having decremented the count.
 *
 * We could have done the trylock with a
 * single "cmpxchg" without failure cases,
 * but then it wouldn't work on a 386.
 */
asmlinkage int __down_trylock(struct semaphore * sem)
{
	int sleepers;
	unsigned long flags;

	spin_lock_irqsave(&sem->wait.lock, flags);
	sleepers = sem->sleepers + 1;
	sem->sleepers = 0;

	/*
	 * Add "everybody else" and us into it. They aren't
	 * playing, because we own the spinlock in the
	 * wait_queue_head.
	 */
	if (!atomic_add_negative(sleepers, &sem->count)) {
		wake_up_locked(&sem->wait);
	}

	spin_unlock_irqrestore(&sem->wait.lock, flags);
	return 1;
}
