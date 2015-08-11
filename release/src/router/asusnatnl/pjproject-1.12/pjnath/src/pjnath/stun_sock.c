/* $Id: stun_sock.c 4400 2013-02-27 14:31:05Z riza $ */
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
#include <pjnath/stun_sock.h>
#include <pjnath/errno.h>
#include <pjnath/stun_transaction.h>
#include <pjnath/stun_session.h>
#include <pjlib-util/srv_resolver.h>
#include <pj/activesock.h>
#include <pj/addr_resolv.h>
#include <pj/array.h>
#include <pj/assert.h>
#include <pj/ip_helper.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pjnath/nat_detect.h>

#define THIS_FILE "stun_sock.c"

struct pj_stun_sock
{
    char		*obj_name;	/* Log identification	    */
    pj_pool_t		*pool;		/* Pool			    */
    void		*user_data;	/* Application user data    */

    int			 af;		/* Address family	    */
    pj_stun_config	 stun_cfg;	/* STUN config (ioqueue etc)*/
    pj_stun_sock_cb	 cb;		/* Application callbacks    */

    int			 ka_interval;	/* Keep alive interval	    */
    pj_timer_entry	 ka_timer;	/* Keep alive timer.	    */

	pj_sockaddr		 srv_addr;	/* Resolved server addr	    */
	pj_sockaddr		 mapped_addr;	/* Our public address	    */
	pj_sockaddr		 current_local_addr;	/* Our current bound address	    */
	pj_sockaddr		 previous_local_addr;	/* Our previous bound address	    */

    pj_dns_srv_async_query *q;		/* Pending DNS query	    */
    pj_sock_t		 sock_fd;	/* Socket descriptor	    */
    pj_activesock_t	*active_sock;	/* Active socket object	    */
    pj_ioqueue_op_key_t	 send_key;	/* Default send key for app */
    pj_ioqueue_op_key_t	 int_send_key;	/* Send key for internal    */

    pj_uint16_t		 tsx_id[6];	/* .. to match STUN msg	    */
    pj_stun_session	*stun_sess;	/* STUN session		    */

};

/* 
 * Prototypes for static functions 
 */

/* This callback is called by the STUN session to send packet */
static pj_status_t sess_on_send_msg(pj_stun_session *sess,
				    void *token,
				    const void *pkt,
				    pj_size_t pkt_size,
				    const pj_sockaddr_t *dst_addr,
				    unsigned addr_len);

/* This callback is called by the STUN session when outgoing transaction 
 * is complete
 */
static void sess_on_request_complete(pj_stun_session *sess,
				     pj_status_t status,
				     void *token,
				     pj_stun_tx_data *tdata,
				     const pj_stun_msg *response,
				     const pj_sockaddr_t *src_addr,
				     unsigned src_addr_len);
/* DNS resolver callback */
static void dns_srv_resolver_cb(void *user_data,
				pj_status_t status,
				const pj_dns_srv_record *rec);

/* Start sending STUN Binding request */
static pj_status_t get_mapped_addr(pj_stun_sock *stun_sock);

/* Callback from active socket when incoming packet is received */
static pj_bool_t on_data_recvfrom(pj_activesock_t *asock,
				  void *data,
				  pj_size_t size,
				  const pj_sockaddr_t *src_addr,
				  int addr_len,
				  pj_status_t status);

/* Callback from active socket about send status */
static pj_bool_t on_data_sent(pj_activesock_t *asock,
			      pj_ioqueue_op_key_t *send_key,
			      pj_ssize_t sent);

/* Schedule keep-alive timer */
static void start_ka_timer(pj_stun_sock *stun_sock);

/* Keep-alive timer callback */
static void ka_timer_cb(pj_timer_heap_t *th, pj_timer_entry *te);

#define INTERNAL_MSG_TOKEN  (void*)1


/*
 * Retrieve the name representing the specified operation.
 */
PJ_DEF(const char*) pj_stun_sock_op_name(pj_stun_sock_op op)
{
    const char *names[] = {
	"?",
	"DNS resolution",
	"STUN Binding request",
	"Keep-alive",
	"Mapped addr. changed"
    };

    return op < PJ_ARRAY_SIZE(names) ? names[op] : "???";
};


/*
 * Initialize the STUN transport setting with its default values.
 */
PJ_DEF(void) pj_stun_sock_cfg_default(pj_stun_sock_cfg *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));
    cfg->max_pkt_size = PJ_STUN_SOCK_PKT_LEN;
    cfg->async_cnt = 1;
    cfg->ka_interval = PJ_STUN_KEEP_ALIVE_SEC;
    cfg->qos_type = PJ_QOS_TYPE_BEST_EFFORT;
    cfg->qos_ignore_error = PJ_TRUE;
}


/* Check that configuration setting is valid */
static pj_bool_t pj_stun_sock_cfg_is_valid(const pj_stun_sock_cfg *cfg)
{
    return cfg->max_pkt_size > 1 && cfg->async_cnt >= 1;
}

/*
 * Create the STUN transport using the specified configuration.
 */
PJ_DEF(pj_status_t) pj_stun_sock_create( pj_stun_config *stun_cfg,
					 const char *name,
					 int af,
					 const pj_stun_sock_cb *cb,
					 const pj_stun_sock_cfg *cfg,
					 void *user_data,
					 pj_stun_sock **p_stun_sock)
{
    pj_pool_t *pool;
    pj_stun_sock *stun_sock;
    pj_stun_sock_cfg default_cfg;
    unsigned i;
	pj_status_t status;
	long sobuf_size;

    PJ_ASSERT_RETURN(stun_cfg && cb && p_stun_sock, PJ_EINVAL);
    PJ_ASSERT_RETURN(af==pj_AF_INET()||af==pj_AF_INET6(), PJ_EAFNOTSUP);
    PJ_ASSERT_RETURN(!cfg || pj_stun_sock_cfg_is_valid(cfg), PJ_EINVAL);
    PJ_ASSERT_RETURN(cb->on_status, PJ_EINVAL);

    status = pj_stun_config_check_valid(stun_cfg);
    if (status != PJ_SUCCESS)
	return status;

    if (name == NULL)
	name = "stuntp%p";

    if (cfg == NULL) {
	pj_stun_sock_cfg_default(&default_cfg);
	cfg = &default_cfg;
    }


    /* Create structure */
    pool = pj_pool_create(stun_cfg->pf, name, 256, 512, NULL);
    stun_sock = PJ_POOL_ZALLOC_T(pool, pj_stun_sock);
    stun_sock->pool = pool;
    stun_sock->obj_name = pool->obj_name;
    stun_sock->user_data = user_data;
    stun_sock->af = af;
    stun_sock->sock_fd = PJ_INVALID_SOCKET;
    pj_memcpy(&stun_sock->stun_cfg, stun_cfg, sizeof(*stun_cfg));
    pj_memcpy(&stun_sock->cb, cb, sizeof(*cb));

    stun_sock->ka_interval = cfg->ka_interval;
    if (stun_sock->ka_interval == 0)
	stun_sock->ka_interval = PJ_STUN_KEEP_ALIVE_SEC;

    /* Create socket and bind socket */
    status = pj_sock_socket(af, pj_SOCK_DGRAM(), 0, &stun_sock->sock_fd);
    if (status != PJ_SUCCESS)
	goto on_error;

#if 1 // natnl set stun socket recv and send buffer size.
	sobuf_size = cfg->sock_recv_buf_size ? cfg->sock_recv_buf_size : PJ_STUN_SOCK_PKT_LEN;
	status = pj_sock_setsockopt(stun_sock->sock_fd, pj_SOL_SOCKET(), pj_SO_RCVBUF(),
		&sobuf_size, sizeof(sobuf_size));
	if (status != PJ_SUCCESS)
		goto on_error;
	
	sobuf_size = cfg->sock_send_buf_size ? cfg->sock_send_buf_size : PJ_SOCKET_SND_BUFFER_SIZE;
	status = pj_sock_setsockopt(stun_sock->sock_fd, pj_SOL_SOCKET(), pj_SO_SNDBUF(),
		&sobuf_size, sizeof(sobuf_size));
	if (status != PJ_SUCCESS)
		goto on_error;
#endif

    /* Apply QoS, if specified */
    status = pj_sock_apply_qos2(stun_sock->sock_fd, cfg->qos_type,
				&cfg->qos_params, 2, stun_sock->obj_name,
				NULL);
    if (status != PJ_SUCCESS && !cfg->qos_ignore_error)
	goto on_error;

    /* Bind socket */

	if (pj_sockaddr_has_addr(&cfg->bound_addr)) {
	status = pj_sock_bind(stun_sock->sock_fd, &cfg->bound_addr,
			      pj_sockaddr_get_len(&cfg->bound_addr));
    } else {
	pj_sockaddr bound_addr;

	pj_sockaddr_init(af, &bound_addr, NULL, 0);
	status = pj_sock_bind(stun_sock->sock_fd, &bound_addr,
			      pj_sockaddr_get_len(&bound_addr));
    }

    if (status != PJ_SUCCESS)
	goto on_error;

    /* Create more useful information string about this transport */
#if 0
    {
	pj_sockaddr bound_addr;
	int addr_len = sizeof(bound_addr);

	status = pj_sock_getsockname(stun_sock->sock_fd, &bound_addr, 
				     &addr_len);
	if (status != PJ_SUCCESS)
	    goto on_error;

	stun_sock->info = pj_pool_alloc(pool, PJ_INET6_ADDRSTRLEN+10);
	pj_sockaddr_print(&bound_addr, stun_sock->info, 
			  PJ_INET6_ADDRSTRLEN, 3);
    }
#endif

    /* Init active socket configuration */
    {
	pj_activesock_cfg activesock_cfg;
	pj_activesock_cb activesock_cb;

	pj_activesock_cfg_default(&activesock_cfg);
	activesock_cfg.async_cnt = cfg->async_cnt;
	activesock_cfg.concurrency = 0;

        /* Create the active socket */
	pj_bzero(&activesock_cb, sizeof(activesock_cb));
	activesock_cb.on_data_recvfrom = &on_data_recvfrom;
	activesock_cb.on_data_sent = &on_data_sent;
	status = pj_activesock_create(pool, stun_sock->sock_fd, 
				      pj_SOCK_DGRAM(), 
				      &activesock_cfg, stun_cfg->ioqueue,
				      &activesock_cb, stun_sock,
				      &stun_sock->active_sock);
	if (status != PJ_SUCCESS)
	    goto on_error;

        /* Start asynchronous read operations */
	status = pj_activesock_start_recvfrom(stun_sock->active_sock, pool,
					      cfg->max_pkt_size, 0);
	if (status != PJ_SUCCESS)
	    goto on_error;

	/* Init send keys */
	pj_ioqueue_op_key_init(&stun_sock->send_key, 
			       sizeof(stun_sock->send_key));
	pj_ioqueue_op_key_init(&stun_sock->int_send_key,
			       sizeof(stun_sock->int_send_key));
    }

    /* Create STUN session */
    {
	pj_stun_session_cb sess_cb;

	pj_bzero(&sess_cb, sizeof(sess_cb));
	sess_cb.on_request_complete = &sess_on_request_complete;
	sess_cb.on_send_msg = &sess_on_send_msg;
	status = pj_stun_session_create(&stun_sock->stun_cfg, 
					stun_sock->obj_name,
					&sess_cb, PJ_FALSE, 
					&stun_sock->stun_sess);
	if (status != PJ_SUCCESS)
	    goto on_error;
    }

    /* Associate us with the STUN session */
    pj_stun_session_set_user_data(stun_sock->stun_sess, stun_sock);

    /* Initialize random numbers to be used as STUN transaction ID for
     * outgoing Binding request. We use the 80bit number to distinguish
     * STUN messages we sent with STUN messages that the application sends.
     * The last 16bit value in the array is a counter.
     */
    for (i=0; i<PJ_ARRAY_SIZE(stun_sock->tsx_id); ++i) {
	stun_sock->tsx_id[i] = (pj_uint16_t) pj_rand();
    }
    stun_sock->tsx_id[5] = 0;


    /* Init timer entry */
    stun_sock->ka_timer.cb = &ka_timer_cb;
    stun_sock->ka_timer.user_data = stun_sock;

    /* Done */
    *p_stun_sock = stun_sock;
    return PJ_SUCCESS;

on_error:
    pj_stun_sock_destroy(stun_sock);
    return status;
}

/* Start socket. */
PJ_DEF(pj_status_t) pj_stun_sock_start( pj_stun_sock *stun_sock,
				        const pj_str_t *domain,
				        pj_uint16_t default_port,
				        pj_dns_resolver *resolver)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(stun_sock && domain && default_port, PJ_EINVAL);

    /* Check whether the domain contains IP address */
    stun_sock->srv_addr.addr.sa_family = (pj_uint16_t)stun_sock->af;
	//printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>domain=%s", domain->ptr);
    status = pj_inet_pton(stun_sock->af, domain, 
			  pj_sockaddr_get_addr(&stun_sock->srv_addr));
    if (status != PJ_SUCCESS) {
	stun_sock->srv_addr.addr.sa_family = (pj_uint16_t)0;
    }

    /* If resolver is set, try to resolve with DNS SRV first. It
     * will fallback to DNS A/AAAA when no SRV record is found.
     */
    if (status != PJ_SUCCESS && resolver) {
	const pj_str_t res_name = pj_str("_stun._udp.");
	unsigned opt;

	pj_assert(stun_sock->q == NULL);

	opt = PJ_DNS_SRV_FALLBACK_A;
	if (stun_sock->af == pj_AF_INET6()) {
	    opt |= (PJ_DNS_SRV_RESOLVE_AAAA | PJ_DNS_SRV_FALLBACK_AAAA);
	}

	status = pj_dns_srv_resolve(domain, &res_name, default_port, 
				    stun_sock->pool, resolver, opt,
				    stun_sock, &dns_srv_resolver_cb, 
				    &stun_sock->q);

	/* Processing will resume when the DNS SRV callback is called */
	return status;

    } else {

	if (status != PJ_SUCCESS) {
	    pj_addrinfo ai;
	    unsigned cnt = 1;

	    status = pj_getaddrinfo(stun_sock->af, domain, &cnt, &ai);	    
	    if (status != PJ_SUCCESS)
		return status;
	    pj_sockaddr_cp(&stun_sock->srv_addr, &ai.ai_addr);
	}

	pj_sockaddr_set_port(&stun_sock->srv_addr, (pj_uint16_t)default_port);

	/* Start sending Binding request */
	return get_mapped_addr(stun_sock);
    }
}

/* Destroy */
PJ_DEF(pj_status_t) pj_stun_sock_destroy(pj_stun_sock *stun_sock)
{
    if (stun_sock->q) {
	pj_dns_srv_cancel_query(stun_sock->q, PJ_FALSE);
	stun_sock->q = NULL;
    }

    if (stun_sock->stun_sess) {
	pj_stun_session_set_user_data(stun_sock->stun_sess, NULL);
    }
    
    /* Destroy the active socket first just in case we'll get
     * stray callback.
     */
    if (stun_sock->active_sock != NULL) {
	pj_activesock_t	*asock = stun_sock->active_sock;
	stun_sock->active_sock = NULL;
	stun_sock->sock_fd = PJ_INVALID_SOCKET;
	pj_activesock_set_user_data(asock, NULL);
	pj_activesock_close(asock);
    } else if (stun_sock->sock_fd != PJ_INVALID_SOCKET) {
	pj_sock_close(stun_sock->sock_fd);
	stun_sock->sock_fd = PJ_INVALID_SOCKET;
    }

    if (stun_sock->ka_timer.id != 0) {
	pj_timer_heap_cancel(stun_sock->stun_cfg.timer_heap, 
			     &stun_sock->ka_timer);
	stun_sock->ka_timer.id = 0;
    }

    if (stun_sock->stun_sess) {
	pj_stun_session_destroy(stun_sock->stun_sess);
	stun_sock->stun_sess = NULL;
    }

    if (stun_sock->pool) {
	pj_pool_t *pool = stun_sock->pool;
	stun_sock->pool = NULL;
	pj_pool_release(pool);
    }

    return PJ_SUCCESS;
}

/* Associate user data */
PJ_DEF(pj_status_t) pj_stun_sock_set_user_data( pj_stun_sock *stun_sock,
					        void *user_data)
{
    PJ_ASSERT_RETURN(stun_sock, PJ_EINVAL);
    stun_sock->user_data = user_data;
    return PJ_SUCCESS;
}


/* Get user data */
PJ_DEF(void*) pj_stun_sock_get_user_data(pj_stun_sock *stun_sock)
{
    PJ_ASSERT_RETURN(stun_sock, NULL);
    return stun_sock->user_data;
}

/* Notify application that session has failed */
static pj_bool_t sess_fail(pj_stun_sock *stun_sock, 
			   pj_stun_sock_op op,
			   pj_status_t status)
{
    pj_bool_t ret;

    PJ_PERROR(4,(stun_sock->obj_name, status, 
	         "Session failed because %s failed",
		 pj_stun_sock_op_name(op)));

    ret = (*stun_sock->cb.on_status)(stun_sock, op, status);

    return ret;
}

/* DNS resolver callback */
static void dns_srv_resolver_cb(void *user_data,
				pj_status_t status,
				const pj_dns_srv_record *rec)
{
    pj_stun_sock *stun_sock = (pj_stun_sock*) user_data;

    /* Clear query */
    stun_sock->q = NULL;

    /* Handle error */
    if (status != PJ_SUCCESS) {
	sess_fail(stun_sock, PJ_STUN_SOCK_DNS_OP, status);
	return;
    }

    pj_assert(rec->count);
    pj_assert(rec->entry[0].server.addr_count);

    PJ_TODO(SUPPORT_IPV6_IN_RESOLVER);
    pj_assert(stun_sock->af == pj_AF_INET());

    /* Set the address */
    pj_sockaddr_in_init(&stun_sock->srv_addr.ipv4, NULL,
			rec->entry[0].port);
    stun_sock->srv_addr.ipv4.sin_addr = rec->entry[0].server.addr[0];

    /* Start sending Binding request */
    get_mapped_addr(stun_sock);
}


/* Start sending STUN Binding request */
static pj_status_t get_mapped_addr(pj_stun_sock *stun_sock)
{
    pj_stun_tx_data *tdata;
    pj_status_t status;

    /* Increment request counter and create STUN Binding request */
    ++stun_sock->tsx_id[5];
    status = pj_stun_session_create_req(stun_sock->stun_sess,
					PJ_STUN_BINDING_REQUEST,
					PJ_STUN_MAGIC, 
					(const pj_uint8_t*)stun_sock->tsx_id, 
					&tdata);
    if (status != PJ_SUCCESS)
		goto on_error;
#if 0
	// 2013-10-16 DEAN, 
	// Although this is udp sock, we still connect to stun server for getting local address by getsockname.
	status = pj_activesock_start_connect(stun_sock->active_sock, stun_sock->pool, 
		&stun_sock->srv_addr, pj_sockaddr_get_len(&stun_sock->srv_addr));
#endif
    /* Send request */
    status=pj_stun_session_send_msg(stun_sock->stun_sess, INTERNAL_MSG_TOKEN,
				    PJ_FALSE, PJ_TRUE, &stun_sock->srv_addr,
				    pj_sockaddr_get_len(&stun_sock->srv_addr),
					tdata);
	// DEAN, we guarantee data can be sent.
	// DEAN discard this
	//pj_stun_session_cancel_timer(tdata); 
	
	if (status != PJ_SUCCESS && status != PJ_EPENDING)
	goto on_error;

    return PJ_SUCCESS;

on_error:
    sess_fail(stun_sock, PJ_STUN_SOCK_BINDING_OP, status);
    return status;
}
#if PJ_ANDROID==1
#include <j_log.h>
#define PJ_DLOG "PJSIP"
#endif
/* Get info */
PJ_DEF(pj_status_t) pj_stun_sock_get_info( pj_stun_sock *stun_sock,
					   pj_stun_sock_info *info)
{
    int addr_len;
    pj_status_t status;

    PJ_ASSERT_RETURN(stun_sock && info, PJ_EINVAL);

    /* Copy STUN server address and mapped address */
    pj_memcpy(&info->srv_addr, &stun_sock->srv_addr,
		sizeof(pj_sockaddr));
	pj_memcpy(&info->mapped_addr, &stun_sock->mapped_addr, 
		sizeof(pj_sockaddr));

	PJ_LOG(4, ("stun_sock.c", "pj_stun_sock_get_info() pj_sock_getsockname1."));
    /* Retrieve bound address */
    addr_len = sizeof(info->bound_addr);
    status = pj_sock_getsockname(stun_sock->sock_fd, &info->bound_addr,
				 &addr_len);
    if (status != PJ_SUCCESS)
		return status;
	PJ_LOG(4, ("stun_sock.c", "pj_stun_sock_get_info() pj_sock_getsockname2."));

	pj_memcpy(&info->local_addr, &stun_sock->current_local_addr, 
		sizeof(pj_sockaddr));

    /* If socket is bound to a specific interface, then only put that
     * interface in the alias list. Otherwise query all the interfaces 
     * in the host.
     */
    if (pj_sockaddr_has_addr(&info->bound_addr)) {
		info->alias_cnt = 1;
		pj_sockaddr_cp(&info->aliases[0], &info->bound_addr);
    } else {
	pj_sockaddr def_addr;
	pj_uint16_t port = pj_sockaddr_get_port(&info->bound_addr); 
	unsigned i;

	PJ_LOG(4, ("stun_sock.c", "pj_stun_sock_get_info() pj_gethostip1."));
	/* Get the default address */
	status = pj_gethostip(stun_sock->af, &def_addr);
	if (status != PJ_SUCCESS)
		return status;
	PJ_LOG(4, ("stun_sock.c", "pj_stun_sock_get_info() pj_gethostip2."));
	
	pj_sockaddr_set_port(&def_addr, port);	

	PJ_LOG(4, ("stun_sock.c", "pj_stun_sock_get_info() pj_enum_ip_interface1."));
	/* Enum all IP interfaces in the host */
	info->alias_cnt = PJ_ARRAY_SIZE(info->aliases);
	status = pj_enum_ip_interface(stun_sock->af, &info->alias_cnt, 
				      info->aliases);
	if (status != PJ_SUCCESS)
		return status;
	PJ_LOG(4, ("stun_sock.c", "pj_stun_sock_get_info() pj_enum_ip_interface2."));

	/* Set the port number for each address.
	 */
	for (i=0; i<info->alias_cnt; ++i) {
	    pj_sockaddr_set_port(&info->aliases[i], port);
	}
	PJ_LOG(4, ("stun_sock.c", "pj_stun_sock_get_info() pj_sockaddr_set_port."));

	/* Put the default IP in the first slot */
	for (i=0; i<info->alias_cnt; ++i) {
	    if (pj_sockaddr_cmp(&info->aliases[i], &def_addr)==0) {
			if (i!=0) {
				pj_sockaddr_cp(&info->aliases[i], &info->aliases[0]);
				pj_sockaddr_cp(&info->aliases[0], &def_addr);
			}
			break;
	    }
	}
	PJ_LOG(4, ("stun_sock.c", "pj_stun_sock_get_info() pj_sockaddr_cp."));
    }

    return PJ_SUCCESS;
}

/* Send application data */
PJ_DEF(pj_status_t) pj_stun_sock_sendto( pj_stun_sock *stun_sock,
					 pj_ioqueue_op_key_t *send_key,
					 const void *pkt,
					 unsigned pkt_len,
					 unsigned flag,
					 const pj_sockaddr_t *dst_addr,
					 unsigned addr_len)
{
	pj_ssize_t size;
	pj_status_t status;
	pj_bool_t is_stun = PJ_FALSE;
	pj_bool_t is_tnl_data = PJ_FALSE;
    PJ_ASSERT_RETURN(stun_sock && pkt && dst_addr && addr_len, PJ_EINVAL);
    
    if (send_key==NULL)
	send_key = &stun_sock->send_key;

	size = pkt_len;

	if (!stun_sock || !stun_sock->active_sock)
		return PJ_EINVALIDOP;

	/* Check that this is STUN message */
	status = pj_stun_msg_check((const pj_uint8_t*)pkt, size, 
		PJ_STUN_IS_DATAGRAM | PJ_STUN_CHECK_PACKET);
	if (status == PJ_SUCCESS) 
		is_stun = PJ_TRUE;
	else
		is_tnl_data = (((pj_uint8_t*)pkt)[pkt_len] == 1);

	if(is_stun || !is_tnl_data) {
		pj_ioqueue_op_key_t *op_key = (pj_ioqueue_op_key_t*)malloc(sizeof(pj_ioqueue_op_key_t));
		pj_ioqueue_op_key_init(op_key, sizeof(pj_ioqueue_op_key_t));
		return  pj_activesock_sendto(stun_sock->active_sock, op_key,
			pkt, &size, (flag | PJ_IOQUEUE_URGENT_DATA), dst_addr, addr_len);
	} else {
		return  pj_activesock_sendto(stun_sock->active_sock, &stun_sock->send_key,
			pkt, &size, flag, dst_addr, addr_len);
	}
}

/* This callback is called by the STUN session to send packet */
static pj_status_t sess_on_send_msg(pj_stun_session *sess,
				    void *token,
				    const void *pkt,
				    pj_size_t pkt_size,
				    const pj_sockaddr_t *dst_addr,
				    unsigned addr_len)
{
    pj_stun_sock *stun_sock;
	pj_ssize_t size;
	pj_status_t status;
	pj_bool_t is_stun = PJ_FALSE;
	pj_bool_t is_tnl_data = PJ_FALSE;

    stun_sock = (pj_stun_sock *) pj_stun_session_get_user_data(sess);
    if (!stun_sock || !stun_sock->active_sock)
	return PJ_EINVALIDOP;

    pj_assert(token==INTERNAL_MSG_TOKEN);
	PJ_UNUSED_ARG(token);

	/* Check that this is STUN message */
	status = pj_stun_msg_check((const pj_uint8_t*)pkt, pkt_size, 
		/*PJ_STUN_IS_DATAGRAM | */PJ_STUN_CHECK_PACKET);
	if (status == PJ_SUCCESS) 
		is_stun = PJ_TRUE;
	else
		is_tnl_data = (((pj_uint8_t*)pkt)[pkt_size] == 1);

	size = pkt_size;
	if(is_stun || !is_tnl_data) {
		pj_ioqueue_op_key_t *op_key = (pj_ioqueue_op_key_t*)malloc(sizeof(pj_ioqueue_op_key_t));
		pj_ioqueue_op_key_init(op_key, sizeof(pj_ioqueue_op_key_t));
		status = pj_activesock_sendto(stun_sock->active_sock, op_key,
			pkt, &size, PJ_IOQUEUE_URGENT_DATA, dst_addr, addr_len);
	} else {
		status = pj_activesock_sendto(stun_sock->active_sock, &stun_sock->send_key,
			pkt, &size, 0, dst_addr, addr_len);
	}

	return status;
#if 0
	return pj_activesock_sendto(stun_sock->active_sock, 
				&stun_sock->int_send_key,
				pkt, &size, 0, dst_addr, addr_len);
#endif
}

/* This callback is called by the STUN session when outgoing transaction 
 * is complete
 */
static void sess_on_request_complete(pj_stun_session *sess,
				     pj_status_t status,
				     void *token,
				     pj_stun_tx_data *tdata,
				     const pj_stun_msg *response,
				     const pj_sockaddr_t *src_addr,
				     unsigned src_addr_len)
{
    pj_stun_sock *stun_sock;
    const pj_stun_sockaddr_attr *mapped_attr;
	pj_stun_sock_op op;
    pj_bool_t mapped_changed;
	pj_bool_t resched = PJ_TRUE;

	stun_sock = (pj_stun_sock *) pj_stun_session_get_user_data(sess);
    if (!stun_sock)
	return;

    PJ_UNUSED_ARG(tdata);
    PJ_UNUSED_ARG(token);
    PJ_UNUSED_ARG(src_addr);
    PJ_UNUSED_ARG(src_addr_len);

    /* Check if this is a keep-alive or the first Binding request */
    if (pj_sockaddr_has_addr(&stun_sock->mapped_addr))
	op = PJ_STUN_SOCK_KEEP_ALIVE_OP;
    else
	op = PJ_STUN_SOCK_BINDING_OP;

    /* Handle failure */
    if (status != PJ_SUCCESS) {
	resched = sess_fail(stun_sock, op, status);
	goto on_return;
    }

    /* Get XOR-MAPPED-ADDRESS, or MAPPED-ADDRESS when XOR-MAPPED-ADDRESS
     * doesn't exist.
     */
    mapped_attr = (const pj_stun_sockaddr_attr*)
		  pj_stun_msg_find_attr(response, PJ_STUN_ATTR_XOR_MAPPED_ADDR,
					0);
    if (mapped_attr==NULL) {
	mapped_attr = (const pj_stun_sockaddr_attr*)
		      pj_stun_msg_find_attr(response, PJ_STUN_ATTR_MAPPED_ADDR,
					0);
    }

    if (mapped_attr == NULL) {
	resched = sess_fail(stun_sock, op, PJNATH_ESTUNNOMAPPEDADDR);
	goto on_return;
	}

    /* Determine if mapped address has changed, and save the new mapped
     * address and call callback if so 
     */
    mapped_changed = !pj_sockaddr_has_addr(&stun_sock->mapped_addr) ||
		     pj_sockaddr_cmp(&stun_sock->mapped_addr, 
				     &mapped_attr->sockaddr) != 0;
    if (mapped_changed) {
	/* Print mapped adress */
	{
	    char addrinfo[PJ_INET6_ADDRSTRLEN+10];
	    PJ_LOG(2,(stun_sock->obj_name, 
		      "STUN mapped address found/changed: %s",
		      pj_sockaddr_print(&mapped_attr->sockaddr,
					addrinfo, sizeof(addrinfo), 3)));
	}


	pj_sockaddr_cp(&stun_sock->mapped_addr, &mapped_attr->sockaddr);

	if (op==PJ_STUN_SOCK_KEEP_ALIVE_OP) {
	    op = PJ_STUN_SOCK_MAPPED_ADDR_CHANGE;
		PJ_LOG(2, (THIS_FILE, "sess_on_rquest_complete() Operation is PJ_STUN_SOCK_MAPPED_ADDR_CHANGE."));
	}
    }

	// 2013-10-16 DEAN
	// 2013-10-21 DEAN
	{
		int addr_len = sizeof(stun_sock->current_local_addr);
		char addrinfo1[PJ_INET6_ADDRSTRLEN+10];
		char addrinfo2[PJ_INET6_ADDRSTRLEN+10];
		pj_sock_getsockname(stun_sock->sock_fd, &stun_sock->current_local_addr,
			&addr_len);
		PJ_LOG(6,(stun_sock->obj_name, 
			"Current Local address: %s",
			pj_sockaddr_print(&stun_sock->current_local_addr,
			addrinfo1, sizeof(addrinfo1), 3)));

		/*
		 * Find out which interface is used to send to the server.
		 */
		status = get_local_interface(&stun_sock->srv_addr, &((pj_sockaddr_in *)(&stun_sock->current_local_addr))->sin_addr);
		PJ_LOG(6,(stun_sock->obj_name, 
			"Current Local address: %s",
			pj_sockaddr_print(&stun_sock->current_local_addr,
			addrinfo2, sizeof(addrinfo2), 3)));
	}

    /* Notify user */
	resched = (*stun_sock->cb.on_status)(stun_sock, op, PJ_SUCCESS);
	PJ_LOG(5, (THIS_FILE, "sess_on_request_complete() resched=%d.", resched));

on_return:
    /* Start/restart keep-alive timer */
    if (resched)
	start_ka_timer(stun_sock);
}

/* Schedule keep-alive timer */
static void start_ka_timer(pj_stun_sock *stun_sock)
{
    if (stun_sock->ka_timer.id != 0) {
	pj_timer_heap_cancel(stun_sock->stun_cfg.timer_heap, 
			     &stun_sock->ka_timer);
	stun_sock->ka_timer.id = 0;
    }

    pj_assert(stun_sock->ka_interval != 0);
    if (stun_sock->ka_interval > 0) {
	pj_time_val delay;

	delay.sec = stun_sock->ka_interval;
	delay.msec = 0;

	if (pj_timer_heap_schedule(stun_sock->stun_cfg.timer_heap, 
				   &stun_sock->ka_timer, 
				   &delay) == PJ_SUCCESS)
	{
	    stun_sock->ka_timer.id = PJ_TRUE;
	}
    }
}

/* Keep-alive timer callback */
static void ka_timer_cb(pj_timer_heap_t *th, pj_timer_entry *te)
{
    pj_stun_sock *stun_sock;

    stun_sock = (pj_stun_sock *) te->user_data;

    PJ_UNUSED_ARG(th);

    /* Time to send STUN Binding request */
    if (get_mapped_addr(stun_sock) != PJ_SUCCESS)
	return;

    /* Next keep-alive timer will be scheduled once the request
     * is complete.
     */
}

/* Callback from active socket when incoming packet is received */
static pj_bool_t on_data_recvfrom(pj_activesock_t *asock,
				  void *data,
				  pj_size_t size,
				  const pj_sockaddr_t *src_addr,
				  int addr_len,
				  pj_status_t status)
{
    pj_stun_sock *stun_sock;
    pj_stun_msg_hdr *hdr;
	pj_uint16_t type;

    stun_sock = (pj_stun_sock*) pj_activesock_get_user_data(asock);
    if (!stun_sock)
	return PJ_FALSE;

    /* Log socket error */
    if (status != PJ_SUCCESS) {
		PJ_PERROR(2,(stun_sock->obj_name, status, "recvfrom() error"));
	return PJ_TRUE;
    }

    /* Check that this is STUN message */
    status = pj_stun_msg_check((const pj_uint8_t*)data, size, 
    			       PJ_STUN_IS_DATAGRAM | PJ_STUN_CHECK_PACKET);
    if (status != PJ_SUCCESS) {
		/* Not STUN -- give it to application */
	goto process_app_data;
    }

    /* Treat packet as STUN header and copy the STUN message type.
     * We don't want to access the type directly from the header
     * since it may not be properly aligned.
     */
    hdr = (pj_stun_msg_hdr*) data;
    pj_memcpy(&type, &hdr->type, 2);
    type = pj_ntohs(type);

    /* If the packet is a STUN Binding response and part of the
     * transaction ID matches our internal ID, then this is
     * our internal STUN message (Binding request or keep alive).
     * Give it to our STUN session.
     */
    if (!PJ_STUN_IS_RESPONSE(type) ||
	PJ_STUN_GET_METHOD(type) != PJ_STUN_BINDING_METHOD ||
	pj_memcmp(hdr->tsx_id, stun_sock->tsx_id, 10) != 0) 
    {
	/* Not STUN Binding response, or STUN transaction ID mismatch.
	 * This is not our message too -- give it to application.
	 */
	goto process_app_data;
    }

    /* This is our STUN Binding response. Give it to the STUN session */
    status = pj_stun_session_on_rx_pkt(stun_sock->stun_sess, data, size,
				       PJ_STUN_IS_DATAGRAM, NULL, NULL,
				       src_addr, addr_len);
    return status!=PJNATH_ESTUNDESTROYED ? PJ_TRUE : PJ_FALSE;

process_app_data:
    if (stun_sock->cb.on_rx_data) {
	pj_bool_t ret;

	ret = (*stun_sock->cb.on_rx_data)(stun_sock, data, size,
					  src_addr, addr_len);
	return ret;
    }

    return PJ_TRUE;
}

/* Callback from active socket about send status */
static pj_bool_t on_data_sent(pj_activesock_t *asock,
			      pj_ioqueue_op_key_t *send_key,
			      pj_ssize_t sent)
{
    pj_stun_sock *stun_sock;

    stun_sock = (pj_stun_sock*) pj_activesock_get_user_data(asock);
    if (!stun_sock)
	return PJ_FALSE;

    /* Don't report to callback if this is internal message */
    if (send_key == &stun_sock->int_send_key) {
	return PJ_TRUE;
    }

    /* Report to callback */
    if (stun_sock->cb.on_data_sent) {
	pj_bool_t ret;

	/* If app gives NULL send_key in sendto() function, then give
	 * NULL in the callback too 
	 */
	if (send_key == &stun_sock->send_key)
	    send_key = NULL;

	/* Call callback */
	ret = (*stun_sock->cb.on_data_sent)(stun_sock, send_key, sent);

	return ret;
    }

    return PJ_TRUE;
}

PJ_DEF(pj_sockaddr*) pj_stun_sock_get_mapped_addr(pj_stun_sock *stun_sock) {
	return &stun_sock->mapped_addr;
}

PJ_DEF(pj_sockaddr*) pj_stun_sock_get_previous_local_addr(pj_stun_sock *stun_sock) {
	return &stun_sock->previous_local_addr;
}

PJ_DEF(void) pj_stun_sock_set_previous_local_addr(pj_stun_sock *stun_sock, pj_sockaddr* local_addr) {
	pj_sockaddr_cp(&stun_sock->previous_local_addr, local_addr);
}

PJ_DEF(int) pj_stun_sock_get_addr_family(pj_stun_sock *stun_sock) {
	return stun_sock->af;
}
