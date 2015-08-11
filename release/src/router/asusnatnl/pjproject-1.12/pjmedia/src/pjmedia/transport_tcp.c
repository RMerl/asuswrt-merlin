/* $Id: transport_tcp.c 3745 2011-09-08 06:47:28Z bennylp $ */
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
#include <pjmedia/natnl_stream.h>
#include <pjmedia/transport_tcp.h>
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/ioqueue.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pj/string.h>

#define THIS_FILE "transport_tcp.c"


/* Maximum size of incoming RTP packet */
#define RTP_LEN	    PJMEDIA_MAX_MTU

/* Maximum size of incoming RTCP packet */
#define RTCP_LEN    600

/* Maximum pending write operations */
#define MAX_PENDING 4

static const pj_str_t ID_RTP_AVP  = { "RTP/AVP", 7 };

/* Pending write buffer */
typedef struct pending_write
{
    char		buffer[RTP_LEN];
    pj_ioqueue_op_key_t	op_key;
} pending_write;


struct transport_tcp
{
    pjmedia_transport	base;		/**< Base transport.		    */

    pj_pool_t	       *pool;		/**< Memory pool		    */
    unsigned		options;	/**< Transport options.		    */
    unsigned		media_options;	/**< Transport media options.	    */
    void	       *user_data;	/**< Only valid when attached	    */
    pj_bool_t		attached;	/**< Has attachment?		    */
    pj_sockaddr		rem_rtp_addr;	/**< Remote RTP address		    */
    pj_sockaddr		rem_rtcp_addr;	/**< Remote RTCP address	    */
    int			addr_len;	/**< Length of addresses.	    */
    void  (*rtp_cb)(	void*,		/**< To report incoming RTP.	    */
			void*,
			pj_ssize_t);
    void  (*rtcp_cb)(	void*,		/**< To report incoming RTCP.	    */
			void*,
			pj_ssize_t);

    unsigned		tx_drop_pct;	/**< Percent of tx pkts to drop.    */
    unsigned		rx_drop_pct;	/**< Percent of rx pkts to drop.    */

    pj_sock_t	        rtp_sock;	/**< RTP socket			    */
    pj_sockaddr		rtp_addr_name;	/**< Published RTP address.	    */
	pj_ioqueue_key_t   *rtp_key;	/**< RTP socket key in ioqueue	    */
	pj_ioqueue_op_key_t	rtp_read_op;	/**< Pending read operation	    */
    
	unsigned		rtp_write_op_id;/**< Next write_op to use	    */
    pending_write	rtp_pending_write[MAX_PENDING];  /**< Pending write */
    pj_sockaddr		rtp_src_addr;	/**< Actual packet src addr.	    */
    unsigned		rtp_src_cnt;	/**< How many pkt from this addr.   */
    int			rtp_addrlen;	/**< Address length.		    */
    char		rtp_pkt[RTP_LEN];/**< Incoming RTP packet buffer    */

    pj_sock_t		rtcp_sock;	/**< RTCP socket		    */
    pj_sockaddr		rtcp_addr_name;	/**< Published RTCP address.	    */
    pj_sockaddr		rtcp_src_addr;	/**< Actual source RTCP address.    */
    unsigned		rtcp_src_cnt;	/**< How many pkt from this addr.   */
    int			rtcp_addr_len;	/**< Length of RTCP src address.    */
    pj_ioqueue_key_t   *rtcp_key;	/**< RTCP socket key in ioqueue	    */
    pj_ioqueue_op_key_t rtcp_read_op;	/**< Pending read operation	    */
    pj_ioqueue_op_key_t rtcp_write_op;	/**< Pending write operation	    */
    char		rtcp_pkt[RTCP_LEN];/**< Incoming RTCP packet buffer */

	pj_activesock_t     *listen_asock;
	pj_ioqueue_t        *ioqueue;
	pj_activesock_t     *incoming_asock;
};


static void on_rx_rtp( pj_ioqueue_key_t *key, 
                       pj_ioqueue_op_key_t *op_key, 
                       pj_ssize_t bytes_read);
static void on_rx_rtcp(pj_ioqueue_key_t *key, 
                       pj_ioqueue_op_key_t *op_key, 
					   pj_ssize_t bytes_read);

/* activesock call back*/
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

/* For tcp server */
static pj_bool_t on_accept_complete(pj_activesock_t *asock,
									pj_sock_t sock,
									const pj_sockaddr_t *src_addr,
									int src_addr_len);

/*
 * These are media transport operations.
 */
static pj_status_t transport_get_info (pjmedia_transport *tp,
				       pjmedia_transport_info *info);
static pj_status_t transport_attach   (pjmedia_transport *tp,
				       void *user_data,
				       const pj_sockaddr_t *rem_addr,
				       const pj_sockaddr_t *rem_rtcp,
				       unsigned addr_len,
				       void (*rtp_cb)(void*,
						      void*,
						      pj_ssize_t),
				       void (*rtcp_cb)(void*,
						       void*,
						       pj_ssize_t));
static void	   transport_detach   (pjmedia_transport *tp,
				       void *strm);
static pj_status_t transport_send_rtp( pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size);
static pj_status_t transport_send_rtcp(pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size);
static pj_status_t transport_send_rtcp2(pjmedia_transport *tp,
				       const pj_sockaddr_t *addr,
				       unsigned addr_len,
				       const void *pkt,
				       pj_size_t size);
static pj_status_t transport_media_create(pjmedia_transport *tp,
				       pj_pool_t *pool,
				       unsigned options,
				       const pjmedia_sdp_session *sdp_remote,
				       unsigned media_index);
static pj_status_t transport_encode_sdp(pjmedia_transport *tp,
				        pj_pool_t *pool,
				        pjmedia_sdp_session *sdp_local,
				        const pjmedia_sdp_session *rem_sdp,
				        unsigned media_index);
static pj_status_t transport_media_start (pjmedia_transport *tp,
				       pj_pool_t *pool,
				       const pjmedia_sdp_session *sdp_local,
				       const pjmedia_sdp_session *sdp_remote,
				       unsigned media_index);
static pj_status_t transport_media_stop(pjmedia_transport *tp);
static pj_status_t transport_simulate_lost(pjmedia_transport *tp,
				       pjmedia_dir dir,
				       unsigned pct_lost);
static pj_status_t transport_destroy  (pjmedia_transport *tp);


static pjmedia_transport_op transport_tcp_op = 
{
    &transport_get_info,
    &transport_attach,
    &transport_detach,
    &transport_send_rtp,
    &transport_send_rtcp,
    &transport_send_rtcp2,
    &transport_media_create,
    &transport_encode_sdp,
    &transport_media_start,
    &transport_media_stop,
    &transport_simulate_lost,
    &transport_destroy
};


/**
 * Create TCP stream transport.
 */
PJ_DEF(pj_status_t) pjmedia_transport_tcp_create( pjmedia_endpt *endpt,
						  const char *name,
						  int port,
						  unsigned options,
						  pjmedia_transport **p_tp)
{
    return pjmedia_transport_tcp_create2(endpt, name, NULL, port, options, 
					p_tp);
}

/**
 * Create TCP stream transport.
 */
PJ_DEF(pj_status_t) pjmedia_transport_tcp_create2(pjmedia_endpt *endpt,
						  const char *name,
						  const pj_str_t *addr,
						  int port,
						  unsigned options,
						  pjmedia_transport **p_tp)
{
    return pjmedia_transport_tcp_create3(endpt, pj_AF_INET(), name,
					 addr, port, options, p_tp);
}

/**
 * Create TCP stream transport.
 */
PJ_DEF(pj_status_t) pjmedia_transport_tcp_create3(pjmedia_endpt *endpt,
						  int af,
						  const char *name,
						  const pj_str_t *addr,
						  int port,
						  unsigned options,
						  pjmedia_transport **p_tp)
{
    pjmedia_sock_info si;
    pj_status_t status;

    
    /* Sanity check */
    PJ_ASSERT_RETURN(endpt && port && p_tp, PJ_EINVAL);


    pj_bzero(&si, sizeof(pjmedia_sock_info));
    si.rtp_sock = si.rtcp_sock = PJ_INVALID_SOCKET;

    /* Create RTP socket */
    status = pj_sock_socket(af, pj_SOCK_DGRAM(), 0, &si.rtp_sock);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Bind RTP socket */
    status = pj_sockaddr_init(af, &si.rtp_addr_name, addr, (pj_uint16_t)port);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_sock_bind(si.rtp_sock, &si.rtp_addr_name, 
			  pj_sockaddr_get_len(&si.rtp_addr_name));
    if (status != PJ_SUCCESS)
	goto on_error;


    /* Create RTCP socket */
    status = pj_sock_socket(af, pj_SOCK_DGRAM(), 0, &si.rtcp_sock);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Bind RTCP socket */
    status = pj_sockaddr_init(af, &si.rtcp_addr_name, addr, 
			      (pj_uint16_t)(port+1));
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_sock_bind(si.rtcp_sock, &si.rtcp_addr_name,
			  pj_sockaddr_get_len(&si.rtcp_addr_name));
    if (status != PJ_SUCCESS)
	goto on_error;

    
    /* Create TCP transport by attaching socket info */
    return pjmedia_transport_tcp_attach( endpt, name, &si, options, p_tp);


on_error:
    if (si.rtp_sock != PJ_INVALID_SOCKET)
	pj_sock_close(si.rtp_sock);
    if (si.rtcp_sock != PJ_INVALID_SOCKET)
	pj_sock_close(si.rtcp_sock);
    return status;
}


/**
 * Create TCP stream transport from existing socket info.
 */
PJ_DEF(pj_status_t) pjmedia_transport_tcp_attach( pjmedia_endpt *endpt,
						  const char *name,
						  const pjmedia_sock_info *si,
						  unsigned options,
						  pjmedia_transport **p_tp)
{
    struct transport_tcp *tp;
    pj_pool_t *pool;
    //pj_ioqueue_t *ioqueue;
    pj_ssize_t size;
	pj_status_t status;
	pj_sockaddr *listener_addr;
	pj_activesock_cfg asock_cfg;
	pj_activesock_cb listen_cb;
	long sobuf_size;
	char buf[PJ_INET6_ADDRSTRLEN+10];

	PJ_UNUSED_ARG(listener_addr);
	PJ_UNUSED_ARG(size);


    /* Sanity check */
    PJ_ASSERT_RETURN(endpt && si && p_tp, PJ_EINVAL);

    if (name==NULL)
	name = "tcp%p";

    /* Create transport structure */
    pool = pjmedia_endpt_create_pool(endpt, name, 512, 512);
    if (!pool)
	return PJ_ENOMEM;

    tp = PJ_POOL_ZALLOC_T(pool, struct transport_tcp);
    tp->pool = pool;
    tp->options = options;
    pj_memcpy(tp->base.name, pool->obj_name, PJ_MAX_OBJ_NAME);
    tp->base.op = &transport_tcp_op;
	tp->base.type = PJMEDIA_TRANSPORT_TYPE_TCP;
	/* Get ioqueue instance */
	tp->ioqueue = pjmedia_endpt_get_ioqueue(endpt);

    /* Copy socket infos */
    tp->rtp_sock = si->tcp_rtp_sock;
    tp->rtp_addr_name = si->tcp_rtp_addr_name;
#if 0
    tp->rtcp_sock = si->rtcp_sock;
    tp->rtcp_addr_name = si->rtcp_addr_name;
#endif

    /* If address is 0.0.0.0, use host's IP address */
    if (!pj_sockaddr_has_addr(&tp->rtp_addr_name)) {
	pj_sockaddr hostip;

	status = pj_gethostip(tp->rtp_addr_name.addr.sa_family, &hostip);
	if (status != PJ_SUCCESS)
	    goto on_error;

	pj_memcpy(pj_sockaddr_get_addr(&tp->rtp_addr_name), 
		  pj_sockaddr_get_addr(&hostip),
		  pj_sockaddr_get_addr_len(&hostip));
    }
#if 0
    /* Same with RTCP */
    if (!pj_sockaddr_has_addr(&tp->rtcp_addr_name)) {
	pj_memcpy(pj_sockaddr_get_addr(&tp->rtcp_addr_name),
		  pj_sockaddr_get_addr(&tp->rtp_addr_name),
		  pj_sockaddr_get_addr_len(&tp->rtp_addr_name));
    }
#endif
#if 1 // natnl set stun socket recv and send buffer size.
	sobuf_size = PJ_STUN_SOCK_PKT_LEN;
	status = pj_sock_setsockopt(tp->rtp_sock, pj_SOL_SOCKET(), pj_SO_RCVBUF(),
		&sobuf_size, sizeof(sobuf_size));
	if (status != PJ_SUCCESS)
		goto on_error;

	status = pj_sock_setsockopt(tp->rtp_sock, pj_SOL_SOCKET(), pj_SO_SNDBUF(),
		&sobuf_size, sizeof(sobuf_size));
	if (status != PJ_SUCCESS)
		goto on_error;
#endif

	/* Start listening to the address */
	status = pj_sock_listen(tp->rtp_sock, PJSIP_TCP_TRANSPORT_BACKLOG);
	if (status != PJ_SUCCESS)
		goto on_error;

	/* Create active socket */
	pj_activesock_cfg_default(&asock_cfg);
	asock_cfg.whole_data = PJ_TRUE;
	asock_cfg.concurrency = 1;

	pj_bzero(&listen_cb, sizeof(listen_cb));
	listen_cb.on_accept_complete = &on_accept_complete;
	status = pj_activesock_create(pool, tp->rtp_sock, pj_SOCK_STREAM(), &asock_cfg,
		                          tp->ioqueue, &listen_cb, tp, &tp->listen_asock);
	if (status != PJ_SUCCESS)
		goto on_error;

	/* Start pending accept() operations */
	status = pj_activesock_start_accept(tp->listen_asock, pool);
	if (status != PJ_SUCCESS)
		goto on_error;

	PJ_LOG(4, (THIS_FILE, 
		"Transport TCP listener ready for incoming connections at %s",
		pj_sockaddr_print(&tp->rtp_addr_name, buf, sizeof(buf), 3)));

    /* Done */
    *p_tp = &tp->base;
    return PJ_SUCCESS;


on_error:
    transport_destroy(&tp->base);
    return status;
}


/**
 * Close TCP transport.
 */
static pj_status_t transport_destroy(pjmedia_transport *tp)
{
    struct transport_tcp *tcp = (struct transport_tcp*) tp;

    /* Sanity check */
    PJ_ASSERT_RETURN(tp, PJ_EINVAL);

    /* Must not close while application is using this */
    //PJ_ASSERT_RETURN(!tcp->attached, PJ_EINVALIDOP);
    

    if (tcp->rtp_key) {
	/* This will block the execution if callback is still
	 * being called.
	 */
	pj_ioqueue_unregister(tcp->rtp_key);
	tcp->rtp_key = NULL;
	tcp->rtp_sock = PJ_INVALID_SOCKET;
    } else if (tcp->rtp_sock != PJ_INVALID_SOCKET) {
	pj_sock_close(tcp->rtp_sock);
	tcp->rtp_sock = PJ_INVALID_SOCKET;
    }

    if (tcp->rtcp_key) {
	pj_ioqueue_unregister(tcp->rtcp_key);
	tcp->rtcp_key = NULL;
	tcp->rtcp_sock = PJ_INVALID_SOCKET;
    } else if (tcp->rtcp_sock != PJ_INVALID_SOCKET) {
	pj_sock_close(tcp->rtcp_sock);
	tcp->rtcp_sock = PJ_INVALID_SOCKET;
    }

    pj_pool_release(tcp->pool);

    return PJ_SUCCESS;
}


static pj_bool_t on_data_read(pj_activesock_t *asock,
							  void *data,
							  pj_size_t size,
							  pj_status_t status,
							  pj_size_t *remainder)
{
	PJ_UNUSED_ARG(asock);
	PJ_UNUSED_ARG(data);
	PJ_UNUSED_ARG(size);
	PJ_UNUSED_ARG(status);
	PJ_UNUSED_ARG(remainder);
	return PJ_TRUE;
}

/* Callback when packet is sent */
static pj_bool_t on_data_sent(pj_activesock_t *asock,
							  pj_ioqueue_op_key_t *send_key,
							  pj_ssize_t sent)
{
	PJ_UNUSED_ARG(asock);
	PJ_UNUSED_ARG(send_key);
	PJ_UNUSED_ARG(sent);
	return PJ_TRUE;
}

static pj_bool_t on_connect_complete(pj_activesock_t *asock,
									 pj_status_t status)
{
	PJ_UNUSED_ARG(asock);
	PJ_UNUSED_ARG(status);
	return PJ_TRUE;
}

/* For tcp server */
static pj_bool_t on_accept_complete(pj_activesock_t *asock,
									pj_sock_t sock,
									const pj_sockaddr_t *src_addr,
									int src_addr_len)
{

	struct transport_tcp *tp;
	char addr[PJ_INET6_ADDRSTRLEN+10];
	char addr1[PJ_INET6_ADDRSTRLEN+10];
    //pjsip_tp_state_callback state_cb;
    pj_status_t status;

	pj_sockaddr peer_addr;
	int rem_addr_len = sizeof(peer_addr);
	pj_sockaddr local_addr;
	int local_addr_len = sizeof(local_addr);

	pj_activesock_cfg asock_cfg;
	pj_activesock_cb tcp_callback;

	status = pj_sock_getpeername(sock, (pj_sockaddr_t *)&peer_addr, &rem_addr_len);
	status = pj_sock_getsockname(sock, (pj_sockaddr_t *)&local_addr, &local_addr_len);

	PJ_UNUSED_ARG(src_addr);
	PJ_UNUSED_ARG(src_addr_len);
	PJ_UNUSED_ARG(addr);
	PJ_UNUSED_ARG(addr1);

	/* Create active socket */
	pj_activesock_cfg_default(&asock_cfg);
	asock_cfg.async_cnt = 1;
	asock_cfg.whole_data = PJ_TRUE;
	asock_cfg.concurrency = 1;

	pj_bzero(&tcp_callback, sizeof(tcp_callback));

	tcp_callback.on_data_read = &on_data_read;
	tcp_callback.on_data_sent = &on_data_sent;
	tcp_callback.on_connect_complete = &on_connect_complete;
	
	tp = pj_activesock_get_user_data(asock);
	status = pj_activesock_create(tp->pool, sock, pj_SOCK_STREAM(), &asock_cfg,
		tp->ioqueue, &tcp_callback, 
		tp, &tp->incoming_asock);
	if (status != PJ_SUCCESS) {
		return PJ_FALSE;
	}

	status = pj_activesock_start_read(tp->incoming_asock, 
		tp->pool, PJ_TCP_MAX_PKT_LEN, 0);
	if (status != PJ_SUCCESS && status != PJ_EPENDING) {
		PJ_LOG(4, (THIS_FILE, 
			"pj_activesock_start_read() error, status=%d", 
			status));
		return status;
	}

    return PJ_TRUE;
}

/* Notification from ioqueue about incoming RTP packet */
static void on_rx_rtp( pj_ioqueue_key_t *key, 
                       pj_ioqueue_op_key_t *op_key, 
                       pj_ssize_t bytes_read)
{
    struct transport_tcp *tcp;
    pj_status_t status;

    PJ_UNUSED_ARG(op_key);

    tcp = (struct transport_tcp*) pj_ioqueue_get_user_data(key);

    do {
	void (*cb)(void*,void*,pj_ssize_t);
	void *user_data;
	pj_bool_t discard = PJ_FALSE;

	cb = tcp->rtp_cb;
	user_data = tcp->user_data;

	/* Simulate packet lost on RX direction */
	if (tcp->rx_drop_pct) {
	    if ((pj_rand() % 100) <= (int)tcp->rx_drop_pct) {
		PJ_LOG(5,(tcp->base.name, 
			  "RX RTP packet dropped because of pkt lost "
			  "simulation"));
		discard = PJ_TRUE;
	    }
	}

	/* See if source address of RTP packet is different than the 
	 * configured address, and switch RTP remote address to 
	 * source packet address after several consecutive packets
	 * have been received.
	 */
	if (bytes_read>0 && 
	    (tcp->options & PJMEDIA_TCP_NO_SRC_ADDR_CHECKING)==0) 
	{
	    if (pj_sockaddr_cmp(&tcp->rem_rtp_addr, &tcp->rtp_src_addr) == 0) {
		/* We're still receiving from rem_rtp_addr. Don't switch. */
		tcp->rtp_src_cnt = 0;
	    } else {
		tcp->rtp_src_cnt++;

		if (tcp->rtp_src_cnt < PJMEDIA_RTP_NAT_PROBATION_CNT) {
		    discard = PJ_TRUE;
		} else {
		
		    char addr_text[80];

		    /* Set remote RTP address to source address */
		    pj_memcpy(&tcp->rem_rtp_addr, &tcp->rtp_src_addr,
			      sizeof(pj_sockaddr));

		    /* Reset counter */
		    tcp->rtp_src_cnt = 0;

		    PJ_LOG(4,(tcp->base.name,
			      "Remote RTP address switched to %s",
			      pj_sockaddr_print(&tcp->rtp_src_addr, addr_text,
						sizeof(addr_text), 3)));

		    /* Also update remote RTCP address if actual RTCP source
		     * address is not heard yet.
		     */
		    if (!pj_sockaddr_has_addr(&tcp->rtcp_src_addr)) {
			pj_uint16_t port;

			pj_memcpy(&tcp->rem_rtcp_addr, &tcp->rem_rtp_addr, 
				  sizeof(pj_sockaddr));
			pj_sockaddr_copy_addr(&tcp->rem_rtcp_addr,
					      &tcp->rem_rtp_addr);
			port = (pj_uint16_t)
			       (pj_sockaddr_get_port(&tcp->rem_rtp_addr)+1);
			pj_sockaddr_set_port(&tcp->rem_rtcp_addr, port);

			pj_memcpy(&tcp->rtcp_src_addr, &tcp->rem_rtcp_addr, 
				  sizeof(pj_sockaddr));

			PJ_LOG(4,(tcp->base.name,
				  "Remote RTCP address switched to predicted"
				  " address %s",
				  pj_sockaddr_print(&tcp->rtcp_src_addr, 
						    addr_text,
						    sizeof(addr_text), 3)));

		    }
		}
	    }
	}

	if (!discard && tcp->attached && cb)
	    (*cb)(user_data, tcp->rtp_pkt, bytes_read);

	bytes_read = sizeof(tcp->rtp_pkt);
	tcp->rtp_addrlen = sizeof(tcp->rtp_src_addr);
	status = pj_ioqueue_recvfrom(tcp->rtp_key, &tcp->rtp_read_op,
				     tcp->rtp_pkt, &bytes_read, 0,
				     &tcp->rtp_src_addr, 
				     &tcp->rtp_addrlen);

	if (status != PJ_EPENDING && status != PJ_SUCCESS)
	    bytes_read = -status;

    } while (status != PJ_EPENDING && status != PJ_ECANCELLED);
}


/* Notification from ioqueue about incoming RTCP packet */
static void on_rx_rtcp(pj_ioqueue_key_t *key, 
                       pj_ioqueue_op_key_t *op_key, 
                       pj_ssize_t bytes_read)
{
    struct transport_tcp *tcp;
    pj_status_t status;

    PJ_UNUSED_ARG(op_key);

    tcp = (struct transport_tcp*) pj_ioqueue_get_user_data(key);

    do {
	void (*cb)(void*,void*,pj_ssize_t);
	void *user_data;

	cb = tcp->rtcp_cb;
	user_data = tcp->user_data;

	if (tcp->attached && cb)
	    (*cb)(user_data, tcp->rtcp_pkt, bytes_read);

	/* Check if RTCP source address is the same as the configured
	 * remote address, and switch the address when they are
	 * different.
	 */
	if (bytes_read>0 &&
	    (tcp->options & PJMEDIA_TCP_NO_SRC_ADDR_CHECKING)==0)
	{
	    if (pj_sockaddr_cmp(&tcp->rem_rtcp_addr, &tcp->rtcp_src_addr) == 0) {
		/* Still receiving from rem_rtcp_addr, don't switch */
		tcp->rtcp_src_cnt = 0;
	    } else {
		++tcp->rtcp_src_cnt;

		if (tcp->rtcp_src_cnt >= PJMEDIA_RTCP_NAT_PROBATION_CNT	) {
		    char addr_text[80];

		    tcp->rtcp_src_cnt = 0;
		    pj_memcpy(&tcp->rem_rtcp_addr, &tcp->rtcp_src_addr,
			      sizeof(pj_sockaddr));

		    PJ_LOG(4,(tcp->base.name,
			      "Remote RTCP address switched to %s",
			      pj_sockaddr_print(&tcp->rtcp_src_addr, addr_text,
						sizeof(addr_text), 3)));
		}
	    }
	}

	bytes_read = sizeof(tcp->rtcp_pkt);
	tcp->rtcp_addr_len = sizeof(tcp->rtcp_src_addr);
	status = pj_ioqueue_recvfrom(tcp->rtcp_key, &tcp->rtcp_read_op,
				     tcp->rtcp_pkt, &bytes_read, 0,
				     &tcp->rtcp_src_addr, 
				     &tcp->rtcp_addr_len);
	if (status != PJ_EPENDING && status != PJ_SUCCESS)
	    bytes_read = -status;

    } while (status != PJ_EPENDING && status != PJ_ECANCELLED);
}


/* Called to get the transport info */
static pj_status_t transport_get_info(pjmedia_transport *tp,
				      pjmedia_transport_info *info)
{
    struct transport_tcp *tcp = (struct transport_tcp*)tp;
    PJ_ASSERT_RETURN(tp && info, PJ_EINVAL);

    info->sock_info.rtp_sock = tcp->rtp_sock;
    info->sock_info.rtp_addr_name = tcp->rtp_addr_name;
    info->sock_info.rtcp_sock = tcp->rtcp_sock;
    info->sock_info.rtcp_addr_name = tcp->rtcp_addr_name;

    /* Get remote address originating RTP & RTCP. */
    info->src_rtp_name  = tcp->rtp_src_addr;
    info->src_rtcp_name = tcp->rtcp_src_addr;

    return PJ_SUCCESS;
}


/* Called by application to initialize the transport */
static pj_status_t transport_attach(   pjmedia_transport *tp,
				       void *user_data,
				       const pj_sockaddr_t *rem_addr,
				       const pj_sockaddr_t *rem_rtcp,
				       unsigned addr_len,
				       void (*rtp_cb)(void*,
						      void*,
						      pj_ssize_t),
				       void (*rtcp_cb)(void*,
						       void*,
						       pj_ssize_t))
{
    struct transport_tcp *tcp = (struct transport_tcp*) tp;
    const pj_sockaddr *rtcp_addr;

    /* Validate arguments */
    PJ_ASSERT_RETURN(tp && rem_addr && addr_len, PJ_EINVAL);

    /* Must not be "attached" to existing application */
    PJ_ASSERT_RETURN(!tcp->attached, PJ_EINVALIDOP);

    /* Lock the ioqueue keys to make sure that callbacks are
     * not executed. See ticket #844 for details.
     */
    pj_ioqueue_lock_key(tcp->rtp_key);
    pj_ioqueue_lock_key(tcp->rtcp_key);

    /* "Attach" the application: */

    /* Copy remote RTP address */
    pj_memcpy(&tcp->rem_rtp_addr, rem_addr, addr_len);

    /* Copy remote RTP address, if one is specified. */
    rtcp_addr = (const pj_sockaddr*) rem_rtcp;
    if (rtcp_addr && pj_sockaddr_has_addr(rtcp_addr)) {
	pj_memcpy(&tcp->rem_rtcp_addr, rem_rtcp, addr_len);

    } else {
	unsigned rtcp_port;

	/* Otherwise guess the RTCP address from the RTP address */
	pj_memcpy(&tcp->rem_rtcp_addr, rem_addr, addr_len);
	rtcp_port = pj_sockaddr_get_port(&tcp->rem_rtp_addr) + 1;
	pj_sockaddr_set_port(&tcp->rem_rtcp_addr, (pj_uint16_t)rtcp_port);
    }

    /* Save the callbacks */
    tcp->rtp_cb = rtp_cb;
    tcp->rtcp_cb = rtcp_cb;
    tcp->user_data = user_data;

    /* Save address length */
    tcp->addr_len = addr_len;

    /* Last, mark transport as attached */
    tcp->attached = PJ_TRUE;

    /* Reset source RTP & RTCP addresses and counter */
    pj_bzero(&tcp->rtp_src_addr, sizeof(tcp->rtp_src_addr));
    pj_bzero(&tcp->rtcp_src_addr, sizeof(tcp->rtcp_src_addr));
    tcp->rtp_src_cnt = 0;
    tcp->rtcp_src_cnt = 0;

    /* Unlock keys */
    pj_ioqueue_unlock_key(tcp->rtcp_key);
    pj_ioqueue_unlock_key(tcp->rtp_key);

    return PJ_SUCCESS;
}


/* Called by application when it no longer needs the transport */
static void transport_detach( pjmedia_transport *tp,
			      void *user_data)
{
    struct transport_tcp *tcp = (struct transport_tcp*) tp;

    pj_assert(tp);

    if (tcp->attached) {
	/* Lock the ioqueue keys to make sure that callbacks are
	 * not executed. See ticket #460 for details.
	 */
	pj_ioqueue_lock_key(tcp->rtp_key);
	pj_ioqueue_lock_key(tcp->rtcp_key);

	/* User data is unreferenced on Release build */
	PJ_UNUSED_ARG(user_data);

	/* As additional checking, check if the same user data is specified */
	pj_assert(user_data == tcp->user_data);

	/* First, mark transport as unattached */
	tcp->attached = PJ_FALSE;

	/* Clear up application infos from transport */
	tcp->rtp_cb = NULL;
	tcp->rtcp_cb = NULL;
	tcp->user_data = NULL;

	/* Unlock keys */
	pj_ioqueue_unlock_key(tcp->rtcp_key);
	pj_ioqueue_unlock_key(tcp->rtp_key);
    }
}


/* Called by application to send RTP packet */
static pj_status_t transport_send_rtp( pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size)
{
    struct transport_tcp *tcp = (struct transport_tcp*)tp;
    pj_ssize_t sent;
    unsigned id;
    struct pending_write *pw;
    pj_status_t status;

    /* Must be attached */
    PJ_ASSERT_RETURN(tcp->attached, PJ_EINVALIDOP);

    /* Check that the size is supported */
    PJ_ASSERT_RETURN(size <= RTP_LEN, PJ_ETOOBIG);

    /* Simulate packet lost on TX direction */
    if (tcp->tx_drop_pct) {
	if ((pj_rand() % 100) <= (int)tcp->tx_drop_pct) {
	    PJ_LOG(5,(tcp->base.name, 
		      "TX RTP packet dropped because of pkt lost "
		      "simulation"));
	    return PJ_SUCCESS;
	}
    }


    id = tcp->rtp_write_op_id;
    pw = &tcp->rtp_pending_write[id];

    /* We need to copy packet to our buffer because when the
     * operation is pending, caller might write something else
     * to the original buffer.
     */
    pj_memcpy(pw->buffer, pkt, size);

    sent = size;
    status = pj_ioqueue_sendto( tcp->rtp_key, 
				&tcp->rtp_pending_write[id].op_key,
				pw->buffer, &sent, 0,
				&tcp->rem_rtp_addr, 
				tcp->addr_len);

    tcp->rtp_write_op_id = (tcp->rtp_write_op_id + 1) %
			   PJ_ARRAY_SIZE(tcp->rtp_pending_write);

    if (status==PJ_SUCCESS || status==PJ_EPENDING)
	return PJ_SUCCESS;

    return status;
}

/* Called by application to send RTCP packet */
static pj_status_t transport_send_rtcp(pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size)
{
    return transport_send_rtcp2(tp, NULL, 0, pkt, size);
}


/* Called by application to send RTCP packet */
static pj_status_t transport_send_rtcp2(pjmedia_transport *tp,
					const pj_sockaddr_t *addr,
					unsigned addr_len,
				        const void *pkt,
				        pj_size_t size)
{
    struct transport_tcp *tcp = (struct transport_tcp*)tp;
    pj_ssize_t sent;
    pj_status_t status;

    PJ_ASSERT_RETURN(tcp->attached, PJ_EINVALIDOP);

    if (addr == NULL) {
	addr = &tcp->rem_rtcp_addr;
	addr_len = tcp->addr_len;
    }

    sent = size;
    status = pj_ioqueue_sendto( tcp->rtcp_key, &tcp->rtcp_write_op,
				pkt, &sent, 0, addr, addr_len);

    if (status==PJ_SUCCESS || status==PJ_EPENDING)
	return PJ_SUCCESS;

    return status;
}


static pj_status_t transport_media_create(pjmedia_transport *tp,
				  pj_pool_t *pool,
				  unsigned options,
				  const pjmedia_sdp_session *sdp_remote,
				  unsigned media_index)
{
    struct transport_tcp *tcp = (struct transport_tcp*)tp;
	tp->tunnel_type = NATNL_TUNNEL_TYPE_UPNP_TCP;
	//tp->call_id = media_index;

    PJ_ASSERT_RETURN(tp && pool, PJ_EINVAL);
    tcp->media_options = options;

    PJ_UNUSED_ARG(sdp_remote);
    PJ_UNUSED_ARG(media_index);

    return PJ_SUCCESS;
}

static pj_status_t transport_encode_sdp(pjmedia_transport *tp,
				        pj_pool_t *pool,
				        pjmedia_sdp_session *sdp_local,
				        const pjmedia_sdp_session *rem_sdp,
				        unsigned media_index)
{
    struct transport_tcp *tcp = (struct transport_tcp*)tp;

    /* Validate media transport */
    /* By now, this transport only support RTP/AVP transport */
    if ((tcp->media_options & PJMEDIA_TPMED_NO_TRANSPORT_CHECKING) == 0) {
	pjmedia_sdp_media *m_rem, *m_loc;

	m_rem = rem_sdp? rem_sdp->media[media_index] : NULL;
	m_loc = sdp_local->media[media_index];

	if (pj_stricmp(&m_loc->desc.transport, &ID_RTP_AVP) ||
	   (m_rem && pj_stricmp(&m_rem->desc.transport, &ID_RTP_AVP)))
	{
	    pjmedia_sdp_media_deactivate(pool, m_loc);
	    return PJMEDIA_SDP_EINPROTO;
	}
    }

    return PJ_SUCCESS;
}

static pj_status_t transport_media_start(pjmedia_transport *tp,
				  pj_pool_t *pool,
				  const pjmedia_sdp_session *sdp_local,
				  const pjmedia_sdp_session *sdp_remote,
				  unsigned media_index)
{
    PJ_ASSERT_RETURN(tp && pool && sdp_local, PJ_EINVAL);

    PJ_UNUSED_ARG(tp);
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(sdp_local);
    PJ_UNUSED_ARG(sdp_remote);
    PJ_UNUSED_ARG(media_index);

    return PJ_SUCCESS;
}

static pj_status_t transport_media_stop(pjmedia_transport *tp)
{
    PJ_UNUSED_ARG(tp);

    return PJ_SUCCESS;
}

static pj_status_t transport_simulate_lost(pjmedia_transport *tp,
					   pjmedia_dir dir,
					   unsigned pct_lost)
{
    struct transport_tcp *tcp = (struct transport_tcp*)tp;

    PJ_ASSERT_RETURN(tp && pct_lost <= 100, PJ_EINVAL);

    if (dir & PJMEDIA_DIR_ENCODING)
	tcp->tx_drop_pct = pct_lost;
    
    if (dir & PJMEDIA_DIR_DECODING)
	tcp->rx_drop_pct = pct_lost;

    return PJ_SUCCESS;
}

