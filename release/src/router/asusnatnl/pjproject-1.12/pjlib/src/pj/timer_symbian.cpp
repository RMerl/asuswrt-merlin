/* $Id: timer_symbian.cpp 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/timer.h>
#include <pj/pool.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/lock.h>

#include "os_symbian.h"


#define DEFAULT_MAX_TIMED_OUT_PER_POLL  (64)

// Maximum number of miliseconds that RTimer.At() supports
#define MAX_RTIMER_INTERVAL		2147

/* Absolute maximum number of timer entries */
#ifndef PJ_SYMBIAN_TIMER_MAX_COUNT
#  define PJ_SYMBIAN_TIMER_MAX_COUNT	65535
#endif

/* Get the number of free slots in the timer heap */
#define FREECNT(th)	(th->max_size - th->cur_size)

// Forward declaration
class CPjTimerEntry;

/**
 * The implementation of timer heap.
 */
struct pj_timer_heap_t
{
    /** Maximum size of the heap. */
    pj_size_t max_size;

    /** Current size of the heap. */
    pj_size_t cur_size;

    /** Array of timer entries. A scheduled timer will occupy one slot, and
     *  the slot number will be saved in entry->_timer_id
     */
    CPjTimerEntry **entries;
    
    /** Array of free slot indexes in the "entries" array */
    int *free_slots;
};

/**
 * Active object for each timer entry.
 */
class CPjTimerEntry : public CActive 
{
public:
    pj_timer_entry  *entry_;
    
    static CPjTimerEntry* NewL(	pj_timer_heap_t *timer_heap,
    				pj_timer_entry *entry,
    				const pj_time_val *delay);
    
    ~CPjTimerEntry();
    
    virtual void RunL();
    virtual void DoCancel();

private:	
    pj_timer_heap_t *timer_heap_;
    RTimer	     rtimer_;
    pj_uint32_t	     interval_left_;
    
    CPjTimerEntry(pj_timer_heap_t *timer_heap, pj_timer_entry *entry);
    void ConstructL(const pj_time_val *delay);
    void Schedule();
};

//////////////////////////////////////////////////////////////////////////////
/*
 * Implementation.
 */

/* Grow timer heap to the specified size */
static pj_status_t realloc_timer_heap(pj_timer_heap_t *th, pj_size_t new_size)
{
    typedef CPjTimerEntry *entry_ptr;
    CPjTimerEntry **entries = NULL;
    int *free_slots = NULL;
    unsigned i, j;
 
    if (new_size > PJ_SYMBIAN_TIMER_MAX_COUNT) {
	/* Just some sanity limit */
	new_size = PJ_SYMBIAN_TIMER_MAX_COUNT;
	if (new_size <= th->max_size) {
	    /* We've grown large enough */
	    pj_assert(!"Too many timer heap entries");
	    return PJ_ETOOMANY;
	}
    }
    
    /* Allocate entries, move entries from the old array if there is one */
    entries = new entry_ptr[new_size];
    if (th->entries) {
	pj_memcpy(entries, th->entries, th->max_size * sizeof(th->entries[0]));
    }
    /* Initialize the remaining new area */
    pj_bzero(&entries[th->max_size], 
	    (new_size - th->max_size) * sizeof(th->entries[0]));
    
    /* Allocate free slots array */
    free_slots = new int[new_size];
    if (th->free_slots) {
	pj_memcpy(free_slots, th->free_slots, 
		  FREECNT(th) * sizeof(th->free_slots[0]));
    }
    /* Initialize the remaining new area */
    for (i=FREECNT(th), j=th->max_size; j<new_size; ++i, ++j) {
	free_slots[i] = j;
    }
    for ( ; i<new_size; ++i) {
	free_slots[i] = -1;
    }
    
    /* Apply */
    delete [] th->entries;
    th->entries = entries;
    th->max_size = new_size;
    delete [] th->free_slots;
    th->free_slots = free_slots;

    return PJ_SUCCESS;
}

/* Allocate and register an entry to timer heap for newly scheduled entry */
static pj_status_t add_entry(pj_timer_heap_t *th, CPjTimerEntry *entry)
{
    pj_status_t status;
    int slot;
    
    /* Check that there's still capacity left in the timer heap */
    if (FREECNT(th) < 1) {
	// Grow the timer heap twice the capacity
	status = realloc_timer_heap(th, th->max_size * 2);
	if (status != PJ_SUCCESS)
	    return status;
    }
    
    /* Allocate one free slot. Use LIFO */
    slot = th->free_slots[FREECNT(th)-1];
    PJ_ASSERT_RETURN((slot >= 0) && (slot < (int)th->max_size) && 
		     (th->entries[slot]==NULL), PJ_EBUG);
    
    th->free_slots[FREECNT(th)-1] = -1;
    th->entries[slot] = entry;
    entry->entry_->_timer_id = slot;
    ++th->cur_size;
    
    return PJ_SUCCESS;
}

/* Free a slot when an entry's timer has elapsed or cancel */
static pj_status_t remove_entry(pj_timer_heap_t *th, CPjTimerEntry *entry)
{
    int slot = entry->entry_->_timer_id;
    
    PJ_ASSERT_RETURN(slot >= 0 && slot < (int)th->max_size, PJ_EBUG);
    PJ_ASSERT_RETURN(FREECNT(th) < th->max_size, PJ_EBUG);
    PJ_ASSERT_RETURN(th->entries[slot]==entry, PJ_EBUG);
    PJ_ASSERT_RETURN(th->free_slots[FREECNT(th)]==-1, PJ_EBUG);
    
    th->entries[slot] = NULL;
    th->free_slots[FREECNT(th)] = slot;
    entry->entry_->_timer_id = -1;
    --th->cur_size;
    
    return PJ_SUCCESS;
}


CPjTimerEntry::CPjTimerEntry(pj_timer_heap_t *timer_heap,
			     pj_timer_entry *entry)
: CActive(PJ_SYMBIAN_TIMER_PRIORITY), entry_(entry), timer_heap_(timer_heap), 
  interval_left_(0)
{
}

CPjTimerEntry::~CPjTimerEntry() 
{
    Cancel();
    rtimer_.Close();
}

void CPjTimerEntry::Schedule()
{
    pj_int32_t interval;
    
    if (interval_left_ > MAX_RTIMER_INTERVAL) {
	interval = MAX_RTIMER_INTERVAL;
    } else {
	interval = interval_left_;
    }
    
    interval_left_ -= interval;
    rtimer_.After(iStatus, interval * 1000);
    SetActive();
}

void CPjTimerEntry::ConstructL(const pj_time_val *delay) 
{
    rtimer_.CreateLocal();
    CActiveScheduler::Add(this);
    
    interval_left_ = PJ_TIME_VAL_MSEC(*delay);
    Schedule();
}

CPjTimerEntry* CPjTimerEntry::NewL(pj_timer_heap_t *timer_heap,
				   pj_timer_entry *entry,
				   const pj_time_val *delay) 
{
    CPjTimerEntry *self = new CPjTimerEntry(timer_heap, entry);
    CleanupStack::PushL(self);
    self->ConstructL(delay);
    CleanupStack::Pop(self);

    return self;
}

void CPjTimerEntry::RunL() 
{
    if (interval_left_ > 0) {
	Schedule();
	return;
    }
    
    remove_entry(timer_heap_, this);
    entry_->cb(timer_heap_, entry_);
    
    // Finger's crossed!
    delete this;
}

void CPjTimerEntry::DoCancel() 
{
    /* It's possible that _timer_id is -1, see schedule(). In this case,
     * the entry has not been added to the timer heap, so don't remove
     * it.
     */
    if (entry_ && entry_->_timer_id != -1)
	remove_entry(timer_heap_, this);
    
    rtimer_.Cancel();
}


//////////////////////////////////////////////////////////////////////////////


/*
 * Calculate memory size required to create a timer heap.
 */
PJ_DEF(pj_size_t) pj_timer_heap_mem_size(pj_size_t count)
{
    return /* size of the timer heap itself: */
           sizeof(pj_timer_heap_t) + 
           /* size of each entry: */
           (count+2) * (sizeof(void*)+sizeof(int)) +
           /* lock, pool etc: */
           132;
}

/*
 * Create a new timer heap.
 */
PJ_DEF(pj_status_t) pj_timer_heap_create( pj_pool_t *pool,
					  pj_size_t size,
                                          pj_timer_heap_t **p_heap)
{
    pj_timer_heap_t *ht;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && p_heap, PJ_EINVAL);

    *p_heap = NULL;

    /* Allocate timer heap data structure from the pool */
    ht = PJ_POOL_ZALLOC_T(pool, pj_timer_heap_t);
    if (!ht)
        return PJ_ENOMEM;

    /* Allocate slots */
    status = realloc_timer_heap(ht, size);
    if (status != PJ_SUCCESS)
	return status;

    *p_heap = ht;
    return PJ_SUCCESS;
}

PJ_DEF(void) pj_timer_heap_destroy( pj_timer_heap_t *ht )
{
    /* Cancel and delete pending active objects */
    if (ht->entries) {
	unsigned i;
	for (i=0; i<ht->max_size; ++i) {
	    if (ht->entries[i]) {
		ht->entries[i]->entry_ = NULL;
		ht->entries[i]->Cancel();
		delete ht->entries[i];
		ht->entries[i] = NULL;
	    }
	}
    }
    
    delete [] ht->entries;
    delete [] ht->free_slots;
    
    ht->entries = NULL;
    ht->free_slots = NULL;
}

PJ_DEF(void) pj_timer_heap_set_lock(  pj_timer_heap_t *ht,
                                      pj_lock_t *lock,
                                      pj_bool_t auto_del )
{
    PJ_UNUSED_ARG(ht);
    if (auto_del)
    	pj_lock_destroy(lock);
}


PJ_DEF(unsigned) pj_timer_heap_set_max_timed_out_per_poll(pj_timer_heap_t *ht,
                                                          unsigned count )
{
    /* Not applicable */
    PJ_UNUSED_ARG(count);
    return ht->max_size;
}

PJ_DEF(pj_timer_entry*) pj_timer_entry_init( pj_timer_entry *entry,
                                             int id,
                                             void *user_data,
                                             pj_timer_heap_callback *cb )
{
    pj_assert(entry && cb);

    entry->_timer_id = -1;
    entry->id = id;
    entry->user_data = user_data;
    entry->cb = cb;

    return entry;
}

PJ_DEF(pj_status_t) pj_timer_heap_schedule( pj_timer_heap_t *ht,
					    pj_timer_entry *entry, 
					    const pj_time_val *delay)
{
    CPjTimerEntry *timerObj;
    pj_status_t status;
    
    PJ_ASSERT_RETURN(ht && entry && delay, PJ_EINVAL);
    PJ_ASSERT_RETURN(entry->cb != NULL, PJ_EINVAL);

    /* Prevent same entry from being scheduled more than once */
    PJ_ASSERT_RETURN(entry->_timer_id < 1, PJ_EINVALIDOP);

    entry->_timer_id = -1;
    
    timerObj = CPjTimerEntry::NewL(ht, entry, delay);
    status = add_entry(ht, timerObj);
    if (status != PJ_SUCCESS) {
	timerObj->Cancel();
	delete timerObj;
	return status;
    }
    
    return PJ_SUCCESS;
}

PJ_DEF(int) pj_timer_heap_cancel( pj_timer_heap_t *ht,
				  pj_timer_entry *entry)
{
    PJ_ASSERT_RETURN(ht && entry, PJ_EINVAL);
    
    if (entry->_timer_id >= 0 && entry->_timer_id < (int)ht->max_size) {
    	CPjTimerEntry *timerObj = ht->entries[entry->_timer_id];
    	if (timerObj) {
    	    timerObj->Cancel();
    	    delete timerObj;
    	    return 1;
    	} else {
    	    return 0;
    	}
    } else {
    	return 0;
    }
}

PJ_DEF(unsigned) pj_timer_heap_poll( pj_timer_heap_t *ht, 
                                     pj_time_val *next_delay )
{
    /* Polling is not necessary on Symbian, since all async activities
     * are registered to active scheduler.
     */
    PJ_UNUSED_ARG(ht);
    if (next_delay) {
    	next_delay->sec = 1;
    	next_delay->msec = 0;
    }
    return 0;
}

PJ_DEF(pj_size_t) pj_timer_heap_count( pj_timer_heap_t *ht )
{
    PJ_ASSERT_RETURN(ht, 0);

    return ht->cur_size;
}

PJ_DEF(pj_status_t) pj_timer_heap_earliest_time( pj_timer_heap_t * ht,
					         pj_time_val *timeval)
{
    /* We don't support this! */
    PJ_UNUSED_ARG(ht);
    
    timeval->sec = 1;
    timeval->msec = 0;
    
    return PJ_SUCCESS;
}
