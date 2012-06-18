/* Definitions for POSIX 1003.1b-1993 (aka POSIX.4) scheduling interface.
   Copyright (C) 1996,1997,1999,2001-2003,2004 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef	_SCHED_H
#define	_SCHED_H	1

#include <features.h>

/* Get type definitions.  */
#include <bits/types.h>

#define __need_timespec
#include <time.h>

/* Get system specific constant and data structure definitions.  */
#include <bits/sched.h>
/* Define the real names for the elements of `struct sched_param'.  */
#define sched_priority	__sched_priority


__BEGIN_DECLS

/* Set scheduling parameters for a process.  */
extern int sched_setparam (__pid_t __pid, __const struct sched_param *__param)
     __THROW;

/* Retrieve scheduling parameters for a particular process.  */
extern int sched_getparam (__pid_t __pid, struct sched_param *__param) __THROW;

/* Set scheduling algorithm and/or parameters for a process.  */
extern int sched_setscheduler (__pid_t __pid, int __policy,
			       __const struct sched_param *__param) __THROW;

/* Retrieve scheduling algorithm for a particular purpose.  */
extern int sched_getscheduler (__pid_t __pid) __THROW;

/* Yield the processor.  */
extern int sched_yield (void) __THROW;

/* Get maximum priority value for a scheduler.  */
extern int sched_get_priority_max (int __algorithm) __THROW;

/* Get minimum priority value for a scheduler.  */
extern int sched_get_priority_min (int __algorithm) __THROW;

/* Get the SCHED_RR interval for the named process.  */
extern int sched_rr_get_interval (__pid_t __pid, struct timespec *__t) __THROW;


#if defined __USE_GNU && defined __UCLIBC_LINUX_SPECIFIC__
/* Access macros for `cpu_set'.  */
#define CPU_SETSIZE __CPU_SETSIZE
#define CPU_SET(cpu, cpusetp)	__CPU_SET (cpu, cpusetp)
#define CPU_CLR(cpu, cpusetp)	__CPU_CLR (cpu, cpusetp)
#define CPU_ISSET(cpu, cpusetp)	__CPU_ISSET (cpu, cpusetp)
#define CPU_ZERO(cpusetp)	__CPU_ZERO (cpusetp)


/* Set the CPU affinity for a task */
extern int sched_setaffinity (__pid_t __pid, size_t __cpusetsize,
			      __const cpu_set_t *__cpuset) __THROW;

/* Get the CPU affinity for a task */
extern int sched_getaffinity (__pid_t __pid, size_t __cpusetsize,
			      cpu_set_t *__cpuset) __THROW;
#endif

__END_DECLS

#endif /* sched.h */
