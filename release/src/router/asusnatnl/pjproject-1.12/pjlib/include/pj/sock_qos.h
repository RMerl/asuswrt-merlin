/* $Id: sock_qos.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJ_SOCK_QOS_H__
#define __PJ_SOCK_QOS_H__

/**
 * @file sock_qos.h
 * @brief Socket QoS API
 */

#include <pj/sock.h>

PJ_BEGIN_DECL 


/**
 * @defgroup socket_qos Socket Quality of Service (QoS) API: TOS, DSCP, WMM, IEEE 802.1p
 * @ingroup PJ_SOCK
 * @{


    \section intro QoS Technologies

    QoS settings are available for both Layer 2 and 3 of TCP/IP protocols:

    \subsection intro_ieee8021p Layer 2: IEEE 802.1p for Ethernet

    IEEE 802.1p tagging will mark frames sent by a host for prioritized 
    delivery using a 3-bit Priority field in the virtual local area network 
    (VLAN) header of the Ethernet frame. The VLAN header is placed inside 
    the Ethernet header, between the Source Address field and either the 
    Length field (for an IEEE 802.3 frame) or the EtherType field (for an
    Ethernet II frame).

    \subsection intro_wmm Layer 2: WMM

    At the Network Interface layer for IEEE 802.11 wireless, the Wi-Fi 
    Alliance certification for Wi-Fi Multimedia (WMM) defines four access 
    categories for prioritizing network traffic. These access categories 
    are (in order of highest to lowest priority) voice, video, best-effort, 
    and background. Host support for WMM prioritization requires that both 
    wireless network adapters and their drivers support WMM. Wireless 
    access points (APs) must have WMM enabled.

    \subsection intro_dscp Layer 3: DSCP

    At the Internet layer, you can use Differentiated Services/Diffserv and
    set the value of the Differentiated Services Code Point (DSCP) in the 
    IP header. As defined in RFC 2474, the DSCP value is the high-order 6 bits
    of the IP version 4 (IPv4) TOS field and the IP version 6 (IPv6) Traffic 
    Class field.

    \subsection intro_other Layer 3: Other

    Other mechanisms exist (such as RSVP, IntServ) but this will not be 
    implemented.


    \section availability QoS Availability

    \subsection linux Linux

    DSCP is available via IP TOS option.

    Ethernet 802.1p tagging is done by setting setsockopt(SO_PRIORITY) option 
    of the socket, then with the set_egress_map option of the vconfig utility 
    to convert this to set vlan-qos field of the packet.

    WMM is not known to be available.

    \subsection windows Windows and Windows Mobile

    (It's a mess!)

    DSCP is settable with setsockopt() on Windows 2000 or older, but Windows 
    would silently ignore this call on WinXP or later, unless administrator 
    modifies the registry. On Windows 2000, Windows XP, and Windows Server 
    2003, GQoS (Generic QoS) API is the standard API, but this API may not be
    supported in the future. On Vista and Windows 7, the is a new QoS2 API, 
    also known as Quality Windows Audio-Video Experience (qWAVE).

    IEEE 802.1p tagging is available via Traffic Control (TC) API, available
    on Windows XP SP2, but this needs administrator access. For Vista and 
    later, it's in qWAVE.

    WMM is available for mobile platforms on Windows Mobile 6 platform and 
    Windows Embedded CE 6, via setsockopt(IP_DSCP_TRAFFIC_TYPE). qWAVE 
    supports this as well.

    \subsection symbian Symbian S60 3rd Ed

    Both DSCP and WMM is supported via RSocket::SetOpt() with will set both 
    Layer 2 and Layer 3 QoS settings accordingly. Internally, PJLIB sets the
    DSCP field of the socket, and based on certain DSCP values mapping,
    Symbian will set the WMM tag accordingly.

    \section api PJLIB's QoS API Abstraction

    Based on the above, the following API is implemented.

    Declare the following "standard" traffic types.

    \code
     typedef enum pj_qos_type
     {
	PJ_QOS_TYPE_BEST_EFFORT,
	PJ_QOS_TYPE_BACKGROUND,
	PJ_QOS_TYPE_VIDEO,
	PJ_QOS_TYPE_VOICE,
	PJ_QOS_TYPE_CONTROL
     } pj_qos_type;
    \endcode

    The traffic classes above will determine how the Layer 2 and 3 QoS 
    settings will be used. The standard mapping between the classes above 
    to the corresponding Layer 2 and 3 settings are as follows:

    \code
    =================================================================
    PJLIB Traffic Type 	IP DSCP 	WMM 		    802.1p
    -----------------------------------------------------------------
    BEST_EFFORT 	0x00 		BE (Bulk Effort) 	0
    BACKGROUND 		0x08 		BK (Bulk) 		2
    VIDEO 		0x28 		VI (Video) 		5
    VOICE 		0x30 		VO (Voice) 		6
    CONTROL 		0x38 		VO (Voice) 		7
    =================================================================
    \endcode

    There are two sets of API provided to manipulate the QoS parameters.

    \subsection portable_api Portable API

    The first set of API is:

    \code
     // Set QoS parameters
     PJ_DECL(pj_status_t) pj_sock_set_qos_type(pj_sock_t sock,
					       pj_qos_type val);

     // Get QoS parameters
     PJ_DECL(pj_status_t) pj_sock_get_qos_type(pj_sock_t sock,
					       pj_qos_type *p_val);
    \endcode

    The API will set the traffic type according to the DSCP class, for both 
    Layer 2 and Layer 3 QoS settings, where it's available. If any of the 
    layer QoS setting is not settable, the API will silently ignore it. 
    If both layers are not setable, the API will return error.

    The API above is the recommended use of QoS, since it is the most 
    portable across all platforms.

    \subsection detail_api Fine Grained Control API

    The second set of API is intended for application that wants to fine 
    tune the QoS parameters.

    The Layer 2 and 3 QoS parameters are stored in pj_qos_params structure:

    \code
     typedef enum pj_qos_flag
     {
	PJ_QOS_PARAM_HAS_DSCP = 1,
	PJ_QOS_PARAM_HAS_SO_PRIO = 2,
	PJ_QOS_PARAM_HAS_WMM = 4
     } pj_qos_flag;

     typedef enum pj_qos_wmm_prio
     {
	PJ_QOS_WMM_PRIO_BULK_EFFORT,
	PJ_QOS_WMM_PRIO_BULK,
	PJ_QOS_WMM_PRIO_VIDEO,
	PJ_QOS_WMM_PRIO_VOICE
     } pj_qos_wmm_prio;

     typedef struct pj_qos_params
     {
	pj_uint8_t      flags;    // Determines which values to 
				  // set, bitmask of pj_qos_flag
	pj_uint8_t      dscp_val; // The 6 bits DSCP value to set
	pj_uint8_t      so_prio;  // SO_PRIORITY value
	pj_qos_wmm_prio wmm_prio; // WMM priority value
     } pj_qos_params;
    \endcode

    The second set of API with more fine-grained control over the parameters 
    are:

    \code
     // Retrieve QoS params for the specified traffic type
     PJ_DECL(pj_status_t) pj_qos_get_params(pj_qos_type type, 
					    pj_qos_params *p);

     // Set QoS parameters to the socket
     PJ_DECL(pj_status_t) pj_sock_set_qos_params(pj_sock_t sock,
						 const pj_qos_params *p);

     // Get QoS parameters from the socket
     PJ_DECL(pj_status_t) pj_sock_get_qos_params(pj_sock_t sock,
						 pj_qos_params *p);
    \endcode


    Important:

    The pj_sock_set/get_qos_params() APIs are not portable, and it's probably 
    only going to be implemented on Linux. Application should always try to 
    use pj_sock_set_qos_type() instead.
 */


/**
 * High level traffic classification.
 */
typedef enum pj_qos_type
{
    PJ_QOS_TYPE_BEST_EFFORT,	/**< Best effort traffic (default value).
				     Any QoS function calls with specifying
				     this value are effectively no-op	*/
    PJ_QOS_TYPE_BACKGROUND,	/**< Background traffic.		*/
    PJ_QOS_TYPE_VIDEO,		/**< Video traffic.			*/
    PJ_QOS_TYPE_VOICE,		/**< Voice traffic.			*/
    PJ_QOS_TYPE_CONTROL		/**< Control traffic.			*/
} pj_qos_type;

/**
 * Bitmask flag to indicate which QoS layer setting is set in the 
 * \a flags field of the #pj_qos_params structure. 
 */
typedef enum pj_qos_flag
{
    PJ_QOS_PARAM_HAS_DSCP = 1,	    /**< DSCP field is set.	    */
    PJ_QOS_PARAM_HAS_SO_PRIO = 2,   /**< Socket SO_PRIORITY	    */
    PJ_QOS_PARAM_HAS_WMM = 4	    /**< WMM  field is set. 	    */
} pj_qos_flag;


/**
 * Standard WMM priorities.
 */
typedef enum pj_qos_wmm_prio
{
    PJ_QOS_WMM_PRIO_BULK_EFFORT,	/**< Bulk effort priority   */
    PJ_QOS_WMM_PRIO_BULK,		/**< Bulk priority.	    */
    PJ_QOS_WMM_PRIO_VIDEO,		/**< Video priority	    */
    PJ_QOS_WMM_PRIO_VOICE		/**< Voice priority	    */
} pj_qos_wmm_prio;


/**
 * QoS parameters to be set or retrieved to/from the socket.
 */
typedef struct pj_qos_params
{
    pj_uint8_t      flags;    /**< Determines which values to 
				   set, bitmask of pj_qos_flag	    */
    pj_uint8_t      dscp_val; /**< The 6 bits DSCP value to set	    */
    pj_uint8_t      so_prio;  /**< SO_PRIORITY value		    */
    pj_qos_wmm_prio wmm_prio; /**< WMM priority value		    */
} pj_qos_params;



/**
 * This is the high level and portable API to enable QoS on the specified 
 * socket, by setting the traffic type to the specified parameter.
 *
 * @param sock	    The socket.
 * @param type	    Traffic type to be set.
 *
 * @return	    PJ_SUCCESS if at least Layer 2 or Layer 3 setting is
 *		    successfully set. If both Layer 2 and Layer 3 settings
 *		    can't be set, this function will return error.
 */
PJ_DECL(pj_status_t) pj_sock_set_qos_type(pj_sock_t sock,
					  pj_qos_type type);

/**
 * This is the high level and portable API to get the traffic type that has
 * been set on the socket. On occasions where the Layer 2 or Layer 3 settings
 * were modified by using low level API, this function may return approximation
 * of the closest QoS type that matches the settings.
 *
 * @param sock	    The socket.
 * @param p_type    Pointer to receive the traffic type of the socket.
 *
 * @return	    PJ_SUCCESS if traffic type for the socket can be obtained
 *		    or approximated..
 */
PJ_DECL(pj_status_t) pj_sock_get_qos_type(pj_sock_t sock,
					  pj_qos_type *p_type);


/**
 * This is a convenience function to apply QoS to the socket, and print error
 * logging if the operations failed. Both QoS traffic type and the low level
 * QoS parameters can be applied with this function.
 *
 * @param sock		The socket handle.
 * @param qos_type	QoS traffic type. The QoS traffic type will be applied
 *			only if the value is not PJ_QOS_TYPE_BEST_EFFORT,
 * @param qos_params	Optional low-level QoS parameters. This will be 
 *			applied only if this argument is not NULL and the 
 *			flags inside the structure is non-zero. Upon return, 
 *			the flags will indicate which parameters have been 
 *			applied successfully.
 * @param log_level	This function will print to log at this level upon
 *			encountering errors.
 * @param log_sender	Optional sender name in the log.
 * @param sock_name	Optional name to help identify the socket in the log.
 *
 * @return		PJ_SUCCESS if at least Layer 2 or Layer 3 setting is
 *			successfully set. If both Layer 2 and Layer 3 settings
 *			can't be set, this function will return error.
 *
 * @see pj_sock_apply_qos2()
 */
PJ_DECL(pj_status_t) pj_sock_apply_qos(pj_sock_t sock,
				       pj_qos_type qos_type,
				       pj_qos_params *qos_params,
				       unsigned log_level,
				       const char *log_sender,
				       const char *sock_name);

/**
 * Variant of #pj_sock_apply_qos() where the \a qos_params parameter is
 * const.
 *
 * @see pj_sock_apply_qos()
 */
PJ_DECL(pj_status_t) pj_sock_apply_qos2(pj_sock_t sock,
 				        pj_qos_type qos_type,
				        const pj_qos_params *qos_params,
				        unsigned log_level,
				        const char *log_sender,
				        const char *sock_name);

/**
 * Retrieve the standard mapping of QoS params for the specified traffic
 * type.
 *
 * @param type	    The traffic type from which the QoS parameters
 *		    are to be retrieved.
 * @param p_param   Pointer to receive the QoS parameters.
 *
 * @return	    PJ_SUCCESS on success or the appropriate error code.
 */ 
PJ_DECL(pj_status_t) pj_qos_get_params(pj_qos_type type, 
				       pj_qos_params *p_param);


/**
 * Retrieve the traffic type that matches the specified QoS parameters.
 * If no exact matching is found, this function will return an
 * approximation of the closest matching traffic type for the specified
 * QoS parameters.
 *
 * @param param	    Structure containing QoS parameters to map into
 *		    "standard" traffic types.
 * @param p_type    Pointer to receive the traffic type.
 *
 * @return	    PJ_SUCCESS on success or the appropriate error code.
 */ 
PJ_DECL(pj_status_t) pj_qos_get_type(const pj_qos_params *param,
				     pj_qos_type *p_type);


/**
 * This is a low level API to set QoS parameters to the socket.
 *
 * @param sock	    The socket.
 * @param param	    Structure containing QoS parameters to be applied
 *		    to the socket. Upon return, the \a flags field
 *		    of this structure will be set with bitmask value
 *		    indicating which QoS settings have successfully
 *		    been applied to the socket.
 *
 * @return	    PJ_SUCCESS if at least one field setting has been
 *		    successfully set. If no setting can't be set, 
 *		    this function will return error.
 */ 
PJ_DECL(pj_status_t) pj_sock_set_qos_params(pj_sock_t sock,
					    pj_qos_params *param);

/**
 * This is a low level API to get QoS parameters from the socket.
 *
 * @param sock	    The socket.
 * @param p_param   Pointer to receive the parameters. Upon returning
 *		    successfully, the \a flags field of this structure
 *		    will be initialized with the appropriate bitmask
 *		    to indicate which fields have been successfully
 *		    retrieved.
 *
 * @return	    PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_sock_get_qos_params(pj_sock_t sock,
					    pj_qos_params *p_param);


/**
 * @}
 */


PJ_END_DECL

#endif	/* __PJ_SOCK_QOS_H__ */

