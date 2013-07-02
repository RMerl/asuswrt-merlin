/* Copyright (C) 2006   Manuel Novoa III    <mjn3@codepoet.org>
 *
 * GNU Library General Public License (LGPL) version 2 or later.
 *
 * Dedicated to Toni.  See uClibc/DEDICATION.mjn3 for details.
 */

#ifndef _UCLIBC_MUTEX_H
#define _UCLIBC_MUTEX_H

#include <features.h>

#ifdef __UCLIBC_HAS_THREADS__

#include <pthread.h>
#include <bits/uClibc_pthread.h>

#define __UCLIBC_MUTEX_TYPE				pthread_mutex_t

#define __UCLIBC_MUTEX(M)				pthread_mutex_t M
#define __UCLIBC_MUTEX_INIT(M,I)			pthread_mutex_t M = I
#define __UCLIBC_MUTEX_STATIC(M,I)			static pthread_mutex_t M = I
#define __UCLIBC_MUTEX_EXTERN(M)			extern pthread_mutex_t M

#define __UCLIBC_MUTEX_INIT_VAR(M)								\
		((M) = (pthread_mutex_t) PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)

#define __UCLIBC_MUTEX_LOCK_CANCEL_UNSAFE(M)								\
		__pthread_mutex_lock(&(M))

#define __UCLIBC_MUTEX_UNLOCK_CANCEL_UNSAFE(M)								\
		__pthread_mutex_unlock(&(M))

#define __UCLIBC_MUTEX_TRYLOCK_CANCEL_UNSAFE(M)								\
		__pthread_mutex_trylock(&(M))

#define __UCLIBC_MUTEX_CONDITIONAL_LOCK(M,C)								\
	do {												\
		struct _pthread_cleanup_buffer __infunc_pthread_cleanup_buffer;				\
		int __infunc_need_locking = (C);							\
		if (__infunc_need_locking) {								\
			_pthread_cleanup_push_defer(&__infunc_pthread_cleanup_buffer,			\
					   (void (*) (void *))__pthread_mutex_unlock,			\
										&(M));			\
			__pthread_mutex_lock(&(M));							\
		}											\
		((void)0)

#define __UCLIBC_MUTEX_CONDITIONAL_UNLOCK(M,C)								\
		if (__infunc_need_locking) {								\
			_pthread_cleanup_pop_restore(&__infunc_pthread_cleanup_buffer,1);		\
		}											\
	} while (0)

#define __UCLIBC_MUTEX_AUTO_LOCK_VAR(A)		int A

#define __UCLIBC_MUTEX_AUTO_LOCK(M,A,V)									\
        __UCLIBC_MUTEX_CONDITIONAL_LOCK(M,((A=(V)) == 0))

#define __UCLIBC_MUTEX_AUTO_UNLOCK(M,A)									\
        __UCLIBC_MUTEX_CONDITIONAL_UNLOCK(M,(A == 0))

#define __UCLIBC_MUTEX_LOCK(M)										\
        __UCLIBC_MUTEX_CONDITIONAL_LOCK(M, 1)

#define __UCLIBC_MUTEX_UNLOCK(M)									\
        __UCLIBC_MUTEX_CONDITIONAL_UNLOCK(M, 1)

#ifdef __USE_STDIO_FUTEXES__

#include <bits/stdio-lock.h>

#define __UCLIBC_IO_MUTEX(M)			_IO_lock_t M
#define __UCLIBC_IO_MUTEX_LOCK(M) 		_IO_lock_lock(M)
#define __UCLIBC_IO_MUTEX_UNLOCK(M) 	_IO_lock_unlock(M)
#define __UCLIBC_IO_MUTEX_TRYLOCK(M) 	_IO_lock_trylock(M)
#define __UCLIBC_IO_MUTEX_INIT(M) 	_IO_lock_t M = _IO_lock_initializer
#define __UCLIBC_IO_MUTEX_EXTERN(M)		extern _IO_lock_t M

#define __UCLIBC_IO_MUTEX_CONDITIONAL_LOCK(M,C)		\
	if (C) {										\
		_IO_lock_lock(M);							\
	}

#define __UCLIBC_IO_MUTEX_CONDITIONAL_UNLOCK(M,C)	\
	if (C) {										\
		_IO_lock_unlock(M);							\
	}

#define __UCLIBC_IO_MUTEX_AUTO_LOCK(M,A,V)			\
		__UCLIBC_IO_MUTEX_CONDITIONAL_LOCK(M,((A=(V))) == 0)

#define __UCLIBC_IO_MUTEX_AUTO_UNLOCK(M,A)			\
		__UCLIBC_IO_MUTEX_CONDITIONAL_UNLOCK(M,((A) == 0))

#define __UCLIBC_IO_MUTEX_LOCK_CANCEL_UNSAFE(M)		_IO_lock_lock(M)
#define __UCLIBC_IO_MUTEX_UNLOCK_CANCEL_UNSAFE(M) 	_IO_lock_unlock(M)

#else /* of __USE_STDIO_FUTEXES__ */

#define __UCLIBC_IO_MUTEX(M)                        __UCLIBC_MUTEX(M)
#define __UCLIBC_IO_MUTEX_LOCK(M)                   __UCLIBC_MUTEX_CONDITIONAL_LOCK(M, 1)
#define __UCLIBC_IO_MUTEX_UNLOCK(M)                 __UCLIBC_MUTEX_CONDITIONAL_UNLOCK(M, 1)
#define __UCLIBC_IO_MUTEX_TRYLOCK(M)                __UCLIBC_MUTEX_TRYLOCK_CANCEL_UNSAFE(M)
#define __UCLIBC_IO_MUTEX_INIT(M)                   __UCLIBC_MUTEX_INIT(M, PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)
#define __UCLIBC_IO_MUTEX_EXTERN(M)                 __UCLIBC_MUTEX_EXTERN(M)
#define __UCLIBC_IO_MUTEX_AUTO_LOCK(M,A,V)          __UCLIBC_MUTEX_AUTO_LOCK(M,A,V)
#define __UCLIBC_IO_MUTEX_AUTO_UNLOCK(M,A)          __UCLIBC_MUTEX_AUTO_UNLOCK(M,A)
#define __UCLIBC_IO_MUTEX_LOCK_CANCEL_UNSAFE(M)     __UCLIBC_MUTEX_LOCK_CANCEL_UNSAFE(M)
#define __UCLIBC_IO_MUTEX_UNLOCK_CANCEL_UNSAFE(M)   __UCLIBC_MUTEX_UNLOCK_CANCEL_UNSAFE(M)
#define __UCLIBC_IO_MUTEX_CONDITIONAL_LOCK(M,C)     __UCLIBC_MUTEX_CONDITIONAL_LOCK(M, 1)
#define __UCLIBC_IO_MUTEX_CONDITIONAL_UNLOCK(M,C)   __UCLIBC_MUTEX_CONDITIONAL_UNLOCK(M, 1)

#endif /* of __USE_STDIO_FUTEXES__ */


#else /* of __UCLIBC_HAS_THREADS__ */

#define __UCLIBC_MUTEX(M)				void *__UCLIBC_MUTEX_DUMMY_ ## M
#define __UCLIBC_MUTEX_INIT(M,I)			extern void *__UCLIBC_MUTEX_DUMMY_ ## M
#define __UCLIBC_MUTEX_STATIC(M,I)			extern void *__UCLIBC_MUTEX_DUMMY_ ## M
#define __UCLIBC_MUTEX_EXTERN(M)			extern void *__UCLIBC_MUTEX_DUMMY_ ## M

#define __UCLIBC_MUTEX_INIT_VAR(M)					((void)0)
#define __UCLIBC_MUTEX_LOCK_CANCEL_UNSAFE(M)		((void)0)
#define __UCLIBC_MUTEX_UNLOCK_CANCEL_UNSAFE(M)		((void)0)
#define __UCLIBC_MUTEX_TRYLOCK_CANCEL_UNSAFE(M)		(0)	/* Always succeed? */

#define __UCLIBC_MUTEX_CONDITIONAL_LOCK(M,C)		((void)0)
#define __UCLIBC_MUTEX_CONDITIONAL_UNLOCK(M,C)		((void)0)

#define __UCLIBC_MUTEX_AUTO_LOCK_VAR(A)			((void)0)
#define __UCLIBC_MUTEX_AUTO_LOCK(M,A,V)			((void)0)
#define __UCLIBC_MUTEX_AUTO_UNLOCK(M,A)			((void)0)

#define __UCLIBC_MUTEX_LOCK(M)				((void)0)
#define __UCLIBC_MUTEX_UNLOCK(M)			((void)0)

#define __UCLIBC_IO_MUTEX(M)                        __UCLIBC_MUTEX(M)
#define __UCLIBC_IO_MUTEX_LOCK(M)                   __UCLIBC_MUTEX_CONDITIONAL_LOCK(M, 1)
#define __UCLIBC_IO_MUTEX_UNLOCK(M)                 __UCLIBC_MUTEX_CONDITIONAL_UNLOCK(M, 1)
#define __UCLIBC_IO_MUTEX_TRYLOCK(M)                __UCLIBC_MUTEX_TRYLOCK_CANCEL_UNSAFE(M)
#define __UCLIBC_IO_MUTEX_INIT(M)                   __UCLIBC_MUTEX_INIT(M, PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)
#define __UCLIBC_IO_MUTEX_EXTERN(M)                 __UCLIBC_MUTEX_EXTERN(M)
#define __UCLIBC_IO_MUTEX_AUTO_LOCK(M,A,V)          __UCLIBC_MUTEX_AUTO_LOCK(M,A,V)
#define __UCLIBC_IO_MUTEX_AUTO_UNLOCK(M,A)          __UCLIBC_MUTEX_AUTO_UNLOCK(M,A)
#define __UCLIBC_IO_MUTEX_LOCK_CANCEL_UNSAFE(M)     __UCLIBC_MUTEX_LOCK_CANCEL_UNSAFE(M)
#define __UCLIBC_IO_MUTEX_UNLOCK_CANCEL_UNSAFE(M)   __UCLIBC_MUTEX_UNLOCK_CANCEL_UNSAFE(M)
#define __UCLIBC_IO_MUTEX_CONDITIONAL_LOCK(M,C)     __UCLIBC_MUTEX_CONDITIONAL_LOCK(M, 1)
#define __UCLIBC_IO_MUTEX_CONDITIONAL_UNLOCK(M,C)   __UCLIBC_MUTEX_CONDITIONAL_UNLOCK(M, 1)

#endif /* of __UCLIBC_HAS_THREADS__ */

#define __UCLIBC_IO_MUTEX_TRYLOCK_CANCEL_UNSAFE(M)	\
		__UCLIBC_IO_MUTEX_TRYLOCK(M)

#endif /* _UCLIBC_MUTEX_H */
