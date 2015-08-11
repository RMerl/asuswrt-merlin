/* $Id: transport_loop.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia/transport_loop.h>
#include <pj/array.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/ioqueue.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pj/string.h>


struct user
{
    pj_bool_t		rx_disabled;	/**< Doesn't want to receive pkt?   */
    void	       *user_data;	/**< Only valid when attached	    */
    void  (*rtp_cb)(	void*,		/**< To report incoming RTP.	    */
			void*,
			pj_ssize_t);
    void  (*rtcp_cb)(	void*,		/**< To report incoming RTCP.	    */
			void*,
			pj_ssize_t);
};

struct transport_loop
{
    pjmedia_transport	base;		/**< Base transport.		    */

    pj_pool_t	       *pool;		/**< Memory pool		    */
    unsigned		user_cnt;	/**< Number of attachments	    */
    struct user		users[4];	/**< Array of users.		    */

    unsigned		tx_drop_pct;	/**< Percent of tx pkts to drop.    */
    unsigned		rx_drop_pct;	/**< Percent of rx pkts to drop.    */

};



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
 * Create loopback transport.
 */
PJ_DEF(pj_status_t) pjmedia_transport_loop_create(pjmedia_endpt *endpt,
						  pjmedia_transport **p_tp)
{
    struct transport_loop *tp;
    pj_pool_t *pool;

    /* Sanity check */
    PJ_ASSERT_RETURN(endpt && p_tp, PJ_EINVAL);

    /* Create transport structure */
    pool = pjmedia_endpt_create_pool(endpt, "tploop", 512, 512);
    if (!pool)
	return PJ_ENOMEM;

    tp = PJ_POOL_ZALLOC_T(pool, struct transport_loop);
    tp->pool = pool;
    pj_ansi_strncpy(tp->base.name, tp->pool->obj_name, PJ_MAX_OBJ_NAME-1);
    tp->base.op = &transport_udp_op;
    tp->base.type = PJMEDIA_TRANSPORT_TYPE_UDP;

    /* Done */
    *p_tp = &tp->base;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_transport_loop_disable_rx( pjmedia_transport *tp,
						       void *user,
						       pj_bool_t disabled)
{
    struct transport_loop *loop = (struct transport_loop*) tp;
    unsigned i;

    for (i=0; i<loop->user_cnt; ++i) {
	if (loop->users[i].user_data == user) {
	    loop->users[i].rx_disabled = disabled;
	    return PJ_SUCCESS;
	}
    }
	pj_assert(!"Invalid stream user");
	PJ_LOG(4, ("transport_loop.c", "pjmedia_transport_loop_disable_rx() stream user not found."));
    return PJ_ENOTFOUND;
}

/**
 * Close loopback transport.
 */
static pj_status_t transport_destroy(pjmedia_transport *tp)
{
    struct transport_loop *loop = (struct transport_loop*) tp;

    /* Sanity check */
    PJ_ASSERT_RETURN(tp, PJ_EINVAL);

    pj_pool_release(loop->pool);

    return PJ_SUCCESS;
}


/* Called to get the transport info */
static pj_status_t transport_get_info(pjmedia_transport *tp,
				      pjmedia_transport_info *info)
{
    PJ_ASSERT_RETURN(tp && info, PJ_EINVAL);

    info->sock_info.rtp_sock = 1;
    pj_sockaddr_in_init(&info->sock_info.rtp_addr_name.ipv4, 0, 0);
    info->sock_info.rtcp_sock = 2;
    pj_sockaddr_in_init(&info->sock_info.rtcp_addr_name.ipv4, 0, 0);

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
    struct transport_loop *loop = (struct transport_loop*) tp;
    unsigned i;
    const pj_sockaddr *rtcp_addr;

    /* Validate arguments */
    PJ_ASSERT_RETURN(tp && rem_addr && addr_len, PJ_EINVAL);

    /* Must not be "attached" to same user */
    for (i=0; i<loop->user_cnt; ++i) {
	PJ_ASSERT_RETURN(loop->users[i].user_data != user_data,
			 PJ_EINVALIDOP);
    }
    PJ_ASSERT_RETURN(loop->user_cnt != PJ_ARRAY_SIZE(loop->users), 
		     PJ_ETOOMANY);

    PJ_UNUSED_ARG(rem_rtcp);
    PJ_UNUSED_ARG(rtcp_addr);

    /* "Attach" the application: */

    /* Save the new user */
    loop->users[loop->user_cnt].rtp_cb = rtp_cb;
    loop->users[loop->user_cnt].rtcp_cb = rtcp_cb;
    loop->users[loop->user_cnt].user_data = user_data;
    ++loop->user_cnt;

    return PJ_SUCCESS;
}


/* Called by application when it no longer needs the transport */
static void transport_detach( pjmedia_transport *tp,
			      void *user_data)
{
    struct transport_loop *loop = (struct transport_loop*) tp;
    unsigned i;

    pj_assert(tp);

    for (i=0; i<loop->user_cnt; ++i) {
	if (loop->users[i].user_data == user_data)
	    break;
    }

    /* Remove this user */
    if (i != loop->user_cnt) {
	pj_array_erase(loop->users, sizeof(loop->users[0]),
		       loop->user_cnt, i);
	--loop->user_cnt;
    }
}


/* Called by application to send RTP packet */
static pj_status_t transport_send_rtp( pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size)
{
    struct transport_loop *loop = (struct transport_loop*)tp;
    unsigned i;

    /* Simulate packet lost on TX direction */
    if (loop->tx_drop_pct) {
	if ((pj_rand() % 100) <= (int)loop->tx_drop_pct) {
	    PJ_LOG(5,(loop->base.name, 
		      "TX RTP packet dropped because of pkt lost "
		      "simulation"));
	    return PJ_SUCCESS;
	}
    }

    /* Simulate packet lost on RX direction */
    if (loop->rx_drop_pct) {
	if ((pj_rand() % 100) <= (int)loop->rx_drop_pct) {
	    PJ_LOG(5,(loop->base.name, 
		      "RX RTP packet dropped because of pkt lost "
		      "simulation"));
	    return PJ_SUCCESS;
	}
    }

    /* Distribute to users */
    for (i=0; i<loop->user_cnt; ++i) {
	if (!loop->users[i].rx_disabled && loop->users[i].rtp_cb)
	    (*loop->users[i].rtp_cb)(loop->users[i].user_data, (void*)pkt, 
				     size);
    }

    return PJ_SUCCESS;
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
    struct transport_loop *loop = (struct transport_loop*)tp;
    unsigned i;

    PJ_UNUSED_ARG(addr_len);
    PJ_UNUSED_ARG(addr);

    /* Distribute to users */
    for (i=0; i<loop->user_cnt; ++i) {
	if (!loop->users[i].rx_disabled && loop->users[i].rtcp_cb)
	    (*loop->users[i].rtcp_cb)(loop->users[i].user_data, (void*)pkt,
				      size);
    }

    return PJ_SUCCESS;
}


static pj_status_t transport_media_create(pjmedia_transport *tp,
				  pj_pool_t *pool,
				  unsigned options,
				  const pjmedia_sdp_session *sdp_remote,
				  unsigned media_index)
{
    PJ_UNUSED_ARG(tp);
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(options);
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
    PJ_UNUSED_ARG(tp);
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(sdp_local);
    PJ_UNUSED_ARG(rem_sdp);
    PJ_UNUSED_ARG(media_index);
    return PJ_SUCCESS;
}

static pj_status_t transport_media_start(pjmedia_transport *tp,
				  pj_pool_t *pool,
				  const pjmedia_sdp_session *sdp_local,
				  const pjmedia_sdp_session *sdp_remote,
				  unsigned media_index)
{
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
    struct transport_loop *loop = (struct transport_loop*)tp;

    PJ_ASSERT_RETURN(tp && pct_lost <= 100, PJ_EINVAL);

    if (dir & PJMEDIA_DIR_ENCODING)
	loop->tx_drop_pct = pct_lost;
    
    if (dir & PJMEDIA_DIR_DECODING)
	loop->rx_drop_pct = pct_lost;

    return PJ_SUCCESS;
}

