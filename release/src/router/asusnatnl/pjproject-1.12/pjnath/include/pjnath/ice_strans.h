/* $Id: ice_strans.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJNATH_ICE_STRANS_H__
#define __PJNATH_ICE_STRANS_H__


/**
 * @file ice_strans.h
 * @brief ICE Stream Transport
 */
#include <pjnath/ice_session.h>
#include <pjnath/stun_sock.h>
#include <pjnath/turn_sock.h>
#include <pjnath/tcp_sock.h>
#include <pjlib-util/resolver.h>
#include <pj/ioqueue.h>
#include <pj/timer.h>


PJ_BEGIN_DECL


/**
 * @addtogroup PJNATH_ICE_STREAM_TRANSPORT
 * @{
 *
 * This module describes ICE stream transport, as represented by #pj_ice_strans
 * structure, and is part of PJNATH - the Open Source NAT traversal helper
 * library.
 *
 * ICE stream transport, as represented by #pj_ice_strans structure, is an ICE
 * capable class for transporting media streams within a media session. 
 * It consists of one or more transport sockets (typically two for RTP
 * based communication - one for RTP and one for RTCP), and an 
 * \ref PJNATH_ICE_SESSION for performing connectivity checks among the.
 * various candidates of the transport addresses.
 *
 *
 * \section ice_strans_using_sec Using the ICE stream transport
 *
 * The steps below describe how to use ICE session:
 *
 *  - initialize a #pj_ice_strans_cfg structure. This contains various 
 *    settings for the ICE stream transport, and among other things contains
 *    the STUN and TURN settings.\n\n
 *  - create the instance with #pj_ice_strans_create(). Among other things,
 *    the function needs the following arguments:
 *	- the #pj_ice_strans_cfg structure for the main configurations
 *	- number of components to be supported
 *	- instance of #pj_ice_strans_cb structure to report callbacks to
 *	  application.\n\n
 *  - while the #pj_ice_strans_create() call completes immediately, the
 *    initialization will be running in the background to gather the 
 *    candidates (for example STUN and TURN candidates, if they are enabled
 *    in the #pj_ice_strans_cfg setting). Application will be notified when
 *    the initialization completes in the \a on_ice_complete callback of
 *    the #pj_ice_strans_cb structure (the \a op argument of this callback
 *    will be PJ_ICE_STRANS_OP_INIT).\n\n
 *  - when media stream is to be started (for example, a call is to be 
 *    started), create an ICE session by calling #pj_ice_strans_init_ice().\n\n
 *  - the application now typically will need to communicate local ICE
 *    information to remote host. It can achieve this by using the following
 *    functions to query local ICE information:
 *	- #pj_ice_strans_get_ufrag_pwd()
 *	- #pj_ice_strans_enum_cands()
 *	- #pj_ice_strans_get_def_cand()\n
 *    The application may need to encode the above information as SDP.\n\n
 *  - when the application receives remote ICE information (for example, from
 *    the SDP received from remote), it can now start ICE negotiation, by
 *    calling #pj_ice_strans_start_ice(). This function requires some
 *    information about remote ICE agent such as remote ICE username fragment
 *    and password as well as array of remote candidates.\n\n
 *  - note that the PJNATH library does not work with SDP; application would
 *    need to encode and parse the SDP itself.\n\n
 *  - once ICE negotiation has been started, application will be notified
 *    about the completion in the \a on_ice_complete() callback of the
 *    #pj_ice_strans_cb.\n\n
 *  - at any time, application may send or receive data. However the ICE
 *    stream transport may not be able to send it depending on its current
 *    state. Before ICE negotiation is started, the data will be sent using
 *    default candidate of the component. After negotiation is completed,
 *    data will be sent using the candidate from the successful/nominated
 *    pair. The ICE stream transport may not be able to send data while 
 *    negotiation is in progress.\n\n
 *  - application sends data by using #pj_ice_strans_sendto(). Incoming
 *    data will be reported in \a on_rx_data() callback of the
 *    #pj_ice_strans_cb.\n\n
 *  - once the media session has finished (e.g. user hangs up the call),
 *    destroy the ICE session with #pj_ice_strans_stop_ice().\n\n
 *  - at this point, application may destroy the ICE stream transport itself,
 *    or let it run so that it can be reused to create other ICE session.
 *    The benefit of letting the ICE stream transport alive (without any
 *    session active) is to avoid delay with the initialization, howerver
 *    keeping the transport alive means the transport needs to keep the
 *    STUN binding open by using keep-alive and also TURN allocation alive,
 *    and this will consume power which is an important issue for mobile
 *    applications.\n\n
 */

/** Forward declaration for ICE stream transport. */
typedef struct pj_ice_strans pj_ice_strans;

/** 
 * NATNL tunnel type.
 */
typedef enum natnl_tunnel_type
{
	/** NATNL tunnel using UNKNOWN */
	NATNL_TUNNEL_TYPE_UNKNOWN,

	/** NATNL tunnel using UPNP_TCP */
	NATNL_TUNNEL_TYPE_UPNP_TCP,

	/** NATNL tunnel using TURN */
	NATNL_TUNNEL_TYPE_TURN,

	/** NATNL tunnel using UDP */
    NATNL_TUNNEL_TYPE_UDP,

} natnl_tunnel_type;

/** Transport operation types to be reported on \a on_status() callback */
typedef enum pj_ice_strans_op
{
    /** Initialization (candidate gathering) */
    PJ_ICE_STRANS_OP_INIT,

    /** Negotiation */
    PJ_ICE_STRANS_OP_NEGOTIATION,

    /** This operatino is used to report failure in keep-alive operation.
     *  Currently it is only used to report TURN Refresh failure.
     */
    PJ_ICE_STRANS_OP_KEEP_ALIVE,
    /** This operatino is used to report failure in keep-alive operation.
     *  Currently it is only used to report TURN Refresh failure.
     */
    PJ_ICE_STRANS_OP_ADDRESS_CHANGED

} pj_ice_strans_op;

/** Transport operation types to be reported on \a on_status() callback */
typedef enum natnl_addr_changed_type
{
	ADDR_CHANGED_TYPE_NONE,
	ADDR_CHANGED_TYPE_EXTERNAL,
	ADDR_CHANGED_TYPE_LOCAL

} natnl_addr_changed_type;

/** 
 * This structure contains callbacks that will be called by the 
 * ICE stream transport.
 */
typedef struct pj_ice_strans_cb
{
    /**
     * This callback will be called when the ICE transport receives
     * incoming packet from the sockets which is not related to ICE
     * (for example, normal RTP/RTCP packet destined for application).
     *
     * @param ice_st	    The ICE stream transport.
     * @param comp_id	    The component ID.
     * @param pkt	    The packet.
     * @param size	    Size of the packet.
     * @param src_addr	    Source address of the packet.
     * @param src_addr_len  Length of the source address.
     */
    void    (*on_rx_data)(pj_ice_strans *ice_st,
			  unsigned comp_id, 
			  void *pkt, pj_size_t size,
			  const pj_sockaddr_t *src_addr,
			  unsigned src_addr_len);

    /**
     * Callback to report status of various ICE operations.
     * 
     * @param ice_st	    The ICE stream transport.
	 * @param op	    The operation which status is being reported.
	 * @param status	    Operation status.
	 * @param turn_mapped_addr	TURN socket mapped address if any, otherwise it is NULL.
     */
    void    (*on_ice_complete)(pj_ice_strans *ice_st, 
			       pj_ice_strans_op op,
			       pj_status_t status,
				   pj_sockaddr *turn_mapped_addr);

    /**
	 * DEAN Added for change transport_ice's local_turn_srv
	 *
     * Notification when TURN server allocated. 
	 *
	 * @param ice_st	    The ICE stream transport.
     * @param turn_srv	    The TURN server address that was allocated.
     */
	void (*on_turn_srv_allocated)(pj_ice_strans *ice_st, 
					pj_sockaddr_t *turn_srv);

	/**
	* 2013-05-20 DEAN
	* 2014-01-13 DEAN modified.
	 * This call will be called when stun binding completes.
	 * @param local_addr.		The server listening local address.
	 * @param ip_changed_type type of ip changed.
	 */
	pj_status_t (*on_stun_binding_complete)(pj_ice_strans *ice_st,
					pj_sockaddr *local_addr, 
					natnl_addr_changed_type ip_changed_type);

    /**
     * Notification when TCP server has been created. Application should
     * implement this callback to setup upnp port forwarding.
	 *
	 * @param ice_st	    The ICE stream transport.
	 * @param external_addr.	The stun mapped address.
	 * @param local_addr.		The server listening local address.
     */
	pj_status_t (*on_server_created)(pj_ice_strans *ice_st,
					pj_sockaddr *external_addr,
					pj_sockaddr *local_addr);

} pj_ice_strans_cb;


/**
 * This structure describes ICE stream transport configuration. Application
 * should initialize the structure by calling #pj_ice_strans_cfg_default()
 * before changing the settings.
 */
typedef struct pj_ice_strans_cfg
{
    /**
     * Address family, IPv4 or IPv6. Currently only pj_AF_INET() (IPv4)
     * is supported, and this is the default value.
     */
    int			af;

    /**
     * STUN configuration which contains the timer heap and
     * ioqueue instance to be used, and STUN retransmission
     * settings. This setting is mandatory.
     *
     * The default value is all zero. Application must initialize
     * this setting with #pj_stun_config_init().
     */
    pj_stun_config	 stun_cfg;

    /**
     * DNS resolver to be used to resolve servers. If DNS SRV
     * resolution is required, the resolver must be set.
     *
     * The default value is NULL.
     */
    pj_dns_resolver	*resolver;

    /**
     * This contains various STUN session options. Once the ICE stream
     * transport is created, application may also change the options
     * with #pj_ice_strans_set_options().
     */
    pj_ice_sess_options	 opt;

    /**
     * STUN and local transport settings. This specifies the 
     * settings for local UDP socket, which will be resolved
     * to get the STUN mapped address.
     */
    struct {
	/**
	 * Optional configuration for STUN transport. The default
	 * value will be initialized with #pj_stun_sock_cfg_default().
	 */
	pj_stun_sock_cfg     cfg;

	/**
	 * Maximum number of host candidates to be added. If the
	 * value is zero, no host candidates will be added.
	 *
	 * Default: 64
	 */
	unsigned	     max_host_cands;

	/**
	 * Include loopback addresses in the host candidates.
	 *
	 * Default: PJ_FALSE
	 */
	pj_bool_t	     loop_addr;

	/**
	 * Specify the STUN server domain or hostname or IP address.
	 * If DNS SRV resolution is required, application must fill
	 * in this setting with the domain name of the STUN server 
	 * and set the resolver instance in the \a resolver field.
	 * Otherwise if the \a resolver setting is not set, this
	 * field will be resolved with hostname resolution and in
	 * this case the \a port field must be set.
	 *
	 * The \a port field should also be set even when DNS SRV
	 * resolution is used, in case the DNS SRV resolution fails.
	 *
	 * When this field is empty, STUN mapped address resolution
	 * will not be performed. In this case only ICE host candidates
	 * will be added to the ICE transport, unless if \a no_host_cands
	 * field is set. In this case, both host and srflx candidates 
	 * are disabled.
	 *
	 * The default value is empty.
	 */
	pj_str_t	     server;

	/**
	 * The port number of the STUN server, when \a server
	 * field specifies a hostname rather than domain name. This
	 * field should also be set even when the \a server
	 * specifies a domain name, to allow DNS SRV resolution
	 * to fallback to DNS A/AAAA resolution when the DNS SRV
	 * resolution fails.
	 *
	 * The default value is PJ_STUN_PORT.
	 */
	pj_uint16_t	     port;

    } stun;

    /**
     * TURN specific settings.
     */
    pj_turn_server turn;	

    /**
     * TURN specific settings.
     */
    int turn_cnt;

    /**
     * TURN specific settings.
     */
    pj_turn_server turn_list[MAX_TURN_SERVER_COUNT];

    /**
     * TURN specific settings.
     */
    pj_turn_server rem_turn;

    /**
     * TCP specific settings.
     */
    struct {
	/**
	 * Optional TCP socket settings. The default values will be
	 * initialized by #pj_tcp_sock_cfg_default(). This contains
	 * settings such as QoS.
	 */
	pj_tcp_sock_cfg     cfg;

    } tcp;

    /**
     * Component specific settings, which will override the settings in
     * the STUN and TURN settings above. For example, setting the QoS
     * parameters here allows the application to have different QoS
     * traffic type for RTP and RTCP component.
     */
    struct {
	/**
	 * QoS traffic type to be set on this transport. When application
	 * wants to apply QoS tagging to the transport, it's preferable to
	 * set this field rather than \a qos_param fields since this is 
	 * more portable.
	 *
	 * Default value is PJ_QOS_TYPE_BEST_EFFORT.
	 */
	pj_qos_type qos_type;

	/**
	 * Set the low level QoS parameters to the transport. This is a 
	 * lower level operation than setting the \a qos_type field and
	 * may not be supported on all platforms.
	 *
	 * By default all settings in this structure are disabled.
	 */
	pj_qos_params qos_params;

    } comp[PJ_ICE_MAX_COMP];

	//int			  use_upnp_flag;
	int			  user_port_count;
	struct {
		pj_bool_t user_port_assigned;       // user selected port assigned
		pj_uint16_t local_tcp_data_port;    // the user agent's local data port
		pj_uint16_t external_tcp_data_port; // the external port for the incoming packet to local data port
		pj_uint16_t local_tcp_ctl_port;     // the user agent's local control port
		pj_uint16_t external_tcp_ctl_port;  // the external control port for the incoming packet to local port
	} user_ports[MAX_USER_PORT_COUNT]; // user selected port pair

	int      tnl_timeout_msec;          // tunnel timeout value in second.

	int		 use_turn_flag;             // use turn flag. 0 : don't use turn, 1 : uas uses its own turn, 2 : uas uses uac's turn.

} pj_ice_strans_cfg;


/**
 * ICE stream transport's state.
 */
typedef enum pj_ice_strans_state
{
    /**
     * ICE stream transport is not created.
     */
    PJ_ICE_STRANS_STATE_NULL,

    /**
     * ICE candidate gathering process is in progress.
     */
    PJ_ICE_STRANS_STATE_INIT,

    /**
     * ICE stream transport initialization/candidate gathering process is
     * complete, ICE session may be created on this stream transport.
     */
    PJ_ICE_STRANS_STATE_READY,

    /**
     * New session has been created and the session is ready.
     */
    PJ_ICE_STRANS_STATE_SESS_READY,

    /**
     * ICE negotiation is in progress.
     */
    PJ_ICE_STRANS_STATE_NEGO,

    /**
     * ICE negotiation has completed successfully and media is ready
     * to be used.
     */
    PJ_ICE_STRANS_STATE_RUNNING,

    /**
     * ICE negotiation has completed with failure.
     */
    PJ_ICE_STRANS_STATE_FAILED

} pj_ice_strans_state;


/** 
 * Initialize ICE transport configuration with default values.
 *
 * @param cfg		The configuration to be initialized.
 */
PJ_DECL(void) pj_ice_strans_cfg_default(pj_ice_strans_cfg *cfg);


/**
 * Copy configuration.
 *
 * @param pool		Pool.
 * @param dst		Destination.
 * @param src		Source.
 */
PJ_DECL(void) pj_ice_strans_cfg_copy(pj_pool_t *pool,
				     pj_ice_strans_cfg *dst,
				     const pj_ice_strans_cfg *src);


/**
 * Create and initialize the ICE stream transport with the specified
 * parameters. 
 *
 * @param name		Optional name for logging identification.
 * @param cfg		Configuration.
 * @param comp_cnt	Number of components.
 * @param user_data	Arbitrary user data to be associated with this
 *			ICE stream transport.
 * @param cb		Callback.
 * @param p_ice_st	Pointer to receive the ICE stream transport
 *			instance.
 *
 * @return		PJ_SUCCESS if ICE stream transport is created
 *			successfully.
 */
PJ_DECL(pj_status_t) pj_ice_strans_create2(const char *name,
					  const pj_ice_strans_cfg *cfg,
					  unsigned comp_cnt,
					  void *user_data,
					  const pj_ice_strans_cb *cb,
					  int tp_idx,
					  pj_ice_strans **p_ice_st);

PJ_DECL(pj_status_t) pj_ice_strans_create(const char *name,
										  const pj_ice_strans_cfg *cfg,
										  unsigned comp_cnt,
										  void *user_data,
										  const pj_ice_strans_cb *cb,
										  pj_ice_strans **p_ice_st);

/**
 * Get ICE session state.
 *
 * @param ice_st	The ICE stream transport.
 *
 * @return		ICE session state.
 */
PJ_DECL(pj_ice_strans_state) pj_ice_strans_get_state(pj_ice_strans *ice_st);


/**
 * Get string representation of ICE state.
 *
 * @param state		ICE stream transport state.
 *
 * @return		String.
 */
PJ_DECL(const char*) pj_ice_strans_state_name(pj_ice_strans_state state);


/**
 * Destroy the ICE stream transport. This will destroy the ICE session
 * inside the ICE stream transport, close all sockets and release all
 * other resources.
 *
 * @param ice_st	The ICE stream transport.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ice_strans_destroy(pj_ice_strans *ice_st);


/**
 * Get the user data associated with the ICE stream transport.
 *
 * @param ice_st	The ICE stream transport.
 *
 * @return		The user data.
 */
PJ_DECL(void*) pj_ice_strans_get_user_data(pj_ice_strans *ice_st);


/**
 * Get the value of various options of the ICE stream transport.
 *
 * @param ice_st	The ICE stream transport.
 * @param opt		The options to be initialized with the values
 *			from the ICE stream transport.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error.
 */
PJ_DECL(pj_status_t) pj_ice_strans_get_options(pj_ice_strans *ice_st,
					       pj_ice_sess_options *opt);

/**
 * Specify various options for this ICE stream transport. Application 
 * should call #pj_ice_strans_get_options() to initialize the options 
 * with their default values.
 *
 * @param ice_st	The ICE stream transport.
 * @param opt		Options to be applied to this ICE stream transport.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error.
 */
PJ_DECL(pj_status_t) pj_ice_strans_set_options(pj_ice_strans *ice_st,
					       const pj_ice_sess_options *opt);


/**
 * Initialize the ICE session in the ICE stream transport.
 * When application is about to send an offer containing ICE capability,
 * or when it receives an offer containing ICE capability, it must
 * call this function to initialize the internal ICE session. This would
 * register all transport address aliases for each component to the ICE
 * session as candidates. Then application can enumerate all local
 * candidates by calling #pj_ice_strans_enum_cands(), and encode these
 * candidates in the SDP to be sent to remote agent.
 *
 * @param ice_st	The ICE stream transport.
 * @param role		ICE role.
 * @param local_ufrag	Optional local username fragment.
 * @param local_passwd	Optional local password.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ice_strans_init_ice(pj_ice_strans *ice_st,
					    pj_ice_sess_role role,
					    const pj_str_t *local_ufrag,
					    const pj_str_t *local_passwd);
PJ_DECL(pj_status_t) pj_ice_strans_init_ice2(pj_ice_strans *ice_st,
										   pj_ice_sess_role role,
										   const pj_str_t *local_ufrag,
										   const pj_str_t *local_passwd);

/**
 * Check if the ICE stream transport has the ICE session created. The
 * ICE session is created with #pj_ice_strans_init_ice().
 *
 * @param ice_st	The ICE stream transport.
 *
 * @return		PJ_TRUE if #pj_ice_strans_init_ice() has been
 *			called.
 */
PJ_DECL(pj_bool_t) pj_ice_strans_has_sess(pj_ice_strans *ice_st);


/**
 * Check if ICE negotiation is still running.
 *
 * @param ice_st	The ICE stream transport.
 *
 * @return		PJ_TRUE if ICE session has been created and ICE 
 *			negotiation negotiation is in progress.
 */
PJ_DECL(pj_bool_t) pj_ice_strans_sess_is_running(pj_ice_strans *ice_st);


/**
 * Check if ICE negotiation has completed.
 *
 * @param ice_st	The ICE stream transport.
 *
 * @return		PJ_TRUE if ICE session has been created and the
 *			negotiation is complete.
 */
PJ_DECL(pj_bool_t) pj_ice_strans_sess_is_complete(pj_ice_strans *ice_st);


/**
 * Get the current/running component count. If ICE negotiation has not
 * been started, the number of components will be equal to the number
 * when the ICE stream transport was created. Once negotiation been
 * started, the number of components will be the lowest number of 
 * component between local and remote agents.
 *
 * @param ice_st	The ICE stream transport.
 *
 * @return		The running number of components.
 */
PJ_DECL(unsigned) pj_ice_strans_get_running_comp_cnt(pj_ice_strans *ice_st);


/**
 * Get the ICE username fragment and password of the ICE session. The
 * local username fragment and password can only be retrieved once ICE
 * session has been created with #pj_ice_strans_init_ice(). The remote
 * username fragment and password can only be retrieved once ICE session
 * has been started with #pj_ice_strans_start_ice().
 *
 * Note that the string returned by this function is only valid throughout
 * the duration of the ICE session, and the application must not modify
 * these strings. Once the ICE session has been stopped with
 * #pj_ice_strans_stop_ice(), the pointer in the string will no longer be
 * valid.
 *
 * @param ice_st	The ICE stream transport.
 * @param loc_ufrag	Optional pointer to receive ICE username fragment
 *			of local endpoint from the ICE session.
 * @param loc_pwd	Optional pointer to receive ICE password of local
 *			endpoint from the ICE session.
 * @param rem_ufrag	Optional pointer to receive ICE username fragment
 *			of remote endpoint from the ICE session.
 * @param rem_pwd	Optional pointer to receive ICE password of remote
 *			endpoint from the ICE session.
 *
 * @return		PJ_SUCCESS if the strings have been retrieved
 *			successfully.
 */
PJ_DECL(pj_status_t) pj_ice_strans_get_ufrag_pwd(pj_ice_strans *ice_st,
						 pj_str_t *loc_ufrag,
						 pj_str_t *loc_pwd,
						 pj_str_t *rem_ufrag,
						 pj_str_t *rem_pwd);


/**
 * Get the number of local candidates for the specified component ID.
 *
 * @param ice_st	The ICE stream transport.
 * @param comp_id	Component ID.
 *
 * @return		The number of candidates.
 */
PJ_DECL(unsigned) pj_ice_strans_get_cands_count(pj_ice_strans *ice_st,
					        unsigned comp_id);

/**
 * Enumerate the local candidates for the specified component.
 *
 * @param ice_st	The ICE stream transport.
 * @param comp_id	Component ID.
 * @param count		On input, it specifies the maximum number of
 *			elements. On output, it will be filled with
 *			the number of candidates copied to the
 *			array.
 * @param cand		Array of candidates.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ice_strans_enum_cands(pj_ice_strans *ice_st,
					      unsigned comp_id,
					      unsigned *count,
					      pj_ice_sess_cand cand[]);

/**
 * Get the default candidate for the specified component. When this
 * function is called before ICE negotiation completes, the default
 * candidate is selected according to local preference criteria. When
 * this function is called after ICE negotiation completes, the
 * default candidate is the candidate that forms the valid pair.
 *
 * @param ice_st	The ICE stream transport.
 * @param comp_id	Component ID.
 * @param cand		Pointer to receive the default candidate
 *			information.
 */
PJ_DECL(pj_status_t) pj_ice_strans_get_def_cand(pj_ice_strans *ice_st,
						unsigned comp_id,
						pj_ice_sess_cand *cand);

/**
 * Get the current ICE role. ICE session must have been initialized
 * before this function can be called.
 *
 * @param ice_st	The ICE stream transport.
 *
 * @return		Current ICE role.
 */
PJ_DECL(pj_ice_sess_role) pj_ice_strans_get_role(pj_ice_strans *ice_st);


/**
 * Change session role. This happens for example when ICE session was
 * created with controlled role when receiving an offer, but it turns out
 * that the offer contains "a=ice-lite" attribute when the SDP gets
 * inspected. ICE session must have been initialized before this function
 * can be called.
 *
 * @param ice_st	The ICE stream transport.
 * @param new_role	The new role to be set.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error.
 */
PJ_DECL(pj_status_t) pj_ice_strans_change_role(pj_ice_strans *ice_st,
					       pj_ice_sess_role new_role);


/**
 * Start ICE connectivity checks. This function can only be called
 * after the ICE session has been created in the ICE stream transport
 * with #pj_ice_strans_init_ice().
 *
 * This function must be called once application has received remote
 * candidate list (typically from the remote SDP). This function pairs
 * local candidates with remote candidates, and starts ICE connectivity
 * checks. The ICE session/transport will then notify the application 
 * via the callback when ICE connectivity checks completes, either 
 * successfully or with failure.
 *
 * @param ice_st	The ICE stream transport.
 * @param rem_ufrag	Remote ufrag, as seen in the SDP received from 
 *			the remote agent.
 * @param rem_passwd	Remote password, as seen in the SDP received from
 *			the remote agent.
 * @param rcand_cnt	Number of remote candidates in the array.
 * @param rcand		Remote candidates array.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ice_strans_start_ice(pj_ice_strans *ice_st,
					     const pj_str_t *rem_ufrag,
					     const pj_str_t *rem_passwd,
					     unsigned rcand_cnt,
					     const pj_ice_sess_cand rcand[]);

/**
 * Retrieve the candidate pair that has been nominated and successfully
 * checked for the specified component. If ICE negotiation is still in
 * progress or it has failed, this function will return NULL.
 *
 * @param ice_st	The ICE stream transport.
 * @param comp_id	Component ID.
 *
 * @return		The valid pair as ICE checklist structure if the
 *			pair exist.
 */
PJ_DECL(const pj_ice_sess_check*) 
pj_ice_strans_get_valid_pair(const pj_ice_strans *ice_st,
			     unsigned comp_id);

/**
 * Retrieve the candidate pair that has been nominated and successfully
 * checked for the specified component. If ICE negotiation is still in
 * progress or it has failed, this function will return NULL.
 *
 * @param ice_st	The ICE stream transport.
 * @param comp_id	Component ID.
 *
 * @return		The valid pair as ICE checklist structure if the
 *			pair exist.
 */
PJ_DECL(const pj_ice_sess_check*) 
pj_ice_strans_get_nominated_pair(const pj_ice_strans *ice_st,
			     unsigned comp_id);

/**
 * Stop and destroy the ICE session inside this media transport. Application
 * needs to call this function once the media session is over (the call has
 * been disconnected).
 *
 * Application MAY reuse this ICE stream transport for subsequent calls.
 * In this case, it must call #pj_ice_strans_stop_ice() when the call is
 * disconnected, and reinitialize the ICE stream transport for subsequent
 * call with #pj_ice_strans_init_ice()/#pj_ice_strans_start_ice(). In this
 * case, the ICE stream transport will maintain the internal sockets and
 * continue to send STUN keep-alive packets and TURN Refresh request to 
 * keep the NAT binding/TURN allocation open and to detect change in STUN
 * mapped address.
 *
 * If application does not want to reuse the ICE stream transport for
 * subsequent calls, it must call #pj_ice_strans_destroy() to destroy the
 * ICE stream transport altogether.
 *
 * @param ice_st	The ICE stream transport.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ice_strans_stop_ice(pj_ice_strans *ice_st);


/**
 * Send outgoing packet using this transport. 
 * Application can send data (normally RTP or RTCP packets) at any time
 * by calling this function. This function takes a destination
 * address as one of the arguments, and this destination address should
 * be taken from the default transport address of the component (that is
 * the address in SDP c= and m= lines, or in a=rtcp attribute). 
 * If ICE negotiation is in progress, this function will send the data 
 * to the destination address. Otherwise if ICE negotiation has completed
 * successfully, this function will send the data to the nominated remote 
 * address, as negotiated by ICE.
 *
 * @param ice_st	The ICE stream transport.
 * @param comp_id	Component ID.
 * @param data		The data or packet to be sent.
 * @param data_len	Size of data or packet, in bytes.
 * @param dst_addr	The destination address.
 * @param dst_addr_len	Length of destination address.
 *
 * @return		PJ_SUCCESS if data is sent successfully.
 */
PJ_DECL(pj_status_t) pj_ice_strans_sendto(pj_ice_strans *ice_st,
					  unsigned comp_id,
					  const void *data,
					  pj_size_t data_len,
					  const pj_sockaddr_t *dst_addr,
					  int dst_addr_len);

PJ_DECL(void*) pj_ice_strans_get_ice_session(void *user_data);

PJ_DECL(void* ) pj_ice_strans_get_ice_strans(void* user_data);

PJ_DECL(unsigned int) pj_ice_strans_get_comp_id(void *user_data);

PJ_DECL(pj_stun_session *) pj_ice_strans_get_stun_session(void *user_data);

PJ_DECL(void *) pj_ice_strans_get_tcp_server_accepted_sess(void *user_data, unsigned comp_id, int tcp_sess_idx);

PJ_DECL(void *) pj_ice_strans_get_tcp_client_sess(void *user_data, unsigned comp_id, int tcp_sess_idx);

PJ_DECL(pj_bool_t) pj_ice_strans_tp_is_upnp_tcp(unsigned transport_id);

PJ_DECL(pj_bool_t) pj_ice_strans_tp_is_stun(unsigned transport_id);

PJ_DECL(pj_bool_t) pj_ice_strans_tp_is_turn(unsigned transport_id);

PJ_DECL(natnl_tunnel_type) pj_ice_strans_get_use_tunnel_type(struct pj_ice_strans *ice_st);

PJ_DECL(void) pj_ice_strans_set_use_upnp_flag(void *user_data, int use_upnp_flag);

PJ_DECL(void) pj_ice_strans_set_use_stun_cand(void *user_data, pj_bool_t use_stun_cand);

PJ_DECL(void) pj_ice_strans_set_use_turn_flag(void *user_data, int use_turn_flag);

PJ_DECL(void) pj_ice_strans_set_ice_role(void *user_data, pj_ice_sess_role ice_role);

PJ_DECL(int) pj_ice_strans_get_wait_turn_cnt(struct pj_ice_strans *ice_st);

PJ_DECL(void) pj_ice_strans_set_remote_turn_cfg(struct pj_ice_strans *ice_st,
												pj_str_t server, pj_uint16_t port, pj_str_t realm,
												pj_str_t username, pj_str_t password);

PJ_DECL(void) pj_ice_strans_set_turn_password(struct pj_ice_strans *ice_st, pj_str_t password);

PJ_DECL(void) pj_ice_strans_set_sess_fail(struct pj_ice_strans *ice_st,
										 pj_ice_strans_op op, 
										 const char *title, pj_status_t status);

PJ_DECL(void) pj_ice_strans_set_app_lock(void *user_data, pj_mutex_t *lock);

PJ_DECL(pj_bool_t) pj_ice_strans_get_local_path_selected(void *user_data);

PJ_DECL(natnl_addr_changed_type) pj_ice_strans_get_addr_changed_type(void *user_data);

PJ_DECL(pj_str_t *) pj_ice_strans_get_dest_uri(void *user_data);

PJ_DECL(void) pj_ice_strans_set_dest_uri(struct pj_ice_strans *ice_st, pj_str_t *dest_uri);

PJ_DECL(void) pj_ice_strans_set_check_tcp_session_ready(struct pj_ice_strans *ice_st, 
													   int check_idx, pj_bool_t ready);

PJ_DECL(void) pj_ice_strans_update_or_add_incoming_check(void *user_data,
														 const pj_sockaddr_t *local_addr,
														 const pj_sockaddr_t *peer_addr,
														 int tcp_sess_idx);



/**
 * @}
 */


PJ_END_DECL



#endif	/* __PJNATH_ICE_STRANS_H__ */

