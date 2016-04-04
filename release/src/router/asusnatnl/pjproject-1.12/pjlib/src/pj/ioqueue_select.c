/* $Id: ioqueue_select.c 3553 2011-05-05 06:14:19Z nanang $ */
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
 * sock_select.c
 *
 * This is the implementation of IOQueue using pj_sock_select().
 * It runs anywhere where pj_sock_select() is available (currently
 * Win32, Linux, Linux kernel, etc.).
 */

#include <pj/ioqueue.h>
#include <pj/os.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/list.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/assert.h>
#include <pj/sock.h>
#include <pj/compat/socket.h>
#include <pj/sock_select.h>
#include <pj/sock_qos.h>
#include <pj/errno.h>
#include <pj/rand.h>

/* Now that we have access to OS'es <sys/select>, lets check again that
 * PJ_IOQUEUE_MAX_HANDLES is not greater than FD_SETSIZE
 */
#if PJ_IOQUEUE_MAX_HANDLES > FD_SETSIZE
#   error "PJ_IOQUEUE_MAX_HANDLES cannot be greater than FD_SETSIZE"
#endif


/*
 * Include declaration from common abstraction.
 */
#include "ioqueue_common_abs.h"

/*
 * ISSUES with ioqueue_select()
 *
 * EAGAIN/EWOULDBLOCK error in recv():
 *  - when multiple threads are working with the ioqueue, application
 *    may receive EAGAIN or EWOULDBLOCK in the receive callback.
 *    This error happens because more than one thread is watching for
 *    the same descriptor set, so when all of them call recv() or recvfrom()
 *    simultaneously, only one will succeed and the rest will get the error.
 *
 */
#define THIS_FILE   "ioq_select"

/*
 * The select ioqueue relies on socket functions (pj_sock_xxx()) to return
 * the correct error code.
 */
#if PJ_RETURN_OS_ERROR(100) != PJ_STATUS_FROM_OS(100)
#   error "Error reporting must be enabled for this function to work!"
#endif

/*
 * During debugging build, VALIDATE_FD_SET is set.
 * This will check the validity of the fd_sets.
 */
/*
#if defined(PJ_DEBUG) && PJ_DEBUG != 0
#  define VALIDATE_FD_SET		1
#else
#  define VALIDATE_FD_SET		0
#endif
*/
#define VALIDATE_FD_SET     0

#if 0
#  define TRACE__(args)	PJ_LOG(3,args)
#else
#  define TRACE__(args)
#endif

/*
 * This describes each key.
 */
struct pj_ioqueue_key_t
{
    DECLARE_COMMON_KEY
};

/*
 * This describes the I/O queue itself.
 */
struct pj_ioqueue_t
{
    DECLARE_COMMON_IOQUEUE

    unsigned		max, count;	/* Max and current key count	    */
    int			nfds;		/* The largest fd value (for select)*/
    pj_ioqueue_key_t	active_list;	/* List of active keys.		    */
    pj_fd_set_t		rfdset;
    pj_fd_set_t		wfdset;
#if PJ_HAS_TCP
    pj_fd_set_t		xfdset;
#endif

#if PJ_IOQUEUE_HAS_SAFE_UNREG
    pj_mutex_t	       *ref_cnt_mutex;
    pj_ioqueue_key_t	closing_list;
    pj_ioqueue_key_t	free_list;
#endif
};

/* Proto */
#if defined(PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
	    PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0
static pj_status_t replace_udp_sock(pj_ioqueue_key_t *h);
#endif

/* Include implementation for common abstraction after we declare
 * pj_ioqueue_key_t and pj_ioqueue_t.
 */
#include "ioqueue_common_abs.c"

#if PJ_IOQUEUE_HAS_SAFE_UNREG
/* Scan closing keys to be put to free list again */
static void scan_closing_keys(pj_ioqueue_t *ioqueue);
#endif

/*
 * pj_ioqueue_name()
 */
PJ_DEF(const char*) pj_ioqueue_name(void)
{
    return "select";
}

/* 
 * Scan the socket descriptor sets for the largest descriptor.
 * This value is needed by select().
 */
#if defined(PJ_SELECT_NEEDS_NFDS) && PJ_SELECT_NEEDS_NFDS!=0
static void rescan_fdset(pj_ioqueue_t *ioqueue)
{
    pj_ioqueue_key_t *key = ioqueue->active_list.next;
    int max = 0;

    while (key != &ioqueue->active_list) {
	if (key->fd > max)
	    max = key->fd;
	key = key->next;
    }

    ioqueue->nfds = max;
}
#else
static void rescan_fdset(pj_ioqueue_t *ioqueue)
{
    ioqueue->nfds = FD_SETSIZE-1;
}
#endif


/*
 * pj_ioqueue_create()
 *
 * Create select ioqueue.
 */
PJ_DEF(pj_status_t) pj_ioqueue_create( pj_pool_t *pool, 
                                       pj_size_t max_fd,
                                       pj_ioqueue_t **p_ioqueue)
{
    pj_ioqueue_t *ioqueue;
    pj_lock_t *lock;
    unsigned i;
    pj_status_t rc;

    /* Check that arguments are valid. */
    PJ_ASSERT_RETURN(pool != NULL && p_ioqueue != NULL && 
                     max_fd > 0 && max_fd <= PJ_IOQUEUE_MAX_HANDLES, 
                     PJ_EINVAL);

    /* Check that size of pj_ioqueue_op_key_t is sufficient */
    PJ_ASSERT_RETURN(sizeof(pj_ioqueue_op_key_t)-sizeof(void*) >=
                     sizeof(union operation_key), PJ_EBUG);

    /* Create and init common ioqueue stuffs */
    ioqueue = PJ_POOL_ALLOC_T(pool, pj_ioqueue_t);
    ioqueue_init(ioqueue);

    ioqueue->max = (unsigned)max_fd;
    ioqueue->count = 0;
    PJ_FD_ZERO(&ioqueue->rfdset);
    PJ_FD_ZERO(&ioqueue->wfdset);
#if PJ_HAS_TCP
    PJ_FD_ZERO(&ioqueue->xfdset);
#endif
    pj_list_init(&ioqueue->active_list);

    rescan_fdset(ioqueue);

#if PJ_IOQUEUE_HAS_SAFE_UNREG
    /* When safe unregistration is used (the default), we pre-create
     * all keys and put them in the free list.
     */

    /* Mutex to protect key's reference counter 
     * We don't want to use key's mutex or ioqueue's mutex because
     * that would create deadlock situation in some cases.
     */
    rc = pj_mutex_create_simple(pool, NULL, &ioqueue->ref_cnt_mutex);
    if (rc != PJ_SUCCESS)
	return rc;


    /* Init key list */
    pj_list_init(&ioqueue->free_list);
    pj_list_init(&ioqueue->closing_list);


    /* Pre-create all keys according to max_fd */
    for (i=0; i<max_fd; ++i) {
	pj_ioqueue_key_t *key;

	key = PJ_POOL_ALLOC_T(pool, pj_ioqueue_key_t);
	key->ref_count = 0;
	rc = pj_lock_create_recursive_mutex(pool, NULL, &key->lock);
	if (rc != PJ_SUCCESS) {
	    key = ioqueue->free_list.next;
	    while (key != &ioqueue->free_list) {
		pj_lock_destroy(key->lock);
		key = key->next;
	    }
	    pj_mutex_destroy(ioqueue->ref_cnt_mutex);
	    return rc;
	}

	pj_list_push_back(&ioqueue->free_list, key);
    }
#endif

    /* Create and init ioqueue mutex */
    rc = pj_lock_create_simple_mutex(pool, "ioq%p", &lock);
    if (rc != PJ_SUCCESS)
	return rc;

    rc = pj_ioqueue_set_lock(ioqueue, lock, PJ_TRUE);
    if (rc != PJ_SUCCESS)
        return rc;

    PJ_LOG(4, ("pjlib", "select() I/O Queue created (%p)", ioqueue));

    *p_ioqueue = ioqueue;
    return PJ_SUCCESS;
}

/*
 * pj_ioqueue_destroy()
 *
 * Destroy ioqueue.
 */
PJ_DEF(pj_status_t) pj_ioqueue_destroy(pj_ioqueue_t *ioqueue)
{
    pj_ioqueue_key_t *key;

    PJ_ASSERT_RETURN(ioqueue, PJ_EINVAL);

    pj_lock_acquire(ioqueue->lock);

#if PJ_IOQUEUE_HAS_SAFE_UNREG
    /* Destroy reference counters */
    key = ioqueue->active_list.next;
    while (key != &ioqueue->active_list) {
	pj_lock_destroy(key->lock);
	key = key->next;
    }

    key = ioqueue->closing_list.next;
    while (key != &ioqueue->closing_list) {
	pj_lock_destroy(key->lock);
	key = key->next;
    }

    key = ioqueue->free_list.next;
    while (key != &ioqueue->free_list) {
	pj_lock_destroy(key->lock);
	key = key->next;
    }

    pj_mutex_destroy(ioqueue->ref_cnt_mutex);
#endif

    return ioqueue_destroy(ioqueue);
}


/*
 * pj_ioqueue_register_sock()
 *
 * Register socket handle to ioqueue.
 */
PJ_DEF(pj_status_t) pj_ioqueue_register_sock2(pj_pool_t *pool,
					      pj_ioqueue_t *ioqueue,
					      pj_sock_t sock,
					      pj_grp_lock_t *grp_lock,
					      void *user_data,
					      const pj_ioqueue_callback *cb,
                                              pj_ioqueue_key_t **p_key)
{
    pj_ioqueue_key_t *key = NULL;
#if defined(PJ_WIN32) && PJ_WIN32!=0 || \
    defined(PJ_WIN64) && PJ_WIN64 != 0 || \
    defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE!=0
    u_long value;
#else
    pj_uint32_t value;
#endif
    pj_status_t rc = PJ_SUCCESS;
    
    PJ_ASSERT_RETURN(pool && ioqueue && sock != PJ_INVALID_SOCKET &&
                     cb && p_key, PJ_EINVAL);

    pj_lock_acquire(ioqueue->lock);

	if (ioqueue->count >= ioqueue->max) {
		PJ_LOG(3, ("pjlib", "pj_ioqueue_register_sock() PJ_ETOOMANY ioqueue->count=%d, ioqueue->max=%d", ioqueue->count, ioqueue->max));
        rc = PJ_ETOOMANY;
	goto on_return;
    }

    /* If safe unregistration (PJ_IOQUEUE_HAS_SAFE_UNREG) is used, get
     * the key from the free list. Otherwise allocate a new one. 
     */
#if PJ_IOQUEUE_HAS_SAFE_UNREG

    /* Scan closing_keys first to let them come back to free_list */
    scan_closing_keys(ioqueue);

    pj_assert(!pj_list_empty(&ioqueue->free_list));
    if (pj_list_empty(&ioqueue->free_list)) {
	rc = PJ_ETOOMANY;
	goto on_return;
    }

    key = ioqueue->free_list.next;
    pj_list_erase(key);
#else
    key = (pj_ioqueue_key_t*)pj_pool_zalloc(pool, sizeof(pj_ioqueue_key_t));
#endif

    rc = ioqueue_init_key(pool, ioqueue, key, sock, grp_lock, user_data, cb);
    if (rc != PJ_SUCCESS) {
	key = NULL;
	goto on_return;
    }

    /* Set socket to nonblocking. */
    value = 1;
#if defined(PJ_WIN32) && PJ_WIN32!=0 || \
    defined(PJ_WIN64) && PJ_WIN64 != 0 || \
    defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE!=0
    if (ioctlsocket(sock, FIONBIO, &value)) {
#else
    if (ioctl(sock, FIONBIO, &value)) {
#endif
        rc = pj_get_netos_error();
	goto on_return;
    }


    /* Put in active list. */
    pj_list_insert_before(&ioqueue->active_list, key);
	++ioqueue->count;
	PJ_LOG(3, ("pjlib", "pj_ioqueue_register_sock() ioqueue->count=%d, key=%p", ioqueue->count, key));

    /* Rescan fdset to get max descriptor */
    rescan_fdset(ioqueue);

on_return:
    /* On error, socket may be left in non-blocking mode. */
    if (rc != PJ_SUCCESS) {
	if (key && key->grp_lock)
	    pj_grp_lock_dec_ref_dbg(key->grp_lock, "ioqueue", 0);
    }
    *p_key = key;
    pj_lock_release(ioqueue->lock);
    
    return rc;
}

PJ_DEF(pj_status_t) pj_ioqueue_register_sock( pj_pool_t *pool,
					      pj_ioqueue_t *ioqueue,
					      pj_sock_t sock,
					      void *user_data,
					      const pj_ioqueue_callback *cb,
					      pj_ioqueue_key_t **p_key)
{
    return pj_ioqueue_register_sock2(pool, ioqueue, sock, NULL, user_data,
                                     cb, p_key);
}

#if PJ_IOQUEUE_HAS_SAFE_UNREG
/* Increment key's reference counter */
static void increment_counter(pj_ioqueue_key_t *key)
{
    pj_mutex_lock(key->ioqueue->ref_cnt_mutex);
    ++key->ref_count;
    pj_mutex_unlock(key->ioqueue->ref_cnt_mutex);
}

/* Decrement the key's reference counter, and when the counter reach zero,
 * destroy the key.
 *
 * Note: MUST NOT CALL THIS FUNCTION WHILE HOLDING ioqueue's LOCK.
 */
static void decrement_counter(pj_ioqueue_key_t *key)
{
    pj_lock_acquire(key->ioqueue->lock);
    pj_mutex_lock(key->ioqueue->ref_cnt_mutex);
    --key->ref_count;
    if (key->ref_count == 0) {

	pj_assert(key->closing == 1);
	pj_gettickcount(&key->free_time);
	key->free_time.msec += PJ_IOQUEUE_KEY_FREE_DELAY;
	pj_time_val_normalize(&key->free_time);

	pj_list_erase(key);
	pj_list_push_back(&key->ioqueue->closing_list, key);
	/* Rescan fdset to get max descriptor */
	rescan_fdset(key->ioqueue);
    }
    pj_mutex_unlock(key->ioqueue->ref_cnt_mutex);
    pj_lock_release(key->ioqueue->lock);
}
#endif


/*
 * pj_ioqueue_unregister()
 *
 * Unregister handle from ioqueue.
 */
PJ_DEF(pj_status_t) pj_ioqueue_unregister( pj_ioqueue_key_t *key)
{
    pj_ioqueue_t *ioqueue;

    PJ_ASSERT_RETURN(key, PJ_EINVAL);

    ioqueue = key->ioqueue;

    /* Lock the key to make sure no callback is simultaneously modifying
     * the key. We need to lock the key before ioqueue here to prevent
     * deadlock.
     */
    pj_ioqueue_lock_key(key);

    /* Also lock ioqueue */
    pj_lock_acquire(ioqueue->lock);

    pj_assert(ioqueue->count > 0);
	--ioqueue->count;
	PJ_LOG(3, ("pjlib", "pj_ioqueue_unregister() ioqueue->count=%d, key=%p", ioqueue->count, key));
#if !PJ_IOQUEUE_HAS_SAFE_UNREG
    /* Ticket #520, key will be erased more than once */
    pj_list_erase(key);
#endif
    PJ_FD_CLR(key->fd, &ioqueue->rfdset);
    PJ_FD_CLR(key->fd, &ioqueue->wfdset);
#if PJ_HAS_TCP
    PJ_FD_CLR(key->fd, &ioqueue->xfdset);
#endif

    /* Close socket. */
    pj_sock_close(key->fd);

    /* Clear callback */
    key->cb.on_accept_complete = NULL;
    key->cb.on_connect_complete = NULL;
    key->cb.on_read_complete = NULL;
    key->cb.on_write_complete = NULL;

    /* Must release ioqueue lock first before decrementing counter, to
     * prevent deadlock.
     */
    pj_lock_release(ioqueue->lock);

#if PJ_IOQUEUE_HAS_SAFE_UNREG
    /* Mark key is closing. */
    key->closing = 1;

    /* Decrement counter. */
    decrement_counter(key);

    /* Done. */
    if (key->grp_lock) {
	/* just dec_ref and unlock. we will set grp_lock to NULL
	 * elsewhere */
	pj_grp_lock_t *grp_lock = key->grp_lock;
	// Don't set grp_lock to NULL otherwise the other thread
	// will crash. Just leave it as dangling pointer, but this
	// should be safe
	//key->grp_lock = NULL;
	pj_grp_lock_dec_ref_dbg(grp_lock, "ioqueue", 0);
	pj_grp_lock_release(grp_lock);
    } else {
	pj_ioqueue_unlock_key(key);
    }
#else
    if (key->grp_lock) {
	/* set grp_lock to NULL and unlock */
	pj_grp_lock_t *grp_lock = key->grp_lock;
	// Don't set grp_lock to NULL otherwise the other thread
	// will crash. Just leave it as dangling pointer, but this
	// should be safe
	//key->grp_lock = NULL;
	pj_grp_lock_dec_ref_dbg(grp_lock, "ioqueue", 0);
	pj_grp_lock_release(grp_lock);
    } else {
	pj_ioqueue_unlock_key(key);
    }

    pj_lock_destroy(key->lock);
#endif

    return PJ_SUCCESS;
}


/* This supposed to check whether the fd_set values are consistent
 * with the operation currently set in each key.
 */
#if VALIDATE_FD_SET
static void validate_sets(const pj_ioqueue_t *ioqueue,
			  const pj_fd_set_t *rfdset,
			  const pj_fd_set_t *wfdset,
			  const pj_fd_set_t *xfdset)
{
    pj_ioqueue_key_t *key;

    /*
     * This basicly would not work anymore.
     * We need to lock key before performing the check, but we can't do
     * so because we're holding ioqueue mutex. If we acquire key's mutex
     * now, the will cause deadlock.
     */
    pj_assert(0);

    key = ioqueue->active_list.next;
    while (key != &ioqueue->active_list) {
	if (!pj_list_empty(&key->read_list)
#if defined(PJ_HAS_TCP) && PJ_HAS_TCP != 0
	    || !pj_list_empty(&key->accept_list)
#endif
	    ) 
	{
	    pj_assert(PJ_FD_ISSET(key->fd, rfdset));
	} 
	else {
	    pj_assert(PJ_FD_ISSET(key->fd, rfdset) == 0);
	}
	if (!pj_list_empty(&key->write_list)
#if defined(PJ_HAS_TCP) && PJ_HAS_TCP != 0
	    || key->connecting
#endif
	   )
	{
	    pj_assert(PJ_FD_ISSET(key->fd, wfdset));
	}
	else {
	    pj_assert(PJ_FD_ISSET(key->fd, wfdset) == 0);
	}
#if defined(PJ_HAS_TCP) && PJ_HAS_TCP != 0
	if (key->connecting)
	{
	    pj_assert(PJ_FD_ISSET(key->fd, xfdset));
	}
	else {
	    pj_assert(PJ_FD_ISSET(key->fd, xfdset) == 0);
	}
#endif /* PJ_HAS_TCP */

	key = key->next;
    }
}
#endif	/* VALIDATE_FD_SET */


/* ioqueue_remove_from_set()
 * This function is called from ioqueue_dispatch_event() to instruct
 * the ioqueue to remove the specified descriptor from ioqueue's descriptor
 * set for the specified event.
 */
static void ioqueue_remove_from_set( pj_ioqueue_t *ioqueue,
                                     pj_ioqueue_key_t *key, 
                                     enum ioqueue_event_type event_type)
{
    pj_lock_acquire(ioqueue->lock);

    if (event_type == READABLE_EVENT)
        PJ_FD_CLR((pj_sock_t)key->fd, &ioqueue->rfdset);
    else if (event_type == WRITEABLE_EVENT)
        PJ_FD_CLR((pj_sock_t)key->fd, &ioqueue->wfdset);
#if defined(PJ_HAS_TCP) && PJ_HAS_TCP!=0
    else if (event_type == EXCEPTION_EVENT)
        PJ_FD_CLR((pj_sock_t)key->fd, &ioqueue->xfdset);
#endif
    else
        pj_assert(0);

    pj_lock_release(ioqueue->lock);
}

/*
 * ioqueue_add_to_set()
 * This function is called from pj_ioqueue_recv(), pj_ioqueue_send() etc
 * to instruct the ioqueue to add the specified handle to ioqueue's descriptor
 * set for the specified event.
 */
static void ioqueue_add_to_set( pj_ioqueue_t *ioqueue,
                                pj_ioqueue_key_t *key,
                                enum ioqueue_event_type event_type )
{
    pj_lock_acquire(ioqueue->lock);

    if (event_type == READABLE_EVENT)
        PJ_FD_SET((pj_sock_t)key->fd, &ioqueue->rfdset);
    else if (event_type == WRITEABLE_EVENT)
        PJ_FD_SET((pj_sock_t)key->fd, &ioqueue->wfdset);
#if defined(PJ_HAS_TCP) && PJ_HAS_TCP!=0
    else if (event_type == EXCEPTION_EVENT)
        PJ_FD_SET((pj_sock_t)key->fd, &ioqueue->xfdset);
#endif
    else
        pj_assert(0);

    pj_lock_release(ioqueue->lock);
}

#if PJ_IOQUEUE_HAS_SAFE_UNREG
/* Scan closing keys to be put to free list again */
static void scan_closing_keys(pj_ioqueue_t *ioqueue)
{
    pj_time_val now;
    pj_ioqueue_key_t *h;

    pj_gettickcount(&now);
    h = ioqueue->closing_list.next;
    while (h != &ioqueue->closing_list) {
	pj_ioqueue_key_t *next = h->next;

	pj_assert(h->closing != 0);

	if (PJ_TIME_VAL_GTE(now, h->free_time)) {
	    pj_list_erase(h);
	    // Don't set grp_lock to NULL otherwise the other thread
	    // will crash. Just leave it as dangling pointer, but this
	    // should be safe
	    //h->grp_lock = NULL;
	    pj_list_push_back(&ioqueue->free_list, h);
	}
	h = next;
    }
}
#endif

#if defined(PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
    PJ_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0
static pj_status_t replace_udp_sock(pj_ioqueue_key_t *h)
{
    enum flags {
	HAS_PEER_ADDR = 1,
	HAS_QOS = 2
    };
    pj_sock_t old_sock, new_sock = PJ_INVALID_SOCKET;
    pj_sockaddr local_addr, rem_addr;
    int val, addr_len;
    pj_fd_set_t *fds[3];
    unsigned i, fds_cnt, flags=0;
    pj_qos_params qos_params;
    unsigned msec;
    pj_status_t status;

    pj_lock_acquire(h->ioqueue->lock);

    old_sock = h->fd;

    /* Can only replace UDP socket */
    pj_assert(h->fd_type == pj_SOCK_DGRAM());

    PJ_LOG(4,(THIS_FILE, "Attempting to replace UDP socket %d", old_sock));

    /* Investigate the old socket */
    addr_len = sizeof(local_addr);
    status = pj_sock_getsockname(old_sock, &local_addr, &addr_len);
    if (status != PJ_SUCCESS)
	goto on_error;
    
    addr_len = sizeof(rem_addr);
    status = pj_sock_getpeername(old_sock, &rem_addr, &addr_len);
    if (status == PJ_SUCCESS)
	flags |= HAS_PEER_ADDR;

    status = pj_sock_get_qos_params(old_sock, &qos_params);
    if (status == PJ_SUCCESS)
	flags |= HAS_QOS;

    /* We're done with the old socket, close it otherwise we'll get
     * error in bind()
     */
    pj_sock_close(old_sock);

    /* Prepare the new socket */
    status = pj_sock_socket(local_addr.addr.sa_family, PJ_SOCK_DGRAM, 0,
			    &new_sock);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Even after the socket is closed, we'll still get "Address in use"
     * errors, so force it with SO_REUSEADDR
     */
    val = 1;
    status = pj_sock_setsockopt(new_sock, SOL_SOCKET, SO_REUSEADDR,
				&val, sizeof(val));
    if (status != PJ_SUCCESS)
	goto on_error;

    /* The loop is silly, but what else can we do? */
    addr_len = pj_sockaddr_get_len(&local_addr);
    for (msec=20; ; msec<1000? msec=msec*2 : 1000) {
	status = pj_sock_bind(new_sock, &local_addr, addr_len);
	if (status != PJ_STATUS_FROM_OS(EADDRINUSE))
	    break;
	PJ_LOG(4,(THIS_FILE, "Address is still in use, retrying.."));
	pj_thread_sleep(msec);
    }

    if (status != PJ_SUCCESS)
	goto on_error;

    if (flags & HAS_QOS) {
	status = pj_sock_set_qos_params(new_sock, &qos_params);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    if (flags & HAS_PEER_ADDR) {
	status = pj_sock_connect(new_sock, &rem_addr, addr_len);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    /* Set socket to nonblocking. */
    val = 1;
#if defined(PJ_WIN32) && PJ_WIN32!=0 || \
    defined(PJ_WIN64) && PJ_WIN64 != 0 || \
    defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE!=0
    if (ioctlsocket(new_sock, FIONBIO, &val)) {
#else
    if (ioctl(new_sock, FIONBIO, &val)) {
#endif
        status = pj_get_netos_error();
	goto on_error;
    }

    /* Replace the occurrence of old socket with new socket in the
     * fd sets.
     */
    fds_cnt = 0;
    fds[fds_cnt++] = &h->ioqueue->rfdset;
    fds[fds_cnt++] = &h->ioqueue->wfdset;
#if PJ_HAS_TCP
    fds[fds_cnt++] = &h->ioqueue->xfdset;
#endif

    for (i=0; i<fds_cnt; ++i) {
	if (PJ_FD_ISSET(old_sock, fds[i])) {
	    PJ_FD_CLR(old_sock, fds[i]);
	    PJ_FD_SET(new_sock, fds[i]);
	}
    }

    /* And finally replace the fd in the key */
    h->fd = new_sock;

    PJ_LOG(4,(THIS_FILE, "UDP has been replaced successfully!"));

    pj_lock_release(h->ioqueue->lock);

    return PJ_SUCCESS;

on_error:
    if (new_sock != PJ_INVALID_SOCKET)
	pj_sock_close(new_sock);
    PJ_PERROR(1,(THIS_FILE, status, "Error replacing socket"));
    pj_lock_release(h->ioqueue->lock);
    return status;
}
#endif


/*
 * pj_ioqueue_poll()
 *
 * Few things worth written:
 *
 *  - we used to do only one callback called per poll, but it didn't go
 *    very well. The reason is because on some situation, the write 
 *    callback gets called all the time, thus doesn't give the read
 *    callback to get called. This happens, for example, when user
 *    submit write operation inside the write callback.
 *    As the result, we changed the behaviour so that now multiple
 *    callbacks are called in a single poll. It should be fast too,
 *    just that we need to be carefull with the ioqueue data structs.
 *
 *  - to guarantee preemptiveness etc, the poll function must strictly
 *    work on fd_set copy of the ioqueue (not the original one).
 */
PJ_DEF(int) pj_ioqueue_poll( pj_ioqueue_t *ioqueue, const pj_time_val *timeout)
{
    pj_fd_set_t rfdset, wfdset, xfdset;
    int nfds;
    int count, i, counter;
    pj_ioqueue_key_t *h;
    struct event
    {
        pj_ioqueue_key_t	*key;
        enum ioqueue_event_type  event_type;
    } event[PJ_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL];

    PJ_ASSERT_RETURN(ioqueue, -PJ_EINVAL);

    /* Lock ioqueue before making fd_set copies */
    pj_lock_acquire(ioqueue->lock);

    /* We will only do select() when there are sockets to be polled.
     * Otherwise select() will return error.
     */
    if (PJ_FD_COUNT(&ioqueue->rfdset)==0 &&
        PJ_FD_COUNT(&ioqueue->wfdset)==0 
#if defined(PJ_HAS_TCP) && PJ_HAS_TCP!=0
        && PJ_FD_COUNT(&ioqueue->xfdset)==0
#endif
	)
    {
#if PJ_IOQUEUE_HAS_SAFE_UNREG
	scan_closing_keys(ioqueue);
#endif
	pj_lock_release(ioqueue->lock);
	TRACE__((THIS_FILE, "     poll: no fd is set"));
        if (timeout)
            pj_thread_sleep(PJ_TIME_VAL_MSEC(*timeout));
        return 0;
    }

    /* Copy ioqueue's pj_fd_set_t to local variables. */
    pj_memcpy(&rfdset, &ioqueue->rfdset, sizeof(pj_fd_set_t));
    pj_memcpy(&wfdset, &ioqueue->wfdset, sizeof(pj_fd_set_t));
#if PJ_HAS_TCP
    pj_memcpy(&xfdset, &ioqueue->xfdset, sizeof(pj_fd_set_t));
#else
    PJ_FD_ZERO(&xfdset);
#endif

#if VALIDATE_FD_SET
    validate_sets(ioqueue, &rfdset, &wfdset, &xfdset);
#endif

    nfds = ioqueue->nfds;

    /* Unlock ioqueue before select(). */
    pj_lock_release(ioqueue->lock);

    count = pj_sock_select(nfds+1, &rfdset, &wfdset, &xfdset, 
			   timeout);
    
    if (count == 0)
	return 0;
    else if (count < 0)
	return -pj_get_netos_error();
    else if (count > PJ_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL)
        count = PJ_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL;

    /* Scan descriptor sets for event and add the events in the event
     * array to be processed later in this function. We do this so that
     * events can be processed in parallel without holding ioqueue lock.
     */
    pj_lock_acquire(ioqueue->lock);

    counter = 0;

    /* Scan for writable sockets first to handle piggy-back data
     * coming with accept().
     */
    h = ioqueue->active_list.next;
    for ( ; h!=&ioqueue->active_list && counter<count; h = h->next) {

	if ( (key_has_pending_write(h) || key_has_pending_connect(h))
	     && PJ_FD_ISSET(h->fd, &wfdset) && !IS_CLOSING(h))
        {
#if PJ_IOQUEUE_HAS_SAFE_UNREG
	    increment_counter(h);
#endif
            event[counter].key = h;
            event[counter].event_type = WRITEABLE_EVENT;
            ++counter;
        }

        /* Scan for readable socket. */
	if ((key_has_pending_read(h) || key_has_pending_accept(h))
            && PJ_FD_ISSET(h->fd, &rfdset) && !IS_CLOSING(h) &&
	    counter<count)
        {
#if PJ_IOQUEUE_HAS_SAFE_UNREG
	    increment_counter(h);
#endif
            event[counter].key = h;
            event[counter].event_type = READABLE_EVENT;
            ++counter;
	}

#if PJ_HAS_TCP
        if (key_has_pending_connect(h) && PJ_FD_ISSET(h->fd, &xfdset) &&
	    !IS_CLOSING(h) && counter<count) 
	{
#if PJ_IOQUEUE_HAS_SAFE_UNREG
	    increment_counter(h);
#endif
            event[counter].key = h;
            event[counter].event_type = EXCEPTION_EVENT;
            ++counter;
        }
#endif
    }

    for (i=0; i<counter; ++i) {
	if (event[i].key->grp_lock)
	    pj_grp_lock_add_ref_dbg(event[i].key->grp_lock, "ioqueue", 0);
    }

    PJ_RACE_ME(5);

    pj_lock_release(ioqueue->lock);

    PJ_RACE_ME(5);

    count = counter;

    /* Now process all events. The dispatch functions will take care
     * of locking in each of the key
     */
    for (counter=0; counter<count; ++counter) {
        switch (event[counter].event_type) {
        case READABLE_EVENT:
            ioqueue_dispatch_read_event(ioqueue, event[counter].key);
            break;
        case WRITEABLE_EVENT:
            ioqueue_dispatch_write_event(ioqueue, event[counter].key);
            break;
        case EXCEPTION_EVENT:
            ioqueue_dispatch_exception_event(ioqueue, event[counter].key);
            break;
        case NO_EVENT:
            pj_assert(!"Invalid event!");
            break;
        }

#if PJ_IOQUEUE_HAS_SAFE_UNREG
	decrement_counter(event[counter].key);
#endif

	if (event[counter].key->grp_lock)
	    pj_grp_lock_dec_ref_dbg(event[counter].key->grp_lock,
	                            "ioqueue", 0);
    }


    return count;
}

