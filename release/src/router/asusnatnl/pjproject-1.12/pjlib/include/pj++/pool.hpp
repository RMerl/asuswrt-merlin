/* $Id: pool.hpp 2394 2008-12-23 17:27:53Z bennylp $ */
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
#ifndef __PJPP_POOL_HPP__
#define __PJPP_POOL_HPP__

#include <pj/pool.h>

class Pj_Pool;
class Pj_Caching_Pool;

//
// Base class for all Pjlib objects
//
class Pj_Object
{
public:
    void *operator new(unsigned int class_size, Pj_Pool *pool);
    void *operator new(unsigned int class_size, Pj_Pool &pool);

    void operator delete(void*)
    {
    }

    void operator delete(void*, Pj_Pool*)
    {
    }

    void operator delete(void*, Pj_Pool&)
    {
    }

    //
    // Inline implementations at the end of this file.
    //

private:
    // Can not use normal new operator; must use pool.
    // e.g.:
    //   obj = new(pool) Pj_The_Object(pool, ...);
    //
    void *operator new(unsigned int)
    {}
};


//
// Pool.
//
class Pj_Pool : public Pj_Object
{
public:
    //
    // Default constructor, initializes internal pool to NULL.
    // Application must call attach() some time later.
    //
    Pj_Pool()
        : p_(NULL)
    {
    }

    //
    // Create pool.
    //
    Pj_Pool(Pj_Caching_Pool &caching_pool,
            pj_size_t initial_size, 
            pj_size_t increment_size, 
            const char *name = NULL, 
            pj_pool_callback *callback = NULL);

    //
    // Construct from existing pool.
    //
    explicit Pj_Pool(pj_pool_t *pool)
        : p_(pool)
    {
    }

    //
    // Attach existing pool.
    //
    void attach(pj_pool_t *pool)
    {
        p_ = pool;
    }

    //
    // Destructor.
    //
    // Release pool back to factory. Remember: if you delete pool, then 
    // make sure that all objects that have been allocated from this pool
    // have been properly destroyed.
    //
    // This is where C++ is trickier than plain C!!
    //
    ~Pj_Pool()
    {
        if (p_)
	    pj_pool_release(p_);
    }

    //
    // Get name.
    //
    const char *getobjname() const
    {
	return pj_pool_getobjname(p_);
    }

    //
    // You can cast Pj_Pool to pj_pool_t*
    //
    operator pj_pool_t*()
    {
	return p_;
    }

    //
    // Get pjlib compatible pool object.
    //
    pj_pool_t *pool_()
    {
	return p_;
    }

    //
    // Get pjlib compatible pool object.
    //
    const pj_pool_t *pool_() const
    {
	return p_;
    }

    //
    // Get pjlib compatible pool object.
    //
    pj_pool_t *pj_pool_t_()
    {
	return p_;
    }

    //
    // Reset pool.
    //
    void reset()
    {
	pj_pool_reset(p_);
    }

    //
    // Get current capacity.
    //
    pj_size_t get_capacity()
    {
	pj_pool_get_capacity(p_);
    }

    //
    // Get current total bytes allocated from the pool.
    //
    pj_size_t get_used_size()
    {
	pj_pool_get_used_size(p_);
    }

    //
    // Allocate.
    //
    void *alloc(pj_size_t size)
    {
	return pj_pool_alloc(p_, size);
    }

    //
    // Allocate elements and zero fill the memory.
    //
    void *calloc(pj_size_t count, pj_size_t elem)
    {
	return pj_pool_calloc(p_, count, elem);
    }

    //
    // Allocate and zero fill memory.
    //
    void *zalloc(pj_size_t size)
    {
        return pj_pool_zalloc(p_, size);
    }

private:
    pj_pool_t *p_;
};


//
// Caching pool.
//
class Pj_Caching_Pool
{
public:
    //
    // Construct caching pool.
    //
    Pj_Caching_Pool( pj_size_t cache_capacity = 0,
	             const pj_pool_factory_policy *pol=&pj_pool_factory_default_policy)
    {
	pj_caching_pool_init(&cp_, pol, cache_capacity);
    }

    //
    // Destroy caching pool.
    //
    ~Pj_Caching_Pool()
    {
	pj_caching_pool_destroy(&cp_);
    }

    //
    // Create pool.
    //
    pj_pool_t *create_pool( pj_size_t initial_size, 
                            pj_size_t increment_size, 
                            const char *name = NULL, 
                            pj_pool_callback *callback = NULL)
    {
	return (pj_pool_t*)(*cp_.factory.create_pool)(&cp_.factory, name, 
                                                     initial_size, 
                                                     increment_size, 
                                                     callback);
    }

private:
    pj_caching_pool cp_;
};

//
// Inlines for Pj_Object
//
inline void *Pj_Object::operator new(unsigned int class_size, Pj_Pool *pool)
{
    return pool->alloc(class_size);
}
inline void *Pj_Object::operator new(unsigned int class_size, Pj_Pool &pool)
{
    return pool.alloc(class_size);
}

//
// Inlines for Pj_Pool
//
inline Pj_Pool::Pj_Pool( Pj_Caching_Pool &caching_pool,
                         pj_size_t initial_size, 
                         pj_size_t increment_size, 
                         const char *name, 
                         pj_pool_callback *callback)
{
    p_ = caching_pool.create_pool(initial_size, increment_size, name,
                                  callback);
}


#endif	/* __PJPP_POOL_HPP__ */

