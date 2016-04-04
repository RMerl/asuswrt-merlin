/* $Id: ioqueue.h 3553 2011-05-05 06:14:19Z nanang $
 */
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
#ifndef __PJ_IOQUEUE_H__
#define __PJ_IOQUEUE_H__

/**
 * @file ioqueue.h
 * @brief I/O Dispatching Mechanism
 */

#include <pj/types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_IO Input/Output
 * @brief Input/Output
 * @ingroup PJ_OS
 *
 * This section contains API building blocks to perform network I/O and 
 * communications. If provides:
 *  - @ref PJ_SOCK
 *\n
 *    A highly portable socket abstraction, runs on all kind of
 *    network APIs such as standard BSD socket, Windows socket, Linux
 *    \b kernel socket, PalmOS networking API, etc.
 *
 *  - @ref pj_addr_resolve
 *\n
 *    Portable address resolution, which implements #pj_gethostbyname().
 *
 *  - @ref PJ_SOCK_SELECT
 *\n
 *    A portable \a select() like API (#pj_sock_select()) which can be
 *    implemented with various back-ends.
 *
 *  - @ref PJ_IOQUEUE
 *\n
 *    Framework for dispatching network events.
 *
 * For more information see the modules below.
 */

/**
 * @defgroup PJ_IOQUEUE IOQueue: I/O Event Dispatching with Proactor Pattern
 * @ingroup PJ_IO
 * @{
 *
 * I/O Queue provides API for performing asynchronous I/O operations. It
 * conforms to proactor pattern, which allows application to submit an
 * asynchronous operation and to be notified later when the operation has
 * completed.
 *
 * The I/O Queue can work on both socket and file descriptors. For 
 * asynchronous file operations however, one must make sure that the correct
 * file I/O back-end is used, because not all file I/O back-end can be
 * used with the ioqueue. Please see \ref PJ_FILE_IO for more details.
 *
 * The framework works natively in platforms where asynchronous operation API
 * exists, such as in Windows NT with IoCompletionPort/IOCP. In other 
 * platforms, the I/O queue abstracts the operating system's event poll API
 * to provide semantics similar to IoCompletionPort with minimal penalties
 * (i.e. per ioqueue and per handle mutex protection).
 *
 * The I/O queue provides more than just unified abstraction. It also:
 *  - makes sure that the operation uses the most effective way to utilize
 *    the underlying mechanism, to achieve the maximum theoritical
 *    throughput possible on a given platform.
 *  - choose the most efficient mechanism for event polling on a given
 *    platform.
 *
 * Currently, the I/O Queue is implemented using:
 *  - <tt><b>select()</b></tt>, as the common denominator, but the least 
 *    efficient. Also the number of descriptor is limited to 
 *    \c PJ_IOQUEUE_MAX_HANDLES (which by default is 64).
 *  - <tt><b>/dev/epoll</b></tt> on Linux (user mode and kernel mode), 
 *    a much faster replacement for select() on Linux (and more importantly
 *    doesn't have limitation on number of descriptors).
 *  - <b>I/O Completion ports</b> on Windows NT/2000/XP, which is the most 
 *    efficient way to dispatch events in Windows NT based OSes, and most 
 *    importantly, it doesn't have the limit on how many handles to monitor.
 *    And it works with files (not only sockets) as well.
 *
 *
 * \section pj_ioqueue_concurrency_sec Concurrency Rules
 *
 * The ioqueue has been fine tuned to allow multiple threads to poll the
 * handles simultaneously, to maximize scalability when the application is
 * running on multiprocessor systems. When more than one threads are polling
 * the ioqueue and there are more than one handles are signaled, more than
 * one threads will execute the callback simultaneously to serve the events.
 * These parallel executions are completely safe when the events happen for
 * two different handles.
 *
 * However, with multithreading, care must be taken when multiple events 
 * happen on the same handle, or when event is happening on a handle (and 
 * the callback is being executed) and application is performing 
 * unregistration to the handle at the same time.
 *
 * The treatments of above scenario differ according to the concurrency
 * setting that are applied to the handle.
 *
 * \subsection pj_ioq_concur_set Concurrency Settings for Handles
 *
 * Concurrency can be set on per handle (key) basis, by using
 * #pj_ioqueue_set_concurrency() function. The default key concurrency value 
 * for the handle is inherited from the key concurrency setting of the ioqueue, 
 * and the key concurrency setting for the ioqueue can be changed by using
 * #pj_ioqueue_set_default_concurrency(). The default key concurrency setting 
 * for ioqueue itself is controlled by compile time setting
 * PJ_IOQUEUE_DEFAULT_ALLOW_CONCURRENCY.
 *
 * Note that this key concurrency setting only controls whether multiple
 * threads are allowed to operate <b>on the same key</b> at the same time. 
 * The ioqueue itself always allows multiple threads to enter the ioqeuue at 
 * the same time, and also simultaneous callback calls to <b>differrent 
 * keys</b> is always allowed regardless to the key concurrency setting.
 *
 * \subsection pj_ioq_parallel Parallel Callback Executions for the Same Handle
 *
 * Note that when key concurrency is enabled (i.e. parallel callback calls on
 * the same key is allowed; this is the default setting), the ioqueue will only
 * perform simultaneous callback executions on the same key when the key has
 * invoked multiple pending operations. This could be done for example by
 * calling #pj_ioqueue_recvfrom() more than once on the same key, each with
 * the same key but different operation key (pj_ioqueue_op_key_t). With this
 * scenario, when multiple packets arrive on the key at the same time, more
 * than one threads may execute the callback simultaneously, each with the
 * same key but different operation key.
 *
 * When there is only one pending operation on the key (e.g. there is only one
 * #pj_ioqueue_recvfrom() invoked on the key), then events occuring to the
 * same key will be queued by the ioqueue, thus no simultaneous callback calls
 * will be performed.
 *
 * \subsection pj_ioq_allow_concur Concurrency is Enabled (Default Value)
 *
 * The default setting for the ioqueue is to allow multiple threads to
 * execute callbacks for the same handle/key. This setting is selected to
 * promote good performance and scalability for application.
 *
 * However this setting has a major drawback with regard to synchronization,
 * and application MUST carefully follow the following guidelines to ensure 
 * that parallel access to the key does not cause problems:
 *
 *  - Always note that callback may be called simultaneously for the same
 *    key.
 *  - <b>Care must be taken when unregistering a key</b> from the
 *    ioqueue. Application must take care that when one thread is issuing
 *    an unregistration, other thread is not simultaneously invoking the
 *    callback <b>to the same key</b>.
 *\n
 *    This happens because the ioqueue functions are working with a pointer
 *    to the key, and there is a possible race condition where the pointer
 *    has been rendered invalid by other threads before the ioqueue has a
 *    chance to acquire mutex on it.
 *
 * \subsection pj_ioq_disallow_concur Concurrency is Disabled
 *
 * Alternatively, application may disable key concurrency to make 
 * synchronization easier. As noted above, there are three ways to control
 * key concurrency setting:
 *  - by controlling on per handle/key basis, with #pj_ioqueue_set_concurrency().
 *  - by changing default key concurrency setting on the ioqueue, with
 *    #pj_ioqueue_set_default_concurrency().
 *  - by changing the default concurrency on compile time, by declaring
 *    PJ_IOQUEUE_DEFAULT_ALLOW_CONCURRENCY macro to zero in your config_site.h
 *
 * \section pj_ioqeuue_examples_sec Examples
 *
 * For some examples on how to use the I/O Queue, please see:
 *
 *  - \ref page_pjlib_ioqueue_tcp_test
 *  - \ref page_pjlib_ioqueue_udp_test
 *  - \ref page_pjlib_ioqueue_perf_test
 */


/**
 * This structure describes operation specific key to be submitted to
 * I/O Queue when performing the asynchronous operation. This key will
 * be returned to the application when completion callback is called.
 *
 * Application normally wants to attach it's specific data in the
 * \c user_data field so that it can keep track of which operation has
 * completed when the callback is called. Alternatively, application can
 * also extend this struct to include its data, because the pointer that
 * is returned in the completion callback will be exactly the same as
 * the pointer supplied when the asynchronous function is called.
 */
typedef struct pj_ioqueue_op_key_t
{ 
    void *internal__[32];           /**< Internal I/O Queue data.   */
    void *activesock_data;	    /**< Active socket data.	    */
    void *user_data;                /**< Application data.          */
} pj_ioqueue_op_key_t;

/**
 * This structure describes the callbacks to be called when I/O operation
 * completes.
 */
typedef struct pj_ioqueue_callback
{
    /**
     * This callback is called when #pj_ioqueue_recv or #pj_ioqueue_recvfrom
     * completes.
     *
     * @param key	    The key.
     * @param op_key        Operation key.
     * @param bytes_read    >= 0 to indicate the amount of data read, 
     *                      otherwise negative value containing the error
     *                      code. To obtain the pj_status_t error code, use
     *                      (pj_status_t code = -bytes_read).
     */
    void (*on_read_complete)(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read);

    /**
     * This callback is called when #pj_ioqueue_send or #pj_ioqueue_sendto
     * completes.
     *
     * @param key	    The key.
     * @param op_key        Operation key.
     * @param bytes_sent    >= 0 to indicate the amount of data written, 
     *                      otherwise negative value containing the error
     *                      code. To obtain the pj_status_t error code, use
     *                      (pj_status_t code = -bytes_sent).
     */
    void (*on_write_complete)(pj_ioqueue_key_t *key, 
                              pj_ioqueue_op_key_t *op_key, 
                              pj_ssize_t bytes_sent);

    /**
     * This callback is called when #pj_ioqueue_accept completes.
     *
     * @param key	    The key.
     * @param op_key        Operation key.
     * @param sock          Newly connected socket.
     * @param status	    Zero if the operation completes successfully.
     */
    void (*on_accept_complete)(pj_ioqueue_key_t *key, 
                               pj_ioqueue_op_key_t *op_key, 
                               pj_sock_t sock, 
                               pj_status_t status);

    /**
     * This callback is called when #pj_ioqueue_connect completes.
     *
     * @param key	    The key.
     * @param status	    PJ_SUCCESS if the operation completes successfully.
     */
    void (*on_connect_complete)(pj_ioqueue_key_t *key, 
                                pj_status_t status);
} pj_ioqueue_callback;


/**
 * Types of pending I/O Queue operation. This enumeration is only used
 * internally within the ioqueue.
 */
typedef enum pj_ioqueue_operation_e
{
    PJ_IOQUEUE_OP_NONE		= 0,	/**< No operation.          */
    PJ_IOQUEUE_OP_READ		= 1,	/**< read() operation.      */
    PJ_IOQUEUE_OP_RECV          = 2,    /**< recv() operation.      */
    PJ_IOQUEUE_OP_RECV_FROM	= 4,	/**< recvfrom() operation.  */
    PJ_IOQUEUE_OP_WRITE		= 8,	/**< write() operation.     */
    PJ_IOQUEUE_OP_SEND          = 16,   /**< send() operation.      */
    PJ_IOQUEUE_OP_SEND_TO	= 32,	/**< sendto() operation.    */
#if defined(PJ_HAS_TCP) && PJ_HAS_TCP != 0
    PJ_IOQUEUE_OP_ACCEPT	= 64,	/**< accept() operation.    */
    PJ_IOQUEUE_OP_CONNECT	= 128	/**< connect() operation.   */
#endif	/* PJ_HAS_TCP */
} pj_ioqueue_operation_e;


/**
 * This macro specifies the maximum number of events that can be
 * processed by the ioqueue on a single poll cycle, on implementation
 * that supports it. The value is only meaningfull when specified
 * during PJLIB build.
 */
#ifndef PJ_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL
#   define PJ_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL     (16)
#endif

/**
 * When this flag is specified in ioqueue's recv() or send() operations,
 * the ioqueue will always mark the operation as asynchronous.
 */
#define PJ_IOQUEUE_ALWAYS_ASYNC	    ((pj_uint32_t)1 << (pj_uint32_t)31)

/**
 * when this flags is specified in ioqueue's send() operations, 
 * the ioqueue will promise to send out the data  -- Andrew Hung
 */
#define PJ_IOQUEUE_URGENT_DATA      ((pj_uint32_t)1 << (pj_uint32_t)30)

/**
 * Return the name of the ioqueue implementation.
 *
 * @return		Implementation name.
 */
PJ_DECL(const char*) pj_ioqueue_name(void);


/**
 * Create a new I/O Queue framework.
 *
 * @param pool		The pool to allocate the I/O queue structure. 
 * @param max_fd	The maximum number of handles to be supported, which 
 *			should not exceed PJ_IOQUEUE_MAX_HANDLES.
 * @param ioqueue	Pointer to hold the newly created I/O Queue.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pj_ioqueue_create( pj_pool_t *pool, 
					pj_size_t max_fd,
					pj_ioqueue_t **ioqueue);

/**
 * Destroy the I/O queue.
 *
 * @param ioque	        The I/O Queue to be destroyed.
 *
 * @return              PJ_SUCCESS if success.
 */
PJ_DECL(pj_status_t) pj_ioqueue_destroy( pj_ioqueue_t *ioque );

/**
 * Set the lock object to be used by the I/O Queue. This function can only
 * be called right after the I/O queue is created, before any handle is
 * registered to the I/O queue.
 *
 * Initially the I/O queue is created with non-recursive mutex protection. 
 * Applications can supply alternative lock to be used by calling this 
 * function.
 *
 * @param ioque         The ioqueue instance.
 * @param lock          The lock to be used by the ioqueue.
 * @param auto_delete   In non-zero, the lock will be deleted by the ioqueue.
 *
 * @return              PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ioqueue_set_lock( pj_ioqueue_t *ioque, 
					  pj_lock_t *lock,
					  pj_bool_t auto_delete );

/**
 * Set default concurrency policy for this ioqueue. If this function is not
 * called, the default concurrency policy for the ioqueue is controlled by 
 * compile time setting PJ_IOQUEUE_DEFAULT_ALLOW_CONCURRENCY.
 *
 * Note that changing the concurrency setting to the ioqueue will only affect
 * subsequent key registrations. To modify the concurrency setting for
 * individual key, use #pj_ioqueue_set_concurrency().
 *
 * @param ioqueue	The ioqueue instance.
 * @param allow		Non-zero to allow concurrent callback calls, or
 *			PJ_FALSE to disallow it.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ioqueue_set_default_concurrency(pj_ioqueue_t *ioqueue,
							pj_bool_t allow);

/**
 * Register a socket to the I/O queue framework. 
 * When a socket is registered to the IOQueue, it may be modified to use
 * non-blocking IO. If it is modified, there is no guarantee that this 
 * modification will be restored after the socket is unregistered.
 *
 * @param pool	    To allocate the resource for the specified handle, 
 *		    which must be valid until the handle/key is unregistered 
 *		    from I/O Queue.
 * @param ioque	    The I/O Queue.
 * @param sock	    The socket.
 * @param user_data User data to be associated with the key, which can be
 *		    retrieved later.
 * @param cb	    Callback to be called when I/O operation completes. 
 * @param key       Pointer to receive the key to be associated with this
 *                  socket. Subsequent I/O queue operation will need this
 *                  key.
 *
 * @return	    PJ_SUCCESS on success, or the error code.
 */
PJ_DECL(pj_status_t) pj_ioqueue_register_sock( pj_pool_t *pool,
					       pj_ioqueue_t *ioque,
					       pj_sock_t sock,
					       void *user_data,
					       const pj_ioqueue_callback *cb,
                                               pj_ioqueue_key_t **key );

/**
 * Variant of pj_ioqueue_register_sock() with additional group lock parameter.
 * If group lock is set for the key, the key will add the reference counter
 * when the socket is registered and decrease it when it is destroyed.
 */
PJ_DECL(pj_status_t) pj_ioqueue_register_sock2(pj_pool_t *pool,
					       pj_ioqueue_t *ioque,
					       pj_sock_t sock,
					       pj_grp_lock_t *grp_lock,
					       void *user_data,
					       const pj_ioqueue_callback *cb,
                                               pj_ioqueue_key_t **key );

/**
 * Unregister from the I/O Queue framework. Caller must make sure that
 * the key doesn't have any pending operations before calling this function,
 * by calling #pj_ioqueue_is_pending() for all previously submitted
 * operations except asynchronous connect, and if necessary call
 * #pj_ioqueue_post_completion() to cancel the pending operations.
 *
 * Note that asynchronous connect operation will automatically be 
 * cancelled during the unregistration.
 *
 * Also note that when I/O Completion Port backend is used, application
 * MUST close the handle immediately after unregistering the key. This is
 * because there is no unregistering API for IOCP. The only way to
 * unregister the handle from IOCP is to close the handle.
 *
 * @param key	    The key that was previously obtained from registration.
 *
 * @return          PJ_SUCCESS on success or the error code.
 *
 * @see pj_ioqueue_is_pending
 */
PJ_DECL(pj_status_t) pj_ioqueue_unregister( pj_ioqueue_key_t *key );


/**
 * Get user data associated with an ioqueue key.
 *
 * @param key	    The key that was previously obtained from registration.
 *
 * @return          The user data associated with the descriptor, or NULL 
 *                  on error or if no data is associated with the key during
 *                  registration.
 */
PJ_DECL(void*) pj_ioqueue_get_user_data( pj_ioqueue_key_t *key );

/**
 * Set or change the user data to be associated with the file descriptor or
 * handle or socket descriptor.
 *
 * @param key	    The key that was previously obtained from registration.
 * @param user_data User data to be associated with the descriptor.
 * @param old_data  Optional parameter to retrieve the old user data.
 *
 * @return          PJ_SUCCESS on success or the error code.
 */
PJ_DECL(pj_status_t) pj_ioqueue_set_user_data( pj_ioqueue_key_t *key,
                                               void *user_data,
                                               void **old_data);

/**
 * Configure whether the ioqueue is allowed to call the key's callback
 * concurrently/in parallel. The default concurrency setting for the key
 * is controlled by ioqueue's default concurrency value, which can be
 * changed by calling #pj_ioqueue_set_default_concurrency().
 *
 * If concurrency is allowed for the key, it means that if there are more
 * than one pending operations complete simultaneously, more than one
 * threads may call the key's  callback at the same time. This generally
 * would promote good scalability for application, at the expense of more
 * complexity to manage the concurrent accesses in application's code.
 *
 * Alternatively application may disable the concurrent access by
 * setting the \a allow flag to false. With concurrency disabled, only
 * one thread can call the key's callback at one time.
 *
 * @param key	    The key that was previously obtained from registration.
 * @param allow	    Set this to non-zero to allow concurrent callback calls
 *		    and zero (PJ_FALSE) to disallow it.
 *
 * @return	    PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ioqueue_set_concurrency(pj_ioqueue_key_t *key,
						pj_bool_t allow);

/**
 * Acquire the key's mutex. When the key's concurrency is disabled, 
 * application may call this function to synchronize its operation
 * with the key's callback (i.e. this function will block until the
 * key's callback returns).
 *
 * @param key	    The key that was previously obtained from registration.
 *
 * @return	    PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ioqueue_lock_key(pj_ioqueue_key_t *key);

/**
 * Release the lock previously acquired with pj_ioqueue_lock_key().
 *
 * @param key	    The key that was previously obtained from registration.
 *
 * @return	    PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ioqueue_unlock_key(pj_ioqueue_key_t *key);

/**
 * Initialize operation key.
 *
 * @param op_key    The operation key to be initialied.
 * @param size	    The size of the operation key.
 */
PJ_DECL(void) pj_ioqueue_op_key_init( pj_ioqueue_op_key_t *op_key,
				      pj_size_t size );

/**
 * Check if operation is pending on the specified operation key.
 * The \c op_key must have been initialized with #pj_ioqueue_op_key_init() 
 * or submitted as pending operation before, or otherwise the result 
 * is undefined.
 *
 * @param key       The key.
 * @param op_key    The operation key, previously submitted to any of
 *                  the I/O functions and has returned PJ_EPENDING.
 *
 * @return          Non-zero if operation is still pending.
 */
PJ_DECL(pj_bool_t) pj_ioqueue_is_pending( pj_ioqueue_key_t *key,
                                          pj_ioqueue_op_key_t *op_key );


/**
 * Post completion status to the specified operation key and call the
 * appropriate callback. When the callback is called, the number of bytes 
 * received in read/write callback or the status in accept/connect callback
 * will be set from the \c bytes_status parameter.
 *
 * @param key           The key.
 * @param op_key        Pending operation key.
 * @param bytes_status  Number of bytes or status to be set. A good value
 *                      to put here is -PJ_ECANCELLED.
 *
 * @return              PJ_SUCCESS if completion status has been successfully
 *                      sent.
 */
PJ_DECL(pj_status_t) pj_ioqueue_post_completion( pj_ioqueue_key_t *key,
                                                 pj_ioqueue_op_key_t *op_key,
                                                 pj_ssize_t bytes_status );



#if defined(PJ_HAS_TCP) && PJ_HAS_TCP != 0
/**
 * Instruct I/O Queue to accept incoming connection on the specified 
 * listening socket. This function will return immediately (i.e. non-blocking)
 * regardless whether a connection is immediately available. If the function
 * can't complete immediately, the caller will be notified about the incoming
 * connection when it calls pj_ioqueue_poll(). If a new connection is
 * immediately available, the function returns PJ_SUCCESS with the new
 * connection; in this case, the callback WILL NOT be called.
 *
 * @param key	    The key which registered to the server socket.
 * @param op_key    An operation specific key to be associated with the
 *                  pending operation, so that application can keep track of
 *                  which operation has been completed when the callback is
 *                  called.
 * @param new_sock  Argument which contain pointer to receive the new socket
 *                  for the incoming connection.
 * @param local	    Optional argument which contain pointer to variable to 
 *                  receive local address.
 * @param remote    Optional argument which contain pointer to variable to 
 *                  receive the remote address.
 * @param addrlen   On input, contains the length of the buffer for the
 *		    address, and on output, contains the actual length of the
 *		    address. This argument is optional.
 * @return
 *  - PJ_SUCCESS    When connection is available immediately, and the 
 *                  parameters will be updated to contain information about 
 *                  the new connection. In this case, a completion callback
 *                  WILL NOT be called.
 *  - PJ_EPENDING   If no connection is available immediately. When a new
 *                  connection arrives, the callback will be called.
 *  - non-zero      which indicates the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ioqueue_accept( pj_ioqueue_key_t *key,
                                        pj_ioqueue_op_key_t *op_key,
					pj_sock_t *new_sock,
					pj_sockaddr_t *local,
					pj_sockaddr_t *remote,
					int *addrlen );

/**
 * Initiate non-blocking socket connect. If the socket can NOT be connected
 * immediately, asynchronous connect() will be scheduled and caller will be
 * notified via completion callback when it calls pj_ioqueue_poll(). If
 * socket is connected immediately, the function returns PJ_SUCCESS and
 * completion callback WILL NOT be called.
 *
 * @param key	    The key associated with TCP socket
 * @param addr	    The remote address.
 * @param addrlen   The remote address length.
 *
 * @return
 *  - PJ_SUCCESS    If socket is connected immediately. In this case, the
 *                  completion callback WILL NOT be called.
 *  - PJ_EPENDING   If operation is queued, or 
 *  - non-zero      Indicates the error code.
 */
PJ_DECL(pj_status_t) pj_ioqueue_connect( pj_ioqueue_key_t *key,
					 const pj_sockaddr_t *addr,
					 int addrlen );

#endif	/* PJ_HAS_TCP */

/**
 * Poll the I/O Queue for completed events.
 *
 * Note: polling the ioqueue is not necessary in Symbian. Please see
 * @ref PJ_SYMBIAN_OS for more info.
 *
 * @param ioque		the I/O Queue.
 * @param timeout	polling timeout, or NULL if the thread wishes to wait
 *			indefinetely for the event.
 *
 * @return 
 *  - zero if timed out (no event).
 *  - (<0) if error occured during polling. Callback will NOT be called.
 *  - (>1) to indicate numbers of events. Callbacks have been called.
 */
PJ_DECL(int) pj_ioqueue_poll( pj_ioqueue_t *ioque,
			      const pj_time_val *timeout);


/**
 * Instruct the I/O Queue to read from the specified handle. This function
 * returns immediately (i.e. non-blocking) regardless whether some data has 
 * been transferred. If the operation can't complete immediately, caller will 
 * be notified about the completion when it calls pj_ioqueue_poll(). If data
 * is immediately available, the function will return PJ_SUCCESS and the
 * callback WILL NOT be called.
 *
 * @param key	    The key that uniquely identifies the handle.
 * @param op_key    An operation specific key to be associated with the
 *                  pending operation, so that application can keep track of
 *                  which operation has been completed when the callback is
 *                  called. Caller must make sure that this key remains 
 *                  valid until the function completes.
 * @param buffer    The buffer to hold the read data. The caller MUST make sure
 *		    that this buffer remain valid until the framework completes
 *		    reading the handle.
 * @param length    On input, it specifies the size of the buffer. If data is
 *                  available to be read immediately, the function returns
 *                  PJ_SUCCESS and this argument will be filled with the
 *                  amount of data read. If the function is pending, caller
 *                  will be notified about the amount of data read in the
 *                  callback. This parameter can point to local variable in
 *                  caller's stack and doesn't have to remain valid for the
 *                  duration of pending operation.
 * @param flags     Recv flag. If flags has PJ_IOQUEUE_ALWAYS_ASYNC then
 *		    the function will never return PJ_SUCCESS.
 *
 * @return
 *  - PJ_SUCCESS    If immediate data has been received in the buffer. In this
 *                  case, the callback WILL NOT be called.
 *  - PJ_EPENDING   If the operation has been queued, and the callback will be
 *                  called when data has been received.
 *  - non-zero      The return value indicates the error code.
 */
PJ_DECL(pj_status_t) pj_ioqueue_recv( pj_ioqueue_key_t *key,
                                      pj_ioqueue_op_key_t *op_key,
				      void *buffer,
				      pj_ssize_t *length,
				      pj_uint32_t flags );

/**
 * This function behaves similarly as #pj_ioqueue_recv(), except that it is
 * normally called for socket, and the remote address will also be returned
 * along with the data. Caller MUST make sure that both buffer and addr
 * remain valid until the framework completes reading the data.
 *
 * @param key	    The key that uniquely identifies the handle.
 * @param op_key    An operation specific key to be associated with the
 *                  pending operation, so that application can keep track of
 *                  which operation has been completed when the callback is
 *                  called.
 * @param buffer    The buffer to hold the read data. The caller MUST make sure
 *		    that this buffer remain valid until the framework completes
 *		    reading the handle.
 * @param length    On input, it specifies the size of the buffer. If data is
 *                  available to be read immediately, the function returns
 *                  PJ_SUCCESS and this argument will be filled with the
 *                  amount of data read. If the function is pending, caller
 *                  will be notified about the amount of data read in the
 *                  callback. This parameter can point to local variable in
 *                  caller's stack and doesn't have to remain valid for the
 *                  duration of pending operation.
 * @param flags     Recv flag. If flags has PJ_IOQUEUE_ALWAYS_ASYNC then
 *		    the function will never return PJ_SUCCESS.
 * @param addr      Optional Pointer to buffer to receive the address.
 * @param addrlen   On input, specifies the length of the address buffer.
 *                  On output, it will be filled with the actual length of
 *                  the address. This argument can be NULL if \c addr is not
 *                  specified.
 *
 * @return
 *  - PJ_SUCCESS    If immediate data has been received. In this case, the 
 *		    callback must have been called before this function 
 *		    returns, and no pending operation is scheduled.
 *  - PJ_EPENDING   If the operation has been queued.
 *  - non-zero      The return value indicates the error code.
 */
PJ_DECL(pj_status_t) pj_ioqueue_recvfrom( pj_ioqueue_key_t *key,
                                          pj_ioqueue_op_key_t *op_key,
					  void *buffer,
					  pj_ssize_t *length,
                                          pj_uint32_t flags,
					  pj_sockaddr_t *addr,
					  int *addrlen);

/**
 * Instruct the I/O Queue to write to the handle. This function will return
 * immediately (i.e. non-blocking) regardless whether some data has been 
 * transferred. If the function can't complete immediately, the caller will
 * be notified about the completion when it calls pj_ioqueue_poll(). If 
 * operation completes immediately and data has been transferred, the function
 * returns PJ_SUCCESS and the callback will NOT be called.
 *
 * @param key	    The key that identifies the handle.
 * @param op_key    An operation specific key to be associated with the
 *                  pending operation, so that application can keep track of
 *                  which operation has been completed when the callback is
 *                  called.
 * @param data	    The data to send. Caller MUST make sure that this buffer 
 *		    remains valid until the write operation completes.
 * @param length    On input, it specifies the length of data to send. When
 *                  data was sent immediately, this function returns PJ_SUCCESS
 *                  and this parameter contains the length of data sent. If
 *                  data can not be sent immediately, an asynchronous operation
 *                  is scheduled and caller will be notified via callback the
 *                  number of bytes sent. This parameter can point to local 
 *                  variable on caller's stack and doesn't have to remain 
 *                  valid until the operation has completed.
 * @param flags     Send flags. If flags has PJ_IOQUEUE_ALWAYS_ASYNC then
 *		    the function will never return PJ_SUCCESS.
 *
 * @return
 *  - PJ_SUCCESS    If data was immediately transferred. In this case, no
 *                  pending operation has been scheduled and the callback
 *                  WILL NOT be called.
 *  - PJ_EPENDING   If the operation has been queued. Once data base been
 *                  transferred, the callback will be called.
 *  - non-zero      The return value indicates the error code.
 */
PJ_DECL(pj_status_t) pj_ioqueue_send( pj_ioqueue_key_t *key,
                                      pj_ioqueue_op_key_t *op_key,
				      const void *data,
				      pj_ssize_t *length,
				      pj_uint32_t flags );


/**
 * Instruct the I/O Queue to write to the handle. This function will return
 * immediately (i.e. non-blocking) regardless whether some data has been 
 * transferred. If the function can't complete immediately, the caller will
 * be notified about the completion when it calls pj_ioqueue_poll(). If 
 * operation completes immediately and data has been transferred, the function
 * returns PJ_SUCCESS and the callback will NOT be called.
 *
 * @param key	    the key that identifies the handle.
 * @param op_key    An operation specific key to be associated with the
 *                  pending operation, so that application can keep track of
 *                  which operation has been completed when the callback is
 *                  called.
 * @param data	    the data to send. Caller MUST make sure that this buffer 
 *		    remains valid until the write operation completes.
 * @param length    On input, it specifies the length of data to send. When
 *                  data was sent immediately, this function returns PJ_SUCCESS
 *                  and this parameter contains the length of data sent. If
 *                  data can not be sent immediately, an asynchronous operation
 *                  is scheduled and caller will be notified via callback the
 *                  number of bytes sent. This parameter can point to local 
 *                  variable on caller's stack and doesn't have to remain 
 *                  valid until the operation has completed.
 * @param flags     send flags. If flags has PJ_IOQUEUE_ALWAYS_ASYNC then
 *		    the function will never return PJ_SUCCESS.
 * @param addr      Optional remote address.
 * @param addrlen   Remote address length, \c addr is specified.
 *
 * @return
 *  - PJ_SUCCESS    If data was immediately written.
 *  - PJ_EPENDING   If the operation has been queued.
 *  - non-zero      The return value indicates the error code.
 */
PJ_DECL(pj_status_t) pj_ioqueue_sendto( pj_ioqueue_key_t *key,
                                        pj_ioqueue_op_key_t *op_key,
					const void *data,
					pj_ssize_t *length,
                                        pj_uint32_t flags,
					const pj_sockaddr_t *addr,
					int addrlen);


/**
 * !}
 */

PJ_END_DECL

#endif	/* __PJ_IOQUEUE_H__ */

