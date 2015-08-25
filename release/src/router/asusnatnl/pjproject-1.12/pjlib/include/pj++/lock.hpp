/* $Id: lock.hpp 2394 2008-12-23 17:27:53Z bennylp $  */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJPP_LOCK_HPP__
#define __PJPP_LOCK_HPP__

#include <pj++/types.hpp>
#include <pj/lock.h>
#include <pj++/pool.hpp>

//////////////////////////////////////////////////////////////////////////////
// Lock object.
//
class Pj_Lock : public Pj_Object
{
public:
    //
    // Constructor.
    //
    explicit Pj_Lock(pj_lock_t *lock)
        : lock_(lock)
    {
    }

    //
    // Destructor.
    //
    ~Pj_Lock()
    {
        if (lock_)
            pj_lock_destroy(lock_);
    }

    //
    // Get pjlib compatible lock object.
    //
    pj_lock_t *pj_lock_t_()
    {
        return lock_;
    }

    //
    // acquire lock.
    //
    pj_status_t acquire()
    {
        return pj_lock_acquire(lock_);
    }

    //
    // release lock,.
    //
    pj_status_t release()
    {
        return pj_lock_release(lock_);
    }

protected:
    pj_lock_t *lock_;
};


//////////////////////////////////////////////////////////////////////////////
// Null lock object.
//
class Pj_Null_Lock : public Pj_Lock
{
public:
    //
    // Default constructor.
    //
    explicit Pj_Null_Lock(Pj_Pool *pool, const char *name = NULL)
        : Pj_Lock(NULL)
    {
        pj_lock_create_null_mutex(pool->pool_(), name, &lock_);
    }
};

//////////////////////////////////////////////////////////////////////////////
// Simple mutex lock object.
//
class Pj_Simple_Mutex_Lock : public Pj_Lock
{
public:
    //
    // Default constructor.
    //
    explicit Pj_Simple_Mutex_Lock(Pj_Pool *pool, const char *name = NULL)
        : Pj_Lock(NULL)
    {
        pj_lock_create_simple_mutex(pool->pool_(), name, &lock_);
    }
};

//////////////////////////////////////////////////////////////////////////////
// Recursive mutex lock object.
//
class Pj_Recursive_Mutex_Lock : public Pj_Lock
{
public:
    //
    // Default constructor.
    //
    explicit Pj_Recursive_Mutex_Lock(Pj_Pool *pool, const char *name = NULL)
        : Pj_Lock(NULL)
    {
        pj_lock_create_recursive_mutex(pool->pool_(), name, &lock_);
    }
};

//////////////////////////////////////////////////////////////////////////////
// Semaphore lock object.
//
class Pj_Semaphore_Lock : public Pj_Lock
{
public:
    //
    // Default constructor.
    //
    explicit Pj_Semaphore_Lock(Pj_Pool *pool, 
                               unsigned max=PJ_MAXINT32,
                               unsigned initial=0,
                               const char *name=NULL)
        : Pj_Lock(NULL)
    {
        pj_lock_create_semaphore(pool->pool_(), name, initial, max, &lock_);
    }
};



#endif	/* __PJPP_LOCK_HPP__ */

