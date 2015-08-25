/* $Id: nat_detect.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJNATH_NAT_DETECT_H__
#define __PJNATH_NAT_DETECT_H__

/**
 * @file ice_session.h
 * @brief ICE session management
 */
#include <pjnath/stun_session.h>


PJ_BEGIN_DECL


/**
 * @defgroup PJNATH_NAT_DETECT NAT Classification/Detection Tool
 * @brief NAT Classification/Detection Tool
 * @ingroup PJNATH
 * @{
 *
 * This module provides one function to perform NAT classification and
 * detection. NAT type detection is performed by calling
 * #pj_stun_detect_nat_type() function.
 */


/**
 * This enumeration describes the NAT types, as specified by RFC 3489
 * Section 5, NAT Variations.
 */
typedef enum pj_stun_nat_type
{
    /**
     * NAT type is unknown because the detection has not been performed.
     */
    PJ_STUN_NAT_TYPE_UNKNOWN,

    /**
     * NAT type is unknown because there is failure in the detection
     * process, possibly because server does not support RFC 3489.
     */
    PJ_STUN_NAT_TYPE_ERR_UNKNOWN,

    /**
     * This specifies that the client has open access to Internet (or
     * at least, its behind a firewall that behaves like a full-cone NAT,
     * but without the translation)
     */
    PJ_STUN_NAT_TYPE_OPEN,

    /**
     * This specifies that communication with server has failed, probably
     * because UDP packets are blocked.
     */
    PJ_STUN_NAT_TYPE_BLOCKED,

    /**
     * Firewall that allows UDP out, and responses have to come back to
     * the source of the request (like a symmetric NAT, but no
     * translation.
     */
    PJ_STUN_NAT_TYPE_SYMMETRIC_UDP,

    /**
     * A full cone NAT is one where all requests from the same internal 
     * IP address and port are mapped to the same external IP address and
     * port.  Furthermore, any external host can send a packet to the 
     * internal host, by sending a packet to the mapped external address.
     */
    PJ_STUN_NAT_TYPE_FULL_CONE,

    /**
     * A symmetric NAT is one where all requests from the same internal 
     * IP address and port, to a specific destination IP address and port,
     * are mapped to the same external IP address and port.  If the same 
     * host sends a packet with the same source address and port, but to 
     * a different destination, a different mapping is used.  Furthermore,
     * only the external host that receives a packet can send a UDP packet
     * back to the internal host.
     */
    PJ_STUN_NAT_TYPE_SYMMETRIC,

    /**
     * A restricted cone NAT is one where all requests from the same 
     * internal IP address and port are mapped to the same external IP 
     * address and port.  Unlike a full cone NAT, an external host (with 
     * IP address X) can send a packet to the internal host only if the 
     * internal host had previously sent a packet to IP address X.
     */
    PJ_STUN_NAT_TYPE_RESTRICTED,

    /**
     * A port restricted cone NAT is like a restricted cone NAT, but the 
     * restriction includes port numbers. Specifically, an external host 
     * can send a packet, with source IP address X and source port P, 
     * to the internal host only if the internal host had previously sent
     * a packet to IP address X and port P.
     */
    PJ_STUN_NAT_TYPE_PORT_RESTRICTED

} pj_stun_nat_type;


/**
 * This structure contains the result of NAT classification function.
 */
typedef struct pj_stun_nat_detect_result
{
    /**
     * Status of the detection process. If this value is not PJ_SUCCESS,
     * the detection has failed and \a nat_type field will contain
     * PJ_STUN_NAT_TYPE_UNKNOWN.
     */
    pj_status_t		 status;

    /**
     * The text describing the status, if the status is not PJ_SUCCESS.
     */
    const char		*status_text;

    /**
     * This contains the NAT type as detected by the detection procedure.
     * This value is only valid when the \a status is PJ_SUCCESS.
     */
    pj_stun_nat_type	 nat_type;

    /**
     * Text describing that NAT type.
     */
    const char		*nat_type_name;

} pj_stun_nat_detect_result;


/**
 * Type of callback to be called when the NAT detection function has
 * completed.
 */
typedef void pj_stun_nat_detect_cb(int inst_id,
				   void *local_addr, void *mapped_addr,
				   const pj_stun_nat_detect_result *res);


/**
 * Get the NAT name from the specified NAT type.
 *
 * @param type		NAT type.
 *
 * @return		NAT name.
 */
PJ_DECL(const char*) pj_stun_get_nat_name(pj_stun_nat_type type);

/*
 * Find out which interface is used to send to the server.
 */
PJ_DECL(pj_status_t) get_local_interface(const pj_sockaddr_in *server,
										pj_in_addr *local_addr);


/**
 * Perform NAT classification function according to the procedures
 * specified in RFC 3489. Once this function returns successfully,
 * the procedure will run in the "background" and will complete
 * asynchronously. Application can register a callback to be notified
 * when such detection has completed.
 *
 * @param inst_id 	The instance id of pjsua.
 * @param server	STUN server address.
 * @param stun_cfg	A structure containing various STUN configurations,
 *			such as the ioqueue and timer heap instance used
 *			to receive network I/O and timer events.
 * @param user_data	Application data, which will be returned back
 *			in the callback.
 * @param cb		Callback to be registered to receive notification
 *			about detection result.
 *
 * @return		If this function returns PJ_SUCCESS, the procedure
 *			will complete asynchronously and callback will be
 *			called when it completes. For other return
 *			values, it means that an error has occured and
 *			the procedure did not start.
 */
PJ_DECL(pj_status_t) pj_stun_detect_nat_type(int inst_id,
						 const pj_sockaddr_in *server,
					     pj_stun_config *stun_cfg,
					     void *user_data,
					     pj_stun_nat_detect_cb *cb);


/**
 * @}
 */
PJ_DECL(pj_status_t) get_local_interface(const pj_sockaddr_in *server, pj_in_addr *local_addr);

PJ_END_DECL


#endif	/* __PJNATH_NAT_DETECT_H__ */

