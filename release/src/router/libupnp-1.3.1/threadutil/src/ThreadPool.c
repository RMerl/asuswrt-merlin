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

#include "ThreadPool.h"
#include "FreeList.h"
#include <assert.h>
#include <stdlib.h>

#ifdef STATS
#include <stdio.h>
#endif

/****************************************************************************
 * Function: CmpThreadPoolJob
 *
 *  Description:
 *      Compares thread pool jobs.
 *  Parameters:
 *      void * - job A
 *      void * - job B
 *****************************************************************************/
static int
CmpThreadPoolJob( void *jobA,
                  void *jobB )
{
    ThreadPoolJob *a = ( ThreadPoolJob * ) jobA;
    ThreadPoolJob *b = ( ThreadPoolJob * ) jobB;

    assert( jobA != NULL );
    assert( jobB != NULL );
    return ( a->jobId == b->jobId );
}

/****************************************************************************
 * Function: FreeThreadPoolJob
 *
 *  Description:
 *      Deallocates a dynamically allocated ThreadPoolJob.
 *  Parameters:
 *      ThreadPoolJob *tpj - must be allocated with CreateThreadPoolJob
 *****************************************************************************/
static void
FreeThreadPoolJob( ThreadPool * tp,
                   ThreadPoolJob * tpj )
{
    assert( tp != NULL );

    FreeListFree( &tp->jobFreeList, tpj );
}

/****************************************************************************
 * Function: SetPolicyType
 *
 *  Description:
 *      Sets the scheduling policy of the current process.
 *      Internal only.
 *  Parameters:
 *      PolocyType in
 *  Returns:
 *      0 on success, nonzero on failure
 *      Returns result of GetLastError() on failure.
 *
 *****************************************************************************/
static int
SetPolicyType( PolicyType in )
{
    struct sched_param current;

    sched_getparam( 0, &current );
    current.sched_priority = DEFAULT_SCHED_PARAM;
    return sched_setscheduler( 0, in, &current );
}

/****************************************************************************
 * Function: SetPriority
 *
 *  Description:
 *      Sets the priority of the currently running thread.
 *      Internal only.
 *  Parameters:
 *      ThreadPriority priority
 *  Returns:
 *      0 on success, nonzero on failure
 *      EINVAL invalid priority
 *      Returns result of GerLastError on failure.
 *
 *****************************************************************************/
static int
SetPriority( ThreadPriority priority )
{

    int currentPolicy;
    int minPriority = 0;
    int maxPriority = 0;
    int actPriority = 0;
    int midPriority = 0;
    struct sched_param newPriority;

    pthread_getschedparam( ithread_self(  ), &currentPolicy,
                           &newPriority );
    minPriority = sched_get_priority_min( currentPolicy );
    maxPriority = sched_get_priority_max( currentPolicy );
    midPriority = ( maxPriority - minPriority ) / 2;
    switch ( priority ) {

        case LOW_PRIORITY:
            actPriority = minPriority;
            break;
        case MED_PRIORITY:
            actPriority = midPriority;
            break;
        case HIGH_PRIORITY:
            actPriority = maxPriority;
            break;
        default:
            return EINVAL;
    };

    newPriority.sched_priority = actPriority;

    return pthread_setschedparam( ithread_self(  ), currentPolicy,
                                  &newPriority );

}

/****************************************************************************
 * Function: DiffMillis
 *
 *  Description:
 *      Returns the difference in milliseconds between two
 *      timeb structures.
 *      Internal Only.
 *  Parameters:
 *      struct timeb *time1,
 *      struct timeb *time2,
 *  Returns:
 *       the difference in milliseconds, time1-time2.
 *****************************************************************************/
static double
DiffMillis( struct timeb *time1,
            struct timeb *time2 )
{
    double temp = 0;

    assert( time1 != NULL );
    assert( time2 != NULL );

    temp = ( ( double )( time1->time ) - time2->time );
    temp = temp * 1000;
    temp += ( time1->millitm - time2->millitm );
    return temp;
}

/****************************************************************************
 * Function: BumpPriority
 *
 *  Description:
 *      Determines whether any jobs
 *      need to be bumped to a higher priority Q and bumps them.
 *
 *      tp->mutex must be locked.
 *      Internal Only.
 *  Parameters:
 *      ThreadPool *tp
 *****************************************************************************/
static void
BumpPriority( ThreadPool * tp )
{
    int done = 0;
    struct timeb now;
    double diffTime = 0;
    ThreadPoolJob *tempJob = NULL;

    assert( tp != NULL );

    ftime( &now );

    while( !done ) {
        if( tp->medJobQ.size ) {
            tempJob = ( ThreadPoolJob * ) tp->medJobQ.head.next->item;
            diffTime = DiffMillis( &now, &tempJob->requestTime );

            if( diffTime >= ( tp->attr.starvationTime ) ) {
                //If job has waited longer than the
                //starvation time
                //bump priority (add to higher priority Q)

                STATSONLY( tp->stats.totalJobsMQ++;
                     );
                STATSONLY( tp->stats.totalTimeMQ += diffTime;
                     );

                ListDelNode( &tp->medJobQ, tp->medJobQ.head.next, 0 );
                ListAddTail( &tp->highJobQ, tempJob );
                continue;
            }
        }

        if( tp->lowJobQ.size ) {
            tempJob = ( ThreadPoolJob * ) tp->lowJobQ.head.next->item;

            diffTime = DiffMillis( &now, &tempJob->requestTime );

            if( diffTime >= ( tp->attr.maxIdleTime ) ) {
                //If job has waited longer than the
                //starvation time
                //bump priority (add to higher priority Q)

                STATSONLY( tp->stats.totalJobsLQ++;
                     );
                STATSONLY( tp->stats.totalTimeLQ += diffTime;
                     );

                ListDelNode( &tp->lowJobQ, tp->lowJobQ.head.next, 0 );
                ListAddTail( &tp->medJobQ, tempJob );
                continue;
            }
        }

        done = 1;
    }
}

/****************************************************************************
 * Function: SetRelTimeout
 *
 *  Description:
 *      Sets the fields of the
 *      passed in timespec to be relMillis milliseconds in the future.
 *      Internal Only.
 *  Parameters:
 *      struct timespec *time
 *      int relMillis - milliseconds in the future
 *****************************************************************************/
static void
SetRelTimeout( struct timespec *time,
               int relMillis )
{
    struct timeb now;
    int sec = relMillis / 1000;
    int milliSeconds = relMillis % 1000;

    assert( time != NULL );

    ftime( &now );

    time->tv_sec = now.time + sec;
    time->tv_nsec = ( now.millitm + milliSeconds ) * 1000000;
}

/****************************************************************************
 * Function: StatsInit
 *
 *  Description:
 *      Initializes the statistics structure.
 *      Internal Only.
 *  Parameters:
 *      ThreadPoolStats *stats must be valid non null stats structure
 *****************************************************************************/
STATSONLY( static void StatsInit( ThreadPoolStats * stats ) {
           assert( stats != NULL ); stats->totalIdleTime = 0; stats->totalJobsHQ = 0; stats->totalJobsLQ = 0; stats->totalJobsMQ = 0; stats->totalTimeHQ = 0; stats->totalTimeMQ = 0; stats->totalTimeLQ = 0; stats->totalWorkTime = 0; stats->totalIdleTime = 0; stats->avgWaitHQ = 0; //average wait in HQ
           stats->avgWaitMQ = 0;    //average wait in MQ
           stats->avgWaitLQ = 0;
           stats->workerThreads = 0;
           stats->idleThreads = 0;
           stats->persistentThreads = 0;
           stats->maxThreads = 0; stats->totalThreads = 0;}
 )

/****************************************************************************
 * Function: CalcWaitTime
 *
 *  Description:
 *      Calculates the time the job has been waiting at the specified
 *      priority. Adds to the totalTime and totalJobs kept in the
 *      thread pool statistics structure.
 *      Internal Only.
 *
 *  Parameters:
 *      ThreadPool *tp
 *      ThreadPriority p
 *      ThreadPoolJob *job
 *****************************************************************************/
STATSONLY( static void CalcWaitTime( ThreadPool * tp,
                                     ThreadPriority p,
                                     ThreadPoolJob * job ) {
           struct timeb now;
           double diff;
           assert( tp != NULL );
           assert( job != NULL );
           ftime( &now );
           diff = DiffMillis( &now, &job->requestTime ); switch ( p ) {
case HIGH_PRIORITY:
tp->stats.totalJobsHQ++; tp->stats.totalTimeHQ += diff; break; case MED_PRIORITY:
tp->stats.totalJobsMQ++; tp->stats.totalTimeMQ += diff; break; case LOW_PRIORITY:
tp->stats.totalJobsLQ++; tp->stats.totalTimeLQ += diff; break; default:
           assert( 0 );}
           }

 )

/****************************************************************************
 * Function: SetSeed
 *
 *  Description:
 *      Sets seed for random number generator.
 *      Each thread sets the seed random number generator.
 *      Internal Only.
 *  Parameters:
 *      
 *****************************************************************************/
    static void SetSeed(  ) {
    struct timeb t;

    ftime( &t );
    srand( ( unsigned int )t.millitm + ithread_get_current_thread_id(  ) );
    }

/****************************************************************************
 * Function: WorkerThread
 *
 *  Description:
 *      Implements a thread pool worker.
 *      Worker waits for a job to become available.
 *      Worker picks up persistent jobs first, high priority, med priority,
 *             then low priority.
 *      If worker remains idle for more than specified max, the worker
 *      is released.
 *      Internal Only.
 *  Parameters:
 *      void * arg -> is cast to ThreadPool *
 *****************************************************************************/
    static void *WorkerThread( void *arg ) {

        STATSONLY( time_t start = 0;
             )

        ThreadPoolJob *job = NULL;
        ListNode *head = NULL;

        struct timespec timeout;
        int retCode = 0;
        int persistent = -1;
        ThreadPool *tp = ( ThreadPool * ) arg;

        assert( tp != NULL );

        //Increment total thread count
        ithread_mutex_lock( &tp->mutex );
        tp->totalThreads++;
        ithread_cond_broadcast( &tp->start_and_shutdown );
        ithread_mutex_unlock( &tp->mutex );

        SetSeed(  );

        STATSONLY( time( &start );
             );

        while( 1 ) {

            ithread_mutex_lock( &tp->mutex );

            if( job ) {

                FreeThreadPoolJob( tp, job );
                job = NULL;

            }

            retCode = 0;

            STATSONLY( tp->stats.idleThreads++;
                 );
            STATSONLY( tp->stats.totalWorkTime += ( time( NULL ) - start );
                 );             //work time
            STATSONLY( time( &start );
                 );             //idle time

            if( persistent == 1 ) {
                //Persistent thread
                //becomes a regular thread
                tp->persistentThreads--;
            }

            STATSONLY( if( persistent == 0 )
                       tp->stats.workerThreads--; );

            //Check for a job or shutdown
            while( ( tp->lowJobQ.size == 0 )
                   && ( tp->medJobQ.size == 0 )
                   && ( tp->highJobQ.size == 0 )
                   && ( !tp->persistentJob )
                   && ( !tp->shutdown ) ) {

                //If wait timed out
                //and we currently have more than the
                //min threads, or if we have more than the max threads
                // (only possible if the attributes have been reset)
                //let this thread die.

                if( ( ( retCode == ETIMEDOUT )
                      && ( ( tp->totalThreads ) > tp->attr.minThreads ) )
                    || ( ( tp->attr.maxThreads != -1 )
                         && ( ( tp->totalThreads ) >
                              tp->attr.maxThreads ) ) ) {

                    STATSONLY( tp->stats.idleThreads-- );

                    tp->totalThreads--;
                    ithread_cond_broadcast( &tp->start_and_shutdown );
                    ithread_mutex_unlock( &tp->mutex );

                    return NULL;
                }

                SetRelTimeout( &timeout, tp->attr.maxIdleTime );

                //wait for a job up to the specified max time
                retCode = ithread_cond_timedwait( &tp->condition,
                                                  &tp->mutex, &timeout );

            }

            STATSONLY( tp->stats.idleThreads--;
                 );
            STATSONLY( tp->stats.totalIdleTime += ( time( NULL ) - start );
                 );             //idle time
            STATSONLY( time( &start );
                 );             //work time

            //bump priority of starved jobs
            BumpPriority( tp );

            //if shutdown then stop
            if( tp->shutdown ) {
                tp->totalThreads--;

                ithread_cond_broadcast( &tp->start_and_shutdown );

                ithread_mutex_unlock( &tp->mutex );

                return NULL;
            } else {

                //Pick up persistent job if available
                if( tp->persistentJob ) {

                    job = tp->persistentJob;
                    tp->persistentJob = NULL;
                    tp->persistentThreads++;
                    persistent = 1;
                    ithread_cond_broadcast( &tp->start_and_shutdown );

                } else {
                    STATSONLY( tp->stats.workerThreads++ );
                    persistent = 0;

                    //Pick the highest priority job
                    if( tp->highJobQ.size > 0 ) {
                        head = ListHead( &tp->highJobQ );
                        job = ( ThreadPoolJob * ) head->item;
                        STATSONLY( CalcWaitTime
                                   ( tp, HIGH_PRIORITY, job ) );
                        ListDelNode( &tp->highJobQ, head, 0 );

                    } else if( tp->medJobQ.size > 0 ) {
                        head = ListHead( &tp->medJobQ );
                        job = ( ThreadPoolJob * ) head->item;
                        STATSONLY( CalcWaitTime( tp, MED_PRIORITY, job ) );
                        ListDelNode( &tp->medJobQ, head, 0 );

                    } else if( tp->lowJobQ.size > 0 ) {
                        head = ListHead( &tp->lowJobQ );
                        job = ( ThreadPoolJob * ) head->item;
                        STATSONLY( CalcWaitTime( tp, LOW_PRIORITY, job ) );
                        ListDelNode( &tp->lowJobQ, head, 0 );

                    } else {

                        // Should never get here
                        assert( 0 );
                        STATSONLY( tp->stats.workerThreads-- );
                        tp->totalThreads--;
                        ithread_cond_broadcast( &tp->start_and_shutdown );
                        ithread_mutex_unlock( &tp->mutex );

                        return NULL;
                    }

                }
            }

            ithread_mutex_unlock( &tp->mutex );

            if( SetPriority( job->priority ) != 0 ) {
                // In the future can log
                // info
            } else {
                // In the future can log
                // info
            }

            //run the job

            job->func( job->arg );

            //return to Normal
            SetPriority( DEFAULT_PRIORITY );

        }
    }

/****************************************************************************
 * Function: CreateThreadPoolJob
 *
 *  Description:
 *      Creates a Thread Pool Job. (Dynamically allocated)
 *      Internal to thread pool.
 *  Parameters:
 *      ThreadPoolJob * job - job is copied
 *      id - id of job
 *
 *  Returns:
 *      ThreadPoolJob * on success, NULL on failure.
 *****************************************************************************/
    static ThreadPoolJob *CreateThreadPoolJob( ThreadPoolJob * job,
                                               int id,
                                               ThreadPool * tp ) {

        ThreadPoolJob *newJob = NULL;

        assert( job != NULL );
        assert( tp != NULL );

        newJob = ( ThreadPoolJob * ) FreeListAlloc( &tp->jobFreeList );

        if( newJob ) {
            ( *newJob ) = ( *job );
            newJob->jobId = id;
            ftime( &newJob->requestTime );
        }
        return newJob;
    }

/****************************************************************************
 * Function: CreateWorker
 *
 *  Description:
 *      Creates a worker thread, if the thread pool
 *      does not already have max threads.
 *      Internal to thread pool.
 *  Parameters:
 *      ThreadPool *tp
 *
 *  Returns:
 *      0 on success, <0 on failure
 *      EMAXTHREADS if already max threads reached
 *      EAGAIN if system can not create thread
 *
 *****************************************************************************/
    static int CreateWorker( ThreadPool * tp ) {
        ithread_t temp;
        int rc = 0;
        int currentThreads = tp->totalThreads + 1;

        assert( tp != NULL );

        if( ( tp->attr.maxThreads != INFINITE_THREADS )
            && ( currentThreads > tp->attr.maxThreads ) ) {
            return EMAXTHREADS;
        }

        rc = ithread_create( &temp, NULL, WorkerThread, tp );

        if( rc == 0 ) {

            rc = ithread_detach( temp );

            while( tp->totalThreads < currentThreads ) {

                ithread_cond_wait( &tp->start_and_shutdown, &tp->mutex );

            }

        }

        STATSONLY( if( tp->stats.maxThreads < tp->totalThreads ) {
                   tp->stats.maxThreads = tp->totalThreads;}
         )

            return rc;
    }

/****************************************************************************
 * Function: AddWorker
 *
 *  Description:
 *      Determines whether or not a thread should be added
 *      based on the jobsPerThread ratio.
 *      Adds a thread if appropriate.
 *      Internal to Thread Pool.
 *  Parameters:
 *      ThreadPool* tp
 *
 *****************************************************************************/
    static void AddWorker( ThreadPool * tp ) {
        int jobs = 0;
        int threads = 0;

        assert( tp != NULL );

        jobs = tp->highJobQ.size + tp->lowJobQ.size + tp->medJobQ.size;

        threads = tp->totalThreads - tp->persistentThreads;
//+++Patch by shiang for WSC
#if 0
        while( ( threads == 0 )
               || ( ( jobs / threads ) > tp->attr.jobsPerThread ) ) {

            if( CreateWorker( tp ) != 0 )
                return;
            threads++;
        }
#else
        while( ( threads == 0 )
               || ( ( jobs / threads ) >= tp->attr.jobsPerThread ) ) {

            if( CreateWorker( tp ) != 0 )
                return;
            threads++;
        }

#endif
//---Patch by shiang for WSC
    }

/****************************************************************************
 * Function: ThreadPoolInit
 *
 *  Description:
 *      Initializes and starts ThreadPool. Must be called first.
 *      And only once for ThreadPool.
 *  Parameters:
 *      tp  - must be valid, non null, pointer to ThreadPool.
 *      minWorkerThreads - minimum number of worker threads
 *                         thread pool will never have less than this
 *                         number of threads.
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
    int ThreadPoolInit( ThreadPool * tp,
                        ThreadPoolAttr * attr ) {
        int retCode = 0;
        int i = 0;

        assert( tp != NULL );

        if( tp == NULL ) {
            return EINVAL;
        }

        retCode += ithread_mutex_init( &tp->mutex, NULL );
        assert( retCode == 0 );

        retCode += ithread_mutex_lock( &tp->mutex );
        assert( retCode == 0 );

        retCode += ithread_cond_init( &tp->condition, NULL );
        assert( retCode == 0 );

        retCode += ithread_cond_init( &tp->start_and_shutdown, NULL );
        assert( retCode == 0 );

        if( retCode != 0 ) {
            return EAGAIN;
        }

        if( attr ) {
            tp->attr = ( *attr );
        } else {
            TPAttrInit( &tp->attr );
        }

        if( SetPolicyType( tp->attr.schedPolicy ) != 0 ) {
            ithread_mutex_unlock( &tp->mutex );
            ithread_mutex_destroy( &tp->mutex );
            ithread_cond_destroy( &tp->condition );
            ithread_cond_destroy( &tp->start_and_shutdown );
            return INVALID_POLICY;
        }

        retCode += FreeListInit( &tp->jobFreeList, sizeof( ThreadPoolJob ),
                                 JOBFREELISTSIZE );
        assert( retCode == 0 );

        STATSONLY( StatsInit( &tp->stats ) );

        retCode += ListInit( &tp->highJobQ, CmpThreadPoolJob, NULL );
        assert( retCode == 0 );

        retCode += ListInit( &tp->medJobQ, CmpThreadPoolJob, NULL );
        assert( retCode == 0 );

        retCode += ListInit( &tp->lowJobQ, CmpThreadPoolJob, NULL );
        assert( retCode == 0 );

        if( retCode != 0 ) {
            retCode = EAGAIN;
        } else {

            tp->persistentJob = NULL;
            tp->lastJobId = 0;
            tp->shutdown = 0;
            tp->totalThreads = 0;
            tp->persistentThreads = 0;

            for( i = 0; i < tp->attr.minThreads; i++ ) {

                if( ( retCode = CreateWorker( tp ) ) != 0 ) {
                    break;
                }
            }
        }

        ithread_mutex_unlock( &tp->mutex );

        if( retCode != 0 ) {
            //clean up if the min threads could
            //not be created
            ThreadPoolShutdown( tp );
        }

        return retCode;
    }

/****************************************************************************
 * Function: ThreadPoolAddPersistent
 *
 *  Description:
 *      Adds a long term job to the thread pool.
 *      Job will be run as soon as possible.
 *      Call will block until job is scheduled.
 *  Parameters:
 *      tp - valid thread pool pointer
 *      job-> valid ThreadPoolJob pointer with following fields
 *          func - ThreadFunction to run
 *          arg - argument to function.
 *          priority - priority of job.
 *          free_function - function to use when freeing argument
 *  Returns:
 *      0 on success, nonzero on failure
 *      EOUTOFMEM not enough memory to add job.
 *      EMAXTHREADS not enough threads to add persistent job.
 *****************************************************************************/
    int ThreadPoolAddPersistent( ThreadPool * tp,
                                 ThreadPoolJob * job,
                                 int *jobId ) {
        int tempId = -1;

        ThreadPoolJob *temp = NULL;

        assert( tp != NULL );
        assert( job != NULL );

        if( ( tp == NULL ) || ( job == NULL ) ) {
            return EINVAL;
        }

        if( jobId == NULL )
            jobId = &tempId;

        ( *jobId ) = INVALID_JOB_ID;

        ithread_mutex_lock( &tp->mutex );

        assert( ( job->priority == LOW_PRIORITY )
                || ( job->priority == MED_PRIORITY )
                || ( job->priority == HIGH_PRIORITY ) );

        //Create A worker if less than max threads running
        if( tp->totalThreads < tp->attr.maxThreads ) {
            CreateWorker( tp );
        } else {
            //if there is more than one worker thread
            //available then schedule job, otherwise fail
            if( ( tp->totalThreads - tp->persistentThreads ) - 1 == 0 ) {
                ithread_mutex_unlock( &tp->mutex );
                return EMAXTHREADS;
            }
        }

        temp = CreateThreadPoolJob( job, tp->lastJobId, tp );

        if( temp == NULL ) {
            ithread_mutex_unlock( &tp->mutex );
            return EOUTOFMEM;
        }

        tp->persistentJob = temp;

        //Notify a waiting thread

        ithread_cond_signal( &tp->condition );

        //wait until long job has been picked up
        while( tp->persistentJob != NULL ) {
            ithread_cond_wait( &tp->start_and_shutdown, &tp->mutex );
        }

        ( *jobId ) = tp->lastJobId++;
        ithread_mutex_unlock( &tp->mutex );
        return 0;
    }

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
 *      jobId - id of job
 *      duration - whether or not this is a persistent thread
 *      free_function - function to use when freeing argument
 *  Returns:
 *      0 on success, nonzero on failure
 *      EOUTOFMEM if not enough memory to add job.
 *****************************************************************************/
    int ThreadPoolAdd( ThreadPool * tp,
                       ThreadPoolJob * job,
                       int *jobId ) {
        int rc = EOUTOFMEM;

        int tempId = -1;

        ThreadPoolJob *temp = NULL;

        assert( tp != NULL );
        assert( job != NULL );

        if( ( tp == NULL ) || ( job == NULL ) ) {
            return EINVAL;
        }

        ithread_mutex_lock( &tp->mutex );

        assert( ( job->priority == LOW_PRIORITY )
                || ( job->priority == MED_PRIORITY )
                || ( job->priority == HIGH_PRIORITY ) );

        if( jobId == NULL )
            jobId = &tempId;

        ( *jobId ) = INVALID_JOB_ID;

        temp = CreateThreadPoolJob( job, tp->lastJobId, tp );

        if( temp == NULL ) {
            ithread_mutex_unlock( &tp->mutex );
            return rc;
        }

        if( job->priority == HIGH_PRIORITY ) {
            if( ListAddTail( &tp->highJobQ, temp ) )
                rc = 0;
        } else if( job->priority == MED_PRIORITY ) {
            if( ListAddTail( &tp->medJobQ, temp ) )
                rc = 0;
        } else {
            if( ListAddTail( &tp->lowJobQ, temp ) )
                rc = 0;
        }

        //AddWorker if appropriate
        AddWorker( tp );

        //Notify a waiting thread
        if( rc == 0 ) {
            ithread_cond_signal( &tp->condition );

        } else {
            FreeThreadPoolJob( tp, temp );
        }

        ( *jobId ) = tp->lastJobId++;

        ithread_mutex_unlock( &tp->mutex );
        return rc;
    }

/****************************************************************************
 * Function: ThreadPoolRemove
 *
 *  Description:
 *      Removes a job from the thread pool.
 *      Can only remove jobs which are not
 *      currently running.
 *  Parameters:
 *      tp - valid thread pool pointer
 *      jobId - id of job
 *      ThreadPoolJob *out - space for removed job.
 *                           Can be null if not needed.
 *
 *  Returns:
 *      0 on success. INVALID_JOB_ID on failure.
 *****************************************************************************/
    int ThreadPoolRemove( ThreadPool * tp,
                          int jobId,
                          ThreadPoolJob * out ) {
        ThreadPoolJob *temp = NULL;
        int ret = INVALID_JOB_ID;
        ListNode *tempNode = NULL;
        ThreadPoolJob dummy;

        assert( tp != NULL );

        if( tp == NULL ) {
            return EINVAL;
        }

        if( out == NULL ) {
            out = &dummy;
        }

        dummy.jobId = jobId;

        ithread_mutex_lock( &tp->mutex );

        tempNode = ListFind( &tp->highJobQ, NULL, &dummy );

        if( tempNode ) {
            temp = ( ThreadPoolJob * ) tempNode->item;
            ( *out ) = ( *temp );
            ListDelNode( &tp->highJobQ, tempNode, 0 );
            FreeThreadPoolJob( tp, temp );
            ithread_mutex_unlock( &tp->mutex );
            return 0;
        }

        tempNode = ListFind( &tp->medJobQ, NULL, &dummy );

        if( tempNode ) {
            temp = ( ThreadPoolJob * ) tempNode->item;
            ( *out ) = ( *temp );
            ListDelNode( &tp->medJobQ, tempNode, 0 );
            FreeThreadPoolJob( tp, temp );
            ithread_mutex_unlock( &tp->mutex );
            return 0;
        }

        tempNode = ListFind( &tp->lowJobQ, NULL, &dummy );

        if( tempNode ) {
            temp = ( ThreadPoolJob * ) tempNode->item;
            ( *out ) = ( *temp );
            ListDelNode( &tp->lowJobQ, tempNode, 0 );
            FreeThreadPoolJob( tp, temp );
            ithread_mutex_unlock( &tp->mutex );
            return 0;
        }

        if( ( tp->persistentJob )
            && ( tp->persistentJob->jobId == jobId ) ) {
            ( *out ) = ( *tp->persistentJob );
            FreeThreadPoolJob( tp, tp->persistentJob );
            tp->persistentJob = NULL;
            ithread_mutex_unlock( &tp->mutex );
            return 0;
        }

        ithread_mutex_unlock( &tp->mutex );
        return ret;
    }

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
    int ThreadPoolGetAttr( ThreadPool * tp,
                           ThreadPoolAttr * out ) {
        assert( tp != NULL );

        assert( out != NULL );

        if( ( tp == NULL ) || ( out == NULL ) ) {
            return EINVAL;
        }

        if( !tp->shutdown ) {
            ithread_mutex_lock( &tp->mutex );
        }

        ( *out ) = tp->attr;

        if( !tp->shutdown ) {
            ithread_mutex_unlock( &tp->mutex );
        }

        return 0;
    }

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
    int ThreadPoolSetAttr( ThreadPool * tp,
                           ThreadPoolAttr * attr ) {
        int retCode = 0;
        ThreadPoolAttr temp;
        int i = 0;

        assert( tp != NULL );

        if( tp == NULL ) {
            return EINVAL;
        }
        ithread_mutex_lock( &tp->mutex );

        if( attr != NULL ) {
            temp = ( *attr );
        } else {
            TPAttrInit( &temp );
        }

        if( SetPolicyType( temp.schedPolicy ) != 0 ) {
            ithread_mutex_unlock( &tp->mutex );
            return INVALID_POLICY;
        }

        tp->attr = ( temp );

        if( tp->totalThreads < tp->attr.minThreads )    //add threads
        {
            for( i = tp->totalThreads; i < tp->attr.minThreads; i++ ) {

                if( ( retCode = CreateWorker( tp ) ) != 0 ) {
                    break;
                }
            }
        }

        ithread_cond_signal( &tp->condition );  //signal changes 

        ithread_mutex_unlock( &tp->mutex );

        if( retCode != 0 ) {
            //clean up if the min threads could
            //not be created
            ThreadPoolShutdown( tp );
        }

        return retCode;
    }

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
    int ThreadPoolShutdown( ThreadPool * tp ) {

        ListNode *head = NULL;
        ThreadPoolJob *temp = NULL;

        assert( tp != NULL );

        if( tp == NULL ) {
            return EINVAL;
        }

        ithread_mutex_lock( &tp->mutex );

        //clean up high priority jobs
        while( tp->highJobQ.size ) {
            head = ListHead( &tp->highJobQ );
            temp = ( ThreadPoolJob * ) head->item;
            if( temp->free_func )
                temp->free_func( temp->arg );
            FreeThreadPoolJob( tp, temp );
            ListDelNode( &tp->highJobQ, head, 0 );
        }

        ListDestroy( &tp->highJobQ, 0 );

        //clean up med priority jobs
        while( tp->medJobQ.size ) {
            head = ListHead( &tp->medJobQ );
            temp = ( ThreadPoolJob * ) head->item;
            if( temp->free_func )
                temp->free_func( temp->arg );
            FreeThreadPoolJob( tp, temp );
            ListDelNode( &tp->medJobQ, head, 0 );
        }

        ListDestroy( &tp->medJobQ, 0 );

        //clean up low priority jobs
        while( tp->lowJobQ.size ) {
            head = ListHead( &tp->lowJobQ );
            temp = ( ThreadPoolJob * ) head->item;
            if( temp->free_func )
                temp->free_func( temp->arg );
            FreeThreadPoolJob( tp, temp );
            ListDelNode( &tp->lowJobQ, head, 0 );
        }

        ListDestroy( &tp->lowJobQ, 0 );

        //clean up long term job
        if( tp->persistentJob ) {
            temp = tp->persistentJob;
            if( temp->free_func )
                temp->free_func( temp->arg );
            FreeThreadPoolJob( tp, temp );
            tp->persistentJob = NULL;
        }

        tp->shutdown = 1;
        ithread_cond_broadcast( &tp->condition );   //signal shutdown

        //wait for all threads to finish
        while( tp->totalThreads > 0 ) {

            ithread_cond_wait( &tp->start_and_shutdown, &tp->mutex );
        }

        //destroy condition
        while( ithread_cond_destroy( &tp->condition ) != 0 ) {
        }

        while( ithread_cond_destroy( &tp->start_and_shutdown ) != 0 ) {
        }

        FreeListDestroy( &tp->jobFreeList );

        ithread_mutex_unlock( &tp->mutex );

        //destroy mutex
        while( ithread_mutex_destroy( &tp->mutex ) != 0 ) {
        }

        return 0;
    }

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
    int TPAttrInit( ThreadPoolAttr * attr ) {
        assert( attr != NULL );

        if( attr == NULL ) {
            return EINVAL;
        }

        attr->jobsPerThread = DEFAULT_JOBS_PER_THREAD;
        attr->maxIdleTime = DEFAULT_IDLE_TIME;
        attr->maxThreads = DEFAULT_MAX_THREADS;
        attr->minThreads = DEFAULT_MIN_THREADS;
        attr->schedPolicy = DEFAULT_POLICY;
        attr->starvationTime = DEFAULT_STARVATION_TIME;
        return 0;
    }

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
    int TPJobInit( ThreadPoolJob * job,
                   start_routine func,
                   void *arg ) {
        assert( job != NULL );
        assert( func != NULL );

        if( ( job == NULL ) || ( func == NULL ) ) {
            return EINVAL;
        }

        job->func = func;
        job->arg = arg;
        job->priority = DEFAULT_PRIORITY;
        job->free_func = DEFAULT_FREE_ROUTINE;
        return 0;
    }

/****************************************************************************
 * Function: TPJobSetPriority
 *
 *  Description:
 *      Sets the max threads for the thread pool attributes.
 *  Parameters:
 *      attr - must be valid thread pool attributes.
 *      maxThreads - value to set
 *  Returns:
 *      Returns 0 on success nonzero on failure.
 *      Returns EINVAL if invalid priority.
 *****************************************************************************/
    int TPJobSetPriority( ThreadPoolJob * job,
                          ThreadPriority priority ) {
        assert( job != NULL );

        if( job == NULL ) {
            return EINVAL;
        }

        if( priority == LOW_PRIORITY || priority == MED_PRIORITY ||
            priority == HIGH_PRIORITY ) {
            job->priority = priority;
            return 0;
        } else {
            return EINVAL;
        }
    }

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
    int TPJobSetFreeFunction( ThreadPoolJob * job,
                              free_routine func ) {
        assert( job != NULL );

        if( job == NULL ) {
            return EINVAL;
        }

        job->free_func = func;
        return 0;
    }

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
    int TPAttrSetMaxThreads( ThreadPoolAttr * attr,
                             int maxThreads ) {
        assert( attr != NULL );

        if( attr == NULL ) {
            return EINVAL;
        }

        attr->maxThreads = maxThreads;
        return 0;
    }

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
    int TPAttrSetMinThreads( ThreadPoolAttr * attr,
                             int minThreads ) {
        assert( attr != NULL );

        if( attr == NULL ) {
            return EINVAL;
        }

        attr->minThreads = minThreads;
        return 0;
    }

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
    int TPAttrSetIdleTime( ThreadPoolAttr * attr,
                           int idleTime ) {
        assert( attr != NULL );

        if( attr == NULL ) {
            return EINVAL;
        }

        attr->maxIdleTime = idleTime;
        return 0;
    }

/****************************************************************************
 * Function: TPAttrSetJobsPerThread
 *
 *  Description:
 *      Sets the max thre
 *  Parameters:
 *      attr - must be valid thread pool attributes.
 *  Returns:
 *      Always returns 0.
 *****************************************************************************/
    int TPAttrSetJobsPerThread( ThreadPoolAttr * attr,
                                int jobsPerThread ) {
        assert( attr != NULL );

        if( attr == NULL ) {
            return EINVAL;
        }

        attr->jobsPerThread = jobsPerThread;
        return 0;
    }

/****************************************************************************
 * Function: TPAttrSetStarvationTime
 *
 *  Description:
 *      Sets the starvation time for the thread pool attributes.
 *  Parameters:
 *      attr - must be valid thread pool attributes.
 *  Returns:
 *      Always returns 0.
 *****************************************************************************/
    int TPAttrSetStarvationTime( ThreadPoolAttr * attr,
                                 int starvationTime ) {
        assert( attr != NULL );

        if( attr == NULL ) {
            return EINVAL;
        }

        attr->starvationTime = starvationTime;
        return 0;
    }

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
    int TPAttrSetSchedPolicy( ThreadPoolAttr * attr,
                              PolicyType schedPolicy ) {
        assert( attr != NULL );
        if( attr == NULL ) {
            return EINVAL;
        }
        attr->schedPolicy = schedPolicy;
        return 0;
    }

    STATSONLY( void ThreadPoolPrintStats( ThreadPoolStats * stats ) {
               assert( stats != NULL ); if( stats == NULL ) {
               return;}

               printf( "ThreadPoolStats at Time: %ld\n", time( NULL ) );
               printf
               ( "Average Wait in High Priority Q in milliseconds: %lf\n",
                 stats->avgWaitHQ );
               printf
               ( "Average Wait in Med Priority Q in milliseconds: %lf\n",
                 stats->avgWaitMQ );
               printf
               ( "Averate Wait in Low Priority Q in milliseconds: %lf\n",
                 stats->avgWaitLQ );
               printf( "Max Threads Active: %d\n", stats->maxThreads );
               printf( "Current Worker Threads: %d\n",
                       stats->workerThreads );
               printf( "Current Persistent Threads: %d\n",
                       stats->persistentThreads );
               printf( "Current Idle Threads: %d\n", stats->idleThreads );
               printf( "Total Threads : %d\n", stats->totalThreads );
               printf( "Total Time spent Working in seconds: %lf\n",
                       stats->totalWorkTime );
               printf( "Total Time spent Idle in seconds : %lf\n",
                       stats->totalIdleTime );}
     )

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
        STATSONLY( int
                   ThreadPoolGetStats( ThreadPool * tp,
                                       ThreadPoolStats * stats ) {

                   assert( tp != NULL );
                   assert( stats != NULL );
                   if( ( tp == NULL ) || ( stats == NULL ) ) {
                   return EINVAL;}

                   //if not shutdown then acquire mutex
                   if( !tp->shutdown ) {
                   ithread_mutex_lock( &tp->mutex );}

                   ( *stats ) = tp->stats; if( stats->totalJobsHQ > 0 )
                   stats->avgWaitHQ =
                   stats->totalTimeHQ / stats->totalJobsHQ;
                   else
                   stats->avgWaitHQ = 0; if( stats->totalJobsMQ > 0 )
                   stats->avgWaitMQ =
                   stats->totalTimeMQ / stats->totalJobsMQ;
                   else
                   stats->avgWaitMQ = 0; if( stats->totalJobsLQ > 0 )
                   stats->avgWaitLQ =
                   stats->totalTimeLQ / stats->totalJobsLQ;
                   else
                   stats->avgWaitLQ = 0;
                   stats->totalThreads = tp->totalThreads;
                   stats->persistentThreads = tp->persistentThreads;
                   stats->currentJobsHQ = ListSize( &tp->highJobQ );
                   stats->currentJobsLQ = ListSize( &tp->lowJobQ );
                   stats->currentJobsMQ = ListSize( &tp->medJobQ );
                   //if not shutdown then release mutex
                   if( !tp->shutdown ) {
                   ithread_mutex_unlock( &tp->mutex );}

                   return 0;}

 )
