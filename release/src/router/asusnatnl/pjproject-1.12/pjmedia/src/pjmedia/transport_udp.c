/* $Id: transport_udp.c 3745 2011-09-08 06:47:28Z bennylp $ */
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
#include <pjmedia/transport_udp.h>
#include <pj/addr_resolv.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/ioqueue.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pj/string.h>


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


struct transport_udp
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
};



static void on_rx_rtp( pj_ioqueue_key_t *key, 
                       pj_ioqueue_op_key_t *op_key, 
                       pj_ssize_t bytes_read);
static void on_rx_rtcp(pj_ioqueue_key_t *key, 
                       pj_ioqueue_op_key_t *op_key, 
                       pj_ssize_t bytes_read);

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


static pjmedia_transport_op transport_udp_op = 
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
 * Create UDP stream transport.
 */
PJ_DEF(pj_status_t) pjmedia_transport_udp_create( pjmedia_endpt *endpt,
						  const char *name,
						  int port,
						  unsigned options,
						  pjmedia_transport **p_tp)
{
    return pjmedia_transport_udp_create2(endpt, name, NULL, port, options, 
					p_tp);
}

/**
 * Create UDP stream transport.
 */
PJ_DEF(pj_status_t) pjmedia_transport_udp_create2(pjmedia_endpt *endpt,
						  const char *name,
						  const pj_str_t *addr,
						  int port,
						  unsigned options,
						  pjmedia_transport **p_tp)
{
    return pjmedia_transport_udp_create3(endpt, pj_AF_INET(), name,
					 addr, port, options, p_tp);
}

/**
 * Create UDP stream transport.
 */
PJ_DEF(pj_status_t) pjmedia_transport_udp_create3(pjmedia_endpt *endpt,
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

    
    /* Create UDP transport by attaching socket info */
    return pjmedia_transport_udp_attach( endpt, name, &si, options, p_tp);


on_error:
    if (si.rtp_sock != PJ_INVALID_SOCKET)
	pj_sock_close(si.rtp_sock);
    if (si.rtcp_sock != PJ_INVALID_SOCKET)
	pj_sock_close(si.rtcp_sock);
    return status;
}


/**
 * Create UDP stream transport from existing socket info.
 */
PJ_DEF(pj_status_t) pjmedia_transport_udp_attach( pjmedia_endpt *endpt,
						  const char *name,
						  const pjmedia_sock_info *si,
						  unsigned options,
						  pjmedia_transport **p_tp)
{
    struct transport_udp *tp;
    pj_pool_t *pool;
    pj_ioqueue_t *ioqueue;
    pj_ioqueue_callback rtp_cb, rtcp_cb;
    pj_ssize_t size;
    unsigned i;
	pj_status_t status;
	long sobuf_size;


    /* Sanity check */
    PJ_ASSERT_RETURN(endpt && si && p_tp, PJ_EINVAL);

    /* Get ioqueue instance */
    ioqueue = pjmedia_endpt_get_ioqueue(endpt);

    if (name==NULL)
	name = "udp%p";

    /* Create transport structure */
    pool = pjmedia_endpt_create_pool(endpt, name, 512, 512);
    if (!pool)
	return PJ_ENOMEM;

    tp = PJ_POOL_ZALLOC_T(pool, struct transport_udp);
    tp->pool = pool;
    tp->options = options;
    pj_memcpy(tp->base.name, pool->obj_name, PJ_MAX_OBJ_NAME);
    tp->base.op = &transport_udp_op;
    tp->base.type = PJMEDIA_TRANSPORT_TYPE_UDP;
	tp->base.inst_id = pjmedia_endpt_get_inst_id(endpt);

    /* Copy socket infos */
    tp->rtp_sock = si->rtp_sock;
    tp->rtp_addr_name = si->rtp_addr_name;
    tp->rtcp_sock = si->rtcp_sock;
	tp->rtcp_addr_name = si->rtcp_addr_name;

#if 1 // natnl set stun socket recv and send buffer size.
	sobuf_size = PJ_STUN_SOCK_PKT_LEN;
	status = pj_sock_setsockopt(tp->rtp_sock, pj_SOL_SOCKET(), pj_SO_RCVBUF(),
		&sobuf_size, sizeof(sobuf_size));
	if (status != PJ_SUCCESS)
		goto on_error;

	status = pj_sock_setsockopt(tp->rtcp_sock, pj_SOL_SOCKET(), pj_SO_SNDBUF(),
		&sobuf_size, sizeof(sobuf_size));
	if (status != PJ_SUCCESS)
		goto on_error;
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

    /* Same with RTCP */
    if (!pj_sockaddr_has_addr(&tp->rtcp_addr_name)) {
	pj_memcpy(pj_sockaddr_get_addr(&tp->rtcp_addr_name),
		  pj_sockaddr_get_addr(&tp->rtp_addr_name),
		  pj_sockaddr_get_addr_len(&tp->rtp_addr_name));
    }

    /* Setup RTP socket with the ioqueue */
    pj_bzero(&rtp_cb, sizeof(rtp_cb));
    rtp_cb.on_read_complete = &on_rx_rtp;

    status = pj_ioqueue_register_sock(pool, ioqueue, tp->rtp_sock, tp,
				      &rtp_cb, &tp->rtp_key);
    if (status != PJ_SUCCESS)
	goto on_error;
    
    /* Disallow concurrency so that detach() and destroy() are
     * synchronized with the callback.
     */
    status = pj_ioqueue_set_concurrency(tp->rtp_key, PJ_FALSE);
    if (status != PJ_SUCCESS)
	goto on_error;

    pj_ioqueue_op_key_init(&tp->rtp_read_op, sizeof(tp->rtp_read_op));
    for (i=0; i<PJ_ARRAY_SIZE(tp->rtp_pending_write); ++i)
	pj_ioqueue_op_key_init(&tp->rtp_pending_write[i].op_key, 
			       sizeof(tp->rtp_pending_write[i].op_key));

    /* Kick of pending RTP read from the ioqueue */
    tp->rtp_addrlen = sizeof(tp->rtp_src_addr);
    size = sizeof(tp->rtp_pkt);
    status = pj_ioqueue_recvfrom(tp->rtp_key, &tp->rtp_read_op,
			         tp->rtp_pkt, &size, PJ_IOQUEUE_ALWAYS_ASYNC,
				 &tp->rtp_src_addr, &tp->rtp_addrlen);
    if (status != PJ_EPENDING)
	goto on_error;


    /* Setup RTCP socket with ioqueue */
    pj_bzero(&rtcp_cb, sizeof(rtcp_cb));
    rtcp_cb.on_read_complete = &on_rx_rtcp;

    status = pj_ioqueue_register_sock(pool, ioqueue, tp->rtcp_sock, tp,
				      &rtcp_cb, &tp->rtcp_key);
    if (status != PJ_SUCCESS)
	goto on_error;

    status = pj_ioqueue_set_concurrency(tp->rtcp_key, PJ_FALSE);
    if (status != PJ_SUCCESS)
	goto on_error;

    pj_ioqueue_op_key_init(&tp->rtcp_read_op, sizeof(tp->rtcp_read_op));
    pj_ioqueue_op_key_init(&tp->rtcp_write_op, sizeof(tp->rtcp_write_op));


    /* Kick of pending RTCP read from the ioqueue */
    size = sizeof(tp->rtcp_pkt);
    tp->rtcp_addr_len = sizeof(tp->rtcp_src_addr);
    status = pj_ioqueue_recvfrom( tp->rtcp_key, &tp->rtcp_read_op,
				  tp->rtcp_pkt, &size, PJ_IOQUEUE_ALWAYS_ASYNC,
				  &tp->rtcp_src_addr, &tp->rtcp_addr_len);
    if (status != PJ_EPENDING)
	goto on_error;


    /* Done */
    *p_tp = &tp->base;
    return PJ_SUCCESS;


on_error:
    transport_destroy(&tp->base);
    return status;
}


/**
 * Close UDP transport.
 */
static pj_status_t transport_destroy(pjmedia_transport *tp)
{
    struct transport_udp *udp = (struct transport_udp*) tp;

    /* Sanity check */
    PJ_ASSERT_RETURN(tp, PJ_EINVAL);

    /* Must not close while application is using this */
    //PJ_ASSERT_RETURN(!udp->attached, PJ_EINVALIDOP);
    

    if (udp->rtp_key) {
	/* This will block the execution if callback is still
	 * being called.
	 */
	pj_ioqueue_unregister(udp->rtp_key);
	udp->rtp_key = NULL;
	udp->rtp_sock = PJ_INVALID_SOCKET;
    } else if (udp->rtp_sock != PJ_INVALID_SOCKET) {
	pj_sock_close(udp->rtp_sock);
	udp->rtp_sock = PJ_INVALID_SOCKET;
    }

    if (udp->rtcp_key) {
	pj_ioqueue_unregister(udp->rtcp_key);
	udp->rtcp_key = NULL;
	udp->rtcp_sock = PJ_INVALID_SOCKET;
    } else if (udp->rtcp_sock != PJ_INVALID_SOCKET) {
	pj_sock_close(udp->rtcp_sock);
	udp->rtcp_sock = PJ_INVALID_SOCKET;
    }

    pj_pool_release(udp->pool);

    return PJ_SUCCESS;
}


/* Notification from ioqueue about incoming RTP packet */
static void on_rx_rtp( pj_ioqueue_key_t *key, 
                       pj_ioqueue_op_key_t *op_key, 
                       pj_ssize_t bytes_read)
{
    struct transport_udp *udp;
    pj_status_t status;

    PJ_UNUSED_ARG(op_key);

    udp = (struct transport_udp*) pj_ioqueue_get_user_data(key);

    do {
	void (*cb)(void*,void*,pj_ssize_t);
	void *user_data;
	pj_bool_t discard = PJ_FALSE;

	cb = udp->rtp_cb;
	user_data = udp->user_data;

	/* Simulate packet lost on RX direction */
	if (udp->rx_drop_pct) {
	    if ((pj_rand() % 100) <= (int)udp->rx_drop_pct) {
		PJ_LOG(5,(udp->base.name, 
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
	    (udp->options & PJMEDIA_UDP_NO_SRC_ADDR_CHECKING)==0) 
	{
	    if (pj_sockaddr_cmp(&udp->rem_rtp_addr, &udp->rtp_src_addr) == 0) {
		/* We're still receiving from rem_rtp_addr. Don't switch. */
		udp->rtp_src_cnt = 0;
	    } else {
		udp->rtp_src_cnt++;

		if (udp->rtp_src_cnt < PJMEDIA_RTP_NAT_PROBATION_CNT) {
		    discard = PJ_TRUE;
		} else {
		
		    char addr_text[80];

		    /* Set remote RTP address to source address */
		    pj_memcpy(&udp->rem_rtp_addr, &udp->rtp_src_addr,
			      sizeof(pj_sockaddr));

		    /* Reset counter */
		    udp->rtp_src_cnt = 0;

		    PJ_LOG(4,(udp->base.name,
			      "Remote RTP address switched to %s",
			      pj_sockaddr_print(&udp->rtp_src_addr, addr_text,
						sizeof(addr_text), 3)));

		    /* Also update remote RTCP address if actual RTCP source
		     * address is not heard yet.
		     */
		    if (!pj_sockaddr_has_addr(&udp->rtcp_src_addr)) {
			pj_uint16_t port;

			pj_memcpy(&udp->rem_rtcp_addr, &udp->rem_rtp_addr, 
				  sizeof(pj_sockaddr));
			pj_sockaddr_copy_addr(&udp->rem_rtcp_addr,
					      &udp->rem_rtp_addr);
			port = (pj_uint16_t)
			       (pj_sockaddr_get_port(&udp->rem_rtp_addr)+1);
			pj_sockaddr_set_port(&udp->rem_rtcp_addr, port);

			pj_memcpy(&udp->rtcp_src_addr, &udp->rem_rtcp_addr, 
				  sizeof(pj_sockaddr));

			PJ_LOG(4,(udp->base.name,
				  "Remote RTCP address switched to predicted"
				  " address %s",
				  pj_sockaddr_print(&udp->rtcp_src_addr, 
						    addr_text,
						    sizeof(addr_text), 3)));

		    }
		}
	    }
	}

	if (!discard && udp->attached && cb)
	    (*cb)(user_data, udp->rtp_pkt, bytes_read);

	bytes_read = sizeof(udp->rtp_pkt);
	udp->rtp_addrlen = sizeof(udp->rtp_src_addr);
	status = pj_ioqueue_recvfrom(udp->rtp_key, &udp->rtp_read_op,
				     udp->rtp_pkt, &bytes_read, 0,
				     &udp->rtp_src_addr, 
				     &udp->rtp_addrlen);

	if (status != PJ_EPENDING && status != PJ_SUCCESS)
	    bytes_read = -status;

    } while (status != PJ_EPENDING && status != PJ_ECANCELLED);
}


/* Notification from ioqueue about incoming RTCP packet */
static void on_rx_rtcp(pj_ioqueue_key_t *key, 
                       pj_ioqueue_op_key_t *op_key, 
                       pj_ssize_t bytes_read)
{
    struct transport_udp *udp;
    pj_status_t status;

    PJ_UNUSED_ARG(op_key);

    udp = (struct transport_udp*) pj_ioqueue_get_user_data(key);

    do {
	void (*cb)(void*,void*,pj_ssize_t);
	void *user_data;

	cb = udp->rtcp_cb;
	user_data = udp->user_data;

	if (udp->attached && cb)
	    (*cb)(user_data, udp->rtcp_pkt, bytes_read);

	/* Check if RTCP source address is the same as the configured
	 * remote address, and switch the address when they are
	 * different.
	 */
	if (bytes_read>0 &&
	    (udp->options & PJMEDIA_UDP_NO_SRC_ADDR_CHECKING)==0)
	{
	    if (pj_sockaddr_cmp(&udp->rem_rtcp_addr, &udp->rtcp_src_addr) == 0) {
		/* Still receiving from rem_rtcp_addr, don't switch */
		udp->rtcp_src_cnt = 0;
	    } else {
		++udp->rtcp_src_cnt;

		if (udp->rtcp_src_cnt >= PJMEDIA_RTCP_NAT_PROBATION_CNT	) {
		    char addr_text[80];

		    udp->rtcp_src_cnt = 0;
		    pj_memcpy(&udp->rem_rtcp_addr, &udp->rtcp_src_addr,
			      sizeof(pj_sockaddr));

		    PJ_LOG(4,(udp->base.name,
			      "Remote RTCP address switched to %s",
			      pj_sockaddr_print(&udp->rtcp_src_addr, addr_text,
						sizeof(addr_text), 3)));
		}
	    }
	}

	bytes_read = sizeof(udp->rtcp_pkt);
	udp->rtcp_addr_len = sizeof(udp->rtcp_src_addr);
	status = pj_ioqueue_recvfrom(udp->rtcp_key, &udp->rtcp_read_op,
				     udp->rtcp_pkt, &bytes_read, 0,
				     &udp->rtcp_src_addr, 
				     &udp->rtcp_addr_len);
	if (status != PJ_EPENDING && status != PJ_SUCCESS)
	    bytes_read = -status;

    } while (status != PJ_EPENDING && status != PJ_ECANCELLED);
}


/* Called to get the transport info */
static pj_status_t transport_get_info(pjmedia_transport *tp,
				      pjmedia_transport_info *info)
{
    struct transport_udp *udp = (struct transport_udp*)tp;
    PJ_ASSERT_RETURN(tp && info, PJ_EINVAL);

    info->sock_info.rtp_sock = udp->rtp_sock;
    info->sock_info.rtp_addr_name = udp->rtp_addr_name;
    info->sock_info.rtcp_sock = udp->rtcp_sock;
    info->sock_info.rtcp_addr_name = udp->rtcp_addr_name;

    /* Get remote address originating RTP & RTCP. */
    info->src_rtp_name  = udp->rtp_src_addr;
    info->src_rtcp_name = udp->rtcp_src_addr;

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
    struct transport_udp *udp = (struct transport_udp*) tp;
    const pj_sockaddr *rtcp_addr;

    /* Validate arguments */
    PJ_ASSERT_RETURN(tp && rem_addr && addr_len, PJ_EINVAL);

    /* Must not be "attached" to existing application */
    PJ_ASSERT_RETURN(!udp->attached, PJ_EINVALIDOP);

    /* Lock the ioqueue keys to make sure that callbacks are
     * not executed. See ticket #844 for details.
     */
    pj_ioqueue_lock_key(udp->rtp_key);
    pj_ioqueue_lock_key(udp->rtcp_key);

    /* "Attach" the application: */

    /* Copy remote RTP address */
    pj_memcpy(&udp->rem_rtp_addr, rem_addr, addr_len);

    /* Copy remote RTP address, if one is specified. */
    rtcp_addr = (const pj_sockaddr*) rem_rtcp;
    if (rtcp_addr && pj_sockaddr_has_addr(rtcp_addr)) {
	pj_memcpy(&udp->rem_rtcp_addr, rem_rtcp, addr_len);

    } else {
	unsigned rtcp_port;

	/* Otherwise guess the RTCP address from the RTP address */
	pj_memcpy(&udp->rem_rtcp_addr, rem_addr, addr_len);
	rtcp_port = pj_sockaddr_get_port(&udp->rem_rtp_addr) + 1;
	pj_sockaddr_set_port(&udp->rem_rtcp_addr, (pj_uint16_t)rtcp_port);
    }

    /* Save the callbacks */
    udp->rtp_cb = rtp_cb;
    udp->rtcp_cb = rtcp_cb;
    udp->user_data = user_data;

    /* Save address length */
    udp->addr_len = addr_len;

    /* Last, mark transport as attached */
    udp->attached = PJ_TRUE;

    /* Reset source RTP & RTCP addresses and counter */
    pj_bzero(&udp->rtp_src_addr, sizeof(udp->rtp_src_addr));
    pj_bzero(&udp->rtcp_src_addr, sizeof(udp->rtcp_src_addr));
    udp->rtp_src_cnt = 0;
    udp->rtcp_src_cnt = 0;

    /* Unlock keys */
    pj_ioqueue_unlock_key(udp->rtcp_key);
    pj_ioqueue_unlock_key(udp->rtp_key);

    return PJ_SUCCESS;
}


/* Called by application when it no longer needs the transport */
static void transport_detach( pjmedia_transport *tp,
			      void *user_data)
{
    struct transport_udp *udp = (struct transport_udp*) tp;

    pj_assert(tp);

    if (udp->attached) {
	/* Lock the ioqueue keys to make sure that callbacks are
	 * not executed. See ticket #460 for details.
	 */
	pj_ioqueue_lock_key(udp->rtp_key);
	pj_ioqueue_lock_key(udp->rtcp_key);

	/* User data is unreferenced on Release build */
	PJ_UNUSED_ARG(user_data);

	/* As additional checking, check if the same user data is specified */
	pj_assert(user_data == udp->user_data);

	/* First, mark transport as unattached */
	udp->attached = PJ_FALSE;

	/* Clear up application infos from transport */
	udp->rtp_cb = NULL;
	udp->rtcp_cb = NULL;
	udp->user_data = NULL;

	/* Unlock keys */
	pj_ioqueue_unlock_key(udp->rtcp_key);
	pj_ioqueue_unlock_key(udp->rtp_key);
    }
}


/* Called by application to send RTP packet */
static pj_status_t transport_send_rtp( pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size)
{
    struct transport_udp *udp = (struct transport_udp*)tp;
    pj_ssize_t sent;
    unsigned id;
    struct pending_write *pw;
    pj_status_t status;

    /* Must be attached */
    PJ_ASSERT_RETURN(udp->attached, PJ_EINVALIDOP);

    /* Check that the size is supported */
    PJ_ASSERT_RETURN(size <= RTP_LEN, PJ_ETOOBIG);

    /* Simulate packet lost on TX direction */
    if (udp->tx_drop_pct) {
	if ((pj_rand() % 100) <= (int)udp->tx_drop_pct) {
	    PJ_LOG(5,(udp->base.name, 
		      "TX RTP packet dropped because of pkt lost "
		      "simulation"));
	    return PJ_SUCCESS;
	}
    }


    id = udp->rtp_write_op_id;
    pw = &udp->rtp_pending_write[id];

    /* We need to copy packet to our buffer because when the
     * operation is pending, caller might write something else
     * to the original buffer.
     */
    pj_memcpy(pw->buffer, pkt, size);

    sent = size;
    status = pj_ioqueue_sendto( udp->rtp_key, 
				&udp->rtp_pending_write[id].op_key,
				pw->buffer, &sent, 0,
				&udp->rem_rtp_addr, 
				udp->addr_len);

    udp->rtp_write_op_id = (udp->rtp_write_op_id + 1) %
			   PJ_ARRAY_SIZE(udp->rtp_pending_write);

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
    struct transport_udp *udp = (struct transport_udp*)tp;
    pj_ssize_t sent;
    pj_status_t status;

    PJ_ASSERT_RETURN(udp->attached, PJ_EINVALIDOP);

    if (addr == NULL) {
	addr = &udp->rem_rtcp_addr;
	addr_len = udp->addr_len;
    }

    sent = size;
    status = pj_ioqueue_sendto( udp->rtcp_key, &udp->rtcp_write_op,
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
    struct transport_udp *udp = (struct transport_udp*)tp;
	tp->tunnel_type = NATNL_TUNNEL_TYPE_UDP;
	//tp->call_id = media_index;

    PJ_ASSERT_RETURN(tp && pool, PJ_EINVAL);
    udp->media_options = options;

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
    struct transport_udp *udp = (struct transport_udp*)tp;

    /* Validate media transport */
    /* By now, this transport only support RTP/AVP transport */
    if ((udp->media_options & PJMEDIA_TPMED_NO_TRANSPORT_CHECKING) == 0) {
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
    struct transport_udp *udp = (struct transport_udp*)tp;

    PJ_ASSERT_RETURN(tp && pct_lost <= 100, PJ_EINVAL);

    if (dir & PJMEDIA_DIR_ENCODING)
	udp->tx_drop_pct = pct_lost;
    
    if (dir & PJMEDIA_DIR_DECODING)
	udp->rx_drop_pct = pct_lost;

    return PJ_SUCCESS;
}

