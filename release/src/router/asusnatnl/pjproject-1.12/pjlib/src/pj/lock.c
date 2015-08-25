/* $Id: lock.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pj/lock.h>
#include <pj/os.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/errno.h>


typedef void LOCK_OBJ;

/*
 * Lock structure.
 */
struct pj_lock_t
{
    LOCK_OBJ *lock_object;

    pj_status_t	(*acquire)	(LOCK_OBJ*);
    pj_status_t	(*tryacquire)	(LOCK_OBJ*);
    pj_status_t	(*release)	(LOCK_OBJ*);
    pj_status_t	(*destroy)	(LOCK_OBJ*);
};

typedef pj_status_t (*FPTR)(LOCK_OBJ*);

/******************************************************************************
 * Implementation of lock object with mutex.
 */
static pj_lock_t mutex_lock_template = 
{
    NULL,
    (FPTR) &pj_mutex_lock,
    (FPTR) &pj_mutex_trylock,
    (FPTR) &pj_mutex_unlock,
    (FPTR) &pj_mutex_destroy
};

static pj_status_t create_mutex_lock( pj_pool_t *pool,
				      const char *name,
				      int type,
				      pj_lock_t **lock )
{
    pj_lock_t *p_lock;
    pj_mutex_t *mutex;
    pj_status_t rc;

    PJ_ASSERT_RETURN(pool && lock, PJ_EINVAL);

    p_lock = PJ_POOL_ALLOC_T(pool, pj_lock_t);
    if (!p_lock)
	return PJ_ENOMEM;

    pj_memcpy(p_lock, &mutex_lock_template, sizeof(pj_lock_t));
    rc = pj_mutex_create(pool, name, type, &mutex);
    if (rc != PJ_SUCCESS)
	return rc;

    p_lock->lock_object = mutex;
    *lock = p_lock;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_lock_create_simple_mutex( pj_pool_t *pool,
						 const char *name,
						 pj_lock_t **lock )
{
    return create_mutex_lock(pool, name, PJ_MUTEX_SIMPLE, lock);
}

PJ_DEF(pj_status_t) pj_lock_create_recursive_mutex( pj_pool_t *pool,
						    const char *name,
						    pj_lock_t **lock )
{
    return create_mutex_lock(pool, name, PJ_MUTEX_RECURSE, lock);
}


/******************************************************************************
 * Implementation of NULL lock object.
 */
static pj_status_t null_op(void *arg)
{
    PJ_UNUSED_ARG(arg);
    return PJ_SUCCESS;
}

static pj_lock_t null_lock_template = 
{
    NULL,
    &null_op,
    &null_op,
    &null_op,
    &null_op
};

PJ_DEF(pj_status_t) pj_lock_create_null_mutex( pj_pool_t *pool,
					       const char *name,
					       pj_lock_t **lock )
{
    PJ_UNUSED_ARG(name);
    PJ_UNUSED_ARG(pool);

    PJ_ASSERT_RETURN(lock, PJ_EINVAL);

    *lock = &null_lock_template;
    return PJ_SUCCESS;
}


/******************************************************************************
 * Implementation of semaphore lock object.
 */
#if defined(PJ_HAS_SEMAPHORE) && PJ_HAS_SEMAPHORE != 0

static pj_lock_t sem_lock_template = 
{
    NULL,
    (FPTR) &pj_sem_wait,
    (FPTR) &pj_sem_trywait,
    (FPTR) &pj_sem_post,
    (FPTR) &pj_sem_destroy
};

PJ_DEF(pj_status_t) pj_lock_create_semaphore(  pj_pool_t *pool,
					       const char *name,
					       unsigned initial,
					       unsigned max,
					       pj_lock_t **lock )
{
    pj_lock_t *p_lock;
    pj_sem_t *sem;
    pj_status_t rc;

    PJ_ASSERT_RETURN(pool && lock, PJ_EINVAL);

    p_lock = PJ_POOL_ALLOC_T(pool, pj_lock_t);
    if (!p_lock)
	return PJ_ENOMEM;

    pj_memcpy(p_lock, &sem_lock_template, sizeof(pj_lock_t));
    rc = pj_sem_create( pool, name, initial, max, &sem);
    if (rc != PJ_SUCCESS)
        return rc;

    p_lock->lock_object = sem;
    *lock = p_lock;

    return PJ_SUCCESS;
}


#endif	/* PJ_HAS_SEMAPHORE */


PJ_DEF(pj_status_t) pj_lock_acquire( pj_lock_t *lock )
{
    PJ_ASSERT_RETURN(lock != NULL, PJ_EINVAL);
    return (*lock->acquire)(lock->lock_object);
}

PJ_DEF(pj_status_t) pj_lock_tryacquire( pj_lock_t *lock )
{
    PJ_ASSERT_RETURN(lock != NULL, PJ_EINVAL);
    return (*lock->tryacquire)(lock->lock_object);
}

PJ_DEF(pj_status_t) pj_lock_release( pj_lock_t *lock )
{
    PJ_ASSERT_RETURN(lock != NULL, PJ_EINVAL);
    return (*lock->release)(lock->lock_object);
}

PJ_DEF(pj_status_t) pj_lock_destroy( pj_lock_t *lock )
{
    PJ_ASSERT_RETURN(lock != NULL, PJ_EINVAL);
    return (*lock->destroy)(lock->lock_object);
}

