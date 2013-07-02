/* Linuxthreads - a simple clone()-based implementation of Posix        */
/* threads for Linux.                                                   */
/* Copyright (C) 1996 Xavier Leroy (Xavier.Leroy@inria.fr)              */
/*                                                                      */
/* This program is free software; you can redistribute it and/or        */
/* modify it under the terms of the GNU Library General Public License  */
/* as published by the Free Software Foundation; either version 2       */
/* of the License, or (at your option) any later version.               */
/*                                                                      */
/* This program is distributed in the hope that it will be useful,      */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of       */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        */
/* GNU Library General Public License for more details.                 */

#ifndef _PTHREAD_H
#define _PTHREAD_H	1

#include <features.h>

#include <sched.h>
#include <time.h>

#define __need_sigset_t
#include <signal.h>
#include <bits/pthreadtypes.h>
#include <bits/initspin.h>


__BEGIN_DECLS

/* Initializers.  */

#define PTHREAD_MUTEX_INITIALIZER \
  {0, 0, 0, PTHREAD_MUTEX_ADAPTIVE_NP, __LOCK_INITIALIZER}
#ifdef __USE_GNU
# define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP \
  {0, 0, 0, PTHREAD_MUTEX_RECURSIVE_NP, __LOCK_INITIALIZER}
# define PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP \
  {0, 0, 0, PTHREAD_MUTEX_ERRORCHECK_NP, __LOCK_INITIALIZER}
# define PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP \
  {0, 0, 0, PTHREAD_MUTEX_ADAPTIVE_NP, __LOCK_INITIALIZER}
#endif

#define PTHREAD_COND_INITIALIZER {__LOCK_INITIALIZER, 0}

#if defined __USE_UNIX98 || defined __USE_XOPEN2K
# define PTHREAD_RWLOCK_INITIALIZER \
  { __LOCK_INITIALIZER, 0, NULL, NULL, NULL,				      \
    PTHREAD_RWLOCK_DEFAULT_NP, PTHREAD_PROCESS_PRIVATE }
#endif
#ifdef __USE_GNU
# define PTHREAD_RWLOCK_WRITER_NONRECURSIVE_INITIALIZER_NP \
  { __LOCK_INITIALIZER, 0, NULL, NULL, NULL,				      \
    PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP, PTHREAD_PROCESS_PRIVATE }
#endif

/* Values for attributes.  */

enum
{
  PTHREAD_CREATE_JOINABLE,
#define PTHREAD_CREATE_JOINABLE	PTHREAD_CREATE_JOINABLE
  PTHREAD_CREATE_DETACHED
#define PTHREAD_CREATE_DETACHED	PTHREAD_CREATE_DETACHED
};

enum
{
  PTHREAD_INHERIT_SCHED,
#define PTHREAD_INHERIT_SCHED	PTHREAD_INHERIT_SCHED
  PTHREAD_EXPLICIT_SCHED
#define PTHREAD_EXPLICIT_SCHED	PTHREAD_EXPLICIT_SCHED
};

enum
{
  PTHREAD_SCOPE_SYSTEM,
#define PTHREAD_SCOPE_SYSTEM	PTHREAD_SCOPE_SYSTEM
  PTHREAD_SCOPE_PROCESS
#define PTHREAD_SCOPE_PROCESS	PTHREAD_SCOPE_PROCESS
};

enum
{
  PTHREAD_MUTEX_ADAPTIVE_NP,
  PTHREAD_MUTEX_RECURSIVE_NP,
  PTHREAD_MUTEX_ERRORCHECK_NP,
  PTHREAD_MUTEX_TIMED_NP
#ifdef __USE_UNIX98
  ,
  PTHREAD_MUTEX_NORMAL = PTHREAD_MUTEX_ADAPTIVE_NP,
  PTHREAD_MUTEX_RECURSIVE = PTHREAD_MUTEX_RECURSIVE_NP,
  PTHREAD_MUTEX_ERRORCHECK = PTHREAD_MUTEX_ERRORCHECK_NP,
  PTHREAD_MUTEX_DEFAULT = PTHREAD_MUTEX_NORMAL
#endif
#ifdef __USE_GNU
  /* For compatibility.  */
  , PTHREAD_MUTEX_FAST_NP = PTHREAD_MUTEX_ADAPTIVE_NP
#endif
};

enum
{
  PTHREAD_PROCESS_PRIVATE,
#define PTHREAD_PROCESS_PRIVATE	PTHREAD_PROCESS_PRIVATE
  PTHREAD_PROCESS_SHARED
#define PTHREAD_PROCESS_SHARED	PTHREAD_PROCESS_SHARED
};

#if defined __USE_UNIX98 || defined __USE_XOPEN2K
enum
{
  PTHREAD_RWLOCK_PREFER_READER_NP,
  PTHREAD_RWLOCK_PREFER_WRITER_NP,
  PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP,
  PTHREAD_RWLOCK_DEFAULT_NP = PTHREAD_RWLOCK_PREFER_WRITER_NP
};
#endif	/* Unix98 */

#define PTHREAD_ONCE_INIT 0

/* Special constants */

#ifdef __USE_XOPEN2K
/* -1 is distinct from 0 and all errno constants */
# define PTHREAD_BARRIER_SERIAL_THREAD -1
#endif

/* Cleanup buffers */

struct _pthread_cleanup_buffer
{
  void (*__routine) (void *);		  /* Function to call.  */
  void *__arg;				  /* Its argument.  */
  int __canceltype;			  /* Saved cancellation type. */
  struct _pthread_cleanup_buffer *__prev; /* Chaining of cleanup functions.  */
};

/* Cancellation */

enum
{
  PTHREAD_CANCEL_ENABLE,
#define PTHREAD_CANCEL_ENABLE	PTHREAD_CANCEL_ENABLE
  PTHREAD_CANCEL_DISABLE
#define PTHREAD_CANCEL_DISABLE	PTHREAD_CANCEL_DISABLE
};
enum
{
  PTHREAD_CANCEL_DEFERRED,
#define PTHREAD_CANCEL_DEFERRED	PTHREAD_CANCEL_DEFERRED
  PTHREAD_CANCEL_ASYNCHRONOUS
#define PTHREAD_CANCEL_ASYNCHRONOUS	PTHREAD_CANCEL_ASYNCHRONOUS
};
#define PTHREAD_CANCELED ((void *) -1)


/* Function for handling threads.  */

/* Create a thread with given attributes ATTR (or default attributes
   if ATTR is NULL), and call function START_ROUTINE with given
   arguments ARG.  */
extern int pthread_create (pthread_t *__restrict __threadp,
			   __const pthread_attr_t *__restrict __attr,
			   void *(*__start_routine) (void *),
			   void *__restrict __arg) __THROW;

/* Obtain the identifier of the current thread.  */
extern pthread_t pthread_self (void) __THROW;

/* Compare two thread identifiers.  */
extern int pthread_equal (pthread_t __thread1, pthread_t __thread2) __THROW;

/* Terminate calling thread.  */
extern void pthread_exit (void *__retval) __attribute__ ((__noreturn__));

/* Make calling thread wait for termination of the thread TH.  The
   exit status of the thread is stored in *THREAD_RETURN, if THREAD_RETURN
   is not NULL.  */
extern int pthread_join (pthread_t __th, void **__thread_return);

/* Indicate that the thread TH is never to be joined with PTHREAD_JOIN.
   The resources of TH will therefore be freed immediately when it
   terminates, instead of waiting for another thread to perform PTHREAD_JOIN
   on it.  */
extern int pthread_detach (pthread_t __th) __THROW;


/* Functions for handling attributes.  */

/* Initialize thread attribute *ATTR with default attributes
   (detachstate is PTHREAD_JOINABLE, scheduling policy is SCHED_OTHER,
    no user-provided stack).  */
extern int pthread_attr_init (pthread_attr_t *__attr) __THROW;

/* Destroy thread attribute *ATTR.  */
extern int pthread_attr_destroy (pthread_attr_t *__attr) __THROW;

/* Set the `detachstate' attribute in *ATTR according to DETACHSTATE.  */
extern int pthread_attr_setdetachstate (pthread_attr_t *__attr,
					int __detachstate) __THROW;

/* Return in *DETACHSTATE the `detachstate' attribute in *ATTR.  */
extern int pthread_attr_getdetachstate (__const pthread_attr_t *__attr,
					int *__detachstate) __THROW;

/* Set scheduling parameters (priority, etc) in *ATTR according to PARAM.  */
extern int pthread_attr_setschedparam (pthread_attr_t *__restrict __attr,
				       __const struct sched_param *__restrict
				       __param) __THROW;

/* Return in *PARAM the scheduling parameters of *ATTR.  */
extern int pthread_attr_getschedparam (__const pthread_attr_t *__restrict
				       __attr,
				       struct sched_param *__restrict __param)
     __THROW;

/* Set scheduling policy in *ATTR according to POLICY.  */
extern int pthread_attr_setschedpolicy (pthread_attr_t *__attr, int __policy)
     __THROW;

/* Return in *POLICY the scheduling policy of *ATTR.  */
extern int pthread_attr_getschedpolicy (__const pthread_attr_t *__restrict
					__attr, int *__restrict __policy)
     __THROW;

/* Set scheduling inheritance mode in *ATTR according to INHERIT.  */
extern int pthread_attr_setinheritsched (pthread_attr_t *__attr,
					 int __inherit) __THROW;

/* Return in *INHERIT the scheduling inheritance mode of *ATTR.  */
extern int pthread_attr_getinheritsched (__const pthread_attr_t *__restrict
					 __attr, int *__restrict __inherit)
     __THROW;

/* Set scheduling contention scope in *ATTR according to SCOPE.  */
extern int pthread_attr_setscope (pthread_attr_t *__attr, int __scope)
     __THROW;

/* Return in *SCOPE the scheduling contention scope of *ATTR.  */
extern int pthread_attr_getscope (__const pthread_attr_t *__restrict __attr,
				  int *__restrict __scope) __THROW;

#ifdef __USE_UNIX98
/* Set the size of the guard area at the bottom of the thread.  */
extern int pthread_attr_setguardsize (pthread_attr_t *__attr,
				      size_t __guardsize) __THROW;

/* Get the size of the guard area at the bottom of the thread.  */
extern int pthread_attr_getguardsize (__const pthread_attr_t *__restrict
				      __attr, size_t *__restrict __guardsize)
     __THROW;
#endif

#if 0 /* uClibc: deprecated stuff disabled. def __UCLIBC_SUSV3_LEGACY__ */
/* Set the starting address of the stack of the thread to be created.
   Depending on whether the stack grows up or down the value must either
   be higher or lower than all the address in the memory block.  The
   minimal size of the block must be PTHREAD_STACK_SIZE.  */
extern int pthread_attr_setstackaddr (pthread_attr_t *__attr,
				      void *__stackaddr) __THROW;

/* Return the previously set address for the stack.  */
extern int pthread_attr_getstackaddr (__const pthread_attr_t *__restrict
				      __attr, void **__restrict __stackaddr)
     __THROW;
#endif

#ifdef __USE_XOPEN2K
/* The following two interfaces are intended to replace the last two.  They
   require setting the address as well as the size since only setting the
   address will make the implementation on some architectures impossible.  */
extern int pthread_attr_setstack (pthread_attr_t *__attr, void *__stackaddr,
				  size_t __stacksize) __THROW;

/* Return the previously set address for the stack.  */
extern int pthread_attr_getstack (__const pthread_attr_t *__restrict __attr,
				  void **__restrict __stackaddr,
				  size_t *__restrict __stacksize) __THROW;
#endif

/* Add information about the minimum stack size needed for the thread
   to be started.  This size must never be less than PTHREAD_STACK_SIZE
   and must also not exceed the system limits.  */
extern int pthread_attr_setstacksize (pthread_attr_t *__attr,
				      size_t __stacksize) __THROW;

/* Return the currently used minimal stack size.  */
extern int pthread_attr_getstacksize (__const pthread_attr_t *__restrict
				      __attr, size_t *__restrict __stacksize)
     __THROW;

#if 0
/* Not yet implemented in uClibc! */

#ifdef __USE_GNU
/* Initialize thread attribute *ATTR with attributes corresponding to the
   already running thread TH.  It shall be called on uninitialized ATTR
   and destroyed with pthread_attr_destroy when no longer needed.  */
extern int pthread_getattr_np (pthread_t __th, pthread_attr_t *__attr) __THROW;
#endif
#endif

/* Functions for scheduling control.  */

/* Set the scheduling parameters for TARGET_THREAD according to POLICY
   and *PARAM.  */
extern int pthread_setschedparam (pthread_t __target_thread, int __policy,
				  __const struct sched_param *__param)
     __THROW;

/* Return in *POLICY and *PARAM the scheduling parameters for TARGET_THREAD.  */
extern int pthread_getschedparam (pthread_t __target_thread,
				  int *__restrict __policy,
				  struct sched_param *__restrict __param)
     __THROW;

#ifdef __USE_UNIX98
/* Determine level of concurrency.  */
extern int pthread_getconcurrency (void) __THROW;

/* Set new concurrency level to LEVEL.  */
extern int pthread_setconcurrency (int __level) __THROW;
#endif

#ifdef __USE_GNU
/* Same thing, different name */
#define pthread_yield() sched_yield()
#endif

/* Functions for mutex handling.  */

/* Initialize MUTEX using attributes in *MUTEX_ATTR, or use the
   default values if later is NULL.  */
extern int pthread_mutex_init (pthread_mutex_t *__restrict __mutex,
			       __const pthread_mutexattr_t *__restrict
			       __mutex_attr) __THROW;

/* Destroy MUTEX.  */
extern int pthread_mutex_destroy (pthread_mutex_t *__mutex) __THROW;

/* Try to lock MUTEX.  */
extern int pthread_mutex_trylock (pthread_mutex_t *__mutex) __THROW;

/* Wait until lock for MUTEX becomes available and lock it.  */
extern int pthread_mutex_lock (pthread_mutex_t *__mutex) __THROW;

#ifdef __USE_XOPEN2K
/* Wait until lock becomes available, or specified time passes. */
extern int pthread_mutex_timedlock (pthread_mutex_t *__restrict __mutex,
				    __const struct timespec *__restrict
				    __abstime) __THROW;
#endif

/* Unlock MUTEX.  */
extern int pthread_mutex_unlock (pthread_mutex_t *__mutex) __THROW;


/* Functions for handling mutex attributes.  */

/* Initialize mutex attribute object ATTR with default attributes
   (kind is PTHREAD_MUTEX_TIMED_NP).  */
extern int pthread_mutexattr_init (pthread_mutexattr_t *__attr) __THROW;

/* Destroy mutex attribute object ATTR.  */
extern int pthread_mutexattr_destroy (pthread_mutexattr_t *__attr) __THROW;

/* Get the process-shared flag of the mutex attribute ATTR.  */
extern int pthread_mutexattr_getpshared (__const pthread_mutexattr_t *
					 __restrict __attr,
					 int *__restrict __pshared) __THROW;

/* Set the process-shared flag of the mutex attribute ATTR.  */
extern int pthread_mutexattr_setpshared (pthread_mutexattr_t *__attr,
					 int __pshared) __THROW;

#ifdef __USE_UNIX98
/* Set the mutex kind attribute in *ATTR to KIND (either PTHREAD_MUTEX_NORMAL,
   PTHREAD_MUTEX_RECURSIVE, PTHREAD_MUTEX_ERRORCHECK, or
   PTHREAD_MUTEX_DEFAULT).  */
extern int pthread_mutexattr_settype (pthread_mutexattr_t *__attr, int __kind)
     __THROW;

/* Return in *KIND the mutex kind attribute in *ATTR.  */
extern int pthread_mutexattr_gettype (__const pthread_mutexattr_t *__restrict
				      __attr, int *__restrict __kind) __THROW;
#endif


/* Functions for handling conditional variables.  */

/* Initialize condition variable COND using attributes ATTR, or use
   the default values if later is NULL.  */
extern int pthread_cond_init (pthread_cond_t *__restrict __cond,
			      __const pthread_condattr_t *__restrict
			      __cond_attr) __THROW;

/* Destroy condition variable COND.  */
extern int pthread_cond_destroy (pthread_cond_t *__cond) __THROW;

/* Wake up one thread waiting for condition variable COND.  */
extern int pthread_cond_signal (pthread_cond_t *__cond) __THROW;

/* Wake up all threads waiting for condition variables COND.  */
extern int pthread_cond_broadcast (pthread_cond_t *__cond) __THROW;

/* Wait for condition variable COND to be signaled or broadcast.
   MUTEX is assumed to be locked before.  */
extern int pthread_cond_wait (pthread_cond_t *__restrict __cond,
			      pthread_mutex_t *__restrict __mutex);

/* Wait for condition variable COND to be signaled or broadcast until
   ABSTIME.  MUTEX is assumed to be locked before.  ABSTIME is an
   absolute time specification; zero is the beginning of the epoch
   (00:00:00 GMT, January 1, 1970).  */
extern int pthread_cond_timedwait (pthread_cond_t *__restrict __cond,
				   pthread_mutex_t *__restrict __mutex,
				   __const struct timespec *__restrict
				   __abstime);

/* Functions for handling condition variable attributes.  */

/* Initialize condition variable attribute ATTR.  */
extern int pthread_condattr_init (pthread_condattr_t *__attr) __THROW;

/* Destroy condition variable attribute ATTR.  */
extern int pthread_condattr_destroy (pthread_condattr_t *__attr) __THROW;

/* Get the process-shared flag of the condition variable attribute ATTR.  */
extern int pthread_condattr_getpshared (__const pthread_condattr_t *
					__restrict __attr,
					int *__restrict __pshared) __THROW;

/* Set the process-shared flag of the condition variable attribute ATTR.  */
extern int pthread_condattr_setpshared (pthread_condattr_t *__attr,
					int __pshared) __THROW;


#if defined __USE_UNIX98 || defined __USE_XOPEN2K
/* Functions for handling read-write locks.  */

/* Initialize read-write lock RWLOCK using attributes ATTR, or use
   the default values if later is NULL.  */
extern int pthread_rwlock_init (pthread_rwlock_t *__restrict __rwlock,
				__const pthread_rwlockattr_t *__restrict
				__attr) __THROW;

/* Destroy read-write lock RWLOCK.  */
extern int pthread_rwlock_destroy (pthread_rwlock_t *__rwlock) __THROW;

/* Acquire read lock for RWLOCK.  */
extern int pthread_rwlock_rdlock (pthread_rwlock_t *__rwlock) __THROW;

/* Try to acquire read lock for RWLOCK.  */
extern int pthread_rwlock_tryrdlock (pthread_rwlock_t *__rwlock) __THROW;

# ifdef __USE_XOPEN2K
/* Try to acquire read lock for RWLOCK or return after specfied time.  */
extern int pthread_rwlock_timedrdlock (pthread_rwlock_t *__restrict __rwlock,
				       __const struct timespec *__restrict
				       __abstime) __THROW;
# endif

/* Acquire write lock for RWLOCK.  */
extern int pthread_rwlock_wrlock (pthread_rwlock_t *__rwlock) __THROW;

/* Try to acquire write lock for RWLOCK.  */
extern int pthread_rwlock_trywrlock (pthread_rwlock_t *__rwlock) __THROW;

# ifdef __USE_XOPEN2K
/* Try to acquire write lock for RWLOCK or return after specfied time.  */
extern int pthread_rwlock_timedwrlock (pthread_rwlock_t *__restrict __rwlock,
				       __const struct timespec *__restrict
				       __abstime) __THROW;
# endif

/* Unlock RWLOCK.  */
extern int pthread_rwlock_unlock (pthread_rwlock_t *__rwlock) __THROW;


/* Functions for handling read-write lock attributes.  */

/* Initialize attribute object ATTR with default values.  */
extern int pthread_rwlockattr_init (pthread_rwlockattr_t *__attr) __THROW;

/* Destroy attribute object ATTR.  */
extern int pthread_rwlockattr_destroy (pthread_rwlockattr_t *__attr) __THROW;

/* Return current setting of process-shared attribute of ATTR in PSHARED.  */
extern int pthread_rwlockattr_getpshared (__const pthread_rwlockattr_t *
					  __restrict __attr,
					  int *__restrict __pshared) __THROW;

/* Set process-shared attribute of ATTR to PSHARED.  */
extern int pthread_rwlockattr_setpshared (pthread_rwlockattr_t *__attr,
					  int __pshared) __THROW;

/* Return current setting of reader/writer preference.  */
extern int pthread_rwlockattr_getkind_np (__const pthread_rwlockattr_t *__attr,
					  int *__pref) __THROW;

/* Set reader/write preference.  */
extern int pthread_rwlockattr_setkind_np (pthread_rwlockattr_t *__attr,
					  int __pref) __THROW;
#endif

#if 0
/* Not yet implemented in uClibc! */

#ifdef __USE_XOPEN2K
/* The IEEE Std. 1003.1j-2000 introduces functions to implement
   spinlocks.  */

/* Initialize the spinlock LOCK.  If PSHARED is nonzero the spinlock can
   be shared between different processes.  */
extern int pthread_spin_init (pthread_spinlock_t *__lock, int __pshared)
     __THROW;

/* Destroy the spinlock LOCK.  */
extern int pthread_spin_destroy (pthread_spinlock_t *__lock) __THROW;

/* Wait until spinlock LOCK is retrieved.  */
extern int pthread_spin_lock (pthread_spinlock_t *__lock) __THROW;

/* Try to lock spinlock LOCK.  */
extern int pthread_spin_trylock (pthread_spinlock_t *__lock) __THROW;

/* Release spinlock LOCK.  */
extern int pthread_spin_unlock (pthread_spinlock_t *__lock) __THROW;


/* Barriers are a also a new feature in 1003.1j-2000. */

extern int pthread_barrier_init (pthread_barrier_t *__restrict __barrier,
				 __const pthread_barrierattr_t *__restrict
				 __attr, unsigned int __count) __THROW;

extern int pthread_barrier_destroy (pthread_barrier_t *__barrier) __THROW;

extern int pthread_barrierattr_init (pthread_barrierattr_t *__attr) __THROW;

extern int pthread_barrierattr_destroy (pthread_barrierattr_t *__attr) __THROW;

extern int pthread_barrierattr_getpshared (__const pthread_barrierattr_t *
					   __restrict __attr,
					   int *__restrict __pshared) __THROW;

extern int pthread_barrierattr_setpshared (pthread_barrierattr_t *__attr,
					   int __pshared) __THROW;

extern int pthread_barrier_wait (pthread_barrier_t *__barrier) __THROW;
#endif
#endif


/* Functions for handling thread-specific data.  */

/* Create a key value identifying a location in the thread-specific
   data area.  Each thread maintains a distinct thread-specific data
   area.  DESTR_FUNCTION, if non-NULL, is called with the value
   associated to that key when the key is destroyed.
   DESTR_FUNCTION is not called if the value associated is NULL when
   the key is destroyed.  */
extern int pthread_key_create (pthread_key_t *__key,
			       void (*__destr_function) (void *)) __THROW;

/* Destroy KEY.  */
extern int pthread_key_delete (pthread_key_t __key) __THROW;

/* Store POINTER in the thread-specific data slot identified by KEY. */
extern int pthread_setspecific (pthread_key_t __key,
				__const void *__pointer) __THROW;

/* Return current value of the thread-specific data slot identified by KEY.  */
extern void *pthread_getspecific (pthread_key_t __key) __THROW;


/* Functions for handling initialization.  */

/* Guarantee that the initialization function INIT_ROUTINE will be called
   only once, even if pthread_once is executed several times with the
   same ONCE_CONTROL argument. ONCE_CONTROL must point to a static or
   extern variable initialized to PTHREAD_ONCE_INIT.

   The initialization functions might throw exception which is why
   this function is not marked with __THROW.  */
extern int pthread_once (pthread_once_t *__once_control,
			 void (*__init_routine) (void));


/* Functions for handling cancellation.  */

/* Set cancelability state of current thread to STATE, returning old
   state in *OLDSTATE if OLDSTATE is not NULL.  */
extern int pthread_setcancelstate (int __state, int *__oldstate);

/* Set cancellation state of current thread to TYPE, returning the old
   type in *OLDTYPE if OLDTYPE is not NULL.  */
extern int pthread_setcanceltype (int __type, int *__oldtype);

/* Cancel THREAD immediately or at the next possibility.  */
extern int pthread_cancel (pthread_t __cancelthread);

/* Test for pending cancellation for the current thread and terminate
   the thread as per pthread_exit(PTHREAD_CANCELED) if it has been
   cancelled.  */
extern void pthread_testcancel (void);


/* Install a cleanup handler: ROUTINE will be called with arguments ARG
   when the thread is cancelled or calls pthread_exit.  ROUTINE will also
   be called with arguments ARG when the matching pthread_cleanup_pop
   is executed with non-zero EXECUTE argument.
   pthread_cleanup_push and pthread_cleanup_pop are macros and must always
   be used in matching pairs at the same nesting level of braces. */

#define pthread_cleanup_push(routine,arg) \
  { struct _pthread_cleanup_buffer _buffer;				      \
    _pthread_cleanup_push (&_buffer, (routine), (arg));

extern void _pthread_cleanup_push (struct _pthread_cleanup_buffer *__buffer,
				   void (*__routine) (void *),
				   void *__arg) __THROW;

/* Remove a cleanup handler installed by the matching pthread_cleanup_push.
   If EXECUTE is non-zero, the handler function is called. */

#define pthread_cleanup_pop(execute) \
    _pthread_cleanup_pop (&_buffer, (execute)); }

extern void _pthread_cleanup_pop (struct _pthread_cleanup_buffer *__buffer,
				  int __execute) __THROW;

/* Install a cleanup handler as pthread_cleanup_push does, but also
   saves the current cancellation type and set it to deferred cancellation.  */

#ifdef __USE_GNU
# define pthread_cleanup_push_defer_np(routine,arg) \
  { struct _pthread_cleanup_buffer _buffer;				      \
    _pthread_cleanup_push_defer (&_buffer, (routine), (arg));

extern void _pthread_cleanup_push_defer (struct _pthread_cleanup_buffer *__buffer,
					 void (*__routine) (void *),
					 void *__arg) __THROW;
extern void __pthread_cleanup_push_defer (struct _pthread_cleanup_buffer *__buffer,
					  void (*__routine) (void *),
					  void *__arg) __THROW;

/* Remove a cleanup handler as pthread_cleanup_pop does, but also
   restores the cancellation type that was in effect when the matching
   pthread_cleanup_push_defer was called.  */

# define pthread_cleanup_pop_restore_np(execute) \
  _pthread_cleanup_pop_restore (&_buffer, (execute)); }

extern void _pthread_cleanup_pop_restore (struct _pthread_cleanup_buffer *__buffer,
					  int __execute) __THROW;
extern void __pthread_cleanup_pop_restore (struct _pthread_cleanup_buffer *__buffer,
					   int __execute) __THROW;
#endif


#if 0
/* Not yet implemented in uClibc! */

#ifdef __USE_XOPEN2K
/* Get ID of CPU-time clock for thread THREAD_ID.  */
extern int pthread_getcpuclockid (pthread_t __thread_id,
				  clockid_t *__clock_id) __THROW;
#endif
#endif


/* Functions for handling signals.  */
#include <bits/sigthread.h>


/* Functions for handling process creation and process execution.  */

/* Install handlers to be called when a new process is created with FORK.
   The PREPARE handler is called in the parent process just before performing
   FORK. The PARENT handler is called in the parent process just after FORK.
   The CHILD handler is called in the child process.  Each of the three
   handlers can be NULL, meaning that no handler needs to be called at that
   point.
   PTHREAD_ATFORK can be called several times, in which case the PREPARE
   handlers are called in LIFO order (last added with PTHREAD_ATFORK,
   first called before FORK), and the PARENT and CHILD handlers are called
   in FIFO (first added, first called).  */

extern int pthread_atfork (void (*__prepare) (void),
			   void (*__parent) (void),
			   void (*__child) (void)) __THROW;

/* Terminate all threads in the program except the calling process.
   Should be called just before invoking one of the exec*() functions.  */

extern void pthread_kill_other_threads_np (void) __THROW;

__END_DECLS

#endif	/* pthread.h */
