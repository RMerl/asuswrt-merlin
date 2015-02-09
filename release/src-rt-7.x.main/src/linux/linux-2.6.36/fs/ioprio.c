/*
 * fs/ioprio.c
 *
 * Copyright (C) 2004 Jens Axboe <axboe@kernel.dk>
 *
 * Helper functions for setting/querying io priorities of processes. The
 * system calls closely mimmick getpriority/setpriority, see the man page for
 * those. The prio argument is a composite of prio class and prio data, where
 * the data argument has meaning within that class. The standard scheduling
 * classes have 8 distinct prio levels, with 0 being the highest prio and 7
 * being the lowest.
 *
 * IOW, setting BE scheduling class with prio 2 is done ala:
 *
 * unsigned int prio = (IOPRIO_CLASS_BE << IOPRIO_CLASS_SHIFT) | 2;
 *
 * ioprio_set(PRIO_PROCESS, pid, prio);
 *
 * See also Documentation/block/ioprio.txt
 *
 */
#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/ioprio.h>
#include <linux/blkdev.h>
#include <linux/capability.h>
#include <linux/syscalls.h>
#include <linux/security.h>
#include <linux/pid_namespace.h>

int set_task_ioprio(struct task_struct *task, int ioprio)
{
	int err;
	struct io_context *ioc;
	const struct cred *cred = current_cred(), *tcred;

	rcu_read_lock();
	tcred = __task_cred(task);
	if (tcred->uid != cred->euid &&
	    tcred->uid != cred->uid && !capable(CAP_SYS_NICE)) {
		rcu_read_unlock();
		return -EPERM;
	}
	rcu_read_unlock();

	err = security_task_setioprio(task, ioprio);
	if (err)
		return err;

	task_lock(task);
	do {
		ioc = task->io_context;
		/* see wmb() in current_io_context() */
		smp_read_barrier_depends();
		if (ioc)
			break;

		ioc = alloc_io_context(GFP_ATOMIC, -1);
		if (!ioc) {
			err = -ENOMEM;
			break;
		}
		task->io_context = ioc;
	} while (1);

	if (!err) {
		ioc->ioprio = ioprio;
		ioc->ioprio_changed = 1;
	}

	task_unlock(task);
	return err;
}
EXPORT_SYMBOL_GPL(set_task_ioprio);

SYSCALL_DEFINE3(ioprio_set, int, which, int, who, int, ioprio)
{
	int class = IOPRIO_PRIO_CLASS(ioprio);
	int data = IOPRIO_PRIO_DATA(ioprio);
	struct task_struct *p, *g;
	struct user_struct *user;
	struct pid *pgrp;
	int ret;

	switch (class) {
		case IOPRIO_CLASS_RT:
			if (!capable(CAP_SYS_ADMIN))
				return -EPERM;
			/* fall through, rt has prio field too */
		case IOPRIO_CLASS_BE:
			if (data >= IOPRIO_BE_NR || data < 0)
				return -EINVAL;

			break;
		case IOPRIO_CLASS_IDLE:
			break;
		case IOPRIO_CLASS_NONE:
			if (data)
				return -EINVAL;
			break;
		default:
			return -EINVAL;
	}

	ret = -ESRCH;
	/*
	 * We want IOPRIO_WHO_PGRP/IOPRIO_WHO_USER to be "atomic",
	 * so we can't use rcu_read_lock(). See re-copy of ->ioprio
	 * in copy_process().
	 */
	read_lock(&tasklist_lock);
	switch (which) {
		case IOPRIO_WHO_PROCESS:
			if (!who)
				p = current;
			else
				p = find_task_by_vpid(who);
			if (p)
				ret = set_task_ioprio(p, ioprio);
			break;
		case IOPRIO_WHO_PGRP:
			if (!who)
				pgrp = task_pgrp(current);
			else
				pgrp = find_vpid(who);
			do_each_pid_thread(pgrp, PIDTYPE_PGID, p) {
				ret = set_task_ioprio(p, ioprio);
				if (ret)
					break;
			} while_each_pid_thread(pgrp, PIDTYPE_PGID, p);
			break;
		case IOPRIO_WHO_USER:
			if (!who)
				user = current_user();
			else
				user = find_user(who);

			if (!user)
				break;

			do_each_thread(g, p) {
				if (__task_cred(p)->uid != who)
					continue;
				ret = set_task_ioprio(p, ioprio);
				if (ret)
					goto free_uid;
			} while_each_thread(g, p);
free_uid:
			if (who)
				free_uid(user);
			break;
		default:
			ret = -EINVAL;
	}

	read_unlock(&tasklist_lock);
	return ret;
}

static int get_task_ioprio(struct task_struct *p)
{
	int ret;

	ret = security_task_getioprio(p);
	if (ret)
		goto out;
	ret = IOPRIO_PRIO_VALUE(IOPRIO_CLASS_NONE, IOPRIO_NORM);
	if (p->io_context)
		ret = p->io_context->ioprio;
out:
	return ret;
}

int ioprio_best(unsigned short aprio, unsigned short bprio)
{
	unsigned short aclass = IOPRIO_PRIO_CLASS(aprio);
	unsigned short bclass = IOPRIO_PRIO_CLASS(bprio);

	if (aclass == IOPRIO_CLASS_NONE)
		aclass = IOPRIO_CLASS_BE;
	if (bclass == IOPRIO_CLASS_NONE)
		bclass = IOPRIO_CLASS_BE;

	if (aclass == bclass)
		return min(aprio, bprio);
	if (aclass > bclass)
		return bprio;
	else
		return aprio;
}

SYSCALL_DEFINE2(ioprio_get, int, which, int, who)
{
	struct task_struct *g, *p;
	struct user_struct *user;
	struct pid *pgrp;
	int ret = -ESRCH;
	int tmpio;

	read_lock(&tasklist_lock);
	switch (which) {
		case IOPRIO_WHO_PROCESS:
			if (!who)
				p = current;
			else
				p = find_task_by_vpid(who);
			if (p)
				ret = get_task_ioprio(p);
			break;
		case IOPRIO_WHO_PGRP:
			if (!who)
				pgrp = task_pgrp(current);
			else
				pgrp = find_vpid(who);
			do_each_pid_thread(pgrp, PIDTYPE_PGID, p) {
				tmpio = get_task_ioprio(p);
				if (tmpio < 0)
					continue;
				if (ret == -ESRCH)
					ret = tmpio;
				else
					ret = ioprio_best(ret, tmpio);
			} while_each_pid_thread(pgrp, PIDTYPE_PGID, p);
			break;
		case IOPRIO_WHO_USER:
			if (!who)
				user = current_user();
			else
				user = find_user(who);

			if (!user)
				break;

			do_each_thread(g, p) {
				if (__task_cred(p)->uid != user->uid)
					continue;
				tmpio = get_task_ioprio(p);
				if (tmpio < 0)
					continue;
				if (ret == -ESRCH)
					ret = tmpio;
				else
					ret = ioprio_best(ret, tmpio);
			} while_each_thread(g, p);

			if (who)
				free_uid(user);
			break;
		default:
			ret = -EINVAL;
	}

	read_unlock(&tasklist_lock);
	return ret;
}
