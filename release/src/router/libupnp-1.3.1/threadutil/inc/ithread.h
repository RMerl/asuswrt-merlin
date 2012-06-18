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

#ifndef ITHREADH
#define ITHREADH
#ifdef __cplusplus
extern "C" {
#endif


#ifdef DEBUG
#define DEBUG_ONLY(x) x
#else
#define DEBUG_ONLY(x)
#endif

#include <pthread.h>
#include <unistd.h>

#define ITHREAD_MUTEX_FAST_NP PTHREAD_MUTEX_FAST_NP
#define ITHREAD_MUTEX_RECURSIVE_NP PTHREAD_MUTEX_RECURSIVE_NP
#define ITHREAD_MUTEX_ERRORCHECK_NP PTHREAD_MUTEX_ERRORCHECK_NP
#define ITHREAD_CANCELED PTHREAD_CANCELED
  
  
  /***************************************************************************
   * Name: ithread_t
   *
   *  Description:
   *      Thread handle.
   *      typedef to pthread_t.
   *      Internal Use Only.
   ***************************************************************************/
  typedef pthread_t ithread_t; 
  
  /****************************************************************************
   * Name: ithread_attr_t
   *
   *  Description:
   *      Thread attribute.
   *      typedef to pthread_attr_t
   *      Internal Use Only
   ***************************************************************************/
  typedef pthread_attr_t ithread_attr_t;	


  /****************************************************************************
   * Name: start_routine
   *
   *  Description:
   *      Thread start routine 
   *      Internal Use Only.
   ***************************************************************************/
  typedef void * (*start_routine) (void *arg);

  
  /****************************************************************************
   * Name: ithread_cond_t
   *
   *  Description:
   *      condition variable.
   *      typedef to pthread_cond_t
   *      Internal Use Only.
   ***************************************************************************/
  typedef pthread_cond_t ithread_cond_t;


  /****************************************************************************
   * Name: ithread_mutexattr_t
   *
   *  Description:
   *      Mutex attribute.
   *      typedef to pthread_mutexattr_t
   *      Internal Use Only
   ***************************************************************************/
  typedef pthread_mutexattr_t ithread_mutexattr_t;	


  /****************************************************************************
   * Name: ithread_mutex_t
   *
   *  Description:
   *      Mutex.
   *      typedef to pthread_mutex_t
   *      Internal Use Only.
   ***************************************************************************/
  typedef pthread_mutex_t ithread_mutex_t;


  /****************************************************************************
   * Name: ithread_condattr_t
   *
   *  Description:
   *      Condition attribute.
   *      typedef to pthread_condattr_t
   *      NOT USED
   *      Internal Use Only
   ***************************************************************************/
  typedef pthread_condattr_t ithread_condattr_t;	

  /****************************************************************************
   * Function: ithread_mutexattr_init
   *
   *  Description:
   *      Initializes a mutex attribute variable.
   *      Used to set the type of the mutex.
   *  Parameters:
   *      ithread_mutexattr_init * attr (must be valid non NULL pointer to 
   *                                     pthread_mutexattr_t)
   *  Returns:
   *      0 on success, Nonzero on failure.
   *      Always returns 0.
   *      See man page for pthread_mutexattr_init
   ***************************************************************************/
  
#define ithread_mutexattr_init pthread_mutexattr_init
  
  /****************************************************************************
   * Function: ithread_mutexattr_destroy
   *
   *  Description:
   *      Releases any resources held by the mutex attribute.
   *      Currently there are no resources associated with the attribute
   *  Parameters:
   *      ithread_mutexattr_t * attr (must be valid non NULL pointer to 
   *                                  pthread_mutexattr_t)
   *  Returns:
   *      0 on success, Nonzero on failure.
   *      Always returns 0.
   *      See man page for pthread_mutexattr_destroy
   ***************************************************************************/
#define ithread_mutexattr_destroy pthread_mutexattr_destroy
  
  
/****************************************************************************
 * Function: ithread_mutexattr_setkind_np
 *
 *  Description:
 *      Sets the mutex type in the attribute.
 *      Valid types are: ITHREAD_MUTEX_FAST_NP 
 *                       ITHREAD_MUTEX_RECURSIVE_NP 
 *                       ITHREAD_MUTEX_ERRORCHECK_NP
 *
 *  Parameters:
 *      ithread_mutexattr_t * mutex (must be valid non NULL pointer to 
 *                                   ithread_mutexattr_t)
 *      int kind (one of ITHREAD_MUTEX_FAST_NP or ITHREAD_MUTEX_RECURSIVE_NP
 *                or ITHREAD_MUTEX_ERRORCHECK_NP)
 *  Returns:
 *      0 on success. Nonzero on failure.
 *      Returns EINVAL if the kind is not supported.
 *      See man page for pthread_mutexattr_setkind_np
 *****************************************************************************/
#define ithread_mutexattr_setkind_np pthread_mutexattr_setkind_np


/****************************************************************************
 * Function: ithread_mutexattr_getkind_np
 *
 *  Description:
 *      Gets the mutex type in the attribute.
 *      Valid types are: ITHREAD_MUTEX_FAST_NP 
 *                       ITHREAD_MUTEX_RECURSIVE_NP 
 *                       ITHREAD_MUTEX_ERRORCHECK_NP
 *
 *  Parameters:
 *      ithread_mutexattr_t * mutex (must be valid non NULL pointer to 
 *                                   pthread_mutexattr_t)
 *      int *kind (one of ITHREAD_MUTEX_FAST_NP or ITHREAD_MUTEX_RECURSIVE_NP
 *                or ITHREAD_MUTEX_ERRORCHECK_NP)
 *  Returns:
 *      0 on success. Nonzero on failure.
 *      Always returns 0.
 *      See man page for pthread_mutexattr_getkind_np
 *****************************************************************************/
#define ithread_mutexattr_getkind_np pthread_mutexattr_getkind_np

  
/****************************************************************************
 * Function: ithread_mutex_init
 *
 *  Description:
 *      Initializes mutex.
 *      Must be called before use.
 *      
 *  Parameters:
 *      ithread_mutex_t * mutex (must be valid non NULL pointer to pthread_mutex_t)
 *      const ithread_mutexattr_t * mutex_attr 
 *  Returns:
 *      0 on success, Nonzero on failure.
 *      Always returns 0.
 *      See man page for pthread_mutex_init
 *****************************************************************************/
#define ithread_mutex_init pthread_mutex_init
  
/****************************************************************************
 * Function: ithread_mutex_lock
 *
 *  Description:
 *      Locks mutex.
 *  Parameters:
 *      ithread_mutex_t * mutex (must be valid non NULL pointer to pthread_mutex_t)
 *      mutex must be initialized.
 *      
 *  Returns:
 *      0 on success, Nonzero on failure.
 *      Always returns 0.
 *      See man page for pthread_mutex_lock
 *****************************************************************************/
#define ithread_mutex_lock pthread_mutex_lock
  

/****************************************************************************
 * Function: ithread_mutex_unlock
 *
 *  Description:
 *      Unlocks mutex.
 *
 *  Parameters:
 *      ithread_mutex_t * mutex (must be valid non NULL pointer to pthread_mutex_t)
 *      mutex must be initialized.
 *      
 *  Returns:
 *      0 on success, Nonzero on failure.
 *      Always returns 0.
 *      See man page for pthread_mutex_unlock
 *****************************************************************************/
#define ithread_mutex_unlock pthread_mutex_unlock


/****************************************************************************
 * Function: ithread_mutex_destroy
 *
 *  Description:
 *      Releases any resources held by the mutex. 
 *		Mutex can no longer be used after this call.
 *		Mutex is only destroyed when there are no longer any threads waiting on it. 
 *		Mutex cannot be destroyed if it is locked.
 *  Parameters:
 *      ithread_mutex_t * mutex (must be valid non NULL pointer to pthread_mutex_t)
 *      mutex must be initialized.
 *  Returns:
 *      0 on success. Nonzero on failure.
 *      Always returns 0.
 *      See man page for pthread_mutex_destroy
 *****************************************************************************/
#define ithread_mutex_destroy pthread_mutex_destroy

  
/****************************************************************************
 * Function: ithread_cond_init
 *
 *  Description:
 *      Initializes condition variable.
 *      Must be called before use.
 *  Parameters:
 *      ithread_cond_t * cond (must be valid non NULL pointer to pthread_cond_t)
 *      const ithread_condattr_t * cond_attr (ignored)
 *  Returns:
 *      0 on success, Nonzero on failure.
 *      See man page for pthread_cond_init
 *****************************************************************************/
#define ithread_cond_init pthread_cond_init



/****************************************************************************
 * Function: ithread_cond_signal
 *
 *  Description:
 *      Wakes up exactly one thread waiting on condition.
 *      Associated mutex MUST be locked by thread before entering this call.
 *  Parameters:
 *      ithread_cond_t * cond (must be valid non NULL pointer to 
 *      ithread_cond_t)
 *      cond must be initialized
 *  Returns:
 *      0 on success, Nonzero on failure.
 *      See man page for pthread_cond_signal
 *****************************************************************************/
#define ithread_cond_signal pthread_cond_signal


/****************************************************************************
 * Function: ithread_cond_broadcast
 *
 *  Description:
 *      Wakes up all threads waiting on condition.
 *      Associated mutex MUST be locked by thread before entering this call.
 *  Parameters:
 *      ithread_cond_t * cond (must be valid non NULL pointer to 
 *      ithread_cond_t)
 *      cond must be initialized
 *  Returns:
 *      0 on success, Nonzero on failure.
 *      See man page for pthread_cond_broadcast
 *****************************************************************************/
#define ithread_cond_broadcast pthread_cond_broadcast
  

/****************************************************************************
 * Function: ithread_cond_wait
 *
 *  Description:
 *      Atomically releases mutex and waits on condition.
 *      Associated mutex MUST be locked by thread before entering this call.
 *      Mutex is reacquired when call returns.
 *  Parameters:
 *      ithread_cond_t * cond (must be valid non NULL pointer to 
 *      ithread_cond_t)
 *      cond must be initialized
 *      ithread_mutex_t *mutex (must be valid non NULL pointer to 
 *      ithread_mutex_t)
 *      Mutex must be locked.
 *  Returns:
 *      0 on success, Nonzero on failure.
 *      See man page for pthread_cond_wait
 *****************************************************************************/
#define ithread_cond_wait pthread_cond_wait
  

  /****************************************************************************
   * Function: pthread_cond_timedwait
   *
   *  Description:      
   *      Atomically releases the associated mutex and waits on the condition. 
   *		If the condition is not signaled in the specified time 
   *              than the 
   *		call times out and returns.
   *		Associated mutex MUST be locked by thread before entering 
   *              this call.
   *      Mutex is reacquired when call returns.
   *  Parameters:
   *      ithread_cond_t * cond (must be valid non NULL pointer to 
   *      ithread_cond_t)
   *      cond must be initialized
   *      ithread_mutex_t *mutex (must be valid non NULL pointer to 
   *      ithread_mutex_t)
   *      Mutex must be locked.
   *      const struct timespec *abstime (absolute time, measured 
   *      from Jan 1, 1970)
   *  Returns:
   *      0 on success. ETIMEDOUT on timeout. Nonzero on failure.
   *      See man page for pthread_cond_timedwait
   ***************************************************************************/
 
#define ithread_cond_timedwait pthread_cond_timedwait
  

  /****************************************************************************
   * Function: ithread_cond_destroy
   *
   *  Description:
   *      Releases any resources held by the condition variable. 
   *		Condition variable can no longer be used after this call.	
   *  Parameters:
   *      ithread_cond_t * cond (must be valid non NULL pointer to 
   *      ithread_cond_t)
   *      cond must be initialized.
   *  Returns:
   *      0 on success. Nonzero on failure.
   *      See man page for pthread_cond_destroy
   ***************************************************************************/
#define ithread_cond_destroy pthread_cond_destroy


  /****************************************************************************
   * Function: ithread_create
   *
   *  Description:
   *		Creates a thread with the given start routine
   *      and argument.
   *  Parameters:
   *      ithread_t * thread (must be valid non NULL pointer to pthread_t)
   *      ithread_attr_t *attr, IGNORED
   *      void * (start_routine) (void *arg) (start routine)
   *      void * arg - argument.
   *  Returns:
   *      0 on success. Nonzero on failure.
   *	    Returns EAGAIN if a new thread can not be created.
   *      Returns EINVAL if there is a problem with the arguments.
   *      See man page fore pthread_create
   ***************************************************************************/
#define ithread_create pthread_create


  /****************************************************************************
   * Function: ithread_cancel
   *
   *  Description:
   *		Cancels a thread.
   *  Parameters:
   *      ithread_t * thread (must be valid non NULL pointer to ithread_t)
   *  Returns:
   *      0 on success. Nonzero on failure.
   *      See man page for pthread_cancel
   ***************************************************************************/
#define ithread_cancel pthread_cancel
  

  /****************************************************************************
   * Function: ithread_exit
   *
   *  Description:
   *		Returns a return code from a thread.
   *      Implicitly called when the start routine returns.
   *  Parameters:
   *      void  * return_code return code to return
   *      See man page for pthread_exit
   ***************************************************************************/
#define ithread_exit pthread_exit

/****************************************************************************
   * Function: ithread_get_current_thread_id
   *
   *  Description:
   *		Returns the handle of the currently running thread.
   *  Returns:
   *		The handle of the currently running thread.
   *              See man page for pthread_self
   ***************************************************************************/
#define ithread_get_current_thread_id pthread_self


  /****************************************************************************
   * Function: ithread_self
   *
   *  Description:
   *		Returns the handle of the currently running thread.
   *  Returns:
   *		The handle of the currently running thread.
   *              See man page for pthread_self
   ***************************************************************************/
#define ithread_self pthread_self

  /****************************************************************************
   * Function: ithread_detach
   *
   *  Description:
   *		Makes a thread's resources reclaimed immediately 
   *            after it finishes
   *            execution.  
   *  Returns:
   *		0 on success, Nonzero on failure.
   *      See man page for pthread_detach
   ***************************************************************************/
#define ithread_detach pthread_detach  

  /****************************************************************************
   * Function: ithread_join
   *
   *  Description:
   *		Suspends the currently running thread until the 
   * specified thread
   *      has finished. 
   *      Returns the return code of the thread, or ITHREAD_CANCELED 
   *      if the thread has been canceled.
   *  Parameters:
   *      ithread_t *thread (valid non null thread identifier)
   *      void ** return (space for return code) 
   *  Returns:
   *		0 on success, Nonzero on failure.
   *     See man page for pthread_join
   ***************************************************************************/
#define ithread_join pthread_join
  


/****************************************************************************
 * Function: isleep
 *
 *  Description:
 *		Suspends the currently running thread for the specified number 
 *      of seconds
 *      Always returns 0.
 *  Parameters:
 *      unsigned int seconds - number of seconds to sleep.
 *  Returns:
 *		0 on success, Nonzero on failure.
 *              See man page for sleep (man 3 sleep)
 *****************************************************************************/
#define isleep sleep

/****************************************************************************
 * Function: isleep
 *
 *  Description:
 *		Suspends the currently running thread for the specified number 
 *      of milliseconds
 *      Always returns 0.
 *  Parameters:
 *      unsigned int milliseconds - number of milliseconds to sleep.
 *  Returns:
 *		0 on success, Nonzero on failure.
 *              See man page for sleep (man 3 sleep)
 *****************************************************************************/
#define imillisleep(x) usleep(1000*x)



//NK: Added for satisfying the gcc compiler  
int pthread_mutexattr_setkind_np(pthread_mutexattr_t *attr, int kind);

#ifdef __cplusplus
}
#endif

#endif //ITHREADH
