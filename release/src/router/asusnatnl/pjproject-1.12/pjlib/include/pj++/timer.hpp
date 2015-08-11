/* $Id: timer.hpp 2394 2008-12-23 17:27:53Z bennylp $ */
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
#ifndef __PJPP_TIMER_HPP__
#define __PJPP_TIMER_HPP__

#include <pj/timer.h>
#include <pj++/types.hpp>
#include <pj/assert.h>
#include <pj++/lock.hpp>

class Pj_Timer_Heap;

//////////////////////////////////////////////////////////////////////////////
// Timer entry.
//
// How to use:
//  Derive class from Pj_Timer_Entry and override on_timeout().
//  Scheduler timer in Pj_Timer_Heap.
//
class Pj_Timer_Entry : public Pj_Object
{
    friend class Pj_Timer_Heap;

public:
    //
    // Default constructor.
    //
    Pj_Timer_Entry() 
    { 
        entry_.user_data = this;
        entry_.cb = &timer_heap_callback; 
    }

    //
    // Destructor, do nothing.
    //
    ~Pj_Timer_Entry()
    {
    }

    //
    // Override this to get the timeout notification.
    //
    virtual void on_timeout(int id) = 0;

private:
    pj_timer_entry entry_;

    static void timer_heap_callback(pj_timer_heap_t*, pj_timer_entry *e)
    {
        Pj_Timer_Entry *entry = (Pj_Timer_Entry*) e->user_data;
        entry->on_timeout(e->id);
    }

};

//////////////////////////////////////////////////////////////////////////////
// Timer heap.
//
class Pj_Timer_Heap : public Pj_Object
{
public:
    //
    // Default constructor.
    //
    Pj_Timer_Heap() 
        : ht_(NULL) 
    {
    }

    //
    // Construct timer heap.
    //
    Pj_Timer_Heap(Pj_Pool *pool, pj_size_t initial_count)
        : ht_(NULL)
    {
        create(pool, initial_count);
    }

    //
    // Destructor.
    //
    ~Pj_Timer_Heap()
    {
        destroy();
    }

    //
    // Create
    // 
    pj_status_t create(Pj_Pool *pool, pj_size_t initial_count)
    {
        destroy();
	return pj_timer_heap_create(pool->pool_(), initial_count, &ht_);
    }

    //
    // Destroy
    //
    void destroy()
    {
        if (ht_) {
            pj_timer_heap_destroy(ht_);
            ht_ = NULL;
        }
    }

    //
    // Get pjlib compatible timer heap object.
    //
    pj_timer_heap_t *get_timer_heap()
    {
	return ht_;
    }

    //
    // Set the lock object.
    //
    void set_lock( Pj_Lock *lock, bool auto_delete )
    {
        pj_timer_heap_set_lock( ht_, lock->pj_lock_t_(), auto_delete);
    }

    //
    // Set maximum number of timed out entries to be processed per poll.
    //
    unsigned set_max_timed_out_per_poll(unsigned count)
    {
        return pj_timer_heap_set_max_timed_out_per_poll(ht_, count);
    }

    //
    // Schedule a timer.
    //
    bool schedule( Pj_Timer_Entry *ent, const Pj_Time_Val &delay,
                   int id)
    {
        ent->entry_.id = id;
	return pj_timer_heap_schedule(ht_, &ent->entry_, &delay) == 0;
    }

    //
    // Cancel a timer.
    //
    bool cancel(Pj_Timer_Entry *ent)
    {
	return pj_timer_heap_cancel(ht_, &ent->entry_) == 1;
    }

    //
    // Get current number of timers
    //
    pj_size_t count()
    {
	return pj_timer_heap_count(ht_);
    }

    //
    // Get the earliest time.
    // Return false if no timer is found.
    //
    bool earliest_time(Pj_Time_Val *t)
    {
	return pj_timer_heap_earliest_time(ht_, t) == PJ_SUCCESS;
    }

    //
    // Poll the timer.
    // Return number of timed out entries has been called.
    //
    unsigned poll(Pj_Time_Val *next_delay = NULL)
    {
	return pj_timer_heap_poll(ht_, next_delay);
    }

private:
    pj_timer_heap_t *ht_;
};

#endif	/* __PJPP_TIMER_HPP__ */

