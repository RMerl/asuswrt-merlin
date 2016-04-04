/* $Id: ice_strans.c 3596 2011-06-22 10:57:11Z bennylp $ */
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
#include <pjnath/natnl_tnl_cache.h>
#include <pjnath/ice_strans.h>
#include <pjnath/errno.h>
#include <pj/addr_resolv.h>
#include <pj/array.h>
#include <pj/assert.h>
#include <pj/ip_helper.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/rand.h>
#include <pj/string.h>
#include <pj/compat/socket.h>

#define THIS_FILE "ice_strans.c"


#if 0
#  define TRACE_PKT(expr)	    PJ_LOG(5,expr)
#else
#  define TRACE_PKT(expr)
#endif

#define LOG4(expr)		PJ_LOG(4,expr)
#define LOG5(expr)		PJ_LOG(4,expr)


/* Transport IDs */
enum tp_type
{
    TP_NONE,
    TP_STUN,
    TP_TURN,
	TP_UPNP_TCP,
	TP_TURN_TCP
};

/* Candidate's local preference values. This is mostly used to
 * specify preference among candidates with the same type. Since
 * we don't have the facility to specify that, we'll just set it
 * all to the same value.
 */
#if PJNATH_ICE_PRIO_STD
#   define SRFLX_PREF  32768
//#   define HOST_PREF   32768
#   define HOST_PREF    40960
#   define RELAY_PREF  16384
#   define UPNP_TCP_PREF  65535
#else
#   define SRFLX_PREF  0
#   define HOST_PREF   0
#   define RELAY_PREF  0
#endif


/* The candidate type preference when STUN candidate is used */
static pj_uint8_t srflx_pref_table[6] = 
{
#if PJNATH_ICE_PRIO_STD
    100,    /**< PJ_ICE_HOST_PREF	    */
    110,    /**< PJ_ICE_SRFLX_PREF	    */
	115,//126,    /**< PJ_ICE_PRFLX_PREF	    */
	0,	    /**< PJ_ICE_RELAYED_PREF    */   //natnl
	126,	/**< PJ_ICE_HOST_TCP_PREF    */      //nantl
	120, //126,	    /**< PJ_ICE_SRFLX_TCP_PREF    */ //natnl
#else
    /* Keep it to 2 bits */
    1,	/**< PJ_ICE_HOST_PREF	    */
    2,	/**< PJ_ICE_SRFLX_PREF	    */
    3,	/**< PJ_ICE_PRFLX_PREF	    */
	0	/**< PJ_ICE_RELAYED_PREF    */
	3,	/**< PJ_ICE_HOST_TCP_PREF    */
	3	/**< PJ_ICE_SRFLX_TCP_PREF    */
#endif
};


/* The data that will be attached to the n to wait
 * tcp sesssion ready.
 */
typedef struct tcp_sess_timer_data
{
	pj_ice_strans	*ice_st;
	int				count;
	int				timeout_count;
} tcp_sess_timer_data;


/* ICE callbacks */
static void	   on_ice_complete(pj_ice_sess *ice, pj_status_t status);
static pj_status_t ice_tx_pkt(pj_ice_sess *ice, 
							  unsigned comp_id,
							  unsigned transport_id,
							  const void *pkt, pj_size_t size,
							  const pj_sockaddr_t *dst_addr,
							  unsigned dst_addr_len,
							  int tcp_sess_idx);
static pj_status_t ice_tcp_tx_pkt(pj_ice_sess *ice, 
							  unsigned comp_id,
							  unsigned transport_id,
							  const void *pkt, pj_size_t size,
							  const pj_sockaddr_t *dst_addr,
							  unsigned dst_addr_len,
							  void *user_data,
							  int tcp_sess_idx);
static void	   ice_rx_data(pj_ice_sess *ice, 
			       unsigned comp_id, 
			       unsigned transport_id,
			       void *pkt, pj_size_t size,
			       const pj_sockaddr_t *src_addr,
			       unsigned src_addr_len);


/* STUN socket callbacks */
/* Notification when incoming packet has been received. */
static pj_bool_t stun_on_rx_data(pj_stun_sock *stun_sock,
				 void *pkt,
				 unsigned pkt_len,
				 const pj_sockaddr_t *src_addr,
				 unsigned addr_len);
/* Notifification when asynchronous send operation has completed. */
static pj_bool_t stun_on_data_sent(pj_stun_sock *stun_sock,
				   pj_ioqueue_op_key_t *send_key,
				   pj_ssize_t sent);
/* Notification when the status of the STUN transport has changed. */
static pj_bool_t stun_on_status(pj_stun_sock *stun_sock,
				pj_stun_sock_op op,
				pj_status_t status);


/* TURN callbacks */
static void turn_on_rx_data(pj_turn_sock *turn_sock,
			    void *pkt,
			    unsigned pkt_len,
			    const pj_sockaddr_t *peer_addr,
			    unsigned addr_len);
static void turn_on_state(pj_turn_sock *turn_sock, pj_turn_state_t old_state,
			  pj_turn_state_t new_state);
static void turn_on_allocated(pj_turn_sock *turn_sock, 
							  pj_sockaddr_t *turn_srv);


/* UPnP TCP callbacks */
static void tcp_on_rx_data(void *sess,
			    void *pkt,
			    unsigned pkt_len,
			    const pj_sockaddr_t *peer_addr,
			    unsigned addr_len);
static void tcp_on_state(pj_tcp_sock *tcp_sock, pj_tcp_state_t old_state,
			  pj_tcp_state_t new_state);

static pj_status_t tcp_on_server_binding_complete(pj_tcp_sock *tcp_sock,
										 pj_sockaddr *external_addr,
										 pj_sockaddr *local_addr);



/* Forward decls */
static void ice_st_on_destroy(void *obj);
static void destroy_ice_st(pj_ice_strans *ice_st);
#define ice_st_perror(ice_st,msg,rc) pjnath_perror(ice_st->obj_name,msg,rc)
static void sess_init_update(pj_ice_strans *ice_st);

/**
 * This structure describes an ICE stream transport component. A component
 * in ICE stream transport typically corresponds to a single socket created
 * for this component, and bound to a specific transport address. This
 * component may have multiple alias addresses, for example one alias
 * address for each interfaces in multi-homed host, another for server
 * reflexive alias, and another for relayed alias. For each transport
 * address alias, an ICE stream transport candidate (#pj_ice_sess_cand) will
 * be created, and these candidates will eventually registered to the ICE
 * session.
 */
typedef struct pj_ice_strans_comp
{
    pj_ice_strans	*ice_st;	/**< ICE stream transport.	*/
    unsigned		 comp_id;	/**< Component ID.		*/

	pj_stun_sock	*stun_sock;	/**< STUN transport.		*/
	pj_turn_sock	*turn_sock;	/**< TURN relay transport.	*/
	pj_turn_sock	*turn_tcp_sock;	/**< TURN relay transport.	*/
    pj_bool_t		 turn_log_off;	/**< TURN loggin off?		*/
    unsigned		 turn_err_cnt;	/**< TURN disconnected count.	*/

    pj_tcp_sock		*tcp_sock;	/**< TCP transport.	*/
    pj_sockaddr           tcp_addr;
    pj_sockaddr           tcp_base_addr;
    pj_sockaddr           tcp_rel_addr;

    unsigned		 cand_cnt;	/**< # of candidates/aliaes.	*/
    pj_ice_sess_cand	 cand_list[PJ_ICE_ST_MAX_CAND];	/**< Cand array	*/

    unsigned		 default_cand;	/**< Default candidate.		*/

} pj_ice_strans_comp;


/**
 * This structure represents the ICE stream transport.
 */
struct pj_ice_strans
{
    char		    *obj_name;	/**< Log ID.			*/
    pj_pool_t		    *pool;	/**< Pool used by this object.	*/
    void		    *user_data;	/**< Application data.		*/
    pj_ice_strans_cfg	     cfg;	/**< Configuration.		*/
    pj_ice_strans_cb	     cb;	/**< Application callback.	*/
    pj_grp_lock_t	    *grp_lock;  /**< Group lock.		*/

    pj_ice_strans_state	     state;	/**< Session state.		*/
    pj_ice_sess		    *ice;	/**< ICE session.		*/
    pj_time_val		     start_time;/**< Time when ICE was started	*/

    unsigned		     comp_cnt;	/**< Number of components.	*/
    pj_ice_strans_comp	   **comp;	/**< Components array.		*/

    pj_timer_entry	     ka_timer;	/**< STUN keep-alive timer.	*/

    pj_bool_t		     destroy_req;/**< Destroy has been called?	*/
    pj_bool_t		     cb_called;	/**< Init error callback called?*/
	pj_bool_t		     ka_cb_called;	/**< Init error callback called?*/
	//pj_bool_t		     use_upnp_tcp;	/**< Use UPnP TCP?*/
	natnl_tunnel_type    tunnel_type;  /**< Used tunnel type*/

    /**
     * A timer used to wait tcp session ready.
     */
	pj_timer_entry	     tcp_sess_timer;
	int					 use_upnp_flag;
	pj_bool_t			 use_stun_cand;
	int					 use_turn_flag;
	int user_port_count;  // user port count;
	struct {
		char local_data[6];    // the user agent's local data port
		char external_data[6]; // the external port for the incoming packet to local data port
		char local_ctl[6];     // the user agent's local control port
		char external_ctl[6];  // the external control port for the incoming packet to local port
	} user_port; // user selected port pair
	int wait_turn_alloc_ok_cnt;
	pj_ice_sess_role	 ice_role;
	pj_mutex_t          *app_lock;

	/* tunnel timeout value in second */
	int tnl_timeout_msec;
	natnl_addr_changed_type addr_changed_type;          // 0: addr isn't changed, 1: mapped addr is changed, 2: local addr is changed.

	// 2013-10-17 DEAN, for tunnel cache
	pj_str_t dest_uri;
};


/* Validate configuration */
static pj_status_t pj_ice_strans_cfg_check_valid(const pj_ice_strans_cfg *cfg)
{
    pj_status_t status;

    status = pj_stun_config_check_valid(&cfg->stun_cfg);
    if (!status)
	return status;

    return PJ_SUCCESS;
}


/*
 * Initialize ICE transport configuration with default values.
 */
PJ_DEF(void) pj_ice_strans_cfg_default(pj_ice_strans_cfg *cfg)
{
    pj_bzero(cfg, sizeof(*cfg));

    pj_stun_config_init(0, &cfg->stun_cfg, NULL, 0, NULL, NULL);
    pj_stun_sock_cfg_default(&cfg->stun.cfg);
    pj_turn_alloc_param_default(&cfg->turn.alloc_param);
    pj_turn_sock_cfg_default(&cfg->turn.cfg);

    pj_ice_sess_options_default(&cfg->opt);

    cfg->af = pj_AF_INET();
    cfg->stun.port = PJ_STUN_PORT;
    cfg->turn.conn_type = PJ_TURN_TP_UDP;

    cfg->stun.max_host_cands = 64;

	cfg->tnl_timeout_msec = MAX_TNL_TIMEOUT_SEC * 1000;
}


/*
 * Copy configuration.
 */
PJ_DEF(void) pj_ice_strans_cfg_copy( pj_pool_t *pool,
				     pj_ice_strans_cfg *dst,
				     const pj_ice_strans_cfg *src)
{
	int i;

    pj_memcpy(dst, src, sizeof(*src));

    if (src->stun.server.slen)
	pj_strdup(pool, &dst->stun.server, &src->stun.server);
    if (src->turn.server.slen)
	pj_strdup(pool, &dst->turn.server, &src->turn.server);
    pj_stun_auth_cred_dup(pool, &dst->turn.auth_cred,
			  &src->turn.auth_cred);

	dst->user_port_count = src->user_port_count;
	for (i=0; i<dst->user_port_count; i++)
	{
		dst->user_ports[i].user_port_assigned = 
			src->user_ports[i].user_port_assigned;
		if (dst->user_ports[i].user_port_assigned) {
			dst->user_ports[i].local_tcp_data_port = 
				src->user_ports[i].local_tcp_data_port;
			dst->user_ports[i].external_tcp_data_port = 
				src->user_ports[i].external_tcp_data_port;
			dst->user_ports[i].local_tcp_ctl_port = 
				src->user_ports[i].local_tcp_ctl_port;
			dst->user_ports[i].external_tcp_ctl_port = 
				src->user_ports[i].external_tcp_ctl_port;
		}
	}
	
}


/*
 * Add or update TURN candidate.
 */
static pj_status_t add_update_turn(pj_ice_strans *ice_st,
								   pj_ice_sess_role role,
				   pj_ice_strans_comp *comp)
{
    pj_turn_sock_cb turn_sock_cb;
    pj_ice_sess_cand *cand = NULL;
    unsigned i;
    pj_status_t status;
	pj_bool_t use_remote_turn = PJ_FALSE;

    /* Find relayed candidate in the component */
    for (i=0; i<comp->cand_cnt; ++i) {
	if (comp->cand_list[i].type == PJ_ICE_CAND_TYPE_RELAYED ||
		comp->cand_list[i].type == PJ_ICE_CAND_TYPE_RELAYED_TCP) {
	    cand = &comp->cand_list[i];
	    break;
	}
    }

    /* If candidate is found, invalidate it first */
    if (cand) {
	cand->status = PJ_EPENDING;

	/* Also if this component's default candidate is set to relay,
	 * move it temporarily to something else.
	 */
	if ((int)comp->default_cand == cand - comp->cand_list) {
	    /* Init to something */
	    comp->default_cand = 0;
	    /* Use srflx candidate as the default, if any */
	    for (i=0; i<comp->cand_cnt; ++i) {
		if (comp->cand_list[i].type == PJ_ICE_CAND_TYPE_RELAYED ||
			comp->cand_list[i].type == PJ_ICE_CAND_TYPE_RELAYED_TCP) {
		    comp->default_cand = i;
		    break;
		}
	    }
	}
    }

    /* Init TURN socket */
    pj_bzero(&turn_sock_cb, sizeof(turn_sock_cb));
    turn_sock_cb.on_rx_data = &turn_on_rx_data;
	turn_sock_cb.on_state = &turn_on_state;
	// DEAN Added 2013-03-19
	turn_sock_cb.on_turn_srv_allocated = &turn_on_allocated;

    /* Override with component specific QoS settings, if any */
    if (ice_st->cfg.comp[comp->comp_id-1].qos_type) {
	ice_st->cfg.turn.cfg.qos_type =
	    ice_st->cfg.comp[comp->comp_id-1].qos_type;
    }
    if (ice_st->cfg.comp[comp->comp_id-1].qos_params.flags) {
	pj_memcpy(&ice_st->cfg.turn.cfg.qos_params,
		  &ice_st->cfg.comp[comp->comp_id-1].qos_params,
		  sizeof(ice_st->cfg.turn.cfg.qos_params));
    }
#if 1
    /* Create the TURN UDP transport */
    status = pj_turn_sock_create(&ice_st->cfg.stun_cfg, ice_st->cfg.af,
				 PJ_TURN_TP_UDP,
				 &turn_sock_cb, &ice_st->cfg.turn.cfg,
				 comp, &comp->turn_sock);
    if (status != PJ_SUCCESS) {
	return status;
    }
#endif
#if 1
    /* Create the TURN TCP transport */
    status = pj_turn_sock_create(&ice_st->cfg.stun_cfg, ice_st->cfg.af,
				 PJ_TURN_TP_TCP,
				 &turn_sock_cb, &ice_st->cfg.turn.cfg,
				 comp, &comp->turn_tcp_sock);
    if (status != PJ_SUCCESS) {
	return status;
    }
#endif
    /* Add pending job */
	///sess_add_ref(ice_st);

	// if it is UAS, use try use remote turn config
	if (role == PJ_ICE_SESS_ROLE_CONTROLLED &&
		(ice_st->use_turn_flag & TURN_FLAG_USE_UAC_TURN) > 0 &&
		ice_st->cfg.rem_turn.server.slen)
		use_remote_turn = PJ_TRUE;

	/* Start allocation */
	if (use_remote_turn) {
		if ((ice_st->use_turn_flag & TURN_FLAG_USE_TCP_TURN) == 0)
		{
			status=pj_turn_sock_alloc(comp->turn_sock,
				&ice_st->cfg.rem_turn.server,
				ice_st->cfg.rem_turn.port,
				ice_st->cfg.resolver,
				&ice_st->cfg.rem_turn.auth_cred,
				&ice_st->cfg.rem_turn.alloc_param);
		}

		status=pj_turn_sock_alloc(comp->turn_tcp_sock,
			&ice_st->cfg.rem_turn.server,
			ice_st->cfg.rem_turn.port,
			ice_st->cfg.resolver,
			&ice_st->cfg.rem_turn.auth_cred,
			&ice_st->cfg.rem_turn.alloc_param);
	} else {
		if ((ice_st->use_turn_flag & TURN_FLAG_USE_TCP_TURN) == 0)
		{
			status=pj_turn_sock_alloc2(comp->turn_sock,
						&ice_st->cfg.turn.server,
						ice_st->cfg.turn.port,
						ice_st->cfg.resolver,
						&ice_st->cfg.turn.auth_cred,
						&ice_st->cfg.turn.alloc_param,
						0,
						ice_st->cfg.turn_cnt,  
						ice_st->cfg.turn_list);
		}
		status=pj_turn_sock_alloc2(comp->turn_tcp_sock,
			&ice_st->cfg.turn.server,
			ice_st->cfg.turn.port,
			ice_st->cfg.resolver,
			&ice_st->cfg.turn.auth_cred,
			&ice_st->cfg.turn.alloc_param,
			0,
			ice_st->cfg.turn_cnt,  
			ice_st->cfg.turn_list); 

	}
    if (status != PJ_SUCCESS) {
	///sess_dec_ref(ice_st);
	return status;
    }

    /* Add relayed candidate with pending status if there's no existing one */
    if (cand == NULL) {
	cand = &comp->cand_list[comp->cand_cnt++];
	cand->type = PJ_ICE_CAND_TYPE_RELAYED;
	cand->status = PJ_EPENDING;
	cand->local_pref = RELAY_PREF;
	cand->transport_id = TP_TURN;
	cand->comp_id = (pj_uint8_t) comp->comp_id;
	cand->tcp_type = PJ_ICE_CAND_TCP_TYPE_NONE;
	pj_gettimeofday(&cand->adding_time); // save start collecting time.
    }

    PJ_LOG(4,(ice_st->obj_name,
		  "Comp %d: TURN relay candidate waiting for allocation. thread_name=%s",
		  comp->comp_id, pj_thread_get_name(pj_thread_this(ice_st->cfg.stun_cfg.inst_id))));

    return PJ_SUCCESS;
}

/*
 * Add or update TCP candidate.
 */
static pj_status_t add_update_tcp_candidate(pj_ice_strans *ice_st,
				   pj_ice_strans_comp *comp, pj_sockaddr *tcp_addr, 
				   pj_sockaddr *tcp_base_addr, pj_sockaddr *tcp_rel_addr, 
				   pj_bool_t use_user_port, pj_time_val adding_time)
{
    pj_ice_sess_cand *cand = NULL;
    unsigned i;

	PJ_UNUSED_ARG(tcp_rel_addr);

    /* Find relayed candidate in the component */
    for (i=0; i<comp->cand_cnt; ++i) {
	if (comp->cand_list[i].type == PJ_ICE_CAND_TYPE_SRFLX_TCP && 
		comp->cand_list[i].transport_id == TP_UPNP_TCP) {
	    cand = &comp->cand_list[i];
	    break;
	}
    }

    /* If candidate is found, invalidate it first */
    if (cand) {
	cand->status = PJ_EPENDING;

	/* Also if this component's default candidate is set to relay,
	 * move it temporarily to something else.
	 */
	if ((int)comp->default_cand == cand - comp->cand_list) {
	    /* Init to something */
	    comp->default_cand = 0;
	    /* Use srflx candidate as the default, if any */
	    for (i=0; i<comp->cand_cnt; ++i) {
		if (comp->cand_list[i].type == PJ_ICE_CAND_TYPE_SRFLX_TCP) {
		    comp->default_cand = i;
		    break;
		}
	    }
	}
    }

    /* Add reflex TCP candidate with pending status if there's no existing one */
    if (cand == NULL) {
	cand = &comp->cand_list[comp->cand_cnt++];
	cand->type = PJ_ICE_CAND_TYPE_SRFLX_TCP;
	cand->status = PJ_SUCCESS;
	cand->local_pref = UPNP_TCP_PREF;
	cand->transport_id = TP_UPNP_TCP;
	cand->comp_id = (pj_uint8_t) comp->comp_id;
	cand->tcp_type = PJ_ICE_CAND_TCP_TYPE_PASSIVE;
	cand->use_user_port = use_user_port;
	cand->adding_time = adding_time;     // save adding time.
	pj_gettimeofday(&cand->ending_time); // save ending time.
	/* The cand->addr is assigned in stun_on_status 
	function when mapped-address changed */
	/* If addr is not assigned here, it will 
	be empty when NAT type is Open. */
	pj_sockaddr_cp(&cand->addr, tcp_addr);
	pj_sockaddr_cp(&cand->base_addr, tcp_base_addr);
    pj_sockaddr_cp(&cand->rel_addr, &cand->base_addr);
    pj_ice_calc_foundation(ice_st->pool, &cand->foundation,
			   cand->type, &cand->base_addr);
    }

    PJ_LOG(4,(ice_st->obj_name,
		  "Comp %d: TCP candidate waiting for check",
		  comp->comp_id));

    return PJ_SUCCESS;
}


/*
 * Create the component.
 */
static pj_status_t create_comp(pj_ice_strans *ice_st, unsigned comp_id,
							   int tp_idx)
{
    pj_ice_strans_comp *comp = NULL;
    pj_status_t status;

    /* Verify arguments */
    PJ_ASSERT_RETURN(ice_st && comp_id, PJ_EINVAL);

    /* Check that component ID present */
    PJ_ASSERT_RETURN(comp_id <= ice_st->comp_cnt, PJNATH_EICEINCOMPID);

    /* Create component */
    comp = PJ_POOL_ZALLOC_T(ice_st->pool, pj_ice_strans_comp);
    comp->ice_st = ice_st;
    comp->comp_id = comp_id;

    ice_st->comp[comp_id-1] = comp;

    /* Initialize default candidate */
    comp->default_cand = 0;

    /* Create STUN transport if configured */
    if (ice_st->cfg.stun.server.slen || ice_st->cfg.stun.max_host_cands) {
	pj_stun_sock_cb stun_sock_cb;
	pj_ice_sess_cand *cand;

	pj_bzero(&stun_sock_cb, sizeof(stun_sock_cb));
	stun_sock_cb.on_rx_data = &stun_on_rx_data;
	stun_sock_cb.on_status = &stun_on_status;
	stun_sock_cb.on_data_sent = &stun_on_data_sent;

	/* Override component specific QoS settings, if any */
	if (ice_st->cfg.comp[comp_id-1].qos_type) {
	    ice_st->cfg.stun.cfg.qos_type =
		ice_st->cfg.comp[comp_id-1].qos_type;
	}
	if (ice_st->cfg.comp[comp_id-1].qos_params.flags) {
	    pj_memcpy(&ice_st->cfg.stun.cfg.qos_params,
		      &ice_st->cfg.comp[comp_id-1].qos_params,
		      sizeof(ice_st->cfg.stun.cfg.qos_params));
	}

	/* Create the STUN transport */
	status = pj_stun_sock_create(&ice_st->cfg.stun_cfg, NULL,
				     ice_st->cfg.af, &stun_sock_cb,
				     &ice_st->cfg.stun.cfg,
				     comp, &comp->stun_sock);
	if (status != PJ_SUCCESS)
	    return status;

	/* Start STUN Binding resolution and add srflx candidate
	 * only if server is set
	 */
	if (ice_st->cfg.stun.server.slen) {
	    pj_stun_sock_info stun_sock_info;

	    /* Enumerate addresses */
	    status = pj_stun_sock_get_info(comp->stun_sock, &stun_sock_info);
	    if (status != PJ_SUCCESS) {
		///sess_dec_ref(ice_st);
		return status;
	    }

	    /* Add srflx candidate with pending status. */
	    cand = &comp->cand_list[comp->cand_cnt++];
	    cand->type = PJ_ICE_CAND_TYPE_SRFLX;
	    cand->status = PJ_EPENDING;
	    cand->local_pref = SRFLX_PREF;
	    cand->transport_id = TP_STUN;
	    cand->comp_id = (pj_uint8_t) comp_id;
		cand->use_user_port = PJ_FALSE;
		pj_gettimeofday(&cand->adding_time); // save start collecting time.
		/* The cand->addr is assigned in stun_on_status 
		function when mapped-address changed */
		pj_sockaddr_cp(&cand->base_addr, &stun_sock_info.aliases[0]);
	    pj_sockaddr_cp(&cand->rel_addr, &cand->base_addr);
	    pj_ice_calc_foundation(ice_st->pool, &cand->foundation,
				   cand->type, &cand->base_addr);

	    /* Set default candidate to srflx */
		comp->default_cand = cand - comp->cand_list;

		// DEAN. Move here. Don't add UDP candidate if stun server is not configured.
		/* Add local addresses to host candidates, unless max_host_cands
		 * is set to zero.
		 */
		if (ice_st->cfg.stun.max_host_cands) {
			//pj_stun_sock_info stun_sock_info;
			unsigned i;

			// 2015-03-05 No need to get stun_sock again, it may waste time. 
			// Because the
			/* Enumerate addresses */ 
			//status = pj_stun_sock_get_info(comp->stun_sock, &stun_sock_info);
			//if (status != PJ_SUCCESS)
			//return status;

			for (i=0; i<stun_sock_info.alias_cnt && 
				  i<ice_st->cfg.stun.max_host_cands; ++i) 
			{
			char addrinfo[PJ_INET6_ADDRSTRLEN+10];
			const pj_sockaddr *addr = &stun_sock_info.aliases[i];

			/* Leave one candidate for relay */
			if (comp->cand_cnt >= PJ_ICE_ST_MAX_CAND-1) {
				PJ_LOG(4,(ice_st->obj_name, "Too many host candidates"));
				break;
			}

			/* Ignore loopback addresses unless cfg->stun.loop_addr 
			 * is set 
			 */
			if ((pj_ntohl(addr->ipv4.sin_addr.s_addr)>>24)==127) {
				if (ice_st->cfg.stun.loop_addr==PJ_FALSE)
				continue;
			}

			cand = &comp->cand_list[comp->cand_cnt++];

			cand->type = PJ_ICE_CAND_TYPE_HOST;
			cand->status = PJ_SUCCESS;
			cand->local_pref = HOST_PREF;
			cand->transport_id = TP_STUN;
			cand->comp_id = (pj_uint8_t) comp_id;
			pj_gettimeofday(&cand->adding_time); // save adding time.
			PJ_LOG(4,(ice_st->obj_name, "pj_gettimeofday1"));
			pj_gettimeofday(&cand->ending_time); // save ending time.
			pj_memcpy(&cand->ending_time, &cand->adding_time, sizeof(pj_time_val));
			PJ_LOG(4,(ice_st->obj_name, "pj_gettimeofday2"));
			pj_sockaddr_cp(&cand->addr, addr);
			PJ_LOG(4,(ice_st->obj_name, "pj_sockaddr_cp1"));
			pj_sockaddr_cp(&cand->base_addr, addr);
			PJ_LOG(4,(ice_st->obj_name, "pj_sockaddr_cp2"));
			pj_bzero(&cand->rel_addr, sizeof(cand->rel_addr));
			PJ_LOG(4,(ice_st->obj_name, "pj_bzero"));
			pj_ice_calc_foundation(ice_st->pool, &cand->foundation,
				cand->type, &cand->base_addr);
			PJ_LOG(4,(ice_st->obj_name, "pj_ice_calc_foundation"));

			PJ_LOG(4,(ice_st->obj_name, 
				  "Comp %d: host candidate %s added",
				  comp_id, pj_sockaddr_print(&cand->addr, addrinfo,
								 sizeof(addrinfo), 3)));
			}
		}
		///sess_add_ref(ice_st);

		PJ_LOG(4,(ice_st->obj_name, 
			"Comp %d: srflx candidate starts Binding discovery",
			comp_id));

		/* Start Binding resolution */
		status = pj_stun_sock_start(comp->stun_sock, 
			&ice_st->cfg.stun.server,
			ice_st->cfg.stun.port, 
			ice_st->cfg.resolver);
		if (status != PJ_SUCCESS) {
			///sess_dec_ref(ice_st);
			return status;
		}
		/* Add pending job */
		PJ_LOG(4,(ice_st->obj_name, "pj_stun_sock_start()."));
	}

	// DEAN. Move here. Independent from STUN.
	/* Create TCP srflx candidate. */
	{
		//delay to pj_ice_strans_start_ice
		//2013-09-13 DEAN, start every candidate tcp server at this moment.
		//2013-10-18 DEAN, delay to pj_ice_strans_start_ice again. 
		//2014-01-10 DEAN, dont do this here, but start every candidate tcp server at candidate adding moment.
#ifdef COLLECT_TCP_CAND
		pj_tcp_sock_cb tcp_sock_cb;
		pj_status_t status;
		pj_stun_sock_info stun_sock_info;
		int rand_port;
		pj_time_val adding_time;

		/* Enumerate addresses */
		status = pj_stun_sock_get_info(comp->stun_sock, &stun_sock_info);
		if (status != PJ_SUCCESS) {
			///sess_dec_ref(ice_st);
			return status;
		}

		/* Init TCP socket */
		pj_bzero(&tcp_sock_cb, sizeof(tcp_sock_cb));
		tcp_sock_cb.on_rx_data = &tcp_on_rx_data;
		tcp_sock_cb.on_state = &tcp_on_state;
		tcp_sock_cb.on_tcp_server_binding_complete = &tcp_on_server_binding_complete;

		/* Override with component specific QoS settings, if any */
		if (ice_st->cfg.comp[comp->comp_id-1].qos_type) {
			ice_st->cfg.tcp.cfg.qos_type =
				ice_st->cfg.comp[comp->comp_id-1].qos_type;
		}
		if (ice_st->cfg.comp[comp->comp_id-1].qos_params.flags) {
			pj_memcpy(&ice_st->cfg.tcp.cfg.qos_params,
				&ice_st->cfg.comp[comp->comp_id-1].qos_params,
				sizeof(ice_st->cfg.tcp.cfg.qos_params));
		}

		/* Create the TCP transport */
		status = pj_tcp_sock_create(&ice_st->cfg.stun_cfg, ice_st->cfg.af,
			&tcp_sock_cb,/* &ice_st->cfg.tcp.cfg,*/
			comp, comp->comp_id, &comp->tcp_sock);
		if (status != PJ_SUCCESS) {
			return status;
		}

		pj_sockaddr_cp(&comp->tcp_addr, &stun_sock_info.aliases[0]);
		pj_sockaddr_cp(&comp->tcp_base_addr, &comp->tcp_addr);

		/* Check if user selected port assigned */
		if (ice_st->cfg.user_ports[tp_idx].user_port_assigned) {
			if (comp_id == 1) {
				pj_sockaddr_set_port(&comp->tcp_addr, 
					ice_st->cfg.user_ports[tp_idx].external_tcp_data_port);
				pj_sockaddr_set_port(&comp->tcp_base_addr, 
					ice_st->cfg.user_ports[tp_idx].local_tcp_data_port);
				//pj_sockaddr_set_port(&comp->tcp_rel_addr, 
				//	ice_st->cfg.user_ports[tp_idx].local_tcp_data_port);
			} else {
				pj_sockaddr_set_port(&comp->tcp_addr, 
					ice_st->cfg.user_ports[tp_idx].external_tcp_ctl_port);
				pj_sockaddr_set_port(&comp->tcp_base_addr, 
					ice_st->cfg.user_ports[tp_idx].local_tcp_ctl_port);
				//pj_sockaddr_set_port(&comp->tcp_rel_addr, 
				//	ice_st->cfg.user_ports[tp_idx].local_tcp_ctl_port);
			}
		}

		pj_gettimeofday(&adding_time); // Save adding time.
		status = tcp_sock_create_server(ice_st->pool, comp->tcp_sock, 
			&comp->tcp_addr,
			&comp->tcp_base_addr,
			!ice_st->cfg.user_ports[tp_idx].user_port_assigned);
		if (status == PJ_SUCCESS) {
			if (!ice_st->cfg.user_ports[tp_idx].user_port_assigned)
			{
				pj_sockaddr_cp(&comp->tcp_rel_addr, &comp->tcp_base_addr);
			}

			add_update_tcp_candidate(ice_st, comp, &comp->tcp_addr, 
				&comp->tcp_base_addr, &comp->tcp_rel_addr, 
				ice_st->cfg.user_ports[tp_idx].user_port_assigned,
				adding_time);
		}
#endif
	}

#ifdef COLLECT_TCP_CAND
	/* Add local addresses to host TCP candidates, unless max_host_cands
	 * is set to zero.
	 */
	if (ice_st->cfg.stun.max_host_cands) {
	    pj_stun_sock_info stun_sock_info;
	    unsigned i;

	    /* Enumerate addresses */
	    status = pj_stun_sock_get_info(comp->stun_sock, &stun_sock_info);
	    if (status != PJ_SUCCESS)
		return status;

	    for (i=0; i<stun_sock_info.alias_cnt && 
		      i<ice_st->cfg.stun.max_host_cands; ++i) 
	    {
		char addrinfo[PJ_INET6_ADDRSTRLEN+10];
		pj_sockaddr *addr = &comp->tcp_base_addr;//&stun_sock_info.aliases[i];

		/* Leave one candidate for relay */
		if (comp->cand_cnt >= PJ_ICE_ST_MAX_CAND-1) {
		    PJ_LOG(4,(ice_st->obj_name, "Too many host candidates"));
		    break;
		}

		/* Ignore loopback addresses unless cfg->stun.loop_addr 
		 * is set 
		 */
		if ((pj_ntohl(addr->ipv4.sin_addr.s_addr)>>24)==127) {
		    if (ice_st->cfg.stun.loop_addr==PJ_FALSE)
			continue;
		}

		/* Check if user selected port assigned */
		if (ice_st->cfg.user_ports[tp_idx].user_port_assigned) {
			if (comp_id == 1) {
				pj_sockaddr_set_port(addr, 
					ice_st->cfg.user_ports[tp_idx].local_tcp_data_port);
			} else {
				pj_sockaddr_set_port(addr, 
					ice_st->cfg.user_ports[tp_idx].local_tcp_ctl_port);
			}
		}

		cand = &comp->cand_list[comp->cand_cnt++];

		cand->type = PJ_ICE_CAND_TYPE_HOST_TCP;
		cand->status = PJ_SUCCESS;
		cand->local_pref = UPNP_TCP_PREF;
		cand->transport_id = TP_UPNP_TCP;
		cand->comp_id = (pj_uint8_t) comp_id;
		cand->tcp_type = PJ_ICE_CAND_TCP_TYPE_ACTIVE;
		pj_gettimeofday(&cand->adding_time); // save adding time.
		pj_gettimeofday(&cand->ending_time); // save ending time.
		pj_sockaddr_cp(&cand->addr, addr);
		pj_sockaddr_cp(&cand->base_addr, addr);
		//pj_sockaddr_set_port(&cand->addr, 9);
		//pj_sockaddr_set_port(&cand->base_addr, 9);
		pj_bzero(&cand->rel_addr, sizeof(cand->rel_addr));
		pj_ice_calc_foundation(ice_st->pool, &cand->foundation,
				       cand->type, &cand->base_addr);

		PJ_LOG(4,(ice_st->obj_name, 
			  "Comp %d: host TCP candidate %s added",
			  comp_id, pj_sockaddr_print(&cand->addr, addrinfo,
						     sizeof(addrinfo), 3)));
	    }
	}
#endif
    }

#if 0
    /* Create TURN relay if configured. */
    if (ice_st->cfg.turn.server.slen) {
	add_update_turn(ice_st, comp);
    }
#endif
    return PJ_SUCCESS;
}


/* 
 * Create ICE stream transport 
 */
PJ_DEF(pj_status_t) pj_ice_strans_create2( const char *name,
					  const pj_ice_strans_cfg *cfg,
					  unsigned comp_cnt,
					  void *user_data,
					  const pj_ice_strans_cb *cb,
					  int tp_idx,
					  pj_ice_strans **p_ice_st)
{
    pj_pool_t *pool;
    pj_ice_strans *ice_st;
    unsigned i;
    pj_status_t status;

    status = pj_ice_strans_cfg_check_valid(cfg);
    if (status != PJ_SUCCESS)
	return status;

    PJ_ASSERT_RETURN(comp_cnt && cb && p_ice_st &&
		     comp_cnt <= PJ_ICE_MAX_COMP , PJ_EINVAL);

    if (name == NULL)
	name = "ice%p";

    pool = pj_pool_create(cfg->stun_cfg.pf, name, PJNATH_POOL_LEN_ICE_STRANS,
			  PJNATH_POOL_INC_ICE_STRANS, NULL);
    ice_st = PJ_POOL_ZALLOC_T(pool, pj_ice_strans);
    ice_st->pool = pool;
    ice_st->obj_name = pool->obj_name;
    ice_st->user_data = user_data;

    PJ_LOG(4,(ice_st->obj_name,
	      "Creating ICE stream transport with %d component(s)",
	      comp_cnt));

    status = pj_grp_lock_create(pool, NULL, &ice_st->grp_lock);
    if (status != PJ_SUCCESS) {
	pj_pool_release(pool);
	return status;
    }

    pj_grp_lock_add_ref(ice_st->grp_lock);
    pj_grp_lock_add_handler(ice_st->grp_lock, pool, ice_st,
			    &ice_st_on_destroy);

    pj_ice_strans_cfg_copy(pool, &ice_st->cfg, cfg);
    ice_st->cfg.stun.cfg.grp_lock = ice_st->grp_lock;
    ice_st->cfg.turn.cfg.grp_lock = ice_st->grp_lock;
    pj_memcpy(&ice_st->cb, cb, sizeof(*cb));

    ice_st->comp_cnt = comp_cnt;
    ice_st->comp = (pj_ice_strans_comp**)
		   pj_pool_calloc(pool, comp_cnt, sizeof(pj_ice_strans_comp*));

    /* Move state to candidate gathering */
    ice_st->state = PJ_ICE_STRANS_STATE_INIT;

    /* Acquire initialization mutex to prevent callback to be
     * called before we finish initialization.
     */
    pj_grp_lock_acquire(ice_st->grp_lock);

    for (i=0; i<comp_cnt; ++i) {
	status = create_comp(ice_st, i+1, tp_idx);
	if (status != PJ_SUCCESS) {
	    pj_grp_lock_release(ice_st->grp_lock);
	    destroy_ice_st(ice_st);
	    return status;
	}
    }

    /* Done with initialization */
    pj_grp_lock_release(ice_st->grp_lock);

    PJ_LOG(4,(ice_st->obj_name, "ICE stream transport %p created", ice_st));

    *p_ice_st = ice_st;

    /* Check if all candidates are ready (this may call callback) */
    sess_init_update(ice_st);

    PJ_LOG(4,(ice_st->obj_name, "ICE stream transport created"));

	// natnl tunnel timeout second
	ice_st->tnl_timeout_msec = cfg->tnl_timeout_msec;

    return PJ_SUCCESS;
}


/* 
 * Create ICE stream transport 
 */
PJ_DEF(pj_status_t) pj_ice_strans_create( const char *name,
					  const pj_ice_strans_cfg *cfg,
					  unsigned comp_cnt,
					  void *user_data,
					  const pj_ice_strans_cb *cb,
					  pj_ice_strans **p_ice_st)
{
	return pj_ice_strans_create2(name, cfg, comp_cnt, 
		user_data, cb, -1, p_ice_st);
}

/* REALLY destroy ICE */
static void ice_st_on_destroy(void *obj)
{
    pj_ice_strans *ice_st = (pj_ice_strans*)obj;

    PJ_LOG(4,(ice_st->obj_name, "ICE stream transport %p destroyed", obj));

    /* Done */
    pj_pool_release(ice_st->pool);
}

/* Destroy ICE */
static void destroy_ice_st(pj_ice_strans *ice_st)
{
    unsigned i;

    PJ_LOG(5,(ice_st->obj_name, "ICE stream transport %p destroy request..",
	      ice_st));

    pj_grp_lock_acquire(ice_st->grp_lock);

    if (ice_st->destroy_req) {
	pj_grp_lock_release(ice_st->grp_lock);
	return;
    }

    ice_st->destroy_req = PJ_TRUE;

    /* Destroy ICE if we have ICE */
    if (ice_st->ice) {
        // To avoid hangup during making call and raise assertion.
	    pj_timer_heap_cancel_if_active(ice_st->ice->stun_cfg.timer_heap,
	                                   &ice_st->tcp_sess_timer, PJ_FALSE);
        pj_ice_sess_destroy(ice_st->ice);
        ice_st->ice = NULL;
    }

    /* Destroy all components */
    for (i=0; i<ice_st->comp_cnt; ++i) {
		if (ice_st->comp[i]) {
			// natnl
			if (ice_st->comp[i]->stun_sock) {
				pj_stun_sock_set_user_data(ice_st->comp[i]->stun_sock, NULL);
				pj_stun_sock_destroy(ice_st->comp[i]->stun_sock);
				ice_st->comp[i]->stun_sock = NULL;
			}
			if (ice_st->comp[i]->turn_sock) {
				PJ_LOG(4, ("ice_strans.c", "!!! TURN DEALLOCATE !!! in destroy_ice_st() destroy turn_sock."));
				pj_turn_sock_set_user_data(ice_st->comp[i]->turn_sock, NULL);
				pj_turn_sock_destroy(ice_st->comp[i]->turn_sock);
				ice_st->comp[i]->turn_sock = NULL;
			}
			if (ice_st->comp[i]->tcp_sock) {
				pj_tcp_sock_set_user_data(ice_st->comp[i]->tcp_sock, NULL);
				pj_tcp_sock_destroy(ice_st->comp[i]->tcp_sock);
				ice_st->comp[i]->tcp_sock = NULL;
			}
		}
    }

    ice_st->comp_cnt = 0;
    pj_grp_lock_dec_ref(ice_st->grp_lock);
    pj_grp_lock_release(ice_st->grp_lock);

	// 2013-11-04 DEAN, free dest_uri pointer
	if (ice_st->dest_uri.ptr)
		free(ice_st->dest_uri.ptr);

    /* Done */
    //pj_pool_release(ice_st->pool);
}

/* Get ICE session state. */
PJ_DEF(pj_ice_strans_state) pj_ice_strans_get_state(pj_ice_strans *ice_st)
{
    return ice_st->state;
}

/* State string */
PJ_DEF(const char*) pj_ice_strans_state_name(pj_ice_strans_state state)
{
    const char *names[] = {
	"Null",
	"Candidate Gathering",
	"Candidate Gathering Complete",
	"Session Initialized",
	"Negotiation In Progress",
	"Negotiation Success",
	"Negotiation Failed"
    };

    PJ_ASSERT_RETURN(state <= PJ_ICE_STRANS_STATE_FAILED, "???");
    return names[state];
}

/* Notification about failure */
static void sess_fail(pj_ice_strans *ice_st, pj_ice_strans_op op,
		      const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(4,(ice_st->obj_name, "%s: %s", title, errmsg));

    if (op==PJ_ICE_STRANS_OP_INIT && ice_st->cb_called)
	return;

    ice_st->cb_called = PJ_TRUE;

	if (op==PJ_ICE_STRANS_OP_KEEP_ALIVE && ice_st->ka_cb_called)
		return;
	//else if (op==PJ_ICE_STRANS_OP_KEEP_ALIVE && !ice_st->ka_cb_called)
	//	ice_st->ka_cb_called = PJ_TRUE;

    if (ice_st->cb.on_ice_complete)
	(*ice_st->cb.on_ice_complete)(ice_st, op, status, NULL);
}

/* Update initialization status */
static void sess_init_update(pj_ice_strans *ice_st)
{
    unsigned i;

    /* Ignore if init callback has been called */
    if (!ice_st->addr_changed_type && ice_st->cb_called)
	return;

    /* Notify application when all candidates have been gathered */
    for (i=0; i<ice_st->comp_cnt; ++i) {
	unsigned j;
	pj_ice_strans_comp *comp = ice_st->comp[i];

	for (j=0; j<comp->cand_cnt; ++j) {
	    pj_ice_sess_cand *cand = &comp->cand_list[j];

		if (cand->status == PJ_EPENDING) {
			PJ_LOG(4, ("ice_strans.c", "!!!!NATNL!!!!Candidate(type=%d) status is pending", 
				cand->type));
		return;
		}
	}
    }

    /* All candidates have been gathered */
    ice_st->cb_called = PJ_TRUE;
    ice_st->state = PJ_ICE_STRANS_STATE_READY;
	if (ice_st->cb.on_ice_complete) {
		(*ice_st->cb.on_ice_complete)(ice_st, PJ_ICE_STRANS_OP_INIT, 
		PJ_SUCCESS, NULL);
}
}
/*
 * Destroy ICE stream transport.
 */
PJ_DEF(pj_status_t) pj_ice_strans_destroy(pj_ice_strans *ice_st)
{
    destroy_ice_st(ice_st);
    return PJ_SUCCESS;
}


/*
 * Get user data
 */
PJ_DEF(void*) pj_ice_strans_get_user_data(pj_ice_strans *ice_st)
{
    PJ_ASSERT_RETURN(ice_st, NULL);
    return ice_st->user_data;
}


/*
 * Get the value of various options of the ICE stream transport.
 */
PJ_DEF(pj_status_t) pj_ice_strans_get_options( pj_ice_strans *ice_st,
					       pj_ice_sess_options *opt)
{
    PJ_ASSERT_RETURN(ice_st && opt, PJ_EINVAL);
    pj_memcpy(opt, &ice_st->cfg.opt, sizeof(*opt));
    return PJ_SUCCESS;
}

/*
 * Specify various options for this ICE stream transport.
 */
PJ_DEF(pj_status_t) pj_ice_strans_set_options(pj_ice_strans *ice_st,
					      const pj_ice_sess_options *opt)
{
    PJ_ASSERT_RETURN(ice_st && opt, PJ_EINVAL);
    pj_memcpy(&ice_st->cfg.opt, opt, sizeof(*opt));
    if (ice_st->ice)
	pj_ice_sess_set_options(ice_st->ice, &ice_st->cfg.opt);
    return PJ_SUCCESS;
}

/**
 * Get the group lock for this ICE stream transport.
 */
PJ_DEF(pj_grp_lock_t *) pj_ice_strans_get_grp_lock(pj_ice_strans *ice_st)
{
    PJ_ASSERT_RETURN(ice_st, NULL);
    return ice_st->grp_lock;
}

PJ_DEF(pj_status_t) pj_ice_strans_init_ice2(pj_ice_strans *ice_st,
										   pj_ice_sess_role role,
										   const pj_str_t *local_ufrag,
										   const pj_str_t *local_passwd) {
    pj_status_t status;
    unsigned i;
	pj_ice_sess_cb ice_cb;

    /* Init callback */
    pj_bzero(&ice_cb, sizeof(ice_cb));
    ice_cb.on_ice_complete = &on_ice_complete;
	ice_cb.on_rx_data = &ice_rx_data;
	ice_cb.on_tx_pkt = &ice_tx_pkt;
	ice_cb.on_tx_pkt2 = &ice_tcp_tx_pkt;

    /* Create! */
    status = pj_ice_sess_create(&ice_st->cfg.stun_cfg, ice_st->obj_name, role,
			        ice_st->comp_cnt, &ice_cb, 
			        local_ufrag, local_passwd, 
			        ice_st->grp_lock,
			        &ice_st->ice,
					ice_st->tnl_timeout_msec);
    if (status != PJ_SUCCESS)
	return status;

    /* Associate user data */
	ice_st->ice->user_data = (void*)ice_st;

    /* Set options */
    pj_ice_sess_set_options(ice_st->ice, &ice_st->cfg.opt);

    /* If default candidate for components are SRFLX one, upload a custom
     * type priority to ICE session so that SRFLX candidates will get
     * checked first.
     */
	if (ice_st->comp[0]->default_cand >= 0 &&
		(ice_st->comp[0]->cand_list[ice_st->comp[0]->default_cand].type 
		== PJ_ICE_CAND_TYPE_SRFLX ||
		ice_st->comp[0]->cand_list[ice_st->comp[0]->default_cand].type 
		== PJ_ICE_CAND_TYPE_SRFLX_TCP))
    {
	pj_ice_sess_set_prefs(ice_st->ice, srflx_pref_table);
    }

    /* Add components/candidates */
    for (i=0; i<ice_st->comp_cnt; ++i) {
	unsigned j;
	pj_ice_strans_comp *comp = ice_st->comp[i];

	/* Re-enable logging for Send/Data indications */
	if (comp->turn_sock) {
	    PJ_LOG(5,(ice_st->obj_name, 
		      "Disabling STUN Indication logging for "
		      "component %d", i+1));
	    pj_turn_sock_set_log(comp->turn_sock, 0xFFFF);
	    comp->turn_log_off = PJ_FALSE;
	}

	/* Re-enable logging for Send/Data indications */
	if (comp->turn_tcp_sock) {
		PJ_LOG(5,(ice_st->obj_name, 
			"Disabling STUN Indication logging for "
			"component %d", i+1));
		pj_turn_sock_set_log(comp->turn_tcp_sock, 0xFFFF);
		comp->turn_log_off = PJ_FALSE;
	}

	for (j=0; j<comp->cand_cnt; ++j) {
	    pj_ice_sess_cand *cand = &comp->cand_list[j];
	    unsigned ice_cand_id;
#ifdef TODO
            if (comp->tcp_sock)
                pj_tcp_sock_set_stun_session_user_data(comp->tcp_sock, ice_st, comp->comp_id);
#endif

	    /* Skip if candidate is not ready */
	    if (cand->status != PJ_SUCCESS) {
			if (cand->type != PJ_ICE_CAND_TYPE_SRFLX) {
				PJ_LOG(4,(ice_st->obj_name, 
					  "Candidate %d of comp %d is not added (pending)",
					  j, i));
				cand->enabled = PJ_FALSE;
				pj_gettimeofday(&cand->ending_time);
				continue;
			} else {
				cand->status = PJ_SUCCESS;
				cand->enabled = PJ_TRUE;
			}
		} else {
			cand->enabled = PJ_TRUE;
		}

		// don't add tcp candidate if upnp_tcp disabled
		if (ice_st->use_upnp_flag == 0 && 
			cand->type == PJ_ICE_CAND_TYPE_SRFLX_TCP
			/*pj_ice_strans_tp_is_upnp_tcp(cand->transport_id)*/) {
				cand->enabled = PJ_FALSE;
				pj_gettimeofday(&cand->ending_time);
				continue;
		}

		// don't add stun candidate if stun disabled
		if (!ice_st->use_stun_cand && 
			pj_ice_strans_tp_is_stun(cand->transport_id)) {
				cand->enabled = PJ_FALSE;
				pj_gettimeofday(&cand->ending_time);
				continue;
		}

		if (!pj_ice_strans_tp_is_upnp_tcp(cand->transport_id)) {
			/* Must have address */
			//pj_assert(pj_sockaddr_has_addr(&cand->addr));
			if (!pj_sockaddr_has_addr(&cand->addr)) {
				status = PJNATH_EICENOHOSTCAND;
				goto on_error;
			}

			/* Add the candidate */
			status = pj_ice_sess_add_cand(ice_st->ice, comp->comp_id, 
				cand->transport_id, cand->type, 
				cand->local_pref, 
				&cand->foundation, &cand->addr, 
				&cand->base_addr,  &cand->rel_addr,
				pj_sockaddr_get_len(&cand->addr),
				(unsigned*)&ice_cand_id, 
				cand->tcp_type,
				cand->adding_time,
				cand->ending_time,
				cand->enabled);
		} else {
			/* Must have address */
			//pj_assert(pj_sockaddr_has_addr(&cand->addr));
			if (!pj_sockaddr_has_addr(&cand->addr)) {
				status = PJNATH_EICENOHOSTCAND;
				goto on_error;
			} else {
				char buf[PJ_INET6_ADDRSTRLEN+10];
				// DEAN for assertion fail
				PJ_LOG(6, (ice_st->obj_name, "pj_ice_strans_init_ice() cand->tcp_addr=%s", 
					pj_sockaddr_print(&cand->addr, buf, sizeof(buf), 3)));
			}

			/* Add the candidate */
			status = pj_ice_sess_add_cand(ice_st->ice, comp->comp_id, 
				cand->transport_id, cand->type, 
				cand->local_pref, 
				&cand->foundation, &cand->addr, 
				&cand->base_addr,  &cand->rel_addr,
				pj_sockaddr_get_len(&cand->addr),
				(unsigned*)&ice_cand_id, 
				cand->tcp_type,
				cand->adding_time,
				cand->ending_time,
				cand->enabled);
		}
	    if (status != PJ_SUCCESS)
		goto on_error;
	}
    }

    /* ICE session is ready for negotiation */
    ice_st->state = PJ_ICE_STRANS_STATE_SESS_READY;

    return PJ_SUCCESS;

on_error:
    pj_ice_strans_stop_ice(ice_st);
    return status;
}

/* TURN ioqueue temporary thread function. */
static int turn_ioq_temp_thread(void *arg)
{
	pj_time_val timeout = { 0, 10};
	int curr_tv = 0;

	pj_ice_strans *ice_st = (pj_ice_strans *)arg;
	int c = 0;
	int max_waiting_time = 7000*ice_st->cfg.turn_cnt; // (turn_srv_cnt*22secs, 22 tcp connect timeout)

	if ((ice_st->use_turn_flag & TURN_FLAG_USE_UAC_TURN) > 0 && ice_st->ice_role == PJ_ICE_SESS_ROLE_CONTROLLED)
		max_waiting_time = 7000;

	do {
		//c = pjsua_handle_events(ice_st->pool->factory->inst_id, timeout);
		c = pj_ioqueue_poll(ice_st->cfg.stun_cfg.ioqueue, &timeout);

		if (c < 0)
			break;

		if (!ice_st->wait_turn_alloc_ok_cnt)
			break;
			
		curr_tv += timeout.msec;
		// if TURN allocation isn't success in some period, then regarded as timeout.
		if (curr_tv >= max_waiting_time) {
			PJ_LOG(4, ("ice_strans.c", "turn_ioq_temp_thread turn allocation timeout. timeout=%d", max_waiting_time));
			break;
		}
	} while (1);

	return 0;
}

/*
 * Create ICE!
 */
PJ_DEF(pj_status_t) pj_ice_strans_init_ice(pj_ice_strans *ice_st,
					   pj_ice_sess_role role,
					   const pj_str_t *local_ufrag,
					   const pj_str_t *local_passwd)
{
    pj_status_t status;
    unsigned i;
    //const pj_uint8_t srflx_prio[4] = { 100, 126, 110, 0 };

    /* Check arguments */
    //PJ_ASSERT_RETURN(ice_st, PJ_EINVAL);
	if(!ice_st)
		return PJ_EINVAL;
    /* Must not have ICE */
	//PJ_ASSERT_RETURN(ice_st->ice == NULL, PJ_EINVALIDOP);
	if(ice_st->ice)
		return PJ_EINVALIDOP;
    /* Components must have been created */
	//PJ_ASSERT_RETURN(ice_st->comp[0] != NULL, PJ_EINVALIDOP);
	if(!ice_st->comp[0])
		return PJ_EINVALIDOP;

	if (ice_st->use_turn_flag > 0)
	{
		ice_st->wait_turn_alloc_ok_cnt = 0;
		for (i=0; i<ice_st->comp_cnt;i++)
		{
			pj_str_t turn_server;
			pj_ice_strans_comp *comp = ice_st->comp[i];
			/* Create TURN relay if configured. */
			// if it is UAS, use try use remote turn config
			if (role == PJ_ICE_SESS_ROLE_CONTROLLING)
				turn_server = ice_st->cfg.turn.server;
			else {
				if ((ice_st->use_turn_flag & TURN_FLAG_USE_UAC_TURN) > 0 &&  // uas use uac's turn
					ice_st->cfg.rem_turn.server.slen)
					turn_server = ice_st->cfg.rem_turn.server;
				else
					turn_server = ice_st->cfg.turn.server;
			}
			if (turn_server.slen) {
				add_update_turn(ice_st, role, comp);
				ice_st->wait_turn_alloc_ok_cnt++;
			}
		}
	}

	if (!ice_st->wait_turn_alloc_ok_cnt) {
		return pj_ice_strans_init_ice2(ice_st, role, local_ufrag, local_passwd);
	} else { // create a temporary thread to poll ioqueue for turn allocation complete callback.
		pj_thread_t *turn_thread;
		pj_bool_t lock_needed = PJ_FALSE;

		// 2013-12-05 DEAN no need to check app_lock is locked because of PJ_DEBUG disabled.
		// 2013-06-04 DEAN we should check if app_lock is locked.
		// 2013-04-22 DEAN unlock app's lock first to avoid deadlock with acc
		if (ice_st->app_lock && pj_mutex_is_locked(ice_st->app_lock)) {
			PJ_LOG(2, ("pj_mutex_unlock", "%p/%s/%d thread(%s) unlock", ice_st->app_lock, THIS_FILE, __LINE__, pj_thread_get_name(pj_thread_this(ice_st->cfg.stun_cfg.inst_id))));
			pj_mutex_unlock(ice_st->app_lock);
			lock_needed = PJ_TRUE;
		}

		status = pj_thread_create(ice_st->pool, "turn_ioq_thread", &turn_ioq_temp_thread,
			(void *)ice_st, 0, 0, &turn_thread);
		pj_thread_join(turn_thread);

		// 2013-04-22 DEAN unlock app's lock first to avoid deadlock with acc
		if (ice_st->app_lock && lock_needed)
			PJ_LOG(2, ("pj_mutex_lock", "%p/%s/%d thread(%s) lock", ice_st->app_lock, THIS_FILE, __LINE__, pj_thread_get_name(pj_thread_this(ice_st->cfg.stun_cfg.inst_id))));
			pj_mutex_lock(ice_st->app_lock);

		pj_thread_destroy(turn_thread);
		return pj_ice_strans_init_ice2(ice_st, role, local_ufrag, local_passwd);
	}
}

/*
 * Check if the ICE stream transport has the ICE session created.
 */
PJ_DEF(pj_bool_t) pj_ice_strans_has_sess(pj_ice_strans *ice_st)
{
    PJ_ASSERT_RETURN(ice_st, PJ_FALSE);
    return ice_st->ice != NULL;
}

/*
 * Check if ICE negotiation is still running.
 */
PJ_DEF(pj_bool_t) pj_ice_strans_sess_is_running(pj_ice_strans *ice_st)
{
    return ice_st && ice_st->ice && ice_st->ice->rcand_cnt &&
	   !pj_ice_strans_sess_is_complete(ice_st);
}


/*
 * Check if ICE negotiation has completed.
 */
PJ_DEF(pj_bool_t) pj_ice_strans_sess_is_complete(pj_ice_strans *ice_st)
{
    return ice_st && ice_st->ice && ice_st->ice->is_complete;
}


/*
 * Get the current/running component count.
 */
PJ_DEF(unsigned) pj_ice_strans_get_running_comp_cnt(pj_ice_strans *ice_st)
{
    PJ_ASSERT_RETURN(ice_st, PJ_EINVAL);

    if (ice_st->ice && ice_st->ice->rcand_cnt) {
	return ice_st->ice->comp_cnt;
    } else {
	return ice_st->comp_cnt;
    }
}


/*
 * Get the ICE username fragment and password of the ICE session.
 */
PJ_DEF(pj_status_t) pj_ice_strans_get_ufrag_pwd( pj_ice_strans *ice_st,
						 pj_str_t *loc_ufrag,
						 pj_str_t *loc_pwd,
						 pj_str_t *rem_ufrag,
						 pj_str_t *rem_pwd)
{
    PJ_ASSERT_RETURN(ice_st && ice_st->ice, PJ_EINVALIDOP);

    if (loc_ufrag) *loc_ufrag = ice_st->ice->rx_ufrag;
    if (loc_pwd) *loc_pwd = ice_st->ice->rx_pass;

    if (rem_ufrag || rem_pwd) {
	PJ_ASSERT_RETURN(ice_st->ice->rcand_cnt != 0, PJ_EINVALIDOP);
	if (rem_ufrag) *rem_ufrag = ice_st->ice->tx_ufrag;
	if (rem_pwd) *rem_pwd = ice_st->ice->tx_pass;
    }

    return PJ_SUCCESS;
}

/*
 * Get number of candidates
 */
PJ_DEF(unsigned) pj_ice_strans_get_cands_count(pj_ice_strans *ice_st,
					       unsigned comp_id)
{
    unsigned i, cnt;

    PJ_ASSERT_RETURN(ice_st && ice_st->ice && comp_id &&
		     comp_id <= ice_st->comp_cnt, 0);

    cnt = 0;
    for (i=0; i<ice_st->ice->lcand_cnt; ++i) {
	if (ice_st->ice->lcand[i].comp_id != comp_id)
	    continue;
	++cnt;
    }

    return cnt;
}

/*
 * Enum candidates
 */
PJ_DEF(pj_status_t) pj_ice_strans_enum_cands(pj_ice_strans *ice_st,
					     unsigned comp_id,
					     unsigned *count,
					     pj_ice_sess_cand cand[])
{
    unsigned i, cnt;

    PJ_ASSERT_RETURN(ice_st && ice_st->ice && comp_id &&
		     comp_id <= ice_st->comp_cnt && count && cand, PJ_EINVAL);

    cnt = 0;
    for (i=0; i<ice_st->ice->lcand_cnt && cnt<*count; ++i) {
	if (ice_st->ice->lcand[i].comp_id != comp_id)
	    continue;
	pj_memcpy(&cand[cnt], &ice_st->ice->lcand[i],
		  sizeof(pj_ice_sess_cand));
	++cnt;
    }

    *count = cnt;
    return PJ_SUCCESS;
}

/*
 * Get default candidate.
 */
PJ_DEF(pj_status_t) pj_ice_strans_get_def_cand( pj_ice_strans *ice_st,
						unsigned comp_id,
						pj_ice_sess_cand *cand)
{
    const pj_ice_sess_check *valid_pair;

    PJ_ASSERT_RETURN(ice_st && comp_id && comp_id <= ice_st->comp_cnt &&
		      cand, PJ_EINVAL);

    valid_pair = pj_ice_strans_get_valid_pair(ice_st, comp_id);
    if (valid_pair) {
	pj_memcpy(cand, valid_pair->lcand, sizeof(pj_ice_sess_cand));
    } else {
	pj_ice_strans_comp *comp = ice_st->comp[comp_id - 1];
	pj_assert(comp->default_cand>=0 && comp->default_cand<comp->cand_cnt);
	pj_memcpy(cand, &comp->cand_list[comp->default_cand], 
		  sizeof(pj_ice_sess_cand));
    }
    return PJ_SUCCESS;
}

/*
 * Get the current ICE role.
 */
PJ_DEF(pj_ice_sess_role) pj_ice_strans_get_role(pj_ice_strans *ice_st)
{
    PJ_ASSERT_RETURN(ice_st && ice_st->ice, PJ_ICE_SESS_ROLE_UNKNOWN);
    return ice_st->ice->role;
}

/*
 * Change session role.
 */
PJ_DEF(pj_status_t) pj_ice_strans_change_role( pj_ice_strans *ice_st,
					       pj_ice_sess_role new_role)
{
    PJ_ASSERT_RETURN(ice_st && ice_st->ice, PJ_EINVALIDOP);
    return pj_ice_sess_change_role(ice_st->ice, new_role);
}

/* Timer callback to perform periodic check */
static void tcp_sess_check_timer(pj_timer_heap_t *th, 
						   pj_timer_entry *te)
{
	pj_status_t status;
	tcp_sess_timer_data *td;
	int td_count, td_timeout_count;
	int i;
	pj_tcp_sock *tcp_sock;
	pj_tcp_session *tcp_sess;

	td = (struct tcp_sess_timer_data*) te->user_data;
	td_count = td->count;
	tcp_sock = td->ice_st->comp[0]->tcp_sock;
	td_timeout_count = td->timeout_count;

	/* Set timer ID to FALSE first */
	te->id = PJ_FALSE;

	// keep tcp_sess pointer is newest.
	if (td_count < td_timeout_count && td->ice_st->ice && 
		td->ice_st->ice->role == PJ_ICE_SESS_ROLE_CONTROLLING) {
		pj_bool_t wait = PJ_TRUE;
		for (i = 0; i < MAX_TCP_CLIENT_SESS; i++)
		{
			tcp_sess = pj_tcp_sock_get_client_session_by_idx(tcp_sock, i);
			if (tcp_sess != NULL && pj_tcp_session_get_state(tcp_sess) == PJ_TCP_STATE_READY)
			{
				wait = PJ_FALSE;
				td->ice_st->ice->clist.checks[pj_tcp_session_get_idx(tcp_sess)].tcp_sess_ready = PJ_TRUE;
				break;
			}
		}
		if (wait) {
			/* Schedule for next timer */
			pj_ice_strans *ice_st = (pj_ice_strans *)td->ice_st;
			pj_time_val delay;

			if (te->id > 0) {
				pj_timer_heap_cancel(th, te);
				te->id = 0;
			}

			ice_st->tcp_sess_timer.id = PJ_FALSE;
			td = PJ_POOL_ZALLOC_T(ice_st->pool, tcp_sess_timer_data);
			td->ice_st = ice_st;
			td->count = ++td_count;
			td->timeout_count = td_timeout_count;
			ice_st->tcp_sess_timer.user_data = (void*)td;
			ice_st->tcp_sess_timer.cb = &tcp_sess_check_timer;

			ice_st->tcp_sess_timer.id = PJ_TRUE;
			delay.sec = 0;
			delay.msec = 10;
			status = pj_timer_heap_schedule(ice_st->ice->stun_cfg.timer_heap, 
				&ice_st->tcp_sess_timer, &delay);
			if (status != PJ_SUCCESS) {
				ice_st->tcp_sess_timer.id = PJ_FALSE;
				return;
			}

			return;
		} else {
			LOG4((td->ice_st->obj_name, "tcp_sess_check_timer() at least one tcp session ready for check."));
		}
	} else {
		PJ_LOG(3, (td->ice_st->obj_name, "tcp_sess_check_timer() all tcp session failed for check."));
	}

	if (td->ice_st && td->ice_st->ice && 
		td->ice_st->ice->cached_clist.count > 0)
	{
		/* Start ICE negotiation! */
		status = pj_ice_sess_start_check2(td->ice_st->ice, 1);
		if (status != PJ_SUCCESS) {
			pj_ice_strans_stop_ice(td->ice_st);
			return;
		}
	}
	else
	{
		// 2013-11-04 DEAN. 
		// We should check if ice is complete. 
		// Because it ice may be complete with triggered check.
        if (td->ice_st && td->ice_st->ice &&
            !td->ice_st->ice->is_complete)
		{
			/* Start ICE negotiation! */
			status = pj_ice_sess_start_check(td->ice_st->ice);
			if (status != PJ_SUCCESS) {
				pj_ice_strans_stop_ice(td->ice_st);
				return;
			}	
		}
	}
}

/*
 * Start ICE processing !
 */
PJ_DEF(pj_status_t) pj_ice_strans_start_ice( pj_ice_strans *ice_st,
                                             const pj_str_t *rem_ufrag,
                                             const pj_str_t *rem_passwd,
                                             unsigned rem_cand_cnt,
                                             const pj_ice_sess_cand rem_cand[])
{
    pj_status_t status;
    tcp_sess_timer_data *td;
    pj_time_val delay;
    int i;
    unsigned tcp_sess_cnt;
    pj_ice_strans_comp *comp;
	pj_tcp_sock_cb tcp_sock_cb;
	pj_bool_t try_to_connect_srflx_cand = PJ_FALSE;

    PJ_ASSERT_RETURN(ice_st && rem_ufrag && rem_passwd &&
                     rem_cand_cnt && rem_cand, PJ_EINVAL);

    /* Mark start time */
    pj_gettimeofday(&ice_st->start_time);


#if 0
	temp_cache = natnl_get_from_cache(&ice_st->dest_uri);
	if (temp_cache)
	{
		status = pj_ice_sess_create_cached_check_list(ice_st->ice, rem_ufrag, rem_passwd, temp_cache->check);
	}
	else
#endif
	{
		/* Build check list */
		status = pj_ice_sess_create_check_list(ice_st->ice, rem_ufrag, rem_passwd,
			rem_cand_cnt, rem_cand, ice_st->use_upnp_flag);
		if (status != PJ_SUCCESS)
			return status;
	}


    /* If we have TURN candidate, now is the time to create the permissions */
    if (ice_st->comp[0]->turn_sock) {
        unsigned i;

        for (i=0; i<ice_st->comp_cnt; ++i) {
            pj_sockaddr addrs[PJ_ICE_ST_MAX_CAND];
            unsigned j, count=0;
            comp = ice_st->comp[i];

            // Dean. if wait turn allocation flag is still signal, then destroy turn sock.
			if (pj_turn_session_state(pj_turn_sock_get_session(comp->turn_sock)) < PJ_TURN_STATE_READY || 
				ice_st->wait_turn_alloc_ok_cnt && comp->turn_sock) {
				PJ_LOG(4, ("ice_strans.c", "!!! TURN DEALLOCATE !!! in pj_ice_strans_start_ice() destroy turn_sock for ice_st->wait_turn_alloc_ok_cnt=%d.",
					ice_st->wait_turn_alloc_ok_cnt));
                pj_turn_sock_set_user_data(comp->turn_sock, NULL);
                pj_turn_sock_destroy(comp->turn_sock);
				comp->turn_sock = NULL;
				if (ice_st->wait_turn_alloc_ok_cnt > 0)
					ice_st->wait_turn_alloc_ok_cnt--;
                continue;
            }

            /* Gather remote addresses for this component */
            for (j=0; j<rem_cand_cnt && count<PJ_ARRAY_SIZE(addrs); ++j) {
				if (rem_cand[j].comp_id==i+1) {
                    pj_memcpy(&addrs[count++], &rem_cand[j].addr,
                              pj_sockaddr_get_len(&rem_cand[j].addr));
                }
            }

            if (count) {
                status = pj_turn_sock_set_perm(comp->turn_sock, count, addrs, 1);
                if (status != PJ_SUCCESS) {
                    pj_ice_strans_stop_ice(ice_st);
                    return status;
                }
            }
        }
	} else if (ice_st->comp[0]->turn_tcp_sock) {
		unsigned i;

		for (i=0; i<ice_st->comp_cnt; ++i) {
			pj_sockaddr addrs[PJ_ICE_ST_MAX_CAND];
			unsigned j, count=0;
			comp = ice_st->comp[i];

			// Dean. if wait turn allocation flag is still signal, then destroy turn sock.
			if (pj_turn_session_state(pj_turn_sock_get_session(comp->turn_tcp_sock)) < PJ_TURN_STATE_READY || 
				ice_st->wait_turn_alloc_ok_cnt && comp->turn_tcp_sock) {
				PJ_LOG(4, ("ice_strans.c", "!!! TURN DEALLOCATE !!! in pj_ice_strans_start_ice() destroy turn_sock for ice_st->wait_turn_alloc_ok_cnt=%d.",
					ice_st->wait_turn_alloc_ok_cnt));
				pj_turn_sock_set_user_data(comp->turn_tcp_sock, NULL);
				pj_turn_sock_destroy(comp->turn_tcp_sock);
				comp->turn_tcp_sock = NULL;
				if (ice_st->wait_turn_alloc_ok_cnt > 0)
					ice_st->wait_turn_alloc_ok_cnt--;
				continue;
			}

			/* Gather remote addresses for this component */
			for (j=0; j<rem_cand_cnt && count<PJ_ARRAY_SIZE(addrs); ++j) {
				if (rem_cand[j].comp_id==i+1) {
					// DEAN for assertion fail
					PJ_LOG(6, ("ice_strans.c", "pj_ice_strans_start_ice() pj_sockadre_get_len() addr->addr.sa_family=[%d]", 
						rem_cand[j].addr.addr.sa_family));
					pj_memcpy(&addrs[count++], &rem_cand[j].addr,
						pj_sockaddr_get_len(&rem_cand[j].addr));
				}
			}

			if (count) {
				status = pj_turn_sock_set_perm(comp->turn_tcp_sock, count, addrs, 1);
				if (status != PJ_SUCCESS) {
					pj_ice_strans_stop_ice(ice_st);
					return status;
				}
			}
		}
	}

	//2013-10-17 DEAN, dont do this here, but start every candidate tcp server at candidate adding moment.
	//2013-10-18 DEAN, do this here. 
	//2014-01-10 DEAN, dont do this here, but start every candidate tcp server at candidate adding moment.
#if 0
    /* Init TCP socket */
    comp = ice_st->comp[0]; //currently, we only focus on comp[0] ==> RTP 
    pj_bzero(&tcp_sock_cb, sizeof(tcp_sock_cb));
    tcp_sock_cb.on_rx_data = &tcp_on_rx_data;
    tcp_sock_cb.on_state = &tcp_on_state;
	tcp_sock_cb.on_server_created = &tcp_on_server_created;

    /* Override with component specific QoS settings, if any */
    if (ice_st->cfg.comp[comp->comp_id-1].qos_type) {
        ice_st->cfg.tcp.cfg.qos_type = ice_st->cfg.comp[comp->comp_id-1].qos_type;
    }
    if (ice_st->cfg.comp[comp->comp_id-1].qos_params.flags) {
        pj_memcpy(&ice_st->cfg.tcp.cfg.qos_params,
                  &ice_st->cfg.comp[comp->comp_id-1].qos_params,
                  sizeof(ice_st->cfg.tcp.cfg.qos_params));
    }

    /* Create the TCP transport */
    status = pj_tcp_sock_create(&ice_st->cfg.stun_cfg, ice_st->cfg.af,
                                &tcp_sock_cb,/* &ice_st->cfg.tcp.cfg,*/
                                comp, comp->comp_id, &comp->tcp_sock);
    if (status != PJ_SUCCESS) {
        return status;
    }

    status = natnl_create_tcp_server(ice_st->pool, comp->tcp_sock, 
										&comp->tcp_base_addr);
    if (status != PJ_SUCCESS) {
        //return status; // DEAN temporily ignore this error
    }
#endif

	tcp_sess_cnt = 0;
#if 1
    if (ice_st->ice->role == PJ_ICE_SESS_ROLE_CONTROLLING)
#endif
    {
        for (i=0; i<ice_st->comp_cnt; ++i) {
            pj_ice_strans_comp *comp = ice_st->comp[i];
            /* Try to build outgoing tcp sock connection to reflx remote addr */
            if (comp->tcp_sock) {
				unsigned j;
				// 2013-10-17 DEAN, handle cached check list first.
				for (j=0; j<ice_st->ice->cached_clist.count; ++j) {
					pj_ice_sess_check *check = &ice_st->ice->cached_clist.checks[j];
					pj_ice_sess_cand *rcand = check->rcand;
					if (rcand->comp_id==i+1 && 
						(rcand->type == PJ_ICE_CAND_TYPE_SRFLX_TCP ||
						rcand->type == PJ_ICE_CAND_TYPE_HOST_TCP)) {

							if (0 && rcand->type == PJ_ICE_CAND_TYPE_SRFLX_TCP && 
								rcand->tcp_type == PJ_ICE_CAND_TCP_TYPE_PASSIVE) {
									// DEAN check if stun's mapped public address is same with remote endpoint's.
									// If true it represent remote endpoint and ourself use same router.
									// So we don't make external connection.
									pj_sockaddr *pub_addr = pj_stun_sock_get_mapped_addr(comp->stun_sock);
									pj_sockaddr rem_pub_addr;
									pj_sockaddr_cp(&rem_pub_addr, &rcand->addr);
									pj_sockaddr_set_port(&rem_pub_addr, pj_sockaddr_get_port(pub_addr));
									if (pj_sockaddr_cmp(pub_addr, &rem_pub_addr) == 0)
										continue;

							}

							if (rcand->type == PJ_ICE_CAND_TYPE_SRFLX_TCP)
								try_to_connect_srflx_cand = PJ_TRUE;

							status = tcp_sock_make_connection(&ice_st->cfg.stun_cfg, 
								ice_st->cfg.af, comp->tcp_sock,                                                                        
								&rcand->addr, sizeof(pj_sockaddr_in),
								&check->tcp_sess_idx, 
								j);
							if (status != PJ_SUCCESS && status != PJ_EPENDING) {
								pj_ice_strans_stop_ice(ice_st);
								return status;
							}
							tcp_sess_cnt++;
					}
				}

				// 2013-10-17 DEAN, handle active check list.
				for (j=0; j<ice_st->ice->clist.count; ++j) {
					pj_ice_sess_check *check = &ice_st->ice->clist.checks[j];
					pj_ice_sess_cand *rcand = check->rcand;
					if (rcand->comp_id==i+1 && 
						 (rcand->type == PJ_ICE_CAND_TYPE_SRFLX_TCP ||
						 rcand->type == PJ_ICE_CAND_TYPE_HOST_TCP)) {

							 if (0 && rcand->type == PJ_ICE_CAND_TYPE_SRFLX_TCP && 
								 rcand->tcp_type == PJ_ICE_CAND_TCP_TYPE_PASSIVE) {
									 // DEAN check if stun's mapped public address is same with remote endpoint's.
									 // If true it represent remote endpoint and ourself use same router.
									 // So we don't make external connection.
									 pj_sockaddr *pub_addr = pj_stun_sock_get_mapped_addr(comp->stun_sock);
									 pj_sockaddr rem_pub_addr;
									 pj_sockaddr_cp(&rem_pub_addr, &rcand->addr);
									 pj_sockaddr_set_port(&rem_pub_addr, pj_sockaddr_get_port(pub_addr));
									 if (pj_sockaddr_cmp(pub_addr, &rem_pub_addr) == 0)
										 continue;

							 }

							 if (rcand->type == PJ_ICE_CAND_TYPE_SRFLX_TCP)
								 try_to_connect_srflx_cand = PJ_TRUE;

							status = tcp_sock_make_connection(&ice_st->cfg.stun_cfg, 
								ice_st->cfg.af, comp->tcp_sock,                                                                        
								&rcand->addr, sizeof(pj_sockaddr_in),
								&check->tcp_sess_idx, 
								j);
							if (status != PJ_SUCCESS && status != PJ_EPENDING) {
								pj_ice_strans_stop_ice(ice_st);
								return status;
							}
							tcp_sess_cnt++;
					}
                }
                pj_tcp_sock_set_stun_session_user_data(comp->tcp_sock, ice_st, comp->comp_id);
            }
        }
    }

    /* Start ICE negotiation! */
    PJ_LOG(4, (THIS_FILE, "pj_ice_strans_start_ice() tcp_sess_cnt=%d", tcp_sess_cnt));

    /**
     * comment the following code to have local host candidate have highest priority -- Andrew (2013/01/24)
	 * discard tcp check timer due to tcp candidate didn't be added -- Dean (2015/02/25)
     */
#if 1
	if (ice_st && ice_st->ice && 
		ice_st->ice->cached_clist.count > 0)
	{
		/* Start ICE negotiation! */
		status = pj_ice_sess_start_check2(ice_st->ice, 1);
		if (status != PJ_SUCCESS) {
			pj_ice_strans_stop_ice(ice_st);
			return status;
		}
	}
	else
	{
		// 2013-11-04 DEAN. 
		// We should check if ice is complete. 
		// Because it ice may be complete with triggered check.
		if (ice_st && ice_st->ice &&
			!ice_st->ice->is_complete)
		{
			/* Start ICE negotiation! */
			status = pj_ice_sess_start_check(ice_st->ice);
			if (status != PJ_SUCCESS) {
				pj_ice_strans_stop_ice(ice_st);
				return status;
			}	
		}
	}
#else
    {
        ice_st->tcp_sess_timer.id = PJ_FALSE;
        td = PJ_POOL_ZALLOC_T(ice_st->pool, tcp_sess_timer_data);
        td->ice_st = ice_st;
        td->count = 0;
		// DEAN. If we try to connect to srflx candidate, we give it more waiting time.
		if (try_to_connect_srflx_cand)
			td->timeout_count = 100;
		else
			td->timeout_count = 30;
		PJ_LOG(4, (THIS_FILE, "pj_ice_strans_start_ice(). Start a timer to check if tcp connection is success in %d ms", 
			td->timeout_count*10));
        ice_st->tcp_sess_timer.user_data = (void*)td;
        ice_st->tcp_sess_timer.cb = &tcp_sess_check_timer;

        ice_st->tcp_sess_timer.id = PJ_TRUE;
        delay.sec = delay.msec = 0;
        status = pj_timer_heap_schedule(ice_st->ice->stun_cfg.timer_heap, 
                                        &ice_st->tcp_sess_timer, &delay);
        if (status != PJ_SUCCESS) {
            ice_st->tcp_sess_timer.id = PJ_FALSE;
            return status;
        }
	}
#endif

    ice_st->state = PJ_ICE_STRANS_STATE_NEGO;
    return status;
}

/*
 * Get valid pair.
 */
PJ_DEF(const pj_ice_sess_check*)
pj_ice_strans_get_valid_pair(const pj_ice_strans *ice_st,
			     unsigned comp_id)
{
    PJ_ASSERT_RETURN(ice_st && comp_id && comp_id <= ice_st->comp_cnt,
		     NULL);

    if (ice_st->ice == NULL)
	return NULL;

    return ice_st->ice->comp[comp_id-1].valid_check;
}

/*
 * Get nominated pair.
 */
PJ_DEF(const pj_ice_sess_check*) 
pj_ice_strans_get_nominated_pair(const pj_ice_strans *ice_st,
			     unsigned comp_id)
{
    PJ_ASSERT_RETURN(ice_st && comp_id && comp_id <= ice_st->comp_cnt,
		     NULL);
    
    if (ice_st->ice == NULL)
	return NULL;
    
    return ice_st->ice->comp[comp_id-1].nominated_check;
}

/*
 * Stop ICE!
 */
PJ_DEF(pj_status_t) pj_ice_strans_stop_ice(pj_ice_strans *ice_st)
{
	int i;

    PJ_ASSERT_RETURN(ice_st, PJ_EINVAL);
    
    /* Protect with group lock, since this may cause race condition with
     * pj_ice_strans_sendto().
     * See ticket #1877.
     */
    pj_grp_lock_acquire(ice_st->grp_lock);

    if (ice_st->ice) {
        // To avoid hangup during making call and raise assertion.
	    pj_timer_heap_cancel_if_active(ice_st->ice->stun_cfg.timer_heap,
	                                   &ice_st->tcp_sess_timer, PJ_FALSE);
        pj_ice_sess_destroy(ice_st->ice);
        ice_st->ice = NULL;
    }

	/* Destroy all components */
	for (i=0; i<ice_st->comp_cnt; ++i) {
		if (ice_st->comp[i]) {
			if (ice_st->comp[i]->stun_sock) {
				PJ_LOG(4, ("ice_strans.c", "pj_ice_strans_stop_ice() destroy stun_sock."));
				pj_stun_sock_set_user_data(ice_st->comp[i]->stun_sock, NULL);
				pj_stun_sock_destroy(ice_st->comp[i]->stun_sock);
				ice_st->comp[i]->stun_sock = NULL;
			}

			if (ice_st->comp[i]->turn_sock) {
				PJ_LOG(4, ("ice_strans.c", "pj_ice_strans_stop_ice() destroy turn_sock."));
				pj_turn_sock_set_user_data(ice_st->comp[i]->turn_sock, NULL);
				pj_turn_sock_destroy(ice_st->comp[i]->turn_sock);
				ice_st->comp[i]->turn_sock = NULL;
			}

			if (ice_st->comp[i]->turn_tcp_sock) {
				PJ_LOG(4, ("ice_strans.c", "pj_ice_strans_stop_ice() destroy turn_tcp_sock."));
				pj_turn_sock_set_user_data(ice_st->comp[i]->turn_tcp_sock, NULL);
				pj_turn_sock_destroy(ice_st->comp[i]->turn_tcp_sock);
				ice_st->comp[i]->turn_tcp_sock = NULL;
			}

			if (ice_st->comp[i]->tcp_sock) {
				PJ_LOG(4, ("ice_strans.c", "pj_ice_strans_stop_ice() destroy tcp_sock."));
				pj_tcp_sock_destroy(ice_st->comp[i]->tcp_sock);
				ice_st->comp[i]->tcp_sock = NULL;
			}
		}
	}

    ice_st->state = PJ_ICE_STRANS_STATE_INIT;

    pj_grp_lock_release(ice_st->grp_lock);

    return PJ_SUCCESS;
}

/*
 * Application wants to send outgoing packet.
 */
PJ_DEF(pj_status_t) pj_ice_strans_sendto( pj_ice_strans *ice_st,
					  unsigned comp_id,
					  const void *data,
					  pj_size_t data_len,
					  const pj_sockaddr_t *dst_addr,
					  int dst_addr_len)
{
    pj_ssize_t pkt_size;
    pj_ice_strans_comp *comp;
    unsigned def_cand;
    pj_status_t status;

    PJ_ASSERT_RETURN(ice_st && comp_id && comp_id <= ice_st->comp_cnt &&
		     dst_addr && dst_addr_len, PJ_EINVAL);

    comp = ice_st->comp[comp_id-1];

    /* Check that default candidate for the component exists */
    def_cand = comp->default_cand;
    if (def_cand >= comp->cand_cnt)
	return PJ_EINVALIDOP;

    /* Protect with group lock, since this may cause race condition with
     * pj_ice_strans_stop_ice().
     * See ticket #1877.
     */
    pj_grp_lock_acquire(ice_st->grp_lock);

    /* If ICE is available, send data with ICE, otherwise send with the
     * default candidate selected during initialization.
     */
    if (ice_st->ice && ice_st->state == PJ_ICE_STRANS_STATE_RUNNING) {
	status = pj_ice_sess_send_data(ice_st->ice, comp_id, data, data_len);
	
	pj_grp_lock_release(ice_st->grp_lock);
	
	return status;

    } else if (comp->cand_list[def_cand].status == PJ_SUCCESS) {

    pj_grp_lock_release(ice_st->grp_lock);

	if (comp->cand_list[def_cand].type == PJ_ICE_CAND_TYPE_RELAYED) {

	    enum {
		msg_disable_ind = 0xFFFF &
				  ~(PJ_STUN_SESS_LOG_TX_IND|
				    PJ_STUN_SESS_LOG_RX_IND)
	    };

	    /* https://trac.pjsip.org/repos/ticket/1316 */
	    if (comp->turn_sock == NULL) {
		/* TURN socket error */
		return PJ_EINVALIDOP;
	    }

	    if (!comp->turn_log_off) {
		/* Disable logging for Send/Data indications */
		PJ_LOG(5,(ice_st->obj_name,
			  "Disabling STUN Indication logging for "
			  "component %d", comp->comp_id));
		pj_turn_sock_set_log(comp->turn_sock, msg_disable_ind);
		comp->turn_log_off = PJ_TRUE;
	    }

	    status = pj_turn_sock_sendto(comp->turn_sock, (const pj_uint8_t*)data, data_len,
					 dst_addr, dst_addr_len);
	    return (status==PJ_SUCCESS||status==PJ_EPENDING) ? 
		    PJ_SUCCESS : status;
	} else if (comp->cand_list[def_cand].type == PJ_ICE_CAND_TYPE_RELAYED_TCP) {

	    enum {
		msg_disable_ind = 0xFFFF &
				  ~(PJ_STUN_SESS_LOG_TX_IND|
				    PJ_STUN_SESS_LOG_RX_IND)
	    };

	    /* https://trac.pjsip.org/repos/ticket/1316 */
	    if (comp->turn_tcp_sock == NULL) {
		/* TURN socket error */
		return PJ_EINVALIDOP;
	    }

	    if (!comp->turn_log_off) {
		/* Disable logging for Send/Data indications */
		PJ_LOG(5,(ice_st->obj_name, 
			  "Disabling STUN Indication logging for "
			  "component %d", comp->comp_id));
		pj_turn_sock_set_log(comp->turn_tcp_sock, msg_disable_ind);

		comp->turn_log_off = PJ_TRUE;
	    }

		status = pj_turn_sock_sendto(comp->turn_tcp_sock, (const pj_uint8_t*)data, data_len,
			dst_addr, dst_addr_len);
	    return (status==PJ_SUCCESS||status==PJ_EPENDING) ? 
		    PJ_SUCCESS : status;
	} else {
		pkt_size = data_len;
		status = pj_stun_sock_sendto(comp->stun_sock, NULL, data, 
			data_len, 0, dst_addr, dst_addr_len);
	    return (status==PJ_SUCCESS||status==PJ_EPENDING) ? 
		    PJ_SUCCESS : status;
	}

    } else
	return PJ_EINVALIDOP;
}

/*
 * Callback called by ICE session when ICE processing is complete, either
 * successfully or with failure.
 */
static void on_ice_complete(pj_ice_sess *ice, pj_status_t status)
{
    pj_ice_strans *ice_st = (pj_ice_strans*)ice->user_data;
    pj_time_val t;
    unsigned msec;

	pj_sockaddr *turn_mapped_addr = NULL;
	
    pj_grp_lock_add_ref(ice_st->grp_lock);

    pj_gettimeofday(&t);
    PJ_TIME_VAL_SUB(t, ice_st->start_time);
    msec = PJ_TIME_VAL_MSEC(t);

    if (ice_st->cb.on_ice_complete) {
	if (status != PJ_SUCCESS) {
	    char errmsg[PJ_ERR_MSG_SIZE];
	    pj_strerror(status, errmsg, sizeof(errmsg));
	    PJ_LOG(4,(ice_st->obj_name,
		      "ICE negotiation failed after %ds:%03d: %s",
		      msec/1000, msec%1000, errmsg));
	} else {
	    unsigned i;
	    enum {
		msg_disable_ind = 0xFFFF &
				  ~(PJ_STUN_SESS_LOG_TX_IND|
				    PJ_STUN_SESS_LOG_RX_IND)
	    };

	    PJ_LOG(4,(ice_st->obj_name,
		      "ICE negotiation success after %ds:%03d",
		      msec/1000, msec%1000));

	    for (i=0; i<ice_st->comp_cnt; ++i) {
		const pj_ice_sess_check *check;

		check = pj_ice_strans_get_nominated_pair(ice_st, i+1);
		if (check) {
			char lip[PJ_INET6_ADDRSTRLEN+10];
			char rip[PJ_INET6_ADDRSTRLEN+10];

		    pj_sockaddr_print(&check->lcand->addr, lip, 
				      sizeof(lip), 3);
		    pj_sockaddr_print(&check->rcand->addr, rip, 
				sizeof(rip), 3);

		    if (check->lcand->transport_id == TP_TURN) {
				//DEAN, set tunnel type, if will be used in natnl_adapter.
				if (i == 0)
					ice_st->tunnel_type = NATNL_TUNNEL_TYPE_TURN;
				
				/* Activate channel binding for the remote address
				 * for more efficient data transfer using TURN.
				 */
				status = pj_turn_sock_bind_channel(
						ice_st->comp[i]->turn_sock,
						&check->rcand->addr,
						sizeof(check->rcand->addr));

				/* Disable logging for Send/Data indications */
				PJ_LOG(5,(ice_st->obj_name, 
					  "Disabling STUN Indication logging for "
					  "component %d", i+1));
				pj_turn_sock_set_log(ice_st->comp[i]->turn_sock,
							 msg_disable_ind);

				// dean : save turn mapped address.
				turn_mapped_addr = &check->lcand->rel_addr;

				// Close useless resouces
				//if (ice_st->comp[i]->tcp_sock) {
				//	pj_tcp_sock_destroy(ice_st->comp[i]->tcp_sock);
				//	ice_st->comp[i]->tcp_sock = NULL;
				//}
				ice_st->comp[i]->turn_log_off = PJ_TRUE;

				// Close useless resources
				if (ice_st->comp[i]->stun_sock) {
					PJ_LOG(4, ("ice_strans.c", "on_ice_complete() destroy stun_sock for choosing TURN_UDP."));
					pj_stun_sock_destroy(ice_st->comp[i]->stun_sock);
					ice_st->comp[i]->stun_sock = NULL;
				}
				if (ice_st->comp[i]->tcp_sock) {
					PJ_LOG(4, ("ice_strans.c", "on_ice_complete() destroy tcp_sock for choosing TURN_UDP."));
					pj_tcp_sock_destroy(ice_st->comp[i]->tcp_sock);
					ice_st->comp[i]->tcp_sock = NULL;
				}
				if (ice_st->comp[i]->turn_tcp_sock) {
					PJ_LOG(4, ("ice_strans.c", "on_ice_complete() destroy turn_tcp_sock for choosing TURN_UDP."));
					pj_turn_sock_destroy(ice_st->comp[i]->turn_tcp_sock);
					ice_st->comp[i]->turn_tcp_sock = NULL;
				}
			} else if (check->lcand->transport_id == TP_TURN_TCP) {
				//DEAN, set tunnel type, if will be used in natnl_adapter.
				if (i == 0)
					ice_st->tunnel_type = NATNL_TUNNEL_TYPE_TURN;
				
				/* Activate channel binding for the remote address
				 * for more efficient data transfer using TURN.
				 */
				status = pj_turn_sock_bind_channel(
						ice_st->comp[i]->turn_tcp_sock,
						&check->rcand->addr,
						sizeof(check->rcand->addr));

				/* Disable logging for Send/Data indications */
				PJ_LOG(5,(ice_st->obj_name, 
					  "Disabling STUN Indication logging for "
					  "component %d", i+1));
				pj_turn_sock_set_log(ice_st->comp[i]->turn_tcp_sock,
							 msg_disable_ind);

				// dean : save turn mapped address.
				turn_mapped_addr = &check->lcand->rel_addr;

				// Close useless resouces
				//if (ice_st->comp[i]->tcp_sock) {
				//	pj_tcp_sock_destroy(ice_st->comp[i]->tcp_sock);
				//	ice_st->comp[i]->tcp_sock = NULL;
				//}
				ice_st->comp[i]->turn_log_off = PJ_TRUE;

				// Close useless resources
				if (ice_st->comp[i]->stun_sock) {
					PJ_LOG(4, ("ice_strans.c", "on_ice_complete() destroy stun_sock for choosing TURN_TCP."));
					pj_stun_sock_destroy(ice_st->comp[i]->stun_sock);
					ice_st->comp[i]->stun_sock = NULL;
				}
				if (ice_st->comp[i]->tcp_sock) {
					PJ_LOG(4, ("ice_strans.c", "on_ice_complete() destroy tcp_sock for choosing TURN_TCP."));
					pj_tcp_sock_destroy(ice_st->comp[i]->tcp_sock);
					ice_st->comp[i]->tcp_sock = NULL;
				}
				if (ice_st->comp[i]->turn_sock) {
					PJ_LOG(4, ("ice_strans.c", "on_ice_complete() destroy turn_sock for choosing TURN_TCP."));
					pj_turn_sock_destroy(ice_st->comp[i]->turn_sock);
					ice_st->comp[i]->turn_sock = NULL;
				}
			} else if (check->lcand->transport_id == TP_UPNP_TCP) { //DEAN
				//DEAN, set tunnel type, if will be used in natnl_adapter.
				if (i == 0)
					ice_st->tunnel_type = NATNL_TUNNEL_TYPE_UPNP_TCP;
				
				// Close useless resources
				if (ice_st->comp[i]->stun_sock) {
					PJ_LOG(4, ("ice_strans.c", "on_ice_complete() destroy stun_sock for choosing UPNP_TCP."));
					pj_stun_sock_destroy(ice_st->comp[i]->stun_sock);
					ice_st->comp[i]->stun_sock = NULL;
				}
				if (ice_st->comp[i]->turn_sock) {
					PJ_LOG(4, ("ice_strans.c", "on_ice_complete() destroy turn_sock for choosing UPNP_TCP."));
					pj_turn_sock_destroy(ice_st->comp[i]->turn_sock);
					ice_st->comp[i]->turn_sock = NULL;
				}
				if (ice_st->comp[i]->turn_tcp_sock) {
					PJ_LOG(4, ("ice_strans.c", "on_ice_complete() destroy turn_tcp_sock for choosing UPNP_TCP."));
					pj_turn_sock_destroy(ice_st->comp[i]->turn_tcp_sock);
					ice_st->comp[i]->turn_tcp_sock = NULL;
				}
			} else if (check->lcand->transport_id == TP_STUN) { //DEAN
				//DEAN, set tunnel type, if will be used in natnl_adapter.
				if (i == 0)
					ice_st->tunnel_type = NATNL_TUNNEL_TYPE_UDP;

				// Close useless resources
				if (ice_st->comp[i]->tcp_sock) {
					PJ_LOG(4, ("ice_strans.c", "on_ice_complete() destroy tcp_sock for choosing UDP."));
					pj_tcp_sock_destroy(ice_st->comp[i]->tcp_sock);
					ice_st->comp[i]->tcp_sock = NULL;
				}
				if (ice_st->comp[i]->turn_sock) {
					PJ_LOG(4, ("ice_strans.c", "on_ice_complete() destroy turn_sock for choosing UDP."));
					pj_turn_sock_destroy(ice_st->comp[i]->turn_sock);
					ice_st->comp[i]->turn_sock = NULL;
				}
				if (ice_st->comp[i]->turn_tcp_sock) {
					PJ_LOG(4, ("ice_strans.c", "on_ice_complete() destroy turn_tcp_sock for choosing UDP."));
					pj_turn_sock_destroy(ice_st->comp[i]->turn_tcp_sock);
					ice_st->comp[i]->turn_tcp_sock = NULL;
				}
			}

		    PJ_LOG(4,(ice_st->obj_name, " Comp %d: "
			      "sending from %s candidate %s to "
			      "%s candidate %s",
			      i+1,
			      pj_ice_get_cand_type_name(check->lcand->type),
			      lip,
			      pj_ice_get_cand_type_name(check->rcand->type),
			      rip));

		} else {
		    PJ_LOG(4,(ice_st->obj_name,
			      "Comp %d: disabled", i+1));
		}
	    }
	}

	ice_st->state = (status==PJ_SUCCESS) ? PJ_ICE_STRANS_STATE_RUNNING :
					       PJ_ICE_STRANS_STATE_FAILED;

	(*ice_st->cb.on_ice_complete)(ice_st, PJ_ICE_STRANS_OP_NEGOTIATION, 
				      status, turn_mapped_addr);

    }

    pj_grp_lock_dec_ref(ice_st->grp_lock);
}

/*
 * Callback called by ICE session when it wants to send outgoing packet.
 */
static pj_status_t ice_tx_pkt(pj_ice_sess *ice,
			      unsigned comp_id,
			      unsigned transport_id,
			      const void *pkt, pj_size_t size,
			      const pj_sockaddr_t *dst_addr,
			      unsigned dst_addr_len,
				  int tcp_sess_idx)
{
    pj_ice_strans *ice_st = (pj_ice_strans*)ice->user_data;
    pj_ice_strans_comp *comp;
    pj_status_t status;

    PJ_ASSERT_RETURN(comp_id && comp_id <= ice_st->comp_cnt, PJ_EINVAL);

    comp = ice_st->comp[comp_id-1];

    TRACE_PKT((comp->ice_st->obj_name, 
	      "Component %d TX packet to %s:%d with transport %d",
	      comp_id, 
	      pj_inet_ntoa(((pj_sockaddr_in*)dst_addr)->sin_addr),
	      (int)pj_ntohs(((pj_sockaddr_in*)dst_addr)->sin_port),
	      transport_id));

	if (transport_id == TP_TURN) {
		if (comp->turn_sock) {
			status = pj_turn_sock_sendto(comp->turn_sock, 
				(const pj_uint8_t*)pkt, size,
				dst_addr, dst_addr_len);
		} else {
			status = PJ_EINVALIDOP;
		}
	} else if (transport_id == TP_TURN_TCP) {
		if (comp->turn_tcp_sock) {
			status = pj_turn_sock_sendto(comp->turn_tcp_sock, 
				(const pj_uint8_t*)pkt, size,
				dst_addr, dst_addr_len);
		} else {
			status = PJ_EINVALIDOP;
		}
	} else if (transport_id == TP_UPNP_TCP) {
		if (comp->tcp_sock) {
			status = pj_tcp_sock_sendto(comp->tcp_sock, 
				(const pj_uint8_t*)pkt, size,
				dst_addr, dst_addr_len, tcp_sess_idx);
		} else {
			status = PJ_EINVALIDOP;
		}
	} else if (transport_id == TP_STUN) {
		status = pj_stun_sock_sendto(comp->stun_sock, NULL, 
			pkt, size, 0,
			dst_addr, dst_addr_len);
	} else {
		pj_assert(!"Invalid transport ID");
		status = PJ_EINVALIDOP;
	}
    
    return (status==PJ_SUCCESS||status==PJ_EPENDING) ? PJ_SUCCESS : status;
}

/*
 * Callback called by ICE session when it wants to send outgoing packet.
 */
static pj_status_t ice_tcp_tx_pkt(pj_ice_sess *ice, 
			      unsigned comp_id, 
			      unsigned transport_id,
			      const void *pkt, pj_size_t size,
			      const pj_sockaddr_t *dst_addr,
			      unsigned dst_addr_len,
				  void *user_data,
				  int tcp_sess_idx)
{
    pj_ice_strans *ice_st = (pj_ice_strans*)ice->user_data;
    pj_ice_strans_comp *comp;
    pj_status_t status;

    PJ_ASSERT_RETURN(comp_id && comp_id <= ice_st->comp_cnt, PJ_EINVAL);

    comp = ice_st->comp[comp_id-1];

    TRACE_PKT((comp->ice_st->obj_name, 
	      "Component %d TX packet to %s:%d with transport %d",
	      comp_id, 
	      pj_inet_ntoa(((pj_sockaddr_in*)dst_addr)->sin_addr),
	      (int)pj_ntohs(((pj_sockaddr_in*)dst_addr)->sin_port),
	      transport_id));

	if (transport_id == TP_TURN) {
		if (comp->turn_sock) {
			status = pj_turn_sock_sendto(comp->turn_sock, 
				(const pj_uint8_t*)pkt, size,
				dst_addr, dst_addr_len);
		} else {
			status = PJ_EINVALIDOP;
		}
	} else if (transport_id == TP_TURN) {
		if (comp->turn_tcp_sock) {
			status = pj_turn_sock_sendto(comp->turn_tcp_sock, 
				(const pj_uint8_t*)pkt, size,
				dst_addr, dst_addr_len);
		} else {
			status = PJ_EINVALIDOP;
		}
	} else if (transport_id == TP_UPNP_TCP) {
		if (comp->tcp_sock) {
			status = pj_tcp_sock_sendto(user_data, 
				(const pj_uint8_t*)pkt, size,
				dst_addr, dst_addr_len, tcp_sess_idx);
		} else {
			status = PJ_EINVALIDOP;
		}
	} else if (transport_id == TP_STUN) {
		status = pj_stun_sock_sendto(comp->stun_sock, NULL, 
			pkt, size, 0,
			dst_addr, dst_addr_len);
	} else {
		pj_assert(!"Invalid transport ID");
		status = PJ_EINVALIDOP;
	}
    
    return (status==PJ_SUCCESS||status==PJ_EPENDING) ? PJ_SUCCESS : status;
}

/*
 * Callback called by ICE session when it receives application data.
 */
static void ice_rx_data(pj_ice_sess *ice,
		        unsigned comp_id,
			unsigned transport_id,
		        void *pkt, pj_size_t size,
		        const pj_sockaddr_t *src_addr,
		        unsigned src_addr_len)
{
    pj_ice_strans *ice_st = (pj_ice_strans*)ice->user_data;

    PJ_UNUSED_ARG(transport_id);

    if (ice_st->cb.on_rx_data) {
	(*ice_st->cb.on_rx_data)(ice_st, comp_id, pkt, size,
				 src_addr, src_addr_len);
    }
}

/* Notification when incoming packet has been received from
 * the STUN socket.
 */
static pj_bool_t stun_on_rx_data(pj_stun_sock *stun_sock,
				 void *pkt,
				 unsigned pkt_len,
				 const pj_sockaddr_t *src_addr,
				 unsigned addr_len)
{
    pj_ice_strans_comp *comp;
    pj_ice_strans *ice_st;
    pj_status_t status;

    comp = (pj_ice_strans_comp*) pj_stun_sock_get_user_data(stun_sock);
    if (comp == NULL) {
	/* We have disassociated ourselves from the STUN socket */
	return PJ_FALSE;
    }

    ice_st = comp->ice_st;

    pj_grp_lock_add_ref(ice_st->grp_lock);

    if (ice_st->ice == NULL) {
	/* The ICE session is gone, but we're still receiving packets.
	 * This could also happen if remote doesn't do ICE. So just
	 * report this to application.
	 */
	if (ice_st->cb.on_rx_data) {
	    (*ice_st->cb.on_rx_data)(ice_st, comp->comp_id, pkt, pkt_len,
				     src_addr, addr_len);
	}

    } else {

	/* Hand over the packet to ICE session */
	status = pj_ice_sess_on_rx_pkt(comp->ice_st->ice, comp->comp_id,
				       TP_STUN, pkt, pkt_len,
				       src_addr, addr_len);

	if (status != PJ_SUCCESS) {
	    ice_st_perror(comp->ice_st, "Error processing packet",
			  status);
	}
    }

    return pj_grp_lock_dec_ref(ice_st->grp_lock) ? PJ_FALSE : PJ_TRUE;
}

/* Notifification when asynchronous send operation to the STUN socket
 * has completed.
 */
static pj_bool_t stun_on_data_sent(pj_stun_sock *stun_sock,
				   pj_ioqueue_op_key_t *send_key,
				   pj_ssize_t sent)
{
    PJ_UNUSED_ARG(stun_sock);
    PJ_UNUSED_ARG(send_key);
    PJ_UNUSED_ARG(sent);
    return PJ_TRUE;
}

static natnl_addr_changed_type stun_compare_and_update_local_addr(pj_stun_sock *stun_sock)
{
	pj_sockaddr *previous_local_addr = pj_stun_sock_get_previous_local_addr(stun_sock);
	pj_stun_sock_info info;
	char ipaddr[PJ_INET6_ADDRSTRLEN+10];
	natnl_addr_changed_type changed_type = 0;

	if (pj_sockaddr_has_addr(previous_local_addr))
	{
		PJ_LOG(6,("ice_strans.c", 
			"%%%%%% previous address is %s",
			pj_sockaddr_print(previous_local_addr, ipaddr, 
			sizeof(ipaddr), 3)));

		pj_stun_sock_get_info(stun_sock, &info);
		if (pj_sockaddr_has_addr(&info.local_addr)) {
			PJ_LOG(6,("ice_strans.c", 
				"%%%%%% current address is %s",
				pj_sockaddr_print(&info.local_addr, ipaddr, 
				sizeof(ipaddr), 3)));

			if (pj_sockaddr_cmp(previous_local_addr, &info.local_addr) != 0) {
				changed_type |= ADDR_CHANGED_TYPE_LOCAL;
				//(*ice_st->cb.on_ice_complete)(ice_st, PJ_ICE_STRANS_OP_ADDRESS_CHANGED, 
				//	PJ_SUCCESS);
				pj_stun_sock_set_previous_local_addr(stun_sock, &info.local_addr);
			}
		}
	}
	else
	{
		pj_stun_sock_get_info(stun_sock, &info);
		if (pj_sockaddr_has_addr(&info.local_addr))
		{
			PJ_LOG(6, ("ice_strans.c", 
				"%%%%%% current address is %s",
				pj_sockaddr_print(&info.local_addr, ipaddr, 
				sizeof(ipaddr), 3)));
			pj_stun_sock_set_previous_local_addr(stun_sock, &info.local_addr);
		}
	}
	return changed_type;
}

/* Notification when the status of the STUN transport has changed. */
static pj_bool_t stun_on_status(pj_stun_sock *stun_sock,
				pj_stun_sock_op op,
				pj_status_t status)
{
    pj_ice_strans_comp *comp;
    pj_ice_strans *ice_st;
    pj_ice_sess_cand *cand = NULL;
    pj_ice_sess_cand *tcp_cand = NULL;
	unsigned i;
	//pj_stun_sock_info info;

	//STUN packet will be marked as PJ_IOQUEUE_URGENT_DATA, so pending is allowed.
    //pj_assert(status != PJ_EPENDING);

    comp = (pj_ice_strans_comp*) pj_stun_sock_get_user_data(stun_sock);
    ice_st = comp->ice_st;

    pj_grp_lock_add_ref(ice_st->grp_lock);

    /* Wait until initialization completes */
    pj_grp_lock_acquire(ice_st->grp_lock);

    /* Find the srflx UDP cancidate */
    for (i=0; i<comp->cand_cnt; ++i) {
		if (comp->cand_list[i].type == PJ_ICE_CAND_TYPE_SRFLX) {
			if (comp->cand_list[i].transport_id == TP_STUN)
				cand = &comp->cand_list[i];
		}
		if (cand)
			break;
	}
    
    pj_grp_lock_release(ice_st->grp_lock);

    /* It is possible that we don't have srflx candidate even though this
     * callback is called. This could happen when we cancel adding srflx
     * candidate due to initialization error.
     */
    if (cand == NULL) {
	PJ_LOG(1, (THIS_FILE, "stun_on_status() cand is NULL"));
	return pj_grp_lock_dec_ref(ice_st->grp_lock) ? PJ_FALSE : PJ_TRUE;
    }

    switch (op) {
    case PJ_STUN_SOCK_DNS_OP:
	if (status != PJ_SUCCESS) {
	    /* May not have cand, e.g. when error during init */
		if (cand) {
			cand->status = status;
			pj_gettimeofday(&cand->ending_time);
		}
	    sess_fail(ice_st, PJ_ICE_STRANS_OP_INIT, "DNS resolution failed", 
		      status);
	}
	break;
	case PJ_STUN_SOCK_BINDING_OP:
#if 0 // 2014-10-25 no need ip changed detection anymore.
		stun_compare_and_update_local_addr(stun_sock);
#endif
	case PJ_STUN_SOCK_MAPPED_ADDR_CHANGE:
		PJ_LOG(4, (THIS_FILE, "stun_on_status() PJ_STUN_SOCK_MAPPED_ADDR_CHANGE status=[%d]", status));
		if (status == PJ_SUCCESS) { // DEAN. If error occur, don't do this. Or there is assertion terminate.
		//if (1) {
			// 2015-03-05. Don't use pj_stun_sock_get_info to get mapped_addr
			//status = pj_stun_sock_get_info(stun_sock, &info);
			//PJ_LOG(4, (THIS_FILE, "stun_on_status() pj_stun_sock_get_info returned. status=[%d]", status));

			//if (status == PJ_SUCCESS) 
			pj_sockaddr *mapped_addr = pj_stun_sock_get_mapped_addr(stun_sock);
			if (pj_sockaddr_has_addr(mapped_addr))
			{
				char ipaddr[PJ_INET6_ADDRSTRLEN+10];
				const char *op_name = (op==PJ_STUN_SOCK_BINDING_OP) ?
							"Binding discovery complete" :
							"srflx address changed";
				const char *op_name_tcp = (op==PJ_STUN_SOCK_BINDING_OP) ?
							"Binding discovery complete" :
							"tcp srflx address changed";
				pj_bool_t dup = PJ_FALSE;

				/* Eliminate the srflx candidate if the address is
				 * equal to other (host) candidates.
				 */
				for (i=0; i<comp->cand_cnt; ++i) {
					if (comp->cand_list[i].type == PJ_ICE_CAND_TYPE_HOST &&
						comp->cand_list[i].transport_id == TP_STUN && 
					pj_sockaddr_cmp(&comp->cand_list[i].addr,
							mapped_addr) == 0)
					{
					dup = PJ_TRUE;
					break;
					}
				}

				if (dup) {
					/* Duplicate found, remove the srflx UDP candidate */
					unsigned idx = cand - comp->cand_list;

					/* Update default candidate index */
					if (comp->default_cand > idx) {
					--comp->default_cand;
					} else if (comp->default_cand == idx) {
					comp->default_cand = 0;
					}

					/* Remove srflx candidate */
					pj_array_erase(comp->cand_list, sizeof(comp->cand_list[0]),
						   comp->cand_cnt, idx);
					--comp->cand_cnt;
				} else {
					/* Otherwise update the address */
					if (pj_sockaddr_has_addr(mapped_addr)) { // To avoid assertion fail.
						pj_sockaddr_cp(&cand->addr, mapped_addr);
						cand->status = PJ_SUCCESS;
						pj_gettimeofday(&cand->ending_time);
					}
				}

				PJ_LOG(4,(comp->ice_st->obj_name, 
					  "Comp %d: %s, "
					  "srflx address is %s",
					  comp->comp_id, op_name, 
					  pj_sockaddr_print(mapped_addr, ipaddr, 
								 sizeof(ipaddr), 3)));

#ifdef COLLECT_TCP_CAND
				dup = PJ_FALSE;

				/* Find the srflx TCP candidate */
				for (i=0; i<comp->cand_cnt; ++i) {
					if (comp->cand_list[i].type == PJ_ICE_CAND_TYPE_SRFLX_TCP) {
						if (comp->cand_list[i].transport_id == TP_UPNP_TCP)
							tcp_cand = &comp->cand_list[i];
					}
					if (tcp_cand)
						break;
				}

				/* Eliminate the srflx TCP candidate if the address is
				 * equal to other (host) candidates.
				 */
				for (i=0; i<comp->cand_cnt; ++i) {
					if (comp->cand_list[i].type == PJ_ICE_CAND_TYPE_HOST_TCP &&
						comp->cand_list[i].transport_id == TP_UPNP_TCP && 
					pj_sockaddr_cmp(&comp->cand_list[i].addr,
							&info.mapped_addr) == 0)
					{
					dup = PJ_TRUE;
					break;
					}
				}

				if (dup) {
					/* Duplicate found, remove the srflx UDP candidate */
					unsigned idx = tcp_cand - comp->cand_list;

					/* Update default candidate index */
					if (comp->default_cand > idx) {
					--comp->default_cand;
					} else if (comp->default_cand == idx) {
					comp->default_cand = 0;
					}

					/* Remove srflx candidate */
					pj_array_erase(comp->cand_list, sizeof(comp->cand_list[0]),
						   comp->cand_cnt, idx);
					--comp->cand_cnt;
				} else {
					/* Otherwise update the address */
					if (tcp_cand != NULL &&
						!tcp_cand->use_user_port) { // not use user port
						pj_sockaddr_cp(&tcp_cand->addr, &info.mapped_addr);
						tcp_cand->status = PJ_SUCCESS;
					} else if (tcp_cand != NULL && 
						tcp_cand->use_user_port) {
							pj_uint16_t port = pj_sockaddr_get_port(&tcp_cand->addr);
						pj_sockaddr_cp(&tcp_cand->addr, &info.mapped_addr);
						pj_sockaddr_set_port(&tcp_cand->addr, port);
						tcp_cand->status = PJ_SUCCESS;
					}
				}

				if (tcp_cand != NULL) {
					PJ_LOG(4,(comp->ice_st->obj_name, 
						  "Comp %d: %s, "
						  "srflx address is %s",
						  comp->comp_id, op_name_tcp, 
						  pj_sockaddr_print(&tcp_cand->addr, ipaddr, 
						  sizeof(ipaddr), 3)));
				} else {
					PJ_LOG(2, (THIS_FILE, "stun_on_status() tcp_cand is NULL."));
				}
#endif
				sess_init_update(ice_st);
			}

		} else if (status != PJ_SUCCESS) {
			/* May not have cand, e.g. when error during init */
			if (cand) {
				cand->status = status;
				pj_gettimeofday(&cand->ending_time);
			}
			sess_fail(ice_st, PJ_ICE_STRANS_OP_INIT, 
				  "STUN binding request failed", status);
		}
		break;
	case PJ_STUN_SOCK_KEEP_ALIVE_OP:
#if 0 //2014-10-25 discarded. no need ip change detection anymore.
		{ // 2013-03-21 DEAN Added, for check local address changed.
			natnl_addr_changed_type changed_type = stun_compare_and_update_local_addr(stun_sock);
			ice_st->addr_changed_type |= changed_type;
	}
#endif

	if (status != PJ_SUCCESS) {
		if (cand) {
			cand->status = status;
			pj_gettimeofday(&cand->ending_time);
		}
#if 0   // DEAN Discard this, because this may cause SDK notify every call's KA_TIMEOUT on unpluging ehternet.
		// DEAN modified for keep-alive failed notification
	    sess_fail(ice_st, PJ_ICE_STRANS_OP_KEEP_ALIVE, 
		      "STUN keep-alive failed", status);
#endif
	}
	break;
	}
#if 0 //2014-10-25 discarded. no need ip change detection anymore.
	status = pj_stun_sock_get_info(stun_sock, &info);
	if (status == PJ_SUCCESS && 
		(op == PJ_STUN_SOCK_BINDING_OP || 
		op == PJ_STUN_SOCK_MAPPED_ADDR_CHANGE || 
		ice_st->addr_changed_type > 0) &&
		tcp_cand != NULL)
	{
		// 2013-05-20 call callback to set upnp port mapping.
		if (ice_st->cb.on_stun_binding_complete)
			(*ice_st->cb.on_stun_binding_complete)(ice_st, 
			&tcp_cand->base_addr, 
			ice_st->addr_changed_type);
	}

	// Call callback if there is ip changed.
	if (ice_st->addr_changed_type > ADDR_CHANGED_TYPE_NONE)
		(*ice_st->cb.on_ice_complete)(ice_st, PJ_ICE_STRANS_OP_ADDRESS_CHANGED, 
		PJ_SUCCESS);
#endif

    return pj_grp_lock_dec_ref(ice_st->grp_lock)? PJ_FALSE : PJ_TRUE;
}

/* Callback when TURN socket has received a packet */
static void turn_on_rx_data(pj_turn_sock *turn_sock,
			    void *pkt,
			    unsigned pkt_len,
			    const pj_sockaddr_t *peer_addr,
			    unsigned addr_len)
{
    pj_ice_strans_comp *comp;
    pj_status_t status;

    comp = (pj_ice_strans_comp*) pj_turn_sock_get_user_data(turn_sock);
    if (comp == NULL) {
	/* We have disassociated ourselves from the TURN socket */
	return;
    }

    pj_grp_lock_add_ref(comp->ice_st->grp_lock);

    if (comp->ice_st->ice == NULL) {
	/* The ICE session is gone, but we're still receiving packets.
	 * This could also happen if remote doesn't do ICE and application
	 * specifies TURN as the default address in SDP.
	 * So in this case just give the packet to application.
	 */
	if (comp->ice_st->cb.on_rx_data) {
	    (*comp->ice_st->cb.on_rx_data)(comp->ice_st, comp->comp_id, pkt,
					   pkt_len, peer_addr, addr_len);
	}

    } else {

	/* Hand over the packet to ICE */
		if (pj_turn_sock_get_conn_type(turn_sock) == PJ_TURN_TP_UDP)
			status = pj_ice_sess_on_rx_pkt(comp->ice_st->ice, comp->comp_id,
				       TP_TURN, pkt, pkt_len,
				       peer_addr, addr_len);
		else
			status = pj_ice_sess_on_rx_pkt(comp->ice_st->ice, comp->comp_id,
						TP_TURN_TCP, pkt, pkt_len,
						peer_addr, addr_len);

	if (status != PJ_SUCCESS) {
	    ice_st_perror(comp->ice_st,
			  "Error processing packet from TURN relay",
			  status);
	}
    }

    pj_grp_lock_dec_ref(comp->ice_st->grp_lock);
}


/* Callback when TURN client state has changed */
static void turn_on_state(pj_turn_sock *turn_sock, pj_turn_state_t old_state,
			  pj_turn_state_t new_state)
{
    pj_ice_strans_comp *comp;

    comp = (pj_ice_strans_comp*) pj_turn_sock_get_user_data(turn_sock);
    if (comp == NULL) {
	/* Not interested in further state notification once the relay is
	 * disconnecting.
	 */
	return;
    }

    PJ_LOG(5,(comp->ice_st->obj_name, "TURN client state changed %s --> %s",
	      pj_turn_state_name(old_state), pj_turn_state_name(new_state)));

    pj_grp_lock_add_ref(comp->ice_st->grp_lock);

    if (new_state == PJ_TURN_STATE_READY) {
	pj_turn_session_info rel_info;
	char ipaddr[PJ_INET6_ADDRSTRLEN+8];
	pj_ice_sess_cand *cand = NULL;
	unsigned i;

	comp->turn_err_cnt = 0;

	/* Get allocation info */
	pj_turn_sock_get_info(turn_sock, &rel_info);

	/* Wait until initialization completes */
	pj_grp_lock_acquire(comp->ice_st->grp_lock);

	/* Find relayed candidate in the component */
	for (i=0; i<comp->cand_cnt; ++i) {
	    if (comp->cand_list[i].type == PJ_ICE_CAND_TYPE_RELAYED || 
			comp->cand_list[i].type == PJ_ICE_CAND_TYPE_RELAYED_TCP) {
		cand = &comp->cand_list[i];
		break;
	    }
	}

	pj_grp_lock_release(comp->ice_st->grp_lock);

	if (!cand) {
		return;
	}

	pj_gettimeofday(&cand->ending_time);
#if 1
	if (cand->status == PJ_SUCCESS && 
		pj_turn_sock_get_conn_type(turn_sock) == PJ_TURN_TP_TCP) {
		PJ_LOG(4, (comp->ice_st->obj_name, "turn_on_state() destroy turn_tcp_sock for previous turn udp success."));
		if (comp->turn_tcp_sock) {
			pj_turn_sock_destroy(comp->turn_tcp_sock);
			comp->turn_tcp_sock = NULL;
		}
		return;
	} else if (cand->status == PJ_SUCCESS && 
		pj_turn_sock_get_conn_type(turn_sock) == PJ_TURN_TP_UDP) {
		PJ_LOG(4, (comp->ice_st->obj_name, "turn_on_state() destroy turn_tcp_sock for current turn udp success."));
		if (comp->turn_tcp_sock) {
			pj_turn_sock_destroy(comp->turn_tcp_sock);
			comp->turn_tcp_sock = NULL;
		}
	}
#endif

	/* Update candidate */
	pj_sockaddr_cp(&cand->addr, &rel_info.relay_addr);
	pj_sockaddr_cp(&cand->base_addr, &rel_info.relay_addr);
	pj_sockaddr_cp(&cand->rel_addr, &rel_info.mapped_addr);
	pj_ice_calc_foundation(comp->ice_st->pool, &cand->foundation, 
			       PJ_ICE_CAND_TYPE_RELAYED, 
			       &rel_info.relay_addr);
	cand->status = PJ_SUCCESS;
	
	// Set transport_id to TURN TCP
	if (pj_turn_sock_get_conn_type(turn_sock) == PJ_TURN_TP_TCP) {
		cand->transport_id = TP_TURN_TCP;
		cand->type = PJ_ICE_CAND_TYPE_RELAYED_TCP;
	} else if (pj_turn_sock_get_conn_type(turn_sock) == PJ_TURN_TP_UDP) {
		cand->transport_id = TP_TURN;
		cand->type = PJ_ICE_CAND_TYPE_RELAYED;
	}

	// DEAN, Don't set relay as default candidate if upnp_tcp enabled, 
	// or ice-mismatch error will occur when we remove relay candidate dynamically.
	/* Set default candidate to relay */
#if 1
	if (comp->ice_st->use_upnp_flag == 0 && !comp->ice_st->use_stun_cand)
		comp->default_cand = cand - comp->cand_list;
#endif


	PJ_LOG(4,(comp->ice_st->obj_name,
		  "Comp %d: TURN allocation complete, relay address is %s",
		  comp->comp_id,
		  pj_sockaddr_print(&rel_info.relay_addr, ipaddr,
				     sizeof(ipaddr), 3)));

	sess_init_update(comp->ice_st);

	// decrease waiting count
	if (comp->ice_st->wait_turn_alloc_ok_cnt)
		comp->ice_st->wait_turn_alloc_ok_cnt--;

    } else if (new_state >= PJ_TURN_STATE_DEALLOCATING) {
	pj_turn_session_info info;

	comp->turn_err_cnt++;
	// 2013-05-09 DEAN
	// if new state is deallocated, increase error count again.
	// for stopping turn allocation thread.
	if (new_state == PJ_TURN_STATE_DEALLOCATED)
		comp->turn_err_cnt++;

	pj_turn_sock_get_info(turn_sock, &info);

	/* Unregister ourself from the TURN relay */
	pj_turn_sock_set_user_data(turn_sock, NULL);

	if (pj_turn_sock_get_conn_type(turn_sock) == PJ_TURN_TP_TCP)
		comp->turn_tcp_sock = NULL;
	else
		comp->turn_sock = NULL;

	/* Set session to fail if we're still initializing */
	if (comp->ice_st->state < PJ_ICE_STRANS_STATE_READY) {
		// natnl don't set ice failed when turn failed.
#if 0
	    sess_fail(comp->ice_st, PJ_ICE_STRANS_OP_INIT,
			"TURN allocation failed", info.last_status);
#endif
		// retry for initialization
		PJ_PERROR(4,(comp->ice_st->obj_name, info.last_status,
		      "Comp %d: TURN allocation failed, retrying",
		      comp->comp_id));
		add_update_turn(comp->ice_st, comp->ice_st->ice_role, comp);
		// decrease waiting count
		if (comp->ice_st->wait_turn_alloc_ok_cnt)
			comp->ice_st->wait_turn_alloc_ok_cnt--;
	} else if (comp->turn_err_cnt > 1) {
		// natnl don't set ice failed when turn failed.
#if 0
	    sess_fail(comp->ice_st, PJ_ICE_STRANS_OP_KEEP_ALIVE,
			"TURN refresh failed", info.last_status);
#endif

		// decrease waiting count
		if (comp->ice_st->wait_turn_alloc_ok_cnt)
			comp->ice_st->wait_turn_alloc_ok_cnt--;
	} else {
	    // DEAN move to comp->ice_st->state < PJ_ICE_STRANS_STATE_READY
		//PJ_PERROR(4,(comp->ice_st->obj_name, info.last_status,
		//      "Comp %d: TURN allocation failed, retrying",
		//      comp->comp_id));
		//add_update_turn(comp->ice_st, comp->ice_st->ice_role, comp);
	}
    }

    pj_grp_lock_dec_ref(comp->ice_st->grp_lock);
}

/* DEAN Added 2013-03-19 */
static void turn_on_allocated(pj_turn_sock *turn_sock, 
						  pj_sockaddr_t *turn_srv)
{
	pj_ice_strans_comp *comp;

	comp = (pj_ice_strans_comp*) pj_turn_sock_get_user_data(turn_sock);

	if (comp && comp->ice_st && comp->ice_st->cb.on_turn_srv_allocated)
		(*comp->ice_st->cb.on_turn_srv_allocated)(comp->ice_st, turn_srv);
}

/* Callback when UPnP TCP socket has received a packet */
static void tcp_on_rx_data(void *sess,
			    void *pkt,
			    unsigned pkt_len,
			    const pj_sockaddr_t *peer_addr,
			    unsigned addr_len)
{
    pj_ice_strans_comp *comp;
    pj_status_t status;

	pj_tcp_session *tcp_sess = (pj_tcp_session *)sess;
	pj_tcp_sock *tcp_sock = (pj_tcp_sock *)pj_tcp_session_get_user_data(tcp_sess);

    comp = (pj_ice_strans_comp*) pj_tcp_sock_get_user_data(tcp_sock);
    if (comp == NULL) {
	/* We have disassociated ourselves from the UPnP TCP socket */
	return;
    }

    pj_grp_lock_add_ref(comp->ice_st->grp_lock);

    if (comp->ice_st->ice == NULL) {
	/* The ICE session is gone, but we're still receiving packets.
	 * This could also happen if remote doesn't do ICE and application
	 * specifies TCP as the default address in SDP.
	 * So in this case just give the packet to application.
	 */
	if (comp->ice_st->cb.on_rx_data) {
	    (*comp->ice_st->cb.on_rx_data)(comp->ice_st, comp->comp_id, pkt,
					   pkt_len, peer_addr, addr_len);
	}

    } else {

	/* Hand over the packet to ICE */
	status = pj_ice_sess_on_rx_pkt2(comp->ice_st->ice, comp->comp_id,
				       TP_UPNP_TCP, pkt, pkt_len,
				       peer_addr, addr_len, pj_tcp_session_get_stun_session(tcp_sess));

	if (status != PJ_SUCCESS) {
	    ice_st_perror(comp->ice_st, 
			  "Error processing packet from TCP", 
			  status);
	}
    }

    pj_grp_lock_dec_ref(comp->ice_st->grp_lock);
}


/* Callback when UPnP TCP client state has changed */
static void tcp_on_state(pj_tcp_sock *tcp_sock, pj_tcp_state_t old_state,
			  pj_tcp_state_t new_state)
{
    pj_ice_strans_comp *comp;

    comp = (pj_ice_strans_comp*) pj_tcp_sock_get_user_data(tcp_sock);
    if (comp == NULL) {
	/* Not interested in further state notification once the relay is
	 * disconnecting.
	 */
	return;
    }

    PJ_LOG(5,(comp->ice_st->obj_name, "TCP client state changed %s --> %s",
	      pj_tcp_state_name(old_state), pj_tcp_state_name(new_state)));

    pj_grp_lock_add_ref(comp->ice_st->grp_lock);

    if (new_state == PJ_TCP_STATE_READY) {
	pj_tcp_session_info rel_info;
	pj_ice_sess_cand *cand = NULL;
	unsigned i;
#if 0
	comp->tcp_err_cnt = 0;
#endif
	/* Get allocation info */
	pj_tcp_sock_get_info(tcp_sock, &rel_info);

	/* Wait until initialization completes */
	pj_grp_lock_acquire(comp->ice_st->grp_lock);

	/* Find relayed candidate in the component */
	for (i=0; i<comp->cand_cnt; ++i) {
	    if (comp->cand_list[i].transport_id == TP_UPNP_TCP) {
		cand = &comp->cand_list[i];
		break;
	    }
	}

	pj_grp_lock_release(comp->ice_st->grp_lock);

	if (!cand)
		return;
#if 0
	/* Update candidate */
	pj_sockaddr_cp(&cand->addr, &rel_info.relay_addr);
	pj_sockaddr_cp(&cand->base_addr, &rel_info.relay_addr);
	pj_sockaddr_cp(&cand->rel_addr, &rel_info.mapped_addr);
	pj_ice_calc_foundation(comp->ice_st->pool, &cand->foundation, 
			       PJ_ICE_CAND_TYPE_RELAYED, 
			       &rel_info.relay_addr);
	cand->status = PJ_SUCCESS;
#endif

	/* Set default candidate to UPnP TCP */
	//if (comp->ice_st->use_upnp_flag > 0)
	comp->default_cand = cand - comp->cand_list;

#if 0
	PJ_LOG(4,(comp->ice_st->obj_name, 
		  "Comp %d: TCP allocation complete, relay address is %s",
		  comp->comp_id, 
		  pj_sockaddr_print(&rel_info.relay_addr, ipaddr, 
				     sizeof(ipaddr), 3)));
#endif
	sess_init_update(comp->ice_st);

    } else if (new_state >= PJ_TCP_STATE_DISCONNECTING) {
	pj_tcp_session_info info;
#if 0
	++comp->tcp_err_cnt;
#endif
	pj_tcp_sock_get_info(tcp_sock, &info);

#if 0
	/* Unregister ourself from the TCP */
	pj_tcp_sock_set_user_data(tcp_sock, NULL);
	comp->tcp_sock = NULL;
#endif

	/* Set session to fail if we're still initializing */
	if (comp->ice_st->state < PJ_ICE_STRANS_STATE_READY) {
	    sess_fail(comp->ice_st, PJ_ICE_STRANS_OP_INIT,
		      "TCP allocation failed", info.last_status);
#if 0
	} else if (comp->tcp_err_cnt > 1) {
	    sess_fail(comp->ice_st, PJ_ICE_STRANS_OP_KEEP_ALIVE,
		      "TCP refresh failed", info.last_status);
#endif
	} else {
	    PJ_PERROR(4,(comp->ice_st->obj_name, info.last_status,
		      "Comp %d: TCP allocation failed, retrying",
		      comp->comp_id));
	    //add_update_tcp_candidate(comp->ice_st, comp);
	}
    }

    pj_grp_lock_dec_ref(comp->ice_st->grp_lock);
}

static pj_status_t tcp_on_server_binding_complete(pj_tcp_sock *tcp_sock,
										 pj_sockaddr *external_addr,
										 pj_sockaddr *local_addr)
{
	pj_ice_strans_comp *comp = pj_tcp_sock_get_user_data(tcp_sock);
	pj_status_t status;

	if (comp->ice_st->cb.on_server_created) {
		return (*comp->ice_st->cb.on_server_created)(comp->ice_st, 
						external_addr, local_addr);
	}

	return PJ_SUCCESS;
}

PJ_DEF(void *) pj_ice_strans_get_ice_session(void *user_data)
{
	pj_ice_strans *ice_st = (pj_ice_strans *)user_data;
	return ice_st->ice;
}

PJ_DEF(void *) pj_ice_strans_get_ice_strans(void *user_data)
{
	pj_ice_strans_comp *comp = (pj_ice_strans_comp *)user_data;
        return comp->ice_st;
}

PJ_DEF(unsigned int) pj_ice_strans_get_comp_id(void *user_data)
{
	pj_ice_strans_comp *comp = (pj_ice_strans_comp *)user_data;
        return (unsigned int)comp->comp_id;
}

PJ_DEF(pj_stun_session *) pj_ice_strans_get_stun_session(void *user_data)
{
	pj_ice_strans_comp *comp = (pj_ice_strans_comp *)user_data;
	unsigned comp_id = comp->comp_id;
	return comp->ice_st->ice->comp[comp_id-1].stun_sess;
}

PJ_DEF(void *) pj_ice_strans_get_tcp_server_accepted_sess(void *user_data, unsigned comp_id, int tcp_sess_idx)
{
	pj_ice_strans *ice_st = (pj_ice_strans *)user_data;
	return ice_st == NULL ? NULL : 
		pj_tcp_sock_get_accept_session_by_idx(ice_st->comp[comp_id-1]->tcp_sock, tcp_sess_idx);
}

PJ_DEF(void *) pj_ice_strans_get_tcp_client_sess(void *user_data, unsigned comp_id, int tcp_sess_idx)
{
	pj_ice_strans *ice_st = (pj_ice_strans *)user_data;
	return ice_st == NULL ? NULL : 
		pj_tcp_sock_get_client_session_by_idx(ice_st->comp[comp_id-1]->tcp_sock, tcp_sess_idx);
}

PJ_DEF(pj_bool_t) pj_ice_strans_tp_is_upnp_tcp(unsigned transport_id)
{
	return transport_id == TP_UPNP_TCP;
}

PJ_DEF(pj_bool_t) pj_ice_strans_tp_is_stun(unsigned transport_id)
{
	return transport_id == TP_STUN;
}

PJ_DEF(pj_bool_t) pj_ice_strans_tp_is_turn(unsigned transport_id)
{
	return transport_id == TP_TURN || transport_id == TP_TURN_TCP;
}

PJ_DEF(natnl_tunnel_type) pj_ice_strans_get_use_tunnel_type(struct pj_ice_strans *ice_st)
{
	return ice_st->tunnel_type;
}

PJ_DEF(void) pj_ice_strans_set_use_upnp_flag(void *user_data, int use_upnp_flag)
{

	struct pj_ice_strans *ice_st = (struct pj_ice_strans *)user_data;

	ice_st->use_upnp_flag = use_upnp_flag;
}

PJ_DEF(void) pj_ice_strans_set_use_stun_cand(void *user_data, pj_bool_t use_stun_cand)
{

	struct pj_ice_strans *ice_st = (struct pj_ice_strans *)user_data;

	ice_st->use_stun_cand = use_stun_cand;
}

PJ_DEF(void) pj_ice_strans_set_use_turn_flag(void *user_data, int use_turn_flag)
{

	struct pj_ice_strans *ice_st = (struct pj_ice_strans *)user_data;

	ice_st->use_turn_flag = use_turn_flag;
}

PJ_DEF(void) pj_ice_strans_set_ice_role(void *user_data, pj_ice_sess_role ice_role)
{

	struct pj_ice_strans *ice_st = (struct pj_ice_strans *)user_data;

	ice_st->ice_role = ice_role;
}

PJ_DEF(int) pj_ice_strans_get_wait_turn_cnt(struct pj_ice_strans *ice_st)
{
	return ice_st->wait_turn_alloc_ok_cnt;
}

PJ_DEF(void) pj_ice_strans_set_remote_turn_cfg(struct pj_ice_strans *ice_st,
											   pj_str_t server, pj_uint16_t port, 
											   pj_str_t realm, pj_str_t username, 
											   pj_str_t password)
{
	ice_st->cfg.rem_turn.server = server;
	ice_st->cfg.rem_turn.port = port;
	ice_st->cfg.rem_turn.auth_cred.data.static_cred.realm = realm;
	ice_st->cfg.rem_turn.auth_cred.data.static_cred.username = username; 
	ice_st->cfg.rem_turn.auth_cred.data.static_cred.data = password;
}

PJ_DEF(void) pj_ice_strans_set_turn_password(struct pj_ice_strans *ice_st,
											   pj_str_t password)
{
	//pj_strdup(ice_st->pool, &ice_st->cfg.turn.auth_cred.data.static_cred.data, &password);
	pj_strdup(ice_st->pool, &ice_st->cfg.rem_turn.auth_cred.data.static_cred.data, &password);
}

PJ_DEF(void) pj_ice_strans_set_sess_fail(struct pj_ice_strans *ice_st,
											 pj_ice_strans_op op, 
											 const char *title, pj_status_t status) 
{
	sess_fail(ice_st, op, title, status);
}

PJ_DEF(pj_str_t *)pj_ice_strans_get_cfg_turn_srv_ip(struct pj_ice_strans *ice_st)
{
        return &ice_st->cfg.turn.server;
}

PJ_DEF(pj_uint16_t)pj_ice_strans_get_cfg_turn_srv_port(struct pj_ice_strans *ice_st)
{
        return ice_st->cfg.turn.port;
}

PJ_DEF(void) pj_ice_strans_set_app_lock(void *user_data, pj_mutex_t *lock)
{

	struct pj_ice_strans *ice_st = (struct pj_ice_strans *)user_data;

	ice_st->app_lock = lock;
}

PJ_DEF(pj_bool_t) pj_ice_strans_get_local_path_selected(void *user_data)
{
	struct pj_ice_strans *ice_st = (struct pj_ice_strans *)user_data;
	return pj_ice_sess_local_path_selected(ice_st->ice);
}

PJ_DEF(natnl_addr_changed_type) pj_ice_strans_get_addr_changed_type(void *user_data)
{
	struct pj_ice_strans *ice_st = (struct pj_ice_strans *)user_data;
	return ice_st->addr_changed_type;
}

PJ_DEF(pj_str_t *) pj_ice_strans_get_dest_uri(void *user_data)
{
	struct pj_ice_strans *ice_st = (struct pj_ice_strans *)user_data;
	return &ice_st->dest_uri;
}

PJ_DEF(void) pj_ice_strans_set_dest_uri(struct pj_ice_strans *ice_st, pj_str_t *dest_uri)
{
	if (dest_uri)
	{
		if (ice_st->dest_uri.ptr)
		{
			free(ice_st->dest_uri.ptr);
			ice_st->dest_uri.ptr = NULL;
		}

		ice_st->dest_uri.ptr = (char *)malloc(dest_uri->slen);
		pj_strcpy(&ice_st->dest_uri, dest_uri);
	}
}

PJ_DEF(void) pj_ice_strans_set_check_tcp_session_ready(struct pj_ice_strans *ice_st, 
													   int check_idx, pj_bool_t ready)
{
	ice_st->ice->clist.checks[check_idx].tcp_sess_ready = ready;
}

PJ_DEF(void) pj_ice_strans_update_or_add_incoming_check(void *user_data,
														 const pj_sockaddr_t *local_addr,
														 const pj_sockaddr_t *peer_addr,
														 int tcp_sess_idx)
{
	int i, check_idx, lcand_idx, rcand_idx;
	struct pj_ice_strans_comp *ice_st_comp = (struct pj_ice_strans_comp *)user_data;
	pj_ice_sess *ice = ice_st_comp->ice_st->ice;
	pj_ice_sess_check *check, *new_check;
	pj_ice_sess_cand *lcand, *rcand;

	if (!ice)
		return;

	pj_grp_lock_acquire(ice->grp_lock);

	check_idx = ice->clist.count++;
	pj_assert(ice->clist.count < PJ_ICE_MAX_CHECKS);
	new_check = &ice->clist.checks[check_idx];

	check = NULL;
	// lcand
	for (i=0; i<ice->clist.count-1;i++)
	{
		check = &ice->clist.checks[i];
		if (check->lcand->type == PJ_ICE_CAND_TYPE_SRFLX_TCP &&
			check->lcand->transport_id == TP_UPNP_TCP && 
			check->lcand->tcp_type == PJ_ICE_CAND_TCP_TYPE_PASSIVE &&
			pj_sockaddr_cmp(&check->lcand->addr, local_addr) == 0)
			break;
	}
	
	if (check && i < ice->clist.count-1) // same lcand exists in check list
	{
		new_check->lcand = check->lcand;
		lcand = new_check->lcand;
	}
	else
	{
		lcand_idx = ice->lcand_cnt++;
		pj_assert(ice->lcand_cnt < PJ_ICE_MAX_CAND);
		new_check->lcand = &ice->lcand[lcand_idx];
		lcand = new_check->lcand;
		lcand->type = PJ_ICE_CAND_TYPE_SRFLX_TCP;
		lcand->status = PJ_SUCCESS;
		lcand->local_pref = UPNP_TCP_PREF;
		lcand->transport_id = TP_UPNP_TCP;
		lcand->comp_id = (pj_uint8_t) ice_st_comp->comp_id;
		lcand->tcp_type = PJ_ICE_CAND_TCP_TYPE_PASSIVE;
		lcand->use_user_port = 0;
		lcand->prio = CALC_CAND_PRIO(ice, lcand->type, lcand->local_pref, lcand->comp_id);
		pj_sockaddr_cp(&lcand->addr, local_addr);
		pj_sockaddr_cp(&lcand->base_addr, local_addr);
		pj_sockaddr_cp(&lcand->rel_addr, &lcand->base_addr);
		pj_ice_calc_foundation(ice->pool, &lcand->foundation,
			lcand->type, &lcand->base_addr);
	}

	check = NULL;
	//rcand
	for (i=0; i<ice->clist.count-1;i++)
	{
		check = &ice->clist.checks[i];
		if (check->rcand->type == PJ_ICE_CAND_TYPE_HOST_TCP &&
			check->rcand->transport_id == TP_UPNP_TCP && 
			check->rcand->tcp_type == PJ_ICE_CAND_TCP_TYPE_ACTIVE &&
			pj_sockaddr_cmp(&check->rcand->addr, peer_addr) == 0)
			break;
	}

	if (check && i < ice->clist.count-1) // same rcand exists in check list
	{
		new_check->rcand = check->rcand;
		rcand = new_check->rcand;
	}
	else
	{
		rcand_idx = ice->rcand_cnt++;
		pj_assert(ice->rcand_cnt < PJ_ICE_MAX_CAND);
		new_check->rcand = &ice->rcand[rcand_idx];
		rcand = new_check->rcand;
		rcand->type = PJ_ICE_CAND_TYPE_HOST_TCP;
		rcand->status = PJ_SUCCESS;
		rcand->local_pref = UPNP_TCP_PREF;
		rcand->transport_id = TP_UPNP_TCP;
		rcand->comp_id = (pj_uint8_t) ice_st_comp->comp_id;
		rcand->tcp_type = PJ_ICE_CAND_TCP_TYPE_ACTIVE;
		rcand->use_user_port = 0;
		rcand->prio = CALC_CAND_PRIO(ice, rcand->type, rcand->local_pref, rcand->comp_id);
		pj_sockaddr_cp(&rcand->addr, peer_addr);
		pj_sockaddr_cp(&rcand->base_addr, peer_addr);
		pj_sockaddr_cp(&rcand->rel_addr, &rcand->base_addr);
		pj_ice_calc_foundation(ice->pool, &rcand->foundation,
			rcand->type, &rcand->base_addr);
	}

	new_check->prio = CALC_CHECK_PRIO(ice, new_check->lcand, new_check->rcand);
	new_check->state = PJ_ICE_SESS_CHECK_STATE_WAITING;
	new_check->nominated = PJ_FALSE;
	new_check->local_path = 
		(lcand->tcp_type == PJ_ICE_CAND_TCP_TYPE_ACTIVE && 
		(lcand->tcp_type == rcand->tcp_type)) || 
		(lcand->tcp_type == PJ_ICE_CAND_TCP_TYPE_ACTIVE && 
		rcand->tcp_type == PJ_ICE_CAND_TCP_TYPE_NONE);
	new_check->err_code = PJ_SUCCESS;
	new_check->tcp_sess_idx = tcp_sess_idx;


	PJ_LOG(4, (ice->obj_name, "pj_ice_strans_update_or_add_incoming_check() New check %s is added", 
		dump_check(ice->tmp.txt, sizeof(ice->tmp.txt), 
		&ice->clist, new_check)));

	pj_grp_lock_release(ice->grp_lock);
}
