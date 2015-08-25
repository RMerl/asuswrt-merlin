/* $Id: transport_ice.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_TRANSPORT_ICE_H__
#define __PJMEDIA_TRANSPORT_ICE_H__


/**
 * @file transport_ice.h
 * @brief ICE capable media transport.
 */

#include <pjmedia/stream.h>
#include <pjnath/ice_strans.h>


/**
 * @defgroup PJMEDIA_TRANSPORT_ICE ICE Media Transport 
 * @ingroup PJMEDIA_TRANSPORT
 * @brief Interactive Connectivity Establishment (ICE) transport
 * @{
 *
 * This describes the implementation of media transport using
 * Interactive Connectivity Establishment (ICE) protocol.
 */

PJ_BEGIN_DECL


/**
 * Structure containing callbacks to receive ICE notifications.
 */
typedef struct pjmedia_ice_cb
{
    /**
     * This callback will be called when ICE negotiation completes.
     *
     * @param tp	PJMEDIA ICE transport.
     * @param op	The operation
	 * @param status	Operation status.
	 * @param turn_mapped_addr	TURN socket mapped address if any, otherwise it is NULL.
     */
    void    (*on_ice_complete)(pjmedia_transport *tp,
			       pj_ice_strans_op op,
				   pj_status_t status,
				   pj_sockaddr *turn_mapped_addr);

	/**
	 * 2013-05-20 DEAN
	 * This call will be called when stun binding completes.
	 * @param local_addr.		The server listening local address.
	 */
	pj_status_t    (*on_stun_binding_complete)(pjmedia_transport *tp,
										 pj_sockaddr *local_addr, 
										 int ip_chanaged_type);

	/**
	 * 2014-01-12 DEAN
	 * This call will be called when tcp server was created.
	 * @param [out] external_addr.	The stun mapped address.
	 * @param [in] local_addr.		The server listening local address.
	 */
	pj_status_t	   (*on_tcp_server_binding_complete)(pjmedia_transport *tp,
					pj_sockaddr *external_addr,
					pj_sockaddr *local_addr);

} pjmedia_ice_cb;


/**
 * This structure specifies ICE transport specific info. This structure
 * will be filled in media transport specific info.
 */
typedef struct pjmedia_ice_transport_info
{
    /**
     * ICE sesion state.
     */
    pj_ice_strans_state sess_state;

    /**
     * Session role.
     */
    pj_ice_sess_role role;

    /**
     * Number of components in the component array. Before ICE negotiation
     * is complete, the number represents the number of components of the
     * local agent. After ICE negotiation has been completed successfully,
     * the number represents the number of common components between local
     * and remote agents.
     */
    unsigned comp_cnt;

    /**
     * Array of ICE components. Typically the first element denotes RTP and
     * second element denotes RTCP.
     */
    struct
    {
	/**
	 * Local candidate type.
	 */
	pj_ice_cand_type    lcand_type;

	/**
	 * Remote candidate type.
	 */
	pj_ice_cand_type    rcand_type;

    } comp[2];

} pjmedia_ice_transport_info;


/**
 * Options that can be specified when creating ICE transport.
 */
enum pjmedia_transport_ice_options
{
    /**
     * Normally when remote doesn't use ICE, the ICE transport will 
     * continuously check the source address of incoming packets to see 
     * if it is different than the configured remote address, and switch 
     * the remote address to the source address of the packet if they 
     * are different after several packets are received.
     * Specifying this option will disable this feature.
     */
    PJMEDIA_ICE_NO_SRC_ADDR_CHECKING = 1
};


/**
 * Create the Interactive Connectivity Establishment (ICE) media transport
 * using the specified configuration. When STUN or TURN (or both) is used,
 * the creation operation will complete asynchronously, when STUN resolution
 * and TURN allocation completes. When the initialization completes, the
 * \a on_ice_complete() complete will be called with \a op parameter equal
 * to PJ_ICE_STRANS_OP_INIT.
 *
 * In addition, this transport will also notify the application about the
 * result of ICE negotiation, also in \a on_ice_complete() callback. In this
 * case the callback will be called with \a op parameter equal to
 * PJ_ICE_STRANS_OP_NEGOTIATION.
 *
 * Other than this, application should use the \ref PJMEDIA_TRANSPORT API
 * to manipulate this media transport.
 *
 * @param endpt		The media endpoint.
 * @param name		Optional name to identify this ICE media transport
 *			for logging purposes.
 * @param comp_cnt	Number of components to be created.
 * @param cfg		Pointer to configuration settings.
 * @param cb		Optional structure containing ICE specific callbacks.
 * @param inv_recv_time		The invite message receiving time for encoding into SDP of 200 OK message.
 * @param p_tp		Pointer to receive the media transport instance.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_ice_create(pjmedia_endpt *endpt,
					const char *name,
					unsigned comp_cnt,
					const pj_ice_strans_cfg *cfg,
					const pjmedia_ice_cb *cb,
					int tp_idx,
					pj_time_val inv_recv_time,
					pjmedia_transport **p_tp);


/**
 * The same as #pjmedia_ice_create() with additional \a options param.
 *
 * @param endpt		The media endpoint.
 * @param name		Optional name to identify this ICE media transport
 *			for logging purposes.
 * @param comp_cnt	Number of components to be created.
 * @param cfg		Pointer to configuration settings.
 * @param cb		Optional structure containing ICE specific callbacks.
 * @param options	Options, see #pjmedia_transport_ice_options.
 * @param p_tp		Pointer to receive the media transport instance.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_ice_create2(pjmedia_endpt *endpt,
					 const char *name,
					 unsigned comp_cnt,
					 const pj_ice_strans_cfg *cfg,
					 const pjmedia_ice_cb *cb,
					 unsigned options,
					 int tp_idx,
					 pj_time_val inv_recv_time,
					 pjmedia_transport **p_tp);
PJ_DECL(pj_bool_t) pjmedia_ice_get_ice_restart(void *user_data);

PJ_DECL(void) pjmedia_ice_set_ice_restart(void *user_data, pj_bool_t ice_restart);

PJ_DECL(natnl_tunnel_type) pjmedia_ice_get_use_tunnel_type(void *user_data);

PJ_DECL(void) pjmedia_ice_set_turn_password(void *user_data, pj_str_t turn_password);

#if 0

PJ_DECL(void) pjmedia_ice_set_use_upnp_flag(void *user_data, int use_upnp_flag);

PJ_DECL(void) pjmedia_ice_set_use_stun_cand(void *user_data, pj_bool_t use_stun_cand);

PJ_DECL(void) pjmedia_ice_set_use_turn_flag(void *user_data, int use_turn_flag);


//====== local and remote user id ======
/* Dean Added */
PJ_DECL(void) pjmedia_ice_set_local_userid(void *user_data, char *user_id, int user_id_len);
/* Dean Added */
PJ_DECL(void) pjmedia_ice_get_local_userid(void *user_data, char *user_id);

/* Dean Added */
PJ_DECL(void) pjmedia_ice_set_remote_userid(void *user_data, char *user_id, int user_id_len);
/* Dean Added */
PJ_DECL(void) pjmedia_ice_get_remote_userid(void *user_data, char *user_id);


//====== local and remote device id ======
/* Dean Added */
PJ_DECL(void) pjmedia_ice_set_local_deviceid(void *user_data, char *device_id, int user_id_len);
/* Dean Added */
PJ_DECL(void) pjmedia_ice_get_local_deviceid(void *user_data, char *device_id);

/* Dean Added */
PJ_DECL(void) pjmedia_ice_set_remote_deviceid(void *user_data, char *device_id, int device_id_len);
/* Dean Added */
PJ_DECL(void) pjmedia_ice_get_remote_deviceid(void *user_data, char *device_id);


//====== local and remote turn server ======
/* Dean Added */
PJ_DECL(void) pjmedia_ice_get_local_turnsrv(void *user_data, char *turn_server);
/* Dean Added */
PJ_DECL(void) pjmedia_ice_set_local_turnsrv(void *user_data, char *turn_server, int turn_server_len);

/* Dean Added */
PJ_DECL(void) pjmedia_ice_set_remote_turnsvr(void *user_data, char *turn_server, int turn_server_len);
/* Dean Added */
PJ_DECL(void) pjmedia_ice_get_remote_turnsvr(void *user_data, char *turn_server);


//====== local and remote turn password ======
/* Dean Added */
PJ_DECL(void) pjmedia_ice_set_local_turnpwd(void *user_data, char *turn_pwd, int turn_pwd_len );
/* Dean Added */
PJ_DECL(void) pjmedia_ice_get_local_turnpwd(void *user_data, char *turn_pwd);

/* Dean Added */
PJ_DECL(void) pjmedia_ice_set_remote_turnpwd(void *user_data, char *turn_pwd, int turn_pwd_len);
/* Dean Added */
PJ_DECL(void) pjmedia_ice_get_remote_turnpwd(void *user_data, char *turn_pwd);
#endif
/* Dean Added */
PJ_DECL(pj_bool_t) pjmedia_ice_get_local_path_selected(void *user_data);
/* Dean Added */
PJ_DECL(natnl_addr_changed_type) pjmedia_ice_get_addr_chagned_type(void *user_data);
#if 0
/* Dean Added */
PJ_DECL(pj_str_t *) pjmedia_ice_get_dest_uri(void *user_data);
#endif

PJ_END_DECL


/**
 * @}
 */


#endif	/* __PJMEDIA_TRANSPORT_ICE_H__ */


