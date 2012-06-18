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

#ifndef THREADPOOL_H
#define THREADPOOL_H

#ifdef __cplusplus
extern "C" {
#endif

//Size of job free list
#define JOBFREELISTSIZE 100

#define INFINITE_THREADS -1

#define EMAXTHREADS (-8 & 1<<29)

//Invalid Policy
#define INVALID_POLICY (-9 & 1<<29)

//Invalid JOB Id
#define INVALID_JOB_ID (-2 & 1<<29)

typedef enum duration {SHORT_TERM,PERSISTENT} Duration;

typedef enum priority {LOW_PRIORITY,
		       MED_PRIORITY,
		       HIGH_PRIORITY} ThreadPriority;

#define DEFAULT_PRIORITY MED_PRIORITY //default priority used by TPJobInit
#define DEFAULT_MIN_THREADS 1	      //default minimum used by TPAttrInit
#define DEFAULT_MAX_THREADS 10	      //default max used by TPAttrInit	
#define DEFAULT_JOBS_PER_THREAD 10    //default jobs per thread used by TPAttrInit
#define DEFAULT_STARVATION_TIME	500   //default starvation time used by TPAttrInit
#define DEFAULT_IDLE_TIME 10 * 1000   //default idle time used by TPAttrInit
#define DEFAULT_FREE_ROUTINE NULL     //default free routine used TPJobInit 

#define STATS 1 //always include stats because code change is minimal


//Statistics 
#ifdef STATS
#define STATSONLY(x) x
#else
#define STATSONLY(x)
#endif

#ifdef _DEBUG
#define DEBUG 1
#endif

//DEBUGGING
#ifdef DEBUG
#define DBGONLY(x) x
#else
#define DBGONLY(x)
#endif

#include "LinkedList.h"
#include <sys/timeb.h>
#include "FreeList.h"

#include "ithread.h"
#include <errno.h>
#include <sys/timeb.h>
#define EXPORT 
typedef int PolicyType;
#define DEFAULT_POLICY SCHED_OTHER
#define DEFAULT_SCHED_PARAM 0 //default priority

/****************************************************************************
 * Name: free_routine
 *
 *  Description:
 *     Function for freeing a thread argument
 *****************************************************************************/
typedef void (*free_routine)(void *arg);

/****************************************************************************
 * Name: ThreadPoolAttr
 *
 *  Description:
 *     Attributes for thread pool. Used to set and change parameters of 
 *     thread pool
 *****************************************************************************/
typedef struct THREADPOOLATTR
{
  int minThreads; //minThreads, ThreadPool will always maintain at least
                  //this many threads
  
  int maxThreads; //maxThreads, ThreadPool will never have more than this
                  //number of threads
  
  int maxIdleTime;   //maxIdleTime (in milliseconds) 
                     // this is the maximum time a thread will remain idle
                     // before dying

  int jobsPerThread; //jobs per thread to maintain
					  
  int starvationTime;   //the time a low priority or med priority
	                    //job waits before getting bumped
                        //up a priority (in milliseconds)
  
  PolicyType schedPolicy; //scheduling policy to use

} ThreadPoolAttr;

/****************************************************************************
 * Name: ThreadPool
 *
 *  Description:
 *     Internal ThreadPool Job
 *****************************************************************************/
typedef struct THREADPOOLJOB
{
  start_routine func; //function
  void *arg;          //arg
  free_routine free_func; //free function
  struct timeb requestTime; //time of request
  int priority;       //priority of request
  int jobId;         //id 
} ThreadPoolJob;

/****************************************************************************
 * Name: ThreadPoolStats
 *
 *  Description:
 *     Structure to hold statistics
 *****************************************************************************/

STATSONLY(

typedef struct TPOOLSTATS
{
  double totalTimeHQ; //total time spent by all jobs in high priority Q
  int totalJobsHQ;    //total jobs in HQ run so far
  double avgWaitHQ;   //average wait in HQ 
  double totalTimeMQ; //total time spent by all jobs in med priority Q
  int totalJobsMQ;    //total jobs in MQ run so far
  double avgWaitMQ;   //average wait in MQ
  double totalTimeLQ; //total time spent by all jobs in low priority Q
  int totalJobsLQ;    //total jobs in LQ run so far
  double avgWaitLQ;	//average wait in LQ	
  double totalWorkTime; //total time spent working for all threads
  double totalIdleTime; //total time spent idle for all threads
  int workerThreads; //number of current workerThreads
  int idleThreads;   //number of current idle threads
  int persistentThreads; //number of persistent threads
  int totalThreads; //total number of current threads
  int maxThreads; //max threads so far	
  int currentJobsHQ; // current jobs in Q
  int currentJobsLQ; //current jobs in Q
  int currentJobsMQ; //current jobs in Q
}ThreadPoolStats;

)


/****************************************************************************
 * Name: ThreadPool
 *
 *  Description:
 *     A thread pool similar to the thread pool in the UPnP SDK.
 *     Allows jobs to be scheduled for running by threads in a 
 *     thread pool. The thread pool is initialized with a 
 *     minimum and maximum thread number as well as a 
 *	   max idle time
 *     and a jobs per thread ratio. If a worker thread waits the whole
 *     max idle time without receiving a job and the thread pool
 *     currently has more threads running than the minimum
 *     then the worker thread will exit. If when 
 *     scheduling a job the current job to thread ratio
 *     becomes greater than the set ratio and the thread pool currently has
 *     less than the maximum threads then a new thread will
 *     be created.
 *
 *****************************************************************************/

typedef struct THREADPOOL
{
  ithread_mutex_t mutex; //mutex to protect job qs
  ithread_cond_t condition; //condition variable to signal Q
  ithread_cond_t start_and_shutdown; //condition variable for start 
                                     //and stop     
  int lastJobId; //ids for jobs 
  int shutdown;   //whether or not we are shutting down
  int totalThreads;       //total number of threads	
  int persistentThreads; //number of persistent threads
  FreeList jobFreeList; //free list of jobs
  LinkedList lowJobQ;    //low priority job Q
  LinkedList medJobQ;    //med priority job Q
  LinkedList highJobQ;   //high priority job Q
  ThreadPoolJob *persistentJob; //persistent job
 
  ThreadPoolAttr attr; //thread pool attributes
  
  //statistics 
  STATSONLY(ThreadPoolStats stats;)
 
} ThreadPool;



/****************************************************************************
 * Function: ThreadPoolInit
 *
 *  Description:
 *      Initializes and starts ThreadPool. Must be called first.
 *      And only once for ThreadPool.
 *  Parameters:
 *      tp  - must be valid, non null, pointer to ThreadPool.
 *      attr - can be null
 *
 *       if not null then attr contains the following fields:
 *
 *      minWorkerThreads - minimum number of worker threads
 *                                 thread pool will never have less than this
 *                                  number of threads.
 *      maxWorkerThreads - maximum number of worker threads
 *                         thread pool will never have more than this
 *                         number of threads.
 *      maxIdleTime      - maximum time that a worker thread will spend
 *                         idle. If a worker is idle longer than this
 *                         time and there are more than the min
 *                         number of workers running, than the
 *                         worker thread exits.
 *      jobsPerThread    - ratio of jobs to thread to try and maintain
 *                         if a job is scheduled and the number of jobs per
 *                         thread is greater than this number,and  
 *                         if less than the maximum number of
 *                         workers are running then a new thread is 
 *                         started to help out with efficiency.
 *      schedPolicy      - scheduling policy to try and set (OS dependent)
 *  Returns:
 *      0 on success, nonzero on failure.
 *      EAGAIN if not enough system resources to create minimum threads.
 *      INVALID_POLICY if schedPolicy can't be set
 *      EMAXTHREADS if minimum threads is greater than maximum threads
 *****************************************************************************/
int ThreadPoolInit(ThreadPool *tp,
  ThreadPoolAttr *attr);

/****************************************************************************
 * Function: ThreadPoolAddPersistent
 *
 *  Description:
 *      Adds a persistent job to the thread pool.
 *      Job will be run as soon as possible.
 *      Call will block until job is scheduled.
 *  Parameters:
 *      tp - valid thread pool pointer
 *      ThreadPoolJob - valid thread pool job with the following fields:
 *
 *        func - ThreadFunction to run
 *        arg - argument to function.
 *        priority - priority of job.
 * 
 *  Returns:
 *      0 on success, nonzero on failure
 *      EOUTOFMEM not enough memory to add job.
 *      EMAXTHREADS not enough threads to add persistent job.
 *****************************************************************************/
int ThreadPoolAddPersistent (ThreadPool*tp,
  ThreadPoolJob *job,
  int *jobId);

/****************************************************************************
 * Function: ThreadPoolGetAttr
 *
 *  Description:
 *      Gets the current set of attributes
 *      associated with the thread pool.
 *  Parameters:
 *      tp - valid thread pool pointer
 *      out - non null pointer to store attributes
 *  Returns:
 *      0 on success, nonzero on failure
 *      Always returns 0.
 *****************************************************************************/
int ThreadPoolGetAttr(ThreadPool *tp,
  ThreadPoolAttr *out);
/****************************************************************************
 * Function: ThreadPoolSetAttr
 *
 *  Description:
 *      Sets the attributes for the thread pool.
 *      Only affects future calculations. 
 *  Parameters:
 *      tp - valid thread pool pointer
 *      attr - pointer to attributes, null sets attributes to default.
 *  Returns:
 *      0 on success, nonzero on failure
 *      Returns INVALID_POLICY if policy can not be set.
 *****************************************************************************/
int ThreadPoolSetAttr(ThreadPool *tp,
  ThreadPoolAttr *attr);

/****************************************************************************
 * Function: ThreadPoolAdd
 *
 *  Description:
 *      Adds a job to the thread pool.
 *      Job will be run as soon as possible.
 *  Parameters:
 *      tp - valid thread pool pointer
 *      func - ThreadFunction to run
 *      arg - argument to function.
 *      priority - priority of job.
 *      poolid - id of job
 *      free_function - function to use when freeing argument 
 *  Returns:
 *      0 on success, nonzero on failure
 *      EOUTOFMEM if not enough memory to add job.
 *****************************************************************************/
int ThreadPoolAdd (ThreadPool*tp,
  ThreadPoolJob *job,
  int *jobId);

/****************************************************************************
 * Function: ThreadPoolRemove
 *
 *  Description:
 *      Removes a job from the thread pool.
 *      Can only remove jobs which are not
 *      currently running.
 *  Parameters:
 *      tp - valid thread pool pointer
 *      jobid - id of job
 *      out - space for removed job.
 *  Returns:
 *      0 on success, nonzero on failure.
 *      INVALID_JOB_ID if job not found. 
 *****************************************************************************/
int ThreadPoolRemove(ThreadPool *tp,
  int jobId, ThreadPoolJob *out);



/****************************************************************************
 * Function: ThreadPoolShutdown
 *
 *  Description:
 *      Shuts the thread pool down.
 *      Waits for all threads to finish. 
 *      May block indefinitely if jobs do not
 *      exit.
 *  Parameters:
 *      tp - must be valid tp     
 *  Returns:
 *      0 on success, nonzero on failure
 *      Always returns 0.
 *****************************************************************************/
int ThreadPoolShutdown(ThreadPool *tp);


/****************************************************************************
 * Function: TPJobInit
 *
 *  Description:
 *      Initializes thread pool job.
 *      Sets the priority to default defined in ThreadPool.h.
 *      Sets the free_routine to default defined in ThreadPool.h
 *  Parameters:
 *      ThreadPoolJob *job - must be valid thread pool attributes.    
 *      start_routine func - function to run, must be valid
 *      void * arg - argument to pass to function.
 *  Returns:
 *      Always returns 0.
 *****************************************************************************/
int TPJobInit(ThreadPoolJob *job, start_routine func, void *arg);

/****************************************************************************
 * Function: TPJobSetPriority
 *
 *  Description:
 *      Sets the max threads for the thread pool attributes.
 *  Parameters:
 *      attr - must be valid thread pool attributes. 
 *      maxThreads - value to set
 *  Returns:
 *      Always returns 0.
 *****************************************************************************/
int TPJobSetPriority(ThreadPoolJob *job, ThreadPriority priority);

/****************************************************************************
 * Function: TPJobSetFreeFunction
 *
 *  Description:
 *      Sets the max threads for the thread pool attributes.
 *  Parameters:
 *      attr - must be valid thread pool attributes. 
 *      maxThreads - value to set
 *  Returns:
 *      Always returns 0.
 *****************************************************************************/
int TPJobSetFreeFunction(ThreadPoolJob *job, free_routine func);


/****************************************************************************
 * Function: TPAttrInit
 *
 *  Description:
 *      Initializes thread pool attributes.
 *      Sets values to defaults defined in ThreadPool.h.
 *  Parameters:
 *      attr - must be valid thread pool attributes.    
 *  Returns:
 *      Always returns 0.
 *****************************************************************************/
int TPAttrInit(ThreadPoolAttr *attr);

/****************************************************************************
 * Function: TPAttrSetMaxThreads
 *
 *  Description:
 *      Sets the max threads for the thread pool attributes.
 *  Parameters:
 *      attr - must be valid thread pool attributes. 
 *      maxThreads - value to set
 *  Returns:
 *      Always returns 0.
 *****************************************************************************/
int TPAttrSetMaxThreads(ThreadPoolAttr *attr, int maxThreads);

/****************************************************************************
 * Function: TPAttrSetMinThreads
 *
 *  Description:
 *      Sets the min threads for the thread pool attributes.
 *  Parameters:
 *      attr - must be valid thread pool attributes. 
 *      minThreads - value to set
 *  Returns:
 *      Always returns 0.
 *****************************************************************************/
int TPAttrSetMinThreads(ThreadPoolAttr *attr, int minThreads);

/****************************************************************************
 * Function: TPAttrSetIdleTime
 *
 *  Description:
 *      Sets the idle time for the thread pool attributes.
 *  Parameters:
 *      attr - must be valid thread pool attributes.    
 *  Returns:
 *      Always returns 0.
 *****************************************************************************/
int TPAttrSetIdleTime(ThreadPoolAttr *attr, int idleTime);

/****************************************************************************
 * Function: TPAttrSetJobsPerThread
 *
 *  Description:
 *      Sets the jobs per thread ratio
 *  Parameters:
 *      attr - must be valid thread pool attributes.
 *      jobsPerThread - number of jobs per thread to maintain
 *  Returns:
 *      Always returns 0.
 *****************************************************************************/
int TPAttrSetJobsPerThread(ThreadPoolAttr *attr, int jobsPerThread);

/****************************************************************************
 * Function: TPAttrSetStarvationTime
 *
 *  Description:
 *      Sets the starvation time for the thread pool attributes.
 *  Parameters:
 *      attr - must be valid thread pool attributes.    
 *      int starvationTime - milliseconds
 *  Returns:
 *      Always returns 0.
 *****************************************************************************/
int TPAttrSetStarvationTime(ThreadPoolAttr *attr, int starvationTime);

/****************************************************************************
 * Function: TPAttrSetSchedPolicy
 *
 *  Description:
 *      Sets the scheduling policy for the thread pool attributes.
 *  Parameters:
 *      attr - must be valid thread pool attributes.    
 *      PolicyType schedPolicy - must be a valid policy type.
 *  Returns:
 *      Always returns 0.
 *****************************************************************************/
int TPAttrSetSchedPolicy(ThreadPoolAttr *attr, PolicyType schedPolicy);


/****************************************************************************
 * Function: ThreadPoolGetStats
 *
 *  Description:
 *      Returns various statistics about the
 *      thread pool.
 *      Only valid if STATS has been defined.
 *  Parameters:
 *      ThreadPool *tp - valid initialized threadpool    
 *      ThreadPoolStats *stats - valid stats, out parameter
 *  Returns:
 *      Always returns 0.
 *****************************************************************************/
STATSONLY( EXPORT int ThreadPoolGetStats(ThreadPool *tp, ThreadPoolStats *stats););

STATSONLY(EXPORT void ThreadPoolPrintStats(ThreadPoolStats *stats););

#ifdef __cplusplus
}
#endif

#endif //ThreadPool
