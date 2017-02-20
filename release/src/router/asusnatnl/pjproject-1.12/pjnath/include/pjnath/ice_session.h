/* $Id: ice_session.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJNATH_ICE_SESSION_H__
#define __PJNATH_ICE_SESSION_H__

/**
 * @file ice_session.h
 * @brief ICE session management
 */
#include <pjnath/types.h>
#include <pjnath/stun_session.h>
#include <pjnath/tcp_session.h>
#include <pjnath/errno.h>
#include <pj/sock.h>
#include <pj/timer.h>

PJ_BEGIN_DECL


/**
 * @addtogroup PJNATH_ICE_SESSION
 * @{
 *
 * This module describes #pj_ice_sess, a transport independent ICE session,
 * part of PJNATH - the Open Source NAT helper library.
 *
 * \section pj_ice_sess_sec ICE Session
 *
 * An ICE session, represented by #pj_ice_sess structure, is the lowest 
 * abstraction of ICE in PJNATH, and it is used to perform and manage
 * connectivity checks of transport address candidates <b>within a
 * single media stream</b> (note: this differs from what is described
 * in ICE draft, where an ICE session manages the whole media sessions
 * rather than just a single stream).
 *
 * The ICE session described here is independent from any transports,
 * meaning that the actual network I/O for this session would have to
 * be performed by the application, or higher layer abstraction. 
 * Using this framework, application would give any incoming packets to
 * the ICE session, and it would provide the ICE session with a callback
 * to send outgoing message.
 *
 * For higher abstraction of ICE where transport is included, please 
 * see \ref PJNATH_ICE_STREAM_TRANSPORT.
 *
 * \subsection pj_ice_sess_using_sec Using The ICE Session
 *
 * The steps below describe how to use ICE session. Alternatively application
 * can use the higher level ICE API, \ref PJNATH_ICE_STREAM_TRANSPORT,
 * which has provided the integration of ICE with socket transport.
 *
 * The steps to use ICE session is similar for both offerer and
 * answerer:
 * - create ICE session with #pj_ice_sess_create(). Among other things,
 *   application needs to specify:
 *	- STUN configuration (pj_stun_config), containing STUN settings
 *	  such as timeout values and the instances of timer heap and
 *	  ioqueue.
 *	- Session name, useful for identifying this session in the log.
 *	- Initial ICE role (#pj_ice_sess_role). The role can be changed
 *	  at later time with #pj_ice_sess_change_role(), and ICE session
 *	  can also change its role automatically when it detects role
 *	  conflict.
 *	- Number of components in the media session.
 *	- Callback to receive ICE events (#pj_ice_sess_cb)
 *	- Optional local ICE username and password. If these arguments
 *	  are NULL, they will be generated randomly.
 * - Add local candidates for each component, with #pj_ice_sess_add_cand().
 *   A candidate is represented with #pj_ice_sess_cand structure.
 *   Each component must be provided with at least one candidate, and
 *   all components must have the same number of candidates. Failing
 *   to comply with this will cause failure during pairing process.
 * - Create offer to describe local ICE candidates. ICE session does not
 *   provide a function to create such offer, but application should be
 *   able to create one since it knows about all components and candidates.
 *   If application uses \ref PJNATH_ICE_STREAM_TRANSPORT, it can
 *   enumerate local candidates by calling #pj_ice_strans_enum_cands().
 *   Application may use #pj_ice_sess_find_default_cand() to let ICE
 *   session chooses the default transport address to be used in SDP
 *   c= and m= lines.
 * - Send the offer to remote endpoint using signaling such as SIP.
 * - Once application has received the answer, it should parse this
 *   answer, build array of remote candidates, and create check lists by
 *   calling #pj_ice_sess_create_check_list(). This process is known as
 *   pairing the candidates, and will result in the creation of check lists.
 * - Once checklist has been created, application then can call
 *   #pj_ice_sess_start_check() to instruct ICE session to start
 *   performing connectivity checks. The ICE session performs the
 *   connectivity checks by processing each check in the checklists.
 * - Application will be notified about the result of ICE connectivity
 *   checks via the callback that was given in #pj_ice_sess_create()
 *   above.
 *
 * To send data, application calls #pj_ice_sess_send_data(). If ICE
 * negotiation has not completed, ICE session would simply drop the data,
 * and return error to caller. If ICE negotiation has completed
 * successfully, ICE session will in turn call the \a on_tx_pkt
 * callback of #pj_ice_sess_cb instance that was previously registered
 * in #pj_ice_sess_create() above.
 *
 * When application receives any packets on the underlying sockets, it
 * must call #pj_ice_sess_on_rx_pkt(). The ICE session will inspect the
 * packet to decide whether to process it locally (if the packet is a
 * STUN message and is part of ICE session) or otherwise pass it back to
 * application via \a on_rx_data callback.
 */

/**
 * Forward declaration for checklist.
 */
typedef struct pj_ice_sess_checklist pj_ice_sess_checklist;

/**
 * This enumeration describes the type of an ICE candidate.
 */
//charles debug
#pragma pack(1)
//charles debug
typedef enum pj_ice_cand_type
{
    /**
     * ICE host candidate. A host candidate represents the actual local
     * transport address in the host.
     */
    PJ_ICE_CAND_TYPE_HOST,

    /**
     * ICE server reflexive candidate, which represents the public mapped
     * address of the local address, and is obtained by sending STUN
     * Binding request from the host candidate to a STUN server.
     */
    PJ_ICE_CAND_TYPE_SRFLX,

    /**
     * ICE peer reflexive candidate, which is the address as seen by peer
     * agent during connectivity check.
     */
    PJ_ICE_CAND_TYPE_PRFLX,

    /**
     * ICE relayed candidate, which represents the address allocated in
     * TURN server.
     */
    PJ_ICE_CAND_TYPE_RELAYED,

    /**
     * ICE host candidate. A host candidate represents the actual local
     * transport address in the host.
     */
    PJ_ICE_CAND_TYPE_HOST_TCP,

    /**
     * ICE server reflexive candidate, which represents the public mapped
     * address of the local address, and is obtained by sending STUN
     * Binding request from the host candidate to a STUN server.
     */
    PJ_ICE_CAND_TYPE_SRFLX_TCP,

    /**
     * ICE relayed candidate, which represents the address allocated in
     * TURN server by using TCP.
     */
    PJ_ICE_CAND_TYPE_RELAYED_TCP,

} pj_ice_cand_type;

/**
 * This enumeration describes the tcp type of an ICE candidate.
 */
typedef enum pj_ice_cand_tcp_type
{
    /**
     * ICE candidate tcp type none, it is for active connection from local.
     */
    PJ_ICE_CAND_TCP_TYPE_NONE,
    /**
     * ICE candidate tcp type active, it is for active connection from local.
     */
    PJ_ICE_CAND_TCP_TYPE_ACTIVE,

    /**
     * ICE candidate tcp type passive, it is for passive connection from remote.
     */
    PJ_ICE_CAND_TCP_TYPE_PASSIVE,

    /**
     * ICE candidate simultaneous-open, we don't use this present.
     */
    PJ_ICE_CAND_TCP_TYPE_SO

} pj_ice_cand_tcp_type;


/** Forward declaration for pj_ice_sess */
typedef struct pj_ice_sess pj_ice_sess;

/** Forward declaration for pj_ice_sess_check */
typedef struct pj_ice_sess_check pj_ice_sess_check;


/**
 * This structure describes ICE component. 
 * A media stream may require multiple components, each of which has 
 * to work for the media stream as a whole to work.  For media streams
 * based on RTP, there are two components per media stream - one for RTP,
 * and one for RTCP.
 */
typedef struct pj_ice_sess_comp
{
    /**
     * Pointer to ICE check with highest priority which connectivity check
     * has been successful. The value will be NULL if a no successful check
     * has not been found for this component.
     */
    pj_ice_sess_check	*valid_check;

    /**
     * Pointer to ICE check with highest priority which connectivity check
     * has been successful and it has been nominated. The value may be NULL
     * if there is no such check yet.
     */
    pj_ice_sess_check	*nominated_check;

    /**
     * The STUN session to be used to send and receive STUN messages for this
     * component.
     */
    pj_stun_session	*stun_sess;

} pj_ice_sess_comp;


/**
 * Data structure to be attached to internal message processing.
 */
typedef struct pj_ice_msg_data
{
    /** Transport ID for this message */
    unsigned	transport_id;

    /** Flag to indicate whether data.req contains data */
    pj_bool_t	has_req_data;

    /** The data */
    union data {
	/** Request data */
	struct request_data {
	    pj_ice_sess		    *ice;   /**< ICE session	*/
	    pj_ice_sess_checklist   *clist; /**< Checklist	*/
	    unsigned		     ckid;  /**< Check ID	*/
	} req;
    } data;

} pj_ice_msg_data;


/**
 * This structure describes an ICE candidate.
 * ICE candidate is a transport address that is to be tested by ICE
 * procedures in order to determine its suitability for usage for
 * receipt of media.  Candidates also have properties - their type
 * (server reflexive, relayed or host), priority, foundation, and
 * base.
 */
typedef struct pj_ice_sess_cand
{
    /**
     * The candidate type, as described in #pj_ice_cand_type enumeration.
     */
    pj_ice_cand_type	 type;

    /** 
     * Status of this candidate. The value will be PJ_SUCCESS if candidate
     * address has been resolved successfully, PJ_EPENDING when the address
     * resolution process is in progress, or other value when the address 
     * resolution has completed with failure.
     */
    pj_status_t		 status;

    /**
     * The component ID of this candidate. Note that component IDs starts
     * with one for RTP and two for RTCP. In other words, it's not zero
     * based.
     */
    pj_uint8_t		 comp_id;

    /**
     * Transport ID to be used to send packets for this candidate.
     */
    pj_uint8_t		 transport_id;

    /**
     * Local preference value, which typically is 65535.
     */
    pj_uint16_t		 local_pref;

    /**
     * The foundation string, which is an identifier which value will be
     * equivalent for two candidates that are of the same type, share the 
     * same base, and come from the same STUN server. The foundation is 
     * used to optimize ICE performance in the Frozen algorithm.
     */
    pj_str_t		 foundation;

    /**
     * The candidate's priority, a 32-bit unsigned value which value will be
     * calculated by the ICE session when a candidate is registered to the
     * ICE session.
     */
    pj_uint32_t		 prio;

    /**
     * IP address of this candidate. For host candidates, this represents
     * the local address of the socket. For reflexive candidates, the value
     * will be the public address allocated in NAT router for the host
     * candidate and as reported in MAPPED-ADDRESS or XOR-MAPPED-ADDRESS
     * attribute of STUN Binding request. For relayed candidate, the value 
     * will be the address allocated in the TURN server by STUN Allocate
     * request.
     */
    pj_sockaddr		 addr;

    /**
     * Base address of this candidate. "Base" refers to the address an agent 
     * sends from for a particular candidate.  For host candidates, the base
     * is the same as the host candidate itself. For reflexive candidates, 
     * the base is the local IP address of the socket. For relayed candidates,
     * the base address is the transport address allocated in the TURN server
     * for this candidate.
     */
    pj_sockaddr		 base_addr;

    /**
     * Related address, which is used for informational only and is not used
     * in any way by the ICE session.
     */
    pj_sockaddr		 rel_addr;

	/**
	 * Active : 
	 * Passive : 
	 */
	pj_ice_cand_tcp_type tcp_type;

	pj_bool_t        use_user_port;

	int              collect_consume_time;

	pj_time_val      adding_time;
	pj_time_val      ending_time;

	pj_bool_t		 enabled;

} pj_ice_sess_cand;


/**
 * This enumeration describes the state of ICE check.
 */
typedef enum pj_ice_sess_check_state
{
    /**
     * A check for this pair hasn't been performed, and it can't
     * yet be performed until some other check succeeds, allowing this
     * pair to unfreeze and move into the Waiting state.
     */
    PJ_ICE_SESS_CHECK_STATE_FROZEN,

    /**
     * A check has not been performed for this pair, and can be
     * performed as soon as it is the highest priority Waiting pair on
     * the check list.
     */
    PJ_ICE_SESS_CHECK_STATE_WAITING,

    /**
     * A check has not been performed for this pair, and can be
     * performed as soon as it is the highest priority Waiting pair on
     * the check list.
     */
    PJ_ICE_SESS_CHECK_STATE_IN_PROGRESS,

    /**
     * A check has not been performed for this pair, and can be
     * performed as soon as it is the highest priority Waiting pair on
     * the check list.
     */
    PJ_ICE_SESS_CHECK_STATE_SUCCEEDED,

    /**
     * A check for this pair was already done and failed, either
     * never producing any response or producing an unrecoverable failure
     * response.
     */
    PJ_ICE_SESS_CHECK_STATE_FAILED

} pj_ice_sess_check_state;


/**
 * This structure describes an ICE connectivity check. An ICE check
 * contains a candidate pair, and will involve sending STUN Binding 
 * Request transaction for the purposes of verifying connectivity. 
 * A check is sent from the local candidate to the remote candidate 
 * of a candidate pair.
 */
struct pj_ice_sess_check
{
    /**
     * Pointer to local candidate entry of this check.
     */
    pj_ice_sess_cand	*lcand;

    /**
     * Pointer to remote candidate entry of this check.
     */
    pj_ice_sess_cand	*rcand;

    /**
     * Check priority.
     */
    pj_timestamp	 prio;

    /**
     * Connectivity check state.
     */
    pj_ice_sess_check_state	 state;

    /**
     * STUN transmit data containing STUN Binding request that was sent 
     * as part of this check. The value will only be set when this check 
     * has a pending transaction, and is used to cancel the transaction
     * when other check has succeeded.
     */
    pj_stun_tx_data	*tdata;

    /**
     * Flag to indicate whether this check is nominated. A nominated check
     * contains USE-CANDIDATE attribute in its STUN Binding request.
     */
    pj_bool_t		 nominated;

    /**
     * When the check failed, this will contain the failure status of the
     * STUN transaction.
     */
    pj_status_t		 err_code;

    /**
     * When the tcp_type of both lcand and rcand are PJ_ICE_CAND_TCP_TYPE_ACTIVE, 
	 * local_path is true.
     */
	pj_bool_t		 local_path;

	// DEAN, just for upnp tcp
	int              tcp_sess_idx;

	// DEAN, valid when candidate type is PJ_ICE_CAND_TYPE_HOST_TCP or PJ_ICE_CAND_TYPE_SRFLX_TCP
	pj_bool_t        tcp_sess_ready;

	// Store transmit count
	int transmit_count;
};


/**
 * This enumeration describes ICE checklist state.
 */
typedef enum pj_ice_sess_checklist_state
{
    /**
     * The checklist is not yet running.
     */
    PJ_ICE_SESS_CHECKLIST_ST_IDLE,

    /**
     * In this state, ICE checks are still in progress for this
     * media stream.
     */
    PJ_ICE_SESS_CHECKLIST_ST_RUNNING,

    /**
     * In this state, ICE checks have completed for this media stream,
     * either successfully or with failure.
     */
    PJ_ICE_SESS_CHECKLIST_ST_COMPLETED

} pj_ice_sess_checklist_state;


/**
 * This structure represents ICE check list, that is an ordered set of 
 * candidate pairs that an agent will use to generate checks.
 */
struct pj_ice_sess_checklist
{
    /**
     * The checklist state.
     */
    pj_ice_sess_checklist_state   state;

    /**
     * Number of candidate pairs (checks).
     */
    unsigned		     count;

    /**
     * Array of candidate pairs (checks).
     */
    pj_ice_sess_check	     checks[PJ_ICE_MAX_CHECKS];

    /**
     * A timer used to perform periodic check for this checklist.
     */
    pj_timer_entry	     timer;

};


/**
 * This structure contains callbacks that will be called by the ICE
 * session.
 */
typedef struct pj_ice_sess_cb
{
    /**
     * An optional callback that will be called by the ICE session when
     * ICE negotiation has completed, successfully or with failure.
     *
     * @param ice	    The ICE session.
     * @param status	    Will contain PJ_SUCCESS if ICE negotiation is
     *			    successful, or some error code.
     */
    void	(*on_ice_complete)(pj_ice_sess *ice, pj_status_t status);

    /**
     * A mandatory callback which will be called by the ICE session when
     * it needs to send outgoing STUN packet. 
     *
     * @param ice	    The ICE session.
     * @param comp_id	    ICE component ID.
     * @param transport_id  Transport ID.
     * @param pkt	    The STUN packet.
     * @param size	    The size of the packet.
	 * @param dst_addr	    Packet destination address.
	 * @param dst_addr_len  Length of destination address.
	 * @param tcp_sess_idx  for tcp is tcp session index.
     */
    pj_status_t (*on_tx_pkt)(pj_ice_sess *ice, unsigned comp_id, 
			     unsigned transport_id,
			     const void *pkt, pj_size_t size,
			     const pj_sockaddr_t *dst_addr,
			     unsigned dst_addr_len,
				 int tcp_sess_idx);

    /**
     * A mandatory callback which will be called by the ICE session when
     * it needs to send outgoing STUN packet. 
     *
     * @param ice	    The ICE session.
     * @param comp_id	    ICE component ID.
     * @param transport_id  Transport ID.
     * @param pkt	    The STUN packet.
     * @param size	    The size of the packet.
	 * @param dst_addr	    Packet destination address.
	 * @param dst_addr_len  Length of destination address.
	 * @param user_data  for tcp is tcp session.
	 * @param tcp_sess_idx  for tcp is tcp session index.
     */
    pj_status_t (*on_tx_pkt2)(pj_ice_sess *ice, unsigned comp_id, 
			     unsigned transport_id,
			     const void *pkt, pj_size_t size,
			     const pj_sockaddr_t *dst_addr,
			     unsigned dst_addr_len,
				 void *user_data,
				 int tcp_sess_idx);

    /**
     * A mandatory callback which will be called by the ICE session when
     * it receives packet which is not part of ICE negotiation.
     *
     * @param ice	    The ICE session.
     * @param comp_id	    ICE component ID.
     * @param transport_id  Transport ID.
     * @param pkt	    The whole packet.
     * @param size	    Size of the packet.
     * @param src_addr	    Source address where this packet was received 
     *			    from.
     * @param src_addr_len  The length of source address.
     */
    void	(*on_rx_data)(pj_ice_sess *ice, unsigned comp_id,
			      unsigned transport_id, 
			      void *pkt, pj_size_t size,
			      const pj_sockaddr_t *src_addr,
			      unsigned src_addr_len);
} pj_ice_sess_cb;


/**
 * This enumeration describes the role of the ICE agent.
 */
typedef enum pj_ice_sess_role
{
    /**
     * The role is unknown.
     */
    PJ_ICE_SESS_ROLE_UNKNOWN,

    /**
     * The ICE agent is in controlled role.
     */
    PJ_ICE_SESS_ROLE_CONTROLLED,

    /**
     * The ICE agent is in controlling role.
     */
    PJ_ICE_SESS_ROLE_CONTROLLING

} pj_ice_sess_role;


/**
 * This structure represents an incoming check (an incoming Binding
 * request message), and is mainly used to keep early checks in the
 * list in the ICE session. An early check is a request received
 * from remote when we haven't received SDP answer yet, therefore we
 * can't perform triggered check. For such cases, keep the incoming
 * request in a list, and we'll do triggered checks (simultaneously)
 * as soon as we receive answer.
 */
typedef struct pj_ice_rx_check
{
    PJ_DECL_LIST_MEMBER(struct pj_ice_rx_check); /**< Standard list     */

    unsigned		 comp_id;	/**< Component ID.		*/
    unsigned		 transport_id;	/**< Transport ID.		*/

    pj_sockaddr		 src_addr;	/**< Source address of request	*/
    unsigned		 src_addr_len;	/**< Length of src address.	*/

    pj_bool_t		 use_candidate;	/**< USE-CANDIDATE is present?	*/
    pj_uint32_t		 priority;	/**< PRIORITY value in the req.	*/
    pj_stun_uint64_attr *role_attr;	/**< ICE-CONTROLLING/CONTROLLED	*/

} pj_ice_rx_check;


/**
 * This structure describes various ICE session options. Application
 * configure the ICE session with these options by calling 
 * #pj_ice_sess_set_options().
 */
typedef struct pj_ice_sess_options
{
    /**
     * Specify whether to use aggressive nomination.
     */
    pj_bool_t		aggressive;

    /**
     * For controlling agent if it uses regular nomination, specify the delay
     * to perform nominated check (connectivity check with USE-CANDIDATE 
     * attribute) after all components have a valid pair.
     *
     * Default value is PJ_ICE_NOMINATED_CHECK_DELAY.
     */
    unsigned		nominated_check_delay;

    /**
     * For a controlled agent, specify how long it wants to wait (in 
     * milliseconds) for the controlling agent to complete sending 
     * connectivity check with nominated flag set to true for all components
     * after the controlled agent has found that all connectivity checks in
     * its checklist have been completed and there is at least one successful
     * (but not nominated) check for every component.
     *
     * Default value for this option is 
     * ICE_CONTROLLED_AGENT_WAIT_NOMINATION_TIMEOUT. Specify -1 to disable
     * this timer.
     */
    int			controlled_agent_want_nom_timeout;

} pj_ice_sess_options;


/**
 * This structure describes the ICE session. For this version of PJNATH,
 * an ICE session corresponds to a single media stream (unlike the ICE
 * session described in the ICE standard where an ICE session covers the
 * whole media and may consist of multiple media streams). The decision
 * to support only a single media session was chosen for simplicity,
 * while still allowing application to utilize multiple media streams by
 * creating multiple ICE sessions, one for each media stream.
 */
struct pj_ice_sess
{
    char		obj_name[PJ_MAX_OBJ_NAME];  /**< Object name.	    */

    pj_pool_t		*pool;			    /**< Pool instance.	    */
    void		*user_data;		    /**< App. data.	    */
    pj_grp_lock_t	*grp_lock;		    /**< Group lock	    */
    pj_ice_sess_role	 role;			    /**< ICE role.	    */
    pj_ice_sess_options	 opt;			    /**< Options	    */
    pj_timestamp	 tie_breaker;		    /**< Tie breaker value  */
    pj_uint8_t		*prefs;			    /**< Type preference.   */
    pj_bool_t		 is_nominating;		    /**< Nominating stage   */
    pj_bool_t		 is_complete;		    /**< Complete?	    */
    pj_bool_t		 is_destroying;		    /**< Destroy is called  */
    pj_status_t		 ice_status;		    /**< Error status.	    */
    pj_timer_entry	 timer;			    /**< ICE timer.	    */
    pj_ice_sess_cb	 cb;			    /**< Callback.	    */

    pj_stun_config	 stun_cfg;		    /**< STUN settings.	    */

    /* STUN credentials */
    pj_str_t		 tx_ufrag;		    /**< Remote ufrag.	    */
    pj_str_t		 tx_uname;		    /**< Uname for TX.	    */
    pj_str_t		 tx_pass;		    /**< Remote password.   */
    pj_str_t		 rx_ufrag;		    /**< Local ufrag.	    */
    pj_str_t		 rx_uname;		    /**< Uname for RX	    */
    pj_str_t		 rx_pass;		    /**< Local password.    */

    /* Components */
    unsigned		 comp_cnt;		    /**< # of components.   */
    pj_ice_sess_comp	 comp[PJ_ICE_MAX_COMP];	    /**< Component array    */
    unsigned		 comp_ka;		    /**< Next comp for KA   */

    /* Local candidates */
    unsigned		 lcand_cnt;		    /**< # of local cand.   */
    pj_ice_sess_cand	 lcand[PJ_ICE_MAX_CAND];    /**< Array of cand.	    */

    /* Remote candidates */
    unsigned		 rcand_cnt;		    /**< # of remote cand.  */
    pj_ice_sess_cand	 rcand[PJ_ICE_MAX_CAND];    /**< Array of cand.	    */

    /** Array of transport datas */
    pj_ice_msg_data	 tp_data[5];

    /* List of eearly checks */
	pj_ice_rx_check	 early_check;		    /**< Early checks.	    */

	/* Checklist */
	pj_ice_sess_checklist clist;		    /**< Active checklist   */

	/* Cached Checklist DEAN*/
	pj_ice_sess_checklist cached_clist;		/**< Cached checklist   */
    
    /* Valid list */
    pj_ice_sess_checklist valid_list;		    /**< Valid list.	    */
    
    /** Temporary buffer for misc stuffs to avoid using stack too much */
    union {
    	char txt[128];
	char errmsg[PJ_ERR_MSG_SIZE];
    } tmp;  

	pj_bool_t partial_destroy;

	/* tunnel timeout value in second */
	int tnl_timeout_msec;
};
//charles debug
#pragma pack()
//charles debug
/**
 * This is a utility function to retrieve the string name for the
 * particular candidate type.
 *
 * @param type		Candidate type.
 *
 * @return		The string representation of the candidate type.
 */
PJ_DECL(const char*) pj_ice_get_cand_type_name(pj_ice_cand_type type);


/**
 * This is a utility function to retrieve the string name for the
 * particular role type.
 *
 * @param role		Role type.
 *
 * @return		The string representation of the role.
 */
PJ_DECL(const char*) pj_ice_sess_role_name(pj_ice_sess_role role);


/**
 * This is a utility function to calculate the foundation identification
 * for a candidate.
 *
 * @param pool		Pool to allocate the foundation string.
 * @param foundation	Pointer to receive the foundation string.
 * @param type		Candidate type.
 * @param base_addr	Base address of the candidate.
 */
PJ_DECL(void) pj_ice_calc_foundation(pj_pool_t *pool,
				     pj_str_t *foundation,
				     pj_ice_cand_type type,
				     const pj_sockaddr *base_addr);

/**
 * Initialize ICE session options with library default values.
 *
 * @param opt		ICE session options.
 */
PJ_DECL(void) pj_ice_sess_options_default(pj_ice_sess_options *opt);

/**
 * Create ICE session with the specified role and number of components.
 * Application would typically need to create an ICE session before
 * sending an offer or upon receiving one. After the session is created,
 * application can register candidates to the ICE session by calling
 * #pj_ice_sess_add_cand() function.
 *
 * @param stun_cfg	The STUN configuration settings, containing among
 *			other things the timer heap instance to be used
 *			by the ICE session.
 * @param name		Optional name to identify this ICE instance in
 *			the log file.
 * @param role		ICE role.
 * @param comp_cnt	Number of components.
 * @param cb		ICE callback.
 * @param local_ufrag	Optional string to be used as local username to
 *			authenticate incoming STUN binding request. If
 *			the value is NULL, a random string will be 
 *			generated.
 * @param local_passwd	Optional string to be used as local password.
 * @param grp_lock	Optional group lock to be used by this session.
 * 			If NULL, the session will create one itself.
 * @param p_ice		Pointer to receive the ICE session instance.
 *
 * @return		PJ_SUCCESS if ICE session is created successfully.
 */
PJ_DECL(pj_status_t) pj_ice_sess_create(pj_stun_config *stun_cfg,
				        const char *name,
				        pj_ice_sess_role role,
				        unsigned comp_cnt,
				        const pj_ice_sess_cb *cb,
				        const pj_str_t *local_ufrag,
				        const pj_str_t *local_passwd,
				        pj_grp_lock_t *grp_lock,
						pj_ice_sess **p_ice,
						int tnl_timeout_msec);

/**
 * Get the value of various options of the ICE session.
 *
 * @param ice		The ICE session.
 * @param opt		The options to be initialized with the values
 *			from the ICE session.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error.
 */
PJ_DECL(pj_status_t) pj_ice_sess_get_options(pj_ice_sess *ice,
					     pj_ice_sess_options *opt);

/**
 * Specify various options for this ICE session. Application MUST only
 * call this function after the ICE session has been created but before
 * any connectivity check is started.
 *
 * Application should call #pj_ice_sess_get_options() to initialize the
 * options with their default values.
 *
 * @param ice		The ICE session.
 * @param opt		Options to be applied to the ICE session.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error.
 */
PJ_DECL(pj_status_t) pj_ice_sess_set_options(pj_ice_sess *ice,
					     const pj_ice_sess_options *opt);

/**
 * Destroy ICE session. This will cancel any connectivity checks currently
 * running, if any, and any other events scheduled by this session, as well
 * as all memory resources.
 *
 * @param ice		ICE session instance.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pj_ice_sess_destroy(pj_ice_sess *ice);


/**
 * Change session role. This happens for example when ICE session was
 * created with controlled role when receiving an offer, but it turns out
 * that the offer contains "a=ice-lite" attribute when the SDP gets
 * inspected.
 *
 * @param ice		The ICE session.
 * @param new_role	The new role to be set.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error.
 */
PJ_DECL(pj_status_t) pj_ice_sess_change_role(pj_ice_sess *ice,
					     pj_ice_sess_role new_role);


/**
 * Assign a custom preference values for ICE candidate types. By assigning
 * custom preference value, application can control the order of candidates
 * to be checked first. The default preference settings is to use 126 for 
 * host candidates, 100 for server reflexive candidates, 110 for peer 
 * reflexive candidates, an 0 for relayed candidates.
 *
 * Note that this function must be called before any candidates are added
 * to the ICE session.
 *
 * @param ice		The ICE session.
 * @param prefs		Array of candidate preference value. The values are
 *			put in the array indexed by the candidate type as
 *			specified in pj_ice_cand_type.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ice_sess_set_prefs(pj_ice_sess *ice,
					   const pj_uint8_t prefs[]);

PJ_DECL(pj_uint32_t) CALC_CAND_PRIO(pj_ice_sess *ice,
								   pj_ice_cand_type type,
								   pj_uint32_t local_pref,
								   pj_uint32_t comp_id);

/**
 * Add a candidate to this ICE session. Application must add candidates for
 * each components ID before it can start pairing the candidates and 
 * performing connectivity checks.
 *
 * @param ice		ICE session instance.
 * @param comp_id	Component ID of this candidate.
 * @param transport_id	Transport ID to be used to send packets for this
 *			candidate.
 * @param type		Candidate type.
 * @param local_pref	Local preference for this candidate, which
 *			normally should be set to 65535.
 * @param foundation	Foundation identification.
 * @param addr		The candidate address.
 * @param base_addr	The candidate's base address.
 * @param rel_addr	Optional related address.
 * @param addr_len	Length of addresses.
 * @param p_cand_id	Optional pointer to receive the candidate ID.
 * @param tcp_type	the candidate's tcp type.
 * @param adding_time	the timestamp of candidate being added.
 * @param ending_time	the timestamp of candidate status being success.
 * @param enabled the candidate is valid or not.
 *
 * @return		PJ_SUCCESS if candidate is successfully added.
 */
PJ_DECL(pj_status_t) pj_ice_sess_add_cand(pj_ice_sess *ice,
					  unsigned comp_id,
					  unsigned transport_id,
					  pj_ice_cand_type type,
					  pj_uint16_t local_pref,
					  const pj_str_t *foundation,
					  const pj_sockaddr_t *addr,
					  const pj_sockaddr_t *base_addr,
					  const pj_sockaddr_t *rel_addr,
					  int addr_len,
					  unsigned *p_cand_id,
					  pj_ice_cand_tcp_type tcp_type,
					  pj_time_val adding_time,
					  pj_time_val ending_time,
					  pj_bool_t enabled);

/**
 * Find default candidate for the specified component ID, using this
 * rule:
 *  - if the component has a successful candidate pair, then the
 *    local candidate of this pair will be returned.
 *  - otherwise a relay, reflexive, or host candidate will be selected 
 *    on that specified order.
 *
 * @param ice		The ICE session instance.
 * @param comp_id	The component ID.
 * @param p_cand_id	Pointer to receive the candidate ID.
 *
 * @return		PJ_SUCCESS if a candidate has been selected.
 */
PJ_DECL(pj_status_t) pj_ice_sess_find_default_cand(pj_ice_sess *ice,
						   unsigned comp_id,
						   int *p_cand_id);

PJ_DECL(pj_timestamp) CALC_CHECK_PRIO(const pj_ice_sess *ice, 
									const pj_ice_sess_cand *lcand,
									const pj_ice_sess_cand *rcand);

PJ_DECL(const char *)dump_check(char *buffer, unsigned bufsize,
							   const pj_ice_sess_checklist *clist,
							   const pj_ice_sess_check *check);

/**
 * Pair the local and remote candidates to create check list. Application
 * typically would call this function after receiving SDP containing ICE
 * candidates from the remote host (either upon receiving the initial
 * offer, for UAS, or upon receiving the answer, for UAC).
 *
 * Note that ICE connectivity check will not start until application calls
 * #pj_ice_sess_start_check().
 *
 * @param ice		ICE session instance.
 * @param rem_ufrag	Remote ufrag, as seen in the SDP received from 
 *			the remote agent.
 * @param rem_passwd	Remote password, as seen in the SDP received from
 *			the remote agent.
 * @param rem_cand_cnt	Number of remote candidates.
 * @param rem_cand	Remote candidate array. Remote candidates are
 *			gathered from the SDP received from the remote 
 *			agent.
 *
 * @return		PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) 
pj_ice_sess_create_check_list(pj_ice_sess *ice,
			      const pj_str_t *rem_ufrag,
			      const pj_str_t *rem_passwd,
			      unsigned rem_cand_cnt,
				  const pj_ice_sess_cand rem_cand[],
				  int use_upnp_flag);

// 2013-10-17 DEAN
PJ_DECL(pj_status_t) pj_ice_sess_create_cached_check_list(
	pj_ice_sess *ice,
	const pj_str_t *rem_ufrag,
	const pj_str_t *rem_passwd,
	pj_ice_sess_check *cached_check);

/**
 * Start ICE periodic check. This function will return immediately, and
 * application will be notified about the connectivity check status in
 * #pj_ice_sess_cb callback.
 *
 * @param ice		The ICE session instance.
 *
 * @return		PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ice_sess_start_check(pj_ice_sess *ice);

PJ_DECL(pj_status_t) pj_ice_sess_start_check2(pj_ice_sess *ice, pj_bool_t check_cached_list);


/**
 * Send data using this ICE session. If ICE checks have not produced a
 * valid check for the specified component ID, this function will return
 * with failure. Otherwise ICE session will send the packet to remote
 * destination using the nominated local candidate for the specified
 * component.
 *
 * This function will in turn call \a on_tx_pkt function in
 * #pj_ice_sess_cb callback to actually send the packet to the wire.
 *
 * @param ice		The ICE session.
 * @param comp_id	Component ID.
 * @param data		The data or packet to be sent.
 * @param data_len	Size of data or packet, in bytes.
 *
 * @return		PJ_SUCCESS if data is sent successfully.
 */
PJ_DECL(pj_status_t) pj_ice_sess_send_data(pj_ice_sess *ice,
					   unsigned comp_id,
					   const void *data,
					   pj_size_t data_len);

PJ_DECL(pj_status_t) pj_ice_sess_on_rx_pkt2(pj_ice_sess *ice,
										  unsigned comp_id,
										  unsigned transport_id,
										  void *pkt,
										  pj_size_t pkt_size,
										  const pj_sockaddr_t *src_addr,
										  int src_addr_len,
										  pj_stun_session *stun_sess) ;

/**
 * Report the arrival of packet to the ICE session. Since ICE session
 * itself doesn't have any transports, it relies on application or
 * higher layer component to give incoming packets to the ICE session.
 * If the packet is not a STUN packet, this packet will be given back
 * to application via \a on_rx_data() callback in #pj_ice_sess_cb.
 *
 * @param ice		The ICE session.
 * @param comp_id	Component ID.
 * @param transport_id	Number to identify where this packet was received
 *			from. This parameter will be returned back to
 *			application in \a on_tx_pkt() callback.
 * @param pkt		Incoming packet.
 * @param pkt_size	Size of incoming packet.
 * @param src_addr	Source address of the packet.
 * @param src_addr_len	Length of the address.
 *
 * @return		PJ_SUCCESS or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_ice_sess_on_rx_pkt(pj_ice_sess *ice,
					   unsigned comp_id,
					   unsigned transport_id,
					   void *pkt,
					   pj_size_t pkt_size,
					   const pj_sockaddr_t *src_addr,
					   int src_addr_len);

PJ_DECL(pj_bool_t) pj_ice_sess_local_path_selected(pj_ice_sess *ice) ;


PJ_END_DECL


#endif	/* __PJNATH_ICE_SESSION_H__ */

