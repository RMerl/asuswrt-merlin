/* $Id: os.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJ_OS_H__
#define __PJ_OS_H__

/**
 * @file os.h
 * @brief OS dependent functions
 */
#include <pj/types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_OS Operating System Dependent Functionality.
 */


/* **************************************************************************/
/**
 * @defgroup PJ_SYS_INFO System Information
 * @ingroup PJ_OS
 * @{
 */

/**
 * These enumeration contains constants to indicate support of miscellaneous
 * system features. These will go in "flags" field of #pj_sys_info structure.
 */
typedef enum pj_sys_info_flag
{
    /**
     * Support for Apple iOS background feature.
     */
    PJ_SYS_HAS_IOS_BG = 1

} pj_sys_info_flag;


/**
 * This structure contains information about the system. Use #pj_get_sys_info()
 * to obtain the system information.
 */
typedef struct pj_sys_info
{
    /**
     * Null terminated string containing processor information (e.g. "i386",
     * "x86_64"). It may contain empty string if the value cannot be obtained.
     */
    pj_str_t	machine;

    /**
     * Null terminated string identifying the system operation (e.g. "Linux",
     * "win32", "wince"). It may contain empty string if the value cannot be
     * obtained.
     */
    pj_str_t	os_name;

    /**
     * A number containing the operating system version number. By convention,
     * this field is divided into four bytes, where the highest order byte
     * contains the most major version of the OS, the next less significant
     * byte contains the less major version, and so on. How the OS version
     * number is mapped into these four bytes would be specific for each OS.
     * For example, Linux-2.6.32-28 would yield "os_ver" value of 0x0206201c,
     * while for Windows 7 it will be 0x06010000 (because dwMajorVersion is
     * 6 and dwMinorVersion is 1 for Windows 7).
     *
     * This field may contain zero if the OS version cannot be obtained.
     */
    pj_uint32_t	os_ver;

    /**
     * Null terminated string identifying the SDK name that is used to build
     * the library (e.g. "glibc", "uclibc", "msvc", "wince"). It may contain
     * empty string if the value cannot eb obtained.
     */
    pj_str_t	sdk_name;

    /**
     * A number containing the SDK version, using the numbering convention as
     * the "os_ver" field. The value will be zero if the version cannot be
     * obtained.
     */
    pj_uint32_t	sdk_ver;

    /**
     * A longer null terminated string identifying the underlying system with
     * as much information as possible.
     */
    pj_str_t	info;

    /**
     * Other flags containing system specific information. The value is
     * bitmask of #pj_sys_info_flag constants.
     */
    pj_uint32_t	flags;

} pj_sys_info;


/**
 * Obtain the system information.
 *
 * @return	System information structure.
 */
PJ_DECL(const pj_sys_info*) pj_get_sys_info(void);


/**
 * DEAN Added
 *
 * Obtain the system information.
 *
 * @return	System information structure.
 */
PJ_DECL(const pj_sys_info*) natnl_get_sys_info(void);

/*
 * @}
 */

/* **************************************************************************/
/**
 * @defgroup PJ_THREAD Threads
 * @ingroup PJ_OS
 * @{
 * This module provides multithreading API.
 *
 * \section pj_thread_examples_sec Examples
 *
 * For examples, please see:
 *  - \ref page_pjlib_thread_test
 *  - \ref page_pjlib_sleep_test
 *
 */

/**
 * Thread creation flags:
 * - PJ_THREAD_SUSPENDED: specify that the thread should be created suspended.
 */
typedef enum pj_thread_create_flags
{
    PJ_THREAD_SUSPENDED = 1
} pj_thread_create_flags;


/**
 * Type of thread entry function.
 */
typedef int (PJ_THREAD_FUNC pj_thread_proc)(void*);

/**
 * Size of thread struct.
 */
#if !defined(PJ_THREAD_DESC_SIZE)
#   define PJ_THREAD_DESC_SIZE	    (64)
#endif

/**
 * Thread structure, to thread's state when the thread is created by external
 * or native API. 
 */
typedef long pj_thread_desc[PJ_THREAD_DESC_SIZE];

/**
 * Get process ID.
 * @return process ID.
 */
PJ_DECL(pj_uint32_t) pj_getpid(void);

/**
 * Get thread ID.
 * @return thread ID.
 */
PJ_DECL(pj_uint32_t) pj_gettid(void);

/**
 * Create a new thread.
 *
 * @param pool          The memory pool from which the thread record 
 *                      will be allocated from.
 * @param thread_name   The optional name to be assigned to the thread.
 * @param proc          Thread entry function.
 * @param arg           Argument to be passed to the thread entry function.
 * @param stack_size    The size of the stack for the new thread, or ZERO or
 *                      PJ_THREAD_DEFAULT_STACK_SIZE to let the 
 *		        library choose the reasonable size for the stack. 
 *                      For some systems, the stack will be allocated from 
 *                      the pool, so the pool must have suitable capacity.
 * @param flags         Flags for thread creation, which is bitmask combination 
 *                      from enum pj_thread_create_flags.
 * @param thread        Pointer to hold the newly created thread.
 *
 * @return	        PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_thread_create(  pj_pool_t *pool, 
                                        const char *thread_name,
				        pj_thread_proc *proc, 
                                        void *arg,
				        pj_size_t stack_size, 
                                        unsigned flags,
					pj_thread_t **thread );

/**
 * Register a thread that was created by external or native API to PJLIB.
 * This function must be called in the context of the thread being registered.
 * When the thread is created by external function or API call,
 * it must be 'registered' to PJLIB using pj_thread_register(), so that it can
 * cooperate with PJLIB's framework. During registration, some data needs to
 * be maintained, and this data must remain available during the thread's 
 * lifetime.
 *
 * @param inst_id       The instance id of pjsua.
 * @param thread_name   The optional name to be assigned to the thread.
 * @param desc          Thread descriptor, which must be available throughout 
 *                      the lifetime of the thread.
 * @param thread        Pointer to hold the created thread handle.
 *
 * @return              PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_thread_register ( int inst_id,
					  const char *thread_name,
					  pj_thread_desc desc,
					  pj_thread_t **thread);

/**
 * Check if this thread has been registered to PJLIB.
 *
 * @param inst_id The instance id of pjsua.
 * @return		Non-zero if it is registered.
 */
PJ_DECL(pj_bool_t) pj_thread_is_registered(int inst_id);


/**
 * Get thread priority value for the thread.
 *
 * @param thread	Thread handle.
 *
 * @return		Thread priority value, or -1 on error.
 */
PJ_DECL(int) pj_thread_get_prio(pj_thread_t *thread);


/**
 * Set the thread priority. The priority value must be in the priority
 * value range, which can be retrieved with #pj_thread_get_prio_min() and
 * #pj_thread_get_prio_max() functions.
 *
 * @param thread	Thread handle.
 * @param prio		New priority to be set to the thread.
 *
 * @return		PJ_SUCCESS on success or the error code.
 */
PJ_DECL(pj_status_t) pj_thread_set_prio(pj_thread_t *thread,  int prio);

/**
 * Get the lowest priority value available for this thread.
 *
 * @param thread	Thread handle.
 * @return		Minimum thread priority value, or -1 on error.
 */
PJ_DECL(int) pj_thread_get_prio_min(pj_thread_t *thread);


/**
 * Get the highest priority value available for this thread.
 *
 * @param thread	Thread handle.
 * @return		Minimum thread priority value, or -1 on error.
 */
PJ_DECL(int) pj_thread_get_prio_max(pj_thread_t *thread);


/**
 * Return native handle from pj_thread_t for manipulation using native
 * OS APIs.
 *
 * @param thread	PJLIB thread descriptor.
 *
 * @return		Native thread handle. For example, when the
 *			backend thread uses pthread, this function will
 *			return pointer to pthread_t, and on Windows,
 *			this function will return HANDLE.
 */
PJ_DECL(void*) pj_thread_get_os_handle(pj_thread_t *thread);

/**
 * Return native thread id from pj_thread_t.
 *
 * @param thread	PJLIB thread descriptor.
 *
 * @return		Native thread id. For example, when the
 *			backend thread uses pthread, this function will
 *			return pointer to pthread_t, and on Windows,
 *			this function will return idthread.
 */
PJ_DECL(void*) pj_thread_get_thread_id(pj_thread_t *thread);
/**
 * Get thread name.
 *
 * @param thread    The thread handle.
 *
 * @return Thread name as null terminated string.
 */
PJ_DECL(const char*) pj_thread_get_name(pj_thread_t *thread);

/**
 * Resume a suspended thread.
 *
 * @param thread    The thread handle.
 *
 * @return zero on success.
 */
PJ_DECL(pj_status_t) pj_thread_resume(pj_thread_t *thread);

/**
 * Get the current thread.
 *
 * @param inst_id The instance id of pjsua.
 * @return Thread handle of current thread.
 */
PJ_DECL(pj_thread_t*) pj_thread_this(int inst_id);

/**
 * Join thread, and block the caller thread until the specified thread exits.
 * If the specified thread has already been dead, or it does not exist,
 * the function will return immediately with successfull status.
 *
 * @param thread    The thread handle.
 *
 * @return PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pj_thread_join(pj_thread_t *thread);


/**
 * Destroy thread and release resources allocated for the thread.
 * However, the memory allocated for the pj_thread_t itself will only be released
 * when the pool used to create the thread is destroyed.
 *
 * @param thread    The thread handle.
 *
 * @return zero on success.
 */
PJ_DECL(pj_status_t) pj_thread_destroy(pj_thread_t *thread);


/**
 * Put the current thread to sleep for the specified miliseconds.
 *
 * @param msec Miliseconds delay.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_thread_sleep(unsigned msec);

/**
 * @def PJ_CHECK_STACK()
 * PJ_CHECK_STACK() macro is used to check the sanity of the stack.
 * The OS implementation may check that no stack overflow occurs, and
 * it also may collect statistic about stack usage.
 */
#if defined(PJ_OS_HAS_CHECK_STACK) && PJ_OS_HAS_CHECK_STACK!=0

#  define PJ_CHECK_STACK() pj_thread_check_stack(__FILE__, __LINE__)

/** @internal
 * The implementation of stack checking. 
 */
PJ_DECL(void) pj_thread_check_stack(const char *file, int line);

/** @internal
 * Get maximum stack usage statistic. 
 */
PJ_DECL(pj_uint32_t) pj_thread_get_stack_max_usage(pj_thread_t *thread);

/** @internal
 * Dump thread stack status. 
 */
PJ_DECL(pj_status_t) pj_thread_get_stack_info(pj_thread_t *thread,
					      const char **file,
					      int *line);
#else

#  define PJ_CHECK_STACK()
/** pj_thread_get_stack_max_usage() for the thread */
#  define pj_thread_get_stack_max_usage(thread)	    0
/** pj_thread_get_stack_info() for the thread */
#  define pj_thread_get_stack_info(thread,f,l)	    (*(f)="",*(l)=0)
#endif	/* PJ_OS_HAS_CHECK_STACK */

/**
 * @}
 */

/* **************************************************************************/
/**
 * @defgroup PJ_SYMBIAN_OS Symbian OS Specific
 * @ingroup PJ_OS
 * @{
 * Functionalities specific to Symbian OS.
 *
 * Symbian OS strongly discourages the use of polling since this wastes
 * CPU power, and instead provides Active Object and Active Scheduler
 * pattern to allow application (in this case, PJLIB) to register asynchronous
 * tasks. PJLIB port for Symbian complies to this recommended behavior.
 * As the result, few things have been changed in PJLIB for Symbian:
 *	- the timer heap (see @ref PJ_TIMER) is implemented with active
 *	  object framework, and each timer entry registered to the timer 
 *	  heap will register an Active Object to the Active Scheduler.
 *	  Because of this, polling the timer heap with pj_timer_heap_poll()
 *	  is no longer necessary, and this function will just evaluate
 *	  to nothing.
 *	- the ioqueue (see @ref PJ_IOQUEUE) is also implemented with
 *	  active object framework, with each asynchronous operation will
 *	  register an Active Object to the Active Scheduler. Because of
 *	  this, polling the ioqueue with pj_ioqueue_poll() is no longer
 *	  necessary, and this function will just evaluate to nothing.
 *
 * Since timer heap and ioqueue polling are no longer necessary, Symbian
 * application can now poll for all events by calling 
 * \a User::WaitForAnyRequest() and \a CActiveScheduler::RunIfReady().
 * PJLIB provides a thin wrapper which calls these two functions,
 * called pj_symbianos_poll().
 */
 
/**
 * Wait the completion of any Symbian active objects. When the timeout
 * value is not specified (the \a ms_timeout argument is -1), this 
 * function is a thin wrapper which calls \a User::WaitForAnyRequest() 
 * and \a CActiveScheduler::RunIfReady(). If the timeout value is
 * specified, this function will schedule a timer entry to the timer
 * heap (which is an Active Object), to limit the wait time for event
 * occurences. Scheduling a timer entry is an expensive operation,
 * therefore application should only specify a timeout value when it's
 * really necessary (for example, when it's not sure there are other
 * Active Objects currently running in the application).
 *
 * @param priority	The minimum priority of the Active Objects to
 *			poll, which values are from CActive::TPriority
 *			constants. If -1 is given, CActive::EPriorityStandard.
 *			priority will be used.
 * @param ms_timeout	Optional timeout to wait. Application should
 *			specify -1 to let the function wait indefinitely
 *			for any events.
 *
 * @return		PJ_TRUE if there have been any events executed
 *			during the polling. This function will only return
 *			PJ_FALSE if \a ms_timeout argument is specified
 *			(i.e. the value is not -1) and there was no event
 *			executed when the timeout timer elapsed.
 */
PJ_DECL(pj_bool_t) pj_symbianos_poll(int priority, int ms_timeout);


/**
 * This structure declares Symbian OS specific parameters that can be
 * specified when calling #pj_symbianos_set_params().
 */
typedef struct pj_symbianos_params 
{
    /**
     * Optional RSocketServ instance to be used by PJLIB. If this
     * value is NULL, PJLIB will create a new RSocketServ instance
     * when pj_init() is called.
     */
    void	*rsocketserv;
    
    /**
     * Optional RConnection instance to be used by PJLIB when creating
     * sockets. If this value is NULL, no RConnection will be
     * specified when creating sockets.
     */
    void	*rconnection;
    
    /**
     * Optional RHostResolver instance to be used by PJLIB. If this value
     * is NULL, a new RHostResolver instance will be created when
     * pj_init() is called.
     */
    void 	*rhostresolver;
     
    /**
     * Optional RHostResolver for IPv6 instance to be used by PJLIB. 
     * If this value is NULL, a new RHostResolver instance will be created
     * when pj_init() is called.
     */
    void 	*rhostresolver6;
     
} pj_symbianos_params;

/**
 * Specify Symbian OS parameters to be used by PJLIB. This function MUST
 * be called before #pj_init() is called.
 *
 * @param prm		Symbian specific parameters.
 *
 * @return		PJ_SUCCESS if the parameters can be applied
 *			successfully.
 */
PJ_DECL(pj_status_t) pj_symbianos_set_params(pj_symbianos_params *prm);

/**
 *  Notify PJLIB that the access point connection has been down or unusable
 *  and PJLIB should not try to access the Symbian socket API (especially ones
 *  that send packets). Sending packet when RConnection is reconnected to 
 *  different access point may cause the WaitForRequest() for the function to 
 *  block indefinitely.
 *  
 *  @param up		If set to PJ_FALSE it will cause PJLIB to not try
 *  			to access socket API, and error will be returned
 *  			immediately instead.
 */
PJ_DECL(void) pj_symbianos_set_connection_status(pj_bool_t up);

/**
 * @}
 */
 
/* **************************************************************************/
/**
 * @defgroup PJ_TLS Thread Local Storage.
 * @ingroup PJ_OS
 * @{
 */

/** 
 * Allocate thread local storage index. The initial value of the variable at
 * the index is zero.
 *
 * @param index	    Pointer to hold the return value.
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_thread_local_alloc(long *index);

/**
 * Deallocate thread local variable.
 *
 * @param index	    The variable index.
 */
PJ_DECL(void) pj_thread_local_free(long index);

/**
 * Set the value of thread local variable.
 *
 * @param index	    The index of the variable.
 * @param value	    The value.
 */
PJ_DECL(pj_status_t) pj_thread_local_set(long index, void *value);

/**
 * Get the value of thread local variable.
 *
 * @param index	    The index of the variable.
 * @return	    The value.
 */
PJ_DECL(void*) pj_thread_local_get(long index);


/**
 * @}
 */


/* **************************************************************************/
/**
 * @defgroup PJ_ATOMIC Atomic Variables
 * @ingroup PJ_OS
 * @{
 *
 * This module provides API to manipulate atomic variables.
 *
 * \section pj_atomic_examples_sec Examples
 *
 * For some example codes, please see:
 *  - @ref page_pjlib_atomic_test
 */


/**
 * Create atomic variable.
 *
 * @param pool	    The pool.
 * @param initial   The initial value of the atomic variable.
 * @param atomic    Pointer to hold the atomic variable upon return.
 *
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_atomic_create( pj_pool_t *pool, 
				       pj_atomic_value_t initial,
				       pj_atomic_t **atomic );

/**
 * Destroy atomic variable.
 *
 * @param atomic_var	the atomic variable.
 *
 * @return PJ_SUCCESS if success.
 */
PJ_DECL(pj_status_t) pj_atomic_destroy( pj_atomic_t *atomic_var );

/**
 * Set the value of an atomic type, and return the previous value.
 *
 * @param atomic_var	the atomic variable.
 * @param value		value to be set to the variable.
 */
PJ_DECL(void) pj_atomic_set( pj_atomic_t *atomic_var, 
			     pj_atomic_value_t value);

/**
 * Get the value of an atomic type.
 *
 * @param atomic_var	the atomic variable.
 *
 * @return the value of the atomic variable.
 */
PJ_DECL(pj_atomic_value_t) pj_atomic_get(pj_atomic_t *atomic_var);

/**
 * Increment the value of an atomic type.
 *
 * @param atomic_var	the atomic variable.
 */
PJ_DECL(void) pj_atomic_inc(pj_atomic_t *atomic_var);

/**
 * Increment the value of an atomic type and get the result.
 *
 * @param atomic_var	the atomic variable.
 *
 * @return              The incremented value.
 */
PJ_DECL(pj_atomic_value_t) pj_atomic_inc_and_get(pj_atomic_t *atomic_var);

/**
 * Decrement the value of an atomic type.
 *
 * @param atomic_var	the atomic variable.
 */
PJ_DECL(void) pj_atomic_dec(pj_atomic_t *atomic_var);

/**
 * Decrement the value of an atomic type and get the result.
 *
 * @param atomic_var	the atomic variable.
 *
 * @return              The decremented value.
 */
PJ_DECL(pj_atomic_value_t) pj_atomic_dec_and_get(pj_atomic_t *atomic_var);

/**
 * Add a value to an atomic type.
 *
 * @param atomic_var	The atomic variable.
 * @param value		Value to be added.
 */
PJ_DECL(void) pj_atomic_add( pj_atomic_t *atomic_var,
			     pj_atomic_value_t value);

/**
 * Add a value to an atomic type and get the result.
 *
 * @param atomic_var	The atomic variable.
 * @param value		Value to be added.
 *
 * @return              The result after the addition.
 */
PJ_DECL(pj_atomic_value_t) pj_atomic_add_and_get( pj_atomic_t *atomic_var,
			                          pj_atomic_value_t value);

/**
 * @}
 */

/* **************************************************************************/
/**
 * @defgroup PJ_MUTEX Mutexes.
 * @ingroup PJ_OS
 * @{
 *
 * Mutex manipulation. Alternatively, application can use higher abstraction
 * for lock objects, which provides uniform API for all kinds of lock 
 * mechanisms, including mutex. See @ref PJ_LOCK for more information.
 */

/**
 * Mutex types:
 *  - PJ_MUTEX_DEFAULT: default mutex type, which is system dependent.
 *  - PJ_MUTEX_SIMPLE: non-recursive mutex.
 *  - PJ_MUTEX_RECURSE: recursive mutex.
 */
typedef enum pj_mutex_type_e
{
    PJ_MUTEX_DEFAULT,
    PJ_MUTEX_SIMPLE,
    PJ_MUTEX_RECURSE
} pj_mutex_type_e;


/**
 * Create mutex of the specified type.
 *
 * @param pool	    The pool.
 * @param name	    Name to be associated with the mutex (for debugging).
 * @param type	    The type of the mutex, of type #pj_mutex_type_e.
 * @param mutex	    Pointer to hold the returned mutex instance.
 *
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_mutex_create(pj_pool_t *pool, 
                                     const char *name,
				     int type, 
                                     pj_mutex_t **mutex);

/**
 * Create simple, non-recursive mutex.
 * This function is a simple wrapper for #pj_mutex_create to create 
 * non-recursive mutex.
 *
 * @param pool	    The pool.
 * @param name	    Mutex name.
 * @param mutex	    Pointer to hold the returned mutex instance.
 *
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_mutex_create_simple( pj_pool_t *pool, const char *name,
					     pj_mutex_t **mutex );

/**
 * Create recursive mutex.
 * This function is a simple wrapper for #pj_mutex_create to create 
 * recursive mutex.
 *
 * @param pool	    The pool.
 * @param name	    Mutex name.
 * @param mutex	    Pointer to hold the returned mutex instance.
 *
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_mutex_create_recursive( pj_pool_t *pool,
					    const char *name,
						pj_mutex_t **mutex );

/**
 * Acquire mutex lock.
 *
 * @param mutex	    The mutex.
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_mutex_lock(pj_mutex_t *mutex);

/**
 * Release mutex lock.
 *
 * @param mutex	    The mutex.
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_mutex_unlock(pj_mutex_t *mutex);

/**
 * Try to acquire mutex lock.
 *
 * @param mutex	    The mutex.
 * @return	    PJ_SUCCESS on success, or the error code if the
 *		    lock couldn't be acquired.
 */
PJ_DECL(pj_status_t) pj_mutex_trylock(pj_mutex_t *mutex);

/**
 * Destroy mutex.
 *
 * @param mutex	    Te mutex.
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_mutex_destroy(pj_mutex_t *mutex);

/**
 * Determine whether calling thread is owning the mutex (only available when
 * PJ_DEBUG is set).
 * @param mutex	    The mutex.
 * @return	    Non-zero if yes.
 */
PJ_DECL(pj_bool_t) pj_mutex_is_locked(pj_mutex_t *mutex);

/**
 * Get instance id of mutex
 *
 * @param mutex	    The mutex.
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(int) pj_get_mutex_inst_id(pj_mutex_t *mutex);

/**
 * @}
 */

/* **************************************************************************/
/**
 * @defgroup PJ_RW_MUTEX Reader/Writer Mutex
 * @ingroup PJ_OS
 * @{
 * Reader/writer mutex is a classic synchronization object where multiple
 * readers can acquire the mutex, but only a single writer can acquire the 
 * mutex.
 */

/**
 * Opaque declaration for reader/writer mutex.
 * Reader/writer mutex is a classic synchronization object where multiple
 * readers can acquire the mutex, but only a single writer can acquire the 
 * mutex.
 */
typedef struct pj_rwmutex_t pj_rwmutex_t;

/**
 * Create reader/writer mutex.
 *
 * @param pool	    Pool to allocate memory for the mutex.
 * @param name	    Name to be assigned to the mutex.
 * @param mutex	    Pointer to receive the newly created mutex.
 *
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_rwmutex_create(pj_pool_t *pool, const char *name,
				       pj_rwmutex_t **mutex);

/**
 * Lock the mutex for reading.
 *
 * @param mutex	    The mutex.
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_rwmutex_lock_read(pj_rwmutex_t *mutex);

/**
 * Lock the mutex for writing.
 *
 * @param mutex	    The mutex.
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_rwmutex_lock_write(pj_rwmutex_t *mutex);

/**
 * Release read lock.
 *
 * @param mutex	    The mutex.
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_rwmutex_unlock_read(pj_rwmutex_t *mutex);

/**
 * Release write lock.
 *
 * @param mutex	    The mutex.
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_rwmutex_unlock_write(pj_rwmutex_t *mutex);

/**
 * Destroy reader/writer mutex.
 *
 * @param mutex	    The mutex.
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_rwmutex_destroy(pj_rwmutex_t *mutex);


/**
 * @}
 */


/* **************************************************************************/
/**
 * @defgroup PJ_CRIT_SEC Critical sections.
 * @ingroup PJ_OS
 * @{
 * Critical section protection can be used to protect regions where:
 *  - mutual exclusion protection is needed.
 *  - it's rather too expensive to create a mutex.
 *  - the time spent in the region is very very brief.
 *
 * Critical section is a global object, and it prevents any threads from
 * entering any regions that are protected by critical section once a thread
 * is already in the section.
 *
 * Critial section is \a not recursive!
 *
 * Application <b>MUST NOT</b> call any functions that may cause current
 * thread to block (such as allocating memory, performing I/O, locking mutex,
 * etc.) while holding the critical section.
 */
/**
 * Enter critical section.
 */
PJ_DECL(void) pj_enter_critical_section(int inst_id);

/**
 * Leave critical section.
 */
PJ_DECL(void) pj_leave_critical_section(int inst_id);

/**
 * @}
 */

/* **************************************************************************/
#if defined(PJ_HAS_SEMAPHORE) && PJ_HAS_SEMAPHORE != 0
/**
 * @defgroup PJ_SEM Semaphores.
 * @ingroup PJ_OS
 * @{
 *
 * This module provides abstraction for semaphores, where available.
 */

/**
 * Create semaphore.
 *
 * @param pool	    The pool.
 * @param name	    Name to be assigned to the semaphore (for logging purpose)
 * @param initial   The initial count of the semaphore.
 * @param max	    The maximum count of the semaphore.
 * @param sem	    Pointer to hold the semaphore created.
 *
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_sem_create( pj_pool_t *pool, 
                                    const char *name,
				    unsigned initial, 
                                    unsigned max,
				    pj_sem_t **sem);

/**
 * Wait for semaphore.
 *
 * @param sem	The semaphore.
 *
 * @return	PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_sem_wait(pj_sem_t *sem);

/**
 * Try wait for semaphore.
 *
 * @param sem	The semaphore.
 *
 * @return	PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_sem_trywait(pj_sem_t *sem);

/**
 * Try wait for semaphore2.
 *
 * @param sem	The semaphore.
 *
 * @return	PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_sem_trywait2(pj_sem_t *sem);

/**
 * Release semaphore.
 *
 * @param sem	The semaphore.
 *
 * @return	PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_sem_post(pj_sem_t *sem);

/**
 * Destroy semaphore.
 *
 * @param sem	The semaphore.
 *
 * @return	PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_sem_destroy(pj_sem_t *sem);

/**
 * @}
 */
#endif	/* PJ_HAS_SEMAPHORE */


/* **************************************************************************/
#if defined(PJ_HAS_EVENT_OBJ) && PJ_HAS_EVENT_OBJ != 0
/**
 * @defgroup PJ_EVENT Event Object.
 * @ingroup PJ_OS
 * @{
 *
 * This module provides abstraction to event object (e.g. Win32 Event) where
 * available. Event objects can be used for synchronization among threads.
 */

/**
 * Create event object.
 *
 * @param pool		The pool.
 * @param name		The name of the event object (for logging purpose).
 * @param manual_reset	Specify whether the event is manual-reset
 * @param initial	Specify the initial state of the event object.
 * @param event		Pointer to hold the returned event object.
 *
 * @return event handle, or NULL if failed.
 */
PJ_DECL(pj_status_t) pj_event_create(pj_pool_t *pool, const char *name,
				     pj_bool_t manual_reset, pj_bool_t initial,
				     pj_event_t **event);

/**
 * Wait for event to be signaled.
 *
 * @param event	    The event object.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_event_wait(pj_event_t *event);

/**
 * Try wait for event object to be signalled.
 *
 * @param event The event object.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_event_trywait(pj_event_t *event);

/**
 * Set the event object state to signaled. For auto-reset event, this 
 * will only release the first thread that are waiting on the event. For
 * manual reset event, the state remains signaled until the event is reset.
 * If there is no thread waiting on the event, the event object state 
 * remains signaled.
 *
 * @param event	    The event object.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_event_set(pj_event_t *event);

/**
 * Set the event object to signaled state to release appropriate number of
 * waiting threads and then reset the event object to non-signaled. For
 * manual-reset event, this function will release all waiting threads. For
 * auto-reset event, this function will only release one waiting thread.
 *
 * @param event	    The event object.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_event_pulse(pj_event_t *event);

/**
 * Set the event object state to non-signaled.
 *
 * @param event	    The event object.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_event_reset(pj_event_t *event);

/**
 * Destroy the event object.
 *
 * @param event	    The event object.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_event_destroy(pj_event_t *event);

/**
 * @}
 */
#endif	/* PJ_HAS_EVENT_OBJ */

/* **************************************************************************/
/**
 * @addtogroup PJ_TIME Time Data Type and Manipulation.
 * @ingroup PJ_OS
 * @{
 * This module provides API for manipulating time.
 *
 * \section pj_time_examples_sec Examples
 *
 * For examples, please see:
 *  - \ref page_pjlib_sleep_test
 */

/**
 * Get current time of day in local representation.
 *
 * @param tv	Variable to store the result.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_gettimeofday(pj_time_val *tv);

/**
 * Get current time of day in local representation.
 *
 * @param tv	Variable to store the result.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_gettimeofday2(pj_time_val2 *tv);

/**
 * Parse time value into date/time representation.
 *
 * @param tv	The time.
 * @param pt	Variable to store the date time result.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_time_decode(const pj_time_val *tv, pj_parsed_time *pt);

/**
 * Encode date/time to time value.
 *
 * @param pt	The date/time.
 * @param tv	Variable to store time value result.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_time_encode(const pj_parsed_time *pt, pj_time_val *tv);

/**
 * Convert local time to GMT.
 *
 * @param tv	Time to convert.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_time_local_to_gmt(pj_time_val *tv);

/**
 * Convert GMT to local time.
 *
 * @param tv	Time to convert.
 *
 * @return zero if successfull.
 */
PJ_DECL(pj_status_t) pj_time_gmt_to_local(pj_time_val *tv);

/**
 * @}
 */

/* **************************************************************************/
#if defined(PJ_TERM_HAS_COLOR) && PJ_TERM_HAS_COLOR != 0

/**
 * @defgroup PJ_TERM Terminal
 * @ingroup PJ_OS
 * @{
 */

/**
 * Set current terminal color.
 *
 * @param color	    The RGB color.
 *
 * @return zero on success.
 */
PJ_DECL(pj_status_t) pj_term_set_color(pj_color_t color);

/**
 * Get current terminal foreground color.
 *
 * @return RGB color.
 */
PJ_DECL(pj_color_t) pj_term_get_color(void);

/**
 * @}
 */

#endif	/* PJ_TERM_HAS_COLOR */

/* **************************************************************************/
/**
 * @defgroup PJ_TIMESTAMP High Resolution Timestamp
 * @ingroup PJ_OS
 * @{
 *
 * PJLIB provides <b>High Resolution Timestamp</b> API to access highest 
 * resolution timestamp value provided by the platform. The API is usefull
 * to measure precise elapsed time, and can be used in applications such
 * as profiling.
 *
 * The timestamp value is represented in cycles, and can be related to
 * normal time (in seconds or sub-seconds) using various functions provided.
 *
 * \section pj_timestamp_examples_sec Examples
 *
 * For examples, please see:
 *  - \ref page_pjlib_sleep_test
 *  - \ref page_pjlib_timestamp_test
 */

/*
 * High resolution timer.
 */
#if defined(PJ_HAS_HIGH_RES_TIMER) && PJ_HAS_HIGH_RES_TIMER != 0

/**
 * Get monotonic time since some unspecified starting point.
 *
 * @param tv	Variable to store the result.
 *
 * @return PJ_SUCCESS if successful.
 */
PJ_DECL(pj_status_t) pj_gettickcount(pj_time_val *tv);

/**
 * Acquire high resolution timer value. The time value are stored
 * in cycles.
 *
 * @param ts	    High resolution timer value.
 * @return	    PJ_SUCCESS or the appropriate error code.
 *
 * @see pj_get_timestamp_freq().
 */
PJ_DECL(pj_status_t) pj_get_timestamp(pj_timestamp *ts);

/**
 * Get high resolution timer frequency, in cycles per second.
 *
 * @param freq	    Timer frequency, in cycles per second.
 * @return	    PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_get_timestamp_freq(pj_timestamp *freq);

/**
 * Set timestamp from 32bit values.
 * @param t	    The timestamp to be set.
 * @param hi	    The high 32bit part.
 * @param lo	    The low 32bit part.
 */
PJ_INLINE(void) pj_set_timestamp32(pj_timestamp *t, pj_uint32_t hi,
				   pj_uint32_t lo)
{
    t->u32.hi = hi;
    t->u32.lo = lo;
}


/**
 * Compare timestamp t1 and t2.
 * @param t1	    t1.
 * @param t2	    t2.
 * @return	    -1 if (t1 < t2), 1 if (t1 > t2), or 0 if (t1 == t2)
 */
PJ_INLINE(int) pj_cmp_timestamp(const pj_timestamp *t1, const pj_timestamp *t2)
{
#if PJ_HAS_INT64
    if (t1->u64 < t2->u64)
	return -1;
    else if (t1->u64 > t2->u64)
	return 1;
    else
	return 0;
#else
    if (t1->u32.hi < t2->u32.hi ||
	(t1->u32.hi == t2->u32.hi && t1->u32.lo < t2->u32.lo))
	return -1;
    else if (t1->u32.hi > t2->u32.hi ||
	     (t1->u32.hi == t2->u32.hi && t1->u32.lo > t2->u32.lo))
	return 1;
    else
	return 0;
#endif
}


/**
 * Add timestamp t2 to t1.
 * @param t1	    t1.
 * @param t2	    t2.
 */
PJ_INLINE(void) pj_add_timestamp(pj_timestamp *t1, const pj_timestamp *t2)
{
#if PJ_HAS_INT64
    t1->u64 += t2->u64;
#else
    pj_uint32_t old = t1->u32.lo;
    t1->u32.hi += t2->u32.hi;
    t1->u32.lo += t2->u32.lo;
    if (t1->u32.lo < old)
	++t1->u32.hi;
#endif
}

/**
 * Add timestamp t2 to t1.
 * @param t1	    t1.
 * @param t2	    t2.
 */
PJ_INLINE(void) pj_add_timestamp32(pj_timestamp *t1, pj_uint32_t t2)
{
#if PJ_HAS_INT64
    t1->u64 += t2;
#else
    pj_uint32_t old = t1->u32.lo;
    t1->u32.lo += t2;
    if (t1->u32.lo < old)
	++t1->u32.hi;
#endif
}

/**
 * Substract timestamp t2 from t1.
 * @param t1	    t1.
 * @param t2	    t2.
 */
PJ_INLINE(void) pj_sub_timestamp(pj_timestamp *t1, const pj_timestamp *t2)
{
#if PJ_HAS_INT64
    t1->u64 -= t2->u64;
#else
    t1->u32.hi -= t2->u32.hi;
    if (t1->u32.lo >= t2->u32.lo)
	t1->u32.lo -= t2->u32.lo;
    else {
	t1->u32.lo -= t2->u32.lo;
	--t1->u32.hi;
    }
#endif
}

/**
 * Substract timestamp t2 from t1.
 * @param t1	    t1.
 * @param t2	    t2.
 */
PJ_INLINE(void) pj_sub_timestamp32(pj_timestamp *t1, pj_uint32_t t2)
{
#if PJ_HAS_INT64
    t1->u64 -= t2;
#else
    if (t1->u32.lo >= t2)
	t1->u32.lo -= t2;
    else {
	t1->u32.lo -= t2;
	--t1->u32.hi;
    }
#endif
}

/**
 * Get the timestamp difference between t2 and t1 (that is t2 minus t1),
 * and return a 32bit signed integer difference.
 */
PJ_INLINE(pj_int32_t) pj_timestamp_diff32(const pj_timestamp *t1,
					  const pj_timestamp *t2)
{
    /* Be careful with the signess (I think!) */
#if PJ_HAS_INT64
    pj_int64_t diff = t2->u64 - t1->u64;
    return (pj_int32_t) diff;
#else
    pj_int32 diff = t2->u32.lo - t1->u32.lo;
    return diff;
#endif
}


/**
 * Calculate the elapsed time, and store it in pj_time_val.
 * This function calculates the elapsed time using highest precision
 * calculation that is available for current platform, considering
 * whether floating point or 64-bit precision arithmetic is available. 
 * For maximum portability, application should prefer to use this function
 * rather than calculating the elapsed time by itself.
 *
 * @param start     The starting timestamp.
 * @param stop      The end timestamp.
 *
 * @return	    Elapsed time as #pj_time_val.
 *
 * @see pj_elapsed_usec(), pj_elapsed_cycle(), pj_elapsed_nanosec()
 */
PJ_DECL(pj_time_val) pj_elapsed_time( const pj_timestamp *start,
                                      const pj_timestamp *stop );

/**
 * Calculate the elapsed time as 32-bit miliseconds.
 * This function calculates the elapsed time using highest precision
 * calculation that is available for current platform, considering
 * whether floating point or 64-bit precision arithmetic is available. 
 * For maximum portability, application should prefer to use this function
 * rather than calculating the elapsed time by itself.
 *
 * @param start     The starting timestamp.
 * @param stop      The end timestamp.
 *
 * @return	    Elapsed time in milisecond.
 *
 * @see pj_elapsed_time(), pj_elapsed_cycle(), pj_elapsed_nanosec()
 */
PJ_DECL(pj_uint32_t) pj_elapsed_msec( const pj_timestamp *start,
                                      const pj_timestamp *stop );

/**
 * Variant of #pj_elapsed_msec() which returns 64bit value.
 */
PJ_DECL(pj_uint64_t) pj_elapsed_msec64(const pj_timestamp *start,
                                       const pj_timestamp *stop );

/**
 * Calculate the elapsed time in 32-bit microseconds.
 * This function calculates the elapsed time using highest precision
 * calculation that is available for current platform, considering
 * whether floating point or 64-bit precision arithmetic is available. 
 * For maximum portability, application should prefer to use this function
 * rather than calculating the elapsed time by itself.
 *
 * @param start     The starting timestamp.
 * @param stop      The end timestamp.
 *
 * @return	    Elapsed time in microsecond.
 *
 * @see pj_elapsed_time(), pj_elapsed_cycle(), pj_elapsed_nanosec()
 */
PJ_DECL(pj_uint32_t) pj_elapsed_usec( const pj_timestamp *start,
                                      const pj_timestamp *stop );

/**
 * Calculate the elapsed time in 32-bit nanoseconds.
 * This function calculates the elapsed time using highest precision
 * calculation that is available for current platform, considering
 * whether floating point or 64-bit precision arithmetic is available. 
 * For maximum portability, application should prefer to use this function
 * rather than calculating the elapsed time by itself.
 *
 * @param start     The starting timestamp.
 * @param stop      The end timestamp.
 *
 * @return	    Elapsed time in nanoseconds.
 *
 * @see pj_elapsed_time(), pj_elapsed_cycle(), pj_elapsed_usec()
 */
PJ_DECL(pj_uint32_t) pj_elapsed_nanosec( const pj_timestamp *start,
                                         const pj_timestamp *stop );

/**
 * Calculate the elapsed time in 32-bit cycles.
 * This function calculates the elapsed time using highest precision
 * calculation that is available for current platform, considering
 * whether floating point or 64-bit precision arithmetic is available. 
 * For maximum portability, application should prefer to use this function
 * rather than calculating the elapsed time by itself.
 *
 * @param start     The starting timestamp.
 * @param stop      The end timestamp.
 *
 * @return	    Elapsed time in cycles.
 *
 * @see pj_elapsed_usec(), pj_elapsed_time(), pj_elapsed_nanosec()
 */
PJ_DECL(pj_uint32_t) pj_elapsed_cycle( const pj_timestamp *start,
                                       const pj_timestamp *stop );


#endif	/* PJ_HAS_HIGH_RES_TIMER */

/** @} */


/* **************************************************************************/
/**
 * Internal PJLIB function to initialize the threading subsystem.
 * @return          PJ_SUCCESS or the appropriate error code.
 */
pj_status_t pj_thread_init(int inst_id);


PJ_END_DECL

#endif  /* __PJ_OS_H__ */

