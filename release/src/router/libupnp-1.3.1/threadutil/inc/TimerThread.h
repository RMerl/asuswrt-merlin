///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2003 Intel Corporation 
// All rights reserved. 
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met: 
//
// * Redistributions of source code must retain the above copyright notice, 
// this list of conditions and the following disclaimer. 
// * Redistributions in binary form must reproduce the above copyright notice, 
// this list of conditions and the following disclaimer in the documentation 
// and/or other materials provided with the distribution. 
// * Neither name of Intel Corporation nor the names of its contributors 
// may be used to endorse or promote products derived from this software 
// without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR 
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

#ifndef TIMERTHREAD_H
#define TIMERTHREAD_H

#include "ithread.h"
#include "LinkedList.h"
#include "FreeList.h"
#include "ThreadPool.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INVALID_EVENT_ID (-10 & 1<<29)

//Timeout Types
//absolute means in seconds from Jan 1, 1970
//relative means in seconds from current time
typedef enum timeoutType {ABS_SEC,REL_SEC} TimeoutType;


/****************************************************************************
 * Name: TimerThread
 * 
 *   Description:
 *     A timer thread similar to the one in the Upnp SDK that allows
 *     the scheduling of a job to run at a specified time in the future
 *     Because the timer thread uses the thread pool there is no 
 *     gurantee of timing, only approximate timing.
 *     Uses ThreadPool, Mutex, Condition, Thread
 *    
 * 
 *****************************************************************************/
typedef struct TIMERTHREAD
{
  ithread_mutex_t mutex; //mutex to protect eventQ
  ithread_cond_t condition; //condition variable
  int lastEventId;	//last event id
  LinkedList eventQ; //event q
  int shutdown;      //whether or not we are shutdown  
  FreeList freeEvents; //FreeList for events
  ThreadPool *tp;	 //ThreadPool to use
} TimerThread;


/****************************************************************************
 * Name: TimerEvent
 * 
 *   Description:
 *     
 *     Struct to contain information for a timer event.
 *     Internal to the TimerThread
 *   
 *****************************************************************************/
typedef struct TIMEREVENT
{
  ThreadPoolJob job;
  time_t eventTime; //absolute time for event in seconds since Jan 1, 1970
  Duration persistent;          //long term or short term job
  int id;                //id of job
} TimerEvent;




/************************************************************************
 * Function: TimerThreadInit
 * 
 *  Description:
 *     Initializes and starts timer thread.
 *
 *  Parameters:
 *             timer - valid timer thread pointer.
 *             tp  - valid thread pool to use. Must be
 *                   started. Must be valid for lifetime
 *                   of timer.  Timer must be shutdown
 *                   BEFORE thread pool.
 *  Return:
 *            0 on success, nonzero on failure
 *            Returns error from ThreadPoolAddPersistent on failure.
 *
 ************************************************************************/
int TimerThreadInit(TimerThread *timer,
		    ThreadPool *tp);


/************************************************************************
 * Function: TimerThreadSchedule
 * 
 *  Description:
 *     Schedules an event to run at a specified time.
 *
 *  Parameters:
 *             timer - valid timer thread pointer.
 *             time_t - time of event.
 *                      either in absolute seconds,
 *                      or relative seconds in the future.
 *             timeoutType - either ABS_SEC, or REL_SEC.
 *                           if REL_SEC, then the event
 *                           will be scheduled at the
 *                           current time + REL_SEC.
 *             job-> valid Thread pool job with following fields
 *             func - function to schedule
 *             arg - argument to function
 *             priority - priority of job.
 *         
 *             id - id of timer event. (out, can be null)
 *  Return:
 *            0 on success, nonzero on failure
 *            EOUTOFMEM if not enough memory to schedule job.
 *
 ************************************************************************/
int TimerThreadSchedule(TimerThread* timer,
			time_t time, 
			TimeoutType type,
			ThreadPoolJob *job,
			Duration duration,
			int *id);

/************************************************************************
 * Function: TimerThreadRemove
 * 
 *  Description:
 *     Removes an event from the timer Q.
 *     Events can only be removed 
 *     before they have been placed in the
 *     thread pool.
 *
 *  Parameters:
 *             timer - valid timer thread pointer.
 *             id - id of event to remove.
 *             ThreadPoolJob *out - space for thread pool job.
 *  Return:
 *            0 on success, 
 *            INVALID_EVENT_ID on failure
 *			 
 ************************************************************************/
int TimerThreadRemove(TimerThread *timer,
			   int id,
			   ThreadPoolJob *out);

/************************************************************************
 * Function: TimerThreadShutdown
 * 
 *  Description:
 *    Shutdown the timer thread
 *    Events scheduled in the future will NOT be run.
 *    Timer thread should be shutdown BEFORE it's associated
 *    thread pool.
 *  Returns:
 *    returns 0 if succesfull,
 *            nonzero otherwise.
 *            Always returns 0.
 ***********************************************************************/   
int TimerThreadShutdown(TimerThread *timer);

#ifdef __cplusplus
}
#endif

#endif //TIMER_THREAD_H
