/* $Id: transport_adapter_sample.c 3804 2011-10-09 10:58:38Z bennylp $ */
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
#include <pjmedia/transport_adapter_sample.h>
#include <pjmedia/endpoint.h>
#include <pj/assert.h>
#include <pj/pool.h>


/* Transport functions prototypes */
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
				       pj_pool_t *sdp_pool,
				       unsigned options,
				       const pjmedia_sdp_session *rem_sdp,
				       unsigned media_index);
static pj_status_t transport_encode_sdp(pjmedia_transport *tp,
				       pj_pool_t *sdp_pool,
				       pjmedia_sdp_session *local_sdp,
				       const pjmedia_sdp_session *rem_sdp,
				       unsigned media_index);
static pj_status_t transport_media_start (pjmedia_transport *tp,
				       pj_pool_t *pool,
				       const pjmedia_sdp_session *local_sdp,
				       const pjmedia_sdp_session *rem_sdp,
				       unsigned media_index);
static pj_status_t transport_media_stop(pjmedia_transport *tp);
static pj_status_t transport_simulate_lost(pjmedia_transport *tp,
				       pjmedia_dir dir,
				       unsigned pct_lost);
static pj_status_t transport_destroy  (pjmedia_transport *tp);


/* The transport operations */
static struct pjmedia_transport_op tp_adapter_op = 
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


/* The transport adapter instance */
struct tp_adapter
{
    pjmedia_transport	 base;
    pj_bool_t		 del_base;

    pj_pool_t		*pool;

    /* Stream information. */
    void		*stream_user_data;
    void	       (*stream_rtp_cb)(void *user_data,
					void *pkt,
					pj_ssize_t);
    void	       (*stream_rtcp_cb)(void *user_data,
					 void *pkt,
					 pj_ssize_t);


    /* Add your own member here.. */
    pjmedia_transport	*slave_tp;
};


/*
 * Create the adapter.
 */
PJ_DEF(pj_status_t) pjmedia_tp_adapter_create( pjmedia_endpt *endpt,
					       const char *name,
					       pjmedia_transport *transport,
					       pj_bool_t del_base,
					       pjmedia_transport **p_tp)
{
    pj_pool_t *pool;
    struct tp_adapter *adapter;

    if (name == NULL)
	name = "tpad%p";

    /* Create the pool and initialize the adapter structure */
    pool = pjmedia_endpt_create_pool(endpt, name, 512, 512);
    adapter = PJ_POOL_ZALLOC_T(pool, struct tp_adapter);
    adapter->pool = pool;
    pj_ansi_strncpy(adapter->base.name, pool->obj_name, 
		    sizeof(adapter->base.name));
    adapter->base.type = (pjmedia_transport_type)
			 (PJMEDIA_TRANSPORT_TYPE_USER + 1);
    adapter->base.op = &tp_adapter_op;

    /* Save the transport as the slave transport */
    adapter->slave_tp = transport;
    adapter->del_base = del_base;

    /* Done */
    *p_tp = &adapter->base;
    return PJ_SUCCESS;
}


/*
 * get_info() is called to get the transport addresses to be put
 * in SDP c= line and a=rtcp line.
 */
static pj_status_t transport_get_info(pjmedia_transport *tp,
				      pjmedia_transport_info *info)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;

    /* Since we don't have our own connection here, we just pass
     * this function to the slave transport.
     */
    return pjmedia_transport_get_info(adapter->slave_tp, info);
}


/* This is our RTP callback, that is called by the slave transport when it
 * receives RTP packet.
 */
static void transport_rtp_cb(void *user_data, void *pkt, pj_ssize_t size)
{
    struct tp_adapter *adapter = (struct tp_adapter*)user_data;

    pj_assert(adapter->stream_rtp_cb != NULL);

    /* Call stream's callback */
    adapter->stream_rtp_cb(adapter->stream_user_data, pkt, size);
}

/* This is our RTCP callback, that is called by the slave transport when it
 * receives RTCP packet.
 */
static void transport_rtcp_cb(void *user_data, void *pkt, pj_ssize_t size)
{
    struct tp_adapter *adapter = (struct tp_adapter*)user_data;

    pj_assert(adapter->stream_rtcp_cb != NULL);

    /* Call stream's callback */
    adapter->stream_rtcp_cb(adapter->stream_user_data, pkt, size);
}


/*
 * attach() is called by stream to register callbacks that we should
 * call on receipt of RTP and RTCP packets.
 */
static pj_status_t transport_attach(pjmedia_transport *tp,
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
    struct tp_adapter *adapter = (struct tp_adapter*)tp;
    pj_status_t status;

    /* In this example, we will save the stream information and callbacks
     * to our structure, and we will register different RTP/RTCP callbacks
     * instead.
     */
    pj_assert(adapter->stream_user_data == NULL);
    adapter->stream_user_data = user_data;
    adapter->stream_rtp_cb = rtp_cb;
    adapter->stream_rtcp_cb = rtcp_cb;

    status = pjmedia_transport_attach(adapter->slave_tp, adapter, rem_addr,
				      rem_rtcp, addr_len, &transport_rtp_cb,
				      &transport_rtcp_cb);
    if (status != PJ_SUCCESS) {
	adapter->stream_user_data = NULL;
	adapter->stream_rtp_cb = NULL;
	adapter->stream_rtcp_cb = NULL;
	return status;
    }

    return PJ_SUCCESS;
}

/* 
 * detach() is called when the media is terminated, and the stream is 
 * to be disconnected from us.
 */
static void transport_detach(pjmedia_transport *tp, void *strm)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;
    
    PJ_UNUSED_ARG(strm);

    if (adapter->stream_user_data != NULL) {
	pjmedia_transport_detach(adapter->slave_tp, adapter);
	adapter->stream_user_data = NULL;
	adapter->stream_rtp_cb = NULL;
	adapter->stream_rtcp_cb = NULL;
    }
}


/*
 * send_rtp() is called to send RTP packet. The "pkt" and "size" argument 
 * contain both the RTP header and the payload.
 */
static pj_status_t transport_send_rtp( pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;

    /* You may do some processing to the RTP packet here if you want. */

    /* Send the packet using the slave transport */
    return pjmedia_transport_send_rtp(adapter->slave_tp, pkt, size);
}


/*
 * send_rtcp() is called to send RTCP packet. The "pkt" and "size" argument
 * contain the RTCP packet.
 */
static pj_status_t transport_send_rtcp(pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;

    /* You may do some processing to the RTCP packet here if you want. */

    /* Send the packet using the slave transport */
    return pjmedia_transport_send_rtcp(adapter->slave_tp, pkt, size);
}


/*
 * This is another variant of send_rtcp(), with the alternate destination
 * address in the argument.
 */
static pj_status_t transport_send_rtcp2(pjmedia_transport *tp,
				        const pj_sockaddr_t *addr,
				        unsigned addr_len,
				        const void *pkt,
				        pj_size_t size)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;
    return pjmedia_transport_send_rtcp2(adapter->slave_tp, addr, addr_len, 
					pkt, size);
}

/*
 * The media_create() is called when the transport is about to be used for
 * a new call.
 */
static pj_status_t transport_media_create(pjmedia_transport *tp,
				          pj_pool_t *sdp_pool,
				          unsigned options,
				          const pjmedia_sdp_session *rem_sdp,
				          unsigned media_index)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;

    /* if "rem_sdp" is not NULL, it means we are UAS. You may do some
     * inspections on the incoming SDP to verify that the SDP is acceptable
     * for us. If the SDP is not acceptable, we can reject the SDP by 
     * returning non-PJ_SUCCESS.
     */
    if (rem_sdp) {
	/* Do your stuff.. */
    }

    /* Once we're done with our initialization, pass the call to the
     * slave transports to let it do it's own initialization too.
     */
    return pjmedia_transport_media_create(adapter->slave_tp, sdp_pool, options,
					   rem_sdp, media_index);
}

/*
 * The encode_sdp() is called when we're about to send SDP to remote party,
 * either as SDP offer or as SDP answer.
 */
static pj_status_t transport_encode_sdp(pjmedia_transport *tp,
				        pj_pool_t *sdp_pool,
				        pjmedia_sdp_session *local_sdp,
				        const pjmedia_sdp_session *rem_sdp,
				        unsigned media_index)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;

    /* If "rem_sdp" is not NULL, it means we're encoding SDP answer. You may
     * do some more checking on the SDP's once again to make sure that
     * everything is okay before we send SDP.
     */
    if (rem_sdp) {
	/* Do checking stuffs here.. */
    }

    /* You may do anything to the local_sdp, e.g. adding new attributes, or
     * even modifying the SDP if you want.
     */
    if (1) {
	/* Say we add a proprietary attribute here.. */
	pjmedia_sdp_attr *my_attr;

	my_attr = PJ_POOL_ALLOC_T(sdp_pool, pjmedia_sdp_attr);
	pj_strdup2(sdp_pool, &my_attr->name, "X-adapter");
	pj_strdup2(sdp_pool, &my_attr->value, "some value");

	pjmedia_sdp_attr_add(&local_sdp->media[media_index]->attr_count,
			     local_sdp->media[media_index]->attr,
			     my_attr);
    }

    /* And then pass the call to slave transport to let it encode its 
     * information in the SDP. You may choose to call encode_sdp() to slave
     * first before adding your custom attributes if you want.
     */
    return pjmedia_transport_encode_sdp(adapter->slave_tp, sdp_pool, local_sdp,
					rem_sdp, media_index);
}

/*
 * The media_start() is called once both local and remote SDP have been
 * negotiated successfully, and the media is ready to start. Here we can start
 * committing our processing.
 */
static pj_status_t transport_media_start(pjmedia_transport *tp,
				         pj_pool_t *pool,
				         const pjmedia_sdp_session *local_sdp,
				         const pjmedia_sdp_session *rem_sdp,
				         unsigned media_index)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;

    /* Do something.. */

    /* And pass the call to the slave transport */
    return pjmedia_transport_media_start(adapter->slave_tp, pool, local_sdp,
					 rem_sdp, media_index);
}

/*
 * The media_stop() is called when media has been stopped.
 */
static pj_status_t transport_media_stop(pjmedia_transport *tp)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;

    /* Do something.. */

    /* And pass the call to the slave transport */
    return pjmedia_transport_media_stop(adapter->slave_tp);
}

/*
 * simulate_lost() is called to simulate packet lost
 */
static pj_status_t transport_simulate_lost(pjmedia_transport *tp,
				           pjmedia_dir dir,
				           unsigned pct_lost)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;
    return pjmedia_transport_simulate_lost(adapter->slave_tp, dir, pct_lost);
}

/*
 * destroy() is called when the transport is no longer needed.
 */
static pj_status_t transport_destroy  (pjmedia_transport *tp)
{
    struct tp_adapter *adapter = (struct tp_adapter*)tp;

    /* Close the slave transport */
    if (adapter->del_base) {
	pjmedia_transport_close(adapter->slave_tp);
    }

    /* Self destruct.. */
    pj_pool_release(adapter->pool);

    return PJ_SUCCESS;
}





