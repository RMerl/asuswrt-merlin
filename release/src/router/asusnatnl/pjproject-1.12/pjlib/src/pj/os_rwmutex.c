/* $Id: os_rwmutex.c 3553 2011-05-05 06:14:19Z nanang $ */
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

/* 
 * Note: 
 * 
 * DO NOT BUILD THIS FILE DIRECTLY. THIS FILE WILL BE INCLUDED BY os_core_*.c
 * WHEN MACRO PJ_EMULATE_RWMUTEX IS SET.
 */

/*
 * os_rwmutex.c:
 *
 * Implementation of Read-Write mutex for platforms that lack it (e.g.
 * Win32, RTEMS).
 */


struct pj_rwmutex_t
{
    pj_mutex_t *read_lock;
    /* write_lock must use semaphore, because write_lock may be released
     * by thread other than the thread that acquire the write_lock in the
     * first place.
     */
    pj_sem_t   *write_lock;
    pj_int32_t  reader_count;
	int inst_id;
};

/*
 * Create reader/writer mutex.
 *
 */
PJ_DEF(pj_status_t) pj_rwmutex_create(pj_pool_t *pool, const char *name,
				      pj_rwmutex_t **p_mutex)
{
    pj_status_t status;
    pj_rwmutex_t *rwmutex;

    PJ_ASSERT_RETURN(pool && p_mutex, PJ_EINVAL);

    *p_mutex = NULL;
    rwmutex = PJ_POOL_ALLOC_T(pool, pj_rwmutex_t);
	rwmutex->inst_id = pool->factory->inst_id;

    status = pj_mutex_create_simple(pool, name, &rwmutex ->read_lock);
    if (status != PJ_SUCCESS)
	return status;

    status = pj_sem_create(pool, name, 1, 1, &rwmutex->write_lock);
    if (status != PJ_SUCCESS) {
	pj_mutex_destroy(rwmutex->read_lock);
	return status;
    }

    rwmutex->reader_count = 0;
    *p_mutex = rwmutex;
    return PJ_SUCCESS;
}

/*
 * Lock the mutex for reading.
 *
 */
PJ_DEF(pj_status_t) pj_rwmutex_lock_read(pj_rwmutex_t *mutex)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(mutex, PJ_EINVAL);

    status = pj_mutex_lock(mutex->read_lock);
    if (status != PJ_SUCCESS) {
	pj_assert(!"This pretty much is unexpected");
	return status;
    }

    mutex->reader_count++;

    pj_assert(mutex->reader_count < 0x7FFFFFF0L);

    if (mutex->reader_count == 1)
	pj_sem_wait(mutex->write_lock);

    status = pj_mutex_unlock(mutex->read_lock);
    return status;
}

/*
 * Lock the mutex for writing.
 *
 */
PJ_DEF(pj_status_t) pj_rwmutex_lock_write(pj_rwmutex_t *mutex)
{
    PJ_ASSERT_RETURN(mutex, PJ_EINVAL);
    return pj_sem_wait(mutex->write_lock);
}

/*
 * Release read lock.
 *
 */
PJ_DEF(pj_status_t) pj_rwmutex_unlock_read(pj_rwmutex_t *mutex)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(mutex, PJ_EINVAL);

    status = pj_mutex_lock(mutex->read_lock);
    if (status != PJ_SUCCESS) {
	pj_assert(!"This pretty much is unexpected");
	return status;
    }

    pj_assert(mutex->reader_count >= 1);

    --mutex->reader_count;
    if (mutex->reader_count == 0)
	pj_sem_post(mutex->write_lock);

    status = pj_mutex_unlock(mutex->read_lock);
    return status;
}

/*
 * Release write lock.
 *
 */
PJ_DEF(pj_status_t) pj_rwmutex_unlock_write(pj_rwmutex_t *mutex)
{
    PJ_ASSERT_RETURN(mutex, PJ_EINVAL);
    pj_assert(mutex->reader_count <= 1);
    return pj_sem_post(mutex->write_lock);
}


/*
 * Destroy reader/writer mutex.
 *
 */
PJ_DEF(pj_status_t) pj_rwmutex_destroy(pj_rwmutex_t *mutex)
{
    PJ_ASSERT_RETURN(mutex, PJ_EINVAL);
    pj_mutex_destroy(mutex->read_lock);
    pj_sem_destroy(mutex->write_lock);
    return PJ_SUCCESS;
}

