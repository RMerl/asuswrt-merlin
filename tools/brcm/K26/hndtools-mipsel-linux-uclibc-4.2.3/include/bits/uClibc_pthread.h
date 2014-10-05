/*  Copyright (C) 2003     Manuel Novoa III
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  The GNU C Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with the GNU C Library; if not, write to the Free
 *  Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307 USA.
 */

/* Supply prototypes for the internal thread functions used by the
 * uClibc library code.
 */

#ifndef _UCLIBC_PTHREAD_H
#define _UCLIBC_PTHREAD_H

#ifndef _PTHREAD_H
# error "Always include <pthread.h> rather than <bits/uClibc_pthread.h>"
#endif

#if defined _LIBC && (defined IS_IN_libc || defined NOT_IN_libc)
/* Threading functions internal to uClibc.  Make these thread functions
 * weak so that we can elide them from single-threaded processes.  */
extern int weak_function __pthread_mutex_init (pthread_mutex_t *__mutex,
		__const pthread_mutexattr_t *__mutex_attr);
extern int weak_function __pthread_mutex_destroy (pthread_mutex_t *__mutex);
extern int weak_function __pthread_mutex_lock (pthread_mutex_t *__mutex);
extern int weak_function __pthread_mutex_unlock (pthread_mutex_t *__mutex);
extern void __uclibc_mutex_unlock (void *) attribute_hidden;
extern int weak_function __pthread_mutex_trylock (pthread_mutex_t *__mutex);
# ifndef __UCLIBC_HAS_THREADS_NATIVE__
extern void weak_function _pthread_cleanup_push_defer (
		struct _pthread_cleanup_buffer *__buffer,
		void (*__routine) (void *), void *__arg);
extern void weak_function _pthread_cleanup_pop_restore (
		struct _pthread_cleanup_buffer *__buffer,
		int __execute);
# endif
#endif

#endif
