#ifndef _LINUX_RESOURCE_H
#define _LINUX_RESOURCE_H

#include <linux/time.h>

/*
 * Resource control/accounting header file for linux
 */

/*
 * Definition of struct rusage taken from BSD 4.3 Reno
 * 
 * We don't support all of these yet, but we might as well have them....
 * Otherwise, each time we add new items, programs which depend on this
 * structure will lose.  This reduces the chances of that happening.
 */
#define	RUSAGE_SELF	0
#define	RUSAGE_CHILDREN	(-1)
#define RUSAGE_BOTH	(-2)		/* sys_wait4() uses this */
#define	RUSAGE_THREAD	1		/* only the calling thread */

struct	rusage {
	struct timeval ru_utime;	/* user time used */
	struct timeval ru_stime;	/* system time used */
	long	ru_maxrss;		/* maximum resident set size */
	long	ru_ixrss;		/* integral shared memory size */
	long	ru_idrss;		/* integral unshared data size */
	long	ru_isrss;		/* integral unshared stack size */
	long	ru_minflt;		/* page reclaims */
	long	ru_majflt;		/* page faults */
	long	ru_nswap;		/* swaps */
	long	ru_inblock;		/* block input operations */
	long	ru_oublock;		/* block output operations */
	long	ru_msgsnd;		/* messages sent */
	long	ru_msgrcv;		/* messages received */
	long	ru_nsignals;		/* signals received */
	long	ru_nvcsw;		/* voluntary context switches */
	long	ru_nivcsw;		/* involuntary " */
};

struct rlimit {
	unsigned long	rlim_cur;
	unsigned long	rlim_max;
};

#define RLIM64_INFINITY		(~0ULL)

struct rlimit64 {
	__u64 rlim_cur;
	__u64 rlim_max;
};

#define	PRIO_MIN	(-20)
#define	PRIO_MAX	20

#define	PRIO_PROCESS	0
#define	PRIO_PGRP	1
#define	PRIO_USER	2

/*
 * Limit the stack by to some sane default: root can always
 * increase this limit if needed..  8MB seems reasonable.
 */
#define _STK_LIM	(8*1024*1024)

/*
 * GPG2 wants 64kB of mlocked memory, to make sure pass phrases
 * and other sensitive information are never written to disk.
 */
#define MLOCK_LIMIT	((PAGE_SIZE > 64*1024) ? PAGE_SIZE : 64*1024)

/*
 * Due to binary compatibility, the actual resource numbers
 * may be different for different linux versions..
 */
#include <asm/resource.h>

#ifdef __KERNEL__

struct task_struct;

int getrusage(struct task_struct *p, int who, struct rusage __user *ru);
int do_prlimit(struct task_struct *tsk, unsigned int resource,
		struct rlimit *new_rlim, struct rlimit *old_rlim);

#endif /* __KERNEL__ */

#endif
