/* $Id: ioqueue_common_abs.c 3553 2011-05-05 06:14:19Z nanang $ */
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

/*
 * ioqueue_common_abs.c
 *
 * This contains common functionalities to emulate proactor pattern with
 * various event dispatching mechanisms (e.g. select, epoll).
 *
 * This file will be included by the appropriate ioqueue implementation.
 * This file is NOT supposed to be compiled as stand-alone source.
 */

#define THIS_FILE "ioq_common_abs.c"

#define PENDING_RETRY	2

static void ioqueue_init( pj_ioqueue_t *ioqueue )
{
    ioqueue->lock = NULL;
    ioqueue->auto_delete_lock = 0;
    ioqueue->default_concurrency = PJ_IOQUEUE_DEFAULT_ALLOW_CONCURRENCY;
}

static pj_status_t ioqueue_destroy(pj_ioqueue_t *ioqueue)
{
    if (ioqueue->auto_delete_lock && ioqueue->lock ) {
	pj_lock_release(ioqueue->lock);
        return pj_lock_destroy(ioqueue->lock);
    }
    
    return PJ_SUCCESS;
}

/*
 * pj_ioqueue_set_lock()
 */
PJ_DEF(pj_status_t) pj_ioqueue_set_lock( pj_ioqueue_t *ioqueue, 
					 pj_lock_t *lock,
					 pj_bool_t auto_delete )
{
    PJ_ASSERT_RETURN(ioqueue && lock, PJ_EINVAL);

    if (ioqueue->auto_delete_lock && ioqueue->lock) {
        pj_lock_destroy(ioqueue->lock);
    }

    ioqueue->lock = lock;
    ioqueue->auto_delete_lock = auto_delete;

    return PJ_SUCCESS;
}

static pj_status_t ioqueue_init_key( pj_pool_t *pool,
                                     pj_ioqueue_t *ioqueue,
                                     pj_ioqueue_key_t *key,
                                     pj_sock_t sock,
                                     pj_grp_lock_t *grp_lock,
                                     void *user_data,
                                     const pj_ioqueue_callback *cb)
{
    pj_status_t rc;
    int optlen;

    PJ_UNUSED_ARG(pool);

    key->ioqueue = ioqueue;
    key->fd = sock;
    key->user_data = user_data;
	pj_list_init(&key->read_list);
	pj_list_init(&key->write_list);
	pj_list_init(&key->write_gclist);
#if PJ_HAS_TCP
    pj_list_init(&key->accept_list);
    key->connecting = 0;
#endif

    /* Save callback. */
    pj_memcpy(&key->cb, cb, sizeof(pj_ioqueue_callback));

#if PJ_IOQUEUE_HAS_SAFE_UNREG
    /* Set initial reference count to 1 */
    pj_assert(key->ref_count == 0);
    ++key->ref_count;

    key->closing = 0;
#endif

    rc = pj_ioqueue_set_concurrency(key, ioqueue->default_concurrency);
    if (rc != PJ_SUCCESS)
	return rc;

    /* Get socket type. When socket type is datagram, some optimization
     * will be performed during send to allow parallel send operations.
     */
    optlen = sizeof(key->fd_type);
    rc = pj_sock_getsockopt(sock, pj_SOL_SOCKET(), pj_SO_TYPE(),
                            &key->fd_type, &optlen);
    if (rc != PJ_SUCCESS)
        key->fd_type = pj_SOCK_STREAM();

    /* Create mutex for the key. */
#if !PJ_IOQUEUE_HAS_SAFE_UNREG
    rc = pj_lock_create_simple_mutex(poll, NULL, &key->lock);
#endif
    if (rc != PJ_SUCCESS)
	return rc;

    /* Group lock */
    key->grp_lock = grp_lock;
    if (key->grp_lock) {
	pj_grp_lock_add_ref_dbg(key->grp_lock, "ioqueue", 0);
    }
    
    return PJ_SUCCESS;
}

/*
 * pj_ioqueue_get_user_data()
 *
 * Obtain value associated with a key.
 */
PJ_DEF(void*) pj_ioqueue_get_user_data( pj_ioqueue_key_t *key )
{
    PJ_ASSERT_RETURN(key != NULL, NULL);
    return key->user_data;
}

/*
 * pj_ioqueue_set_user_data()
 */
PJ_DEF(pj_status_t) pj_ioqueue_set_user_data( pj_ioqueue_key_t *key,
                                              void *user_data,
                                              void **old_data)
{
    PJ_ASSERT_RETURN(key, PJ_EINVAL);

    if (old_data)
        *old_data = key->user_data;
    key->user_data = user_data;

    return PJ_SUCCESS;
}

PJ_INLINE(int) key_has_pending_write(pj_ioqueue_key_t *key)
{
    return !pj_list_empty(&key->write_list);
}

PJ_INLINE(int) key_has_pending_read(pj_ioqueue_key_t *key)
{
    return !pj_list_empty(&key->read_list);
}

PJ_INLINE(int) key_has_pending_accept(pj_ioqueue_key_t *key)
{
#if PJ_HAS_TCP
    return !pj_list_empty(&key->accept_list);
#else
    PJ_UNUSED_ARG(key);
    return 0;
#endif
}

PJ_INLINE(int) key_has_pending_connect(pj_ioqueue_key_t *key)
{
    return key->connecting;
}


#if PJ_IOQUEUE_HAS_SAFE_UNREG
#   define IS_CLOSING(key)  (key->closing)
#else
#   define IS_CLOSING(key)  (0)
#endif


/*
 * ioqueue_dispatch_event()
 *
 * Report occurence of an event in the key to be processed by the
 * framework.
 */
void ioqueue_dispatch_write_event(pj_ioqueue_t *ioqueue, pj_ioqueue_key_t *h)
{
    pj_bool_t urgent = PJ_FALSE;

    /* Lock the key. */
    pj_ioqueue_lock_key(h);

    if (IS_CLOSING(h)) {
	pj_ioqueue_unlock_key(h);
	return;
    }

#if defined(PJ_HAS_TCP) && PJ_HAS_TCP!=0
    if (h->connecting) {
	/* Completion of connect() operation */
	pj_status_t status;
	pj_bool_t has_lock;

	/* Clear operation. */
	h->connecting = 0;

        ioqueue_remove_from_set(ioqueue, h, WRITEABLE_EVENT);
        ioqueue_remove_from_set(ioqueue, h, EXCEPTION_EVENT);


#if (defined(PJ_HAS_SO_ERROR) && PJ_HAS_SO_ERROR!=0)
	/* from connect(2): 
	 * On Linux, use getsockopt to read the SO_ERROR option at
	 * level SOL_SOCKET to determine whether connect() completed
	 * successfully (if SO_ERROR is zero).
	 */
	{
	  int value;
	  int vallen = sizeof(value);
	  int gs_rc = pj_sock_getsockopt(h->fd, SOL_SOCKET, SO_ERROR, 
					 &value, &vallen);
	  if (gs_rc != 0) {
	    /* Argh!! What to do now??? 
	     * Just indicate that the socket is connected. The
	     * application will get error as soon as it tries to use
	     * the socket to send/receive.
	     */
	      status = PJ_SUCCESS;
	  } else {
	      status = PJ_STATUS_FROM_OS(value);
	  }
 	}
#elif (defined(PJ_WIN32) && PJ_WIN32!=0) || (defined(PJ_WIN64) && PJ_WIN64!=0) 
	status = PJ_SUCCESS; /* success */
#else
	/* Excellent information in D.J. Bernstein page:
	 * http://cr.yp.to/docs/connect.html
	 *
	 * Seems like the most portable way of detecting connect()
	 * failure is to call getpeername(). If socket is connected,
	 * getpeername() will return 0. If the socket is not connected,
	 * it will return ENOTCONN, and read(fd, &ch, 1) will produce
	 * the right errno through error slippage. This is a combination
	 * of suggestions from Douglas C. Schmidt and Ken Keys.
	 */
	{
	    struct sockaddr_in addr;
	    int addrlen = sizeof(addr);

	    status = pj_sock_getpeername(h->fd, (struct sockaddr*)&addr,
				         &addrlen);
	}
#endif

        /* Unlock; from this point we don't need to hold key's mutex
	 * (unless concurrency is disabled, which in this case we should
	 * hold the mutex while calling the callback) */
	if (h->allow_concurrent) {
	    /* concurrency may be changed while we're in the callback, so
	     * save it to a flag.
	     */
	    has_lock = PJ_FALSE;
	    pj_ioqueue_unlock_key(h);
	} else {
	    has_lock = PJ_TRUE;
	}

	/* Call callback. */
        if (h->cb.on_connect_complete && !IS_CLOSING(h))
	    (*h->cb.on_connect_complete)(h, status);

	/* Unlock if we still hold the lock */
	if (has_lock) {
	    pj_ioqueue_unlock_key(h);
	}

        /* Done. */

    } else 
#endif /* PJ_HAS_TCP */
    if (key_has_pending_write(h)) {
	/* Socket is writable. */
        struct write_operation *write_op;
        pj_ssize_t sent;
        pj_status_t send_rc = PJ_SUCCESS;

        /* Get the first in the queue. */
        write_op = h->write_list.next;
        urgent = (write_op->flags & PJ_IOQUEUE_URGENT_DATA) ? PJ_TRUE : PJ_FALSE;
        write_op->flags &= ~(PJ_IOQUEUE_URGENT_DATA); //clear the PJ_IOQUEUE_URGENT_DATA

        /* For datagrams, we can remove the write_op from the list
         * so that send() can work in parallel.
         */
        if (h->fd_type == pj_SOCK_DGRAM()) {
            pj_list_erase(write_op);

            if (pj_list_empty(&h->write_list))
                ioqueue_remove_from_set(ioqueue, h, WRITEABLE_EVENT);

        }

        /* Send the data. 
         * Unfortunately we must do this while holding key's mutex, thus
         * preventing parallel write on a single key.. :-((
         */
        sent = write_op->size - write_op->written;
        if (write_op->op == PJ_IOQUEUE_OP_SEND) {
            send_rc = pj_sock_send(h->fd, write_op->buf+write_op->written,
                                   &sent, write_op->flags);
            PJ_LOG(6, (THIS_FILE, "ioqueue_dispatch_write_event: write_op=[%p], send_rc=%d, sent=%d", 
                       write_op, send_rc, sent));
            //DumpPacket(write_op->buf+write_op->written, sent, 1, 3);
	    /* Can't do this. We only clear "op" after we're finished sending
	     * the whole buffer.
	     */
	    //write_op->op = 0;
        } else if (write_op->op == PJ_IOQUEUE_OP_SEND_TO) {
	    int retry = 2;
	    while (--retry >= 0) {
		send_rc = pj_sock_sendto(h->fd, 
					 write_op->buf+write_op->written,
					 &sent, write_op->flags,
					 &write_op->rmt_addr, 
					 write_op->rmt_addrlen);
			PJ_LOG(6, (THIS_FILE, "ioqueue_dispatch_write_event () pj_sock_sendto() send_rc=%d, sent=%d", 
				send_rc, sent));
			//DumpPacket(write_op->buf+write_op->written, sent, 1, 3);
#if defined(PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
	    PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0
		/* Special treatment for dead UDP sockets here, see ticket #1107 */
		if (send_rc==PJ_STATUS_FROM_OS(EPIPE) && !IS_CLOSING(h) &&
		    h->fd_type==pj_SOCK_DGRAM())
		{
		    PJ_PERROR(4,(THIS_FILE, send_rc,
				 "Send error for socket %d, retrying",
				 h->fd));
		    replace_udp_sock(h);
		    continue;
		}
#endif
		break;
	    }

	    /* Can't do this. We only clear "op" after we're finished sending
	     * the whole buffer.
	     */
	    //write_op->op = 0;
        } else {
            pj_assert(!"Invalid operation type!");
	    write_op->op = PJ_IOQUEUE_OP_NONE;
            send_rc = PJ_EBUG;
        }

        if (send_rc == PJ_SUCCESS) {
            write_op->written += sent;
        } /*else {
            pj_assert(send_rc > 0);
            write_op->written = -send_rc;
			}*/
#if 1
		// Free the data buffer if data sent completely
		if (write_op->buf && 
			write_op->written == (pj_ssize_t)write_op->size) 
		{
			//free(write_op->buf);
			pj_mem_free(write_op->buf, write_op->size);
			write_op->buf = NULL;
		}
#endif

        /* Are we finished with this buffer? */
        if (send_rc!=PJ_SUCCESS || 
            write_op->written == (pj_ssize_t)write_op->size ||
            h->fd_type == pj_SOCK_DGRAM()) 
        {
	    pj_bool_t has_lock;

	    write_op->op = PJ_IOQUEUE_OP_NONE;

            if (h->fd_type != pj_SOCK_DGRAM()) {
                /* Write completion of the whole stream. */
                pj_list_erase(write_op);

                /* Clear operation if there's no more data to send. */
                if (pj_list_empty(&h->write_list))
                    ioqueue_remove_from_set(ioqueue, h, WRITEABLE_EVENT);

            }

	    /* Unlock; from this point we don't need to hold key's mutex
	     * (unless concurrency is disabled, which in this case we should
	     * hold the mutex while calling the callback) */
	    if (h->allow_concurrent) {
		/* concurrency may be changed while we're in the callback, so
		 * save it to a flag.
		 */
		has_lock = PJ_FALSE;
		pj_ioqueue_unlock_key(h);
		PJ_RACE_ME(5);
	    } else {
		has_lock = PJ_TRUE;
	    }

	    /* Call callback. */
            if (h->cb.on_write_complete && !IS_CLOSING(h)) {
	        (*h->cb.on_write_complete)(h, 
                                           (pj_ioqueue_op_key_t*)write_op,
                                           write_op->written);
            }

	    if (has_lock) {
		pj_ioqueue_unlock_key(h);
	    }

		if (urgent) {
			//free(write_op);
			pj_mem_free(write_op, sizeof(pj_ioqueue_op_key_t));
		}

        } else {
            pj_ioqueue_unlock_key(h);
        }

        /* Done. */
    } else {
        /*
         * This is normal; execution may fall here when multiple threads
         * are signalled for the same event, but only one thread eventually
         * able to process the event.
         */
	pj_ioqueue_unlock_key(h);
    }
}

void ioqueue_dispatch_read_event( pj_ioqueue_t *ioqueue, pj_ioqueue_key_t *h )
{
    pj_status_t rc;

    /* Lock the key. */
    pj_ioqueue_lock_key(h);

    if (IS_CLOSING(h)) {
	pj_ioqueue_unlock_key(h);
	return;
    }

#   if PJ_HAS_TCP
    if (!pj_list_empty(&h->accept_list)) {

        struct accept_operation *accept_op;
	pj_bool_t has_lock;
	
        /* Get one accept operation from the list. */
	accept_op = h->accept_list.next;
        pj_list_erase(accept_op);
        accept_op->op = PJ_IOQUEUE_OP_NONE;

	/* Clear bit in fdset if there is no more pending accept */
        if (pj_list_empty(&h->accept_list))
            ioqueue_remove_from_set(ioqueue, h, READABLE_EVENT);

	rc=pj_sock_accept(h->fd, accept_op->accept_fd, 
                          accept_op->rmt_addr, accept_op->addrlen);
	if (rc==PJ_SUCCESS && accept_op->local_addr) {
	    rc = pj_sock_getsockname(*accept_op->accept_fd, 
                                     accept_op->local_addr,
				     accept_op->addrlen);
	}

	/* Unlock; from this point we don't need to hold key's mutex
	 * (unless concurrency is disabled, which in this case we should
	 * hold the mutex while calling the callback) */
	if (h->allow_concurrent) {
	    /* concurrency may be changed while we're in the callback, so
	     * save it to a flag.
	     */
	    has_lock = PJ_FALSE;
	    pj_ioqueue_unlock_key(h);
	    PJ_RACE_ME(5);
	} else {
	    has_lock = PJ_TRUE;
	}

	/* Call callback. */
        if (h->cb.on_accept_complete && !IS_CLOSING(h)) {
	    (*h->cb.on_accept_complete)(h, 
                                        (pj_ioqueue_op_key_t*)accept_op,
                                        *accept_op->accept_fd, rc);
	}

	if (has_lock) {
	    pj_ioqueue_unlock_key(h);
	}
    }
    else
#   endif
    if (key_has_pending_read(h)) {
        struct read_operation *read_op;
        pj_ssize_t bytes_read;
	pj_bool_t has_lock;

        /* Get one pending read operation from the list. */
        read_op = h->read_list.next;
        pj_list_erase(read_op);

        /* Clear fdset if there is no pending read. */
        if (pj_list_empty(&h->read_list))
            ioqueue_remove_from_set(ioqueue, h, READABLE_EVENT);

        bytes_read = read_op->size;

	if (read_op->op == PJ_IOQUEUE_OP_RECV_FROM) {
	    read_op->op = PJ_IOQUEUE_OP_NONE;
	    rc = pj_sock_recvfrom(h->fd, read_op->buf, &bytes_read, 
				  read_op->flags,
				  read_op->rmt_addr, 
                                  read_op->rmt_addrlen);
	} else if (read_op->op == PJ_IOQUEUE_OP_RECV) {
	    read_op->op = PJ_IOQUEUE_OP_NONE;
	    rc = pj_sock_recv(h->fd, read_op->buf, &bytes_read, 
			      read_op->flags);
        } else {
            pj_assert(read_op->op == PJ_IOQUEUE_OP_READ);
	    read_op->op = PJ_IOQUEUE_OP_NONE;
            /*
             * User has specified pj_ioqueue_read().
             * On Win32, we should do ReadFile(). But because we got
             * here because of select() anyway, user must have put a
             * socket descriptor on h->fd, which in this case we can
             * just call pj_sock_recv() instead of ReadFile().
             * On Unix, user may put a file in h->fd, so we'll have
             * to call read() here.
             * This may not compile on systems which doesn't have 
             * read(). That's why we only specify PJ_LINUX here so
             * that error is easier to catch.
             */
#	    if defined(PJ_WIN32) && PJ_WIN32 != 0 || \
	       defined(PJ_WIN64) && PJ_WIN64 != 0 || \
	       defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE != 0
                rc = pj_sock_recv(h->fd, read_op->buf, &bytes_read, 
				  read_op->flags);
                //rc = ReadFile((HANDLE)h->fd, read_op->buf, read_op->size,
                //              &bytes_read, NULL);
#           elif (defined(PJ_HAS_UNISTD_H) && PJ_HAS_UNISTD_H != 0)
                bytes_read = read(h->fd, read_op->buf, bytes_read);
                rc = (bytes_read >= 0) ? PJ_SUCCESS : pj_get_os_error();
#	    elif defined(PJ_LINUX_KERNEL) && PJ_LINUX_KERNEL != 0
                bytes_read = sys_read(h->fd, read_op->buf, bytes_read);
                rc = (bytes_read >= 0) ? PJ_SUCCESS : -bytes_read;
#           else
#               error "Implement read() for this platform!"
#           endif
        }
	
	if (rc != PJ_SUCCESS) {
#	    if (defined(PJ_WIN32) && PJ_WIN32 != 0) || \
	       (defined(PJ_WIN64) && PJ_WIN64 != 0) 
	    /* On Win32, for UDP, WSAECONNRESET on the receive side 
	     * indicates that previous sending has triggered ICMP Port 
	     * Unreachable message.
	     * But we wouldn't know at this point which one of previous 
	     * key that has triggered the error, since UDP socket can
	     * be shared!
	     * So we'll just ignore it!
	     */

	    if (rc == PJ_STATUS_FROM_OS(WSAECONNRESET)) {
		//PJ_LOG(4,(THIS_FILE, 
                //          "Ignored ICMP port unreach. on key=%p", h));
	    }
#	    endif

            /* In any case we would report this to caller. */
            bytes_read = -rc;

#if defined(PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
    PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0
	    /* Special treatment for dead UDP sockets here, see ticket #1107 */
	    if (rc == PJ_STATUS_FROM_OS(ENOTCONN) && !IS_CLOSING(h) &&
		h->fd_type==pj_SOCK_DGRAM())
	    {
		replace_udp_sock(h);
	    }
#endif
	}

	/* Unlock; from this point we don't need to hold key's mutex
	 * (unless concurrency is disabled, which in this case we should
	 * hold the mutex while calling the callback) */
	if (h->allow_concurrent) {
	    /* concurrency may be changed while we're in the callback, so
	     * save it to a flag.
	     */
	    has_lock = PJ_FALSE;
	    pj_ioqueue_unlock_key(h);
	    PJ_RACE_ME(5);
	} else {
	    has_lock = PJ_TRUE;
	}

	/* Call callback. */
        if (h->cb.on_read_complete && !IS_CLOSING(h)) {
	    (*h->cb.on_read_complete)(h, 
                                      (pj_ioqueue_op_key_t*)read_op,
                                      bytes_read);
        }

	if (has_lock) {
	    pj_ioqueue_unlock_key(h);
	}

    } else {
        /*
         * This is normal; execution may fall here when multiple threads
         * are signalled for the same event, but only one thread eventually
         * able to process the event.
         */
	pj_ioqueue_unlock_key(h);
    }
}


void ioqueue_dispatch_exception_event( pj_ioqueue_t *ioqueue, 
                                       pj_ioqueue_key_t *h )
{
    pj_bool_t has_lock;

    pj_ioqueue_lock_key(h);

    if (!h->connecting) {
        /* It is possible that more than one thread was woken up, thus
         * the remaining thread will see h->connecting as zero because
         * it has been processed by other thread.
         */
	pj_ioqueue_unlock_key(h);
        return;
    }

    if (IS_CLOSING(h)) {
	pj_ioqueue_unlock_key(h);
	return;
    }

    /* Clear operation. */
    h->connecting = 0;

    ioqueue_remove_from_set(ioqueue, h, WRITEABLE_EVENT);
    ioqueue_remove_from_set(ioqueue, h, EXCEPTION_EVENT);

    /* Unlock; from this point we don't need to hold key's mutex
     * (unless concurrency is disabled, which in this case we should
     * hold the mutex while calling the callback) */
    if (h->allow_concurrent) {
	/* concurrency may be changed while we're in the callback, so
	 * save it to a flag.
	 */
	has_lock = PJ_FALSE;
	pj_ioqueue_unlock_key(h);
	PJ_RACE_ME(5);
    } else {
	has_lock = PJ_TRUE;
    }

    /* Call callback. */
    if (h->cb.on_connect_complete && !IS_CLOSING(h)) {
	pj_status_t status = -1;
#if (defined(PJ_HAS_SO_ERROR) && PJ_HAS_SO_ERROR!=0)
	int value;
	int vallen = sizeof(value);
	int gs_rc = pj_sock_getsockopt(h->fd, SOL_SOCKET, SO_ERROR, 
				       &value, &vallen);
	if (gs_rc == 0) {
	    status = PJ_RETURN_OS_ERROR(value);
	}
#endif

	(*h->cb.on_connect_complete)(h, status);
    }

    if (has_lock) {
	pj_ioqueue_unlock_key(h);
    }
}

/*
 * pj_ioqueue_recv()
 *
 * Start asynchronous recv() from the socket.
 */
PJ_DEF(pj_status_t) pj_ioqueue_recv(  pj_ioqueue_key_t *key,
                                      pj_ioqueue_op_key_t *op_key,
				      void *buffer,
				      pj_ssize_t *length,
				      unsigned flags )
{
    struct read_operation *read_op;

    PJ_ASSERT_RETURN(key && op_key && buffer && length, PJ_EINVAL);
    PJ_CHECK_STACK();

    /* Check if key is closing (need to do this first before accessing
     * other variables, since they might have been destroyed. See ticket
     * #469).
     */
    if (IS_CLOSING(key))
	return PJ_ECANCELLED;

    read_op = (struct read_operation*)op_key;
    read_op->op = PJ_IOQUEUE_OP_NONE;

    /* Try to see if there's data immediately available. 
     */
    if ((flags & PJ_IOQUEUE_ALWAYS_ASYNC) == 0) {
	pj_status_t status;
	pj_ssize_t size;

	size = *length;
	status = pj_sock_recv(key->fd, buffer, &size, flags);
	if (status == PJ_SUCCESS) {
	    /* Yes! Data is available! */
	    *length = size;
	    return PJ_SUCCESS;
	} else {
	    /* If error is not EWOULDBLOCK (or EAGAIN on Linux), report
	     * the error to caller.
	     */
	    if (status != PJ_STATUS_FROM_OS(PJ_BLOCKING_ERROR_VAL))
		return status;
	}
    }

    flags &= ~(PJ_IOQUEUE_ALWAYS_ASYNC);

    /*
     * No data is immediately available.
     * Must schedule asynchronous operation to the ioqueue.
     */
    read_op->op = PJ_IOQUEUE_OP_RECV;
    read_op->buf = buffer;
    read_op->size = *length;
    read_op->flags = flags;

    pj_ioqueue_lock_key(key);
    /* Check again. Handle may have been closed after the previous check
     * in multithreaded app. If we add bad handle to the set it will
     * corrupt the ioqueue set. See #913
     */
    if (IS_CLOSING(key)) {
	pj_ioqueue_unlock_key(key);
	return PJ_ECANCELLED;
    }
    pj_list_insert_before(&key->read_list, read_op);
    ioqueue_add_to_set(key->ioqueue, key, READABLE_EVENT);
    pj_ioqueue_unlock_key(key);

    return PJ_EPENDING;
}

/*
 * pj_ioqueue_recvfrom()
 *
 * Start asynchronous recvfrom() from the socket.
 */
PJ_DEF(pj_status_t) pj_ioqueue_recvfrom( pj_ioqueue_key_t *key,
                                         pj_ioqueue_op_key_t *op_key,
				         void *buffer,
				         pj_ssize_t *length,
                                         unsigned flags,
				         pj_sockaddr_t *addr,
				         int *addrlen)
{
    struct read_operation *read_op;

    PJ_ASSERT_RETURN(key && op_key && buffer && length, PJ_EINVAL);
    PJ_CHECK_STACK();

    /* Check if key is closing. */
    if (IS_CLOSING(key))
	return PJ_ECANCELLED;

    read_op = (struct read_operation*)op_key;
    read_op->op = PJ_IOQUEUE_OP_NONE;

    /* Try to see if there's data immediately available. 
     */
    if ((flags & PJ_IOQUEUE_ALWAYS_ASYNC) == 0) {
	pj_status_t status;
	pj_ssize_t size;

	size = *length;
	status = pj_sock_recvfrom(key->fd, buffer, &size, flags,
				  addr, addrlen);
	if (status == PJ_SUCCESS) {
	    /* Yes! Data is available! */
	    *length = size;
	    return PJ_SUCCESS;
	} else {
	    /* If error is not EWOULDBLOCK (or EAGAIN on Linux), report
	     * the error to caller.
	     */
	    if (status != PJ_STATUS_FROM_OS(PJ_BLOCKING_ERROR_VAL))
		return status;
	}
    }

    flags &= ~(PJ_IOQUEUE_ALWAYS_ASYNC);

    /*
     * No data is immediately available.
     * Must schedule asynchronous operation to the ioqueue.
     */
    read_op->op = PJ_IOQUEUE_OP_RECV_FROM;
    read_op->buf = buffer;
    read_op->size = *length;
    read_op->flags = flags;
    read_op->rmt_addr = addr;
    read_op->rmt_addrlen = addrlen;

    pj_ioqueue_lock_key(key);
    /* Check again. Handle may have been closed after the previous check
     * in multithreaded app. If we add bad handle to the set it will
     * corrupt the ioqueue set. See #913
     */
    if (IS_CLOSING(key)) {
	pj_ioqueue_unlock_key(key);
	return PJ_ECANCELLED;
    }
    pj_list_insert_before(&key->read_list, read_op);
    ioqueue_add_to_set(key->ioqueue, key, READABLE_EVENT);
    pj_ioqueue_unlock_key(key);

    return PJ_EPENDING;
}

/*
 * pj_ioqueue_send()
 *
 * Start asynchronous send() to the descriptor.
 */
PJ_DEF(pj_status_t) pj_ioqueue_send( pj_ioqueue_key_t *key,
                                     pj_ioqueue_op_key_t *op_key,
			             const void *data,
			             pj_ssize_t *length,
                                     unsigned flags)
{
    struct write_operation *write_op;
    pj_status_t status;
    unsigned retry;
    pj_ssize_t sent;
    unsigned oflags = flags;
    pj_bool_t urgent = (flags&PJ_IOQUEUE_URGENT_DATA) ? PJ_TRUE : PJ_FALSE;
    flags &= ~(PJ_IOQUEUE_URGENT_DATA); //clear the PJ_IOQUEUE_URGENT_DATA

    PJ_ASSERT_RETURN(key && op_key && data && length, PJ_EINVAL);
    PJ_CHECK_STACK();

    /* Check if key is closing. */
    if (IS_CLOSING(key))
	return PJ_ECANCELLED;

    /* We can not use PJ_IOQUEUE_ALWAYS_ASYNC for socket write. */
    flags &= ~(PJ_IOQUEUE_ALWAYS_ASYNC);

    /* Fast track:
     *   Try to send data immediately, only if there's no pending write!
     * Note:
     *  We are speculating that the list is empty here without properly
     *  acquiring ioqueue's mutex first. This is intentional, to maximize
     *  performance via parallelism.
     *
     *  This should be safe, because:
     *      - by convention, we require caller to make sure that the
     *        key is not unregistered while other threads are invoking
     *        an operation on the same key.
     *      - pj_list_empty() is safe to be invoked by multiple threads,
     *        even when other threads are modifying the list.
     */
    if (pj_list_empty(&key->write_list)) {
        /*
         * See if data can be sent immediately.
         */
        sent = *length;
        status = pj_sock_send(key->fd, data, &sent, flags);
        PJ_LOG(5, (THIS_FILE, "pj_ioqueue_send pj_sock_send=[%d].", status));
        if (status == PJ_SUCCESS) {
            /* Success! */
            *length = sent;
			if (urgent) {
                //free(op_key);
				pj_mem_free(op_key, sizeof(pj_ioqueue_op_key_t));
			}
            return PJ_SUCCESS;
        } else {
            /* If error is not EWOULDBLOCK (or EAGAIN on Linux), report
             * the error to caller.
             */
            if (status != PJ_STATUS_FROM_OS(PJ_BLOCKING_ERROR_VAL)) {
                return status;
            }
        }
    }

    /*
     * Schedule asynchronous send.
     */
    write_op = (struct write_operation*)op_key;

    /* Spin if write_op has pending operation */
    for (retry=0; write_op->op != 0 && retry<PENDING_RETRY; ++retry)
	pj_thread_sleep(0);

    /* Last chance */
    if (write_op->op) {
	/* Unable to send packet because there is already pending write in the
	 * write_op. We could not put the operation into the write_op
	 * because write_op already contains a pending operation! And
	 * we could not send the packet directly with send() either,
	 * because that will break the order of the packet. So we can
	 * only return error here.
	 *
	 * This could happen for example in multithreads program,
	 * where polling is done by one thread, while other threads are doing
	 * the sending only. If the polling thread runs on lower priority
	 * than the sending thread, then it's possible that the pending
	 * write flag is not cleared in-time because clearing is only done
	 * during polling. 
	 *
	 * Aplication should specify multiple write operation keys on
	 * situation like this.
	 */
	//pj_assert(!"ioqueue: there is pending operation on this key!");
	return PJ_EBUSY;
    }

    write_op->op = PJ_IOQUEUE_OP_SEND;
#if 0
	write_op->buf = (char*)data;
#else
	//if (write_op->buf)
	//{
	//	free(write_op->buf);
	//	write_op->buf = NULL;
	//}
	//write_op->buf = malloc(*length);
	write_op->buf = pj_mem_alloc(*length);
	pj_memcpy(write_op->buf, data, *length);
#endif
    write_op->size = *length;
    write_op->written = 0;
    write_op->flags = oflags;

    pj_ioqueue_lock_key(key);
    /* Check again. Handle may have been closed after the previous check
     * in multithreaded app. If we add bad handle to the set it will
     * corrupt the ioqueue set. See #913
     */
    if (IS_CLOSING(key)) {
	pj_ioqueue_unlock_key(key);
	return PJ_ECANCELLED;
    }
    pj_list_insert_before(&key->write_list, write_op);
    ioqueue_add_to_set(key->ioqueue, key, WRITEABLE_EVENT);
    pj_ioqueue_unlock_key(key);

    return PJ_EPENDING;
}


/*
 * pj_ioqueue_sendto()
 *
 * Start asynchronous write() to the descriptor.
 */
PJ_DEF(pj_status_t) pj_ioqueue_sendto( pj_ioqueue_key_t *key,
                                       pj_ioqueue_op_key_t *op_key,
			               const void *data,
			               pj_ssize_t *length,
                                       pj_uint32_t flags,
			               const pj_sockaddr_t *addr,
			               int addrlen)
{
    struct write_operation *write_op;
    unsigned retry;
#if defined(PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
	    PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0
    pj_bool_t restart_retry = PJ_FALSE;
#endif
    pj_status_t status;
    pj_ssize_t sent;
	unsigned oflags = flags;
	pj_bool_t urgent = (flags&PJ_IOQUEUE_URGENT_DATA) ? PJ_TRUE : PJ_FALSE;
	flags &= ~(PJ_IOQUEUE_URGENT_DATA); //clear the PJ_IOQUEUE_URGENT_DATA

    PJ_ASSERT_RETURN(key && op_key && data && length, PJ_EINVAL);
    PJ_CHECK_STACK();

#if defined(PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
	    PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0
retry_on_restart:
#endif
    /* Check if key is closing. */
    if (IS_CLOSING(key))
	return PJ_ECANCELLED;

    /* We can not use PJ_IOQUEUE_ALWAYS_ASYNC for socket write */
    flags &= ~(PJ_IOQUEUE_ALWAYS_ASYNC);

    /* Fast track:
     *   Try to send data immediately, only if there's no pending write!
     * Note:
     *  We are speculating that the list is empty here without properly
     *  acquiring ioqueue's mutex first. This is intentional, to maximize
     *  performance via parallelism.
     *
     *  This should be safe, because:
     *      - by convention, we require caller to make sure that the
     *        key is not unregistered while other threads are invoking
     *        an operation on the same key.
     *      - pj_list_empty() is safe to be invoked by multiple threads,
     *        even when other threads are modifying the list.
     */
    if (pj_list_empty(&key->write_list)) {
        /*
         * See if data can be sent immediately.
         */
        sent = *length;
        status = pj_sock_sendto(key->fd, data, &sent, flags, addr, addrlen);
        if (status == PJ_SUCCESS) {
            /* Success! */
			*length = sent;
			if (urgent) {
				//free(op_key);
				pj_mem_free(op_key, sizeof(pj_ioqueue_op_key_t));
			}
            return PJ_SUCCESS;
        } else {
            /* If error is not EWOULDBLOCK (or EAGAIN on Linux), report
             * the error to caller.
             */
            if (status != PJ_STATUS_FROM_OS(PJ_BLOCKING_ERROR_VAL)) {
#if defined(PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
	    PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0
		/* Special treatment for dead UDP sockets here, see ticket #1107 */
		if (status==PJ_STATUS_FROM_OS(EPIPE) && !IS_CLOSING(key) &&
		    key->fd_type==pj_SOCK_DGRAM() && !restart_retry)
		{
		    PJ_PERROR(4,(THIS_FILE, status,
				 "Send error for socket %d, retrying",
				 key->fd));
		    replace_udp_sock(key);
		    restart_retry = PJ_TRUE;
		    goto retry_on_restart;
		}
#endif

                return status;
            }
        }
    }

    /*
     * Check that address storage can hold the address parameter.
     */
    PJ_ASSERT_RETURN(addrlen <= (int)sizeof(pj_sockaddr_in), PJ_EBUG);

    /*
     * Schedule asynchronous send.
     */
    write_op = (struct write_operation*)op_key;
    
    /* Spin if write_op has pending operation */
    for (retry=0; write_op->op != 0 && retry<PENDING_RETRY; ++retry)
	pj_thread_sleep(0);

    /* Last chance */
    if (write_op->op) {
	/* Unable to send packet because there is already pending write on the
	 * write_op. We could not put the operation into the write_op
	 * because write_op already contains a pending operation! And
	 * we could not send the packet directly with sendto() either,
	 * because that will break the order of the packet. So we can
	 * only return error here.
	 *
	 * This could happen for example in multithreads program,
	 * where polling is done by one thread, while other threads are doing
	 * the sending only. If the polling thread runs on lower priority
	 * than the sending thread, then it's possible that the pending
	 * write flag is not cleared in-time because clearing is only done
	 * during polling. 
	 *
	 * Aplication should specify multiple write operation keys on
	 * situation like this.
	 */
	//pj_assert(!"ioqueue: there is pending operation on this key!");
	return PJ_EBUSY;
    }

	write_op->op = PJ_IOQUEUE_OP_SEND_TO;
#if 0
	write_op->buf = (char*)data;
#else
	//if (write_op->buf)
	//{
	//	free(write_op->buf);
	//	write_op->buf = NULL;
	//}
	//write_op->buf = malloc(*length);
	write_op->buf = pj_mem_alloc(*length);
	pj_memcpy(write_op->buf, data, *length);
#endif
    write_op->size = *length;
	write_op->written = 0;
	write_op->flags = oflags;
    pj_memcpy(&write_op->rmt_addr, addr, addrlen);
    write_op->rmt_addrlen = addrlen;
    
    pj_ioqueue_lock_key(key);
    /* Check again. Handle may have been closed after the previous check
     * in multithreaded app. If we add bad handle to the set it will
     * corrupt the ioqueue set. See #913
     */
    if (IS_CLOSING(key)) {
	pj_ioqueue_unlock_key(key);
	return PJ_ECANCELLED;
    }
    pj_list_insert_before(&key->write_list, write_op);
    ioqueue_add_to_set(key->ioqueue, key, WRITEABLE_EVENT);
    pj_ioqueue_unlock_key(key);

    return PJ_EPENDING;
}

#if PJ_HAS_TCP
/*
 * Initiate overlapped accept() operation.
 */
PJ_DEF(pj_status_t) pj_ioqueue_accept( pj_ioqueue_key_t *key,
                                       pj_ioqueue_op_key_t *op_key,
			               pj_sock_t *new_sock,
			               pj_sockaddr_t *local,
			               pj_sockaddr_t *remote,
			               int *addrlen)
{
    struct accept_operation *accept_op;
    pj_status_t status;

    /* check parameters. All must be specified! */
    PJ_ASSERT_RETURN(key && op_key && new_sock, PJ_EINVAL);

    /* Check if key is closing. */
    if (IS_CLOSING(key))
	return PJ_ECANCELLED;

    accept_op = (struct accept_operation*)op_key;
    accept_op->op = PJ_IOQUEUE_OP_NONE;

    /* Fast track:
     *  See if there's new connection available immediately.
     */
    if (pj_list_empty(&key->accept_list)) {
        status = pj_sock_accept(key->fd, new_sock, remote, addrlen);
        if (status == PJ_SUCCESS) {
            /* Yes! New connection is available! */
            if (local && addrlen) {
                status = pj_sock_getsockname(*new_sock, local, addrlen);
                if (status != PJ_SUCCESS) {
                    pj_sock_close(*new_sock);
                    *new_sock = PJ_INVALID_SOCKET;
                    return status;
                }
            }
            return PJ_SUCCESS;
        } else {
            /* If error is not EWOULDBLOCK (or EAGAIN on Linux), report
             * the error to caller.
             */
            if (status != PJ_STATUS_FROM_OS(PJ_BLOCKING_ERROR_VAL)) {
                return status;
            }
        }
    }

    /*
     * No connection is available immediately.
     * Schedule accept() operation to be completed when there is incoming
     * connection available.
     */
    accept_op->op = PJ_IOQUEUE_OP_ACCEPT;
    accept_op->accept_fd = new_sock;
    accept_op->rmt_addr = remote;
    accept_op->addrlen= addrlen;
    accept_op->local_addr = local;

    pj_ioqueue_lock_key(key);
    /* Check again. Handle may have been closed after the previous check
     * in multithreaded app. If we add bad handle to the set it will
     * corrupt the ioqueue set. See #913
     */
    if (IS_CLOSING(key)) {
	pj_ioqueue_unlock_key(key);
	return PJ_ECANCELLED;
    }
    pj_list_insert_before(&key->accept_list, accept_op);
    ioqueue_add_to_set(key->ioqueue, key, READABLE_EVENT);
    pj_ioqueue_unlock_key(key);

    return PJ_EPENDING;
}

/*
 * Initiate overlapped connect() operation (well, it's non-blocking actually,
 * since there's no overlapped version of connect()).
 */
PJ_DEF(pj_status_t) pj_ioqueue_connect( pj_ioqueue_key_t *key,
					const pj_sockaddr_t *addr,
					int addrlen )
{
    pj_status_t status;
    
    /* check parameters. All must be specified! */
    PJ_ASSERT_RETURN(key && addr && addrlen, PJ_EINVAL);

    /* Check if key is closing. */
    if (IS_CLOSING(key))
	return PJ_ECANCELLED;

    /* Check if socket has not been marked for connecting */
    if (key->connecting != 0)
        return PJ_EPENDING;
    
    status = pj_sock_connect(key->fd, addr, addrlen);
    if (status == PJ_SUCCESS) {
	/* Connected! */
	return PJ_SUCCESS;
    } else {
	if (status == PJ_STATUS_FROM_OS(PJ_BLOCKING_CONNECT_ERROR_VAL)) {
	    /* Pending! */
	    pj_ioqueue_lock_key(key);
	    /* Check again. Handle may have been closed after the previous 
	     * check in multithreaded app. See #913
	     */
	    if (IS_CLOSING(key)) {
		pj_ioqueue_unlock_key(key);
		return PJ_ECANCELLED;
	    }
	    key->connecting = PJ_TRUE;
            ioqueue_add_to_set(key->ioqueue, key, WRITEABLE_EVENT);
            ioqueue_add_to_set(key->ioqueue, key, EXCEPTION_EVENT);
            pj_ioqueue_unlock_key(key);
	    return PJ_EPENDING;
	} else {
	    /* Error! */
	    return status;
	}
    }
}
#endif	/* PJ_HAS_TCP */


PJ_DEF(void) pj_ioqueue_op_key_init( pj_ioqueue_op_key_t *op_key,
				     pj_size_t size )
{
    pj_bzero(op_key, size);
}


/*
 * pj_ioqueue_is_pending()
 */
PJ_DEF(pj_bool_t) pj_ioqueue_is_pending( pj_ioqueue_key_t *key,
                                         pj_ioqueue_op_key_t *op_key )
{
    struct generic_operation *op_rec;

    PJ_UNUSED_ARG(key);

    op_rec = (struct generic_operation*)op_key;
    return op_rec->op != 0;
}


/*
 * pj_ioqueue_post_completion()
 */
PJ_DEF(pj_status_t) pj_ioqueue_post_completion( pj_ioqueue_key_t *key,
                                                pj_ioqueue_op_key_t *op_key,
                                                pj_ssize_t bytes_status )
{
    struct generic_operation *op_rec;

    /*
     * Find the operation key in all pending operation list to
     * really make sure that it's still there; then call the callback.
     */
    pj_ioqueue_lock_key(key);

    /* Find the operation in the pending read list. */
    op_rec = (struct generic_operation*)key->read_list.next;
    while (op_rec != (void*)&key->read_list) {
        if (op_rec == (void*)op_key) {
            pj_list_erase(op_rec);
            op_rec->op = PJ_IOQUEUE_OP_NONE;
            pj_ioqueue_unlock_key(key);

            (*key->cb.on_read_complete)(key, op_key, bytes_status);
            return PJ_SUCCESS;
        }
        op_rec = op_rec->next;
    }

    /* Find the operation in the pending write list. */
    op_rec = (struct generic_operation*)key->write_list.next;
    while (op_rec != (void*)&key->write_list) {
        if (op_rec == (void*)op_key) {
            pj_list_erase(op_rec);
            op_rec->op = PJ_IOQUEUE_OP_NONE;
            pj_ioqueue_unlock_key(key);

            (*key->cb.on_write_complete)(key, op_key, bytes_status);
            return PJ_SUCCESS;
        }
        op_rec = op_rec->next;
    }

    /* Find the operation in the pending accept list. */
    op_rec = (struct generic_operation*)key->accept_list.next;
    while (op_rec != (void*)&key->accept_list) {
        if (op_rec == (void*)op_key) {
            pj_list_erase(op_rec);
            op_rec->op = PJ_IOQUEUE_OP_NONE;
            pj_ioqueue_unlock_key(key);

            (*key->cb.on_accept_complete)(key, op_key, 
                                          PJ_INVALID_SOCKET,
                                          (pj_status_t)bytes_status);
            return PJ_SUCCESS;
        }
        op_rec = op_rec->next;
    }

    pj_ioqueue_unlock_key(key);
    
    return PJ_EINVALIDOP;
}

PJ_DEF(pj_status_t) pj_ioqueue_set_default_concurrency( pj_ioqueue_t *ioqueue,
							pj_bool_t allow)
{
    PJ_ASSERT_RETURN(ioqueue != NULL, PJ_EINVAL);
    ioqueue->default_concurrency = allow;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_ioqueue_set_concurrency(pj_ioqueue_key_t *key,
					       pj_bool_t allow)
{
    PJ_ASSERT_RETURN(key, PJ_EINVAL);

    /* PJ_IOQUEUE_HAS_SAFE_UNREG must be enabled if concurrency is
     * disabled.
     */
    PJ_ASSERT_RETURN(allow || PJ_IOQUEUE_HAS_SAFE_UNREG, PJ_EINVAL);

    key->allow_concurrent = allow;
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_ioqueue_lock_key(pj_ioqueue_key_t *key)
{
    if (key->grp_lock)
	return pj_grp_lock_acquire(key->grp_lock);
    else
	return pj_lock_acquire(key->lock);
}

PJ_DEF(pj_status_t) pj_ioqueue_unlock_key(pj_ioqueue_key_t *key)
{
	if (key->grp_lock)
		return pj_grp_lock_release(key->grp_lock);
	else
		return pj_lock_release(key->lock);
}