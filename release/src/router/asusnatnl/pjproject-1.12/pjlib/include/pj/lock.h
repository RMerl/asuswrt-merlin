/* $Id: lock.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJ_LOCK_H__
#define __PJ_LOCK_H__

/**
 * @file lock.h
 * @brief Higher abstraction for locking objects.
 */
#include <pj/types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_LOCK Lock Objects
 * @ingroup PJ_OS
 * @{
 *
 * <b>Lock Objects</b> are higher abstraction for different lock mechanisms.
 * It offers the same API for manipulating different lock types (e.g.
 * @ref PJ_MUTEX "mutex", @ref PJ_SEM "semaphores", or null locks).
 * Because Lock Objects have the same API for different types of lock
 * implementation, it can be passed around in function arguments. As the
 * result, it can be used to control locking policy for  a particular
 * feature.
 */


/**
 * Create simple, non recursive mutex lock object.
 *
 * @param pool	    Memory pool.
 * @param name	    Lock object's name.
 * @param lock	    Pointer to store the returned handle.
 *
 * @return	    PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_lock_create_simple_mutex( pj_pool_t *pool,
						  const char *name,
						  pj_lock_t **lock );

/**
 * Create recursive mutex lock object.
 *
 * @param pool	    Memory pool.
 * @param name	    Lock object's name.
 * @param lock	    Pointer to store the returned handle.
 *
 * @return	    PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_lock_create_recursive_mutex( pj_pool_t *pool,
						     const char *name,
						     pj_lock_t **lock );


/**
 * Create NULL mutex. A NULL mutex doesn't actually have any synchronization
 * object attached to it.
 *
 * @param pool	    Memory pool.
 * @param name	    Lock object's name.
 * @param lock	    Pointer to store the returned handle.
 *
 * @return	    PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_lock_create_null_mutex( pj_pool_t *pool,
						const char *name,
						pj_lock_t **lock );


#if defined(PJ_HAS_SEMAPHORE) && PJ_HAS_SEMAPHORE != 0
/**
 * Create semaphore lock object.
 *
 * @param pool	    Memory pool.
 * @param name	    Lock object's name.
 * @param initial   Initial value of the semaphore.
 * @param max	    Maximum value of the semaphore.
 * @param lock	    Pointer to store the returned handle.
 *
 * @return	    PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_lock_create_semaphore( pj_pool_t *pool,
					       const char *name,
					       unsigned initial,
					       unsigned max,
					       pj_lock_t **lock );

#endif	/* PJ_HAS_SEMAPHORE */

/**
 * Acquire lock on the specified lock object.
 *
 * @param lock	    The lock object.
 *
 * @return	    PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_lock_acquire( pj_lock_t *lock );


/**
 * Try to acquire lock on the specified lock object.
 *
 * @param lock	    The lock object.
 *
 * @return	    PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_lock_tryacquire( pj_lock_t *lock );


/**
 * Release lock on the specified lock object.
 *
 * @param lock	    The lock object.
 *
 * @return	    PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_lock_release( pj_lock_t *lock );


/**
 * Destroy the lock object.
 *
 * @param lock	    The lock object.
 *
 * @return	    PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_lock_destroy( pj_lock_t *lock );


/** @} */


/**
 * @defgroup PJ_GRP_LOCK Group Lock
 * @ingroup PJ_LOCK
 * @{
 *
 * Group lock is a synchronization object to manage concurrency among members
 * within the same logical group. Example of such groups are:
 *
 *   - dialog, which has members such as the dialog itself, an invite session,
 *     and several transactions
 *   - ICE, which has members such as ICE stream transport, ICE session, STUN
 *     socket, TURN socket, and down to ioqueue key
 *
 * Group lock has three functions:
 *
 *   - mutual exclusion: to protect resources from being accessed by more than
 *     one threads at the same time
 *   - session management: to make sure that the resource is not destroyed
 *     while others are still using or about to use it.
 *   - lock coordinator: to provide uniform lock ordering among more than one
 *     lock objects, which is necessary to avoid deadlock.
 *
 * The requirements of the group lock are:
 *
 *    - must satisfy all the functions above
 *    - must allow members to join or leave the group (for example,
 *      transaction may be added or removed from a dialog)
 *    - must be able to synchronize with external lock (for example, a dialog
 *      lock must be able to sync itself with PJSUA lock)
 *
 * Please see https://trac.pjsip.org/repos/wiki/Group_Lock for more info.
 */

/**
 * Settings for creating the group lock.
 */
typedef struct pj_grp_lock_config
{
    /**
     * Creation flags, currently must be zero.
     */
    unsigned	flags;

} pj_grp_lock_config;


/**
 * Initialize the config with the default values.
 *
 * @param cfg		The config to be initialized.
 */
PJ_DECL(void) pj_grp_lock_config_default(pj_grp_lock_config *cfg);

/**
 * Create a group lock object. Initially the group lock will have reference
 * counter of one.
 *
 * @param pool		The group lock only uses the pool parameter to get
 * 			the pool factory, from which it will create its own
 * 			pool.
 * @param cfg		Optional configuration.
 * @param p_grp_lock	Pointer to receive the newly created group lock.
 *
 * @return		PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_grp_lock_create(pj_pool_t *pool,
                                        const pj_grp_lock_config *cfg,
                                        pj_grp_lock_t **p_grp_lock);

/**
 * Create a group lock object, with the specified destructor handler, to be
 * called by the group lock when it is about to be destroyed. Initially the
 * group lock will have reference counter of one.
 *
 * @param pool		The group lock only uses the pool parameter to get
 * 			the pool factory, from which it will create its own
 * 			pool.
 * @param cfg		Optional configuration.
 * @param member	A pointer to be passed to the handler.
 * @param handler	The destroy handler.
 * @param p_grp_lock	Pointer to receive the newly created group lock.
 *
 * @return		PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_grp_lock_create_w_handler(pj_pool_t *pool,
                                        	  const pj_grp_lock_config *cfg,
                                        	  void *member,
                                                  void (*handler)(void *member),
                                        	  pj_grp_lock_t **p_grp_lock);

/**
 * Forcibly destroy the group lock, ignoring the reference counter value.
 *
 * @param grp_lock	The group lock.
 *
 * @return		PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_grp_lock_destroy( pj_grp_lock_t *grp_lock);

/**
 * Move the contents of the old lock to the new lock and destroy the
 * old lock.
 *
 * @param old_lock	The old group lock to be destroyed.
 * @param new_lock	The new group lock.
 *
 * @return		PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_grp_lock_replace(pj_grp_lock_t *old_lock,
                                         pj_grp_lock_t *new_lock);

/**
 * Acquire lock on the specified group lock.
 *
 * @param grp_lock	The group lock.
 *
 * @return		PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_grp_lock_acquire( pj_grp_lock_t *grp_lock);

/**
 * Acquire lock on the specified group lock if it is available, otherwise
 * return immediately wihout waiting.
 *
 * @param grp_lock	The group lock.
 *
 * @return		PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_grp_lock_tryacquire( pj_grp_lock_t *grp_lock);

/**
 * Release the previously held lock. This may cause the group lock
 * to be destroyed if it is the last one to hold the reference counter.
 * In that case, the function will return PJ_EGONE.
 *
 * @param grp_lock	The group lock.
 *
 * @return		PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_grp_lock_release( pj_grp_lock_t *grp_lock);

/**
 * Add a destructor handler, to be called by the group lock when it is
 * about to be destroyed.
 *
 * @param grp_lock	The group lock.
 * @param pool		Pool to allocate memory for the handler.
 * @param member	A pointer to be passed to the handler.
 * @param handler	The destroy handler.
 *
 * @return		PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_grp_lock_add_handler(pj_grp_lock_t *grp_lock,
                                             pj_pool_t *pool,
                                             void *member,
                                             void (*handler)(void *member));

/**
 * Remove previously registered handler. All parameters must be the same
 * as when the handler was added.
 *
 * @param grp_lock	The group lock.
 * @param member	A pointer to be passed to the handler.
 * @param handler	The destroy handler.
 *
 * @return		PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_grp_lock_del_handler(pj_grp_lock_t *grp_lock,
                                             void *member,
                                             void (*handler)(void *member));

/**
 * Increment reference counter to prevent the group lock grom being destroyed.
 *
 * @param grp_lock	The group lock.
 *
 * @return		PJ_SUCCESS or the appropriate error code.
 */
#if !PJ_GRP_LOCK_DEBUG
PJ_DECL(pj_status_t) pj_grp_lock_add_ref(pj_grp_lock_t *grp_lock);

#define pj_grp_lock_add_ref_dbg(grp_lock, x, y) pj_grp_lock_add_ref(grp_lock)

#else

#define pj_grp_lock_add_ref(g)	pj_grp_lock_add_ref_dbg(g, __FILE__, __LINE__)

PJ_DECL(pj_status_t) pj_grp_lock_add_ref_dbg(pj_grp_lock_t *grp_lock,
                                             const char *file,
                                             int line);
#endif

/**
 * Decrement the reference counter. When the counter value reaches zero, the
 * group lock will be destroyed and all destructor handlers will be called.
 *
 * @param grp_lock	The group lock.
 *
 * @return		PJ_SUCCESS or the appropriate error code.
 */
#if !PJ_GRP_LOCK_DEBUG
PJ_DECL(pj_status_t) pj_grp_lock_dec_ref(pj_grp_lock_t *grp_lock);

#define pj_grp_lock_dec_ref_dbg(grp_lock, x, y) pj_grp_lock_dec_ref(grp_lock)
#else

#define pj_grp_lock_dec_ref(g)	pj_grp_lock_dec_ref_dbg(g, __FILE__, __LINE__)

PJ_DECL(pj_status_t) pj_grp_lock_dec_ref_dbg(pj_grp_lock_t *grp_lock,
                                             const char *file,
                                             int line);

#endif

/**
 * Get current reference count value. This normally is only used for
 * debugging purpose.
 *
 * @param grp_lock	The group lock.
 *
 * @return		The reference count value.
 */
PJ_DECL(int) pj_grp_lock_get_ref(pj_grp_lock_t *grp_lock);


/**
 * Dump group lock info for debugging purpose. If group lock debugging is
 * enabled (via PJ_GRP_LOCK_DEBUG) macro, this will print the group lock
 * reference counter value along with the source file and line. If
 * debugging is disabled, this will only print the reference counter.
 *
 * @param grp_lock	The group lock.
 */
PJ_DECL(void) pj_grp_lock_dump(pj_grp_lock_t *grp_lock);


/**
 * Synchronize an external lock with the group lock, by adding it to the
 * list of locks to be acquired by the group lock when the group lock is
 * acquired.
 *
 * The ''pos'' argument specifies the lock order and also the relative
 * position with regard to lock ordering against the group lock. Locks with
 * lower ''pos'' value will be locked first, and those with negative value
 * will be locked before the group lock (the group lock's ''pos'' value is
 * zero).
 *
 * @param grp_lock	The group lock.
 * @param ext_lock	The external lock
 * @param pos		The position.
 *
 * @return		PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_grp_lock_chain_lock(pj_grp_lock_t *grp_lock,
                                            pj_lock_t *ext_lock,
                                            int pos);

/**
 * Remove an external lock from group lock's list of synchronized locks.
 *
 * @param grp_lock	The group lock.
 * @param ext_lock	The external lock
 *
 * @return		PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_grp_lock_unchain_lock(pj_grp_lock_t *grp_lock,
                                              pj_lock_t *ext_lock);


/** @} */


PJ_END_DECL


#endif	/* __PJ_LOCK_H__ */

