/* $Id: timer.h 3034 2009-12-16 13:30:34Z bennylp $ */
/* 
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

#ifndef __PJ_TIMER_H__
#define __PJ_TIMER_H__

/**
 * @file timer.h
 * @brief Timer Heap
 */

#include <pj/types.h>
#include <pj/lock.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_TIMER Timer Heap Management.
 * @ingroup PJ_MISC
 * @brief
 * The timer scheduling implementation here is based on ACE library's 
 * ACE_Timer_Heap, with only little modification to suit our library's style
 * (I even left most of the comments in the original source).
 *
 * To quote the original quote in ACE_Timer_Heap_T class:
 *
 *      This implementation uses a heap-based callout queue of
 *      absolute times.  Therefore, in the average and worst case,
 *      scheduling, canceling, and expiring timers is O(log N) (where
 *      N is the total number of timers).  In addition, we can also
 *      preallocate as many \a ACE_Timer_Nodes as there are slots in
 *      the heap.  This allows us to completely remove the need for
 *      dynamic memory allocation, which is important for real-time
 *      systems.
 *
 * You can find the fine ACE library at:
 *  http://www.cs.wustl.edu/~schmidt/ACE.html
 *
 * ACE is Copyright (C)1993-2006 Douglas C. Schmidt <d.schmidt@vanderbilt.edu>
 *
 * @{
 *
 * \section pj_timer_examples_sec Examples
 *
 * For some examples on how to use the timer heap, please see the link below.
 *
 *  - \ref page_pjlib_timer_test
 */


/**
 * The type for internal timer ID.
 */
typedef int pj_timer_id_t;

/** 
 * Forward declaration for pj_timer_entry. 
 */
struct pj_timer_entry;

/**
 * The type of callback function to be called by timer scheduler when a timer
 * has expired.
 *
 * @param timer_heap    The timer heap.
 * @param entry         Timer entry which timer's has expired.
 */
typedef void pj_timer_heap_callback(pj_timer_heap_t *timer_heap,
				    struct pj_timer_entry *entry);


/**
 * This structure represents an entry to the timer.
 */
typedef struct pj_timer_entry
{
    /** 
     * User data to be associated with this entry. 
     * Applications normally will put the instance of object that
     * owns the timer entry in this field.
     */
    void *user_data;

    /** 
     * Arbitrary ID assigned by the user/owner of this entry. 
     * Applications can use this ID to distinguish multiple
     * timer entries that share the same callback and user_data.
     */
    int id;

    /** 
     * Callback to be called when the timer expires. 
     */
    pj_timer_heap_callback *cb;

    /** 
     * Internal unique timer ID, which is assigned by the timer heap. 
     * Application should not touch this ID.
     */
    pj_timer_id_t _timer_id;

    /** 
     * The future time when the timer expires, which the value is updated
     * by timer heap when the timer is scheduled.
     */
    pj_time_val _timer_value;

    /**
     * Internal: the group lock used by this entry, set when
     * pj_timer_heap_schedule_w_lock() is used.
     */
    pj_grp_lock_t *_grp_lock;

#if PJ_TIMER_DEBUG
    const char	*src_file;
    int		 src_line;
#endif
} pj_timer_entry;


/**
 * Calculate memory size required to create a timer heap.
 *
 * @param count     Number of timer entries to be supported.
 * @return          Memory size requirement in bytes.
 */
PJ_DECL(pj_size_t) pj_timer_heap_mem_size(pj_size_t count);

/**
 * Create a timer heap.
 *
 * @param pool      The pool where allocations in the timer heap will be 
 *                  allocated. The timer heap will dynamicly allocate 
 *                  more storate from the pool if the number of timer 
 *                  entries registered is more than the size originally 
 *                  requested when calling this function.
 * @param count     The maximum number of timer entries to be supported 
 *                  initially. If the application registers more entries 
 *                  during runtime, then the timer heap will resize.
 * @param ht        Pointer to receive the created timer heap.
 *
 * @return          PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_timer_heap_create( pj_pool_t *pool,
					   pj_size_t count,
                                           pj_timer_heap_t **ht);

/**
 * Destroy the timer heap.
 *
 * @param ht        The timer heap.
 */
PJ_DECL(void) pj_timer_heap_destroy( pj_timer_heap_t *ht );


/**
 * Set lock object to be used by the timer heap. By default, the timer heap
 * uses dummy synchronization.
 *
 * @param ht        The timer heap.
 * @param lock      The lock object to be used for synchronization.
 * @param auto_del  If nonzero, the lock object will be destroyed when
 *                  the timer heap is destroyed.
 */
PJ_DECL(void) pj_timer_heap_set_lock( pj_timer_heap_t *ht,
                                      pj_lock_t *lock,
                                      pj_bool_t auto_del );

/**
 * Set maximum number of timed out entries to process in a single poll.
 *
 * @param ht        The timer heap.
 * @param count     Number of entries.
 *
 * @return          The old number.
 */
PJ_DECL(unsigned) pj_timer_heap_set_max_timed_out_per_poll(pj_timer_heap_t *ht,
                                                           unsigned count );

/**
 * Initialize a timer entry. Application should call this function at least
 * once before scheduling the entry to the timer heap, to properly initialize
 * the timer entry.
 *
 * @param entry     The timer entry to be initialized.
 * @param id        Arbitrary ID assigned by the user/owner of this entry.
 *                  Applications can use this ID to distinguish multiple
 *                  timer entries that share the same callback and user_data.
 * @param user_data User data to be associated with this entry. 
 *                  Applications normally will put the instance of object that
 *                  owns the timer entry in this field.
 * @param cb        Callback function to be called when the timer elapses.
 *
 * @return          The timer entry itself.
 */
PJ_DECL(pj_timer_entry*) pj_timer_entry_init( pj_timer_entry *entry,
                                              int id,
                                              void *user_data,
                                              pj_timer_heap_callback *cb );

/**
 * Queries whether a timer entry is currently running.
 *
 * @param entry     The timer entry to query.
 *
 * @return          PJ_TRUE if the timer is running.  PJ_FALSE if not.
 */
PJ_DECL(pj_bool_t) pj_timer_entry_running( pj_timer_entry *entry );

/**
 * Schedule a timer entry which will expire AFTER the specified delay.
 *
 * @param ht        The timer heap.
 * @param entry     The entry to be registered. 
 * @param delay     The interval to expire.
 * @return          PJ_SUCCESS, or the appropriate error code.
 */
#if PJ_TIMER_DEBUG
#  define pj_timer_heap_schedule(ht,e,d) \
			pj_timer_heap_schedule_dbg(ht,e,d,__FILE__,__LINE__)

  PJ_DECL(pj_status_t) pj_timer_heap_schedule_dbg( pj_timer_heap_t *ht,
        					   pj_timer_entry *entry,
        					   const pj_time_val *delay,
        					   const char *src_file,
        					   int src_line);
#else
PJ_DECL(pj_status_t) pj_timer_heap_schedule( pj_timer_heap_t *ht,
					     pj_timer_entry *entry, 
					     const pj_time_val *delay);
#endif	/* PJ_TIMER_DEBUG */

/**
 * Schedule a timer entry which will expire AFTER the specified delay, and
 * increment the reference counter of the group lock while the timer entry
 * is active. The group lock reference counter will automatically be released
 * after the timer callback is called or when the timer is cancelled.
 *
 * @param ht        The timer heap.
 * @param entry     The entry to be registered.
 * @param id_val    The value to be set to the "id" field of the timer entry
 * 		    once the timer is scheduled.
 * @param delay     The interval to expire.
 * @param grp_lock  The group lock.
 *
 * @return          PJ_SUCCESS, or the appropriate error code.
 */
#if PJ_TIMER_DEBUG
#  define pj_timer_heap_schedule_w_grp_lock(ht,e,d,id,g) \
	pj_timer_heap_schedule_w_grp_lock_dbg(ht,e,d,id,g,__FILE__,__LINE__)

  PJ_DECL(pj_status_t) pj_timer_heap_schedule_w_grp_lock_dbg(
						   pj_timer_heap_t *ht,
        					   pj_timer_entry *entry,
        					   const pj_time_val *delay,
        					   int id_val,
        					   pj_grp_lock_t *grp_lock,
        					   const char *src_file,
        					   int src_line);
#else
PJ_DECL(pj_status_t) pj_timer_heap_schedule_w_grp_lock(
						    pj_timer_heap_t *ht,
						    pj_timer_entry *entry,
						    const pj_time_val *delay,
						    int id_val,
						    pj_grp_lock_t *grp_lock);
#endif	/* PJ_TIMER_DEBUG */


/**
 * Cancel a previously registered timer. This will also decrement the
 * reference counter of the group lock associated with the timer entry,
 * if the entry was scheduled with one.
 *
 * @param ht        The timer heap.
 * @param entry     The entry to be cancelled.
 * @return          The number of timer cancelled, which should be one if the
 *                  entry has really been registered, or zero if no timer was
 *                  cancelled.
 */
PJ_DECL(int) pj_timer_heap_cancel( pj_timer_heap_t *ht,
				   pj_timer_entry *entry);

/**
 * Cancel only if the previously registered timer is active. This will
 * also decrement the reference counter of the group lock associated
 * with the timer entry, if the entry was scheduled with one. In any
 * case, set the "id" to the specified value.
 *
 * @param ht        The timer heap.
 * @param entry     The entry to be cancelled.
 * @param id_val    Value to be set to "id"
 *
 * @return          The number of timer cancelled, which should be one if the
 *                  entry has really been registered, or zero if no timer was
 *                  cancelled.
 */
PJ_DECL(int) pj_timer_heap_cancel_if_active(pj_timer_heap_t *ht,
                                            pj_timer_entry *entry,
                                            int id_val);

/**
 * Get the number of timer entries.
 *
 * @param ht        The timer heap.
 * @return          The number of timer entries.
 */
PJ_DECL(pj_size_t) pj_timer_heap_count( pj_timer_heap_t *ht );

/**
 * Get the earliest time registered in the timer heap. The timer heap
 * MUST have at least one timer being scheduled (application should use
 * #pj_timer_heap_count() before calling this function).
 *
 * @param ht        The timer heap.
 * @param timeval   The time deadline of the earliest timer entry.
 *
 * @return          PJ_SUCCESS, or PJ_ENOTFOUND if no entry is scheduled.
 */
PJ_DECL(pj_status_t) pj_timer_heap_earliest_time( pj_timer_heap_t *ht, 
					          pj_time_val *timeval);

/**
 * Poll the timer heap, check for expired timers and call the callback for
 * each of the expired timers.
 *
 * Note: polling the timer heap is not necessary in Symbian. Please see
 * @ref PJ_SYMBIAN_OS for more info.
 *
 * @param ht         The timer heap.
 * @param next_delay If this parameter is not NULL, it will be filled up with
 *		     the time delay until the next timer elapsed, or 
 *		     PJ_MAXINT32 in the sec part if no entry exist.
 *
 * @return           The number of timers expired.
 */
PJ_DECL(unsigned) pj_timer_heap_poll( pj_timer_heap_t *ht, 
                                      pj_time_val *next_delay);

#if PJ_TIMER_DEBUG
/**
 * Dump timer heap entries.
 *
 * @param ht	    The timer heap.
 */
PJ_DECL(void) pj_timer_heap_dump(pj_timer_heap_t *ht);
#endif

/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJ_TIMER_H__ */

