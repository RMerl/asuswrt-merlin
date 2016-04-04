/* $Id: transport_ice.c 4384 2013-02-27 10:08:07Z nanang $ */
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
#include <pjmedia/transport_ice.h>
#include <pjnath/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/rand.h>

#define THIS_FILE   "transport_ice.c"
#if 0
#   define TRACE__(expr)    PJ_LOG(5,expr)
#else
#   define TRACE__(expr)
#endif

enum oa_role
{
    ROLE_NONE,
    ROLE_OFFERER,
    ROLE_ANSWERER
};

struct sdp_state
{
    unsigned		match_comp_cnt;	/* Matching number of components    */
    pj_bool_t		ice_mismatch;	/* Address doesn't match candidates */
    pj_bool_t		ice_restart;	/* Offer to restart ICE		    */
    pj_ice_sess_role	local_role;	/* Our role			    */
};

struct transport_ice
{
    pjmedia_transport	 base;
    pj_pool_t		*pool;
    int			 af;
    unsigned		 options;	/**< Transport options.		    */

    unsigned		 comp_cnt;
    pj_ice_strans	*ice_st;

    pjmedia_ice_cb	 cb;
    unsigned		 media_option;

    pj_bool_t		 initial_sdp;
    enum oa_role	 oa_role;	/**< Last role in SDP offer/answer  */
    struct sdp_state	 rem_offer_state;/**< Describes the remote offer    */

    void		*stream;
    pj_sockaddr		 remote_rtp;
    pj_sockaddr		 remote_rtcp;
    unsigned		 addr_len;	/**< Length of addresses.	    */

    pj_bool_t		 use_ice;
    pj_sockaddr		 rtp_src_addr;	/**< Actual source RTP address.	    */
    pj_sockaddr		 rtcp_src_addr;	/**< Actual source RTCP address.    */
    unsigned		 rtp_src_cnt;	/**< How many pkt from this addr.   */
    unsigned		 rtcp_src_cnt;  /**< How many pkt from this addr.   */

    unsigned		 tx_drop_pct;	/**< Percent of tx pkts to drop.    */
    unsigned		 rx_drop_pct;	/**< Percent of rx pkts to drop.    */

    void	       (*rtp_cb)(void*,
			         void*,
				  pj_ssize_t);
    void	       (*rtcp_cb)(void*,
				  void*,
				  pj_ssize_t);
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
				       const pjmedia_sdp_session *rem_sdp,
				       unsigned media_index);
static pj_status_t transport_encode_sdp(pjmedia_transport *tp,
				        pj_pool_t *tmp_pool,
				        pjmedia_sdp_session *sdp_local,
				        const pjmedia_sdp_session *rem_sdp,
				        unsigned media_index);
static pj_status_t transport_media_start(pjmedia_transport *tp,
				       pj_pool_t *pool,
				       const pjmedia_sdp_session *sdp_local,
				       const pjmedia_sdp_session *rem_sdp,
				       unsigned media_index);
static pj_status_t transport_media_stop(pjmedia_transport *tp);
static pj_status_t transport_simulate_lost(pjmedia_transport *tp,
				       pjmedia_dir dir,
				       unsigned pct_lost);
static pj_status_t transport_destroy  (pjmedia_transport *tp);

/*
 * And these are ICE callbacks.
 */
static void ice_on_rx_data(pj_ice_strans *ice_st, 
			   unsigned comp_id, 
			   void *pkt, pj_size_t size,
			   const pj_sockaddr_t *src_addr,
			   unsigned src_addr_len);
static void ice_on_ice_complete(pj_ice_strans *ice_st, 
				pj_ice_strans_op op,
				pj_status_t status,
				pj_sockaddr *turn_mapped_addr);

static void ice_on_turn_allocated(pj_ice_strans *ice_st, 
								  pj_sockaddr_t *turn_srv);

static pj_status_t ice_on_stun_binding_complete(pj_ice_strans *ice_st,
										 pj_sockaddr *local_addr, 
										 int ip_chagned_type);

static pj_status_t ice_on_tcp_server_binding_complete(pj_ice_strans *ice_st,
											 pj_sockaddr *external_addr,
											 pj_sockaddr *local_addr);

static pjmedia_transport_op transport_ice_op = 
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

static const pj_str_t STR_RTP_AVP	= { "RTP/AVP", 7 };
static const pj_str_t STR_CANDIDATE	= { "candidate", 9};
static const pj_str_t STR_REM_CAND	= { "remote-candidates", 17 };
static const pj_str_t STR_ICE_LITE	= { "ice-lite", 8};
static const pj_str_t STR_ICE_MISMATCH	= { "ice-mismatch", 12};
static const pj_str_t STR_ICE_UFRAG	= { "ice-ufrag", 9 };
static const pj_str_t STR_ICE_PWD	= { "ice-pwd", 7 };
static const pj_str_t STR_IP4		= { "IP4", 3 };
static const pj_str_t STR_IP6		= { "IP6", 3 };
static const pj_str_t STR_RTCP		= { "rtcp", 4 };
static const pj_str_t STR_BANDW_RR	= { "RR", 2 };
static const pj_str_t STR_BANDW_RS	= { "RS", 2 };
static const pj_str_t STR_X_ADAPTER3	= { "X-adapter3", 10};
static const pj_str_t STR_FINGERPRINT	= { "fingerprint", 11 };

enum {
    COMP_RTP = 1,
    COMP_RTCP = 2
};

/*
 * Create ICE media transport.
 */
PJ_DEF(pj_status_t) pjmedia_ice_create(pjmedia_endpt *endpt,
				       const char *name,
				       unsigned comp_cnt,
				       const pj_ice_strans_cfg *cfg,
					   const pjmedia_ice_cb *cb,
					   int tp_idx,
					   pj_time_val inv_recv_time,
	    			       pjmedia_transport **p_tp)
{
    return pjmedia_ice_create2(endpt, name, comp_cnt, cfg, cb, 0, tp_idx, inv_recv_time, p_tp);
}

/*
 * Create ICE media transport.
 */
PJ_DEF(pj_status_t) pjmedia_ice_create2(pjmedia_endpt *endpt,
				        const char *name,
				        unsigned comp_cnt,
				        const pj_ice_strans_cfg *cfg,
				        const pjmedia_ice_cb *cb,
						unsigned options,
						int tp_idx,
						pj_time_val inv_recv_time,
	    			        pjmedia_transport **p_tp)
{
    pj_pool_t *pool;
    pj_ice_strans_cb ice_st_cb;
    struct transport_ice *tp_ice;
    pj_status_t status;

    PJ_ASSERT_RETURN(endpt && comp_cnt && cfg && p_tp, PJ_EINVAL);

    /* Create transport instance */
    pool = pjmedia_endpt_create_pool(endpt, name, 512, 512);
    tp_ice = PJ_POOL_ZALLOC_T(pool, struct transport_ice);
    tp_ice->pool = pool;
    tp_ice->af = cfg->af;
    tp_ice->options = options;
    tp_ice->comp_cnt = comp_cnt;
    pj_ansi_strncpy(tp_ice->base.name, pool->obj_name, PJ_MAX_OBJ_NAME);
	pj_memset(&tp_ice->base, 0, sizeof(pjmedia_transport)); // DEAN
    tp_ice->base.op = &transport_ice_op;
	tp_ice->base.type = PJMEDIA_TRANSPORT_TYPE_ICE;
	tp_ice->base.inst_id = pjmedia_endpt_get_inst_id(endpt);
	tp_ice->base.inv_recv_time = inv_recv_time;

    tp_ice->initial_sdp = PJ_TRUE;
    tp_ice->oa_role = ROLE_NONE;
    tp_ice->use_ice = PJ_FALSE;

    if (cb)
	pj_memcpy(&tp_ice->cb, cb, sizeof(pjmedia_ice_cb));

    /* Assign return value first because ICE might call callback
     * in create()
     */
    *p_tp = &tp_ice->base;

    /* Configure ICE callbacks */
    pj_bzero(&ice_st_cb, sizeof(ice_st_cb));
    ice_st_cb.on_ice_complete = &ice_on_ice_complete;
    ice_st_cb.on_rx_data = &ice_on_rx_data;
	// DEAN Added 2013-03-19
	ice_st_cb.on_turn_srv_allocated = &ice_on_turn_allocated;
	// 2013-05-20 DEAN
	ice_st_cb.on_stun_binding_complete = &ice_on_stun_binding_complete;
	// 2014-01-12 DEAN
	ice_st_cb.on_server_created = &ice_on_tcp_server_binding_complete;

    /* Create ICE */
    status = pj_ice_strans_create2(name, cfg, comp_cnt, tp_ice, 
				  &ice_st_cb, tp_idx, &tp_ice->ice_st);
    if (status != PJ_SUCCESS) {
	pj_pool_release(pool);
	*p_tp = NULL;
	return status;
    }

    /* Done */
    return PJ_SUCCESS;
}

/* Disable ICE when SDP from remote doesn't contain a=candidate line */
static void set_no_ice(struct transport_ice *tp_ice, const char *reason,
		       pj_status_t err)
{
    if (err != PJ_SUCCESS) {
	char errmsg[PJ_ERR_MSG_SIZE];
	pj_strerror(err, errmsg, sizeof(errmsg));
	PJ_LOG(4,(tp_ice->base.name, 
		  "Stopping ICE, reason=%s:%s", reason, errmsg));
    } else {
	PJ_LOG(4,(tp_ice->base.name, 
		  "Stopping ICE, reason=%s", reason));
    }

    if (tp_ice->ice_st) {
	pj_ice_strans_stop_ice(tp_ice->ice_st);
    }

    tp_ice->use_ice = PJ_FALSE;
}


/* Create SDP candidate attribute */
static int print_sdp_cand_attr(char *buffer, int max_len,
			       const pj_ice_sess_cand *cand, int use_sctp)
{
    char ipaddr[PJ_INET6_ADDRSTRLEN+2];
    int len, len2, len3, len4, len5;
	char *trans_type = "UDP";
	pj_parsed_time adding_ptime, ending_ptime;

	if (cand->transport_id == 3)    // TP_UPNP_TCP
		trans_type = "TCP";

    len = pj_ansi_snprintf( buffer, max_len,
			    "%.*s %u %s %u %s %u typ ",
			    (int)cand->foundation.slen,
			    cand->foundation.ptr,
			    (unsigned)cand->comp_id,
				trans_type,
			    cand->prio,
			    pj_sockaddr_print(&cand->addr, ipaddr, 
					      sizeof(ipaddr), 0),
			    (unsigned)pj_sockaddr_get_port(&cand->addr));
    if (len < 1 || len >= max_len)
	return -1;

	switch (cand->type) {
	case PJ_ICE_CAND_TYPE_HOST:
	case PJ_ICE_CAND_TYPE_HOST_TCP:
	len2 = pj_ansi_snprintf(buffer+len, max_len-len, "host");
	break;
    case PJ_ICE_CAND_TYPE_SRFLX:
    case PJ_ICE_CAND_TYPE_RELAYED:
	case PJ_ICE_CAND_TYPE_RELAYED_TCP:
	case PJ_ICE_CAND_TYPE_PRFLX:
	case PJ_ICE_CAND_TYPE_SRFLX_TCP:
	len2 = pj_ansi_snprintf(buffer+len, max_len-len,
			        "%s raddr %s rport %d",
				pj_ice_get_cand_type_name(cand->type),
				pj_sockaddr_print(&cand->rel_addr, ipaddr,
						  sizeof(ipaddr), 0),
				(int)pj_sockaddr_get_port(&cand->rel_addr));
	break;
    default:
	pj_assert(!"Invalid candidate type");
	len2 = -1;
	break;
    }
    if (len2 < 1 || len2 >= max_len)
	return -1;

	len3 = 0;
	// tcptype
	if (cand->tcp_type > PJ_ICE_CAND_TCP_TYPE_NONE) {
		char *tcptype = "";
		switch(cand->tcp_type) {
			case PJ_ICE_CAND_TCP_TYPE_ACTIVE:
				tcptype = "active";
				break;
			case PJ_ICE_CAND_TCP_TYPE_PASSIVE:
				tcptype = "passive";
				break;
			case PJ_ICE_CAND_TCP_TYPE_SO:
				tcptype = "so";
				break;
		}
	len3 = pj_ansi_snprintf(buffer+len+len2, max_len-len-len2,
			        " tcptype %s",
				tcptype);

    if (len3 < 1 || len3 >= max_len)
	return -1;
	}

	// DEAN. Don't create generation tag on UAC for compatibility.
	// WebRTC generation
	/*if (use_sctp)
		len4 = pj_ansi_snprintf(buffer+len+len2+len3, max_len-len-len2-len3, " generation 0");
	else*/
		len4 = 0;

	len5 = 0;
	// timestamp
	pj_time_decode(&cand->adding_time, &adding_ptime);
	pj_time_decode(&cand->ending_time, &ending_ptime);
	if (cand->transport_id == 4) 
		len5 = pj_ansi_snprintf(buffer+len+len2+len3+len4, max_len-len-len2-len3-len4,
			" TCP %s START:%04d-%02d-%02d_%02d:%02d:%02d.%03d END:%04d-%02d-%02d_%02d:%02d:%02d.%03d",
			cand->enabled ? "Enabled" : "Disabled",
			adding_ptime.year, adding_ptime.mon+1, adding_ptime.day, adding_ptime.hour, adding_ptime.min, adding_ptime.sec, adding_ptime.msec,
			ending_ptime.year, ending_ptime.mon+1, ending_ptime.day, ending_ptime.hour, ending_ptime.min, ending_ptime.sec, ending_ptime.msec);
	else
		len5 = pj_ansi_snprintf(buffer+len+len2+len3+len4, max_len-len-len2-len3-len4,
		" %s START:%04d-%02d-%02d_%02d:%02d:%02d.%03d END:%04d-%02d-%02d_%02d:%02d:%02d.%03d",
		cand->enabled ? "Enabled" : "Disabled",
		adding_ptime.year, adding_ptime.mon+1, adding_ptime.day, adding_ptime.hour, adding_ptime.min, adding_ptime.sec, adding_ptime.msec,
		ending_ptime.year, ending_ptime.mon+1, ending_ptime.day, ending_ptime.hour, ending_ptime.min, ending_ptime.sec, ending_ptime.msec);


    return len+len2+len3+len4+len5;
}


/* Get ice-ufrag and ice-pwd attribute */
static void get_ice_attr(const pjmedia_sdp_session *rem_sdp,
			 const pjmedia_sdp_media *rem_m,
			 const pjmedia_sdp_attr **p_ice_ufrag,
			 const pjmedia_sdp_attr **p_ice_pwd)
{
    pjmedia_sdp_attr *attr;

    /* Find ice-ufrag attribute in media descriptor */
    attr = pjmedia_sdp_attr_find(rem_m->attr_count, rem_m->attr,
				 &STR_ICE_UFRAG, NULL);
    if (attr == NULL) {
	/* Find ice-ufrag attribute in session descriptor */
	attr = pjmedia_sdp_attr_find(rem_sdp->attr_count, rem_sdp->attr,
				     &STR_ICE_UFRAG, NULL);
    }
    *p_ice_ufrag = attr;

    /* Find ice-pwd attribute in media descriptor */
    attr = pjmedia_sdp_attr_find(rem_m->attr_count, rem_m->attr,
				 &STR_ICE_PWD, NULL);
    if (attr == NULL) {
	/* Find ice-pwd attribute in session descriptor */
	attr = pjmedia_sdp_attr_find(rem_sdp->attr_count, rem_sdp->attr,
				     &STR_ICE_PWD, NULL);
    }
    *p_ice_pwd = attr;
}


/* Parse "HOST:PORT" format */
static pj_status_t parse_host_port(const pj_str_t *host_port,
								   pj_str_t *host, pj_uint16_t *port)
{
	pj_str_t str_port;

	str_port.ptr = pj_strchr(host_port, ':');
	if (str_port.ptr != NULL) {
		int iport;

		host->ptr = host_port->ptr;
		host->slen = (str_port.ptr - host->ptr);
		str_port.ptr++;
		str_port.slen = host_port->slen - host->slen - 1;
		iport = (int)pj_strtoul(&str_port);
		if (iport < 1 || iport > 65535)
			return PJ_EINVAL;
		*port = (pj_uint16_t)iport;
	} else {
		*host = *host_port;
		*port = 0;
	}

	return PJ_SUCCESS;
}

/* Parse "HOST:PORT" format */
static pj_status_t parse_user_realm(const pj_str_t *user_realm,
								   pj_str_t *user, pj_str_t *realm)
{
	pj_str_t str_realm;

	str_realm.ptr = pj_strchr(user_realm, '@');
	if (str_realm.ptr != NULL) {
		user->ptr = user_realm->ptr;
		user->slen = (str_realm.ptr - user->ptr);
		str_realm.ptr++;
		str_realm.slen = user_realm->slen - user->slen - 1;
		*realm = str_realm;
	} else {
		*user = *user_realm;
		*realm = pj_str("");
	}

	return PJ_SUCCESS;
}

/* Get ice-ufrag and ice-pwd attribute */
static void retrieve_remote_turn_attr(struct transport_ice *tp_ice,
									  const pjmedia_sdp_media *rem_m,
									  pj_str_t *inv_cid)
{
	pjmedia_sdp_attr *attr, *use_turn_flag;
	pj_uint16_t port = 3479;
	pj_str_t server = pj_str(""), realm = pj_str(""), 
		username = pj_str(""), password = pj_str("");
	pj_str_t local_device_id;
	char tmp[60];

	// turn username and realm
	if (tp_ice) {
		local_device_id = pj_str(tp_ice->base.local_deviceid);
		parse_user_realm(&local_device_id, &password, &realm);
	}

	// turn server
	attr = pjmedia_sdp_attr_find2(rem_m->attr_count, 
		rem_m->attr, "X-adapter3", NULL);
	if (attr != NULL) {
		parse_host_port(&attr->value, &server, &port);
		if (port == 0)
			port = 3479;
	}

	// turn password
	if (inv_cid && inv_cid->slen > 0)
	{
		memset(tmp, 0, sizeof(tmp));
		sprintf(tmp, "z.%.*s", inv_cid->slen, inv_cid->ptr);
		pj_strdup2(tp_ice->pool, &username, tmp);
	}

	// use turn flag
	use_turn_flag = pjmedia_sdp_attr_find2(rem_m->attr_count, 
		rem_m->attr, "X-adapter5", NULL);
	if (use_turn_flag) {
		tp_ice->base.use_turn_flag = atoi(use_turn_flag->value.ptr);
		pj_ice_strans_set_use_turn_flag(tp_ice->ice_st, tp_ice->base.use_turn_flag);
	}

	pj_ice_strans_set_remote_turn_cfg(tp_ice->ice_st, server, port,
		realm, username, password);
}


/* Encode and add "a=ice-mismatch" attribute in the SDP */
static void encode_ice_mismatch(pj_pool_t *sdp_pool,
				pjmedia_sdp_session *sdp_local,
				unsigned media_index)
{
    pjmedia_sdp_attr *attr;
    pjmedia_sdp_media *m = sdp_local->media[media_index];

    attr = PJ_POOL_ALLOC_T(sdp_pool, pjmedia_sdp_attr);
    attr->name = STR_ICE_MISMATCH;
    attr->value.slen = 0;
    pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);
}


/* Encode ICE information in SDP */
static pj_status_t encode_session_in_sdp(struct transport_ice *tp_ice,
					 pj_pool_t *sdp_pool,
					 pjmedia_sdp_session *sdp_local,
					 unsigned media_index,
					 unsigned comp_cnt,
					 pj_bool_t restart_session)
{
    enum { 
	ATTR_BUF_LEN = 200,	/* Max len of a=candidate attr */
	RATTR_BUF_LEN= 200	/* Max len of a=remote-candidates attr */
    };
    pjmedia_sdp_media *m = sdp_local->media[media_index];
    pj_str_t local_ufrag, local_pwd;
    pjmedia_sdp_attr *attr;
    pj_status_t status;

    /* Must have a session */
    PJ_ASSERT_RETURN(pj_ice_strans_has_sess(tp_ice->ice_st), PJ_EBUG);

    /* Get ufrag and pwd from current session */
    pj_ice_strans_get_ufrag_pwd(tp_ice->ice_st, &local_ufrag, &local_pwd,
				NULL, NULL);

    /* The listing of candidates depends on whether ICE has completed
     * or not. When ICE has completed:
     *
     * 9.1.2.2: Existing Media Streams with ICE Completed
     *   The agent MUST include a candidate attributes for candidates
     *   matching the default destination for each component of the 
     *   media stream, and MUST NOT include any other candidates.
     *
     * When ICE has not completed, we shall include all candidates.
     *
     * Except when we have detected that remote is offering to restart
     * the session, in this case we will answer with full ICE SDP and
     * new ufrag/pwd pair.
     */
    if (!restart_session && pj_ice_strans_sess_is_complete(tp_ice->ice_st) &&
	pj_ice_strans_get_state(tp_ice->ice_st) != PJ_ICE_STRANS_STATE_FAILED)
    {
	const pj_ice_sess_check *check;
	char *attr_buf;
	pjmedia_sdp_conn *conn;
	pjmedia_sdp_attr *a_rtcp;
	pj_str_t rem_cand;
	unsigned comp;

	/* Encode ice-ufrag attribute */
	attr = pjmedia_sdp_attr_create(sdp_pool, STR_ICE_UFRAG.ptr,
				       &local_ufrag);
	pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);

	/* Encode ice-pwd attribute */
	attr = pjmedia_sdp_attr_create(sdp_pool, STR_ICE_PWD.ptr, 
				       &local_pwd);
	pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);

	/* Prepare buffer */
	attr_buf = (char*) pj_pool_alloc(sdp_pool, ATTR_BUF_LEN);
	rem_cand.ptr = (char*) pj_pool_alloc(sdp_pool, RATTR_BUF_LEN);
	rem_cand.slen = 0;

	/* 9.1.2.2: Existing Media Streams with ICE Completed
	 *   The default destination for media (i.e., the values of 
	 *   the IP addresses and ports in the m and c line used for
	 *   that media stream) MUST be the local candidate from the
	 *   highest priority nominated pair in the valid list for each
	 *   component.
	 */
	check = pj_ice_strans_get_valid_pair(tp_ice->ice_st, 1);
	if (check == NULL) {
	    pj_assert(!"Shouldn't happen");
	    return PJ_EBUG;
	}

	/* Override connection line address and media port number */
	conn = m->conn;
	if (conn == NULL)
	    conn = sdp_local->conn;

	conn->addr.ptr = (char*) pj_pool_alloc(sdp_pool, 
					       PJ_INET6_ADDRSTRLEN);
	pj_sockaddr_print(&check->lcand->addr, conn->addr.ptr, 
			  PJ_INET6_ADDRSTRLEN, 0);
	conn->addr.slen = pj_ansi_strlen(conn->addr.ptr);
	m->desc.port = pj_sockaddr_get_port(&check->lcand->addr);

	/* Override address RTCP attribute if it's present */
	if (comp_cnt == 2 &&
	    (check = pj_ice_strans_get_valid_pair(tp_ice->ice_st, 
						  COMP_RTCP)) != NULL &&
	    (a_rtcp = pjmedia_sdp_attr_find(m->attr_count, m->attr, 
					    &STR_RTCP, 0)) != NULL) 
	{
	    pjmedia_sdp_attr_remove(&m->attr_count, m->attr, a_rtcp);

	    a_rtcp = pjmedia_sdp_attr_create_rtcp(sdp_pool, 
						  &check->lcand->addr);
	    if (a_rtcp)
		pjmedia_sdp_attr_add(&m->attr_count, m->attr, a_rtcp);
	}

	/* Encode only candidates matching the default destination 
	 * for each component 
	 */
	for (comp=0; comp < comp_cnt; ++comp) {
	    int len;
	    pj_str_t value;

	    /* Get valid pair for this component */
	    check = pj_ice_strans_get_valid_pair(tp_ice->ice_st, comp+1);
	    if (check == NULL)
		continue;

	    /* Print and add local candidate in the pair */
	    value.ptr = attr_buf;
	    value.slen = print_sdp_cand_attr(attr_buf, ATTR_BUF_LEN, 
					     check->lcand, tp_ice->base.use_sctp);
	    if (value.slen < 0) {
		pj_assert(!"Not enough attr_buf to print candidate");
		return PJ_EBUG;
	    }

	    attr = pjmedia_sdp_attr_create(sdp_pool, STR_CANDIDATE.ptr,
					   &value);
	    pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);

	    /* Append to a=remote-candidates attribute */
	    if (pj_ice_strans_get_role(tp_ice->ice_st) == 
				    PJ_ICE_SESS_ROLE_CONTROLLING) 
	    {
		char rem_addr[PJ_INET6_ADDRSTRLEN];

		pj_sockaddr_print(&check->rcand->addr, rem_addr, 
				  sizeof(rem_addr), 0);
		len = pj_ansi_snprintf(
			   rem_cand.ptr + rem_cand.slen,
			   RATTR_BUF_LEN - rem_cand.slen,
			   "%s%u %s %u", 
			   (rem_cand.slen==0? "" : " "),
			   comp+1, rem_addr,
			   pj_sockaddr_get_port(&check->rcand->addr)
			   );
		if (len < 1 || len >= RATTR_BUF_LEN) {
		    pj_assert(!"Not enough buffer to print "
			       "remote-candidates");
		    return PJ_EBUG;
		}

		rem_cand.slen += len;
	    }
	}

	/* 9.1.2.2: Existing Media Streams with ICE Completed
	 *   In addition, if the agent is controlling, it MUST include
	 *   the a=remote-candidates attribute for each media stream 
	 *   whose check list is in the Completed state.  The attribute
	 *   contains the remote candidates from the highest priority 
	 *   nominated pair in the valid list for each component of that
	 *   media stream.
	 */
	if (pj_ice_strans_get_role(tp_ice->ice_st) == 
				    PJ_ICE_SESS_ROLE_CONTROLLING) 
	{
	    attr = pjmedia_sdp_attr_create(sdp_pool, STR_REM_CAND.ptr, 
					   &rem_cand);
	    pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);
	}

    } else if (pj_ice_strans_has_sess(tp_ice->ice_st) &&
	       pj_ice_strans_get_state(tp_ice->ice_st) !=
		        PJ_ICE_STRANS_STATE_FAILED)
    {
	/* Encode all candidates to SDP media */
	char *attr_buf;
	unsigned comp;

	/* If ICE is not restarted, encode current ICE ufrag/pwd.
	 * Otherwise generate new one.
	 */
	if (!restart_session) {
	    attr = pjmedia_sdp_attr_create(sdp_pool, STR_ICE_UFRAG.ptr,
					   &local_ufrag);
	    pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);

	    attr = pjmedia_sdp_attr_create(sdp_pool, STR_ICE_PWD.ptr, 
					   &local_pwd);
	    pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);

	} else {
	    pj_str_t str;

	    str.slen = PJ_ICE_UFRAG_LEN;
	    str.ptr = (char*) pj_pool_alloc(sdp_pool, str.slen);
	    pj_create_random_string(str.ptr, str.slen);
	    attr = pjmedia_sdp_attr_create(sdp_pool, STR_ICE_UFRAG.ptr, &str);
	    pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);

	    str.ptr = (char*) pj_pool_alloc(sdp_pool, str.slen);
	    pj_create_random_string(str.ptr, str.slen);
	    attr = pjmedia_sdp_attr_create(sdp_pool, STR_ICE_PWD.ptr, &str);
	    pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);
	}

	/* Create buffer to encode candidates as SDP attribute */
	attr_buf = (char*) pj_pool_alloc(sdp_pool, ATTR_BUF_LEN);

	for (comp=0; comp < comp_cnt; ++comp) {
	    unsigned cand_cnt;
	    pj_ice_sess_cand cand[PJ_ICE_ST_MAX_CAND];
	    unsigned i;

	    cand_cnt = PJ_ARRAY_SIZE(cand);
	    status = pj_ice_strans_enum_cands(tp_ice->ice_st, comp+1,
					      &cand_cnt, cand);
	    if (status != PJ_SUCCESS)
		return status;

	    for (i=0; i<cand_cnt; ++i) {
		pj_str_t value;

		value.slen = print_sdp_cand_attr(attr_buf, ATTR_BUF_LEN, 
						 &cand[i], tp_ice->base.use_sctp);
		if (value.slen < 0) {
		    pj_assert(!"Not enough attr_buf to print candidate");
		    return PJ_EBUG;
		}

		value.ptr = attr_buf;
		attr = pjmedia_sdp_attr_create(sdp_pool, 
					       STR_CANDIDATE.ptr,
					       &value);
		PJ_LOG(3, ("transport_ice.c", "encode_session_in_sdp() encode cand = %.*s", value.slen, value.ptr));
		pjmedia_sdp_attr_add(&m->attr_count, m->attr, attr);
	    }
	}
    } else {
	/* ICE has failed, application should have terminated this call */
    }

    /* Removing a=rtcp line when there is only one component. */
	if (comp_cnt == 1) {
		const pj_str_t STR_APPLICATION = { "application", 11};
		attr = pjmedia_sdp_attr_find(m->attr_count, m->attr, &STR_RTCP, NULL);
		if (attr)
			pjmedia_sdp_attr_remove(&m->attr_count, m->attr, attr);
		if (pj_strcmp(&sdp_local->media[media_index]->desc.media, &STR_APPLICATION) != 0) {
			/* If RTCP is not in use, we MUST send b=RS:0 and b=RR:0. */
			pj_assert(m->bandw_count + 2 <= PJ_ARRAY_SIZE(m->bandw));
			if (m->bandw_count + 2 <= PJ_ARRAY_SIZE(m->bandw)) {
				m->bandw[m->bandw_count] = PJ_POOL_ZALLOC_T(sdp_pool,
															pjmedia_sdp_bandw);
				pj_memcpy(&m->bandw[m->bandw_count]->modifier, &STR_BANDW_RS,
						  sizeof(pj_str_t));
				m->bandw_count++;
				m->bandw[m->bandw_count] = PJ_POOL_ZALLOC_T(sdp_pool,
															pjmedia_sdp_bandw);
				pj_memcpy(&m->bandw[m->bandw_count]->modifier, &STR_BANDW_RR,
						  sizeof(pj_str_t));
				m->bandw_count++;
			}
		}
    }
    

    return PJ_SUCCESS;
}


/* Parse a=candidate line */
static pj_status_t parse_cand(const char *obj_name,
			      pj_pool_t *pool,
			      const pj_str_t *orig_input,
			      pj_ice_sess_cand *cand)
{
    pj_str_t input;
    char *token, *host;
    int af;
    pj_str_t s;
    pj_status_t status = PJNATH_EICEINCANDSDP;
	pj_bool_t tcp = PJ_FALSE;

    pj_bzero(cand, sizeof(*cand));
    pj_strdup_with_null(pool, &input, orig_input);

	PJ_UNUSED_ARG(obj_name);

	/* Foundation */
	token = strtok(input.ptr, " ");
    if (!token) {
	TRACE__((obj_name, "Expecting ICE foundation in candidate"));
	goto on_return;
    }
    pj_strdup2(pool, &cand->foundation, token);

    /* Component ID */
    token = strtok(NULL, " ");
    if (!token) {
	TRACE__((obj_name, "Expecting ICE component ID in candidate"));
	goto on_return;
    }
    cand->comp_id = (pj_uint8_t) atoi(token);

	/* Transport */
	cand->transport_id = 1; // TP_STUN
    token = strtok(NULL, " ");
    if (!token) {
	TRACE__((obj_name, "Expecting ICE transport in candidate"));
	goto on_return;
    }
    if (pj_ansi_stricmp(token, "UDP") != 0 && 
		pj_ansi_stricmp(token, "TCP") != 0) {
	TRACE__((obj_name, 
		 "Expecting ICE TCP and UDP transport in candidate"));
	goto on_return;
	} else if (pj_ansi_stricmp(token, "TCP") == 0) {
		tcp = PJ_TRUE;
		cand->transport_id = 3; // TP_UPNP_TCP
	}

    /* Priority */
    token = strtok(NULL, " ");
    if (!token) {
	TRACE__((obj_name, "Expecting ICE priority in candidate"));
	goto on_return;
    }
    cand->prio = atoi(token);

    /* Host */
    host = strtok(NULL, " ");
    if (!host) {
	TRACE__((obj_name, "Expecting ICE host in candidate"));
	goto on_return;
    }
    /* Detect address family */
    if (pj_ansi_strchr(host, ':'))
	af = pj_AF_INET6();
    else
	af = pj_AF_INET();
    /* Assign address */
    if (pj_sockaddr_init(af, &cand->addr, pj_cstr(&s, host), 0)) {
	TRACE__((obj_name, "Invalid ICE candidate address"));
	goto on_return;
    }

    /* Port */
    token = strtok(NULL, " ");
    if (!token) {
	TRACE__((obj_name, "Expecting ICE port number in candidate"));
	goto on_return;
    }
    pj_sockaddr_set_port(&cand->addr, (pj_uint16_t)atoi(token));

    /* typ */
    token = strtok(NULL, " ");
    if (!token) {
	TRACE__((obj_name, "Expecting ICE \"typ\" in candidate"));
	goto on_return;
    }
    if (pj_ansi_stricmp(token, "typ") != 0) {
	TRACE__((obj_name, "Expecting ICE \"typ\" in candidate"));
	goto on_return;
    }

    /* candidate type */
    token = strtok(NULL, " ");
    if (!token) {
	TRACE__((obj_name, "Expecting ICE candidate type in candidate"));
	goto on_return;
    }

	if (pj_ansi_stricmp(token, "host") == 0) {
		if (!tcp)
			cand->type = PJ_ICE_CAND_TYPE_HOST;
		else
			cand->type = PJ_ICE_CAND_TYPE_HOST_TCP;

    } else if (pj_ansi_stricmp(token, "srflx") == 0) {
		if (!tcp)
			cand->type = PJ_ICE_CAND_TYPE_SRFLX;
		else
			cand->type = PJ_ICE_CAND_TYPE_SRFLX_TCP;

    } else if (pj_ansi_stricmp(token, "relay") == 0) {
		cand->type = PJ_ICE_CAND_TYPE_RELAYED;
		cand->transport_id = 2; // TP_TURN

    } else if (pj_ansi_stricmp(token, "prflx") == 0) {
	cand->type = PJ_ICE_CAND_TYPE_PRFLX;

    } else {
	PJ_LOG(5,(obj_name, "Invalid ICE candidate type %s in candidate", 
		  token));
	goto on_return;
    }

	if (tcp && cand->type != PJ_ICE_CAND_TYPE_RELAYED_TCP) {
		if (cand->type != PJ_ICE_CAND_TYPE_HOST && 
			cand->type != PJ_ICE_CAND_TYPE_HOST_TCP) {
			int i;
			for (i = 0; i < 4; i++) {
				token = strtok(NULL, " ");
				if (!token) {
				TRACE__((obj_name, "Expecting ICE tcptype in candidate"));
				goto on_return;
				}
			}
		}

		/* tcp type */
		token = strtok(NULL, " ");
		if (!token) {
		TRACE__((obj_name, "Expecting ICE tcptype in candidate"));
		goto on_return;
		}
		if (pj_ansi_stricmp(token, "tcptype") != 0) {
		TRACE__((obj_name, "Expecting ICE \"typ\" in candidate"));
		goto on_return;
		}

		token = strtok(NULL, " ");
		if (!token) {
		TRACE__((obj_name, "Expecting ICE tcp type in candidate"));
		goto on_return;
		}
		if (pj_ansi_stricmp(token, "active") == 0) {
		cand->tcp_type = PJ_ICE_CAND_TCP_TYPE_ACTIVE;

		} else if (pj_ansi_stricmp(token, "passive") == 0) {
		cand->tcp_type = PJ_ICE_CAND_TCP_TYPE_PASSIVE;

		} else if (pj_ansi_stricmp(token, "so") == 0) {
		cand->tcp_type = PJ_ICE_CAND_TCP_TYPE_SO;

		} else {
		cand->tcp_type = PJ_ICE_CAND_TCP_TYPE_NONE;
		}
	}

	if (cand->type == PJ_ICE_CAND_TYPE_RELAYED) {
		if (token)
			token = strtok(NULL, " ");
		if (token)
			token = strtok(NULL, " ");
		if (token)
			token = strtok(NULL, " ");
		if (token)
			token = strtok(NULL, " ");
		if (token)
			token = strtok(NULL, " ");
		
		if (token && pj_ansi_stricmp(token, "generation") == 0) {
			token = strtok(NULL, " ");
			if (token)
				token = strtok(NULL, " ");
		}

		if (token && pj_ansi_stricmp(token, "TCP") == 0) {
			cand->type = PJ_ICE_CAND_TYPE_RELAYED_TCP;
			cand->transport_id = 4; // TP_TURN_TCP
		} else if (token && pj_ansi_stricmp(token, "Enabled") == 0) {
			cand->enabled = PJ_TRUE;
			status = PJ_SUCCESS;
			goto on_return;
		} else if (token && pj_ansi_stricmp(token, "Disabled") == 0) {
			cand->enabled = PJ_FALSE;
			status = PJ_SUCCESS;
			goto on_return;
		} else if (!token) {
			cand->enabled = PJ_TRUE;
			status = PJ_SUCCESS;
			goto on_return;
		}
	}

	if (cand->type == PJ_ICE_CAND_TYPE_SRFLX) {
			if (token)
				token = strtok(NULL, " ");
			if (token)
				token = strtok(NULL, " ");
			if (token)
				token = strtok(NULL, " ");
			if (token)
				token = strtok(NULL, " ");
	}

	/* Enabled or Disabled */
	if (token) {
		token = strtok(NULL, " ");
		if (token) {
			if (pj_ansi_stricmp(token, "Enabled") == 0 ||
				pj_ansi_stricmp(token, "generation") == 0 ) // dean : generation is for WebRTC data channel.
				cand->enabled = PJ_TRUE;
			else
				cand->enabled = PJ_FALSE;
		} else {
			cand->enabled = PJ_TRUE;
		}
	} else {
		cand->enabled = PJ_TRUE;
	}

    status = PJ_SUCCESS;

on_return:
    return status;
}


/* Create initial SDP offer */
static pj_status_t create_initial_offer(struct transport_ice *tp_ice,
					pj_pool_t *sdp_pool,
					pjmedia_sdp_session *loc_sdp,
					unsigned media_index)
{
    pj_status_t status;

    /* Encode ICE in SDP */
    status = encode_session_in_sdp(tp_ice, sdp_pool, loc_sdp, media_index, 
				   tp_ice->comp_cnt, PJ_FALSE);
    if (status != PJ_SUCCESS) {
	set_no_ice(tp_ice, "Error encoding SDP answer", status);
	return status;
    }

    return PJ_SUCCESS;
}


/* Verify incoming offer */
static pj_status_t verify_ice_sdp(struct transport_ice *tp_ice,
				  pj_pool_t *tmp_pool,
				  const pjmedia_sdp_session *rem_sdp,
				  unsigned media_index,
				  pj_ice_sess_role current_ice_role,
				  struct sdp_state *sdp_state)
{
    const pjmedia_sdp_media *rem_m;
    const pjmedia_sdp_attr *ufrag_attr, *pwd_attr;
    const pjmedia_sdp_conn *rem_conn;
    pj_bool_t comp1_found=PJ_FALSE, comp2_found=PJ_FALSE, has_rtcp=PJ_FALSE;
    pj_sockaddr rem_conn_addr, rtcp_addr;
    unsigned i;
    pj_status_t status;

	int inst_id = tp_ice->base.inst_id;

    rem_m = rem_sdp->media[media_index];

    /* Get the "ice-ufrag" and "ice-pwd" attributes */
    get_ice_attr(rem_sdp, rem_m, &ufrag_attr, &pwd_attr);

    /* If "ice-ufrag" or "ice-pwd" are not found, disable ICE */
    if (ufrag_attr==NULL || pwd_attr==NULL) {
	sdp_state->match_comp_cnt = 0;
	return PJ_SUCCESS;
    }

    /* Verify that default target for each component matches one of the 
     * candidate for the component. Otherwise stop ICE with ICE ice_mismatch 
     * error.
     */

    /* Component 1 is the c= line */
    rem_conn = rem_m->conn;
    if (rem_conn == NULL)
	rem_conn = rem_sdp->conn;
    if (!rem_conn)
	return PJMEDIA_SDP_EMISSINGCONN;

    /* Verify address family matches */
    if ((tp_ice->af==pj_AF_INET() && 
	 pj_strcmp(&rem_conn->addr_type, &STR_IP4)!=0) ||
	(tp_ice->af==pj_AF_INET6() && 
	 pj_strcmp(&rem_conn->addr_type, &STR_IP6)!=0))
    {
	return PJMEDIA_SDP_ETPORTNOTEQUAL;
    }

    /* Assign remote connection address */
    status = pj_sockaddr_init(tp_ice->af, &rem_conn_addr, &rem_conn->addr,
			      (pj_uint16_t)rem_m->desc.port);
    if (status != PJ_SUCCESS)
	return status;

    if (tp_ice->comp_cnt > 1) {
	const pjmedia_sdp_attr *attr;

	/* Get default RTCP candidate from a=rtcp line, if present, otherwise
	 * calculate default RTCP candidate from default RTP target.
	 */
	attr = pjmedia_sdp_attr_find(rem_m->attr_count, rem_m->attr, 
				     &STR_RTCP, NULL);
	has_rtcp = (attr != NULL);

	if (attr) {
	    pjmedia_sdp_rtcp_attr rtcp_attr;

	    status = pjmedia_sdp_attr_get_rtcp(inst_id, attr, &rtcp_attr);
	    if (status != PJ_SUCCESS) {
		/* Error parsing a=rtcp attribute */
		return status;
	    }
    	
	    if (rtcp_attr.addr.slen) {
		/* Verify address family matches */
		if ((tp_ice->af==pj_AF_INET() && 
		     pj_strcmp(&rtcp_attr.addr_type, &STR_IP4)!=0) ||
		    (tp_ice->af==pj_AF_INET6() && 
		     pj_strcmp(&rtcp_attr.addr_type, &STR_IP6)!=0))
		{
		    return PJMEDIA_SDP_ETPORTNOTEQUAL;
		}

		/* Assign RTCP address */
		status = pj_sockaddr_init(tp_ice->af, &rtcp_addr,
					  &rtcp_attr.addr,
					  (pj_uint16_t)rtcp_attr.port);
		if (status != PJ_SUCCESS) {
		    return PJMEDIA_SDP_EINRTCP;
		}
	    } else {
		/* Assign RTCP address */
		status = pj_sockaddr_init(tp_ice->af, &rtcp_addr, 
					  NULL, 
					  (pj_uint16_t)rtcp_attr.port);
		if (status != PJ_SUCCESS) {
		    return PJMEDIA_SDP_EINRTCP;
		}
		pj_sockaddr_copy_addr(&rtcp_addr, &rem_conn_addr);
	    }
	} else {
	    unsigned rtcp_port;
    	
		rtcp_port = pj_sockaddr_get_port(&rem_conn_addr) + 1;
	    pj_sockaddr_cp(&rtcp_addr, &rem_conn_addr);
	    pj_sockaddr_set_port(&rtcp_addr, (pj_uint16_t)rtcp_port);
	}
    }

    /* Find the default addresses in a=candidate attributes. 
     */
    for (i=0; i<rem_m->attr_count; ++i) {
	pj_ice_sess_cand cand;

	if (pj_strcmp(&rem_m->attr[i]->name, &STR_CANDIDATE)!=0)
	    continue;

	status = parse_cand(tp_ice->base.name, tmp_pool, 
			    &rem_m->attr[i]->value, &cand);
	if (status != PJ_SUCCESS) {
	    PJ_LOG(4,(tp_ice->base.name, 
		      "Error in parsing SDP candidate attribute '%.*s', "
		      "candidate is ignored",
		      (int)rem_m->attr[i]->value.slen, 
		      rem_m->attr[i]->value.ptr));
	    continue;
	}

	if (!comp1_found && cand.comp_id==COMP_RTP && 
		(pj_ice_strans_tp_is_upnp_tcp(cand.transport_id) || 
		  pj_sockaddr_cmp(&rem_conn_addr, &cand.addr)==0)) 
	{
	    comp1_found = PJ_TRUE;
	} else if (!comp2_found && cand.comp_id==COMP_RTCP &&
		    (pj_ice_strans_tp_is_upnp_tcp(cand.transport_id) || 
			  pj_sockaddr_cmp(&rtcp_addr, &cand.addr)==0)) 
	{
	    comp2_found = PJ_TRUE;
	}

	if (cand.comp_id == COMP_RTCP)
	    has_rtcp = PJ_TRUE;

	if (comp1_found && (comp2_found || tp_ice->comp_cnt==1))
	    break;
    }

    /* Check matched component count and ice_mismatch */
    if (comp1_found && (tp_ice->comp_cnt==1 || !has_rtcp)) {
	sdp_state->match_comp_cnt = 1;
	sdp_state->ice_mismatch = PJ_FALSE;
    } else if (comp1_found && comp2_found) {
	sdp_state->match_comp_cnt = 2;
	sdp_state->ice_mismatch = PJ_FALSE;
    } else {
	sdp_state->match_comp_cnt = (tp_ice->comp_cnt > 1 && has_rtcp)? 2 : 1;
	sdp_state->ice_mismatch = PJ_TRUE;
    }


    /* Detect remote restarting session */
    if (pj_ice_strans_has_sess(tp_ice->ice_st) &&
	(pj_ice_strans_sess_is_running(tp_ice->ice_st) ||
	 pj_ice_strans_sess_is_complete(tp_ice->ice_st))) 
    {
	pj_str_t rem_run_ufrag, rem_run_pwd;
	pj_ice_strans_get_ufrag_pwd(tp_ice->ice_st, NULL, NULL,
				    &rem_run_ufrag, &rem_run_pwd);
	if (pj_strcmp(&ufrag_attr->value, &rem_run_ufrag) ||
	    pj_strcmp(&pwd_attr->value, &rem_run_pwd))
	{
	    /* Remote offers to restart ICE */
	    sdp_state->ice_restart = PJ_TRUE;
	} else {
	    sdp_state->ice_restart = PJ_FALSE;
	}
    } else {
	sdp_state->ice_restart = PJ_FALSE;
    }

    /* Detect our role */
    if (current_ice_role==PJ_ICE_SESS_ROLE_CONTROLLING) {
	sdp_state->local_role = PJ_ICE_SESS_ROLE_CONTROLLING;
    } else {
	if (pjmedia_sdp_attr_find(rem_sdp->attr_count, rem_sdp->attr,
				  &STR_ICE_LITE, NULL) != NULL)
	{
	    /* Remote is ICE Lite */
	    sdp_state->local_role = PJ_ICE_SESS_ROLE_CONTROLLING;
	} else {
	    sdp_state->local_role = PJ_ICE_SESS_ROLE_CONTROLLED;
	}
    }

    PJ_LOG(4,(tp_ice->base.name, 
	      "Processing SDP: support ICE=%u, common comp_cnt=%u, "
	      "ice_mismatch=%u, ice_restart=%u, local_role=%s",
	      (sdp_state->match_comp_cnt != 0), 
	      sdp_state->match_comp_cnt, 
	      sdp_state->ice_mismatch, 
	      sdp_state->ice_restart,
	      pj_ice_sess_role_name(sdp_state->local_role)));

    return PJ_SUCCESS;

}


/* Verify incoming offer and create initial answer */
static pj_status_t create_initial_answer(struct transport_ice *tp_ice,
					 pj_pool_t *sdp_pool,
					 pjmedia_sdp_session *loc_sdp,
					 const pjmedia_sdp_session *rem_sdp,
					 unsigned media_index)
{
    const pjmedia_sdp_media *rem_m = rem_sdp->media[media_index];
    pj_status_t status;

    /* Check if media is removed (just in case) */
    if (rem_m->desc.port == 0) {
	return PJ_SUCCESS;
    }

    /* Verify the offer */
    status = verify_ice_sdp(tp_ice, sdp_pool, rem_sdp, media_index, 
			    PJ_ICE_SESS_ROLE_CONTROLLED, 
			    &tp_ice->rem_offer_state);
    if (status != PJ_SUCCESS) {
	set_no_ice(tp_ice, "Invalid SDP offer", status);
	return status;
    }

    /* Does remote support ICE? */
    if (tp_ice->rem_offer_state.match_comp_cnt==0) {
	set_no_ice(tp_ice, "No ICE found in SDP offer", PJ_SUCCESS);
	return PJ_SUCCESS;
    }

    /* ICE ice_mismatch? */
    if (tp_ice->rem_offer_state.ice_mismatch) {
	set_no_ice(tp_ice, "ICE ice_mismatch in remote offer", PJ_SUCCESS);
	encode_ice_mismatch(sdp_pool, loc_sdp, media_index);
	return PJ_SUCCESS;
    }

    /* Encode ICE in SDP */
    status = encode_session_in_sdp(tp_ice, sdp_pool, loc_sdp, media_index, 
				   tp_ice->rem_offer_state.match_comp_cnt,
				   PJ_FALSE);
    if (status != PJ_SUCCESS) {
	set_no_ice(tp_ice, "Error encoding SDP answer", status);
	return status;
    }

    return PJ_SUCCESS;
}


/* Create subsequent SDP offer */
static pj_status_t create_subsequent_offer(struct transport_ice *tp_ice,
					   pj_pool_t *sdp_pool,
					   pjmedia_sdp_session *loc_sdp,
					   unsigned media_index)
{
    unsigned comp_cnt;

    if (pj_ice_strans_has_sess(tp_ice->ice_st) == PJ_FALSE) {
	/* We don't have ICE */
	return PJ_SUCCESS;
    }

    comp_cnt = pj_ice_strans_get_running_comp_cnt(tp_ice->ice_st);
    return encode_session_in_sdp(tp_ice, sdp_pool, loc_sdp, media_index,
				 comp_cnt, PJ_FALSE);
}


/* Create subsequent SDP answer */
static pj_status_t create_subsequent_answer(struct transport_ice *tp_ice,
					    pj_pool_t *sdp_pool,
					    pjmedia_sdp_session *loc_sdp,
					    const pjmedia_sdp_session *rem_sdp,
					    unsigned media_index)
{
    pj_status_t status;

    /* We have a session */
    status = verify_ice_sdp(tp_ice, sdp_pool, rem_sdp, media_index, 
			    PJ_ICE_SESS_ROLE_CONTROLLED, 
			    &tp_ice->rem_offer_state);
    if (status != PJ_SUCCESS) {
	/* Something wrong with the offer */
	return status;
    }

    if (pj_ice_strans_has_sess(tp_ice->ice_st)) {
	/*
	 * Received subsequent offer while we have ICE active.
	 */

	if (tp_ice->rem_offer_state.match_comp_cnt == 0) {
	    /* Remote no longer offers ICE */
	    return PJ_SUCCESS;
	}

	if (tp_ice->rem_offer_state.ice_mismatch) {
	    encode_ice_mismatch(sdp_pool, loc_sdp, media_index);
	    return PJ_SUCCESS;
	}

	status = encode_session_in_sdp(tp_ice, sdp_pool, loc_sdp, media_index,
				       tp_ice->rem_offer_state.match_comp_cnt,
				       tp_ice->rem_offer_state.ice_restart);
	if (status != PJ_SUCCESS)
	    return status;

	/* Done */

    } else {
	/*
	 * Received subsequent offer while we DON'T have ICE active.
	 */

	if (tp_ice->rem_offer_state.match_comp_cnt == 0) {
	    /* Remote does not support ICE */
	    return PJ_SUCCESS;
	}

	if (tp_ice->rem_offer_state.ice_mismatch) {
	    encode_ice_mismatch(sdp_pool, loc_sdp, media_index);
	    return PJ_SUCCESS;
	}

	/* Looks like now remote is offering ICE, so we need to create
	 * ICE session now.
	 */
	status = pj_ice_strans_init_ice(tp_ice->ice_st, 
					PJ_ICE_SESS_ROLE_CONTROLLED,
					NULL, NULL);
	if (status != PJ_SUCCESS) {
	    /* Fail to create new ICE session */
	    return status;
	}

	status = encode_session_in_sdp(tp_ice, sdp_pool, loc_sdp, media_index,
				       tp_ice->rem_offer_state.match_comp_cnt,
				       tp_ice->rem_offer_state.ice_restart);
	if (status != PJ_SUCCESS)
	    return status;

	/* Done */
    }

    return PJ_SUCCESS;
}


/*
 * For both UAC and UAS, pass in the SDP before sending it to remote.
 * This will add ICE attributes to the SDP.
 */
static pj_status_t transport_media_create(pjmedia_transport *tp,
				          pj_pool_t *sdp_pool,
					  unsigned options,
					  const pjmedia_sdp_session *rem_sdp,
					  unsigned media_index)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    pj_ice_sess_role ice_role;
    pj_status_t status;

    PJ_UNUSED_ARG(media_index);
    PJ_UNUSED_ARG(sdp_pool);

    tp_ice->media_option = options;
    tp_ice->oa_role = ROLE_NONE;
    tp_ice->initial_sdp = PJ_TRUE;

    /* Init ICE, the initial role is set now based on availability of
     * rem_sdp, but it will be checked again later.
     */
    ice_role = (rem_sdp==NULL ? PJ_ICE_SESS_ROLE_CONTROLLING : 
		PJ_ICE_SESS_ROLE_CONTROLLED);

	PJ_LOG(4,(tp_ice->base.name, 
		  "tp_ice=%p, tp_ice->use_upnp_flag=%d, tp_ice->use_stun_cand=%d, tp_ice->use_turn_flag=%d", 
		  tp_ice, tp_ice->base.use_upnp_flag, 
		  tp_ice->base.use_stun_cand,
		  tp_ice->base.use_turn_flag));
	// set use upnp tcp flag
	pj_ice_strans_set_use_upnp_flag(tp_ice->ice_st, tp_ice->base.use_upnp_flag);
	// set use stun cand flag
	pj_ice_strans_set_use_stun_cand(tp_ice->ice_st, tp_ice->base.use_stun_cand);
	// set use turn flag
	pj_ice_strans_set_use_turn_flag(tp_ice->ice_st, tp_ice->base.use_turn_flag);
	// set ice role
	pj_ice_strans_set_ice_role(tp_ice->ice_st, ice_role);
	// set app's lock object
	pj_ice_strans_set_app_lock(tp_ice->ice_st, tp_ice->base.app_lock);
	// set dest uri
	pj_ice_strans_set_dest_uri(tp_ice->ice_st, tp_ice->base.dest_uri);

	// if it is UAS, retrieve remote turn attribute and save it to remo_turn config
	if ((tp_ice->base.use_turn_flag & TURN_FLAG_USE_UAC_TURN) > 0 && 
		ice_role == PJ_ICE_SESS_ROLE_CONTROLLED)
		retrieve_remote_turn_attr(tp_ice, rem_sdp->media[media_index], &rem_sdp->inv_cid);

    status = pj_ice_strans_init_ice(tp_ice->ice_st, ice_role, NULL, NULL);

    /* Done */
    return status;
}


static pj_status_t transport_encode_sdp(pjmedia_transport *tp,
				        pj_pool_t *sdp_pool,
				        pjmedia_sdp_session *sdp_local,
				        const pjmedia_sdp_session *rem_sdp,
				        unsigned media_index)
{
	struct transport_ice *tp_ice = (struct transport_ice*)tp;
    pj_status_t status;

    /* Validate media transport */
    /* This transport only support RTP/AVP transport, unless if
     * transport checking is disabled
     */
    if ((tp_ice->media_option & PJMEDIA_TPMED_NO_TRANSPORT_CHECKING) == 0) {
	pjmedia_sdp_media *loc_m, *rem_m;

	rem_m = rem_sdp? rem_sdp->media[media_index] : NULL;
	loc_m = sdp_local->media[media_index];

	if (pj_stricmp(&loc_m->desc.transport, &STR_RTP_AVP) ||
	   (rem_m && pj_stricmp(&rem_m->desc.transport, &STR_RTP_AVP)))
	{
	    pjmedia_sdp_media_deactivate(sdp_pool, loc_m);
	    return PJMEDIA_SDP_EINPROTO;
	}
    }
#if 1
	if (rem_sdp) { // got remote sdp
		/* Do checking stuffs here.. */
		/* get user id */
		pjmedia_sdp_attr *userid, *deviceid, *turnsrv, *turnpwd;

		if (rem_sdp->media_count > 0) {
			// user id
			userid = pjmedia_sdp_attr_find2(rem_sdp->media[0]->attr_count, 
				rem_sdp->media[0]->attr, "X-adapter1", NULL);
			if (userid == NULL)
				pjmedia_transport_set_remote_userid(&tp_ice->base, "", 0);
			else
				pjmedia_transport_set_remote_userid(&tp_ice->base, 
				userid->value.ptr, userid->value.slen);

			// device id
			deviceid = pjmedia_sdp_attr_find2(rem_sdp->media[0]->attr_count, 
				rem_sdp->media[0]->attr, "X-adapter2", NULL);
			if (deviceid == NULL)
				pjmedia_transport_set_remote_deviceid(&tp_ice->base, "", 0);
			else
				pjmedia_transport_set_remote_deviceid(&tp_ice->base, 
				deviceid->value.ptr, deviceid->value.slen);

			// turn server
			turnsrv = pjmedia_sdp_attr_find2(rem_sdp->media[0]->attr_count, 
				rem_sdp->media[0]->attr, "X-adapter3", NULL);
			if (turnsrv == NULL)
				pjmedia_transport_set_remote_turnsvr(&tp_ice->base, "", 0);
			else
				pjmedia_transport_set_remote_turnsvr(&tp_ice->base, 
				turnsrv->value.ptr, turnsrv->value.slen);

			// turn password
			turnpwd = pjmedia_sdp_attr_find2(rem_sdp->media[0]->attr_count, 
				rem_sdp->media[0]->attr, "X-adapter4", NULL);
			if (turnpwd == NULL)
				pjmedia_transport_set_remote_turnpwd(&tp_ice->base, "", 0);
			else
				pjmedia_transport_set_remote_turnpwd(&tp_ice->base, 
				turnpwd->value.ptr, turnpwd->value.slen);
		}
	}
	

	{
		/* We add a proprietary attribute here.. */
		pjmedia_sdp_attr *my_attr;
		char userid[64];
		char deviceid[128];
		char turnsrv[128];
		char turnpwd[128];
		char inv_recv_time[64];
		char use_turn_flag[64];
		pj_parsed_time inv_recv_ptime;
		int len;
		char nat_type[64];

		// user id
		memset(userid, 0, sizeof(userid));
		pjmedia_transport_get_local_userid(&tp_ice->base, userid);
		my_attr = PJ_POOL_ALLOC_T(sdp_pool, pjmedia_sdp_attr);
		pj_strdup2(sdp_pool, &my_attr->name, "X-adapter1");
		pj_strdup2(sdp_pool, &my_attr->value, userid);

		pjmedia_sdp_attr_add(&sdp_local->media[media_index]->attr_count,
			sdp_local->media[media_index]->attr,
			my_attr);

		// device id
		memset(deviceid, 0, sizeof(deviceid));
		pjmedia_transport_get_local_deviceid(&tp_ice->base, deviceid);
		my_attr = PJ_POOL_ALLOC_T(sdp_pool, pjmedia_sdp_attr);
		pj_strdup2(sdp_pool, &my_attr->name, "X-adapter2");
		pj_strdup2(sdp_pool, &my_attr->value, deviceid);

		pjmedia_sdp_attr_add(&sdp_local->media[media_index]->attr_count,
			sdp_local->media[media_index]->attr,
			my_attr);

		// turn server
		memset(turnsrv, 0, sizeof(turnsrv));
		pjmedia_transport_get_local_turnsrv(&tp_ice->base, turnsrv);
		my_attr = PJ_POOL_ALLOC_T(sdp_pool, pjmedia_sdp_attr);
		pj_strdup2(sdp_pool, &my_attr->name, "X-adapter3");
		pj_strdup2(sdp_pool, &my_attr->value, turnsrv);

		pjmedia_sdp_attr_add(&sdp_local->media[media_index]->attr_count,
			sdp_local->media[media_index]->attr,
			my_attr);

		// turn password
		memset(turnpwd, 0, sizeof(turnpwd));
		pjmedia_transport_get_local_turnpwd(&tp_ice->base, turnpwd);
		my_attr = PJ_POOL_ALLOC_T(sdp_pool, pjmedia_sdp_attr);
		pj_strdup2(sdp_pool, &my_attr->name, "X-adapter4");
		pj_strdup2(sdp_pool, &my_attr->value, turnpwd);

		pjmedia_sdp_attr_add(&sdp_local->media[media_index]->attr_count,
			sdp_local->media[media_index]->attr,
			my_attr);

		if ((tp_ice->base.use_turn_flag & TURN_FLAG_USE_TCP_TURN) > 0) {
			// use turn flag
			memset(use_turn_flag, 0, sizeof(use_turn_flag));
			sprintf(use_turn_flag, "%d", tp_ice->base.use_turn_flag);
			my_attr = PJ_POOL_ALLOC_T(sdp_pool, pjmedia_sdp_attr);
			pj_strdup2(sdp_pool, &my_attr->name, "X-adapter5");
			pj_strdup2(sdp_pool, &my_attr->value, use_turn_flag);

			pjmedia_sdp_attr_add(&sdp_local->media[media_index]->attr_count,
				sdp_local->media[media_index]->attr,
				my_attr);
		}

		// Invite message receive timestamp
		memset(inv_recv_time, 0, sizeof(inv_recv_time));
		pj_time_decode(&tp_ice->base.inv_recv_time, &inv_recv_ptime);
		len = pj_ansi_snprintf(inv_recv_time, sizeof(inv_recv_time),
			"%04d-%02d-%02d_%02d:%02d:%02d.%03d",
			inv_recv_ptime.year, inv_recv_ptime.mon+1, inv_recv_ptime.day, 
			inv_recv_ptime.hour, inv_recv_ptime.min, inv_recv_ptime.sec, inv_recv_ptime.msec);

		my_attr = PJ_POOL_ALLOC_T(sdp_pool, pjmedia_sdp_attr);
		pj_strdup2(sdp_pool, &my_attr->name, "inv-time");
		pj_strdup2(sdp_pool, &my_attr->value, inv_recv_time);

		pjmedia_sdp_attr_add(&sdp_local->media[media_index]->attr_count,
			sdp_local->media[media_index]->attr,
			my_attr);

		// nat type 
		memset(nat_type, 0, sizeof(nat_type));
		pjmedia_transport_get_local_nattype(&tp_ice->base, nat_type);
		my_attr = PJ_POOL_ALLOC_T(sdp_pool, pjmedia_sdp_attr);
		pj_strdup2(sdp_pool, &my_attr->name, "nat-type");
		pj_strdup2(sdp_pool, &my_attr->value, nat_type);

		pjmedia_sdp_attr_add(&sdp_local->media[media_index]->attr_count,
			sdp_local->media[media_index]->attr,
			my_attr);
	}
#endif
    if (tp_ice->initial_sdp) {
	if (rem_sdp) {
	    status = create_initial_answer(tp_ice, sdp_pool, sdp_local, 
					   rem_sdp, media_index);
	} else {
	    status = create_initial_offer(tp_ice, sdp_pool, sdp_local,
					  media_index);
	}
    } else {
	if (rem_sdp) {
	    status = create_subsequent_answer(tp_ice, sdp_pool, sdp_local,
					      rem_sdp, media_index);
	} else {
	    status = create_subsequent_offer(tp_ice, sdp_pool, sdp_local,
					     media_index);
	}
    }

    if (status==PJ_SUCCESS) {
	if (rem_sdp)
	    tp_ice->oa_role = ROLE_ANSWERER;
	else
	    tp_ice->oa_role = ROLE_OFFERER;
    }

    return status;
}


/* Start ICE session with the specified remote SDP */
static pj_status_t start_ice(struct transport_ice *tp_ice,
			     pj_pool_t *tmp_pool,
			     const pjmedia_sdp_session *rem_sdp,
			     unsigned media_index)
{
    pjmedia_sdp_media *rem_m = rem_sdp->media[media_index];
    const pjmedia_sdp_attr *ufrag_attr, *pwd_attr;
    pj_ice_sess_cand *cand;
    unsigned i, cand_cnt;
	pj_status_t status;

    get_ice_attr(rem_sdp, rem_m, &ufrag_attr, &pwd_attr);

    /* Allocate candidate array */
    cand = (pj_ice_sess_cand*)
	   pj_pool_calloc(tmp_pool, PJ_ICE_MAX_CAND, 
			  sizeof(pj_ice_sess_cand));

    /* Get all candidates in the media */
	cand_cnt = 0;
    for (i=0; i<rem_m->attr_count && cand_cnt < PJ_ICE_MAX_CAND; ++i) {
	pjmedia_sdp_attr *attr;

	attr = rem_m->attr[i];

	if (pj_strcmp(&attr->name, &STR_CANDIDATE)!=0)
	    continue;

	/* Parse candidate */
	status = parse_cand(tp_ice->base.name, tmp_pool, &attr->value, 
			    &cand[cand_cnt]);
	if (status != PJ_SUCCESS) {
	    PJ_LOG(4,(tp_ice->base.name, 
		      "Error in parsing SDP candidate attribute '%.*s', "
		      "candidate is ignored",
		      (int)attr->value.slen, attr->value.ptr));
	    continue;
	}

	cand_cnt++;
	}

    /* Start ICE */
    return pj_ice_strans_start_ice(tp_ice->ice_st, &ufrag_attr->value, 
				   &pwd_attr->value, cand_cnt, cand);
}


/*
 * Start ICE checks when both offer and answer have been negotiated
 * by SDP negotiator.
 */
static pj_status_t transport_media_start(pjmedia_transport *tp,
				         pj_pool_t *tmp_pool,
				         const pjmedia_sdp_session *sdp_local,
				         const pjmedia_sdp_session *rem_sdp,
				         unsigned media_index)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    pjmedia_sdp_media *rem_m;
    enum oa_role current_oa_role;
    pj_bool_t initial_oa;
    pj_status_t status;

    PJ_ASSERT_RETURN(tp && tmp_pool && rem_sdp, PJ_EINVAL);
    PJ_ASSERT_RETURN(media_index < rem_sdp->media_count, PJ_EINVAL);

    rem_m = rem_sdp->media[media_index];

    initial_oa = tp_ice->initial_sdp;
    current_oa_role = tp_ice->oa_role;

    /* SDP has been negotiated */
    tp_ice->initial_sdp = PJ_FALSE;
    tp_ice->oa_role = ROLE_NONE;

    /* Nothing to do if we don't have ICE session */
    if (pj_ice_strans_has_sess(tp_ice->ice_st) == PJ_FALSE) {
	return PJ_SUCCESS;
    }

    /* Special case for Session Timer. The re-INVITE for session refresh
     * doesn't call transport_encode_sdp(), causing current_oa_role to
     * be set to ROLE_NONE. This is a workaround.
     */
    if (current_oa_role == ROLE_NONE) {
	current_oa_role = ROLE_OFFERER;
    }

    /* Processing depends on the offer/answer role */
    if (current_oa_role == ROLE_OFFERER) {
	/*
	 * We are offerer. So this will be the first time we see the
	 * remote's SDP.
	 */
	struct sdp_state answer_state;

	/* Verify the answer */
	status = verify_ice_sdp(tp_ice, tmp_pool, rem_sdp, media_index, 
				PJ_ICE_SESS_ROLE_CONTROLLING, &answer_state);
	if (status != PJ_SUCCESS) {
	    /* Something wrong in the SDP answer */
	    set_no_ice(tp_ice, "Invalid remote SDP answer", status);
	    return status;
	}

	/* Does it have ICE? */
	if (answer_state.match_comp_cnt == 0) {
	    /* Remote doesn't support ICE */
	    set_no_ice(tp_ice, "Remote answer doesn't support ICE", 
		       PJ_SUCCESS);
	    return PJ_SUCCESS;
	}

	/* Check if remote has reported ice-mismatch */
	if (pjmedia_sdp_attr_find(rem_m->attr_count, rem_m->attr, 
				  &STR_ICE_MISMATCH, NULL) != NULL)
	{
	    /* Remote has reported ice-mismatch */
	    set_no_ice(tp_ice, 
		       "Remote answer contains 'ice-mismatch' attribute", 
		       PJ_SUCCESS);
	    return PJ_SUCCESS;
	}

	/* Check if remote has indicated a restart */
	if (answer_state.ice_restart) {
	    PJ_LOG(2,(tp_ice->base.name, 
		      "Warning: remote has signalled ICE restart in SDP "
		      "answer which is disallowed. Remote ICE negotiation"
		      " may fail."));
	}

	/* Check if the answer itself is mismatched */
	if (answer_state.ice_mismatch) {
	    /* This happens either when a B2BUA modified remote answer but
	     * strangely didn't modify our offer, or remote is not capable
	     * of detecting mismatch in our offer (it didn't put 
	     * 'ice-mismatch' attribute in the answer).
	     */
	    PJ_LOG(2,(tp_ice->base.name, 
		      "Warning: remote answer mismatch, but it does not "
		      "reject our offer with 'ice-mismatch'. ICE negotiation "
		      "may fail"));
	}

	/* Do nothing if ICE is complete or running */
	if (pj_ice_strans_sess_is_running(tp_ice->ice_st)) {
	    PJ_LOG(4,(tp_ice->base.name,
		      "Ignored offer/answer because ICE is running"));
	    return PJ_SUCCESS;
	}

	if (pj_ice_strans_sess_is_complete(tp_ice->ice_st)) {
	    PJ_LOG(4,(tp_ice->base.name, "ICE session unchanged"));
	    return PJ_SUCCESS;
	}

	/* Start ICE */

    } else {
	/*
	 * We are answerer. We've seen and negotiated remote's SDP
	 * before, and the result is in "rem_offer_state".
	 */
	const pjmedia_sdp_attr *ufrag_attr, *pwd_attr;

	/* Check for ICE in remote offer */
	if (tp_ice->rem_offer_state.match_comp_cnt == 0) {
	    /* No ICE attribute present */
	    set_no_ice(tp_ice, "Remote no longer offers ICE",
		       PJ_SUCCESS);
	    return PJ_SUCCESS;
	}

	/* Check for ICE ice_mismatch condition in the offer */
	if (tp_ice->rem_offer_state.ice_mismatch) {
	    set_no_ice(tp_ice, "Remote offer mismatch: ", 
		       PJNATH_EICEMISMATCH);
	    return PJ_SUCCESS;
	}

	/* If ICE is complete and remote doesn't request restart,
	 * then leave the session as is.
	 */
	if (!initial_oa && tp_ice->rem_offer_state.ice_restart == PJ_FALSE) {
	    /* Remote has not requested ICE restart, so session is
	     * unchanged.
	     */
	    PJ_LOG(4,(tp_ice->base.name, "ICE session unchanged"));
	    return PJ_SUCCESS;
	}

	/* Either remote has requested ICE restart or this is our
	 * first answer. 
	 */

	/* Stop ICE */
	if (!initial_oa) {
	    set_no_ice(tp_ice, "restarting by remote request..", PJ_SUCCESS);

	    /* We have put new ICE ufrag and pwd in the answer. Now
	     * create a new ICE session with that ufrag/pwd pair.
	     */
	    get_ice_attr(sdp_local, sdp_local->media[media_index], 
			&ufrag_attr, &pwd_attr);

		// if it is UAS, retrieve remote turn attribute and save it to remo_turn config
		if ((tp_ice->base.use_turn_flag & TURN_FLAG_USE_UAC_TURN) > 0 && 
			tp_ice->rem_offer_state.local_role == PJ_ICE_SESS_ROLE_CONTROLLED)
			retrieve_remote_turn_attr(tp_ice, sdp_local->media[media_index], &sdp_local->inv_cid);

	    status = pj_ice_strans_init_ice(tp_ice->ice_st, 
					    tp_ice->rem_offer_state.local_role,
					    &ufrag_attr->value, 
					    &pwd_attr->value);
	    if (status != PJ_SUCCESS) {
		PJ_LOG(1,(tp_ice->base.name, 
			  "ICE re-initialization failed (status=%d)!",
			  status));
		return status;
	    }
	}

	/* Ticket #977: Update role if turns out we're supposed to be the 
	 * Controlling agent (e.g. when talking to ice-lite peer). 
	 */
	if (tp_ice->rem_offer_state.local_role==PJ_ICE_SESS_ROLE_CONTROLLING &&
	    pj_ice_strans_has_sess(tp_ice->ice_st)) 
	{
	    pj_ice_strans_change_role(tp_ice->ice_st, 
				      PJ_ICE_SESS_ROLE_CONTROLLING);
	}


	/* start ICE */
	}

    /* Now start ICE */
    status = start_ice(tp_ice, tmp_pool, rem_sdp, media_index);
    if (status != PJ_SUCCESS) {
	PJ_LOG(1,(tp_ice->base.name, 
		  "ICE restart failed (status=%d)!",
		  status));
	return status;
    }

    /* Done */
    tp_ice->use_ice = PJ_TRUE;

    return PJ_SUCCESS;
}


static pj_status_t transport_media_stop(pjmedia_transport *tp)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    
    set_no_ice(tp_ice, "media stop requested", PJ_SUCCESS);

    return PJ_SUCCESS;
}


static pj_status_t transport_get_info(pjmedia_transport *tp,
				      pjmedia_transport_info *info)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    pj_ice_sess_cand cand;
    pj_status_t status;

    pj_bzero(&info->sock_info, sizeof(info->sock_info));
    info->sock_info.rtp_sock = info->sock_info.rtcp_sock = PJ_INVALID_SOCKET;

    /* Get RTP default address */
    status = pj_ice_strans_get_def_cand(tp_ice->ice_st, 1, &cand);
    if (status != PJ_SUCCESS)
	return status;

	if (pj_sockaddr_has_addr(&cand.addr))
    	pj_sockaddr_cp(&info->sock_info.rtp_addr_name, &cand.addr);

    /* Get RTCP default address */
    if (tp_ice->comp_cnt > 1) {
	status = pj_ice_strans_get_def_cand(tp_ice->ice_st, 2, &cand);
	if (status != PJ_SUCCESS)
	    return status;

	if (pj_sockaddr_has_addr(&cand.addr))
		pj_sockaddr_cp(&info->sock_info.rtcp_addr_name, &cand.addr);
    }

    /* Set remote address originating RTP & RTCP if this transport has 
     * ICE activated or received any packets.
     */
    if (tp_ice->use_ice || tp_ice->rtp_src_cnt) {
	info->src_rtp_name  = tp_ice->rtp_src_addr;
    }
    if (tp_ice->use_ice || tp_ice->rtcp_src_cnt) {
	info->src_rtcp_name = tp_ice->rtcp_src_addr;
    }

    /* Fill up transport specific info */
    if (info->specific_info_cnt < PJ_ARRAY_SIZE(info->spc_info)) {
	pjmedia_transport_specific_info *tsi;
	pjmedia_ice_transport_info *ii;
	unsigned i;

	pj_assert(sizeof(*ii) <= sizeof(tsi->buffer));
	tsi = &info->spc_info[info->specific_info_cnt++];
	tsi->type = PJMEDIA_TRANSPORT_TYPE_ICE;
	tsi->cbsize = sizeof(*ii);

	ii = (pjmedia_ice_transport_info*) tsi->buffer;
	pj_bzero(ii, sizeof(*ii));

	if (pj_ice_strans_has_sess(tp_ice->ice_st))
	    ii->role = pj_ice_strans_get_role(tp_ice->ice_st);
	else
	    ii->role = PJ_ICE_SESS_ROLE_UNKNOWN;
	ii->sess_state = pj_ice_strans_get_state(tp_ice->ice_st);
	ii->comp_cnt = pj_ice_strans_get_running_comp_cnt(tp_ice->ice_st);
	
	for (i=1; i<=ii->comp_cnt && i<=PJ_ARRAY_SIZE(ii->comp); ++i) {
	    const pj_ice_sess_check *chk;

	    chk = pj_ice_strans_get_valid_pair(tp_ice->ice_st, i);
	    if (chk) {
		ii->comp[i-1].lcand_type = chk->lcand->type;
		ii->comp[i-1].rcand_type = chk->rcand->type;
	    }
	}
    }

    return PJ_SUCCESS;
}


static pj_status_t transport_attach  (pjmedia_transport *tp,
				      void *stream,
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
    struct transport_ice *tp_ice = (struct transport_ice*)tp;

    tp_ice->stream = stream;
    tp_ice->rtp_cb = rtp_cb;
    tp_ice->rtcp_cb = rtcp_cb;

    pj_memcpy(&tp_ice->remote_rtp, rem_addr, addr_len);
    pj_memcpy(&tp_ice->remote_rtcp, rem_rtcp, addr_len);
    tp_ice->addr_len = addr_len;

    /* Init source RTP & RTCP addresses and counter */
    tp_ice->rtp_src_addr = tp_ice->remote_rtp;
    tp_ice->rtcp_src_addr = tp_ice->remote_rtcp;
    tp_ice->rtp_src_cnt = 0;
    tp_ice->rtcp_src_cnt = 0;

    return PJ_SUCCESS;
}


static void transport_detach(pjmedia_transport *tp,
			     void *strm)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;

    /* TODO: need to solve ticket #460 here */

    tp_ice->rtp_cb = NULL;
    tp_ice->rtcp_cb = NULL;
    tp_ice->stream = NULL;

    PJ_UNUSED_ARG(strm);
}


static pj_status_t transport_send_rtp(pjmedia_transport *tp,
				      const void *pkt,
				      pj_size_t size)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;

    /* Simulate packet lost on TX direction */
    if (tp_ice->tx_drop_pct) {
	if ((pj_rand() % 100) <= (int)tp_ice->tx_drop_pct) {
	    PJ_LOG(5,(tp_ice->base.name, 
		      "TX RTP packet dropped because of pkt lost "
		      "simulation"));
	    return PJ_SUCCESS;
	}
    }

    return pj_ice_strans_sendto(tp_ice->ice_st, 1, 
			        pkt, size, &tp_ice->remote_rtp,
				tp_ice->addr_len);
}


static pj_status_t transport_send_rtcp(pjmedia_transport *tp,
				       const void *pkt,
				       pj_size_t size)
{
    return transport_send_rtcp2(tp, NULL, 0, pkt, size);
}

static pj_status_t transport_send_rtcp2(pjmedia_transport *tp,
				        const pj_sockaddr_t *addr,
				        unsigned addr_len,
				        const void *pkt,
				        pj_size_t size)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;
    if (tp_ice->comp_cnt > 1) {
	if (addr == NULL) {
		addr = &tp_ice->remote_rtcp;
	    addr_len = pj_sockaddr_get_len(addr);
	}
	return pj_ice_strans_sendto(tp_ice->ice_st, 2, pkt, size, 
				    addr, addr_len);
    } else {
	return PJ_SUCCESS;
    }
}


static void ice_on_rx_data(pj_ice_strans *ice_st, unsigned comp_id, 
			   void *pkt, pj_size_t size,
			   const pj_sockaddr_t *src_addr,
			   unsigned src_addr_len)
{
    struct transport_ice *tp_ice;
    pj_bool_t discard = PJ_FALSE;

    tp_ice = (struct transport_ice*) pj_ice_strans_get_user_data(ice_st);

    if (comp_id==1 && tp_ice->rtp_cb) {

	/* Simulate packet lost on RX direction */
	if (tp_ice->rx_drop_pct) {
	    if ((pj_rand() % 100) <= (int)tp_ice->rx_drop_pct) {
		PJ_LOG(5,(tp_ice->base.name, 
			  "RX RTP packet dropped because of pkt lost "
			  "simulation"));
		return;
	    }
	}

	/* See if source address of RTP packet is different than the 
	 * configured address, and switch RTP remote address to 
	 * source packet address after several consecutive packets
	 * have been received.
	 */
	if (!tp_ice->use_ice) {
	    pj_bool_t enable_switch =
		    ((tp_ice->options & PJMEDIA_ICE_NO_SRC_ADDR_CHECKING)==0);

	    if (!enable_switch ||
		pj_sockaddr_cmp(&tp_ice->remote_rtp, src_addr) == 0)
	    {
		/* Don't switch while we're receiving from remote_rtp */
		tp_ice->rtp_src_cnt = 0;
	    } else {

		++tp_ice->rtp_src_cnt;

		/* Check if the source address is recognized. */
		if (pj_sockaddr_cmp(src_addr, &tp_ice->rtp_src_addr) != 0) {
			/* Remember the new source address. */
		    pj_sockaddr_cp(&tp_ice->rtp_src_addr, src_addr);
		    /* Reset counter */
		    tp_ice->rtp_src_cnt = 0;
		    discard = PJ_TRUE;
		}

		if (tp_ice->rtp_src_cnt < PJMEDIA_RTP_NAT_PROBATION_CNT) {
		    discard = PJ_TRUE;
		} else {
		    char addr_text[80];

			/* Set remote RTP address to source address */
			pj_sockaddr_cp(&tp_ice->remote_rtp, &tp_ice->rtp_src_addr);
		    tp_ice->addr_len = pj_sockaddr_get_len(&tp_ice->remote_rtp);

		    /* Reset counter */
		    tp_ice->rtp_src_cnt = 0;

		    PJ_LOG(4,(tp_ice->base.name,
			      "Remote RTP address switched to %s",
			      pj_sockaddr_print(&tp_ice->remote_rtp, addr_text,
						sizeof(addr_text), 3)));

		    /* Also update remote RTCP address if actual RTCP source
		     * address is not heard yet.
		     */
		    if (!pj_sockaddr_has_addr(&tp_ice->rtcp_src_addr)) {
			pj_uint16_t port;

			pj_sockaddr_cp(&tp_ice->remote_rtcp, 
				       &tp_ice->remote_rtp);

			port = (pj_uint16_t)
			       (pj_sockaddr_get_port(&tp_ice->remote_rtp)+1);
			pj_sockaddr_set_port(&tp_ice->remote_rtcp, port);

			PJ_LOG(4,(tp_ice->base.name,
				  "Remote RTCP address switched to predicted "
				  "address %s",
				  pj_sockaddr_print(&tp_ice->remote_rtcp, 
						    addr_text,
						    sizeof(addr_text), 3)));
		    }
		}
	    }
	}

	if (!discard)
	    (*tp_ice->rtp_cb)(tp_ice->stream, pkt, size);

    } else if (comp_id==2 && tp_ice->rtcp_cb) {

	/* Check if RTCP source address is the same as the configured
	 * remote address, and switch the address when they are
	 * different.
	 */
	if (!tp_ice->use_ice &&
	    (tp_ice->options & PJMEDIA_ICE_NO_SRC_ADDR_CHECKING)==0)
	{
	    if (pj_sockaddr_cmp(&tp_ice->remote_rtcp, src_addr) == 0) {
		tp_ice->rtcp_src_cnt = 0;
	    } else {
		char addr_text[80];

		++tp_ice->rtcp_src_cnt;
		if (tp_ice->rtcp_src_cnt < PJMEDIA_RTCP_NAT_PROBATION_CNT) {
		    discard = PJ_TRUE;
		} else {
			tp_ice->rtcp_src_cnt = 0;
		    pj_sockaddr_cp(&tp_ice->rtcp_src_addr, src_addr);
		    pj_sockaddr_cp(&tp_ice->remote_rtcp, src_addr);

		    pj_assert(tp_ice->addr_len==pj_sockaddr_get_len(src_addr));

		    PJ_LOG(4,(tp_ice->base.name,
			      "Remote RTCP address switched to %s",
			      pj_sockaddr_print(&tp_ice->remote_rtcp,
			                        addr_text, sizeof(addr_text),
			                        3)));
		}
	    }
	}

	if (!discard)
		(*tp_ice->rtcp_cb)(tp_ice->stream, pkt, size);
    }

    PJ_UNUSED_ARG(src_addr_len);
}


static void ice_on_ice_complete(pj_ice_strans *ice_st, 
				pj_ice_strans_op op,
				pj_status_t result,
				pj_sockaddr *turn_mapped_addr)
{
    struct transport_ice *tp_ice;

    tp_ice = (struct transport_ice*) pj_ice_strans_get_user_data(ice_st);

	tp_ice->base.tunnel_type = pj_ice_strans_get_use_tunnel_type(ice_st);

    /* Notify application */
    if (tp_ice->cb.on_ice_complete)
		(*tp_ice->cb.on_ice_complete)(&tp_ice->base, op, result, turn_mapped_addr);
}

/* TURN allocated */
static void ice_on_turn_allocated(pj_ice_strans *ice_st, pj_sockaddr_t *turn_srv)
{
	struct transport_ice *tp_ice;
	char turn_addr[PJ_INET6_ADDRSTRLEN] = {0};

	tp_ice = (struct transport_ice*) pj_ice_strans_get_user_data(ice_st);

	pj_sockaddr_print(turn_srv, turn_addr, sizeof(turn_addr), 3);

	memset(tp_ice->base.local_turnsvr, 0, sizeof(tp_ice->base.local_turnsvr));
	memcpy(tp_ice->base.local_turnsvr, turn_addr, strlen(turn_addr));

}

/* STUN binding complete */
static pj_status_t ice_on_stun_binding_complete(pj_ice_strans *ice_st,
										 pj_sockaddr *local_addr,  
										 int ip_chagned_type)
{
	struct transport_ice *tp_ice;

	tp_ice = (struct transport_ice*) pj_ice_strans_get_user_data(ice_st);
	
	if (tp_ice->cb.on_stun_binding_complete)
		(*tp_ice->cb.on_stun_binding_complete)(&tp_ice->base, 
							local_addr, ip_chagned_type);

}

static pj_status_t ice_on_tcp_server_binding_complete(pj_ice_strans *ice_st,
											 pj_sockaddr *external_addr,
											 pj_sockaddr *local_addr)
{
	struct transport_ice *tp_ice;

	tp_ice = (struct transport_ice*) pj_ice_strans_get_user_data(ice_st);
	
	if (tp_ice->cb.on_tcp_server_binding_complete)
		return (*tp_ice->cb.on_tcp_server_binding_complete)(&tp_ice->base, 
									external_addr, local_addr);

	return PJ_SUCCESS;

}


/* Simulate lost */
static pj_status_t transport_simulate_lost(pjmedia_transport *tp,
					   pjmedia_dir dir,
					   unsigned pct_lost)
{
    struct transport_ice *ice = (struct transport_ice*) tp;

    PJ_ASSERT_RETURN(tp && pct_lost <= 100, PJ_EINVAL);

    if (dir & PJMEDIA_DIR_ENCODING)
	ice->tx_drop_pct = pct_lost;

    if (dir & PJMEDIA_DIR_DECODING)
	ice->rx_drop_pct = pct_lost;

    return PJ_SUCCESS;
}


/*
 * Destroy ICE media transport.
 */
static pj_status_t transport_destroy(pjmedia_transport *tp)
{
    struct transport_ice *tp_ice = (struct transport_ice*)tp;

    if (tp_ice->ice_st) {
	pj_ice_strans_destroy(tp_ice->ice_st);
	tp_ice->ice_st = NULL;
    }

    if (tp_ice->pool) {
	pj_pool_t *pool = tp_ice->pool;
	tp_ice->pool = NULL;
	pj_pool_release(pool);
    }

    return PJ_SUCCESS;
}

/* Dean Added */
PJ_DEF(pj_bool_t) pjmedia_ice_get_ice_restart(void *user_data)
{
	struct transport_ice *ice_tp = (struct transport_ice *)user_data;

	return ice_tp->rem_offer_state.ice_restart;
}

/* Dean Added */
PJ_DEF(void) pjmedia_ice_set_ice_restart(void *user_data, pj_bool_t ice_restart)
{
	struct transport_ice *ice_tp = (struct transport_ice *)user_data;

	ice_tp->rem_offer_state.ice_restart = ice_restart;;
}

PJ_DEF(natnl_tunnel_type) pjmedia_ice_get_use_tunnel_type(void *user_data)
{
	struct transport_ice *ice_tp = (struct transport_ice *)user_data;

	return pj_ice_strans_get_use_tunnel_type(ice_tp->ice_st);
}

PJ_DEF(void) pjmedia_ice_set_turn_password(void *user_data, pj_str_t turn_password)
{
	struct transport_ice *tp_ice = (struct transport_ice *)user_data;
	pj_ice_strans_set_turn_password(tp_ice->ice_st, turn_password);
}

#if 0
PJ_DEF(void) pjmedia_ice_set_use_upnp_flag(void *user_data, int use_upnp_flag)
{
	struct transport_ice *tp_ice = (struct transport_ice *)user_data;

	PJ_LOG(4,(tp_ice->base.name, 
		"tp_ice=%p, tp_ice->use_upnp_flag=%d", tp_ice, use_upnp_flag));

	tp_ice->base.use_upnp_flag = use_upnp_flag;
}

PJ_DEF(void) pjmedia_ice_set_use_stun_cand(void *user_data, pj_bool_t use_stun_cand)
{
	struct transport_ice *tp_ice = (struct transport_ice *)user_data;

	PJ_LOG(4,(tp_ice->base.name, 
		"tp_ice=%p, tp_ice->use_stun_cand=%d", tp_ice, use_stun_cand));

	 tp_ice->base.use_stun_cand = use_stun_cand;
}

PJ_DEF(void) pjmedia_ice_set_use_turn_flag(void *user_data, int use_turn_flag)
{
	struct transport_ice *tp_ice = (struct transport_ice *)user_data;

	PJ_LOG(4,(tp_ice->base.name, 
		"tp_ice=%p, tp_ice->use_turn_flag=%d", tp_ice, use_turn_flag));

	tp_ice->base.use_turn_flag = use_turn_flag;
}

//====== local and remote user id ======

/* Dean Added */
PJ_DEF(void) pjmedia_ice_get_local_userid(void *user_data, char *user_id)
{
	if(user_id) {
		struct transport_ice *tp_ice = (struct transport_ice *)user_data;
		strncpy(user_id, tp_ice->base.local_userid, sizeof(tp_ice->base.local_userid));
	}
}

/* Dean Added */
PJ_DEF(void) pjmedia_ice_set_local_userid(void *user_data, char *user_id, int user_id_len)
{
	struct transport_ice *tp_ice = (struct transport_ice *)user_data;
	if(tp_ice->base.local_userid) {
		strncpy(tp_ice->base.local_userid, user_id, user_id_len);
	}
}

/* Dean Added */
PJ_DEF(void) pjmedia_ice_set_remote_userid(void *user_data, char *user_id, int user_id_len)
{
	struct transport_ice *tp_ice = (struct transport_ice *)user_data;
	if(tp_ice->base.remote_userid) {
		strncpy(tp_ice->base.remote_userid, user_id, user_id_len);
	}
}

/* Dean Added */
PJ_DEF(void) pjmedia_ice_get_remote_userid(void *user_data, char *user_id)
{
	if(user_id) {
		struct transport_ice *tp_ice = (struct transport_ice *)user_data;
		strncpy(user_id, tp_ice->base.remote_userid, sizeof(tp_ice->base.remote_userid));
	}
}

//====== local and remote device id ======

/* Dean Added */
PJ_DEF(void) pjmedia_ice_get_local_deviceid(void *user_data, char *device_id)
{
	if(device_id) {
		struct transport_ice *tp_ice = (struct transport_ice *)user_data;
		strncpy(device_id, tp_ice->base.local_deviceid, sizeof(tp_ice->base.local_deviceid));
	}
}

/* Dean Added */
PJ_DEF(void) pjmedia_ice_set_local_deviceid(void *user_data, char *device_id, int device_id_len)
{
	struct transport_ice *tp_ice = (struct transport_ice *)user_data;
	if(tp_ice->base.local_deviceid) {
		strncpy(tp_ice->base.local_deviceid, device_id, device_id_len);
	}
}

/* Dean Added */
PJ_DEF(void) pjmedia_ice_set_remote_deviceid(void *user_data, char *device_id, int user_id_len)
{
	struct transport_ice *tp_ice = (struct transport_ice *)user_data;
	if(tp_ice->base.remote_deviceid) {
		strncpy(tp_ice->base.remote_deviceid, device_id, user_id_len);
	}
}

/* Dean Added */
PJ_DEF(void) pjmedia_ice_get_remote_deviceid(void *user_data, char *device_id)
{
	if(device_id) {
		struct transport_ice *tp_ice = (struct transport_ice *)user_data;
		strncpy(device_id, tp_ice->base.remote_deviceid, sizeof(tp_ice->base.remote_deviceid));
	}
}

//====== local and remote turn server ======

/* Dean Added */
PJ_DEF(void) pjmedia_ice_get_local_turnsrv(void *user_data, char *turn_server)
{
	if(turn_server) {
		struct transport_ice *tp_ice = (struct transport_ice *)user_data;
		strncpy(turn_server, tp_ice->base.local_turnsvr, sizeof(tp_ice->base.local_turnsvr));
	}
}

/* Dean Added */
PJ_DEF(void) pjmedia_ice_set_local_turnsrv(void *user_data, char *turn_server, int turn_server_len)
{
	struct transport_ice *tp_ice = (struct transport_ice *)user_data;
	if(tp_ice->base.local_turnsvr) {
		strncpy(tp_ice->base.local_turnsvr, turn_server, turn_server_len);
	}
}

/* Dean Added */
PJ_DEF(void) pjmedia_ice_set_remote_turnsvr(void *user_data, char *turn_server, int turn_server_len)
{
	struct transport_ice *tp_ice = (struct transport_ice *)user_data;
	if(tp_ice->base.remote_turnsvr) {
		strncpy(tp_ice->base.remote_turnsvr, turn_server, turn_server_len);
	}
}

/* Dean Added */
PJ_DEF(void) pjmedia_ice_get_remote_turnsvr(void *user_data, char *turn_server)
{
	if(turn_server) {
		struct transport_ice *tp_ice = (struct transport_ice *)user_data;
		strncpy(turn_server, tp_ice->base.remote_turnsvr, sizeof(tp_ice->base.remote_turnsvr));
	}
}

//====== local and remote turn password ======

/* Dean Added */
PJ_DEF(void) pjmedia_ice_get_local_turnpwd(void *user_data, char *turn_pwd)
{
	if(turn_pwd) {
		struct transport_ice *tp_ice = (struct transport_ice *)user_data;
		strncpy(turn_pwd, tp_ice->base.local_turnpwd, sizeof(tp_ice->base.local_turnpwd));
	}
}

/* Dean Added */
PJ_DEF(void) pjmedia_ice_set_local_turnpwd(void *user_data, char *turn_pwd, int turn_pwd_len)
{
	struct transport_ice *tp_ice = (struct transport_ice *)user_data;
	if(tp_ice->base.local_turnpwd) {
		strncpy(tp_ice->base.local_turnpwd, turn_pwd, turn_pwd_len);
	}
}

/* Dean Added */
PJ_DEF(void) pjmedia_ice_set_remote_turnpwd(void *user_data, char *turn_pwd, int turn_pwd_len)
{
	struct transport_ice *tp_ice = (struct transport_ice *)user_data;
	if(tp_ice->base.remote_turnpwd) {
		strncpy(tp_ice->base.remote_turnpwd, turn_pwd, turn_pwd_len);
	}
}

/* Dean Added */
PJ_DEF(void) pjmedia_ice_get_remote_turnpwd(void *user_data, char *turn_pwd)
{
	if(turn_pwd) {
		struct transport_ice *tp_ice = (struct transport_ice *)user_data;
		strncpy(turn_pwd, tp_ice->base.remote_turnpwd, sizeof(tp_ice->base.remote_turnpwd));
	}
}
#endif

PJ_DEF(pj_bool_t) pjmedia_ice_get_local_path_selected(void *user_data)
{
	struct transport_ice *tp_ice = (struct transport_ice *)user_data;
	return pj_ice_strans_get_local_path_selected(tp_ice->ice_st);
}

PJ_DEF(natnl_addr_changed_type) pjmedia_ice_get_addr_chagned_type(void *user_data)
{
	struct transport_ice *tp_ice = (struct transport_ice *)user_data;
	return pj_ice_strans_get_addr_changed_type(tp_ice->ice_st);
}

#if 0
PJ_DEF(pj_str_t *) pjmedia_ice_get_dest_uri(void *user_data)
{
	struct transport_ice *tp_ice = (struct transport_ice *)user_data;
	
	return pj_ice_strans_get_dest_uri(tp_ice->ice_st);
}
#endif
