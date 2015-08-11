/* $Id: pool_buf.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __POOL_STACK_H__
#define __POOL_STACK_H__

#include <pj/pool.h>

/**
 * @defgroup PJ_POOL_BUFFER Stack/Buffer Based Memory Pool Allocator
 * @ingroup PJ_POOL_GROUP
 * @brief Stack/buffer based pool.
 *
 * This section describes an implementation of memory pool which uses
 * memory allocated from the stack. Application creates this pool
 * by specifying a buffer (which can be allocated from static memory or
 * stack variable), and then use normal pool API to access/use the pool.
 *
 * If the buffer specified during pool creation is a buffer located in the
 * stack, the pool will be invalidated (or implicitly destroyed) when the
 * execution leaves the enclosing block containing the buffer. Note
 * that application must make sure that any objects allocated from this
 * pool (such as mutexes) have been destroyed before the pool gets
 * invalidated.
 *
 * Sample usage:
 *
 * \code
  #include <pjlib.h>

  static void test()
  {
    char buffer[500];
    pj_pool_t *pool;
    void *p;

    pool = pj_pool_create_on_buf("thepool", buffer, sizeof(buffer));

    // Use the pool as usual
    p = pj_pool_alloc(pool, ...);
    ...

    // No need to release the pool
  }

  int main()
  {
    pj_init();
    test();
    return 0;
  }

   \endcode
 *
 * @{
 */

PJ_BEGIN_DECL

/**
 * Create the pool using the specified buffer as the pool's memory.
 * Subsequent allocations made from the pool will use the memory from
 * this buffer.
 *
 * If the buffer specified in the parameter is a buffer located in the
 * stack, the pool will be invalid (or implicitly destroyed) when the
 * execution leaves the enclosing block containing the buffer. Note
 * that application must make sure that any objects allocated from this
 * pool (such as mutexes) have been destroyed before the pool gets
 * invalidated.
 *
 * @param inst_id   The instance id of pjsua.
 * @param name	    Optional pool name.
 * @param buf	    Buffer to be used by the pool.
 * @param size	    The size of the buffer.
 *
 * @return	    The memory pool instance.
 */
PJ_DECL(pj_pool_t*) pj_pool_create_on_buf(int ins_id,
					  const char *name,
					  void *buf,
					  pj_size_t size);

PJ_END_DECL

/**
 * @}
 */

#endif	/* __POOL_STACK_H__ */

