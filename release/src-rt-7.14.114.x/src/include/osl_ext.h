/*
 * OS Abstraction Layer Extension - the APIs defined by the "extension" API
 * are only supported by a subset of all operating systems.
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * $Id: osl_ext.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef _osl_ext_h_
#define _osl_ext_h_


/* ---- Include Files ---------------------------------------------------- */

#if defined(TARGETOS_nucleus)
	#include <nucleus_osl_ext.h>
#elif defined(TARGETOS_symbian)
	#include <e32def.h>
	#include <symbian_osl_ext.h>
#else
	#error "Unsupported OSL extension requested"
#endif

/* Include base operating system abstraction. */
#include <osl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Constants and Types ---------------------------------------------- */

/* -----------------------------------------------------------------------
 * Generic OS types.
 */
typedef enum osl_ext_status_t
{
	OSL_EXT_SUCCESS,
	OSL_EXT_ERROR,
	OSL_EXT_TIMEOUT

} osl_ext_status_t;

#define OSL_EXT_TIME_FOREVER ((osl_ext_time_ms_t)(-1))
typedef unsigned int osl_ext_time_ms_t;


/* -----------------------------------------------------------------------
 * Timers.
 */
typedef enum
{
	/* One-shot timer. */
	OSL_EXT_TIMER_MODE_ONCE,

	/* Periodic timer. */
	OSL_EXT_TIMER_MODE_REPEAT

} osl_ext_timer_mode_t;

/* User registered callback and parameter to invoke when timer expires. */
typedef void* osl_ext_timer_arg_t;
typedef void (*osl_ext_timer_callback)(osl_ext_timer_arg_t arg);


/* -----------------------------------------------------------------------
 * Tasks.
 */

/* Task entry argument. */
typedef void* osl_ext_task_arg_t;

/* Task entry function. */
typedef void (*osl_ext_task_entry)(osl_ext_task_arg_t arg);

/* Abstract task priority levels. */
typedef enum
{
	OSL_EXT_TASK_IDLE_PRIORITY,
	OSL_EXT_TASK_LOW_PRIORITY,
	OSL_EXT_TASK_LOW_NORMAL_PRIORITY,
	OSL_EXT_TASK_NORMAL_PRIORITY,
	OSL_EXT_TASK_HIGH_NORMAL_PRIORITY,
	OSL_EXT_TASK_HIGHEST_PRIORITY,
	OSL_EXT_TASK_TIME_CRITICAL_PRIORITY,

	/* This must be last. */
	OSL_EXT_TASK_NUM_PRIORITES
} osl_ext_task_priority_t;


/* ---- Variable Externs ------------------------------------------------- */
/* ---- Function Prototypes ---------------------------------------------- */


/* --------------------------------------------------------------------------
** Semaphore
*/

/****************************************************************************
* Function:   osl_ext_sem_create
*
* Purpose:    Creates a counting semaphore object, which can subsequently be
*             used to guard multiple instances of a given resource.
*
* Parameters: name     (in)  Name to assign to the semaphore (must be unique).
*             init_cnt (in)  Initial count that the semaphore should have.
*             sem      (out) Newly created semaphore.
*
* Returns:    OSL_EXT_SUCCESS if the semaphore was created successfully, or an
*             error code if the semaphore could not be created.
*****************************************************************************
*/
osl_ext_status_t osl_ext_sem_create(char *name, int init_cnt, osl_ext_sem_t *sem);

/****************************************************************************
* Function:   osl_ext_sem_delete
*
* Purpose:    Destroys a previously created semaphore object.
*
* Parameters: sem (mod) Semaphore object to destroy.
*
* Returns:    OSL_EXT_SUCCESS if the semaphore was created successfully, or an
*             error code if the semaphore could not be created.
*****************************************************************************
*/
osl_ext_status_t osl_ext_sem_delete(osl_ext_sem_t *sem);

/****************************************************************************
* Function:   osl_ext_sem_give
*
* Purpose:    Increments the count associated with the semaphore. This will
*             cause one thread blocked on a take to wake up.
*
* Parameters: sem (mod) Semaphore object to give.
*
* Returns:    OSL_EXT_SUCCESS if the semaphore was created successfully, or an
*             error code if the semaphore could not be created.
*****************************************************************************
*/
osl_ext_status_t osl_ext_sem_give(osl_ext_sem_t *sem);

/****************************************************************************
* Function:   osl_ext_sem_take
*
* Purpose:    Decrements the count associated with the semaphore. If the count
*             is less than zero, then the calling task will become blocked until
*             another thread does a give on the semaphore. This function will only
*             block the calling thread for timeout_msec milliseconds, before
*             returning with OSL_EXT_TIMEOUT.
*
* Parameters: sem          (mod) Semaphore object to take.
*             timeout_msec (in)  Number of milliseconds to wait for the
*                                semaphore to enter a state where it can be
*                                taken.
*
* Returns:    OSL_EXT_SUCCESS if the semaphore was created successfully, or an
*             error code if the semaphore could not be created.
*****************************************************************************
*/
osl_ext_status_t osl_ext_sem_take(osl_ext_sem_t *sem, osl_ext_time_ms_t timeout_msec);


/* --------------------------------------------------------------------------
** Mutex
*/

/****************************************************************************
* Function:   osl_ext_mutex_create
*
* Purpose:    Creates a mutex object, which can subsequently be used to control
*             mutually exclusive access to a resource.
*
* Parameters: name  (in)  Name to assign to the mutex (must be unique).
*             mutex (out) Mutex object to initialize.
*
* Returns:    OSL_EXT_SUCCESS if the semaphore was created successfully, or an
*             error code if the semaphore could not be created.
*****************************************************************************
*/
osl_ext_status_t osl_ext_mutex_create(char *name, osl_ext_mutex_t *mutex);

/****************************************************************************
* Function:   osl_ext_mutex_delete
*
* Purpose:    Destroys a previously created mutex object.
*
* Parameters: mutex (mod) Mutex object to destroy.
*
* Returns:    OSL_EXT_SUCCESS if the semaphore was created successfully, or an
*             error code if the semaphore could not be created.
*****************************************************************************
*/
osl_ext_status_t osl_ext_mutex_delete(osl_ext_mutex_t *mutex);

/****************************************************************************
* Function:   osl_ext_mutex_acquire
*
* Purpose:    Acquires the indicated mutual exclusion object. If the object is
*             currently acquired by another task, then this function will wait
*             for timeout_msec milli-seconds before returning with OSL_EXT_TIMEOUT.
*
* Parameters: mutex        (mod) Mutex object to acquire.
*             timeout_msec (in)  Number of milliseconds to wait for the mutex.
*
* Returns:    OSL_EXT_SUCCESS if the semaphore was created successfully, or an
*             error code if the semaphore could not be created.
*****************************************************************************
*/
osl_ext_status_t osl_ext_mutex_acquire(osl_ext_mutex_t *mutex, osl_ext_time_ms_t timeout_msec);

/****************************************************************************
* Function:   osl_ext_mutex_release
*
* Purpose:    Releases the indicated mutual exclusion object. This makes it
*             available for another task to acquire.
*
* Parameters: mutex (mod) Mutex object to release.
*
* Returns:    OSL_EXT_SUCCESS if the semaphore was created successfully, or an
*             error code if the semaphore could not be created.
*****************************************************************************
*/
osl_ext_status_t osl_ext_mutex_release(osl_ext_mutex_t *mutex);


/* --------------------------------------------------------------------------
** Timers
*/

/****************************************************************************
* Function:   osl_ext_timer_create
*
* Purpose:    Creates a timer object.
*
* Parameters: name (in)         Name of timer.
*             timeout_msec (in) Invoke callback after this number of milliseconds.
*             mode (in)         One-shot or periodic timer.
*             func (in)         Callback function to invoke on timer expiry.
*             arg (in)          Argument to callback function.
*             timer (out)       Timer object to create.
*
* Returns:    OSL_EXT_SUCCESS if the semaphore was created successfully, or an
*             error code if the semaphore could not be created.
*****************************************************************************
*/
osl_ext_status_t
osl_ext_timer_create(char *name, osl_ext_time_ms_t timeout_msec, osl_ext_timer_mode_t mode,
                 osl_ext_timer_callback func, osl_ext_timer_arg_t arg, osl_ext_timer_t *timer);

/****************************************************************************
* Function:   osl_ext_timer_delete
*
* Purpose:    Destroys a previously created timer object.
*
* Parameters: timer (mod) Timer object to destroy.
*
* Returns:    OSL_EXT_SUCCESS if the semaphore was created successfully, or an
*             error code if the semaphore could not be created.
*****************************************************************************
*/
osl_ext_status_t osl_ext_timer_delete(osl_ext_timer_t *timer);

/****************************************************************************
* Function:   osl_ext_time_get
*
* Purpose:    Returns incrementing time counter.
*
* Parameters: None.
*
* Returns:    Returns incrementing time counter in msec.
*****************************************************************************
*/
osl_ext_time_ms_t osl_ext_time_get(void);

/* --------------------------------------------------------------------------
** Tasks
*/

/****************************************************************************
* Function:   osl_ext_task_create
*
* Purpose:    Create a task.
*
* Parameters: name       (in)  Pointer to task string descriptor.
*             stack_size (in)  Stack size to allocate - in bytes.
*             priority   (in)  Abstract task priority.
*             func       (in)  A pointer to the task entry point function.
*             arg        (in)  Value passed into task entry point function.
*             task       (out) Task to create.
*
* Returns:    OSL_EXT_SUCCESS if the semaphore was created successfully, or an
*             error code if the semaphore could not be created.
*****************************************************************************
*/

#define osl_ext_task_create(name, stack_size, priority, func, arg, task)       \
	   osl_ext_task_create_ex((name), (stack_size), (priority), 0, (func), \
	   (arg), (task))

osl_ext_status_t osl_ext_task_create_ex(char* name, unsigned int stack_size,
                             osl_ext_task_priority_t priority,
                             osl_ext_time_ms_t timslice_msec,
                             osl_ext_task_entry func, osl_ext_task_arg_t arg,
                             osl_ext_task_t *task);

/****************************************************************************
* Function:   osl_ext_task_delete
*
* Purpose:    Destroy a task.
*
* Parameters: task (mod) Task to destroy.
*
* Returns:    OSL_EXT_SUCCESS if the semaphore was created successfully, or an
*             error code if the semaphore could not be created.
*****************************************************************************
*/
osl_ext_status_t osl_ext_task_delete(osl_ext_task_t *task);

/* --------------------------------------------------------------------------
** Queue
*/

/****************************************************************************
* Function:   osl_ext_queue_create
*
* Purpose:    Create a queue.
*
* Parameters: name     (in)  Name to assign to the queue (must be unique).
*             size     (in)  Size of the queue.
*             queue    (out) Newly created queue.
*
* Returns:    OSL_EXT_SUCCESS if the queue was created successfully, or an
*             error code if the queue could not be created.
*****************************************************************************
*/
osl_ext_status_t osl_ext_queue_create(char *name, unsigned int queue_size,
                                      osl_ext_queue_t *queue);

/****************************************************************************
* Function:   osl_ext_queue_delete
*
* Purpose:    Destroys a previously created queue object.
*
* Parameters: queue    (mod) Queue object to destroy.
*
* Returns:    OSL_EXT_SUCCESS if the queue was deleted successfully, or an
*             error code if the queue could not be deleteed.
*****************************************************************************
*/
osl_ext_status_t osl_ext_queue_delete(osl_ext_queue_t *queue);

/****************************************************************************
* Function:   osl_ext_queue_bind
*
* Purpose:    Bind a previously created queue object to a task.
*
* Parameters: queue    Queue object.
*             task     Task object to bind to.
*
* Returns:    OSL_EXT_SUCCESS if the queue bound successfully, or an
*             error code if the queue could not be bound.
*****************************************************************************
*/
osl_ext_status_t osl_ext_queue_bind(osl_ext_queue_t *queue, osl_ext_task_t *task);

/****************************************************************************
* Function:   osl_ext_queue_send
*
* Purpose:    Send/add data to the queue. This function will block the
*             calling thread until the data is queued.
*
* Parameters: queue    (mod) Queue object.
*             data     (in)  Data pointer to be queued.
*
* Returns:    OSL_EXT_SUCCESS if the data was queued successfully, or an
*             error code if the data could not be queued.
*****************************************************************************
*/
osl_ext_status_t osl_ext_queue_send(osl_ext_queue_t *queue, void *data);

/****************************************************************************
* Function:   osl_ext_queue_send_synchronous
*
* Purpose:    Send/add data to the queue. This function will block the
*             calling thread until the data is dequeued.
*
* Parameters: queue    (mod) Queue object.
*             data     (in)  Data pointer to be queued.
*
* Returns:    OSL_EXT_SUCCESS if the data was queued successfully, or an
*             error code if the data could not be queued.
*****************************************************************************
*/
osl_ext_status_t osl_ext_queue_send_synchronous(osl_ext_queue_t *queue, void *data);

/****************************************************************************
* Function:   osl_ext_queue_receive
*
* Purpose:    Receive/remove data from the queue. This function will only
*             block the calling thread for timeout_msec milliseconds, before
*             returning with OSL_EXT_TIMEOUT.
*
* Parameters: queue        (mod) Queue object.
*             timeout_msec (in)  Number of milliseconds to wait for the
*                                data from the queue.
*             data         (out) Data pointer received/removed from the queue.
*
* Returns:    OSL_EXT_SUCCESS if the data was dequeued successfully, or an
*             error code if the data could not be dequeued.
*****************************************************************************
*/
osl_ext_status_t osl_ext_queue_receive(osl_ext_queue_t *queue,
                 osl_ext_time_ms_t timeout_msec, void **data);

/****************************************************************************
* Function:   osl_ext_task_yield
*
* Purpose:    Yield the CPU to other tasks of the same priority that are
*             ready-to-run.
*
* Parameters: None.
*
* Returns:    OSL_EXT_SUCCESS if successful, else error code.
*****************************************************************************
*/
osl_ext_status_t osl_ext_task_yield(void);

#ifdef __cplusplus
}
#endif

#endif	/* _osl_ext_h_ */
