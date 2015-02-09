/*
 * linux/kernel/posix-timers.c
 *
 *
 * 2002-10-15  Posix Clocks & timers
 *                           by George Anzinger george@mvista.com
 *
 *			     Copyright (C) 2002 2003 by MontaVista Software.
 *
 * 2004-06-01  Fix CLOCK_REALTIME clock/timer TIMER_ABSTIME bug.
 *			     Copyright (C) 2004 Boris Hu
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * MontaVista Software | 1237 East Arques Avenue | Sunnyvale | CA 94085 | USA
 */

/* These are all the functions necessary to implement
 * POSIX clocks & timers
 */
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/mutex.h>

#include <asm/uaccess.h>
#include <linux/list.h>
#include <linux/init.h>
#include <linux/compiler.h>
#include <linux/idr.h>
#include <linux/posix-timers.h>
#include <linux/syscalls.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/module.h>

/*
 * Management arrays for POSIX timers.	 Timers are kept in slab memory
 * Timer ids are allocated by an external routine that keeps track of the
 * id and the timer.  The external interface is:
 *
 * void *idr_find(struct idr *idp, int id);           to find timer_id <id>
 * int idr_get_new(struct idr *idp, void *ptr);       to get a new id and
 *                                                    related it to <ptr>
 * void idr_remove(struct idr *idp, int id);          to release <id>
 * void idr_init(struct idr *idp);                    to initialize <idp>
 *                                                    which we supply.
 * The idr_get_new *may* call slab for more memory so it must not be
 * called under a spin lock.  Likewise idr_remore may release memory
 * (but it may be ok to do this under a lock...).
 * idr_find is just a memory look up and is quite fast.  A -1 return
 * indicates that the requested id does not exist.
 */

/*
 * Lets keep our timers in a slab cache :-)
 */
static struct kmem_cache *posix_timers_cache;
static struct idr posix_timers_id;
static DEFINE_SPINLOCK(idr_lock);

/*
 * we assume that the new SIGEV_THREAD_ID shares no bits with the other
 * SIGEV values.  Here we put out an error if this assumption fails.
 */
#if SIGEV_THREAD_ID != (SIGEV_THREAD_ID & ~(SIGEV_SIGNAL | SIGEV_NONE | SIGEV_THREAD))
#error "SIGEV_THREAD_ID must not share bit with other SIGEV values!"
#endif


/*
 * The timer ID is turned into a timer address by idr_find().
 * Verifying a valid ID consists of:
 *
 * a) checking that idr_find() returns other than -1.
 * b) checking that the timer id matches the one in the timer itself.
 * c) that the timer owner is in the callers thread group.
 */

/*
 * CLOCKs: The POSIX standard calls for a couple of clocks and allows us
 *	    to implement others.  This structure defines the various
 *	    clocks and allows the possibility of adding others.	 We
 *	    provide an interface to add clocks to the table and expect
 *	    the "arch" code to add at least one clock that is high
 *	    resolution.	 Here we define the standard CLOCK_REALTIME as a
 *	    1/HZ resolution clock.
 *
 * RESOLUTION: Clock resolution is used to round up timer and interval
 *	    times, NOT to report clock times, which are reported with as
 *	    much resolution as the system can muster.  In some cases this
 *	    resolution may depend on the underlying clock hardware and
 *	    may not be quantifiable until run time, and only then is the
 *	    necessary code is written.	The standard says we should say
 *	    something about this issue in the documentation...
 *
 * FUNCTIONS: The CLOCKs structure defines possible functions to handle
 *	    various clock functions.  For clocks that use the standard
 *	    system timer code these entries should be NULL.  This will
 *	    allow dispatch without the overhead of indirect function
 *	    calls.  CLOCKS that depend on other sources (e.g. WWV or GPS)
 *	    must supply functions here, even if the function just returns
 *	    ENOSYS.  The standard POSIX timer management code assumes the
 *	    following: 1.) The k_itimer struct (sched.h) is used for the
 *	    timer.  2.) The list, it_lock, it_clock, it_id and it_pid
 *	    fields are not modified by timer code.
 *
 *          At this time all functions EXCEPT clock_nanosleep can be
 *          redirected by the CLOCKS structure.  Clock_nanosleep is in
 *          there, but the code ignores it.
 *
 * Permissions: It is assumed that the clock_settime() function defined
 *	    for each clock will take care of permission checks.	 Some
 *	    clocks may be set able by any user (i.e. local process
 *	    clocks) others not.	 Currently the only set able clock we
 *	    have is CLOCK_REALTIME and its high res counter part, both of
 *	    which we beg off on and pass to do_sys_settimeofday().
 */

static struct k_clock posix_clocks[MAX_CLOCKS];

/*
 * These ones are defined below.
 */
static int common_nsleep(const clockid_t, int flags, struct timespec *t,
			 struct timespec __user *rmtp);
static void common_timer_get(struct k_itimer *, struct itimerspec *);
static int common_timer_set(struct k_itimer *, int,
			    struct itimerspec *, struct itimerspec *);
static int common_timer_del(struct k_itimer *timer);

static enum hrtimer_restart posix_timer_fn(struct hrtimer *data);

static struct k_itimer *lock_timer(timer_t timer_id, unsigned long *flags);

static inline void unlock_timer(struct k_itimer *timr, unsigned long flags)
{
	spin_unlock_irqrestore(&timr->it_lock, flags);
}

/*
 * Call the k_clock hook function if non-null, or the default function.
 */
#define CLOCK_DISPATCH(clock, call, arglist) \
 	((clock) < 0 ? posix_cpu_##call arglist : \
 	 (posix_clocks[clock].call != NULL \
 	  ? (*posix_clocks[clock].call) arglist : common_##call arglist))

/*
 * Default clock hook functions when the struct k_clock passed
 * to register_posix_clock leaves a function pointer null.
 *
 * The function common_CALL is the default implementation for
 * the function pointer CALL in struct k_clock.
 */

static inline int common_clock_getres(const clockid_t which_clock,
				      struct timespec *tp)
{
	tp->tv_sec = 0;
	tp->tv_nsec = posix_clocks[which_clock].res;
	return 0;
}

/*
 * Get real time for posix timers
 */
static int common_clock_get(clockid_t which_clock, struct timespec *tp)
{
	ktime_get_real_ts(tp);
	return 0;
}

static inline int common_clock_set(const clockid_t which_clock,
				   struct timespec *tp)
{
	return do_sys_settimeofday(tp, NULL);
}

static int common_timer_create(struct k_itimer *new_timer)
{
	hrtimer_init(&new_timer->it.real.timer, new_timer->it_clock, 0);
	return 0;
}

static int no_timer_create(struct k_itimer *new_timer)
{
	return -EOPNOTSUPP;
}

static int no_nsleep(const clockid_t which_clock, int flags,
		     struct timespec *tsave, struct timespec __user *rmtp)
{
	return -EOPNOTSUPP;
}

/*
 * Return nonzero if we know a priori this clockid_t value is bogus.
 */
static inline int invalid_clockid(const clockid_t which_clock)
{
	if (which_clock < 0)	/* CPU clock, posix_cpu_* will check it */
		return 0;
	if ((unsigned) which_clock >= MAX_CLOCKS)
		return 1;
	if (posix_clocks[which_clock].clock_getres != NULL)
		return 0;
	if (posix_clocks[which_clock].res != 0)
		return 0;
	return 1;
}

/*
 * Get monotonic time for posix timers
 */
static int posix_ktime_get_ts(clockid_t which_clock, struct timespec *tp)
{
	ktime_get_ts(tp);
	return 0;
}

/*
 * Get monotonic time for posix timers
 */
static int posix_get_monotonic_raw(clockid_t which_clock, struct timespec *tp)
{
	getrawmonotonic(tp);
	return 0;
}


static int posix_get_realtime_coarse(clockid_t which_clock, struct timespec *tp)
{
	*tp = current_kernel_time();
	return 0;
}

static int posix_get_monotonic_coarse(clockid_t which_clock,
						struct timespec *tp)
{
	*tp = get_monotonic_coarse();
	return 0;
}

static int posix_get_coarse_res(const clockid_t which_clock, struct timespec *tp)
{
	*tp = ktime_to_timespec(KTIME_LOW_RES);
	return 0;
}
/*
 * Initialize everything, well, just everything in Posix clocks/timers ;)
 */
static __init int init_posix_timers(void)
{
	struct k_clock clock_realtime = {
		.clock_getres = hrtimer_get_res,
	};
	struct k_clock clock_monotonic = {
		.clock_getres = hrtimer_get_res,
		.clock_get = posix_ktime_get_ts,
		.clock_set = do_posix_clock_nosettime,
	};
	struct k_clock clock_monotonic_raw = {
		.clock_getres = hrtimer_get_res,
		.clock_get = posix_get_monotonic_raw,
		.clock_set = do_posix_clock_nosettime,
		.timer_create = no_timer_create,
		.nsleep = no_nsleep,
	};
	struct k_clock clock_realtime_coarse = {
		.clock_getres = posix_get_coarse_res,
		.clock_get = posix_get_realtime_coarse,
		.clock_set = do_posix_clock_nosettime,
		.timer_create = no_timer_create,
		.nsleep = no_nsleep,
	};
	struct k_clock clock_monotonic_coarse = {
		.clock_getres = posix_get_coarse_res,
		.clock_get = posix_get_monotonic_coarse,
		.clock_set = do_posix_clock_nosettime,
		.timer_create = no_timer_create,
		.nsleep = no_nsleep,
	};

	register_posix_clock(CLOCK_REALTIME, &clock_realtime);
	register_posix_clock(CLOCK_MONOTONIC, &clock_monotonic);
	register_posix_clock(CLOCK_MONOTONIC_RAW, &clock_monotonic_raw);
	register_posix_clock(CLOCK_REALTIME_COARSE, &clock_realtime_coarse);
	register_posix_clock(CLOCK_MONOTONIC_COARSE, &clock_monotonic_coarse);

	posix_timers_cache = kmem_cache_create("posix_timers_cache",
					sizeof (struct k_itimer), 0, SLAB_PANIC,
					NULL);
	idr_init(&posix_timers_id);
	return 0;
}

__initcall(init_posix_timers);

static void schedule_next_timer(struct k_itimer *timr)
{
	struct hrtimer *timer = &timr->it.real.timer;

	if (timr->it.real.interval.tv64 == 0)
		return;

	timr->it_overrun += (unsigned int) hrtimer_forward(timer,
						timer->base->get_time(),
						timr->it.real.interval);

	timr->it_overrun_last = timr->it_overrun;
	timr->it_overrun = -1;
	++timr->it_requeue_pending;
	hrtimer_restart(timer);
}

/*
 * This function is exported for use by the signal deliver code.  It is
 * called just prior to the info block being released and passes that
 * block to us.  It's function is to update the overrun entry AND to
 * restart the timer.  It should only be called if the timer is to be
 * restarted (i.e. we have flagged this in the sys_private entry of the
 * info block).
 *
 * To protect aginst the timer going away while the interrupt is queued,
 * we require that the it_requeue_pending flag be set.
 */
void do_schedule_next_timer(struct siginfo *info)
{
	struct k_itimer *timr;
	unsigned long flags;

	timr = lock_timer(info->si_tid, &flags);

	if (timr && timr->it_requeue_pending == info->si_sys_private) {
		if (timr->it_clock < 0)
			posix_cpu_timer_schedule(timr);
		else
			schedule_next_timer(timr);

		info->si_overrun += timr->it_overrun_last;
	}

	if (timr)
		unlock_timer(timr, flags);
}

int posix_timer_event(struct k_itimer *timr, int si_private)
{
	struct task_struct *task;
	int shared, ret = -1;
	timr->sigq->info.si_sys_private = si_private;

	rcu_read_lock();
	task = pid_task(timr->it_pid, PIDTYPE_PID);
	if (task) {
		shared = !(timr->it_sigev_notify & SIGEV_THREAD_ID);
		ret = send_sigqueue(timr->sigq, task, shared);
	}
	rcu_read_unlock();
	/* If we failed to send the signal the timer stops. */
	return ret > 0;
}
EXPORT_SYMBOL_GPL(posix_timer_event);

/*
 * This function gets called when a POSIX.1b interval timer expires.  It
 * is used as a callback from the kernel internal timer.  The
 * run_timer_list code ALWAYS calls with interrupts on.

 * This code is for CLOCK_REALTIME* and CLOCK_MONOTONIC* timers.
 */
static enum hrtimer_restart posix_timer_fn(struct hrtimer *timer)
{
	struct k_itimer *timr;
	unsigned long flags;
	int si_private = 0;
	enum hrtimer_restart ret = HRTIMER_NORESTART;

	timr = container_of(timer, struct k_itimer, it.real.timer);
	spin_lock_irqsave(&timr->it_lock, flags);

	if (timr->it.real.interval.tv64 != 0)
		si_private = ++timr->it_requeue_pending;

	if (posix_timer_event(timr, si_private)) {
		/*
		 * signal was not sent because of sig_ignor
		 * we will not get a call back to restart it AND
		 * it should be restarted.
		 */
		if (timr->it.real.interval.tv64 != 0) {
			ktime_t now = hrtimer_cb_get_time(timer);

#ifdef CONFIG_HIGH_RES_TIMERS
			{
				ktime_t kj = ktime_set(0, NSEC_PER_SEC / HZ);

				if (timr->it.real.interval.tv64 < kj.tv64)
					now = ktime_add(now, kj);
			}
#endif
			timr->it_overrun += (unsigned int)
				hrtimer_forward(timer, now,
						timr->it.real.interval);
			ret = HRTIMER_RESTART;
			++timr->it_requeue_pending;
		}
	}

	unlock_timer(timr, flags);
	return ret;
}

static struct pid *good_sigevent(sigevent_t * event)
{
	struct task_struct *rtn = current->group_leader;

	if ((event->sigev_notify & SIGEV_THREAD_ID ) &&
		(!(rtn = find_task_by_vpid(event->sigev_notify_thread_id)) ||
		 !same_thread_group(rtn, current) ||
		 (event->sigev_notify & ~SIGEV_THREAD_ID) != SIGEV_SIGNAL))
		return NULL;

	if (((event->sigev_notify & ~SIGEV_THREAD_ID) != SIGEV_NONE) &&
	    ((event->sigev_signo <= 0) || (event->sigev_signo > SIGRTMAX)))
		return NULL;

	return task_pid(rtn);
}

void register_posix_clock(const clockid_t clock_id, struct k_clock *new_clock)
{
	if ((unsigned) clock_id >= MAX_CLOCKS) {
		printk("POSIX clock register failed for clock_id %d\n",
		       clock_id);
		return;
	}

	posix_clocks[clock_id] = *new_clock;
}
EXPORT_SYMBOL_GPL(register_posix_clock);

static struct k_itimer * alloc_posix_timer(void)
{
	struct k_itimer *tmr;
	tmr = kmem_cache_zalloc(posix_timers_cache, GFP_KERNEL);
	if (!tmr)
		return tmr;
	if (unlikely(!(tmr->sigq = sigqueue_alloc()))) {
		kmem_cache_free(posix_timers_cache, tmr);
		return NULL;
	}
	memset(&tmr->sigq->info, 0, sizeof(siginfo_t));
	return tmr;
}

#define IT_ID_SET	1
#define IT_ID_NOT_SET	0
static void release_posix_timer(struct k_itimer *tmr, int it_id_set)
{
	if (it_id_set) {
		unsigned long flags;
		spin_lock_irqsave(&idr_lock, flags);
		idr_remove(&posix_timers_id, tmr->it_id);
		spin_unlock_irqrestore(&idr_lock, flags);
	}
	put_pid(tmr->it_pid);
	sigqueue_free(tmr->sigq);
	kmem_cache_free(posix_timers_cache, tmr);
}

/* Create a POSIX.1b interval timer. */

SYSCALL_DEFINE3(timer_create, const clockid_t, which_clock,
		struct sigevent __user *, timer_event_spec,
		timer_t __user *, created_timer_id)
{
	struct k_itimer *new_timer;
	int error, new_timer_id;
	sigevent_t event;
	int it_id_set = IT_ID_NOT_SET;

	if (invalid_clockid(which_clock))
		return -EINVAL;

	new_timer = alloc_posix_timer();
	if (unlikely(!new_timer))
		return -EAGAIN;

	spin_lock_init(&new_timer->it_lock);
 retry:
	if (unlikely(!idr_pre_get(&posix_timers_id, GFP_KERNEL))) {
		error = -EAGAIN;
		goto out;
	}
	spin_lock_irq(&idr_lock);
	error = idr_get_new(&posix_timers_id, new_timer, &new_timer_id);
	spin_unlock_irq(&idr_lock);
	if (error) {
		if (error == -EAGAIN)
			goto retry;
		/*
		 * Weird looking, but we return EAGAIN if the IDR is
		 * full (proper POSIX return value for this)
		 */
		error = -EAGAIN;
		goto out;
	}

	it_id_set = IT_ID_SET;
	new_timer->it_id = (timer_t) new_timer_id;
	new_timer->it_clock = which_clock;
	new_timer->it_overrun = -1;

	if (timer_event_spec) {
		if (copy_from_user(&event, timer_event_spec, sizeof (event))) {
			error = -EFAULT;
			goto out;
		}
		rcu_read_lock();
		new_timer->it_pid = get_pid(good_sigevent(&event));
		rcu_read_unlock();
		if (!new_timer->it_pid) {
			error = -EINVAL;
			goto out;
		}
	} else {
		event.sigev_notify = SIGEV_SIGNAL;
		event.sigev_signo = SIGALRM;
		event.sigev_value.sival_int = new_timer->it_id;
		new_timer->it_pid = get_pid(task_tgid(current));
	}

	new_timer->it_sigev_notify     = event.sigev_notify;
	new_timer->sigq->info.si_signo = event.sigev_signo;
	new_timer->sigq->info.si_value = event.sigev_value;
	new_timer->sigq->info.si_tid   = new_timer->it_id;
	new_timer->sigq->info.si_code  = SI_TIMER;

	if (copy_to_user(created_timer_id,
			 &new_timer_id, sizeof (new_timer_id))) {
		error = -EFAULT;
		goto out;
	}

	error = CLOCK_DISPATCH(which_clock, timer_create, (new_timer));
	if (error)
		goto out;

	spin_lock_irq(&current->sighand->siglock);
	new_timer->it_signal = current->signal;
	list_add(&new_timer->list, &current->signal->posix_timers);
	spin_unlock_irq(&current->sighand->siglock);

	return 0;
 	/*
	 * In the case of the timer belonging to another task, after
	 * the task is unlocked, the timer is owned by the other task
	 * and may cease to exist at any time.  Don't use or modify
	 * new_timer after the unlock call.
	 */
out:
	release_posix_timer(new_timer, it_id_set);
	return error;
}

/*
 * Locking issues: We need to protect the result of the id look up until
 * we get the timer locked down so it is not deleted under us.  The
 * removal is done under the idr spinlock so we use that here to bridge
 * the find to the timer lock.  To avoid a dead lock, the timer id MUST
 * be release with out holding the timer lock.
 */
static struct k_itimer *lock_timer(timer_t timer_id, unsigned long *flags)
{
	struct k_itimer *timr;
	/*
	 * Watch out here.  We do a irqsave on the idr_lock and pass the
	 * flags part over to the timer lock.  Must not let interrupts in
	 * while we are moving the lock.
	 */
	spin_lock_irqsave(&idr_lock, *flags);
	timr = idr_find(&posix_timers_id, (int)timer_id);
	if (timr) {
		spin_lock(&timr->it_lock);
		if (timr->it_signal == current->signal) {
			spin_unlock(&idr_lock);
			return timr;
		}
		spin_unlock(&timr->it_lock);
	}
	spin_unlock_irqrestore(&idr_lock, *flags);

	return NULL;
}

/*
 * Get the time remaining on a POSIX.1b interval timer.  This function
 * is ALWAYS called with spin_lock_irq on the timer, thus it must not
 * mess with irq.
 *
 * We have a couple of messes to clean up here.  First there is the case
 * of a timer that has a requeue pending.  These timers should appear to
 * be in the timer list with an expiry as if we were to requeue them
 * now.
 *
 * The second issue is the SIGEV_NONE timer which may be active but is
 * not really ever put in the timer list (to save system resources).
 * This timer may be expired, and if so, we will do it here.  Otherwise
 * it is the same as a requeue pending timer WRT to what we should
 * report.
 */
static void
common_timer_get(struct k_itimer *timr, struct itimerspec *cur_setting)
{
	ktime_t now, remaining, iv;
	struct hrtimer *timer = &timr->it.real.timer;

	memset(cur_setting, 0, sizeof(struct itimerspec));

	iv = timr->it.real.interval;

	/* interval timer ? */
	if (iv.tv64)
		cur_setting->it_interval = ktime_to_timespec(iv);
	else if (!hrtimer_active(timer) &&
		 (timr->it_sigev_notify & ~SIGEV_THREAD_ID) != SIGEV_NONE)
		return;

	now = timer->base->get_time();

	/*
	 * When a requeue is pending or this is a SIGEV_NONE
	 * timer move the expiry time forward by intervals, so
	 * expiry is > now.
	 */
	if (iv.tv64 && (timr->it_requeue_pending & REQUEUE_PENDING ||
	    (timr->it_sigev_notify & ~SIGEV_THREAD_ID) == SIGEV_NONE))
		timr->it_overrun += (unsigned int) hrtimer_forward(timer, now, iv);

	remaining = ktime_sub(hrtimer_get_expires(timer), now);
	/* Return 0 only, when the timer is expired and not pending */
	if (remaining.tv64 <= 0) {
		/*
		 * A single shot SIGEV_NONE timer must return 0, when
		 * it is expired !
		 */
		if ((timr->it_sigev_notify & ~SIGEV_THREAD_ID) != SIGEV_NONE)
			cur_setting->it_value.tv_nsec = 1;
	} else
		cur_setting->it_value = ktime_to_timespec(remaining);
}

/* Get the time remaining on a POSIX.1b interval timer. */
SYSCALL_DEFINE2(timer_gettime, timer_t, timer_id,
		struct itimerspec __user *, setting)
{
	struct k_itimer *timr;
	struct itimerspec cur_setting;
	unsigned long flags;

	timr = lock_timer(timer_id, &flags);
	if (!timr)
		return -EINVAL;

	CLOCK_DISPATCH(timr->it_clock, timer_get, (timr, &cur_setting));

	unlock_timer(timr, flags);

	if (copy_to_user(setting, &cur_setting, sizeof (cur_setting)))
		return -EFAULT;

	return 0;
}

/*
 * Get the number of overruns of a POSIX.1b interval timer.  This is to
 * be the overrun of the timer last delivered.  At the same time we are
 * accumulating overruns on the next timer.  The overrun is frozen when
 * the signal is delivered, either at the notify time (if the info block
 * is not queued) or at the actual delivery time (as we are informed by
 * the call back to do_schedule_next_timer().  So all we need to do is
 * to pick up the frozen overrun.
 */
SYSCALL_DEFINE1(timer_getoverrun, timer_t, timer_id)
{
	struct k_itimer *timr;
	int overrun;
	unsigned long flags;

	timr = lock_timer(timer_id, &flags);
	if (!timr)
		return -EINVAL;

	overrun = timr->it_overrun_last;
	unlock_timer(timr, flags);

	return overrun;
}

/* Set a POSIX.1b interval timer. */
/* timr->it_lock is taken. */
static int
common_timer_set(struct k_itimer *timr, int flags,
		 struct itimerspec *new_setting, struct itimerspec *old_setting)
{
	struct hrtimer *timer = &timr->it.real.timer;
	enum hrtimer_mode mode;

	if (old_setting)
		common_timer_get(timr, old_setting);

	/* disable the timer */
	timr->it.real.interval.tv64 = 0;
	/*
	 * careful here.  If smp we could be in the "fire" routine which will
	 * be spinning as we hold the lock.  But this is ONLY an SMP issue.
	 */
	if (hrtimer_try_to_cancel(timer) < 0)
		return TIMER_RETRY;

	timr->it_requeue_pending = (timr->it_requeue_pending + 2) & 
		~REQUEUE_PENDING;
	timr->it_overrun_last = 0;

	/* switch off the timer when it_value is zero */
	if (!new_setting->it_value.tv_sec && !new_setting->it_value.tv_nsec)
		return 0;

	mode = flags & TIMER_ABSTIME ? HRTIMER_MODE_ABS : HRTIMER_MODE_REL;
	hrtimer_init(&timr->it.real.timer, timr->it_clock, mode);
	timr->it.real.timer.function = posix_timer_fn;

	hrtimer_set_expires(timer, timespec_to_ktime(new_setting->it_value));

	/* Convert interval */
	timr->it.real.interval = timespec_to_ktime(new_setting->it_interval);

	/* SIGEV_NONE timers are not queued ! See common_timer_get */
	if (((timr->it_sigev_notify & ~SIGEV_THREAD_ID) == SIGEV_NONE)) {
		/* Setup correct expiry time for relative timers */
		if (mode == HRTIMER_MODE_REL) {
			hrtimer_add_expires(timer, timer->base->get_time());
		}
		return 0;
	}

	hrtimer_start_expires(timer, mode);
	return 0;
}

/* Set a POSIX.1b interval timer */
SYSCALL_DEFINE4(timer_settime, timer_t, timer_id, int, flags,
		const struct itimerspec __user *, new_setting,
		struct itimerspec __user *, old_setting)
{
	struct k_itimer *timr;
	struct itimerspec new_spec, old_spec;
	int error = 0;
	unsigned long flag;
	struct itimerspec *rtn = old_setting ? &old_spec : NULL;

	if (!new_setting)
		return -EINVAL;

	if (copy_from_user(&new_spec, new_setting, sizeof (new_spec)))
		return -EFAULT;

	if (!timespec_valid(&new_spec.it_interval) ||
	    !timespec_valid(&new_spec.it_value))
		return -EINVAL;
retry:
	timr = lock_timer(timer_id, &flag);
	if (!timr)
		return -EINVAL;

	error = CLOCK_DISPATCH(timr->it_clock, timer_set,
			       (timr, flags, &new_spec, rtn));

	unlock_timer(timr, flag);
	if (error == TIMER_RETRY) {
		rtn = NULL;	// We already got the old time...
		goto retry;
	}

	if (old_setting && !error &&
	    copy_to_user(old_setting, &old_spec, sizeof (old_spec)))
		error = -EFAULT;

	return error;
}

static inline int common_timer_del(struct k_itimer *timer)
{
	timer->it.real.interval.tv64 = 0;

	if (hrtimer_try_to_cancel(&timer->it.real.timer) < 0)
		return TIMER_RETRY;
	return 0;
}

static inline int timer_delete_hook(struct k_itimer *timer)
{
	return CLOCK_DISPATCH(timer->it_clock, timer_del, (timer));
}

/* Delete a POSIX.1b interval timer. */
SYSCALL_DEFINE1(timer_delete, timer_t, timer_id)
{
	struct k_itimer *timer;
	unsigned long flags;

retry_delete:
	timer = lock_timer(timer_id, &flags);
	if (!timer)
		return -EINVAL;

	if (timer_delete_hook(timer) == TIMER_RETRY) {
		unlock_timer(timer, flags);
		goto retry_delete;
	}

	spin_lock(&current->sighand->siglock);
	list_del(&timer->list);
	spin_unlock(&current->sighand->siglock);
	/*
	 * This keeps any tasks waiting on the spin lock from thinking
	 * they got something (see the lock code above).
	 */
	timer->it_signal = NULL;

	unlock_timer(timer, flags);
	release_posix_timer(timer, IT_ID_SET);
	return 0;
}

/*
 * return timer owned by the process, used by exit_itimers
 */
static void itimer_delete(struct k_itimer *timer)
{
	unsigned long flags;

retry_delete:
	spin_lock_irqsave(&timer->it_lock, flags);

	if (timer_delete_hook(timer) == TIMER_RETRY) {
		unlock_timer(timer, flags);
		goto retry_delete;
	}
	list_del(&timer->list);
	/*
	 * This keeps any tasks waiting on the spin lock from thinking
	 * they got something (see the lock code above).
	 */
	timer->it_signal = NULL;

	unlock_timer(timer, flags);
	release_posix_timer(timer, IT_ID_SET);
}

/*
 * This is called by do_exit or de_thread, only when there are no more
 * references to the shared signal_struct.
 */
void exit_itimers(struct signal_struct *sig)
{
	struct k_itimer *tmr;

	while (!list_empty(&sig->posix_timers)) {
		tmr = list_entry(sig->posix_timers.next, struct k_itimer, list);
		itimer_delete(tmr);
	}
}

/* Not available / possible... functions */
int do_posix_clock_nosettime(const clockid_t clockid, struct timespec *tp)
{
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(do_posix_clock_nosettime);

int do_posix_clock_nonanosleep(const clockid_t clock, int flags,
			       struct timespec *t, struct timespec __user *r)
{
#ifndef ENOTSUP
	return -EOPNOTSUPP;	/* aka ENOTSUP in userland for POSIX */
#else  /*  parisc does define it separately.  */
	return -ENOTSUP;
#endif
}
EXPORT_SYMBOL_GPL(do_posix_clock_nonanosleep);

SYSCALL_DEFINE2(clock_settime, const clockid_t, which_clock,
		const struct timespec __user *, tp)
{
	struct timespec new_tp;

	if (invalid_clockid(which_clock))
		return -EINVAL;
	if (copy_from_user(&new_tp, tp, sizeof (*tp)))
		return -EFAULT;

	return CLOCK_DISPATCH(which_clock, clock_set, (which_clock, &new_tp));
}

SYSCALL_DEFINE2(clock_gettime, const clockid_t, which_clock,
		struct timespec __user *,tp)
{
	struct timespec kernel_tp;
	int error;

	if (invalid_clockid(which_clock))
		return -EINVAL;
	error = CLOCK_DISPATCH(which_clock, clock_get,
			       (which_clock, &kernel_tp));
	if (!error && copy_to_user(tp, &kernel_tp, sizeof (kernel_tp)))
		error = -EFAULT;

	return error;

}

SYSCALL_DEFINE2(clock_getres, const clockid_t, which_clock,
		struct timespec __user *, tp)
{
	struct timespec rtn_tp;
	int error;

	if (invalid_clockid(which_clock))
		return -EINVAL;

	error = CLOCK_DISPATCH(which_clock, clock_getres,
			       (which_clock, &rtn_tp));

	if (!error && tp && copy_to_user(tp, &rtn_tp, sizeof (rtn_tp))) {
		error = -EFAULT;
	}

	return error;
}

/*
 * nanosleep for monotonic and realtime clocks
 */
static int common_nsleep(const clockid_t which_clock, int flags,
			 struct timespec *tsave, struct timespec __user *rmtp)
{
	return hrtimer_nanosleep(tsave, rmtp, flags & TIMER_ABSTIME ?
				 HRTIMER_MODE_ABS : HRTIMER_MODE_REL,
				 which_clock);
}

SYSCALL_DEFINE4(clock_nanosleep, const clockid_t, which_clock, int, flags,
		const struct timespec __user *, rqtp,
		struct timespec __user *, rmtp)
{
	struct timespec t;

	if (invalid_clockid(which_clock))
		return -EINVAL;

	if (copy_from_user(&t, rqtp, sizeof (struct timespec)))
		return -EFAULT;

	if (!timespec_valid(&t))
		return -EINVAL;

	return CLOCK_DISPATCH(which_clock, nsleep,
			      (which_clock, flags, &t, rmtp));
}

/*
 * nanosleep_restart for monotonic and realtime clocks
 */
static int common_nsleep_restart(struct restart_block *restart_block)
{
	return hrtimer_nanosleep_restart(restart_block);
}

/*
 * This will restart clock_nanosleep. This is required only by
 * compat_clock_nanosleep_restart for now.
 */
long
clock_nanosleep_restart(struct restart_block *restart_block)
{
	clockid_t which_clock = restart_block->arg0;

	return CLOCK_DISPATCH(which_clock, nsleep_restart,
			      (restart_block));
}
