/* $Id: tcp_sock.c 3596 2012-06-25 11:27:11Z dean $ */
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
#include <pjnath/ice_strans.h>
#include <pjnath/tcp_sock.h>
#include <pj/activesock.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/ioqueue.h>

#define THIS_FILE "tcp_sock.c"

enum
{
    TIMER_NONE,
    TIMER_DESTROY
};

#define INIT	0x1FFFFFFF

/* The data that will be attached to the STUN session on each
 * component.
 */
typedef struct tcp_stun_data
{
    pj_ice_sess		*ice;
    unsigned		 comp_id;
    pj_ice_sess_comp	*comp;
} tcp_stun_data;

struct pj_tcp_sock
{
    pj_pool_t		*pool;
	const char		*obj_name;
	pj_tcp_session	*server_sess;
	pj_tcp_session	*accept_sess[MAX_TCP_ACCEPT_SESS];
	int				accept_sess_cnt;
	pj_tcp_session	*client_sess[MAX_TCP_CLIENT_SESS];
	int				client_sess_cnt;
	//pj_tcp_session	*client_externl_sess;
	//pj_tcp_session	*client_subnet_sess;
    pj_tcp_sock_cb	 cb;
    void		*user_data;

    pj_lock_t		*lock;

    pj_stun_config	 cfg;
    pj_tcp_sock_cfg	 setting;

    pj_bool_t		 destroy_request;
    pj_timer_entry	 timer;

    int			 af;
	pj_activesock_t	*server_asock;

    pj_ioqueue_op_key_t	 send_key;

	pj_sockaddr     *local_addr;
	pj_bool_t        is_server;

	struct tcp_stun_data tsd;

	pj_bool_t        partial_destroy;
};

struct outgoing_packet
{
	void *pkt;
	unsigned pkt_len;
};

/*
 * Callback prototypes.
 */
static pj_status_t tcp_on_send_pkt(pj_tcp_session *sess,
								   const pj_uint8_t *pkt,
								   unsigned pkt_len,
								   const pj_sockaddr_t *dst_addr,
								   unsigned dst_addr_len);
#if TCP_TODO
static void tcp_on_channel_bound(pj_tcp_session *sess,
								 const pj_sockaddr_t *peer_addr,
								 unsigned addr_len,
								 unsigned ch_num);
#endif
static void tcp_on_rx_data(pj_tcp_session *sess,
						   void *pkt,
						   unsigned pkt_len,
						   const pj_sockaddr_t *peer_addr,
						   unsigned addr_len);
static void tcp_sock_state(pj_tcp_session *sess, 
						 pj_tcp_state_t old_state,
						 pj_tcp_state_t new_state);

static pj_status_t tcp_client_on_send_pkt(pj_tcp_session *sess,
								   const pj_uint8_t *pkt,
								   unsigned pkt_len,
								   const pj_sockaddr_t *dst_addr,
								   unsigned dst_addr_len);
static void tcp_client_on_rx_data(pj_tcp_session *sess,
						   void *pkt,
						   unsigned pkt_len,
						   const pj_sockaddr_t *peer_addr,
						   unsigned addr_len);
static void tcp_client_on_state(pj_tcp_session *sess, 
						 pj_tcp_state_t old_state,
						 pj_tcp_state_t new_state);

#if 0
/* Callback from active socket when incoming packet is received */
static pj_bool_t on_data_recvfrom(pj_activesock_t *asock,
				  void *data,
				  pj_size_t size,
				  const pj_sockaddr_t *src_addr,
				  int addr_len,
				  pj_status_t status);
#endif
static pj_bool_t on_data_read(pj_activesock_t *asock,
			      void *data,
			      pj_size_t size,
			      pj_status_t status,
			      pj_size_t *remainder);

/* Callback when packet is sent */
static pj_bool_t on_data_sent(pj_activesock_t *asock,
			      pj_ioqueue_op_key_t *send_key,
				  pj_ssize_t sent);

static pj_bool_t on_connect_complete(pj_activesock_t *asock,
									 pj_status_t status);

static pj_bool_t on_client_connect_complete(pj_activesock_t *asock,
									 pj_status_t status);

/* For tcp server */
static pj_bool_t on_accept_complete(pj_activesock_t *asock,
				    pj_sock_t sock,
				    const pj_sockaddr_t *src_addr,
				    int src_addr_len);


static void destroy(pj_tcp_sock *tcp_sock);
static void timer_cb(pj_timer_heap_t *th, pj_timer_entry *e);


/* Init config */
PJ_DEF(void) pj_tcp_sock_cfg_default(pj_tcp_sock_cfg *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));
    cfg->qos_type = PJ_QOS_TYPE_BEST_EFFORT;
    cfg->qos_ignore_error = PJ_TRUE;
}

/*
 * Create.
 */
PJ_DEF(pj_status_t) pj_tcp_sock_create(pj_stun_config *cfg,
					int af,
					const pj_tcp_sock_cb *cb,
#if 0
					const pj_tcp_sock_cfg *setting,
#endif
					void *user_data,
					unsigned comp_id,
					pj_tcp_sock **p_tcp_sock)
{
    pj_tcp_sock *tcp_sock;
	pj_tcp_session_cb sess_server_cb, sess_accept_cb;//, sess_client_cb;
#if 0
	pj_tcp_sock_cfg default_setting;
#endif
    pj_pool_t *pool;
    const char *name_tmpl;
    pj_status_t status;
	pj_stun_session *stun;

	struct pj_ice_strans_comp *ice_st_comp = (struct pj_ice_strans_comp *)user_data;

    PJ_ASSERT_RETURN(cfg && p_tcp_sock, PJ_EINVAL);
    PJ_ASSERT_RETURN(af==pj_AF_INET() || af==pj_AF_INET6(), PJ_EINVAL);

	PJ_UNUSED_ARG(comp_id);
#if 0
    if (!setting) {
	pj_tcp_sock_cfg_default(&default_setting);
	setting = &default_setting;
    }
#endif
	name_tmpl = "tcpupnp%p";

    /* Create and init basic data structure */
    pool = pj_pool_create(cfg->pf, name_tmpl, PJNATH_POOL_LEN_TCP_SOCK,
			  PJNATH_POOL_INC_TCP_SOCK, NULL);
    tcp_sock = PJ_POOL_ZALLOC_T(pool, pj_tcp_sock);
    tcp_sock->pool = pool;
    tcp_sock->obj_name = pool->obj_name;
    tcp_sock->user_data = user_data;
    tcp_sock->af = af;
    tcp_sock->local_addr = PJ_POOL_ZALLOC_T(pool, pj_sockaddr);

    {
	struct tcp_stun_data *tsd = &tcp_sock->tsd;
        //tcp_sock->tsd = PJ_POOL_ZALLOC_T(pool, struct tcp_stun_data);
        tsd->ice = pj_ice_strans_get_ice_session(pj_ice_strans_get_ice_strans(ice_st_comp));
        tsd->comp_id = pj_ice_strans_get_comp_id(ice_st_comp);
        tsd->comp = &tsd->ice->comp[tsd->comp_id-1];
    }


    /* Copy STUN config (this contains ioqueue, timer heap, etc.) */
    pj_memcpy(&tcp_sock->cfg, cfg, sizeof(*cfg));

#if 0
    /* Copy setting (QoS parameters etc */
    pj_memcpy(&tcp_sock->setting, setting, sizeof(*setting));
#endif
    /* Set callback */
    if (cb) {
	pj_memcpy(&tcp_sock->cb, cb, sizeof(*cb));
    }
    /* Create lock */
    status = pj_lock_create_recursive_mutex(pool, tcp_sock->obj_name, 
					    &tcp_sock->lock);
    if (status != PJ_SUCCESS) {
	destroy(tcp_sock);
	return status;
    }

    /* Init timer */
	pj_timer_entry_init(&tcp_sock->timer, TIMER_NONE, tcp_sock, &timer_cb);

	/* Init Server TCP session */
	pj_bzero(&sess_server_cb, sizeof(sess_server_cb));
	sess_server_cb.on_send_pkt = &tcp_on_send_pkt;
#if TCP_TODO
	sess_server_cb.on_channel_bound = &tcp_on_channel_bound;
#endif
#if 0
	stun = pj_ice_sess_get_stun_session(tcp_sock->user_data);
#else
	stun = NULL;
#endif
	sess_server_cb.on_rx_data = &tcp_on_rx_data;
	sess_server_cb.on_state = &tcp_sock_state;
	status = pj_tcp_session_create(cfg, "tcps%p", af,
		&sess_server_cb, 0, tcp_sock, stun, &tcp_sock->server_sess,
		-1, -1);
	if (status != PJ_SUCCESS) {
		destroy(tcp_sock);
		return status;
	}
    pj_tcp_session_set_stun_session_user_data(tcp_sock->server_sess, &tcp_sock->tsd);

	/* Init Server Accept TCP session */
	pj_bzero(&sess_accept_cb, sizeof(sess_accept_cb));
	sess_accept_cb.on_send_pkt = &tcp_on_send_pkt;
#if TCP_TODO
	sess_accept_cb.on_channel_bound = &tcp_on_channel_bound;
#endif
#if 0
	stun = pj_ice_sess_get_stun_session(tcp_sock->user_data);
#else
	stun = NULL;
#endif

	sess_accept_cb.on_rx_data = &tcp_on_rx_data;
	sess_accept_cb.on_state = &tcp_sock_state;

	{
		int i;
		for (i = 0; i < tcp_sock->accept_sess_cnt; i++) {
			if (tcp_sock->accept_sess)
			tcp_sock->accept_sess[i] = NULL;
		}
		tcp_sock->accept_sess_cnt = 0;
	}
    pj_tcp_session_set_stun_session_user_data(tcp_sock->server_sess, &tcp_sock->tsd);

    /* Note: socket and ioqueue will be created later once the TCP server
     * has been resolved.
     */
    *p_tcp_sock = tcp_sock;
    return PJ_SUCCESS;
}

/*
 * Destroy.
 */
static void destroy(pj_tcp_sock *tcp_sock)
{
    if (tcp_sock->lock) {
	pj_lock_acquire(tcp_sock->lock);
	}
	if (tcp_sock->server_sess) {
		pj_tcp_session_set_user_data(tcp_sock->server_sess, NULL);
		pj_tcp_session_shutdown(tcp_sock->server_sess);
		tcp_sock->server_sess = NULL;
	}

	{
		int i, count;
		for (i = 0; i < tcp_sock->accept_sess_cnt; i++) {
			if (tcp_sock->accept_sess[i]) {
				pj_tcp_session_set_user_data(tcp_sock->accept_sess[i], NULL);
				pj_tcp_session_shutdown(tcp_sock->accept_sess[i]);
				tcp_sock->accept_sess[i] = NULL;
				//tcp_sock->accept_sess_cnt--;
			}
			tcp_sock->accept_sess_cnt = 0;
		}

		count = tcp_sock->client_sess_cnt;
		for (i = 0; i < count; i++) {
			if (tcp_sock->client_sess[i]) {
				pj_tcp_session_set_user_data(tcp_sock->client_sess[i], NULL);
				pj_tcp_session_shutdown(tcp_sock->client_sess[i]);
				tcp_sock->client_sess[i] = NULL;
				//tcp_sock->client_sess_cnt--;
			}
			tcp_sock->client_sess_cnt = 0;
		}
	}

	if (tcp_sock->server_asock) {
		pj_activesock_close(tcp_sock->server_asock);
		tcp_sock->server_asock = NULL;
	}

    if (tcp_sock->lock) {
	pj_lock_release(tcp_sock->lock);
	pj_lock_destroy(tcp_sock->lock);
	tcp_sock->lock = NULL;
    }

    if (tcp_sock->pool) {
	pj_pool_t *pool = tcp_sock->pool;
	tcp_sock->pool = NULL;
	pj_pool_release(pool);
    }
}


PJ_DEF(void) pj_tcp_sock_destroy(pj_tcp_sock *tcp_sock)
{
	if (tcp_sock->lock) {
		pj_lock_acquire(tcp_sock->lock);
	}
    
    /* Destroy the active socket first just in case we'll get
     * stray callback.
	 */

	if (tcp_sock->server_sess) {
		pj_tcp_session_set_user_data(tcp_sock->server_sess, NULL);
		pj_tcp_session_shutdown(tcp_sock->server_sess);
		//pj_tcp_session_destroy(tcp_sock->server_sess, 0);
		tcp_sock->server_sess = NULL;
	}

	if (tcp_sock->server_asock != NULL) {
		pj_activesock_t	*asock = tcp_sock->server_asock;
		tcp_sock->server_asock = NULL;
		pj_activesock_set_user_data(asock, NULL);
		pj_activesock_close(asock);
	}

	{
		int i, count;
		count = tcp_sock->accept_sess_cnt;
		for (i = 0; i < count; i++) {
			if (tcp_sock->accept_sess[i]) {
				pj_activesock_t **asock = pj_tcp_session_get_asock(tcp_sock->accept_sess[i]);
				if (asock && *asock) {
					pj_activesock_close(*asock);
					pj_tcp_session_set_asock(tcp_sock->accept_sess[i], NULL);
				}
					
				pj_tcp_session_set_user_data(tcp_sock->accept_sess[i], NULL);
				pj_tcp_session_shutdown(tcp_sock->accept_sess[i]);
				tcp_sock->accept_sess[i] = NULL;
			}
		}
		tcp_sock->accept_sess_cnt = 0;

		count = tcp_sock->client_sess_cnt;
		for (i = 0; i < count; i++) {
			if (tcp_sock->client_sess[i]) {
				pj_activesock_t **asock = pj_tcp_session_get_asock(tcp_sock->client_sess[i]);
				if (asock && *asock) {
					pj_activesock_close(*asock);
					pj_tcp_session_set_asock(tcp_sock->client_sess[i], NULL);
				}
				pj_tcp_session_set_user_data(tcp_sock->client_sess[i], NULL);
				pj_tcp_session_shutdown(tcp_sock->client_sess[i]);
				tcp_sock->client_sess[i] = NULL;
			}
		}
		tcp_sock->client_sess_cnt = 0;
	}

	if (tcp_sock->lock) {
		pj_lock_release(tcp_sock->lock);
		pj_lock_destroy(tcp_sock->lock);
		tcp_sock->lock = NULL;
	}

    if (tcp_sock->pool) {
		pj_pool_t *pool = tcp_sock->pool;
		tcp_sock->pool = NULL;
		pj_pool_release(pool);
    }

}

PJ_DECL(void) pj_tcp_sock_reset_sess_state(pj_tcp_sock *tcp_sock)
{
	int i;
	for (i = 0; i < tcp_sock->accept_sess_cnt; i++)
	{
		if (tcp_sock->accept_sess[i]) {
			pj_tcp_session_set_state(tcp_sock->accept_sess[i], PJ_TCP_STATE_NULL);
		}
	}

	for (i = 0; i < tcp_sock->client_sess_cnt; i++)
	{
		if (tcp_sock->client_sess[i]) {
			pj_tcp_session_set_state(tcp_sock->client_sess[i], PJ_TCP_STATE_NULL);
		}
	}
}

/* Timer callback */
static void timer_cb(pj_timer_heap_t *th, pj_timer_entry *e)
{
    pj_tcp_sock *tcp_sock = (pj_tcp_sock*)e->user_data;
    int eid = e->id;

    PJ_UNUSED_ARG(th);

    e->id = TIMER_NONE;

    switch (eid) {
    case TIMER_DESTROY:
	PJ_LOG(4,(tcp_sock->obj_name, "Destroying TCP"));
	destroy(tcp_sock);
	break;
    default:
	pj_assert(!"Invalid timer id");
	break;
    }
}


/* Display error */
static void show_err(pj_tcp_session *tcp_sess, const char *title,
		     pj_status_t status)
{
    PJ_PERROR(5,(pj_tcp_session_get_object_name(tcp_sess), status, title));
}

/* On error, terminate session */
static void sess_fail(pj_tcp_session *tcp_sess, const char *title,
		      pj_status_t status)
{
	pj_tcp_sock *tcp_sock;
    show_err(tcp_sess, title, status);
    if (tcp_sess) {
		tcp_sock = pj_tcp_session_get_tcp_sock(tcp_sess);
		pj_tcp_session_destroy(tcp_sess, status);
		//tcp_sock->client_sess_cnt--;
    }
}

/*
 * Set user data.
 */
PJ_DEF(pj_status_t) pj_tcp_sock_set_user_data( pj_tcp_sock *tcp_sock,
					       void *user_data)
{
    PJ_ASSERT_RETURN(tcp_sock, PJ_EINVAL);
    tcp_sock->user_data = user_data;
    return PJ_SUCCESS;
}

/*
 * Get user data.
 */
PJ_DEF(void*) pj_tcp_sock_get_user_data(pj_tcp_sock *tcp_sock)
{
    PJ_ASSERT_RETURN(tcp_sock, NULL);
    return tcp_sock->user_data;
}

/*
 * Get tcp session.
 */
PJ_DEF(pj_tcp_session*) pj_tcp_sock_get_accept_session(pj_tcp_sock *tcp_sock)
{
	int i;
	for (i = 0; i < tcp_sock->accept_sess_cnt; i++)
	{
		if (tcp_sock->accept_sess[i])
			return tcp_sock->accept_sess[i];
	}
	return NULL;
}

/*
 * Get tcp session.
 */
PJ_DEF(pj_tcp_session*) pj_tcp_sock_get_accept_session_by_idx(pj_tcp_sock *tcp_sock, int tcp_sess_idx)
{
	return tcp_sock == NULL ? NULL : tcp_sock->accept_sess[tcp_sess_idx];
}

PJ_DEF(pj_tcp_session*) pj_tcp_sock_get_client_session_by_idx(pj_tcp_sock *tcp_sock, int tcp_sess_idx) 
{
	return tcp_sock == NULL ? NULL : tcp_sock->client_sess[tcp_sess_idx];
}

PJ_DEF(pj_tcp_session*) pj_tcp_sock_get_client_session(pj_tcp_sock *tcp_sock) 
{
	int i;
	for (i = 0; i < tcp_sock->client_sess_cnt; i++)
	{
		if (tcp_sock->client_sess[i])
			return tcp_sock->client_sess[i];
	}
	return NULL;	
}

/**
 * Get info.
 */
PJ_DEF(pj_status_t) pj_tcp_sock_get_info(pj_tcp_sock *tcp_sock,
					  pj_tcp_session_info *info)
{
    PJ_ASSERT_RETURN(tcp_sock && info, PJ_EINVAL);

    if (tcp_sock->server_sess) {
	return pj_tcp_session_get_info(tcp_sock->server_sess, info);
    } else {
	pj_bzero(info, sizeof(*info));
	info->state = PJ_TCP_STATE_NULL;
	return PJ_SUCCESS;
    }
}

/**
 * Lock the TCP socket. Application may need to call this function to
 * synchronize access to other objects to avoid deadlock.
 */
PJ_DEF(pj_status_t) pj_tcp_sock_lock(pj_tcp_sock *tcp_sock)
{
    return pj_lock_acquire(tcp_sock->lock);
}

/**
 * Unlock the TCP socket.
 */
PJ_DEF(pj_status_t) pj_tcp_sock_unlock(pj_tcp_sock *tcp_sock)
{
    return pj_lock_release(tcp_sock->lock);
}

/*
 * Send packet.
 */ 
PJ_DEF(pj_status_t) pj_tcp_sock_sendto( pj_tcp_sock *tcp_sock,
					const pj_uint8_t *pkt,
					unsigned pkt_len,
					const pj_sockaddr_t *addr,
					unsigned addr_len, 
					int tcp_sess_idx)
{
	PJ_ASSERT_RETURN(tcp_sock && addr && addr_len, PJ_EINVAL);

    if (tcp_sock == NULL)
	return PJ_EINVALIDOP;

	if (tcp_sock->client_sess_cnt > 0) {
		if (tcp_sess_idx == -1)
		{
			int i;
			for (i = 0; i < tcp_sock->client_sess_cnt; i++)
			{
				if (tcp_sock->client_sess[i] && 
					pj_tcp_session_get_state(tcp_sock->client_sess[i]) == PJ_TCP_STATE_READY)
						return pj_tcp_session_sendto(tcp_sock->client_sess[i], pkt, pkt_len, 
						addr, addr_len);
			}
		}
		else
		{
			if (tcp_sock->client_sess[tcp_sess_idx] && 
				pj_tcp_session_get_state(tcp_sock->client_sess[tcp_sess_idx]) == PJ_TCP_STATE_READY)
				return pj_tcp_session_sendto(tcp_sock->client_sess[tcp_sess_idx], pkt, pkt_len, 
				addr, addr_len);
		}
	} else {
		if (tcp_sess_idx == -1)
		{
			int i;
			for (i = 0; i < tcp_sock->accept_sess_cnt; i++) {
				if (tcp_sock->accept_sess[i])
					return pj_tcp_session_sendto(tcp_sock->accept_sess[i], pkt, pkt_len, 
					addr, addr_len);
			}
		}
		else
		{
			if (tcp_sock->accept_sess[tcp_sess_idx])
				return pj_tcp_session_sendto(tcp_sock->accept_sess[tcp_sess_idx], pkt, pkt_len, 
				addr, addr_len);
		}
	}
	return PJ_EINVALIDOP;
}

#if TCP_TODO
/*
 * Bind a peer address to a channel number.
 */
PJ_DEF(pj_status_t) pj_tcp_sock_bind_channel( pj_tcp_sock *tcp_sock,
					      const pj_sockaddr_t *peer,
					      unsigned addr_len)
{
    PJ_ASSERT_RETURN(tcp_sock && peer && addr_len, PJ_EINVAL);
    PJ_ASSERT_RETURN(tcp_sock->server_sess != NULL, PJ_EINVALIDOP);

    return pj_tcp_session_bind_channel(tcp_sock->server_sess, peer, addr_len);
}
#endif

/*
 * Notification when outgoing TCP socket has been connected.
 */
static pj_bool_t on_connect_complete(pj_activesock_t *asock,
				     pj_status_t status)
{
    pj_tcp_session *tcp_sess;

    tcp_sess = (pj_tcp_session *) pj_activesock_get_user_data(asock);

    if (status != PJ_SUCCESS) {
	sess_fail(tcp_sess, "on_connect_complete TCP connect() error", status);
	return PJ_FALSE;
    }

	PJ_LOG(5,(pj_tcp_session_get_object_name(tcp_sess), "TCP connected"));

    /* Kick start pending read operation */
    status = pj_activesock_start_read(asock, 
						pj_tcp_session_get_pool(tcp_sess), 
						PJ_TCP_MAX_PKT_LEN, 0);

#if TCP_TODO
    /* Init send_key */
    pj_ioqueue_op_key_init(&tcp_sess->send_key, sizeof(tcp_sess->send_key));
    /* Send Allocate request */
    status = pj_tcp_session_alloc(tcp_sess->server_sess, &tcp_sess->alloc_param);
    if (status != PJ_SUCCESS) {
	sess_fail(tcp_sess, "Error sending ALLOCATE", status);
	return PJ_FALSE;
    }
#endif
    return PJ_TRUE;
}

static pj_bool_t on_client_connect_complete(pj_activesock_t *asock,
											  pj_status_t status)
{
	pj_tcp_session *client_tcp_sess;
	pj_tcp_sock *tcp_sock;

	client_tcp_sess = (pj_tcp_session*) pj_activesock_get_user_data(asock);
	tcp_sock = (pj_tcp_sock *) pj_tcp_session_get_tcp_sock(client_tcp_sess);
	
	// if fail, destroy sess which was created before.
	if (status != PJ_SUCCESS) {
		int client_idx = pj_tcp_session_get_idx(client_tcp_sess);

		pj_activesock_t **asock = pj_tcp_session_get_asock(client_tcp_sess);
		if (asock && *asock) {
			pj_activesock_close(*asock);
			pj_tcp_session_set_asock(client_tcp_sess, NULL);
		}

		sess_fail(client_tcp_sess, "on_client_connect_complete TCP connect() error", status);
		PJ_LOG(4,(pj_tcp_session_get_object_name(client_tcp_sess), "on_client_connect_complete TCP connect() error=[%d]", status));
		tcp_sock->client_sess[client_idx] = NULL;
		//tcp_sock->client_sess_cnt--;
		return PJ_FALSE;
	}

	PJ_LOG(4,(pj_tcp_session_get_object_name(client_tcp_sess), "TCP connected"));

	/* Kick start pending read operation */
	status = pj_activesock_start_read(asock, 
							pj_tcp_session_get_pool(client_tcp_sess), 
							PJ_TCP_MAX_PKT_LEN, 0);
	
	pj_tcp_session_set_state(client_tcp_sess, PJ_TCP_STATE_READY);

	//pj_ice_strans_set_check_tcp_session_ready(tcp_sock->user_data, 
	//						pj_tcp_session_get_idx(client_tcp_sess), PJ_TRUE);

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
static unsigned has_packet(pj_tcp_session *tcp_sess, const void *buf, pj_size_t bufsize)
{
    pj_bool_t is_stun = PJ_FALSE;
	pj_uint16_t data_len;
	pj_status_t stun_check;

	pj_tcp_sock *tcp_sock = (pj_tcp_sock *)pj_tcp_session_get_user_data(tcp_sess);

    /* Quickly check if this is STUN message, by checking the first two bits and
     * size field which must be multiple of 4 bytes
	 */
	/* Quickly check if this is STUN message */
	//is_stun = (((pj_uint16_t*)buf)[0] != NATNL_UDT_HEADER_MAGIC);
	stun_check = pj_stun_msg_check((const pj_uint8_t*)buf, bufsize, 
		/*PJ_STUN_IS_DATAGRAM | PJ_STUN_CHECK_PACKET*/ 0);

	if (bufsize == 218)
		printf("");

	if (stun_check == PJ_SUCCESS) 
		is_stun = PJ_TRUE;

	if (is_stun) {
		pj_size_t msg_len;

		PJ_LOG(5, (pj_tcp_session_get_object_name(tcp_sess), "has_packet is_stun, %x", 
			((pj_uint32_t*)buf)[0]));

		msg_len = GETVAL16H((const pj_uint8_t*)buf, 2);


		return (msg_len+sizeof(pj_stun_msg_hdr) <= bufsize) ? 
			msg_len+sizeof(pj_stun_msg_hdr) : 0;
	} else {
		PJ_LOG(5, (pj_tcp_session_get_object_name(tcp_sess), "has_packet not_stun, %x", 
			((pj_uint32_t*)buf)[0]));

		if (((pj_uint16_t*)buf)[0] != NATNL_UDT_HEADER_MAGIC) {
			if (bufsize < NATNL_DTLS_HEADER_SIZE)
				return 0;

			data_len = pj_ntohs(((pj_uint16_t*)(((pj_uint8_t*)buf)+11))[0]);
			if (bufsize >= (data_len + NATNL_DTLS_HEADER_SIZE)) 
				return data_len + NATNL_DTLS_HEADER_SIZE;
			else
				return 0;
		} else {
			if (bufsize < NATNL_UDT_HEADER_SIZE)
				return 0;

			data_len = pj_ntohs(((pj_uint16_t*)buf)[1]);
			if (bufsize >= (data_len + NATNL_UDT_HEADER_SIZE)) 
				return data_len + NATNL_UDT_HEADER_SIZE;
			else
				return 0;
		}
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
	pj_tcp_sock *tcp_sock;
    pj_tcp_session *tcp_sess;
    pj_bool_t ret = PJ_TRUE;
	//transport_ice *tp_ice;

    tcp_sess = (pj_tcp_session*) pj_activesock_get_user_data(asock);
	tcp_sock = pj_tcp_session_get_tcp_sock(tcp_sess);
	
	pj_lock_acquire(tcp_sock->lock);

    if (status == PJ_SUCCESS && tcp_sess) {
		//tcp_sock = (pj_tcp_sock *)tcp_sock->user_data;
	/* Report incoming packet to TCP session, repeat while we have
	 * "packet" in the buffer (required for stream-oriented transports)
	 */
		unsigned pkt_len;

		PJ_LOG(5, (pj_tcp_session_get_object_name(tcp_sess), 
			"on_data_read() entered size=[%d], remainder=[%d]", 
			size, *remainder));

	//PJ_LOG(5,(tcp_sock->pool->obj_name, 
	//	  "Incoming data, %lu bytes total buffer", size));

	while ((pkt_len=has_packet(tcp_sess, data, size)) != 0) {
	    pj_size_t parsed_len;
	    const pj_uint8_t *pkt = (const pj_uint8_t*)data;

	    PJ_LOG(5,(tcp_sock->pool->obj_name, 
	    	      "Packet start: %02X %02X %02X %02X", 
	    	      pkt[0], pkt[1], pkt[2], pkt[3]));

	    PJ_LOG(5,(tcp_sock->pool->obj_name, 
	    	      "Processing %lu bytes packet of %lu bytes total buffer",
	    	      pkt_len, size));

	    parsed_len = (unsigned)size;

	    pj_tcp_session_on_rx_pkt(tcp_sess, data,  size, &parsed_len);

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
		PJ_LOG(5, (pj_tcp_session_get_object_name(tcp_sess), 
			"on_data_read() size=[%d], pkt_len=[%d], "
			"parsed_len=[%d], *remainder=[%d]", 
			size, pkt_len, parsed_len, *remainder));
	    size = *remainder;

	    //PJ_LOG(5,(tcp_sock->pool->obj_name, 
	    //	      "Buffer size now %lu bytes", size));
	}
	/* Assigned remainder as size. Because ioqueue may 
	   skip the packet if never enter while loop. */
	if (pkt_len == 0)
		*remainder = size;

	PJ_LOG(5, (pj_tcp_session_get_object_name(tcp_sess), 
		"on_data_read() leaving still remainder=[%d].", 
		*remainder));
    } else if (status != PJ_SUCCESS) 
    {
		if (tcp_sess) {
			pj_tcp_session_set_partial_destroy(tcp_sess, PJ_TRUE);
			//don't do this here, tcp sess destroy will be execute at destroy_ice_st()
			//sess_fail(tcp_sess, "TCP connection closed", status);
		}
		ret = PJ_FALSE;
		goto on_return;
    }

on_return:
    pj_lock_release(tcp_sock->lock);

    return ret;
}

/* 
 * Callback from ioqueue when packet is sent.
 */
static pj_bool_t on_data_sent(pj_activesock_t *asock,
			      pj_ioqueue_op_key_t *op_key,
			      pj_ssize_t bytes_sent)
{
	PJ_UNUSED_ARG(asock);
	PJ_UNUSED_ARG(op_key);
	PJ_UNUSED_ARG(bytes_sent);
#if 0
    struct tcp_transport *tcp = (struct tcp_transport*) 
    				pj_activesock_get_user_data(asock);
    pjsip_tx_data_op_key *tdata_op_key = (pjsip_tx_data_op_key*)op_key;

    /* Note that op_key may be the op_key from keep-alive, thus
     * it will not have tdata etc.
     */

    tdata_op_key->tdata = NULL;

    if (tdata_op_key->callback) {
	/*
	 * Notify sip_transport.c that packet has been sent.
	 */
	if (bytes_sent == 0)
	    bytes_sent = -PJ_RETURN_OS_ERROR(OSERR_ENOTCONN);

	tdata_op_key->callback(&tcp->base, tdata_op_key->token, bytes_sent);

	/* Mark last activity time */
	pj_gettimeofday(&tcp->last_activity);

    }

    /* Check for error/closure */
    if (bytes_sent <= 0) {
	pj_status_t status;

	PJ_LOG(5,(tcp->base.obj_name, "TCP send() error, sent=%d", 
		  bytes_sent));

	status = (bytes_sent == 0) ? PJ_RETURN_OS_ERROR(OSERR_ENOTCONN) :
				     -bytes_sent;

	tcp_init_shutdown(tcp, status);

	return PJ_FALSE;
    }
#endif
    return PJ_TRUE;
}

/*
 * Callback from TCP session to send outgoing packet.
 */
static pj_status_t tcp_on_send_pkt(pj_tcp_session *sess,
				    const pj_uint8_t *pkt,
				    unsigned pkt_len,
				    const pj_sockaddr_t *dst_addr,
				    unsigned dst_addr_len)
{
    pj_tcp_sock *tcp_sock = (pj_tcp_sock*) 
			      pj_tcp_session_get_user_data(sess);
    pj_ssize_t len = pkt_len;
	pj_status_t status;
	pj_bool_t is_tnl_data = PJ_FALSE;
	pj_activesock_t **asock;

	asock = (pj_activesock_t **)pj_tcp_session_get_asock(sess);

    if (tcp_sock == NULL) {
	/* We've been destroyed */
	// https://trac.pjsip.org/repos/ticket/1316
	//pj_assert(!"We should shutdown gracefully");
	return PJ_EINVALIDOP;
    }

    PJ_UNUSED_ARG(dst_addr);
    PJ_UNUSED_ARG(dst_addr_len);

	// check if the pkt is udptunnel control packet
	// if true, prepare op_key and urgent flag
	is_tnl_data = (pkt_len >= TCP_OR_UDP_TOTAL_HEADER_SIZE && 
		((pj_uint16_t*)pkt)[0] == NATNL_UDT_HEADER_MAGIC && 
		(pkt[TCP_SESS_MSG_TYPE_INDEX] == 5 || pkt[TCP_SESS_MSG_TYPE_INDEX] == 6));

	if (!is_tnl_data) { 
			pj_ioqueue_op_key_t *op_key = (pj_ioqueue_op_key_t*)malloc(sizeof(pj_ioqueue_op_key_t));
			pj_ioqueue_op_key_init(op_key, sizeof(pj_ioqueue_op_key_t));
			status = pj_activesock_send(*asock, op_key,
				pkt, &len, PJ_IOQUEUE_URGENT_DATA);
	} else {
		status = pj_activesock_send(*asock, &tcp_sock->send_key,
			pkt, &len, 0);
	}

    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	show_err(sess, "socket send()", status);
    }

    return status;
}

#if TCP_TODO
/*
 * Callback from TCP session when a channel is successfully bound.
 */
static void tcp_on_channel_bound(pj_tcp_session *sess,
				  const pj_sockaddr_t *peer_addr,
				  unsigned addr_len,
				  unsigned ch_num)
{
    PJ_UNUSED_ARG(sess);
    PJ_UNUSED_ARG(peer_addr);
    PJ_UNUSED_ARG(addr_len);
    PJ_UNUSED_ARG(ch_num);
}
#endif

/*
 * Callback from TCP session upon incoming data.
 */
static void tcp_on_rx_data(pj_tcp_session *sess,
			    void *pkt,
			    unsigned pkt_len,
			    const pj_sockaddr_t *peer_addr,
			    unsigned addr_len)
{
    pj_tcp_sock *tcp_sock = (pj_tcp_sock*) 
			   pj_tcp_session_get_user_data(sess);
	char buf[PJ_INET6_ADDRSTRLEN+20];
    if (tcp_sock == NULL) {
	/* We've been destroyed */
	return;
	}

	PJ_LOG(5, (pj_tcp_session_get_object_name(sess), "tcp_on_rx_data() from %s",
		peer_addr == NULL ? "NULL" : pj_sockaddr_print(peer_addr, buf, PJ_INET6_ADDRSTRLEN, 3)));

    if (tcp_sock->cb.on_rx_data) {
	(*tcp_sock->cb.on_rx_data)(sess, pkt, pkt_len,
				  peer_addr, addr_len);
    }
}


/*
 * Callback from TCP session when state has changed
 */
static void tcp_sock_state(pj_tcp_session *sess, 
			  pj_tcp_state_t old_state,
			  pj_tcp_state_t new_state)
{
    pj_tcp_sock *tcp_sock = (pj_tcp_sock*) 
			   pj_tcp_session_get_user_data(sess);

    if (tcp_sock == NULL) {
	/* We've been destroyed */
	return;
    }

    /* Notify app first */
    if (tcp_sock->cb.on_state) {
	(*tcp_sock->cb.on_state)(tcp_sock, old_state, new_state);
    }

    /* Make sure user hasn't destroyed us in the callback */
    if (sess && new_state == PJ_TCP_STATE_RESOLVED) {
	pj_tcp_session_info info;
	pj_tcp_session_get_info(sess, &info);
	new_state = info.state;
    }
    if (new_state >= PJ_TCP_STATE_DESTROYING && sess) {
#if 0
	pj_time_val delay = {0, 0};
#endif

	pj_tcp_session_set_user_data(sess, NULL);
	sess = NULL;

	if (tcp_sock->timer.id) {
	    pj_timer_heap_cancel(tcp_sock->cfg.timer_heap, &tcp_sock->timer);
	    tcp_sock->timer.id = 0;
	}
#if 0
	tcp_sock->timer.id = TIMER_DESTROY;
	pj_timer_heap_schedule(tcp_sock->cfg.timer_heap, &tcp_sock->timer, 
			       &delay);
#endif
    }
}

static void dump_bin(const char *buf, unsigned len)
{
	unsigned i;
	if (len > 64)
		len = 64;
	PJ_LOG(3,("tcp_sock.c", "begin dump"));
	for (i=0; i<len; ++i) {
		int j;
		char bits[9];
		unsigned val = buf[i] & 0xFF;

		bits[8] = '\0';
		for (j=0; j<8; ++j) {
			if (val & (1 << (7-j)))
				bits[j] = '1';
			else
				bits[j] = '0';
		}

		PJ_LOG(3,("tcp_sock.c", "%2d %s [%d]", i, bits, val));
	}
	PJ_LOG(3,("tcp_sock.c", "end dump"));
}

/*
 * Callback from TCP session to send outgoing packet.
 */
static pj_status_t tcp_client_on_send_pkt(pj_tcp_session *sess,
				    const pj_uint8_t *pkt,
				    unsigned pkt_len,
				    const pj_sockaddr_t *dst_addr,
				    unsigned dst_addr_len)
{
    pj_tcp_sock *tcp_sock = (pj_tcp_sock*) 
			      pj_tcp_session_get_user_data(sess);
    pj_ssize_t len = pkt_len;
    pj_status_t status;
	pj_bool_t is_tnl_data = PJ_FALSE;
	pj_activesock_t **asock = (pj_activesock_t **)pj_tcp_session_get_asock(sess);

    if (tcp_sock == NULL) {
	/* We've been destroyed */
	// https://trac.pjsip.org/repos/ticket/1316
	//pj_assert(!"We should shutdown gracefully");
	return PJ_EINVALIDOP;
    }

    PJ_UNUSED_ARG(dst_addr);
	PJ_UNUSED_ARG(dst_addr_len);

	// check if the pkt is udptunnel control packet
	// if true, prepare op_key and urgent flag
	is_tnl_data = (pkt_len >= TCP_OR_UDP_TOTAL_HEADER_SIZE && 
		((pj_uint16_t*)pkt)[0] == NATNL_UDT_HEADER_MAGIC && 
		(pkt[TCP_SESS_MSG_TYPE_INDEX] == 5 || pkt[TCP_SESS_MSG_TYPE_INDEX] == 6));
	//dump_bin(pkt, len);
	if (!is_tnl_data) { 
		pj_ioqueue_op_key_t *op_key = (pj_ioqueue_op_key_t*)malloc(sizeof(pj_ioqueue_op_key_t));
		pj_ioqueue_op_key_init(op_key, sizeof(pj_ioqueue_op_key_t));
		status = pj_activesock_send(*asock, op_key,
			pkt, &len, PJ_IOQUEUE_URGENT_DATA);
	} else {
		status = pj_activesock_send(*asock, &tcp_sock->send_key,
			pkt, &len, 0);
	}

    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	show_err(sess, "socket send()", status);
    }

    return status;
}

/*
 * Callback from TCP session upon incoming data.
 */
static void tcp_client_on_rx_data(pj_tcp_session *sess,
			    void *pkt,
			    unsigned pkt_len,
			    const pj_sockaddr_t *peer_addr,
			    unsigned addr_len)
{
    pj_tcp_sock *tcp_sock = (pj_tcp_sock*) 
			   pj_tcp_session_get_user_data(sess);
	char buf[PJ_INET6_ADDRSTRLEN+20];
    if (tcp_sock == NULL) {
	/* We've been destroyed */
	return;
	}

	PJ_LOG(4, (pj_tcp_session_get_object_name(sess), "tcp_client_on_rx_data() from %s",
		peer_addr == NULL ? "NULL" : pj_sockaddr_print(peer_addr, buf, PJ_INET6_ADDRSTRLEN, 3)));

    if (tcp_sock->cb.on_rx_data) {
	(*tcp_sock->cb.on_rx_data)(tcp_sock, pkt, pkt_len,
				  peer_addr, addr_len);
    }
}


/*
 * Callback from TCP session when state has changed
 */
static void tcp_client_on_state(pj_tcp_session *sess, 
			  pj_tcp_state_t old_state,
			  pj_tcp_state_t new_state)
{
    pj_tcp_sock *tcp_sock = (pj_tcp_sock*) 
			   pj_tcp_session_get_user_data(sess);

    if (tcp_sock == NULL) {
	/* We've been destroyed */
	return;
    }

    /* Notify app first */
    if (tcp_sock->cb.on_state) {
	(*tcp_sock->cb.on_state)(tcp_sock, old_state, new_state);
    }

    /* Make sure user hasn't destroyed us in the callback */
    if (sess && new_state == PJ_TCP_STATE_RESOLVED) {
	pj_tcp_session_info info;
	pj_tcp_session_get_info(sess, &info);
	new_state = info.state;
    }
	if (new_state >= PJ_TCP_STATE_DESTROYING && sess) {
#if 0
		pj_time_val delay = {0, 0};
#endif

	pj_tcp_session_set_user_data(sess, NULL);
	sess = NULL;

	if (tcp_sock->timer.id) {
	    pj_timer_heap_cancel(tcp_sock->cfg.timer_heap, &tcp_sock->timer);
	    tcp_sock->timer.id = 0;
	}

#if 0
	tcp_sock->timer.id = TIMER_DESTROY;
	pj_timer_heap_schedule(tcp_sock->cfg.timer_heap, &tcp_sock->timer, 
			       &delay);
#endif
    }
}


/*
 * This utility function creates receive data buffers and start
 * asynchronous recv() operations from the socket. It is called after
 * accept() or connect() operation complete.
 */
static pj_status_t tcp_start_read(pj_tcp_sock *tcp_sock)
{
    pj_pool_t *pool;
    pj_status_t status;
	pj_activesock_t **asock = (pj_activesock_t **)pj_tcp_session_get_asock(tcp_sock->accept_sess[tcp_sock->accept_sess_cnt-1]);
	pool = tcp_sock->pool;

	status = pj_activesock_start_read(*asock, 
		tcp_sock->pool, PJ_TCP_MAX_PKT_LEN, 0);
    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
	PJ_LOG(4, (tcp_sock->obj_name, 
		   "pj_activesock_start_read() error, status=%d", 
		   status));
	return status;
    }

    return PJ_SUCCESS;
}

/*
 * Common function to create TCP transport, called when pending accept() and
 * pending connect() complete.
 */
static pj_status_t tcp_create( pj_tcp_sock *tcp_sock,
			       pj_pool_t *pool,
			       pj_sock_t sock, pj_bool_t is_server,
			       const pj_sockaddr_in *local,
			       const pj_sockaddr_in *remote)
{
    pj_ioqueue_t *ioqueue;
    pj_activesock_cfg asock_cfg;
    pj_activesock_cb tcp_callback;
    pj_status_t status;
	pj_activesock_t **asock;

    PJ_ASSERT_RETURN(pool, PJ_EINVAL);
    PJ_ASSERT_RETURN(sock != PJ_INVALID_SOCKET, PJ_EINVAL);

	PJ_UNUSED_ARG(is_server);
	PJ_UNUSED_ARG(local);
	PJ_UNUSED_ARG(remote);

    /* Create active socket */
    pj_activesock_cfg_default(&asock_cfg);
    asock_cfg.async_cnt = 1;
	asock_cfg.whole_data = PJ_TRUE;
	asock_cfg.concurrency = 1;

	pj_bzero(&tcp_callback, sizeof(tcp_callback));

	tcp_callback.on_data_read = &on_data_read;
    tcp_callback.on_data_sent = &on_data_sent;
    tcp_callback.on_connect_complete = &on_connect_complete;

	ioqueue = tcp_sock->cfg.ioqueue;
	asock = (pj_activesock_t **)pj_tcp_session_get_asock(tcp_sock->accept_sess[tcp_sock->accept_sess_cnt-1]);
    status = pj_activesock_create(pool, sock, pj_SOCK_STREAM(), &asock_cfg,
				  ioqueue, &tcp_callback, 
				  tcp_sock->accept_sess[tcp_sock->accept_sess_cnt-1], 
				  asock);
    if (status != PJ_SUCCESS) {
		goto on_error;
	}

    return PJ_SUCCESS;

on_error:
    //tcp_destroy(&tcp->base, status);
    return status;
}

/*
 * This callback is called by active socket when pending accept() operation
 * has completed.
 */
static pj_bool_t on_accept_complete(pj_activesock_t *asock,
				    pj_sock_t sock,
				    const pj_sockaddr_t *src_addr,
				    int src_addr_len)
{
	pj_tcp_sock *tcp_sock;
    pj_tcp_session *accept_tcp_sess;
	char addr[PJ_INET6_ADDRSTRLEN+10];
	char addr1[PJ_INET6_ADDRSTRLEN+10];
    //pjsip_tp_state_callback state_cb;
    pj_status_t status;
	int sess_idx;

	pj_sockaddr peer_addr;
	int rem_addr_len = sizeof(peer_addr);
	pj_sockaddr local_addr;
	int local_addr_len = sizeof(local_addr);

	status = pj_sock_getpeername(sock, &peer_addr, &rem_addr_len);
	status = pj_sock_getsockname(sock, &local_addr, &local_addr_len);

    PJ_UNUSED_ARG(src_addr_len);

    accept_tcp_sess = (struct pj_tcp_session*) pj_activesock_get_user_data(asock);
	tcp_sock = (struct pj_tcp_sock *) pj_tcp_session_get_user_data(accept_tcp_sess);	

	{   // DEAN create server_acpt_sess
		pj_tcp_session_cb sess_accept_cb;//, sess_client_cb;
		pj_stun_session *stun = NULL;

		if (tcp_sock->accept_sess_cnt >= MAX_TCP_ACCEPT_SESS) {
			PJ_LOG(3, ("tcp_sock.c", "!!!!!!!!!!!!! on_accept_complete() exceed maximum number[%d] of tcp accept session.", 
				MAX_TCP_ACCEPT_SESS));
			pj_activesock_close(asock);
			return PJ_ETOOMANYCONN;
		}
		
		sess_idx = tcp_sock->accept_sess_cnt++;
		pj_bzero(&sess_accept_cb, sizeof(sess_accept_cb));
		sess_accept_cb.on_send_pkt = &tcp_on_send_pkt;
		sess_accept_cb.on_rx_data = &tcp_on_rx_data;
		sess_accept_cb.on_state = &tcp_sock_state;
		status = pj_tcp_session_create(&tcp_sock->cfg, "tcpa%p", tcp_sock->af,
			&sess_accept_cb, 0, tcp_sock, stun, 
			&tcp_sock->accept_sess[sess_idx],
			sess_idx, -1); //DEAN TODO
 	}

    PJ_ASSERT_RETURN(sock != PJ_INVALID_SOCKET, PJ_TRUE);

	PJ_LOG(4,(pj_tcp_session_get_object_name(accept_tcp_sess), 
	      "TCP listener: got incoming TCP connection "
	      "from %s to %s, sock=%d",
	      pj_sockaddr_print(&peer_addr, addr, sizeof(addr), 3),
		  pj_sockaddr_print(&local_addr, addr1, sizeof(addr1), 3),
		  sock));
#if 0
    /* Apply QoS, if specified */
    status = pj_sock_apply_qos2(sock, listener->qos_type, 
				&listener->qos_params, 
				2, listener->factory.obj_name, 
				"incoming SIP TCP socket");
#endif
    /* 
     * Incoming connection!
     * Create TCP transport for the new socket.
     */
    status = tcp_create( tcp_sock, pj_tcp_session_get_pool(accept_tcp_sess), sock, PJ_TRUE,
			 (const pj_sockaddr_in*)tcp_sock->local_addr,
			 (const pj_sockaddr_in*)src_addr);
    if (status == PJ_SUCCESS) {
		status = tcp_start_read(tcp_sock);

		pj_tcp_session_set_peer_addr(tcp_sock->accept_sess[sess_idx], &peer_addr);
		pj_tcp_session_set_local_addr(tcp_sock->accept_sess[sess_idx], &local_addr);
		
		if (status != PJ_SUCCESS) {
			PJ_LOG(3,(pj_tcp_session_get_object_name(accept_tcp_sess), "New transport cancelled"));
			//tcp_destroy(&tcp->base, status);
		} else {
			pj_tcp_session_set_state(tcp_sock->accept_sess[sess_idx], PJ_TCP_STATE_READY);
		}
	}

	// 2013-09-17 DEAN, must do this becuase we start tcp server at adding candidate moment.
	pj_tcp_sock_set_stun_session_user_data(tcp_sock, 
		pj_ice_strans_get_ice_strans(tcp_sock->user_data), 
		pj_ice_strans_get_comp_id(tcp_sock->user_data));

	// 2013-09-23 DEAN, directly nominated this tcp session and do ice complete
	pj_ice_strans_update_or_add_incoming_check(tcp_sock->user_data, &local_addr, &peer_addr, sess_idx);

    return PJ_TRUE;
}

PJ_DECL(pj_status_t) tcp_sock_create_server(pj_pool_t *pool, 
											 pj_tcp_sock *tcp_sock, 
											 pj_sockaddr *tcp_external_addr,
											 pj_sockaddr *tcp_local_addr,
											 int system_alloc_port)
{
    //pj_pool_t *pool;
    pj_sock_t sock = PJ_INVALID_SOCKET;
	pj_status_t status;
	pj_sockaddr *listener_addr;
    pj_activesock_cfg asock_cfg;
    pj_activesock_cb listener_cb;
	char buf[PJ_INET6_ADDRSTRLEN+10];

	char buf3[PJ_INET6_ADDRSTRLEN+10];

	PJ_LOG(3,(THIS_FILE, "tcp_sock_create_server() local address is [%s]",
		pj_sockaddr_print(tcp_local_addr, buf3, sizeof(buf3), 3)));

    /* Create socket */
    status = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, &sock);
	if (status != PJ_SUCCESS) {
		PJ_LOG(1, (THIS_FILE, "tcp_sock_create_server() Failed to create socket. status=[%d]", status));
		goto on_error;
	}

	// set socket options
	{
		int opt = 1;
		int flag;
		status = pj_sock_setsockopt(sock, pj_SOL_SOCKET(), 
			pj_SO_REUSEADDR(), &opt, sizeof(int));
		if (status != PJ_SUCCESS) {
			PJ_LOG(1, (THIS_FILE, "tcp_sock_create_server() Failed to set SO_REUSEADDR option. status=[%d]", status));
			goto on_error;
		}

		flag = 1;
		status = pj_sock_setsockopt(sock, pj_SOL_TCP(), pj_TCP_NODELAY(), &flag, sizeof(flag));
		if (status != PJ_SUCCESS) {
			PJ_LOG(1, (THIS_FILE, "tcp_sock_create_server() Failed to set TCP_NODELAY option. status=[%d]", status));
			goto on_error;
		}

		flag = PJ_TCP_MAX_PKT_LEN;
		
		status = pj_sock_setsockopt(sock, pj_SOL_SOCKET(), pj_SO_RCVBUF(), &flag, sizeof(flag));
		if (status != PJ_SUCCESS) {
			PJ_LOG(1, (THIS_FILE, "tcp_sock_create_server() Failed to set SO_RCVBUF option. status=[%d]", status));
			goto on_error;
		}

		flag = PJ_SOCKET_SND_BUFFER_SIZE;
		status = pj_sock_setsockopt(sock, pj_SOL_SOCKET(), pj_SO_SNDBUF(), &flag, sizeof(flag));		
		if (status != PJ_SUCCESS) {
			PJ_LOG(1, (THIS_FILE, "tcp_sock_create_server() Failed to set SO_SNDBUF option. status=[%d]", status));
			goto on_error;
		}
	}

    /* Bind socket */
	listener_addr = tcp_sock->local_addr;
	pj_sockaddr_cp(listener_addr, tcp_local_addr);
	if (system_alloc_port)
		pj_sockaddr_set_port(listener_addr, 0); // bind to any port
	listener_addr->ipv4.sin_addr.s_addr = PJ_INADDR_ANY;

    status = pj_sock_bind(sock, listener_addr, 
			  pj_sockaddr_get_len(listener_addr));
	if (status != PJ_SUCCESS) {
		PJ_LOG(1, (THIS_FILE, "tcp_sock_create_server() Failed to bind to address. status=[%d]", status));
		goto on_error;
	}

	{
		pj_sockaddr_in addr;
		int addr_len = sizeof(addr);
		pj_uint16_t local_port;
		if (pj_sock_getsockname(sock, &addr, &addr_len) == PJ_SUCCESS)
		{
			char buf1[PJ_INET6_ADDRSTRLEN+10];
			char buf2[PJ_INET6_ADDRSTRLEN+10];

			// We just need to update local port.
			local_port = pj_sockaddr_get_port(&addr);
			pj_sockaddr_set_port(listener_addr, local_port);
			pj_sockaddr_set_port(tcp_local_addr, local_port);

			PJ_LOG(3,("tcp_sock.c", "tcp_server listen on %s",
				pj_sockaddr_print(&addr, buf1, sizeof(buf1), 3)));

			PJ_LOG(3,("tcp_sock.c", "local addr is %s",
				pj_sockaddr_print(tcp_local_addr, buf2, sizeof(buf2), 3)));
		}

		// Try to setup UPnP now. Because we must wait for stun mapped address.
#if 1
		if (system_alloc_port && tcp_sock->cb.on_tcp_server_binding_complete) {
			status = (*tcp_sock->cb.on_tcp_server_binding_complete)(tcp_sock, 
				tcp_external_addr,
				tcp_local_addr);
			if (status != PJ_SUCCESS)
			{
				if (sock != PJ_INVALID_SOCKET)
					pj_sock_close(sock);

				goto on_error;
			}
		}
#endif
	}

    /* Start listening to the address */
    status = pj_sock_listen(sock, 5/*PJSIP_TCP_TRANSPORT_BACKLOG*/);
	if (status != PJ_SUCCESS) {
		PJ_LOG(1, (THIS_FILE, "tcp_sock_create_server() Failed to start listening. status=[%d]", status));
		goto on_error;
	}

    /* Create active socket */
	pj_activesock_cfg_default(&asock_cfg);
	asock_cfg.whole_data = PJ_TRUE;
	asock_cfg.concurrency = 1;

    pj_bzero(&listener_cb, sizeof(listener_cb));
    listener_cb.on_accept_complete = &on_accept_complete;
    status = pj_activesock_create(pool, sock, pj_SOCK_STREAM(), &asock_cfg,
				  tcp_sock->cfg.ioqueue, 
				  &listener_cb, 
				  tcp_sock->server_sess,
				  &tcp_sock->server_asock);
    /* Start pending accept() operations */
    status = pj_activesock_start_accept(tcp_sock->server_asock, pool);
	if (status != PJ_SUCCESS) {
		PJ_LOG(1, (THIS_FILE, "tcp_sock_create_server() Failed to start to accept incoming connections. status=[%d]", status));
		goto on_error;
	}

    PJ_LOG(4,(tcp_sock->obj_name, 
	     "TCP server is ready for incoming connections at [%s].",
		 pj_sockaddr_print(tcp_sock->local_addr, buf, sizeof(buf), 3)));

on_error:
	return status;
}

PJ_DECL(pj_status_t) tcp_sock_make_connection(pj_stun_config *cfg,
											 int af, pj_tcp_sock *tcp_sock,
											 const pj_sockaddr_t *addr,
											 unsigned addr_len,
											 int *tcp_sess_idx,
											 int check_idx)
{
	pj_status_t status;
	char buf[PJ_INET6_ADDRSTRLEN+20];
	pj_tcp_session_cb sess_client_cb;
	pj_stun_session *stun = NULL;
	pj_activesock_cfg asock_cfg;

	int sess_idx;

	if (tcp_sock->client_sess_cnt >= MAX_TCP_CLIENT_SESS) {
		PJ_LOG(2, (THIS_FILE, "tcp_sock_make_connection() Exceed maximum number[%d] of tcp client session.", 
			MAX_TCP_CLIENT_SESS));
		return PJ_SUCCESS;
	}

	sess_idx = tcp_sock->client_sess_cnt++;

	pj_bzero(&sess_client_cb, sizeof(sess_client_cb));
	sess_client_cb.on_send_pkt = &tcp_client_on_send_pkt;
#if TCP_TODO
	sess_client_cb.on_chanq
		nel_bound = &tcp_on_channel_bound;
#endif
	sess_client_cb.on_rx_data = &tcp_on_rx_data;
	sess_client_cb.on_state = &tcp_client_on_state;

	status = pj_tcp_session_create(cfg, "tcpce%p", af,
		&sess_client_cb, 0, tcp_sock, stun, &tcp_sock->client_sess[sess_idx],
		sess_idx, check_idx);
	if (status != PJ_SUCCESS) {
		destroy(tcp_sock);
		return status;
	}
	*tcp_sess_idx = sess_idx;
	pj_tcp_session_set_stun_session_user_data(tcp_sock->client_sess[sess_idx], &tcp_sock->tsd);

	pj_tcp_session_set_peer_addr(tcp_sock->client_sess[sess_idx], addr);

	PJ_LOG(4, (pj_tcp_session_get_object_name(tcp_sock->client_sess[sess_idx]), 
		"tcp_sock_make_connection() remote=[%s]", pj_sockaddr_print(addr, buf, sizeof(buf), 3)));

	{
		long flag;
		int flaglen;
		/* client active socket */
		pj_sock_t client_sock = PJ_INVALID_SOCKET;
		pj_activesock_cb asock_cb;

		/* Close existing connection, if any. This happens when
		 * we're switching to alternate TCP server when either TCP
		 * connection or ALLOCATE request failed.
		 */
		/* Init socket */
		status = pj_sock_socket(tcp_sock->af, pj_SOCK_STREAM(), 0, &client_sock);
		if (status != PJ_SUCCESS) {
			pj_tcp_sock_destroy(tcp_sock);
			PJ_LOG(2, (THIS_FILE, "tcp_sock_make_connection() Failed to create socket. status=[%d]", status));
			return status;
		}


		flag = 1;
		status = pj_sock_setsockopt(client_sock, pj_SOL_TCP(), pj_TCP_NODELAY(),
			&flag, sizeof(flag));
		if (status != PJ_SUCCESS) {
			PJ_LOG(2, (THIS_FILE, "tcp_sock_make_connection() Failed to set TCP_NODELAY option. status=[%d]", status));
			return status;
		}

		flag = PJ_TCP_MAX_PKT_LEN;
		status = pj_sock_setsockopt(client_sock, pj_SOL_SOCKET(), pj_SO_RCVBUF(),
			&flag, sizeof(flag));
		if (status != PJ_SUCCESS) {
			PJ_LOG(2, (THIS_FILE, "tcp_sock_make_connection() Failed to set SO_ECVBUF option. status=[%d]", status));
			return status;
		}

		flag = PJ_SOCKET_SND_BUFFER_SIZE;
		status = pj_sock_setsockopt(client_sock, pj_SOL_SOCKET(), pj_SO_SNDBUF(),
			&flag, sizeof(flag));
		if (status != PJ_SUCCESS) {
			PJ_LOG(2, (THIS_FILE, "tcp_sock_make_connection() Failed to set SO_SNDBUF option. status=[%d]", status));
			return status;
		}

		/* Create active socket */
		pj_bzero(&asock_cb, sizeof(asock_cb));
		{
			pj_activesock_t **asock = (pj_activesock_t **)pj_tcp_session_get_asock(tcp_sock->client_sess[sess_idx]);
			pj_activesock_cfg_default(&asock_cfg);
			asock_cfg.whole_data = PJ_TRUE;
			asock_cfg.concurrency = 1;
			asock_cb.on_data_read = &on_data_read;
			asock_cb.on_connect_complete = &on_client_connect_complete;
			status = pj_activesock_create(tcp_sock->pool, client_sock,
				pj_SOCK_STREAM(), &asock_cfg,
				tcp_sock->cfg.ioqueue, &asock_cb, 
				tcp_sock->client_sess[sess_idx],
				asock);
			if (status != PJ_SUCCESS) {
				PJ_LOG(2, (THIS_FILE, "tcp_sock_make_connection() Failed to create activesock. status=[%d]", status));
				pj_tcp_sock_destroy(tcp_sock);
				return status;
			}

			PJ_LOG(4, (THIS_FILE, "tcp_sock_make_connection() Activesock starts to connect to remote."));
			status = pj_activesock_start_connect(*asock, tcp_sock->pool, addr, addr_len);
			if (status == PJ_SUCCESS) {
				on_connect_complete(*asock, PJ_SUCCESS);
			} else if (status != PJ_EPENDING) {
				pj_tcp_sock_destroy(tcp_sock);
				PJ_LOG(2, (THIS_FILE, "tcp_sock_make_connection() TCP activesock destroyed. status=[%d]", status));
			}
		}
	}
	return status;
}

PJ_DEF(void) pj_tcp_sock_set_stun_session_user_data(pj_tcp_sock *tcp_sock, 
                                                    void *p_ice_st,
                                                    unsigned comp_id)
{
	pj_ice_strans *ice_st = (pj_ice_strans *)p_ice_st;
	tcp_stun_data *tsd;

	if (tcp_sock->server_sess) {
		tsd = PJ_POOL_ZALLOC_T(tcp_sock->pool, struct tcp_stun_data);
		tsd->ice = pj_ice_strans_get_ice_session(ice_st);
		tsd->comp_id = comp_id;
		tsd->comp = &tsd->ice->comp[tsd->comp_id-1];

		pj_tcp_session_set_stun_session_user_data(tcp_sock->server_sess, tsd);
	}

	{
		int i;
		for (i = 0; i < tcp_sock->accept_sess_cnt; i++)
		{
			if (tcp_sock->accept_sess[i]) {
				tsd = PJ_POOL_ZALLOC_T(tcp_sock->pool, struct tcp_stun_data);
				tsd->ice = pj_ice_strans_get_ice_session(ice_st);
				tsd->comp_id = comp_id;
				tsd->comp = &tsd->ice->comp[tsd->comp_id-1];

				pj_tcp_session_set_stun_session_user_data(tcp_sock->accept_sess[i], tsd);
			}
		}
		
		for (i = 0; i < tcp_sock->client_sess_cnt; i++)
		{
			if (tcp_sock->client_sess[i]) {
				tsd = PJ_POOL_ZALLOC_T(tcp_sock->pool, struct tcp_stun_data);
				tsd->ice = pj_ice_strans_get_ice_session(ice_st);
				tsd->comp_id = comp_id;
				tsd->comp = &tsd->ice->comp[tsd->comp_id-1];

				pj_tcp_session_set_stun_session_user_data(tcp_sock->client_sess[i], tsd);
			}
		}
	}
}


PJ_DEF(void *) pj_tcp_sock_get_tsd(void *user_data)
{
    pj_tcp_sock *tcp_sock = (pj_tcp_sock *)user_data;
    return (void *)&tcp_sock->tsd;
}

