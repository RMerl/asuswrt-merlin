/*
 * Copyright (C) 2000-2006 Erik Andersen <andersen@uclibc.org>
 *
 * Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.
 */
/*
 * Never include this file directly; use <unistd.h> instead.
 */

#ifndef	_BITS_UCLIBC_LOCAL_LIM_H
#define	_BITS_UCLIBC_LOCAL_LIM_H	1

/* This file works correctly only if local_lim.h is the NPTL version */
#if !defined PTHREAD_KEYS_MAX || defined TIMER_MAX
# error local_lim.h was incorrectly updated, use the NPTL version from glibc
#endif

/* This should really be moved to thread specific directories */
#if defined __UCLIBC_HAS_THREADS__ && !defined __UCLIBC_HAS_THREADS_NATIVE__
# define PTHREAD_THREADS_MAX	1024
# define TIMER_MAX		256
#endif

#ifndef __UCLIBC_HAS_THREADS__
# undef _POSIX_THREAD_KEYS_MAX
# undef PTHREAD_KEYS_MAX
# undef _POSIX_THREAD_DESTRUCTOR_ITERATIONS
# undef PTHREAD_DESTRUCTOR_ITERATIONS
# undef PTHREAD_STACK_MIN
# undef DELAYTIMER_MAX
#endif

#endif /* bits/uClibc_local_lim.h */
