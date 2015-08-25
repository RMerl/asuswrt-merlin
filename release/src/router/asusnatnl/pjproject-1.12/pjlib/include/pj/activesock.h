/* $Id: activesock.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJ_ASYNCSOCK_H__
#define __PJ_ASYNCSOCK_H__

/**
 * @file activesock.h
 * @brief Active socket
 */

#include <pj/ioqueue.h>
#include <pj/sock.h>


PJ_BEGIN_DECL

/**
 * @defgroup PJ_ACTIVESOCK Active socket I/O
 * @brief Active socket performs active operations on socket.
 * @ingroup PJ_IO
 * @{
 *
 * Active socket is a higher level abstraction to the ioqueue. It provides
 * automation to socket operations which otherwise would have to be done
 * manually by the applications. For example with socket recv(), recvfrom(),
 * and accept() operations, application only needs to invoke these
 * operation once, and it will be notified whenever data or incoming TCP
 * connection (in the case of accept()) arrives.
 */

/**
 * This opaque structure describes the active socket.
 */
typedef struct pj_activesock_t pj_activesock_t;

/**
 * This structure contains the callbacks to be called by the active socket.
 */
typedef struct pj_activesock_cb
{
    /**
     * This callback is called when a data arrives as the result of
     * pj_activesock_start_read().
     *
     * @param asock	The active socket.
     * @param data	The buffer containing the new data, if any. If 
     *			the status argument is non-PJ_SUCCESS, this 
     *			argument may be NULL.
     * @param size	The length of data in the buffer.
     * @param status	The status of the read operation. This may contain
     *			non-PJ_SUCCESS for example when the TCP connection
     *			has been closed. In this case, the buffer may
     *			contain left over data from previous callback which
     *			the application may want to process.
     * @param remainder	If application wishes to leave some data in the 
     *			buffer (common for TCP applications), it should 
     *			move the remainder data to the front part of the 
     *			buffer and set the remainder length here. The value
     *			of this parameter will be ignored for datagram
     *			sockets.
     *
     * @return		PJ_TRUE if further read is desired, and PJ_FALSE 
     *			when application no longer wants to receive data.
     *			Application may destroy the active socket in the
     *			callback and return PJ_FALSE here.
     */
    pj_bool_t (*on_data_read)(pj_activesock_t *asock,
			      void *data,
			      pj_size_t size,
			      pj_status_t status,
			      pj_size_t *remainder);
    /**
     * This callback is called when a packet arrives as the result of
     * pj_activesock_start_recvfrom().
     *
     * @param asock	The active socket.
     * @param data	The buffer containing the packet, if any. If 
     *			the status argument is non-PJ_SUCCESS, this 
     *			argument will be set to NULL.
     * @param size	The length of packet in the buffer. If 
     *			the status argument is non-PJ_SUCCESS, this 
     *			argument will be set to zero.
     * @param src_addr	Source address of the packet.
     * @param addr_len	Length of the source address.
     * @param status	This contains
     *
     * @return		PJ_TRUE if further read is desired, and PJ_FALSE 
     *			when application no longer wants to receive data.
     *			Application may destroy the active socket in the
     *			callback and return PJ_FALSE here.
     */
    pj_bool_t (*on_data_recvfrom)(pj_activesock_t *asock,
				  void *data,
				  pj_size_t size,
				  const pj_sockaddr_t *src_addr,
				  int addr_len,
				  pj_status_t status);

    /**
     * This callback is called when data has been sent.
     *
     * @param asock	The active socket.
     * @param send_key	Key associated with the send operation.
     * @param sent	If value is positive non-zero it indicates the
     *			number of data sent. When the value is negative,
     *			it contains the error code which can be retrieved
     *			by negating the value (i.e. status=-sent).
     *
     * @return		Application may destroy the active socket in the
     *			callback and return PJ_FALSE here.
     */
    pj_bool_t (*on_data_sent)(pj_activesock_t *asock,
			      pj_ioqueue_op_key_t *send_key,
			      pj_ssize_t sent);

    /**
     * This callback is called when new connection arrives as the result
     * of pj_activesock_start_accept().
     *
     * @param asock	The active socket.
     * @param newsock	The new incoming socket.
     * @param src_addr	The source address of the connection.
     * @param addr_len	Length of the source address.
     *
     * @return		PJ_TRUE if further accept() is desired, and PJ_FALSE
     *			when application no longer wants to accept incoming
     *			connection. Application may destroy the active socket
     *			in the callback and return PJ_FALSE here.
     */
    pj_bool_t (*on_accept_complete)(pj_activesock_t *asock,
				    pj_sock_t newsock,
				    const pj_sockaddr_t *src_addr,
				    int src_addr_len);

    /**
     * This callback is called when pending connect operation has been
     * completed.
     *
     * @param asock	The active  socket.
     * @param status	The connection result. If connection has been
     *			successfully established, the status will contain
     *			PJ_SUCCESS.
     *
     * @return		Application may destroy the active socket in the
     *			callback and return PJ_FALSE here. 
     */
    pj_bool_t (*on_connect_complete)(pj_activesock_t *asock,
				     pj_status_t status);

} pj_activesock_cb;


/**
 * Settings that can be given during active socket creation. Application
 * must initialize this structure with #pj_activesock_cfg_default().
 */
typedef struct pj_activesock_cfg
{
    /**
     * Number of concurrent asynchronous operations that is to be supported
     * by the active socket. This value only affects socket receive and
     * accept operations -- the active socket will issue one or more 
     * asynchronous read and accept operations based on the value of this
     * field. Setting this field to more than one will allow more than one
     * incoming data or incoming connections to be processed simultaneously
     * on multiprocessor systems, when the ioqueue is polled by more than
     * one threads.
     *
     * The default value is 1.
     */
    unsigned async_cnt;

    /**
     * The ioqueue concurrency to be forced on the socket when it is 
     * registered to the ioqueue. See #pj_ioqueue_set_concurrency() for more
     * info about ioqueue concurrency.
     *
     * When this value is -1, the concurrency setting will not be forced for
     * this socket, and the socket will inherit the concurrency setting of 
     * the ioqueue. When this value is zero, the active socket will disable
     * concurrency for the socket. When this value is +1, the active socket
     * will enable concurrency for the socket.
     *
     * The default value is -1.
     */
    int concurrency;

    /**
     * If this option is specified, the active socket will make sure that
     * asynchronous send operation with stream oriented socket will only
     * call the callback after all data has been sent. This means that the
     * active socket will automatically resend the remaining data until
     * all data has been sent.
     *
     * Please note that when this option is specified, it is possible that
     * error is reported after partial data has been sent. Also setting
     * this will disable the ioqueue concurrency for the socket.
     *
     * Default value is 1.
     */
    pj_bool_t whole_data;

} pj_activesock_cfg;


/**
 * Initialize the active socket configuration with the default values.
 *
 * @param cfg		The configuration to be initialized.
 */
PJ_DECL(void) pj_activesock_cfg_default(pj_activesock_cfg *cfg);


/**
 * Create the active socket for the specified socket. This will register
 * the socket to the specified ioqueue. 
 *
 * @param pool		Pool to allocate memory from.
 * @param sock		The socket handle.
 * @param sock_type	Specify socket type, either pj_SOCK_DGRAM() or
 *			pj_SOCK_STREAM(). The active socket needs this
 *			information to handle connection closure for
 *			connection oriented sockets.
 * @param ioqueue	The ioqueue to use.
 * @param opt		Optional settings. When this setting is not specifed,
 *			the default values will be used.
 * @param cb		Pointer to structure containing application
 *			callbacks.
 * @param user_data	Arbitrary user data to be associated with this
 *			active socket.
 * @param p_asock	Pointer to receive the active socket instance.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_activesock_create(pj_pool_t *pool,
					  pj_sock_t sock,
					  int sock_type,
					  const pj_activesock_cfg *opt,
					  pj_ioqueue_t *ioqueue,
					  const pj_activesock_cb *cb,
					  void *user_data,
					  pj_activesock_t **p_asock);

/**
 * Create UDP socket descriptor, bind it to the specified address, and 
 * create the active socket for the socket descriptor.
 *
 * @param pool		Pool to allocate memory from.
 * @param addr		Specifies the address family of the socket and the
 *			address where the socket should be bound to. If
 *			this argument is NULL, then AF_INET is assumed and
 *			the socket will be bound to any addresses and port.
 * @param opt		Optional settings. When this setting is not specifed,
 *			the default values will be used.
 * @param cb		Pointer to structure containing application
 *			callbacks.
 * @param user_data	Arbitrary user data to be associated with this
 *			active socket.
 * @param p_asock	Pointer to receive the active socket instance.
 * @param bound_addr	If this argument is specified, it will be filled with
 *			the bound address on return.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_activesock_create_udp(pj_pool_t *pool,
					      const pj_sockaddr *addr,
					      const pj_activesock_cfg *opt,
					      pj_ioqueue_t *ioqueue,
					      const pj_activesock_cb *cb,
					      void *user_data,
					      pj_activesock_t **p_asock,
					      pj_sockaddr *bound_addr);


/**
 * Close the active socket. This will unregister the socket from the
 * ioqueue and ultimately close the socket.
 *
 * @param asock	    The active socket.
 *
 * @return	    PJ_SUCCESS if the operation has been successful,
 *		    or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_activesock_close(pj_activesock_t *asock);

#if (defined(PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
     PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0) || \
     defined(DOXYGEN)
/**
 * Set iPhone OS background mode setting. Setting to 1 will enable TCP
 * active socket to receive incoming data when application is in the
 * background. Setting to 0 will disable it. Default value of this
 * setting is PJ_ACTIVESOCK_TCP_IPHONE_OS_BG.
 *
 * This API is only available if PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT
 * is set to non-zero.
 *
 * @param asock	    The active socket.
 * @param val	    The value of background mode setting.
 *
 */
PJ_DECL(void) pj_activesock_set_iphone_os_bg(pj_activesock_t *asock,
					     int val);

/**
 * Enable/disable support for iPhone OS background mode. This setting
 * will apply globally and will affect any active sockets created
 * afterwards, if you want to change the setting for a particular
 * active socket, use #pj_activesock_set_iphone_os_bg() instead.
 * By default, this setting is enabled.
 *
 * This API is only available if PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT
 * is set to non-zero.
 *
 * @param val	    The value of global background mode setting.
 *
 */
PJ_DECL(void) pj_activesock_enable_iphone_os_bg(pj_bool_t val);
#endif

/**
 * Associate arbitrary data with the active socket. Application may
 * inspect this data in the callbacks and associate it with higher
 * level processing.
 *
 * @param asock	    The active socket.
 * @param user_data The user data to be associated with the active
 *		    socket.
 *
 * @return	    PJ_SUCCESS if the operation has been successful,
 *		    or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_activesock_set_user_data(pj_activesock_t *asock,
						 void *user_data);

/**
 * Retrieve the user data previously associated with this active
 * socket.
 *
 * @param asock	    The active socket.
 *
 * @return	    The user data.
 */
PJ_DECL(void*) pj_activesock_get_user_data(pj_activesock_t *asock);


/**
 * Starts read operation on this active socket. This function will create
 * \a async_cnt number of buffers (the \a async_cnt parameter was given
 * in \a pj_activesock_create() function) where each buffer is \a buff_size
 * long. The buffers are allocated from the specified \a pool. Once the 
 * buffers are created, it then issues \a async_cnt number of asynchronous
 * \a recv() operations to the socket and returns back to caller. Incoming
 * data on the socket will be reported back to application via the 
 * \a on_data_read() callback.
 *
 * Application only needs to call this function once to initiate read
 * operations. Further read operations will be done automatically by the
 * active socket when \a on_data_read() callback returns non-zero. 
 *
 * @param asock	    The active socket.
 * @param pool	    Pool used to allocate buffers for incoming data.
 * @param buff_size The size of each buffer, in bytes.
 * @param flags	    Flags to be given to pj_ioqueue_recv().
 *
 * @return	    PJ_SUCCESS if the operation has been successful,
 *		    or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_activesock_start_read(pj_activesock_t *asock,
					      pj_pool_t *pool,
					      unsigned buff_size,
					      pj_uint32_t flags);

/**
 * Same as #pj_activesock_start_read(), except that the application
 * supplies the buffers for the read operation so that the acive socket
 * does not have to allocate the buffers.
 *
 * @param asock	    The active socket.
 * @param pool	    Pool used to allocate buffers for incoming data.
 * @param buff_size The size of each buffer, in bytes.
 * @param readbuf   Array of packet buffers, each has buff_size size.
 * @param flags	    Flags to be given to pj_ioqueue_recv().
 *
 * @return	    PJ_SUCCESS if the operation has been successful,
 *		    or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_activesock_start_read2(pj_activesock_t *asock,
					       pj_pool_t *pool,
					       unsigned buff_size,
					       void *readbuf[],
					       pj_uint32_t flags);

/**
 * Same as pj_activesock_start_read(), except that this function is used
 * only for datagram sockets, and it will trigger \a on_data_recvfrom()
 * callback instead.
 *
 * @param asock	    The active socket.
 * @param pool	    Pool used to allocate buffers for incoming data.
 * @param buff_size The size of each buffer, in bytes.
 * @param flags	    Flags to be given to pj_ioqueue_recvfrom().
 *
 * @return	    PJ_SUCCESS if the operation has been successful,
 *		    or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_activesock_start_recvfrom(pj_activesock_t *asock,
						  pj_pool_t *pool,
						  unsigned buff_size,
						  pj_uint32_t flags);

/**
 * Same as #pj_activesock_start_recvfrom() except that the recvfrom() 
 * operation takes the buffer from the argument rather than creating
 * new ones.
 *
 * @param asock	    The active socket.
 * @param pool	    Pool used to allocate buffers for incoming data.
 * @param buff_size The size of each buffer, in bytes.
 * @param readbuf   Array of packet buffers, each has buff_size size.
 * @param flags	    Flags to be given to pj_ioqueue_recvfrom().
 *
 * @return	    PJ_SUCCESS if the operation has been successful,
 *		    or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_activesock_start_recvfrom2(pj_activesock_t *asock,
						   pj_pool_t *pool,
						   unsigned buff_size,
						   void *readbuf[],
						   pj_uint32_t flags);

/**
 * Send data using the socket.
 *
 * @param asock	    The active socket.
 * @param send_key  The operation key to send the data, which is useful
 *		    if application wants to submit multiple pending
 *		    send operations and want to track which exact data 
 *		    has been sent in the \a on_data_sent() callback.
 * @param data	    The data to be sent. This data must remain valid
 *		    until the data has been sent.
 * @param size	    The size of the data.
 * @param flags	    Flags to be given to pj_ioqueue_send().
 *
 *
 * @return	    PJ_SUCCESS if data has been sent immediately, or
 *		    PJ_EPENDING if data cannot be sent immediately. In
 *		    this case the \a on_data_sent() callback will be
 *		    called when data is actually sent. Any other return
 *		    value indicates error condition.
 */
PJ_DECL(pj_status_t) pj_activesock_send(pj_activesock_t *asock,
					pj_ioqueue_op_key_t *send_key,
					const void *data,
					pj_ssize_t *size,
					unsigned flags);

/**
 * Send datagram using the socket.
 *
 * @param asock	    The active socket.
 * @param send_key  The operation key to send the data, which is useful
 *		    if application wants to submit multiple pending
 *		    send operations and want to track which exact data 
 *		    has been sent in the \a on_data_sent() callback.
 * @param data	    The data to be sent. This data must remain valid
 *		    until the data has been sent.
 * @param size	    The size of the data.
 * @param flags	    Flags to be given to pj_ioqueue_send().
 * @param addr	    The destination address.
 * @param addr_len  The length of the address.
 *
 * @return	    PJ_SUCCESS if data has been sent immediately, or
 *		    PJ_EPENDING if data cannot be sent immediately. In
 *		    this case the \a on_data_sent() callback will be
 *		    called when data is actually sent. Any other return
 *		    value indicates error condition.
 */
PJ_DECL(pj_status_t) pj_activesock_sendto(pj_activesock_t *asock,
					  pj_ioqueue_op_key_t *send_key,
					  const void *data,
					  pj_ssize_t *size,
					  unsigned flags,
					  const pj_sockaddr_t *addr,
					  int addr_len);

#if PJ_HAS_TCP
/**
 * Starts asynchronous socket accept() operations on this active socket. 
 * Application must bind the socket before calling this function. This 
 * function will issue \a async_cnt number of asynchronous \a accept() 
 * operations to the socket and returns back to caller. Incoming
 * connection on the socket will be reported back to application via the
 * \a on_accept_complete() callback.
 *
 * Application only needs to call this function once to initiate accept()
 * operations. Further accept() operations will be done automatically by 
 * the active socket when \a on_accept_complete() callback returns non-zero.
 *
 * @param asock	    The active socket.
 * @param pool	    Pool used to allocate some internal data for the
 *		    operation.
 *
 * @return	    PJ_SUCCESS if the operation has been successful,
 *		    or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_activesock_start_accept(pj_activesock_t *asock,
						pj_pool_t *pool);

/**
 * Starts asynchronous socket connect() operation for this socket. Once
 * the connection is done (either successfully or not), the 
 * \a on_connect_complete() callback will be called.
 *
 * @param asock	    The active socket.
 * @param pool	    The pool to allocate some internal data for the
 *		    operation.
 * @param remaddr   Remote address.
 * @param addr_len  Length of the remote address.
 *
 * @return	    PJ_SUCCESS if connection can be established immediately,
 *		    or PJ_EPENDING if connection cannot be established 
 *		    immediately. In this case the \a on_connect_complete()
 *		    callback will be called when connection is complete. 
 *		    Any other return value indicates error condition.
 */
PJ_DECL(pj_status_t) pj_activesock_start_connect(pj_activesock_t *asock,
						 pj_pool_t *pool,
						 const pj_sockaddr_t *remaddr,
						 int addr_len);

#endif	/* PJ_HAS_TCP */

/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJ_ASYNCSOCK_H__ */

