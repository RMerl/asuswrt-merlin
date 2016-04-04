/* $Id: tcp_session.c 3877 2012-06-25 10:28:34Z dean $ */
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
#include <pjnath/tcp_session.h>
#include <pjnath/errno.h>
#include <pjlib-util/srv_resolver.h>
#include <pj/activesock.h>
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/hash.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pj/sock.h>
// charles fixed unix-like x64 address segmentation
#include <pjnath/tcp_sock.h>

#define THIS_FILE "tcp_session.c"

static const char *state_names[] = 
{
    "Null",
    "Resolving",
    "Resolved",
    "Connectting",
    "Ready",
    "Disconnecting",
    "Disconnected",
    "Destroying"
};

enum timer_id_t
{
    TIMER_NONE,
    TIMER_KEEP_ALIVE,
    TIMER_DESTROY
};

/* This structure describes a channel binding. A channel binding is index by
 * the channel number or IP address and port number of the peer.
 */
struct ch_t
{
    /* The channel number */
    pj_uint16_t	    num;

    /* PJ_TRUE if we've received successful response to ChannelBind request
     * for this channel.
     */
    pj_bool_t	    bound;

    /* The peer IP address and port */
    pj_sockaddr	    addr;

    /* The channel binding expiration */
    pj_time_val	    expiry;
};


/* This structure describes a permission. A permission is identified by the
 * IP address only.
 */
struct perm_t
{
    /* Cache of hash value to speed-up lookup */
    pj_uint32_t	    hval;

    /* The permission IP address. The port number MUST be zero */
    pj_sockaddr	    addr;

    /* Number of peers that uses this permission. */
    unsigned	    peer_cnt;

    /* Automatically renew this permission once it expires? */
    pj_bool_t	    renew;

    /* The permission expiration */
    pj_time_val	    expiry;

    /* Arbitrary/random pointer value (token) to map this perm with the 
     * request to create it. It is used to invalidate this perm when the 
     * request fails.
     */
    void	   *req_token;
};


/* The TCP client session structure */
struct pj_tcp_session
{
    pj_pool_t		*pool;
    const char		*obj_name;
    pj_tcp_session_cb	 cb;
    void		*user_data;
    pj_stun_config	 stun_cfg;

    pj_lock_t		*lock;
    int			 busy;

    pj_tcp_state_t	 state;
    pj_status_t		 last_status;
    pj_bool_t		 pending_destroy;
    pj_bool_t		 destroy_notified;

    pj_stun_session	*stun;

    unsigned		 lifetime;
    int			 ka_interval;
    pj_time_val		 expiry;

    pj_timer_heap_t	*timer_heap;
    pj_timer_entry	 timer;

    pj_dns_srv_async_query *dns_async;
    pj_uint16_t		 default_port;

    pj_uint16_t		 af;
    pj_uint16_t		 srv_addr_cnt;
	pj_sockaddr		*srv_addr_list;
	pj_sockaddr		*srv_addr;

	pj_sockaddr_t	*local_addr;
	unsigned		local_addr_len;

	pj_sockaddr_t	*peer_addr;
	unsigned		peer_addr_len;

    pj_uint32_t		 send_ind_tsx_id[3];
    /* tx_pkt must be 16bit aligned */
	//pj_uint8_t		 tx_pkt[PJ_TCP_MAX_PKT_LEN];

	pj_bool_t		 partial_destroy;
	pj_activesock_t	 *asock;
	
	//DEAN
	int              sess_idx;
	int              check_idx;
	int              cached_check_idx;
};

/*
 * Prototypes.
 */
static void sess_shutdown(pj_tcp_session *sess,
			  pj_status_t status);
static void do_destroy(pj_tcp_session *sess);
static void send_refresh(pj_tcp_session *sess, int lifetime);
static pj_status_t stun_on_send_msg(pj_stun_session *sess,
				    void *token,
				    const void *pkt,
				    pj_size_t pkt_size,
				    const pj_sockaddr_t *dst_addr,
				    unsigned addr_len);
#if 0
static void stun_on_request_complete(pj_stun_session *sess,
				     pj_status_t status,
				     void *token,
				     pj_stun_tx_data *tdata,
				     const pj_stun_msg *response,
				     const pj_sockaddr_t *src_addr,
				     unsigned src_addr_len);
#endif
extern void on_stun_request_complete(pj_stun_session *stun_sess,
							  pj_status_t status,
							  void *token,
							  pj_stun_tx_data *tdata,
							  const pj_stun_msg *response,
							  const pj_sockaddr_t *src_addr,
							  unsigned src_addr_len);
extern pj_status_t on_stun_rx_request(pj_stun_session *sess,
							   const pj_uint8_t *pkt,
							   unsigned pkt_len,
							   const pj_stun_rx_data *rdata,
							   void *token,
							   const pj_sockaddr_t *src_addr,
							   unsigned src_addr_len);
extern pj_status_t on_stun_rx_indication(pj_stun_session *sess,
								  const pj_uint8_t *pkt,
								  unsigned pkt_len,
								  const pj_stun_msg *msg,
								  void *token,
								  const pj_sockaddr_t *src_addr,
								  unsigned src_addr_len);

static void dns_srv_resolver_cb(void *user_data,
				pj_status_t status,
				const pj_dns_srv_record *rec);
#if TCP_TODO
static struct ch_t *lookup_ch_by_addr(pj_tcp_session *sess,
				      const pj_sockaddr_t *addr,
				      unsigned addr_len,
				      pj_bool_t update,
				      pj_bool_t bind_channel);
static struct ch_t *lookup_ch_by_chnum(pj_tcp_session *sess,
				       pj_uint16_t chnum);
static struct perm_t *lookup_perm(pj_tcp_session *sess,
				  const pj_sockaddr_t *addr,
				  unsigned addr_len,
				  pj_bool_t update);
static void invalidate_perm(pj_tcp_session *sess,
			    struct perm_t *perm);
#endif
static void on_timer_event(pj_timer_heap_t *th, pj_timer_entry *e);

/*
 * Get TCP state name.
 */
PJ_DEF(const char*) pj_tcp_state_name(pj_tcp_state_t state)
{
    return state_names[state];
}

/*
 * Create TCP client session.
 */
PJ_DEF(pj_status_t) pj_tcp_session_create( const pj_stun_config *cfg,
					    const char *name,
					    int af,
					    const pj_tcp_session_cb *cb,
						unsigned options,
						void *user_data,
						pj_stun_session *default_stun,
					    pj_tcp_session **p_sess,
						int sess_idx,
						int check_idx)
{
    pj_pool_t *pool;
    pj_tcp_session *sess;
    pj_stun_session_cb stun_cb;
    pj_lock_t *null_lock;
    pj_status_t status;

    PJ_ASSERT_RETURN(cfg && cfg->pf && cb && p_sess, PJ_EINVAL);
    PJ_ASSERT_RETURN(cb->on_send_pkt, PJ_EINVAL);

    PJ_UNUSED_ARG(options);

    if (name == NULL)
	name = "tcp%p";

    /* Allocate and create TCP session */
    pool = pj_pool_create(cfg->pf, name, PJNATH_POOL_LEN_TCP_SESS,
			  PJNATH_POOL_INC_TCP_SESS, NULL);
    sess = PJ_POOL_ZALLOC_T(pool, pj_tcp_session);
    sess->pool = pool;
    sess->obj_name = pool->obj_name;
    sess->timer_heap = cfg->timer_heap;
    sess->af = (pj_uint16_t)af;
    sess->ka_interval = PJ_TCP_KEEP_ALIVE_SEC;
    sess->user_data = user_data;
	sess->sess_idx = sess_idx;
	sess->check_idx = check_idx;

    /* Copy STUN session */
    pj_memcpy(&sess->stun_cfg, cfg, sizeof(pj_stun_config));

    /* Copy callback */
    pj_memcpy(&sess->cb, cb, sizeof(*cb));

    /* Session lock */
    status = pj_lock_create_recursive_mutex(pool, sess->obj_name, 
					    &sess->lock);
    if (status != PJ_SUCCESS) {
	do_destroy(sess);
	return status;
    }

    /* Timer */
    pj_timer_entry_init(&sess->timer, TIMER_NONE, sess, &on_timer_event);

	//DEAN
	if (default_stun) {
		sess->stun = default_stun;
	} else {
		/* Create STUN session */
		pj_bzero(&stun_cb, sizeof(stun_cb));
		stun_cb.on_send_msg = &stun_on_send_msg;
#if 0
		stun_cb.on_request_complete = &stun_on_request_complete;
#else
		stun_cb.on_request_complete = &on_stun_request_complete;
#endif
		stun_cb.on_rx_request = &on_stun_rx_request;
		stun_cb.on_rx_indication = &on_stun_rx_indication;

		status = pj_stun_session_create2(&sess->stun_cfg, sess->obj_name, &stun_cb,
			PJ_FALSE, NULL, &sess->stun, PJ_TRUE, sess);
		if (status != PJ_SUCCESS) {
			do_destroy(sess);
			return status;
		}
	}


    /* Attach ourself to STUN session */
    pj_stun_session_set_user_data(sess->stun, pj_tcp_sock_get_tsd(user_data));

    /* Replace mutex in STUN session with a NULL mutex, since access to
     * STUN session is serialized.
     */
    status = pj_lock_create_null_mutex(pool, name, &null_lock);
    if (status != PJ_SUCCESS) {
	do_destroy(sess);
	return status;
    }
    pj_stun_session_set_lock(sess->stun, null_lock, PJ_TRUE);

    /* Done */

    PJ_LOG(4,(sess->obj_name, "TCP client session created"));

    *p_sess = sess;
    return PJ_SUCCESS;
}


/* Destroy */
static void do_destroy(pj_tcp_session *sess)
{
    /* Lock session */
    if (sess->lock) {
	pj_lock_acquire(sess->lock);
    }

    /* Cancel pending timer, if any */
    if (sess->timer.id != TIMER_NONE) {
	pj_timer_heap_cancel(sess->timer_heap, &sess->timer);
	sess->timer.id = TIMER_NONE;
    }

    /* Destroy STUN session */
    if (sess->stun) {
	pj_stun_session_destroy(sess->stun);
	sess->stun = NULL;
    }

    /* Destroy lock */
    if (sess->lock) {
	pj_lock_release(sess->lock);
	pj_lock_destroy(sess->lock);
	sess->lock = NULL;
	}
#if 0
	if (sess->asock) {
		pj_activesock_close(sess->asock);
		sess->asock = NULL;
	}
#endif
    /* Destroy pool */
    if (sess->pool) {
	pj_pool_t *pool = sess->pool;

	PJ_LOG(4,(sess->obj_name, "TCP client session destroyed"));

	sess->pool = NULL;
	pj_pool_release(pool);
    }
}


/* Set session state */
PJ_DECL(void) pj_tcp_session_set_state(pj_tcp_session *sess, enum pj_tcp_state_t state)
{
	pj_tcp_state_t old_state = sess->state;

	if (state==sess->state)
		return;

	PJ_LOG(4,(sess->obj_name, "State changed %s --> %s",
		state_names[old_state], state_names[state]));
	sess->state = state;
#if 0
	if (sess->partial_destroy) {
		do_destroy(sess);
	} else {
#endif
		if (sess->cb.on_state) {
			(*sess->cb.on_state)(sess, old_state, state);
		}
#if 0
	}
#endif

}


/* Get session state */
PJ_DECL(enum pj_tcp_state_t) pj_tcp_session_get_state(pj_tcp_session *sess)
{
	return sess->state;
}

/*
 * Notify application and shutdown the TCP session.
 */
static void sess_shutdown(pj_tcp_session *sess,
			  pj_status_t status)
{
    pj_bool_t can_destroy = PJ_TRUE;

    PJ_LOG(4,(sess->obj_name, "Request to shutdown in state %s, cause:%d",
	      state_names[sess->state], status));

    if (sess->last_status == PJ_SUCCESS && status != PJ_SUCCESS)
	sess->last_status = status;

    switch (sess->state) {
    case PJ_TCP_STATE_NULL:
	break;
    case PJ_TCP_STATE_RESOLVING:
	if (sess->dns_async != NULL) {
	    pj_dns_srv_cancel_query(sess->dns_async, PJ_FALSE);
	    sess->dns_async = NULL;
	}
	break;
    case PJ_TCP_STATE_RESOLVED:
	break;
    case PJ_TCP_STATE_CONNECTING:
	/* We need to wait until connection complete */
	sess->pending_destroy = PJ_TRUE;
	can_destroy = PJ_FALSE;
	break;
    case PJ_TCP_STATE_READY:
		/* Send REFRESH with LIFETIME=0 */
#if 0
	can_destroy = PJ_FALSE;
	send_refresh(sess, 0);
#endif
	break;
    case PJ_TCP_STATE_DISCONNECTING:
	can_destroy = PJ_FALSE;
	/* This may recursively call this function again with
	 * state==PJ_TCP_STATE_DISCONNECTING.
	 */
#if 0
	send_refresh(sess, 0);
#endif
	break;
    case PJ_TCP_STATE_DISCONNECTED:
    case PJ_TCP_STATE_DESTROYING:
	break;
    }

    if (can_destroy) {
	/* Schedule destroy */
	pj_time_val delay = {0, 0};

	pj_tcp_session_set_state(sess, PJ_TCP_STATE_DESTROYING);

	if (sess->timer.id != TIMER_NONE) {
	    pj_timer_heap_cancel(sess->timer_heap, &sess->timer);
	    sess->timer.id = TIMER_NONE;
	}

	sess->timer.id = TIMER_DESTROY;
	pj_timer_heap_schedule(sess->timer_heap, &sess->timer, &delay);
    }
}


/*
 * Public API to destroy TCP client session.
 */
PJ_DEF(pj_status_t) pj_tcp_session_shutdown(pj_tcp_session *sess)
{
    PJ_ASSERT_RETURN(sess, PJ_EINVAL);

    pj_lock_acquire(sess->lock);

    sess_shutdown(sess, PJ_SUCCESS);

    pj_lock_release(sess->lock);

    return PJ_SUCCESS;
}


/**
 * Forcefully destroy the TCP session.
 */
PJ_DEF(pj_status_t) pj_tcp_session_destroy( pj_tcp_session *sess,
					     pj_status_t last_err)
{
    PJ_ASSERT_RETURN(sess, PJ_EINVAL);

    if (last_err != PJ_SUCCESS && sess->last_status == PJ_SUCCESS)
	sess->last_status = last_err;
	if (!sess->partial_destroy)
		pj_tcp_session_set_state(sess, PJ_TCP_STATE_DISCONNECTED);
    sess_shutdown(sess, PJ_SUCCESS);
    return PJ_SUCCESS;
}


/*
 * Get TCP session info.
 */
PJ_DEF(pj_status_t) pj_tcp_session_get_info( pj_tcp_session *sess,
					      pj_tcp_session_info *info)
{
    pj_time_val now;

    PJ_ASSERT_RETURN(sess && info, PJ_EINVAL);

    pj_gettimeofday(&now);

    info->state = sess->state;
    info->lifetime = sess->expiry.sec - now.sec;
    info->last_status = sess->last_status;

    if (sess->srv_addr)
	pj_memcpy(&info->server, sess->srv_addr, sizeof(info->server));
    else
	pj_bzero(&info->server, sizeof(info->server));

    return PJ_SUCCESS;
}


/*
 * Re-assign user data.
 */
PJ_DEF(pj_status_t) pj_tcp_session_set_user_data( pj_tcp_session *sess,
						   void *user_data)
{
    sess->user_data = user_data;
    return PJ_SUCCESS;
}


/**
 * Retrieve user data.
 */
PJ_DEF(void*) pj_tcp_session_get_user_data(pj_tcp_session *sess)
{
    return sess->user_data;
}


/*
 * Configure message logging. By default all flags are enabled.
 *
 * @param sess		The TCP client session.
 * @param flags		Bitmask combination of #pj_stun_sess_msg_log_flag
 */
PJ_DEF(void) pj_tcp_session_set_log( pj_tcp_session *sess,
				      unsigned flags)
{
    pj_stun_session_set_log(sess->stun, flags);
}


/*
 * Set software name
 */
PJ_DEF(pj_status_t) pj_tcp_session_set_software_name( pj_tcp_session *sess,
						       const pj_str_t *sw)
{
    pj_status_t status;

    pj_lock_acquire(sess->lock);
    status = pj_stun_session_set_software_name(sess->stun, sw);
    pj_lock_release(sess->lock);

    return status;
}

/* Set remote peer address */
PJ_DEF(void) pj_tcp_session_set_peer_addr( pj_tcp_session *sess,
											  const pj_sockaddr_t *addr)
{
	char buf[PJ_INET6_ADDRSTRLEN+20];

	if (!sess)
		return;

	if (!sess->peer_addr) {
		sess->peer_addr = (pj_sockaddr_t *)	
		pj_pool_calloc(sess->pool, 1, 
		pj_sockaddr_get_len(addr));
	}
	sess->peer_addr_len = pj_sockaddr_get_len(addr);
	pj_sockaddr_cp(sess->peer_addr, addr);

	//pj_sockaddr_set_str_addr(pj_AF_INET(), (pj_sockaddr *)sess->peer_addr, &str_addr);

	PJ_LOG(4, (THIS_FILE, "pj_tcp_session_set_remote_peer_addr() %s", 
		pj_sockaddr_print(sess->peer_addr, buf, sizeof(buf), 3)));
}

/* Set local peer address */
PJ_DEF(void) pj_tcp_session_set_local_addr( pj_tcp_session *sess,
												  const pj_sockaddr_t *addr)
{
	char buf[PJ_INET6_ADDRSTRLEN+20];

	if (!sess)
		return;

	if (!sess->local_addr) {
		sess->local_addr = (pj_sockaddr_t *)	
			pj_pool_calloc(sess->pool, 1, 
			pj_sockaddr_get_len(addr));
	}
	sess->local_addr_len = pj_sockaddr_get_len(addr);
	pj_sockaddr_cp(sess->local_addr, addr);

	//pj_sockaddr_set_str_addr(pj_AF_INET(), (pj_sockaddr *)sess->peer_addr, &str_addr);

	PJ_LOG(4, (THIS_FILE, "pj_tcp_session_set_remote_peer_addr() %s", 
		pj_sockaddr_print(sess->local_addr, buf, sizeof(buf), 3)));
}


/**
 * Set the server or domain name of the server.
 */
PJ_DEF(pj_status_t) pj_tcp_session_set_server( pj_tcp_session *sess,
					        const pj_str_t *domain,
						int default_port,
						pj_dns_resolver *resolver)
{
    pj_sockaddr tmp_addr;
    pj_bool_t is_ip_addr;
    pj_status_t status;

    PJ_ASSERT_RETURN(sess && domain, PJ_EINVAL);
    PJ_ASSERT_RETURN(sess->state == PJ_TCP_STATE_NULL, PJ_EINVALIDOP);

    pj_lock_acquire(sess->lock);

    /* See if "domain" contains just IP address */
    tmp_addr.addr.sa_family = sess->af;
    status = pj_inet_pton(sess->af, domain, 
			  pj_sockaddr_get_addr(&tmp_addr));
    is_ip_addr = (status == PJ_SUCCESS);

    if (!is_ip_addr && resolver) {
	/* Resolve with DNS SRV resolution, and fallback to DNS A resolution
	 * if default_port is specified.
	 */
	unsigned opt = 0;
	pj_str_t res_name;

	res_name = pj_str("_tcps._tcp.");

	/* Fallback to DNS A only if default port is specified */
	if (default_port>0 && default_port<65536) {
	    opt = PJ_DNS_SRV_FALLBACK_A;
	    sess->default_port = (pj_uint16_t)default_port;
	}

	PJ_LOG(5,(sess->obj_name, "Resolving %.*s%.*s with DNS SRV",
		  (int)res_name.slen, res_name.ptr,
		  (int)domain->slen, domain->ptr));
	pj_tcp_session_set_state(sess, PJ_TCP_STATE_RESOLVING);

	/* User may have destroyed us in the callback */
	if (sess->state != PJ_TCP_STATE_RESOLVING) {
	    status = PJ_ECANCELLED;
	    goto on_return;
	}

	status = pj_dns_srv_resolve(domain, &res_name, default_port, 
				    sess->pool, resolver, opt, sess, 
				    &dns_srv_resolver_cb, &sess->dns_async);
	if (status != PJ_SUCCESS) {
	    pj_tcp_session_set_state(sess, PJ_TCP_STATE_NULL);
	    goto on_return;
	}

    } else {
	/* Resolver is not specified, resolve with standard gethostbyname().
	 * The default_port MUST be specified in this case.
	 */
	pj_addrinfo *ai;
	unsigned i, cnt;

	/* Default port must be specified */
	PJ_ASSERT_RETURN(default_port>0 && default_port<65536, PJ_EINVAL);
	sess->default_port = (pj_uint16_t)default_port;

	cnt = PJ_TCP_MAX_DNS_SRV_CNT;
	ai = (pj_addrinfo*)
	     pj_pool_calloc(sess->pool, cnt, sizeof(pj_addrinfo));

	PJ_LOG(5,(sess->obj_name, "Resolving %.*s with DNS A",
		  (int)domain->slen, domain->ptr));
	pj_tcp_session_set_state(sess, PJ_TCP_STATE_RESOLVING);

	/* User may have destroyed us in the callback */
	if (sess->state != PJ_TCP_STATE_RESOLVING) {
	    status = PJ_ECANCELLED;
	    goto on_return;
	}

	status = pj_getaddrinfo(sess->af, domain, &cnt, ai);
	if (status != PJ_SUCCESS)
	    goto on_return;

	sess->srv_addr_cnt = (pj_uint16_t)cnt;
	sess->srv_addr_list = (pj_sockaddr*)
		              pj_pool_calloc(sess->pool, cnt, 
					     sizeof(pj_sockaddr));
	for (i=0; i<cnt; ++i) {
	    pj_sockaddr *addr = &sess->srv_addr_list[i];
	    pj_memcpy(addr, &ai[i].ai_addr, sizeof(pj_sockaddr));
	    addr->addr.sa_family = sess->af;
	    addr->ipv4.sin_port = pj_htons(sess->default_port);
	}

	sess->srv_addr = &sess->srv_addr_list[0];
	pj_tcp_session_set_state(sess, PJ_TCP_STATE_RESOLVED);
    }

on_return:
    pj_lock_release(sess->lock);
    return status;
}


/**
 * Set credential to be used by the session.
 */
PJ_DEF(pj_status_t) pj_tcp_session_set_credential(pj_tcp_session *sess,
					     const pj_stun_auth_cred *cred)
{
    PJ_ASSERT_RETURN(sess && cred, PJ_EINVAL);
    PJ_ASSERT_RETURN(sess->stun, PJ_EINVALIDOP);

    pj_lock_acquire(sess->lock);

    pj_stun_session_set_credential(sess->stun, PJ_STUN_AUTH_LONG_TERM, cred);

    pj_lock_release(sess->lock);

    return PJ_SUCCESS;
}

/*
 * Send REFRESH
 */
static void send_refresh(pj_tcp_session *sess, int lifetime)
{
    pj_stun_tx_data *tdata;
    pj_status_t status;

    PJ_ASSERT_ON_FAIL(sess->state==PJ_TCP_STATE_READY, return);

    /* Create a bare REFRESH request */
    status = pj_stun_session_create_req(sess->stun, PJ_STUN_REFRESH_REQUEST,
					PJ_STUN_MAGIC, NULL, &tdata);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Add LIFETIME */
    if (lifetime >= 0) {
	pj_stun_msg_add_uint_attr(tdata->pool, tdata->msg,
				  PJ_STUN_ATTR_LIFETIME, lifetime);
    }

#if TCP_TODO
    /* Send request */
    if (lifetime == 0) {
	pj_tcp_session_set_state(sess, PJ_TCP_STATE_DISCONNECTING);
    }

    status = pj_stun_session_send_msg(sess->stun, NULL, PJ_FALSE, 
				      (sess->conn_type==PJ_TCP_TP_UDP),
				      sess->srv_addr,
				      pj_sockaddr_get_len(sess->srv_addr), 
				      tdata);
    if (status != PJ_SUCCESS)
	goto on_error;
#endif
    return;

on_error:
    if (lifetime == 0) {
	pj_tcp_session_set_state(sess, PJ_TCP_STATE_DISCONNECTED);
	sess_shutdown(sess, status);
    }
}

void dumpH(const unsigned char *buff, int len, int send) 
{
	int i;
	//return; //DEAN
	if (send)
		printf("send : ");
	else 
		printf("recv : ");
	for(i=0;i<len;i++) {
		printf("%02X", buff[i]&0xff);
		if(i%16==15)
			printf("\n");
	}
	printf("\n");
}


/**
 * Relay data to the specified peer through the session.
 */
PJ_DEF(pj_status_t) pj_tcp_session_sendto( pj_tcp_session *sess,
					    const pj_uint8_t *pkt,
					    unsigned pkt_len,
					    const pj_sockaddr_t *addr,
					    unsigned addr_len)
{
	pj_status_t status;

	PJ_ASSERT_RETURN(sess && pkt && pkt_len && addr && addr_len, 
		PJ_EINVAL);

	/* Return error if we're not ready */
#if 1
	if (sess->state != PJ_TCP_STATE_READY) {
		return PJ_EIGNORED;
	}
#endif

	/* Lock session now */
	pj_lock_acquire(sess->lock);

	status = sess->cb.on_send_pkt(sess, pkt, pkt_len,
		addr, addr_len);

	pj_lock_release(sess->lock);
	return status;
}

static pj_uint16_t GETVAL16H(const pj_uint8_t *buf, unsigned pos)
{
	return (pj_uint16_t) ((buf[pos + 0] << 8) | \
		(buf[pos + 1] << 0));
}

/**
 * Notify TCP client session upon receiving a packet from server.
 * The packet maybe a STUN packet or ChannelData packet.
 */
PJ_DEF(pj_status_t) pj_tcp_session_on_rx_pkt(pj_tcp_session *sess,
						 void *pkt,
						 unsigned pkt_len,
						 pj_size_t *parsed_len)
{
	pj_bool_t is_stun = PJ_FALSE;
	pj_status_t stun_check;
	pj_status_t status;
	char buf[PJ_INET6_ADDRSTRLEN+20];

	pj_uint16_t data_len;

    /* Packet could be ChannelData or STUN message (response or
     * indication).
     */
    /* Start locking the session */
    pj_lock_acquire(sess->lock);

    /* Quickly check if this is STUN message */
	//is_stun = (((pj_uint16_t*)pkt)[0] != NATNL_UDT_HEADER_MAGIC);
	stun_check = pj_stun_msg_check((const pj_uint8_t*)pkt, pkt_len, 
		/*PJ_STUN_IS_DATAGRAM | PJ_STUN_CHECK_PACKET*/ 0);

	if (stun_check == PJ_SUCCESS) 
		is_stun = PJ_TRUE;

    if (is_stun) {
#if 1
		/* This looks like STUN, give it to the STUN session */
		unsigned options;
		pj_size_t msg_len;

		options = PJ_STUN_CHECK_PACKET | PJ_STUN_NO_FINGERPRINT_CHECK;

		msg_len = GETVAL16H((const pj_uint8_t*)pkt, 2);

		*parsed_len = (msg_len+sizeof(pj_stun_msg_hdr) <= pkt_len) ? 
			msg_len+sizeof(struct pj_stun_msg_hdr) : 0;
		
		/* Notify application */
		if (sess->cb.on_rx_data) {
			(*sess->cb.on_rx_data)(sess, pkt, pkt_len, 
				sess->peer_addr, sess->peer_addr_len);
		}
		//DumpPacket(pkt, pkt_len, 0, 5);

		status = PJ_SUCCESS;
#else
		/* This looks like STUN, give it to the STUN session */
		unsigned options;

		options = PJ_STUN_CHECK_PACKET | PJ_STUN_NO_FINGERPRINT_CHECK;
		status=pj_stun_session_on_rx_pkt(sess->stun, pkt, pkt_len,
			options, NULL, parsed_len,
			sess->peer_addr, sess->peer_addr_len);
#endif
		PJ_LOG(5, (THIS_FILE, "pj_tcp_session_on_rx_pkt() is_stun %s", 
			sess->peer_addr == NULL ? "NULL" : 
			pj_sockaddr_print(sess->peer_addr, buf, PJ_INET6_ADDRSTRLEN, 3)));
	} else {
		PJ_LOG(5, (THIS_FILE, "pj_tcp_session_on_rx_pkt() not_stun %s", 
			sess->peer_addr == NULL ? "NULL" : 
			pj_sockaddr_print(sess->peer_addr, buf, PJ_INET6_ADDRSTRLEN, 3)));

		//DumpPacket(pkt, pkt_len, 0, 7);

		if (((pj_uint16_t*)pkt)[0] != NATNL_UDT_HEADER_MAGIC) {

			if (pkt_len < NATNL_DTLS_HEADER_SIZE) {
				if (parsed_len) {
					*parsed_len = 0;
				}
				return PJ_ETOOSMALL;
			}

			/* Check that size is sane */
			data_len = pj_ntohs(((pj_uint16_t*)(((pj_uint8_t*)pkt)+11))[0]);
			if (pkt_len < data_len+NATNL_DTLS_HEADER_SIZE) {
				if (parsed_len) {
					/* Insufficient fragment */
					*parsed_len = 0;
				}
				status = PJ_ETOOSMALL;
				goto on_return;
			} else {
				if (parsed_len) {
					/* Apply padding too */
					*parsed_len = data_len+NATNL_DTLS_HEADER_SIZE;
				}
			}
		} else {

			if (pkt_len < NATNL_UDT_HEADER_SIZE) {
				if (parsed_len) {
					*parsed_len = 0;
				}
				return PJ_ETOOSMALL;
			}

			/* Check that size is sane */
			data_len = pj_ntohs(((pj_uint16_t*)pkt)[1]);
			if (pkt_len < data_len+NATNL_UDT_HEADER_SIZE) {
				if (parsed_len) {
					/* Insufficient fragment */
					*parsed_len = 0;
				}
				status = PJ_ETOOSMALL;
				goto on_return;
			} else {
				if (parsed_len) {
					/* Apply padding too */
					*parsed_len = data_len+NATNL_UDT_HEADER_SIZE;
				}
			}
		}

		/* Notify application */
		if (sess->cb.on_rx_data) {
			(*sess->cb.on_rx_data)(sess, pkt, *parsed_len, 
				sess->peer_addr, sess->peer_addr_len);
		}
		//DumpPacket(pkt, pkt_len, 0, 6);
		status = PJ_SUCCESS;
	}

on_return:
    pj_lock_release(sess->lock);
    return status;
}


/*
 * This is a callback from STUN session to send outgoing packet.
 */
static pj_status_t stun_on_send_msg(pj_stun_session *stun,
				    void *token,
				    const void *pkt,
				    pj_size_t pkt_size,
				    const pj_sockaddr_t *dst_addr,
				    unsigned addr_len)
{
    pj_tcp_session *sess;

    PJ_UNUSED_ARG(token);

    sess = (pj_tcp_session*) pj_stun_session_get_user_data2(stun);
    return (*sess->cb.on_send_pkt)(sess, (const pj_uint8_t*)pkt, pkt_size, 
				   dst_addr, addr_len);
}


/*
 * Handle failed ALLOCATE or REFRESH request. This may switch to alternate
 * server if we have one.
 */
static void on_session_fail( pj_tcp_session *sess, 
			     enum pj_stun_method_e method,
			     pj_status_t status,
			     const pj_str_t *reason)
{
    sess->last_status = status;

    do {
	pj_str_t reason1;
	char err_msg[PJ_ERR_MSG_SIZE];

	if (reason == NULL) {
	    pj_strerror(status, err_msg, sizeof(err_msg));
	    reason1 = pj_str(err_msg);
	    reason = &reason1;
	}

	PJ_LOG(4,(sess->obj_name, "%s error: %.*s",
		  pj_stun_get_method_name(method),
		  (int)reason->slen, reason->ptr));

	/* If this is ALLOCATE response and we don't have more server 
	 * addresses to try, notify application and destroy the TCP
	 * session.
	 */
	if (method==PJ_STUN_ALLOCATE_METHOD &&
	    sess->srv_addr == &sess->srv_addr_list[sess->srv_addr_cnt-1]) 
	{

	    pj_tcp_session_set_state(sess, PJ_TCP_STATE_DISCONNECTED);
	    sess_shutdown(sess, status);
	    return;
	}

	/* Otherwise if this is not ALLOCATE response, notify application
	 * that session has been TERMINATED.
	 */
	if (method!=PJ_STUN_ALLOCATE_METHOD) {
	    pj_tcp_session_set_state(sess, PJ_TCP_STATE_DISCONNECTED);
	    sess_shutdown(sess, status);
	    return;
	}

	/* Try next server */
	++sess->srv_addr;
	reason = NULL;

	PJ_LOG(4,(sess->obj_name, "Trying next server"));
	pj_tcp_session_set_state(sess, PJ_TCP_STATE_RESOLVED);

    } while (0);
}

/*
 * Notification on completion of DNS SRV resolution.
 */
static void dns_srv_resolver_cb(void *user_data,
				pj_status_t status,
				const pj_dns_srv_record *rec)
{
    pj_tcp_session *sess = (pj_tcp_session*) user_data;
    unsigned i, cnt, tot_cnt;

    /* Clear async resolver */
    sess->dns_async = NULL;

    /* Check failure */
    if (status != PJ_SUCCESS) {
	sess_shutdown(sess, status);
	return;
    }

    /* Calculate total number of server entries in the response */
    tot_cnt = 0;
    for (i=0; i<rec->count; ++i) {
	tot_cnt += rec->entry[i].server.addr_count;
    }

    if (tot_cnt > PJ_TCP_MAX_DNS_SRV_CNT)
	tot_cnt = PJ_TCP_MAX_DNS_SRV_CNT;

    /* Allocate server entries */
    sess->srv_addr_list = (pj_sockaddr*)
		           pj_pool_calloc(sess->pool, tot_cnt, 
					  sizeof(pj_sockaddr));

    /* Copy results to server entries */
    for (i=0, cnt=0; i<rec->count && cnt<PJ_TCP_MAX_DNS_SRV_CNT; ++i) {
	unsigned j;

	for (j=0; j<rec->entry[i].server.addr_count && 
		  cnt<PJ_TCP_MAX_DNS_SRV_CNT; ++j) 
	{
	    pj_sockaddr_in *addr = &sess->srv_addr_list[cnt].ipv4;

	    addr->sin_family = sess->af;
	    addr->sin_port = pj_htons(rec->entry[i].port);
	    addr->sin_addr.s_addr = rec->entry[i].server.addr[j].s_addr;

	    ++cnt;
	}
    }
    sess->srv_addr_cnt = (pj_uint16_t)cnt;

    /* Set current server */
    sess->srv_addr = &sess->srv_addr_list[0];

    /* Set state to PJ_TCP_STATE_RESOLVED */
    pj_tcp_session_set_state(sess, PJ_TCP_STATE_RESOLVED);
}

/*
 * Timer event.
 */
static void on_timer_event(pj_timer_heap_t *th, pj_timer_entry *e)
{
    pj_tcp_session *sess = (pj_tcp_session*)e->user_data;
    enum timer_id_t eid;

    PJ_UNUSED_ARG(th);

    pj_lock_acquire(sess->lock);

    eid = (enum timer_id_t) e->id;
    e->id = TIMER_NONE;
    
    if (eid == TIMER_KEEP_ALIVE) {
	pj_time_val now;
	pj_bool_t resched = PJ_TRUE;

	pj_gettimeofday(&now);

	/* Reshcedule timer */
	if (resched) {
	    pj_time_val delay;

	    delay.sec = sess->ka_interval;
	    delay.msec = 0;

	    sess->timer.id = TIMER_KEEP_ALIVE;
	    pj_timer_heap_schedule(sess->timer_heap, &sess->timer, &delay);
	}

	pj_lock_release(sess->lock);

    } else if (eid == TIMER_DESTROY) {
	/* Time to destroy */
	pj_lock_release(sess->lock);
	do_destroy(sess);
    } else {
	pj_assert(!"Unknown timer event");
	pj_lock_release(sess->lock);
    }
}

PJ_DEF(void *) pj_tcp_session_get_stun_session(void *user_data)
{
	pj_tcp_session *tcp_sess = (pj_tcp_session *)user_data;
	return tcp_sess == NULL ? NULL : tcp_sess->stun;
}

PJ_DEF(void) pj_tcp_session_set_stun_session_user_data(pj_tcp_session *tcp_sess, 
													   void *user_data)
{
	pj_stun_session_set_user_data(tcp_sess->stun, user_data);
}

PJ_DEF(char *) pj_tcp_session_get_object_name(void *user_data)
{
	pj_tcp_session *tcp_sess = (pj_tcp_session *)user_data;
	return tcp_sess == NULL ? "NULL" : tcp_sess->obj_name;
}

PJ_DEF(void *) pj_tcp_session_get_tcp_sock(void *user_data)
{
	pj_tcp_session *tcp_sess = (pj_tcp_session *)user_data;
	return tcp_sess->user_data;
}

PJ_DEF(void *) pj_tcp_session_get_pool(void *user_data)
{
	pj_tcp_session *tcp_sess = (pj_tcp_session *)user_data;
	return tcp_sess->pool;
}

PJ_DEF(void) pj_tcp_session_set_partial_destroy(void *user_data, pj_bool_t value)
{
	pj_tcp_session *tcp_sess = (pj_tcp_session *)user_data;
	tcp_sess->partial_destroy = value;
}

PJ_DEF(void **) pj_tcp_session_get_asock(void *user_data)
{
	pj_tcp_session *tcp_sess = (pj_tcp_session *)user_data;
	return &tcp_sess->asock;
}

PJ_DEF(void *) pj_tcp_session_set_asock(void *user_data, void *value)
{
	pj_tcp_session *tcp_sess = (pj_tcp_session *)user_data;
	return tcp_sess->asock = (pj_activesock_t *)value;
}

PJ_DEF(int) pj_tcp_session_get_idx(void *user_data)
{
	pj_tcp_session *tcp_sess = (pj_tcp_session *)user_data;
	return tcp_sess->sess_idx;
}
