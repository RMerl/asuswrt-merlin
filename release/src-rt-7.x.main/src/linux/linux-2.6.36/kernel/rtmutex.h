/*
 * RT-Mutexes: blocking mutual exclusion locks with PI support
 *
 * started by Ingo Molnar and Thomas Gleixner:
 *
 *  Copyright (C) 2004-2006 Red Hat, Inc., Ingo Molnar <mingo@redhat.com>
 *  Copyright (C) 2006, Timesys Corp., Thomas Gleixner <tglx@timesys.com>
 *
 * This file contains macros used solely by rtmutex.c.
 * Non-debug version.
 */

#define rt_mutex_deadlock_check(l)			(0)
#define rt_mutex_deadlock_account_lock(m, t)		do { } while (0)
#define rt_mutex_deadlock_account_unlock(l)		do { } while (0)
#define debug_rt_mutex_init_waiter(w)			do { } while (0)
#define debug_rt_mutex_free_waiter(w)			do { } while (0)
#define debug_rt_mutex_lock(l)				do { } while (0)
#define debug_rt_mutex_proxy_lock(l,p)			do { } while (0)
#define debug_rt_mutex_proxy_unlock(l)			do { } while (0)
#define debug_rt_mutex_unlock(l)			do { } while (0)
#define debug_rt_mutex_init(m, n)			do { } while (0)
#define debug_rt_mutex_deadlock(d, a ,l)		do { } while (0)
#define debug_rt_mutex_print_deadlock(w)		do { } while (0)
#define debug_rt_mutex_detect_deadlock(w,d)		(d)
#define debug_rt_mutex_reset_waiter(w)			do { } while (0)
