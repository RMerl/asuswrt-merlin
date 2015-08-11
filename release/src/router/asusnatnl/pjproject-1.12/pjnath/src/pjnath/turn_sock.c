/* $Id: turn_sock.c 4386 2013-02-27 10:14:23Z nanang $ */
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
#include <pjnath/turn_sock.h>
#include <pj/activesock.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/ioqueue.h>

enum
{
    TIMER_NONE,
    TIMER_DESTROY
};

#define INIT	0x1FFFFFFF

#define THIS_FILE "turn_sock.c"

struct pj_turn_sock
{
    pj_pool_t		*pool;
    const char		*obj_name;
    pj_turn_session	*sess;
    pj_turn_sock_cb	 cb;
    void		*user_data;

    pj_lock_t		*lock;

    pj_turn_alloc_param	 alloc_param;
    pj_stun_config	 cfg;
    pj_turn_sock_cfg	 setting;

    pj_bool_t		 destroy_request;
    pj_timer_entry	 timer;

    int			 af;
    pj_turn_tp_type	 conn_type;
    pj_activesock_t	*active_sock;
    pj_ioqueue_op_key_t	 send_key;

	// 2013-04-19 DEAN, adding for multi turn sever use.
	struct {
		const pj_str_t *domain;
		int default_port;
		pj_dns_resolver *resolver;
	} turn;

	// 2013-05-09 DEAN for turn auto recovery
	int curr_turn;
	int turn_cnt;
	pj_turn_server turn_list[MAX_TURN_SERVER_COUNT];
};


/*
 * Callback prototypes.
 */
static pj_status_t turn_on_send_pkt(pj_turn_session *sess,
				    const pj_uint8_t *pkt,
				    unsigned pkt_len,
				    const pj_sockaddr_t *dst_addr,
				    unsigned dst_addr_len);
static void turn_on_channel_bound(pj_turn_session *sess,
				  const pj_sockaddr_t *peer_addr,
				  unsigned addr_len,
				  unsigned ch_num);
static void turn_on_rx_data(pj_turn_session *sess,
			    void *pkt,
			    unsigned pkt_len,
			    const pj_sockaddr_t *peer_addr,
			    unsigned addr_len);
static void turn_on_state(pj_turn_session *sess, 
			  pj_turn_state_t old_state,
			  pj_turn_state_t new_state);

static void turn_on_allocated(pj_turn_session *sess, 
							  pj_sockaddr *turn_srv);

static pj_bool_t on_data_read(pj_activesock_t *asock,
			      void *data,
			      pj_size_t size,
			      pj_status_t status,
			      pj_size_t *remainder);
static pj_bool_t on_connect_complete(pj_activesock_t *asock,
				     pj_status_t status);



static void destroy(pj_turn_sock *turn_sock);
static void timer_cb(pj_timer_heap_t *th, pj_timer_entry *e);


/* Init config */
PJ_DEF(void) pj_turn_sock_cfg_default(pj_turn_sock_cfg *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));
    cfg->qos_type = PJ_QOS_TYPE_BEST_EFFORT;
    cfg->qos_ignore_error = PJ_TRUE;
}

/*
 * Create.
 */
PJ_DEF(pj_status_t) pj_turn_sock_create(pj_stun_config *cfg,
					int af,
					pj_turn_tp_type conn_type,
					const pj_turn_sock_cb *cb,
					const pj_turn_sock_cfg *setting,
					void *user_data,
					pj_turn_sock **p_turn_sock)
{
    pj_turn_sock *turn_sock;
    pj_turn_session_cb sess_cb;
    pj_turn_sock_cfg default_setting;
    pj_pool_t *pool;
    const char *name_tmpl;
    pj_status_t status;

    PJ_ASSERT_RETURN(cfg && p_turn_sock, PJ_EINVAL);
    PJ_ASSERT_RETURN(af==pj_AF_INET() || af==pj_AF_INET6(), PJ_EINVAL);
    PJ_ASSERT_RETURN(conn_type!=PJ_TURN_TP_TCP || PJ_HAS_TCP, PJ_EINVAL);

    if (!setting) {
	pj_turn_sock_cfg_default(&default_setting);
	setting = &default_setting;
    }

    switch (conn_type) {
    case PJ_TURN_TP_UDP:
	name_tmpl = "udprel%p";
	break;
    case PJ_TURN_TP_TCP:
	name_tmpl = "tcprel%p";
	break;
    default:
	PJ_ASSERT_RETURN(!"Invalid TURN conn_type", PJ_EINVAL);
	name_tmpl = "tcprel%p";
	break;
    }

    /* Create and init basic data structure */
    pool = pj_pool_create(cfg->pf, name_tmpl, PJNATH_POOL_LEN_TURN_SOCK,
			  PJNATH_POOL_INC_TURN_SOCK, NULL);
    turn_sock = PJ_POOL_ZALLOC_T(pool, pj_turn_sock);
    turn_sock->pool = pool;
    turn_sock->obj_name = pool->obj_name;
    turn_sock->user_data = user_data;
    turn_sock->af = af;
    turn_sock->conn_type = conn_type;

    /* Copy STUN config (this contains ioqueue, timer heap, etc.) */
    pj_memcpy(&turn_sock->cfg, cfg, sizeof(*cfg));

    /* Copy setting (QoS parameters etc */
    pj_memcpy(&turn_sock->setting, setting, sizeof(*setting));

    /* Set callback */
    if (cb) {
	pj_memcpy(&turn_sock->cb, cb, sizeof(*cb));
    }

    /* Create lock */
    status = pj_lock_create_recursive_mutex(pool, turn_sock->obj_name, 
					    &turn_sock->lock);
	if (status != PJ_SUCCESS) {
		PJ_LOG(1, ("turn_session.c", "!!! TURN DEALLOCATE !!! in pj_turn_sock_create() destroy turn_sock for creating mutex failed."));
		destroy(turn_sock);
		return status;
    }

    /* Init timer */
    pj_timer_entry_init(&turn_sock->timer, TIMER_NONE, turn_sock, &timer_cb);

    /* Init TURN session */
    pj_bzero(&sess_cb, sizeof(sess_cb));
    sess_cb.on_send_pkt = &turn_on_send_pkt;
    sess_cb.on_channel_bound = &turn_on_channel_bound;
    sess_cb.on_rx_data = &turn_on_rx_data;
    sess_cb.on_state = &turn_on_state;
	// DEAN Added 2013-03-19
	sess_cb.on_turn_srv_allocated = &turn_on_allocated;
    status = pj_turn_session_create(cfg, pool->obj_name, af, conn_type,
				    &sess_cb, 0, turn_sock, &turn_sock->sess);
	if (status != PJ_SUCCESS) {
		PJ_LOG(1, ("turn_session.c", "!!! TURN DEALLOCATE !!! in pj_turn_sock_create() destroy turn_sock for creating turn session failed."));
		destroy(turn_sock);
		return status;
    }

    /* Note: socket and ioqueue will be created later once the TURN server
     * has been resolved.
     */

    *p_turn_sock = turn_sock;
    return PJ_SUCCESS;
}

/*
 * Destroy.
 */
static void destroy(pj_turn_sock *turn_sock)
{
    if (turn_sock->lock) {
	pj_lock_acquire(turn_sock->lock);
    }

    if (turn_sock->sess) {
	pj_turn_session_set_user_data(turn_sock->sess, NULL);
	pj_turn_session_shutdown(turn_sock->sess);
	turn_sock->sess = NULL;
    }

	if (turn_sock->active_sock) {
		PJ_LOG(4, ("turn_session.c", "!!! TURN DEALLOCATE !!! in destroy() close connection"));
        pj_activesock_set_user_data(turn_sock->active_sock, NULL);
		pj_activesock_close(turn_sock->active_sock);
		turn_sock->active_sock = NULL;
    }

    if (turn_sock->lock) {
	pj_lock_release(turn_sock->lock);
	pj_lock_destroy(turn_sock->lock);
	turn_sock->lock = NULL;
    }

    if (turn_sock->pool) {
	pj_pool_t *pool = turn_sock->pool;
	turn_sock->pool = NULL;
	pj_pool_release(pool);
    }
}


PJ_DEF(void) pj_turn_sock_destroy(pj_turn_sock *turn_sock)
{
    pj_lock_acquire(turn_sock->lock);
    turn_sock->destroy_request = PJ_TRUE;

    if (turn_sock->sess) {
		pj_turn_session_shutdown(turn_sock->sess);
		/* This will ultimately call our state callback, and when
		 * session state is DESTROYING we will schedule a timer to
		 * destroy ourselves.
		 */
		pj_lock_release(turn_sock->lock);
    } else {
		pj_lock_release(turn_sock->lock);
		PJ_LOG(1, ("turn_session.c", "!!! TURN DEALLOCATE !!! in pj_turn_sock_destroy() destroy turn_sock."));
		destroy(turn_sock);
    }

}


/* Timer callback */
static void timer_cb(pj_timer_heap_t *th, pj_timer_entry *e)
{
    pj_turn_sock *turn_sock = (pj_turn_sock*)e->user_data;
    int eid = e->id;

    PJ_UNUSED_ARG(th);

    e->id = TIMER_NONE;

    switch (eid) {
    case TIMER_DESTROY:
	PJ_LOG(5,(turn_sock->obj_name, "Destroying TURN"));
	destroy(turn_sock);
	break;
    default:
	pj_assert(!"Invalid timer id");
	break;
    }
}


/* Display error */
static void show_err(pj_turn_sock *turn_sock, const char *title,
		     pj_status_t status)
{
    PJ_PERROR(6,(turn_sock->obj_name, status, title));
}

/* On error, terminate session */
static void sess_fail(pj_turn_sock *turn_sock, const char *title,
		      pj_status_t status)
{
    show_err(turn_sock, title, status);
    if (turn_sock->sess) {
	pj_turn_session_destroy(turn_sock->sess, status);
    }
}

/*
 * Set user data.
 */
PJ_DEF(pj_status_t) pj_turn_sock_set_user_data( pj_turn_sock *turn_sock,
					       void *user_data)
{
    PJ_ASSERT_RETURN(turn_sock, PJ_EINVAL);
    turn_sock->user_data = user_data;
    return PJ_SUCCESS;
}

/*
 * Get user data.
 */
PJ_DEF(void*) pj_turn_sock_get_user_data(pj_turn_sock *turn_sock)
{
    PJ_ASSERT_RETURN(turn_sock, NULL);
    return turn_sock->user_data;
}

/**
 * Get info.
 */
PJ_DEF(pj_status_t) pj_turn_sock_get_info(pj_turn_sock *turn_sock,
					  pj_turn_session_info *info)
{
    PJ_ASSERT_RETURN(turn_sock && info, PJ_EINVAL);

    if (turn_sock->sess) {
	return pj_turn_session_get_info(turn_sock->sess, info);
    } else {
	pj_bzero(info, sizeof(*info));
	info->state = PJ_TURN_STATE_NULL;
	return PJ_SUCCESS;
    }
}

/**
 * Lock the TURN socket. Application may need to call this function to
 * synchronize access to other objects to avoid deadlock.
 */
PJ_DEF(pj_status_t) pj_turn_sock_lock(pj_turn_sock *turn_sock)
{
    return pj_lock_acquire(turn_sock->lock);
}

/**
 * Unlock the TURN socket.
 */
PJ_DEF(pj_status_t) pj_turn_sock_unlock(pj_turn_sock *turn_sock)
{
    return pj_lock_release(turn_sock->lock);
}

/*
 * Set STUN message logging for this TURN session. 
 */
PJ_DEF(void) pj_turn_sock_set_log( pj_turn_sock *turn_sock,
				   unsigned flags)
{
    pj_turn_session_set_log(turn_sock->sess, flags);
}

/*
 * Set software name
 */
PJ_DEF(pj_status_t) pj_turn_sock_set_software_name( pj_turn_sock *turn_sock,
						    const pj_str_t *sw)
{
    return pj_turn_session_set_software_name(turn_sock->sess, sw);
}

PJ_DEF(pj_status_t) pj_turn_sock_alloc(pj_turn_sock *turn_sock,
									   const pj_str_t *domain,
									   int default_port,
									   pj_dns_resolver *resolver,
									   const pj_stun_auth_cred *cred,
									   const pj_turn_alloc_param *param) {

	return pj_turn_sock_alloc2(turn_sock, domain, default_port, 
		resolver, cred, param, -1, 0, NULL);
}

/*
 * 2013-05-08 DEAN modifedf by adding parameters.
 * Initialize.
 */
PJ_DEF(pj_status_t) pj_turn_sock_alloc2(pj_turn_sock *turn_sock,
				       const pj_str_t *domain,
				       int default_port,
				       pj_dns_resolver *resolver,
				       const pj_stun_auth_cred *cred,
					   const pj_turn_alloc_param *param,
					   int curr_turn,
					   int turn_cnt,
					   pj_turn_server turn_list[])
{
    pj_status_t status;

    PJ_ASSERT_RETURN(turn_sock && domain && turn_cnt <= MAX_TURN_SERVER_COUNT, PJ_EINVAL);
    PJ_ASSERT_RETURN(turn_sock->sess, PJ_EINVALIDOP);

    /* Copy alloc param. We will call session_alloc() only after the 
     * server address has been resolved.
     */
    if (param) {
	pj_turn_alloc_param_copy(turn_sock->pool, &turn_sock->alloc_param, param);
    } else {
	pj_turn_alloc_param_default(&turn_sock->alloc_param);
    }

    /* Set credential */
	if (cred) {
		PJ_LOG(4, (THIS_FILE, "pj_turn_sock_alloc2() turn tp type=%d", turn_sock->conn_type));
		PJ_LOG(4, (THIS_FILE, "pj_turn_sock_alloc2() turn server=%.*s:%d", domain->slen, domain->ptr, default_port));
		PJ_LOG(4, (THIS_FILE, "pj_turn_sock_alloc2() turn realm=%.*s", cred->data.static_cred.realm.slen, cred->data.static_cred.realm.ptr));
		PJ_LOG(4, (THIS_FILE, "pj_turn_sock_alloc2() turn username=%.*s", cred->data.static_cred.username.slen, cred->data.static_cred.username.ptr));
		PJ_LOG(4, (THIS_FILE, "pj_turn_sock_alloc2() turn password=%.*s", cred->data.static_cred.data.slen, cred->data.static_cred.data.ptr));
		status = pj_turn_session_set_credential(turn_sock->sess, cred);
		if (status != PJ_SUCCESS) {
			PJ_LOG(1, ("turn_sock.c", "!!! TURN DEALLOCATE !!! in pj_turn_sock_alloc2() pj_turn_session_set_credential failed=%d", status));
			sess_fail(turn_sock, "Error setting credential", status);
			return status;
		}
    }

	// 2014-04-19 DEAN, for retry another turn use
	turn_sock->turn.domain = domain;
	turn_sock->turn.default_port = default_port;
	turn_sock->turn.resolver = resolver;
	turn_sock->curr_turn = curr_turn;
	turn_sock->turn_cnt = turn_cnt;
	memcpy(turn_sock->turn_list, turn_list, sizeof(pj_turn_server)*turn_cnt);	

    /* Resolve server */
    status = pj_turn_session_set_server(turn_sock->sess, domain, default_port,
					resolver);
	if (status != PJ_SUCCESS) {
		PJ_LOG(1, ("turn_sock.c", "!!! TURN DEALLOCATE !!! in pj_turn_sock_alloc2() pj_turn_session_set_server failed=%d", status));
		sess_fail(turn_sock, "Error setting TURN server", status);
		return status;
    }

    /* Done for now. The next work will be done when session state moved
     * to RESOLVED state.
     */

    return PJ_SUCCESS;
}

/*
 * Install permission
 */
PJ_DEF(pj_status_t) pj_turn_sock_set_perm( pj_turn_sock *turn_sock,
					   unsigned addr_cnt,
					   const pj_sockaddr addr[],
					   unsigned options)
{
    if (turn_sock->sess == NULL)
	return PJ_EINVALIDOP;

    return pj_turn_session_set_perm(turn_sock->sess, addr_cnt, addr, options);
}

/*
 * Send packet.
 */ 
PJ_DEF(pj_status_t) pj_turn_sock_sendto( pj_turn_sock *turn_sock,
					const pj_uint8_t *pkt,
					unsigned pkt_len,
					const pj_sockaddr_t *addr,
					unsigned addr_len)
{
    PJ_ASSERT_RETURN(turn_sock && addr && addr_len, PJ_EINVAL);

    if (turn_sock->sess == NULL)
	return PJ_EINVALIDOP;

    return pj_turn_session_sendto(turn_sock->sess, pkt, pkt_len, 
				  addr, addr_len);
}

/*
 * Bind a peer address to a channel number.
 */
PJ_DEF(pj_status_t) pj_turn_sock_bind_channel( pj_turn_sock *turn_sock,
					      const pj_sockaddr_t *peer,
					      unsigned addr_len)
{
    PJ_ASSERT_RETURN(turn_sock && peer && addr_len, PJ_EINVAL);
    PJ_ASSERT_RETURN(turn_sock->sess != NULL, PJ_EINVALIDOP);

    return pj_turn_session_bind_channel(turn_sock->sess, peer, addr_len);
}


/*
 * Notification when outgoing TCP socket has been connected.
 */
static pj_bool_t on_connect_complete(pj_activesock_t *asock,
				     pj_status_t status)
{
    pj_turn_sock *turn_sock;

    turn_sock = (pj_turn_sock*) pj_activesock_get_user_data(asock);
    if (!turn_sock)
        return PJ_FALSE;

    /* TURN session may have already been destroyed here.
     * See ticket #1557 (http://trac.pjsip.org/repos/ticket/1557).
     */
	if (!turn_sock->sess) {
		PJ_LOG(1, ("turn_sock.c", "!!! TURN DEALLOCATE !!! in on_connect_complete() turn_sock->sess is NULL status=%d", status));
		sess_fail(turn_sock, "TURN session already destroyed", status);
		return PJ_FALSE;
    }

    if (status != PJ_SUCCESS) {

		// DEAN assigned next turn server 
		if (turn_sock->turn_cnt <= ++turn_sock->curr_turn) {
			PJ_LOG(1, ("turn_sock.c", "!!! TURN DEALLOCATE !!! in on_connect_complete() turn_sock->turn_cnt <= ++turn_sock->curr_turn (%d,%d)", 
				turn_sock->turn_cnt, turn_sock->curr_turn));
			sess_fail(turn_sock, "TCP connect() error", status);
			turn_sock->curr_turn = 0;
			PJ_LOG(3, (__FILE__, "Failed(%d) connect to all turn servers.", status));
		} else {

			set_state(turn_sock->sess, PJ_TURN_STATE_NULL);

			PJ_LOG(3, (__FILE__, "Failed(%d) connect to turn server [%.*s:%d].",
				status,
				turn_sock->turn.domain->slen, 
				turn_sock->turn.domain->ptr, 
				turn_sock->turn.default_port));
			turn_sock->turn.domain = &turn_sock->turn_list[turn_sock->curr_turn].server;
			turn_sock->turn.default_port = turn_sock->turn_list[turn_sock->curr_turn].port;
			PJ_LOG(3, (__FILE__, "Try another turn server [%.*s:%d].",
				turn_sock->turn.domain->slen, 
				turn_sock->turn.domain->ptr, 
				turn_sock->turn.default_port));
			
			/* Resolve server */
			status = pj_turn_session_set_server(turn_sock->sess, turn_sock->turn.domain, 
				turn_sock->turn.default_port, turn_sock->turn.resolver);
			if (status != PJ_SUCCESS) {
				PJ_LOG(1, ("turn_sock.c", "!!! TURN DEALLOCATE !!! in on_connect_complete() pj_turn_session_set_server failed status=%d", 
					status));
				sess_fail(turn_sock, "Error setting TURN server", status);
				return status;
			}
		}
		return PJ_FALSE;
    }

    if (turn_sock->conn_type != PJ_TURN_TP_UDP) {
	PJ_LOG(5,(turn_sock->obj_name, "TCP connected"));
    }

    /* Kick start pending read operation */
    status = pj_activesock_start_read(asock, turn_sock->pool, 
				      PJ_TURN_MAX_PKT_LEN, 0);

    /* Init send_key */
    pj_ioqueue_op_key_init(&turn_sock->send_key, sizeof(turn_sock->send_key));

   /* Send Allocate request */
    status = pj_turn_session_alloc(turn_sock->sess, &turn_sock->alloc_param);
	if (status != PJ_SUCCESS) {
		PJ_LOG(1, ("turn_sock.c", "!!! TURN DEALLOCATE !!! in on_connect_complete() pj_turn_session_alloc failed status=%d", 
			status));
		sess_fail(turn_sock, "Error sending ALLOCATE", status);
		return PJ_FALSE;
    }

    return PJ_TRUE;
}

static pj_uint16_t GETVAL16H(const pj_uint8_t *buf, unsigned pos)
{
    return (pj_uint16_t) ((buf[pos + 0] << 8) | \
			  (buf[pos + 1] << 0));
}

/* Quick check to determine if there is enough packet to process in the
 * incoming buffer. Return the packet length, or zero if there's no packet.
 */
static unsigned has_packet(pj_turn_sock *turn_sock, const void *buf, pj_size_t bufsize)
{
    pj_bool_t is_stun;

    if (turn_sock->conn_type == PJ_TURN_TP_UDP)
	return bufsize;

    /* Quickly check if this is STUN message, by checking the first two bits and
     * size field which must be multiple of 4 bytes
     */
    is_stun = ((((pj_uint8_t*)buf)[0] & 0xC0) == 0) &&
	      ((GETVAL16H((const pj_uint8_t*)buf, 2) & 0x03)==0);

    if (is_stun) {
	pj_size_t msg_len = GETVAL16H((const pj_uint8_t*)buf, 2);
	return (msg_len+20 <= bufsize) ? msg_len+20 : 0;
    } else {
	/* This must be ChannelData. */
	pj_turn_channel_data cd;

	if (bufsize < 4)
	    return 0;

	/* Decode ChannelData packet */
	pj_memcpy(&cd, buf, sizeof(pj_turn_channel_data));
	cd.length = pj_ntohs(cd.length);

	if (bufsize >= cd.length+sizeof(cd)) 
	    return (cd.length+sizeof(cd)+3) & (~3);
	else
	    return 0;
    }
}

/*
 * Notification from ioqueue when incoming UDP packet is received.
 */
static pj_bool_t on_data_read(pj_activesock_t *asock,
			      void *data,
			      pj_size_t size,
			      pj_status_t status,
			      pj_size_t *remainder)
{
    pj_turn_sock *turn_sock;
    pj_bool_t ret = PJ_TRUE;

    turn_sock = (pj_turn_sock*) pj_activesock_get_user_data(asock);
    pj_lock_acquire(turn_sock->lock);

    if (status == PJ_SUCCESS && turn_sock->sess) {
	/* Report incoming packet to TURN session, repeat while we have
	 * "packet" in the buffer (required for stream-oriented transports)
	 */
	unsigned pkt_len;

	//PJ_LOG(5,(turn_sock->pool->obj_name, 
	//	  "Incoming data, %lu bytes total buffer", size));

	while ((pkt_len=has_packet(turn_sock, data, size)) != 0) {
	    pj_size_t parsed_len;
	    //const pj_uint8_t *pkt = (const pj_uint8_t*)data;

	    //PJ_LOG(5,(turn_sock->pool->obj_name, 
	    //	      "Packet start: %02X %02X %02X %02X", 
	    //	      pkt[0], pkt[1], pkt[2], pkt[3]));

	    //PJ_LOG(5,(turn_sock->pool->obj_name, 
	    //	      "Processing %lu bytes packet of %lu bytes total buffer",
	    //	      pkt_len, size));

	    parsed_len = (unsigned)size;
	    pj_turn_session_on_rx_pkt(turn_sock->sess, data,  size, &parsed_len);

	    /* parsed_len may be zero if we have parsing error, so use our
	     * previous calculation to exhaust the bad packet.
	     */
	    if (parsed_len == 0)
		parsed_len = pkt_len;

	    if (parsed_len < (unsigned)size) {
		*remainder = size - parsed_len;
		pj_memmove(data, ((char*)data)+parsed_len, *remainder);
	    } else {
		*remainder = 0;
		}
	    size = *remainder;

	    //PJ_LOG(5,(turn_sock->pool->obj_name, 
	    //	      "Buffer size now %lu bytes", size));
	}
	/* Assigned remainder as size. Because ioqueue may 
	   skip the packet if never enter while loop. */
	if (pkt_len == 0)
		*remainder = size;

	PJ_LOG(5, (__FILE__, "on_data_read() leaving still remainder=[%d].", 
		*remainder));
    } else if (status != PJ_SUCCESS && 
	       turn_sock->conn_type != PJ_TURN_TP_UDP) 
    {
		// DEAN don't destroy TURN session, if connection aborted.
		// To avoid the situation of ip changing caused crash.
		if (status != 130053) 
		{
			PJ_LOG(1, ("turn_sock.c", "!!! TURN DEALLOCATE !!! in on_data_read() read failed status=%d", 
				status));
			sess_fail(turn_sock, "TURN TCP connection closed", status);
		}

		ret = PJ_FALSE;
		goto on_return;
    }

on_return:
    pj_lock_release(turn_sock->lock);

    return ret;
}


/*
 * Callback from TURN session to send outgoing packet.
 */
static pj_status_t turn_on_send_pkt(pj_turn_session *sess,
				    const pj_uint8_t *pkt,
				    unsigned pkt_len,
				    const pj_sockaddr_t *dst_addr,
				    unsigned dst_addr_len)
{
    pj_turn_sock *turn_sock = (pj_turn_sock*) 
			      pj_turn_session_get_user_data(sess);
    pj_ssize_t len = pkt_len;
    pj_status_t status;
    pj_bool_t is_stun = PJ_FALSE;
    pj_bool_t is_tnl_data = PJ_FALSE;

    if (turn_sock == NULL) {
	/* We've been destroyed */
	// https://trac.pjsip.org/repos/ticket/1316
	//pj_assert(!"We should shutdown gracefully");
	return PJ_EINVALIDOP;
    }

    PJ_UNUSED_ARG(dst_addr);
    PJ_UNUSED_ARG(dst_addr_len);

    /* Check that this is STUN message */
    status = pj_stun_msg_check((const pj_uint8_t*)pkt, pkt_len, 
    			       PJ_STUN_IS_DATAGRAM | PJ_STUN_CHECK_PACKET);
    if (status == PJ_SUCCESS) 
        is_stun = PJ_TRUE;
	else
		is_tnl_data = (((pj_uint8_t*)pkt)[pkt_len] == 1);

    if(is_stun || !is_tnl_data) {
        pj_ioqueue_op_key_t *op_key = (pj_ioqueue_op_key_t*)malloc(sizeof(pj_ioqueue_op_key_t));
        pj_ioqueue_op_key_init(op_key, sizeof(pj_ioqueue_op_key_t));
	    status = pj_activesock_send(turn_sock->active_sock, op_key,
			        pkt, &len, PJ_IOQUEUE_URGENT_DATA);
     } else {
	    status = pj_activesock_send(turn_sock->active_sock, &turn_sock->send_key,
		        	pkt, &len, 0);
    }

    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	show_err(turn_sock, "socket send()", status);
    }

    return status;
}


/*
 * Callback from TURN session when a channel is successfully bound.
 */
static void turn_on_channel_bound(pj_turn_session *sess,
				  const pj_sockaddr_t *peer_addr,
				  unsigned addr_len,
				  unsigned ch_num)
{
    PJ_UNUSED_ARG(sess);
    PJ_UNUSED_ARG(peer_addr);
    PJ_UNUSED_ARG(addr_len);
    PJ_UNUSED_ARG(ch_num);
}


/*
 * Callback from TURN session upon incoming data.
 */
static void turn_on_rx_data(pj_turn_session *sess,
			    void *pkt,
			    unsigned pkt_len,
			    const pj_sockaddr_t *peer_addr,
			    unsigned addr_len)
{
    pj_turn_sock *turn_sock = (pj_turn_sock*) 
			   pj_turn_session_get_user_data(sess);
    if (turn_sock == NULL) {
	/* We've been destroyed */
	return;
    }

    if (turn_sock->cb.on_rx_data) {
	(*turn_sock->cb.on_rx_data)(turn_sock, pkt, pkt_len, 
				  peer_addr, addr_len);
    }
}


/*
 * Callback from TURN session when state has changed
 */
static void turn_on_state(pj_turn_session *sess, 
			  pj_turn_state_t old_state,
			  pj_turn_state_t new_state)
{
    pj_turn_sock *turn_sock = (pj_turn_sock*) 
			   pj_turn_session_get_user_data(sess);
    pj_status_t status;

    if (turn_sock == NULL) {
	/* We've been destroyed */
	return;
    }

    /* Notify app first */
    if (turn_sock->cb.on_state) {
	(*turn_sock->cb.on_state)(turn_sock, old_state, new_state);
    }

    /* Make sure user hasn't destroyed us in the callback */
    if (turn_sock->sess && new_state == PJ_TURN_STATE_RESOLVED) {
	pj_turn_session_info info;
	pj_turn_session_get_info(turn_sock->sess, &info);
	new_state = info.state;
    }

    if (turn_sock->sess && new_state == PJ_TURN_STATE_RESOLVED) {
	/*
	 * Once server has been resolved, initiate outgoing TCP
	 * connection to the server.
	 */
	pj_turn_session_info info;
	char addrtxt[PJ_INET6_ADDRSTRLEN+8];
	int sock_type;
	pj_sock_t sock;
	pj_activesock_cb asock_cb;
    pj_activesock_cfg asock_cfg;

	/* Close existing connection, if any. This happens when
	 * we're switching to alternate TURN server when either TCP
	 * connection or ALLOCATE request failed.
	 */
	if (turn_sock->active_sock) {
		PJ_LOG(4, (THIS_FILE, "turn_on_state() Close connection for new_state == PJ_TURN_STATE_RESOLVED."));
	    pj_activesock_close(turn_sock->active_sock);
	    turn_sock->active_sock = NULL;
	}

	/* Get server address from session info */
	pj_turn_session_get_info(sess, &info);

	if (turn_sock->conn_type == PJ_TURN_TP_UDP)
	    sock_type = pj_SOCK_DGRAM();
	else
	    sock_type = pj_SOCK_STREAM();

	/* Init socket */
	status = pj_sock_socket(turn_sock->af, sock_type, 0, &sock);
	if (status != PJ_SUCCESS) {
		PJ_LOG(1, (THIS_FILE, "turn_on_state() Failed to destroy turn_sock for sock creation. status=[%d]", status));
	    pj_turn_sock_destroy(turn_sock);
	    return;
	}

	{
		int flag = turn_sock->setting.sock_recv_buf_size ? turn_sock->setting.sock_recv_buf_size : PJ_TCP_MAX_PKT_LEN;

		status = pj_sock_setsockopt(sock, pj_SOL_SOCKET(), pj_SO_RCVBUF(),
			&flag, sizeof(flag));
		if (status != PJ_SUCCESS) {
			PJ_LOG(2, (THIS_FILE, "turn_on_state() Failed to set SO_RCVBUF option. status=[%d]", status));
			return;
		}

		flag = turn_sock->setting.sock_send_buf_size ? turn_sock->setting.sock_send_buf_size : PJ_SOCKET_SND_BUFFER_SIZE;
		status = pj_sock_setsockopt(sock, pj_SOL_SOCKET(), pj_SO_SNDBUF(),
			&flag, sizeof(flag));
		if (status != PJ_SUCCESS) {
			PJ_LOG(2, (THIS_FILE, "turn_on_state() Failed to set SO_SNDBUF option. status=[%d]", status));
			return;
		}
	}

        /* Apply QoS, if specified */
	status = pj_sock_apply_qos2(sock, turn_sock->setting.qos_type,
				    &turn_sock->setting.qos_params, 
				    (turn_sock->setting.qos_ignore_error?2:1),
				    turn_sock->pool->obj_name, NULL);
	if (status != PJ_SUCCESS && !turn_sock->setting.qos_ignore_error) {
		PJ_LOG(1, (THIS_FILE, "turn_on_state() Failed to destroy turn_sock for pj_sock_apply_qos2. status=[%d]", status));
	    pj_turn_sock_destroy(turn_sock);
	    return;
	}

	/* Create active socket */
	pj_activesock_cfg_default(&asock_cfg);
	asock_cfg.concurrency = 1;
	asock_cfg.whole_data = PJ_TRUE;

    pj_bzero(&asock_cb, sizeof(asock_cb));
	asock_cb.on_data_read = &on_data_read;
	asock_cb.on_connect_complete = &on_connect_complete;
	status = pj_activesock_create(turn_sock->pool, sock,
				      sock_type, &asock_cfg,
				      turn_sock->cfg.ioqueue, &asock_cb, 
				      turn_sock,
				      &turn_sock->active_sock);
	if (status != PJ_SUCCESS) {
		PJ_LOG(1, (THIS_FILE, "turn_on_state() Failed to destroy turn_sock for pj_activesock_create. status=[%d]", status));
	    pj_turn_sock_destroy(turn_sock);
	    return;
	}

	PJ_LOG(5,(turn_sock->pool->obj_name,
		  "Connecting to %s", 
		  pj_sockaddr_print(&info.server, addrtxt, 
				    sizeof(addrtxt), 3)));

	/* Initiate non-blocking connect */
#if PJ_HAS_TCP
	status=pj_activesock_start_connect(turn_sock->active_sock, 
					   turn_sock->pool,
					   &info.server, 
					   pj_sockaddr_get_len(&info.server));
	if (status == PJ_SUCCESS) {
	    on_connect_complete(turn_sock->active_sock, PJ_SUCCESS);
	} else if (status != PJ_EPENDING) {
		PJ_LOG(1, (THIS_FILE, "turn_on_state() Failed to destroy turn_sock for pj_activesock_start_connect. status=[%d]", status));
	    pj_turn_sock_destroy(turn_sock);
	    return;
	}
#else
	on_connect_complete(turn_sock->active_sock, PJ_SUCCESS);
#endif

	/* Done for now. Subsequent work will be done in 
	 * on_connect_complete() callback.
	 */
    }

    if (new_state >= PJ_TURN_STATE_DESTROYING && turn_sock->sess) {
	pj_time_val delay = {0, 0};

	turn_sock->sess = NULL;
	pj_turn_session_set_user_data(sess, NULL);

	if (turn_sock->timer.id) {
	    pj_timer_heap_cancel(turn_sock->cfg.timer_heap, &turn_sock->timer);
	    turn_sock->timer.id = 0;
	}

	turn_sock->timer.id = TIMER_DESTROY;
	pj_timer_heap_schedule(turn_sock->cfg.timer_heap, &turn_sock->timer, 
			       &delay);
    }
}

/* DEAN Added 2013-03-19 */
static void turn_on_allocated(pj_turn_session *sess, 
							  pj_sockaddr *turn_srv)
{
	pj_turn_sock *turn_sock = (pj_turn_sock*) pj_turn_session_get_user_data(sess);
	if (turn_sock && turn_sock->cb.on_turn_srv_allocated)
		(*turn_sock->cb.on_turn_srv_allocated)(turn_sock, turn_srv);
}

PJ_DEF(pj_bool_t) pj_turn_sock_is_tp_tcp(pj_turn_sock *turn_sock)
{
	return turn_sock->conn_type == PJ_TURN_TP_TCP;
}

PJ_DEF(void *) pj_turn_sock_get_session(pj_turn_sock *turn_sock)
{
        return turn_sock->sess;
}

PJ_DEF(pj_turn_tp_type) pj_turn_sock_get_conn_type(pj_turn_sock *turn_sock)
{
	return turn_sock->conn_type;
}

