/* $Id: sip_transport.h 3954 2012-02-18 02:12:22Z bennylp $ */
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
#ifndef __PJSIP_SIP_TRANSPORT_H__
#define __PJSIP_SIP_TRANSPORT_H__

/**
 * @file sip_transport.h
 * @brief SIP Transport
 */

#include <pjsip/sip_msg.h>
#include <pjsip/sip_parser.h>
#include <pjsip/sip_resolve.h>
#include <pj/sock.h>
#include <pj/list.h>
#include <pj/ioqueue.h>
#include <pj/timer.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_TRANSPORT Transport
 * @ingroup PJSIP_CORE
 * @brief This is the transport framework.
 *
 * The transport framework is fully extensible. Please see
 * <A HREF="/docs.htm">PJSIP Developer's Guide</A> PDF
 * document for more information.
 *
 * Application MUST register at least one transport to PJSIP before any
 * messages can be sent or received. Please see @ref PJSIP_TRANSPORT_UDP
 * on how to create/register UDP transport to the transport framework.
 *
 * @{
 */

/*****************************************************************************
 *
 * GENERAL TRANSPORT (NAMES, TYPES, ETC.)
 *
 *****************************************************************************/

/*
 * Forward declaration for transport factory (since it is referenced by
 * the transport factory itself).
 */
typedef struct pjsip_tpfactory pjsip_tpfactory;


/**
 * Flags for SIP transports.
 */
enum pjsip_transport_flags_e
{
    PJSIP_TRANSPORT_RELIABLE	    = 1,    /**< Transport is reliable.	    */
    PJSIP_TRANSPORT_SECURE	    = 2,    /**< Transport is secure.	    */
    PJSIP_TRANSPORT_DATAGRAM	    = 4     /**< Datagram based transport.  
					         (it's also assumed to be 
						 connectionless)	    */
};

/**
 * Check if transport tp is reliable.
 */
#define PJSIP_TRANSPORT_IS_RELIABLE(tp)	    \
	    ((tp)->flag & PJSIP_TRANSPORT_RELIABLE)

/**
 * Check if transport tp is secure.
 */
#define PJSIP_TRANSPORT_IS_SECURE(tp)	    \
	    ((tp)->flag & PJSIP_TRANSPORT_SECURE)

/**
 * Register new transport type to PJSIP. The PJSIP transport framework
 * contains the info for some standard transports, as declared by
 * #pjsip_transport_type_e. Application may use non-standard transport
 * with PJSIP, but before it does so, it must register the information
 * about the new transport type to PJSIP by calling this function.
 *
 * @param tp_flag   The flags describing characteristics of this
 *		    transport type.
 * @param tp_name   Transport type name.
 * @param def_port  Default port to be used for the transport.
 * @param p_tp_type On successful registration, it will be filled with
 *		    the registered type. This argument is optional.
 *
 * @return	    PJ_SUCCESS if registration is successful, or
 *		    PJSIP_ETYPEEXISTS if the same transport type has
 *		    already been registered.
 */
PJ_DECL(pj_status_t) pjsip_transport_register_type(unsigned tp_flag,
						   const char *tp_name,
						   int def_port,
						   int *p_tp_type);


/**
 * Get the transport type from the transport name.
 *
 * @param name	    Transport name, such as "TCP", or "UDP".
 *
 * @return	    The transport type, or PJSIP_TRANSPORT_UNSPECIFIED if 
 *		    the name is not recognized as the name of supported 
 *		    transport.
 */
PJ_DECL(pjsip_transport_type_e) 
pjsip_transport_get_type_from_name(const pj_str_t *name);

/**
 * Get the transport type for the specified flags.
 *
 * @param flag	    The transport flag.
 *
 * @return	    Transport type.
 */
PJ_DECL(pjsip_transport_type_e) 
pjsip_transport_get_type_from_flag(unsigned flag);

/**
 * Get the socket address family of a given transport type.
 *
 * @param type	    Transport type.
 *
 * @return	    Transport type.
 */
PJ_DECL(int) pjsip_transport_type_get_af(pjsip_transport_type_e type);

/**
 * Get transport flag from type.
 *
 * @param type	    Transport type.
 *
 * @return	    Transport flags.
 */
PJ_DECL(unsigned)
pjsip_transport_get_flag_from_type( pjsip_transport_type_e type );

/**
 * Get the default SIP port number for the specified type.
 *
 * @param type	    Transport type.
 *
 * @return	    The port number, which is the default SIP port number for
 *		    the specified type.
 */
PJ_DECL(int) 
pjsip_transport_get_default_port_for_type(pjsip_transport_type_e type);

/**
 * Get transport type name.
 *
 * @param t	    Transport type.
 *
 * @return	    Transport name.
 */
PJ_DECL(const char*) pjsip_transport_get_type_name(pjsip_transport_type_e t);

/**
 * Get longer description for the specified transport type.
 *
 * @param t	    Transport type.
 *
 * @return	    Transport description.
 */
PJ_DECL(const char*) pjsip_transport_get_type_desc(pjsip_transport_type_e t);



/*****************************************************************************
 *
 * TRANSPORT SELECTOR.
 *
 *****************************************************************************/

/**
 * This structure describes the type of data in pjsip_tpselector.
 */
typedef enum pjsip_tpselector_type
{
    /** Transport is not specified. */
    PJSIP_TPSELECTOR_NONE,

    /** Use the specific transport to send request. */
    PJSIP_TPSELECTOR_TRANSPORT,

    /** Use the specific listener to send request. */
    PJSIP_TPSELECTOR_LISTENER,

} pjsip_tpselector_type;


/**
 * This structure describes the transport/listener preference to be used
 * when sending outgoing requests.
 *
 * Normally transport will be selected automatically according to rules about
 * sending requests. But some applications (such as proxies or B2BUAs) may 
 * want to explicitly use specific transport to send requests, for example
 * when they want to make sure that outgoing request should go from a specific
 * network interface.
 *
 * The pjsip_tpselector structure is used for that purpose, i.e. to allow
 * application specificly request that a particular transport/listener
 * should be used to send request. This structure is used when calling
 * pjsip_tsx_set_transport() and pjsip_dlg_set_transport().
 */
typedef struct pjsip_tpselector
{
    /** The type of data in the union */
    pjsip_tpselector_type   type;

    /** Union representing the transport/listener criteria to be used. */
    union {
	pjsip_transport	*transport;
	pjsip_tpfactory	*listener;
	void		*ptr;
    } u;

} pjsip_tpselector;


/**
 * Add transport/listener reference in the selector to prevent the specified
 * transport/listener from being destroyed while application still has
 * reference to it.
 *
 * @param sel	The transport selector.
 */
PJ_DECL(void) pjsip_tpselector_add_ref(pjsip_tpselector *sel);


/**
 * Decrement transport/listener reference in the selector.
 * @param sel	The transport selector
 */
PJ_DECL(void) pjsip_tpselector_dec_ref(pjsip_tpselector *sel);


/*****************************************************************************
 *
 * RECEIVE DATA BUFFER.
 *
 *****************************************************************************/

/** 
 * A customized ioqueue async operation key which is used by transport
 * to locate rdata when a pending read operation completes.
 */
typedef struct pjsip_rx_data_op_key
{
    pj_ioqueue_op_key_t		op_key;	/**< ioqueue op_key.		*/
    pjsip_rx_data	       *rdata;	/**< rdata associated with this */
} pjsip_rx_data_op_key;


/**
 * Incoming message buffer.
 * This structure keep all the information regarding the received message. This
 * buffer lifetime is only very short, normally after the transaction has been
 * called, this buffer will be deleted/recycled. So care must be taken when
 * allocating storage from the pool of this buffer.
 */
struct pjsip_rx_data
{

    /**
     * tp_info is part of rdata that remains static for the duration of the
     * buffer. It is initialized when the buffer was created by transport.
     */
    struct 
    {
	/** Memory pool for this buffer. */
	pj_pool_t		*pool;

	/** The transport object which received this packet. */
	pjsip_transport		*transport;

	/** Other transport specific data to be attached to this buffer. */
	void			*tp_data;

	/** Ioqueue key. */
	pjsip_rx_data_op_key	 op_key;

    } tp_info;


    /**
     * pkt_info is initialized by transport when it receives an incoming
     * packet.
     */
    struct
    {
	/** Time when the message was received. */
	pj_time_val		 timestamp;

	/** Pointer to the original packet. */
	char			 packet[PJSIP_MAX_PKT_LEN];

	/** Zero termination for the packet. */
	pj_uint32_t		 zero;

	/** The length of the packet received. */
	pj_ssize_t		 len;

	/** The source address from which the packet was received. */
	pj_sockaddr		 src_addr;

	/** The length of the source address. */
	int			 src_addr_len;

	/** The IP source address string (NULL terminated). */
	char			 src_name[PJ_INET6_ADDRSTRLEN];

	/** The IP source port number. */
	int			 src_port;

    } pkt_info;


    /**
     * msg_info is initialized by transport mgr (tpmgr) before this buffer
     * is passed to endpoint.
     */
    struct
    {
	/** Start of msg buffer. */
	char			*msg_buf;

	/** Length fo message. */
	int			 len;

	/** The parsed message, if any. */
	pjsip_msg		*msg;

	/** Short description about the message. 
	 *  Application should use #pjsip_rx_data_get_info() instead.
	 */
	char			*info;

	/** The Call-ID header as found in the message. */
	pjsip_cid_hdr		*cid;

	/** The From header as found in the message. */
	pjsip_from_hdr		*from;

	/** The To header as found in the message. */
	pjsip_to_hdr		*to;

	/** The topmost Via header as found in the message. */
	pjsip_via_hdr		*via;

	/** The CSeq header as found in the message. */
	pjsip_cseq_hdr		*cseq;

	/** Max forwards header. */
	pjsip_max_fwd_hdr	*max_fwd;

	/** The first route header. */
	pjsip_route_hdr		*route;

	/** The first record-route header. */
	pjsip_rr_hdr		*record_route;

	/** Content-type header. */
	pjsip_ctype_hdr		*ctype;

	/** Content-length header. */
	pjsip_clen_hdr		*clen;

	/** "Require" header containing aggregates of all Require
	 *  headers found in the message, or NULL. 
	 */
	pjsip_require_hdr	*require;

	/** "Supported" header containing aggregates of all Supported
	 *  headers found in the message, or NULL. 
	 */
	pjsip_supported_hdr	*supported;

	/** "Supported" header containing aggregates of all Supported
	 *  headers found in the message, or NULL. 
	 */
	pjsip_tnl_supported_hdr	*tnl_supported;

	/** "User-Agent" header containing aggregates of all Supported
	 *  headers found in the message, or NULL. 
	 */
	pjsip_user_agent_hdr *user_agent;

	/** The list of error generated by the parser when parsing 
	    this message. 
	 */
	pjsip_parser_err_report parse_err;

    } msg_info;


    /**
     * endpt_info is initialized by endpoint after this buffer reaches
     * endpoint.
     */
    struct
    {
		/** 
		 * Data attached by modules to this message. 
		 */
		void	*mod_data[PJSIP_MAX_MODULE];

    } endpt_info;

};

/**
 * Get printable information about the message in the rdata.
 *
 * @param rdata	    The receive data buffer.
 *
 * @return	    Printable information.
 */
PJ_DECL(char*) pjsip_rx_data_get_info(pjsip_rx_data *rdata);


/*****************************************************************************
 *
 * TRANSMIT DATA BUFFER MANIPULATION.
 *
 *****************************************************************************/

/** Customized ioqueue async operation key, used by transport to keep
 *  callback parameters.
 */
typedef struct pjsip_tx_data_op_key
{
    /** ioqueue pending operation key. */
    pj_ioqueue_op_key_t	    key;

    /** Transmit data associated with this key. */
    pjsip_tx_data	   *tdata;

    /** Arbitrary token (attached by transport) */
    void		   *token;

    /** Callback to be called when pending transmit operation has
        completed.
     */
    void		  (*callback)(pjsip_transport*,void*,pj_ssize_t);
} pjsip_tx_data_op_key;


/**
 * Data structure for sending outgoing message. Application normally creates
 * this buffer by calling #pjsip_endpt_create_tdata.
 *
 * The lifetime of this buffer is controlled by the reference counter in this
 * structure, which is manipulated by calling #pjsip_tx_data_add_ref and
 * #pjsip_tx_data_dec_ref. When the reference counter has reached zero, then
 * this buffer will be destroyed.
 *
 * A transaction object normally will add reference counter to this buffer
 * when application calls #pjsip_tsx_send_msg, because it needs to keep the
 * message for retransmission. The transaction will release the reference
 * counter once its state has reached final state.
 */
struct pjsip_tx_data
{
	int inst_id;

    /** This is for transmission queue; it's managed by transports. */
    PJ_DECL_LIST_MEMBER(struct pjsip_tx_data);

    /** Memory pool for this buffer. */
    pj_pool_t		*pool;

    /** A name to identify this buffer. */
    char		 obj_name[PJ_MAX_OBJ_NAME];

    /** Short information describing this buffer and the message in it. 
     *  Application should use #pjsip_tx_data_get_info() instead of
     *  directly accessing this member.
     */
    char		*info;

    /** For response message, this contains the reference to timestamp when 
     *  the original request message was received. The value of this field
     *  is set when application creates response message to a request by
     *  calling #pjsip_endpt_create_response.
     */
    pj_time_val		 rx_timestamp;

    /** The transport manager for this buffer. */
    pjsip_tpmgr		*mgr;

    /** Ioqueue asynchronous operation key. */
    pjsip_tx_data_op_key op_key;

    /** Lock object. */
    pj_lock_t		*lock;

    /** The message in this buffer. */
    pjsip_msg 		*msg;

    /** Strict route header saved by #pjsip_process_route_set(), to be
     *  restored by #pjsip_restore_strict_route_set().
     */
    pjsip_route_hdr	*saved_strict_route;

    /** Buffer to the printed text representation of the message. When the
     *  content of this buffer is set, then the transport will send the content
     *  of this buffer instead of re-printing the message structure. If the
     *  message structure has changed, then application must invalidate this
     *  buffer by calling #pjsip_tx_data_invalidate_msg.
     */
    pjsip_buffer	 buf;

    /** Reference counter. */
    pj_atomic_t		*ref_cnt;

    /** Being processed by transport? */
    int			 is_pending;

    /** Transport manager internal. */
    void		*token;

    /** Callback to be called when this tx_data has been transmitted.	*/
    void	       (*cb)(void*, pjsip_tx_data*, pj_ssize_t);

    /** Destination information, to be used to determine the network address
     *  of the message. For a request, this information is  initialized when
     *  the request is sent with #pjsip_endpt_send_request_stateless() and
     *  network address is resolved. For CANCEL request, this information
     *  will be copied from the original INVITE to make sure that the CANCEL
     *  request goes to the same physical network address as the INVITE
     *  request.
     */
    struct
    {
	/** Server name. 
	 */
	pj_str_t		 name;

	/** Server addresses resolved. 
	 */
	pjsip_server_addresses   addr;

	/** Current server address being tried. 
	 */
	unsigned cur_addr;

    } dest_info;

    /** Transport information, only valid during on_tx_request() and 
     *  on_tx_response() callback.
     */
    struct
    {
	pjsip_transport	    *transport;	    /**< Transport being used.	*/
	pj_sockaddr	     dst_addr;	    /**< Destination address.	*/
	int		     dst_addr_len;  /**< Length of address.	*/
	char		     dst_name[PJ_INET6_ADDRSTRLEN]; /**< Destination address.	*/
	int		     dst_port;	    /**< Destination port.	*/
    } tp_info;

    /** 
     * Transport selector, to specify which transport to be used. 
     * The value here must be set with pjsip_tx_data_set_transport(),
     * to allow reference counter to be set properly.
     */
    pjsip_tpselector	    tp_sel;

    /**
     * Special flag to indicate that this transmit data is a request that has
     * been updated with proper authentication response and is ready to be
     * sent for retry.
     */
    pj_bool_t		    auth_retry;

    /**
     * Arbitrary data attached by PJSIP modules.
     */
    void		    *mod_data[PJSIP_MAX_MODULE];
};


/**
 * Create a new, blank transmit buffer. The reference count is initialized
 * to zero.
 *
 * @param mgr		The transport manager.
 * @param tdata		Pointer to receive transmit data.
 *
 * @return		PJ_SUCCESS, or the appropriate error code.
 *
 * @see pjsip_endpt_create_tdata
 */
PJ_DECL(pj_status_t) pjsip_tx_data_create( pjsip_tpmgr *mgr,
					   pjsip_tx_data **tdata );

/**
 * Add reference counter to the transmit buffer. The reference counter controls
 * the life time of the buffer, ie. when the counter reaches zero, then it 
 * will be destroyed.
 *
 * @param tdata	    The transmit buffer.
 */
PJ_DECL(void) pjsip_tx_data_add_ref( pjsip_tx_data *tdata );

/**
 * Decrement reference counter of the transmit buffer.
 * When the transmit buffer is no longer used, it will be destroyed and
 * caller is informed with PJSIP_EBUFDESTROYED return status.
 *
 * @param tdata	    The transmit buffer data.
 * @return	    This function will always succeeded eventhough the return
 *		    status is non-zero. A status PJSIP_EBUFDESTROYED will be
 *		    returned to inform that buffer is destroyed.
 */
PJ_DECL(pj_status_t) pjsip_tx_data_dec_ref( pjsip_tx_data *tdata );

/**
 * Print the SIP message to transmit data buffer's internal buffer. This
 * may allocate memory for the buffer, if the buffer has not been allocated
 * yet, and encode the SIP message to that buffer.
 *
 * @param tdata	    The transmit buffer.
 *
 * @return	    PJ_SUCCESS on success of the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_tx_data_encode(pjsip_tx_data *tdata);

/**
 * Check if transmit data buffer contains a valid message.
 *
 * @param tdata	    The transmit buffer.
 * @return	    Non-zero (PJ_TRUE) if buffer contains a valid message.
 */
PJ_DECL(pj_bool_t) pjsip_tx_data_is_valid( pjsip_tx_data *tdata );

/**
 * Invalidate the print buffer to force message to be re-printed. Call
 * when the message has changed after it has been printed to buffer. The
 * message is printed to buffer normally by transport when it is about to be 
 * sent to the wire. Subsequent sending of the message will not cause
 * the message to be re-printed, unless application invalidates the buffer
 * by calling this function.
 *
 * @param tdata	    The transmit buffer.
 */
PJ_DECL(void) pjsip_tx_data_invalidate_msg( pjsip_tx_data *tdata );

/**
 * Get short printable info about the transmit data. This will normally return
 * short information about the message.
 *
 * @param tdata	    The transmit buffer.
 *
 * @return	    Null terminated info string.
 */
PJ_DECL(char*) pjsip_tx_data_get_info( pjsip_tx_data *tdata );

/**
 * Set the explicit transport to be used when sending this transmit data.
 * Application should not need to call this function, but rather use
 * pjsip_tsx_set_transport() and pjsip_dlg_set_transport() instead (which
 * will call this function).
 *
 * @param tdata	    The transmit buffer.
 * @param sel	    Transport selector.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_tx_data_set_transport(pjsip_tx_data *tdata,
						 const pjsip_tpselector *sel);


/*****************************************************************************
 *
 * TRANSPORT
 *
 *****************************************************************************/
/**
 * Type of callback to receive transport operation status.
 */
typedef void (*pjsip_transport_callback)(pjsip_transport *tp, void *token,
                                         pj_ssize_t sent_bytes);

/**
 * This structure describes transport key to be registered to hash table.
 */
typedef struct pjsip_transport_key
{
    /**
     * Transport type.
     */
    long		    type;

    /**
     * Destination address.
     */
    pj_sockaddr		    rem_addr;

    /**
     * Destination address.
     */
    //char		    rem_addr_str[PJ_INET6_ADDRSTRLEN+10];

} pjsip_transport_key;

/**
 * DEAN added to replace pjsip_transport_key2.
 * In this structure we use a char array for saving ip:port information.
 * This structure describes transport key to be registered to hash table.
 */
typedef struct pjsip_transport_key2
{
    /**
     * Transport type.
     */
    long		    type;

    /**
     * string format destination address.
     */
    char		    rem_addr_str[PJ_INET6_ADDRSTRLEN+10];

} pjsip_transport_key2;


/**
 * Enumeration of transport direction types.
 */
typedef enum pjsip_transport_dir
{
    PJSIP_TP_DIR_NONE,		    /**< Direction not set, normally used by
				         connectionless transports such as 
					 UDP transport.			    */
    PJSIP_TP_DIR_OUTGOING,	    /**< Outgoing connection or client mode,
				         this is only for connection-oriented 
					 transports.			    */
    PJSIP_TP_DIR_INCOMING,	    /**< Incoming connection or server mode,
					 this is only for connection-oriented
					 transports.			    */
} pjsip_transport_dir;


/**
 * This structure represent the "public" interface of a SIP transport.
 * Applications normally extend this structure to include transport
 * specific members.
 */
struct pjsip_transport
{
    char		    obj_name[PJ_MAX_OBJ_NAME];	/**< Name. */

    pj_pool_t		   *pool;	    /**< Pool used by transport.    */
    pj_atomic_t		   *ref_cnt;	    /**< Reference counter.	    */
    pj_lock_t		   *lock;	    /**< Lock object.		    */
    pj_bool_t		    tracing;	    /**< Tracing enabled?	    */
    pj_bool_t		    is_shutdown;    /**< Being shutdown?	    */
    pj_bool_t		    is_destroying;  /**< Destroy in progress?	    */

    /** Key for indexing this transport in hash table. */
    pjsip_transport_key	    key;

    char		   *type_name;	    /**< Type name.		    */
    unsigned		    flag;	    /**< #pjsip_transport_flags_e   */
    char		   *info;	    /**< Transport info/description.*/

    int			    addr_len;	    /**< Length of addresses.	    */
    pj_sockaddr		    local_addr;	    /**< Bound address.		    */
    pjsip_host_port	    local_name;	    /**< Published name (eg. STUN). */
    pjsip_host_port	    remote_name;    /**< Remote address name.	    */
    pjsip_transport_dir	    dir;	    /**< Connection direction.	    */
    
    pjsip_endpoint	   *endpt;	    /**< Endpoint instance.	    */
    pjsip_tpmgr		   *tpmgr;	    /**< Transport manager.	    */
    pj_timer_entry	    idle_timer;	    /**< Timer when ref cnt is zero.*/

    void		   *data;	    /**< Internal transport data.   */

    /**
     * Function to be called by transport manager to send SIP message.
     *
     * @param transport	    The transport to send the message.
     * @param packet	    The buffer to send.
     * @param length	    The length of the buffer to send.
     * @param op_key	    Completion token, which will be supplied to
     *			    caller when pending send operation completes.
     * @param rem_addr	    The remote destination address.
     * @param addr_len	    Size of remote address.
     * @param callback	    If supplied, the callback will be called
     *			    once a pending transmission has completed. If
     *			    the function completes immediately (i.e. return
     *			    code is not PJ_EPENDING), the callback will not
     *			    be called.
     *
     * @return		    Should return PJ_SUCCESS only if data has been
     *			    succesfully queued to operating system for 
     *			    transmission. Otherwise it may return PJ_EPENDING
     *			    if the underlying transport can not send the
     *			    data immediately and will send it later, which in
     *			    this case caller doesn't have to do anything 
     *			    except wait the calback to be called, if it 
     *			    supplies one.
     *			    Other return values indicate the error code.
     */
    pj_status_t (*send_msg)(pjsip_transport *transport, 
			    pjsip_tx_data *tdata,
			    const pj_sockaddr_t *rem_addr,
			    int addr_len,
			    void *token,
			    pjsip_transport_callback callback);

    /**
     * Instruct the transport to initiate graceful shutdown procedure.
     * After all objects release their reference to this transport,
     * the transport will be deleted.
     *
     * Note that application MUST use #pjsip_transport_shutdown() instead.
     *
     * @param transport	    The transport.
     *
     * @return		    PJ_SUCCESS on success.
     */
    pj_status_t (*do_shutdown)(pjsip_transport *transport);

    /**
     * Forcefully destroy this transport regardless whether there are
     * objects that currently use this transport. This function should only
     * be called by transport manager or other internal objects (such as the
     * transport itself) who know what they're doing. Application should use
     * #pjsip_transport_shutdown() instead.
     *
     * @param transport	    The transport.
     *
     * @return		    PJ_SUCCESS on success.
     */
    pj_status_t (*destroy)(pjsip_transport *transport);

    /*
     * Application may extend this structure..
     */
};


/**
 * Register a transport instance to the transport manager. This function
 * is normally called by the transport instance when it is created
 * by application.
 *
 * @param mgr		The transport manager.
 * @param tp		The new transport to be registered.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_transport_register( pjsip_tpmgr *mgr,
					       pjsip_transport *tp );


/**
 * Start graceful shutdown procedure for this transport. After graceful
 * shutdown has been initiated, no new reference can be obtained for
 * the transport. However, existing objects that currently uses the
 * transport may still use this transport to send and receive packets.
 *
 * After all objects release their reference to this transport,
 * the transport will be destroyed immediately.
 *
 * @param tp		    The transport.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_transport_shutdown(pjsip_transport *tp);

/**
 * Destroy a transport when there is no object currently uses the transport.
 * This function is normally called internally by transport manager or the
 * transport itself. Application should use #pjsip_transport_shutdown()
 * instead.
 *
 * @param tp		The transport instance.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 *			Some of possible errors are PJSIP_EBUSY if the 
 *			transport's reference counter is not zero.
 */
PJ_DECL(pj_status_t) pjsip_transport_destroy( pjsip_transport *tp);

/**
 * Add reference counter to the specified transport. Any objects that wishes
 * to keep the reference of the transport MUST increment the transport's
 * reference counter to prevent it from being destroyed.
 *
 * @param tp		The transport instance.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_transport_add_ref( pjsip_transport *tp );

/**
 * Decrement reference counter of the specified transport. When an object no
 * longer want to keep the reference to the transport, it must decrement the
 * reference counter. When the reference counter of the transport reaches 
 * zero, the transport manager will start the idle timer to destroy the
 * transport if no objects acquire the reference counter during the idle
 * interval.
 *
 * @param tp		The transport instance.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_transport_dec_ref( pjsip_transport *tp );


/**
 * This function is called by transport instances to report an incoming 
 * packet to the transport manager. The transport manager then would try to
 * parse all SIP messages in the packet, and for each parsed SIP message, it
 * would report the message to the SIP endpoint (#pjsip_endpoint).
 *
 * @param mgr		The transport manager instance.
 * @param rdata		The receive data buffer containing the packet. The
 *			transport MUST fully initialize tp_info and pkt_info
 *			member of the rdata.
 *
 * @return		The number of bytes successfully processed from the
 *			packet. If the transport is datagram oriented, the
 *			value will be equal to the size of the packet. For
 *			stream oriented transport (e.g. TCP, TLS), the value
 *			returned may be less than the packet size, if 
 *			partial message is received. The transport then MUST
 *			keep the remainder part and report it again to
 *			this function once more data/packet is received.
 */
PJ_DECL(pj_ssize_t) pjsip_tpmgr_receive_packet(pjsip_tpmgr *mgr,
					       pjsip_rx_data *rdata);


/*****************************************************************************
 *
 * TRANSPORT FACTORY
 *
 *****************************************************************************/


/**
 * A transport factory is normally used for connection oriented transports
 * (such as TCP or TLS) to create instances of transports. It registers
 * a new transport type to the transport manager, and the transport manager
 * would ask the factory to create a transport instance when it received
 * command from application to send a SIP message using the specified
 * transport type.
 */
struct pjsip_tpfactory
{
    /** This list is managed by transport manager. */
    PJ_DECL_LIST_MEMBER(struct pjsip_tpfactory);

    char		    obj_name[PJ_MAX_OBJ_NAME];	/**< Name.	*/

    pj_pool_t		   *pool;	    /**< Owned memory pool.	*/
    pj_lock_t		   *lock;	    /**< Lock object.		*/

    pjsip_transport_type_e  type;	    /**< Transport type.	*/
    char		   *type_name;      /**< Type string name.	*/
    unsigned		    flag;	    /**< Transport flag.	*/

    pj_sockaddr		    local_addr;	    /**< Bound address.		*/
    pjsip_host_port	    addr_name;	    /**< Published name.	*/

    /**
     * Create new outbound connection suitable for sending SIP message
     * to specified remote address.
     * Note that the factory is responsible for both creating the
     * transport and registering it to the transport manager.
     */
    pj_status_t (*create_transport)(pjsip_tpfactory *factory,
				    pjsip_tpmgr *mgr,
				    pjsip_endpoint *endpt,
				    const pj_sockaddr *rem_addr,
				    int addr_len,
				    pjsip_transport **transport);

    /**
     * Create new outbound connection suitable for sending SIP message
     * to specified remote address by also considering outgoing SIP 
     * message data.
     * Note that the factory is responsible for both creating the
     * transport and registering it to the transport manager.
     */
    pj_status_t (*create_transport2)(pjsip_tpfactory *factory,
				     pjsip_tpmgr *mgr,
				     pjsip_endpoint *endpt,
				     const pj_sockaddr *rem_addr,
				     int addr_len,
				     pjsip_tx_data *tdata,
				     pjsip_transport **transport);

    /**
     * Destroy the listener.
     */
    pj_status_t (*destroy)(pjsip_tpfactory *factory);

    /*
     * Application may extend this structure..
     */
};



/**
 * Register a transport factory.
 *
 * @param mgr		The transport manager.
 * @param tpf		Transport factory.
 *
 * @return		PJ_SUCCESS if listener was successfully created.
 */
PJ_DECL(pj_status_t) pjsip_tpmgr_register_tpfactory(pjsip_tpmgr *mgr,
						    pjsip_tpfactory *tpf);

/**
 * Unregister factory.
 *
 * @param mgr		The transport manager.
 * @param tpf		Transport factory.
 *
 * @return		PJ_SUCCESS is sucessfully unregistered.
 */
PJ_DECL(pj_status_t) pjsip_tpmgr_unregister_tpfactory(pjsip_tpmgr *mgr,
						      pjsip_tpfactory *tpf);


/*****************************************************************************
 *
 * TRANSPORT MANAGER
 *
 *****************************************************************************/

/**
 * Type of callback to be called when transport manager receives incoming
 * SIP message.
 *
 * @param ep	    Endpoint.
 * @param status    Receiption status.
 * @param rd	    Received packet.
 */
typedef void (*pjsip_rx_callback)(pjsip_endpoint *ep, pj_status_t status, 
				  pjsip_rx_data *rd);

/**
 * Type of callback to be called before transport manager is about
 * to transmit SIP message.
 *
 * @param ep	    Endpoint.
 * @param td	    Transmit data.
 */
typedef pj_status_t (*pjsip_tx_callback)(pjsip_endpoint *ep, pjsip_tx_data*td);

/**
 * Create a transport manager. Normally application doesn't need to call
 * this function directly, since a transport manager will be created and
 * destroyed automatically by the SIP endpoint.
 *
 * @param pool	    Pool.
 * @param endpt	    Endpoint instance.
 * @param rx_cb	    Callback to receive incoming message.
 * @param tx_cb	    Callback to be called before transport manager is sending
 *		    outgoing message.
 * @param p_mgr	    Pointer to receive the new transport manager.
 *
 * @return	    PJ_SUCCESS or the appropriate error code on error.
 */
PJ_DECL(pj_status_t) pjsip_tpmgr_create( pj_pool_t *pool,
					 pjsip_endpoint * endpt,
					 pjsip_rx_callback rx_cb,
					 pjsip_tx_callback tx_cb,
					 pjsip_tpmgr **p_mgr);


/**
 * Find out the appropriate local address info (IP address and port) to
 * advertise in Contact header based on the remote address to be 
 * contacted. The local address info would be the address name of the
 * transport or listener which will be used to send the request.
 *
 * In this implementation, it will only select the transport based on
 * the transport type in the request.
 *
 * @param tpmgr	    The transport manager.
 * @param pool	    Pool to allocate memory for the IP address.
 * @param type	    Destination address to contact.
 * @param sel	    Optional pointer to prefered transport, if any.
 * @param ip_addr   Pointer to receive the IP address.
 * @param port	    Pointer to receive the port number.
 *
 * @return	    PJ_SUCCESS, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_tpmgr_find_local_addr( pjsip_tpmgr *tpmgr,
						  pj_pool_t *pool,
						  pjsip_transport_type_e type,
						  const pjsip_tpselector *sel,
						  pj_str_t *ip_addr,
						  int *port);

/**
 * Return number of transports currently registered to the transport
 * manager.
 *
 * @param mgr	    The transport manager.
 *
 * @return	    Number of transports.
 */
PJ_DECL(unsigned) pjsip_tpmgr_get_transport_count(pjsip_tpmgr *mgr);


/**
 * Destroy a transport manager. Normally application doesn't need to call
 * this function directly, since a transport manager will be created and
 * destroyed automatically by the SIP endpoint.
 *
 * @param mgr	    The transport manager.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_tpmgr_destroy(pjsip_tpmgr *mgr);


/**
 * Dump transport info and status to log.
 *
 * @param mgr	    The transport manager.
 */
PJ_DECL(void) pjsip_tpmgr_dump_transports(pjsip_tpmgr *mgr);


/*****************************************************************************
 *
 * PUBLIC API
 *
 *****************************************************************************/


/**
 * Find transport to be used to send message to remote destination. If no
 * suitable transport is found, a new one will be created.
 *
 * This is an internal function since normally application doesn't have access
 * to transport manager. Application should use pjsip_endpt_acquire_transport()
 * instead.
 *
 * @param mgr	    The transport manager instance.
 * @param type	    The type of transport to be acquired.
 * @param remote    The remote address to send message to.
 * @param addr_len  Length of the remote address.
 * @param sel	    Optional pointer to transport selector instance which is
 *		    used to find explicit transport, if required.
 * @param tp	    Pointer to receive the transport instance, if one is found.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_tpmgr_acquire_transport(pjsip_tpmgr *mgr,
						   pjsip_transport_type_e type,
						   const pj_sockaddr_t *remote,
						   int addr_len,
						   const pjsip_tpselector *sel,
						   pjsip_transport **tp);

/**
 * Find suitable transport for sending SIP message to specified remote 
 * destination by also considering the outgoing SIP message. If no suitable 
 * transport is found, a new one will be created.
 *
 * This is an internal function since normally application doesn't have access
 * to transport manager. Application should use pjsip_endpt_acquire_transport2()
 * instead.
 *
 * @param mgr	    The transport manager instance.
 * @param type	    The type of transport to be acquired.
 * @param remote    The remote address to send message to.
 * @param addr_len  Length of the remote address.
 * @param sel	    Optional pointer to transport selector instance which is
 *		    used to find explicit transport, if required.
 * @param tdata	    Optional pointer to data to be sent.
 * @param tp	    Pointer to receive the transport instance, if one is found.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_tpmgr_acquire_transport2(pjsip_tpmgr *mgr,
						    pjsip_transport_type_e type,
						    const pj_sockaddr_t *remote,
						    int addr_len,
						    const pjsip_tpselector *sel,
						    pjsip_tx_data *tdata,
						    pjsip_transport **tp);

/**
 * Type of callback to receive notification when message or raw data
 * has been sent.
 *
 * @param token		The token that was given when calling the function
 *			to send message or raw data.
 * @param tdata		The transmit buffer used to send the message.
 * @param bytes_sent	Number of bytes sent. On success, the value will be
 *			positive number indicating the number of bytes sent.
 *			On failure, the value will be a negative number of
 *			the error code (i.e. bytes_sent = -status).
 */
typedef void (*pjsip_tp_send_callback)(void *token, pjsip_tx_data *tdata,
				       pj_ssize_t bytes_sent);


/**
 * This is a low-level function to send a SIP message using the specified
 * transport to the specified destination.
 * 
 * @param tr	    The SIP transport to be used.
 * @param tdata	    Transmit data buffer containing SIP message.
 * @param addr	    Destination address.
 * @param addr_len  Length of destination address.
 * @param token	    Arbitrary token to be returned back to callback.
 * @param cb	    Optional callback to be called to notify caller about
 *		    the completion status of the pending send operation.
 *
 * @return	    If the message has been sent successfully, this function
 *		    will return PJ_SUCCESS and the callback will not be 
 *		    called. If message cannot be sent immediately, this
 *		    function will return PJ_EPENDING, and application will
 *		    be notified later about the completion via the callback.
 *		    Any statuses other than PJ_SUCCESS or PJ_EPENDING
 *		    indicates immediate failure, and in this case the 
 *		    callback will not be called.
 */
PJ_DECL(pj_status_t) pjsip_transport_send( pjsip_transport *tr, 
					   pjsip_tx_data *tdata,
					   const pj_sockaddr_t *addr,
					   int addr_len,
					   void *token,
					   pjsip_tp_send_callback cb);


/**
 * This is a low-level function to send raw data to a destination.
 *
 * See also #pjsip_endpt_send_raw() and #pjsip_endpt_send_raw_to_uri().
 *
 * @param mgr	    Transport manager.
 * @param tp_type   Transport type.
 * @param sel	    Optional pointer to transport selector instance if
 *		    application wants to use a specific transport instance
 *		    rather then letting transport manager finds the suitable
 *		    transport.
 * @param tdata	    Optional transmit data buffer to be used. If this value
 *		    is NULL, this function will create one internally. If
 *		    tdata is specified, this function will decrement the
 *		    reference counter upon completion.
 * @param raw_data  The data to be sent.
 * @param data_len  The length of the data.
 * @param addr	    Destination address.
 * @param addr_len  Length of destination address.
 * @param token	    Arbitrary token to be returned back to callback.
 * @param cb	    Optional callback to be called to notify caller about
 *		    the completion status of the pending send operation.
 *
 * @return	    If the message has been sent successfully, this function
 *		    will return PJ_SUCCESS and the callback will not be 
 *		    called. If message cannot be sent immediately, this
 *		    function will return PJ_EPENDING, and application will
 *		    be notified later about the completion via the callback.
 *		    Any statuses other than PJ_SUCCESS or PJ_EPENDING
 *		    indicates immediate failure, and in this case the 
 *		    callback will not be called.
 */
PJ_DECL(pj_status_t) pjsip_tpmgr_send_raw(pjsip_tpmgr *mgr,
					  pjsip_transport_type_e tp_type,
					  const pjsip_tpselector *sel,
					  pjsip_tx_data *tdata,
					  const void *raw_data,
					  pj_size_t data_len,
					  const pj_sockaddr_t *addr,
					  int addr_len,
					  void *token,
					  pjsip_tp_send_callback cb);


/**
 * Enumeration of transport state types.
 */
typedef enum pjsip_transport_state
{
    PJSIP_TP_STATE_CONNECTED,	    /**< Transport connected, applicable only
					 to connection-oriented transports
					 such as TCP and TLS.		    */
    PJSIP_TP_STATE_DISCONNECTED	    /**< Transport disconnected, applicable
					 only to connection-oriented 
					 transports such as TCP and TLS.    */
} pjsip_transport_state;


/**
 * Definition of transport state listener key.
 */
typedef void pjsip_tp_state_listener_key;

/**
 * Structure of transport state info passed by #pjsip_tp_state_callback.
 */
typedef struct pjsip_transport_state_info {
    /**
     * The last error code related to the transport state.
     */
    pj_status_t		 status;

    /**
     * Optional extended info, the content is specific for each transport type.
     */
    void		*ext_info;

    /**
     * Optional user data. In global transport state notification, this will
     * always be NULL.
     */
    void		*user_data;

} pjsip_transport_state_info;


/**
 * Type of callback to receive transport state notifications, such as
 * transport connected/disconnected. Application may shutdown the transport
 * in this callback.
 *
 * @param tp		The transport instance.
 * @param state		The transport state.
 * @param info		The transport state info.
 */
typedef void (*pjsip_tp_state_callback)(
				    pjsip_transport *tp,
				    pjsip_transport_state state,
				    const pjsip_transport_state_info *info);


/**
 * Set callback of global transport state notification. The caller will be
 * notified whenever the state of any transport is changed. The type of events
 * are defined in #pjsip_transport_state.
 *
 * Note that this function will override the existing callback, if any, so
 * application is recommended to keep the old callback and manually forward
 * the notification to the old callback, otherwise other component that 
 * concerns about the transport state will no longer receive transport state 
 * events.
 * 
 * @param mgr	    Transport manager.
 * @param cb	    Callback to be called to notify caller about transport 
 *		    state changing.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_tpmgr_set_state_cb(pjsip_tpmgr *mgr,
					      pjsip_tp_state_callback cb);


/**
 * Get the callback of global transport state notification.
 * 
 * @param mgr	    Transport manager.
 *
 * @return	    The transport state callback or NULL if it is not set.
 */
PJ_DECL(pjsip_tp_state_callback) pjsip_tpmgr_get_state_cb(
					      const pjsip_tpmgr *mgr);


/**
 * Add a listener to the specified transport for transport state notification.
 * 
 * @param tp	    The transport.
 * @param cb	    Callback to be called to notify listener about transport 
 *		    state changing.
 * @param user_data The user data.
 * @param key	    Output key, used to remove this listener.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_transport_add_state_listener (
					    pjsip_transport *tp,
					    pjsip_tp_state_callback cb,
					    void *user_data,
					    pjsip_tp_state_listener_key **key);


/**
 * Remove a listener from the specified transport for transport state 
 * notification.
 * 
 * @param tp	    The transport.
 * @param key	    The listener key.
 * @param user_data The user data, for validation purpose.
 *
 * @return	    PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_transport_remove_state_listener (
				    pjsip_transport *tp,
				    pjsip_tp_state_listener_key *key,
				    const void *user_data);


/**
 * @}
 */


PJ_END_DECL

#endif	/* __PJSIP_SIP_TRANSPORT_H__ */

