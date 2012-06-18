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

#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H	1

#include <features.h>
#include <sys/types.h>
#ifdef __USE_XOPEN2K
# define __need_timespec
# include <time.h>
#endif

#ifndef _PTHREAD_DESCR_DEFINED
/* Thread descriptors.  Needed for `sem_t' definition.  */
typedef struct _pthread_descr_struct *_pthread_descr;
# define _PTHREAD_DESCR_DEFINED
#endif

/* System specific semaphore definition.  */
typedef struct
{
  struct _pthread_fastlock __sem_lock;
  int __sem_value;
  _pthread_descr __sem_waiting;
} sem_t;



/* Value returned if `sem_open' failed.  */
#define SEM_FAILED	((sem_t *) 0)

/* Maximum value the semaphore can have.  */
#define SEM_VALUE_MAX 	((int) ((~0u) >> 1))


__BEGIN_DECLS

/* Initialize semaphore object SEM to VALUE.  If PSHARED then share it
   with other processes.  */
extern int sem_init (sem_t *__sem, int __pshared, unsigned int __value) __THROW;

/* Free resources associated with semaphore object SEM.  */
extern int sem_destroy (sem_t *__sem) __THROW;

/* Open a named semaphore NAME with open flags OFLAG.  */
extern sem_t *sem_open (__const char *__name, int __oflag, ...) __THROW;

/* Close descriptor for named semaphore SEM.  */
extern int sem_close (sem_t *__sem) __THROW;

/* Remove named semaphore NAME.  */
extern int sem_unlink (__const char *__name) __THROW;

/* Wait for SEM being posted.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
extern int sem_wait (sem_t *__sem);

#ifdef __USE_XOPEN2K
/* Similar to `sem_wait' but wait only until ABSTIME.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
extern int sem_timedwait (sem_t *__restrict __sem,
			  __const struct timespec *__restrict __abstime);
#endif

/* Test whether SEM is posted.  */
extern int sem_trywait (sem_t *__sem) __THROW;

/* Post SEM.  */
extern int sem_post (sem_t *__sem) __THROW;

/* Get current value of SEM and store it in *SVAL.  */
extern int sem_getvalue (sem_t *__restrict __sem, int *__restrict __sval)
     __THROW;


__END_DECLS

#endif	/* semaphore.h */
