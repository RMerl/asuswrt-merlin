/* $Id: stun_msg.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJNATH_STUN_MSG_H__
#define __PJNATH_STUN_MSG_H__

/**
 * @file stun_msg.h
 * @brief STUN message components.
 */

#include <pjnath/types.h>
#include <pj/sock.h>


PJ_BEGIN_DECL


/* **************************************************************************/
/**
 * @defgroup PJNATH_STUN_MSG STUN Message Representation and Parsing
 * @ingroup PJNATH_STUN_BASE
 * @brief Low-level representation and parsing of STUN messages.
 * @{
 */


/**
 * STUN magic cookie.
 */
#define PJ_STUN_MAGIC			    0x2112A442


/**
 * STUN method constants.
 */
enum pj_stun_method_e
{
    /**
     * STUN Binding method as defined by RFC 3489-bis.
     */
    PJ_STUN_BINDING_METHOD		    = 1,

    /**
     * STUN Shared Secret method as defined by RFC 3489-bis.
     */
    PJ_STUN_SHARED_SECRET_METHOD	    = 2,

    /**
     * STUN/TURN Allocate method as defined by draft-ietf-behave-turn
     */
    PJ_STUN_ALLOCATE_METHOD		    = 3,

    /**
     * STUN/TURN Refresh method as defined by draft-ietf-behave-turn
     */
    PJ_STUN_REFRESH_METHOD		    = 4,

    /**
     * STUN/TURN Send indication as defined by draft-ietf-behave-turn
     */
    PJ_STUN_SEND_METHOD			    = 6,

    /**
     * STUN/TURN Data indication as defined by draft-ietf-behave-turn
     */
    PJ_STUN_DATA_METHOD			    = 7,

    /**
     * STUN/TURN CreatePermission method as defined by draft-ietf-behave-turn
     */
    PJ_STUN_CREATE_PERM_METHOD		    = 8,

    /**
     * STUN/TURN ChannelBind as defined by draft-ietf-behave-turn
     */
    PJ_STUN_CHANNEL_BIND_METHOD		    = 9,

    /**
     * All known methods.
     */
    PJ_STUN_METHOD_MAX
};


/**
 * Retrieve the STUN method from the message-type field of the STUN
 * message.
 */
#define PJ_STUN_GET_METHOD(msg_type)	((msg_type) & 0xFEEF)


/**
 * STUN message classes constants.
 */
enum pj_stun_msg_class_e
{
    /**
     * This specifies that the message type is a STUN request message.
     */
    PJ_STUN_REQUEST_CLASS	    = 0,

    /**
     * This specifies that the message type is a STUN indication message.
     */
    PJ_STUN_INDICATION_CLASS	    = 1,

    /**
     * This specifies that the message type is a STUN successful response.
     */
    PJ_STUN_SUCCESS_CLASS	    = 2,

    /**
     * This specifies that the message type is a STUN error response.
     */
    PJ_STUN_ERROR_CLASS		    = 3
};


/**
 * Determine if the message type is a request.
 */
#define PJ_STUN_IS_REQUEST(msg_type)	(((msg_type) & 0x0110) == 0x0000)


/**
 * Determine if the message type is a successful response.
 */
#define PJ_STUN_IS_SUCCESS_RESPONSE(msg_type) (((msg_type) & 0x0110) == 0x0100)

/**
 * The response bit in the message type.
 */
#define PJ_STUN_SUCCESS_RESPONSE_BIT	(0x0100)


/**
 * Determine if the message type is an error response.
 */
#define PJ_STUN_IS_ERROR_RESPONSE(msg_type) (((msg_type) & 0x0110) == 0x0110)

/**
 * The error response bit in the message type.
 */
#define PJ_STUN_ERROR_RESPONSE_BIT	(0x0110)

/**
 * Determine if the message type is a response.
 */
#define PJ_STUN_IS_RESPONSE(msg_type) (((msg_type) & 0x0100) == 0x0100)


/**
 * Determine if the message type is an indication message.
 */
#define PJ_STUN_IS_INDICATION(msg_type)	(((msg_type) & 0x0110) == 0x0010)

/**
 * The error response bit in the message type.
 */
#define PJ_STUN_INDICATION_BIT		(0x0010)


/**
 * This enumeration describes STUN message types.
 */
typedef enum pj_stun_msg_type
{
    /**
     * STUN BINDING request.
     */
    PJ_STUN_BINDING_REQUEST		    = 0x0001,

    /**
     * Successful response to STUN BINDING-REQUEST.
     */
    PJ_STUN_BINDING_RESPONSE		    = 0x0101,

    /**
     * Error response to STUN BINDING-REQUEST.
     */
    PJ_STUN_BINDING_ERROR_RESPONSE	    = 0x0111,

    /**
     * Binding Indication (ICE)
     */
    PJ_STUN_BINDING_INDICATION		    = 0x0011,

    /**
     * STUN SHARED-SECRET reqeust.
     */
    PJ_STUN_SHARED_SECRET_REQUEST	    = 0x0002,

    /**
     * Successful response to STUN SHARED-SECRET reqeust.
     */
    PJ_STUN_SHARED_SECRET_RESPONSE	    = 0x0102,

    /**
     * Error response to STUN SHARED-SECRET reqeust.
     */
    PJ_STUN_SHARED_SECRET_ERROR_RESPONSE    = 0x0112,


    /**
     * STUN/TURN Allocate Request
     */
    PJ_STUN_ALLOCATE_REQUEST		    = 0x0003,

    /**
     * Successful response to STUN/TURN Allocate Request
     */
    PJ_STUN_ALLOCATE_RESPONSE		    = 0x0103,

    /**
     * Failure response to STUN/TURN Allocate Request
     */
    PJ_STUN_ALLOCATE_ERROR_RESPONSE	    = 0x0113,


    /**
     * STUN/TURN REFRESH Request
     */
    PJ_STUN_REFRESH_REQUEST		    = 0x0004,

    /**
     * Successful response to STUN REFRESH request
     */
    PJ_STUN_REFRESH_RESPONSE		    = 0x0104,

    /**
     * Error response to STUN REFRESH request.
     */
    PJ_STUN_REFRESH_ERROR_RESPONSE	    = 0x0114,


    /**
     * TURN Send indication
     */
    PJ_STUN_SEND_INDICATION		    = 0x0016,


    /**
     * TURN Data indication
     */
    PJ_STUN_DATA_INDICATION		    = 0x0017,


    /**
     * TURN CreatePermission request
     */
    PJ_STUN_CREATE_PERM_REQUEST		    = 0x0008,

    /**
     * TURN CreatePermission successful response.
     */
    PJ_STUN_CREATE_PERM_RESPONSE	    = 0x0108,

    /**
     * TURN CreatePermission failure response
     */
    PJ_STUN_CREATE_PERM_ERROR_RESPONSE	    = 0x0118,


    /**
     * STUN/TURN ChannelBind Request
     */
    PJ_STUN_CHANNEL_BIND_REQUEST	    = 0x0009,

    /**
     * Successful response to STUN ChannelBind request
     */
    PJ_STUN_CHANNEL_BIND_RESPONSE	    = 0x0109,

    /**
     * Error response to STUN ChannelBind request.
     */
    PJ_STUN_CHANNEL_BIND_ERROR_RESPONSE	    = 0x0119

} pj_stun_msg_type;



/**
 * This enumeration describes STUN attribute types.
 */
typedef enum pj_stun_attr_type
{
    PJ_STUN_ATTR_MAPPED_ADDR	    = 0x0001,/**< MAPPED-ADDRESS.	    */
    PJ_STUN_ATTR_RESPONSE_ADDR	    = 0x0002,/**< RESPONSE-ADDRESS (deprcatd)*/
    PJ_STUN_ATTR_CHANGE_REQUEST	    = 0x0003,/**< CHANGE-REQUEST (deprecated)*/
    PJ_STUN_ATTR_SOURCE_ADDR	    = 0x0004,/**< SOURCE-ADDRESS (deprecated)*/
    PJ_STUN_ATTR_CHANGED_ADDR	    = 0x0005,/**< CHANGED-ADDRESS (deprecatd)*/
    PJ_STUN_ATTR_USERNAME	    = 0x0006,/**< USERNAME attribute.	    */
    PJ_STUN_ATTR_PASSWORD	    = 0x0007,/**< was PASSWORD attribute.   */
    PJ_STUN_ATTR_MESSAGE_INTEGRITY  = 0x0008,/**< MESSAGE-INTEGRITY.	    */
    PJ_STUN_ATTR_ERROR_CODE	    = 0x0009,/**< ERROR-CODE.		    */
    PJ_STUN_ATTR_UNKNOWN_ATTRIBUTES = 0x000A,/**< UNKNOWN-ATTRIBUTES.	    */
    PJ_STUN_ATTR_REFLECTED_FROM	    = 0x000B,/**< REFLECTED-FROM (deprecatd)*/
    PJ_STUN_ATTR_CHANNEL_NUMBER	    = 0x000C,/**< TURN CHANNEL-NUMBER	    */
    PJ_STUN_ATTR_LIFETIME	    = 0x000D,/**< TURN LIFETIME attr.	    */
    PJ_STUN_ATTR_MAGIC_COOKIE	    = 0x000F,/**< MAGIC-COOKIE attr (deprec)*/
    PJ_STUN_ATTR_BANDWIDTH	    = 0x0010,/**< TURN BANDWIDTH (deprec)   */
    PJ_STUN_ATTR_XOR_PEER_ADDR	    = 0x0012,/**< TURN XOR-PEER-ADDRESS	    */
    PJ_STUN_ATTR_DATA		    = 0x0013,/**< DATA attribute.	    */
    PJ_STUN_ATTR_REALM		    = 0x0014,/**< REALM attribute.	    */
    PJ_STUN_ATTR_NONCE		    = 0x0015,/**< NONCE attribute.	    */
    PJ_STUN_ATTR_XOR_RELAYED_ADDR   = 0x0016,/**< TURN XOR-RELAYED-ADDRESS  */
    PJ_STUN_ATTR_REQ_ADDR_TYPE	    = 0x0017,/**< REQUESTED-ADDRESS-TYPE    */
    PJ_STUN_ATTR_EVEN_PORT	    = 0x0018,/**< TURN EVEN-PORT	    */
    PJ_STUN_ATTR_REQ_TRANSPORT	    = 0x0019,/**< TURN REQUESTED-TRANSPORT  */
    PJ_STUN_ATTR_DONT_FRAGMENT	    = 0x001A,/**< TURN DONT-FRAGMENT	    */
    PJ_STUN_ATTR_XOR_MAPPED_ADDR    = 0x0020,/**< XOR-MAPPED-ADDRESS	    */
    PJ_STUN_ATTR_TIMER_VAL	    = 0x0021,/**< TIMER-VAL attribute.	    */
    PJ_STUN_ATTR_RESERVATION_TOKEN  = 0x0022,/**< TURN RESERVATION-TOKEN    */
    PJ_STUN_ATTR_XOR_REFLECTED_FROM = 0x0023,/**< XOR-REFLECTED-FROM	    */
    PJ_STUN_ATTR_PRIORITY	    = 0x0024,/**< PRIORITY		    */
    PJ_STUN_ATTR_USE_CANDIDATE	    = 0x0025,/**< USE-CANDIDATE		    */
    PJ_STUN_ATTR_ICMP		    = 0x0030,/**< ICMP (TURN)		    */

    PJ_STUN_ATTR_END_MANDATORY_ATTR,

    PJ_STUN_ATTR_START_EXTENDED_ATTR= 0x8021,

    PJ_STUN_ATTR_SOFTWARE	    = 0x8022,/**< SOFTWARE attribute.	    */
    PJ_STUN_ATTR_ALTERNATE_SERVER   = 0x8023,/**< ALTERNATE-SERVER.	    */
    PJ_STUN_ATTR_REFRESH_INTERVAL   = 0x8024,/**< REFRESH-INTERVAL.	    */
    PJ_STUN_ATTR_FINGERPRINT	    = 0x8028,/**< FINGERPRINT attribute.    */
    PJ_STUN_ATTR_ICE_CONTROLLED	    = 0x8029,/**< ICE-CCONTROLLED attribute.*/
    PJ_STUN_ATTR_ICE_CONTROLLING    = 0x802a,/**< ICE-CCONTROLLING attribute*/

    PJ_STUN_ATTR_END_EXTENDED_ATTR

} pj_stun_attr_type;


/**
 * STUN error codes, which goes into STUN ERROR-CODE attribute.
 */
typedef enum pj_stun_status
{
    PJ_STUN_SC_TRY_ALTERNATE		= 300,  /**< Try Alternate	    */
    PJ_STUN_SC_BAD_REQUEST		= 400,  /**< Bad Request	    */
    PJ_STUN_SC_UNAUTHORIZED	        = 401,  /**< Unauthorized	    */
    PJ_STUN_SC_FORBIDDEN		= 403,	/**< Forbidden (TURN)	    */
    PJ_STUN_SC_UNKNOWN_ATTRIBUTE        = 420,  /**< Unknown Attribute	    */
#if 0
    /* These were obsolete in recent rfc3489bis */
    //PJ_STUN_SC_STALE_CREDENTIALS      = 430,  /**< Stale Credentials	    */
    //PJ_STUN_SC_INTEGRITY_CHECK_FAILURE= 431,  /**< Integrity Chk Fail	    */
    //PJ_STUN_SC_MISSING_USERNAME	= 432,  /**< Missing Username	    */
    //PJ_STUN_SC_USE_TLS		= 433,  /**< Use TLS		    */
    //PJ_STUN_SC_MISSING_REALM		= 434,  /**< Missing Realm	    */
    //PJ_STUN_SC_MISSING_NONCE		= 435,  /**< Missing Nonce	    */
    //PJ_STUN_SC_UNKNOWN_USERNAME	= 436,  /**< Unknown Username	    */
#endif
    PJ_STUN_SC_ALLOCATION_MISMATCH      = 437,  /**< TURN Alloc Mismatch    */
    PJ_STUN_SC_STALE_NONCE	        = 438,  /**< Stale Nonce	    */
    PJ_STUN_SC_TRANSITIONING		= 439,  /**< Transitioning.	    */
    PJ_STUN_SC_WRONG_CREDENTIALS	= 441,	/**< TURN Wrong Credentials */
    PJ_STUN_SC_UNSUPP_TRANSPORT_PROTO   = 442,  /**< Unsupported Transport or
						     Protocol (TURN) */
    PJ_STUN_SC_OPER_TCP_ONLY		= 445,  /**< Operation for TCP Only */
    PJ_STUN_SC_CONNECTION_FAILURE       = 446,  /**< Connection Failure	    */
    PJ_STUN_SC_CONNECTION_TIMEOUT       = 447,  /**< Connection Timeout	    */
    PJ_STUN_SC_ALLOCATION_QUOTA_REACHED = 486,  /**< Allocation Quota Reached
						     (TURN) */
    PJ_STUN_SC_ROLE_CONFLICT		= 487,  /**< Role Conflict	    */
    PJ_STUN_SC_SERVER_ERROR	        = 500,  /**< Server Error	    */
    PJ_STUN_SC_INSUFFICIENT_CAPACITY	= 508,  /**< Insufficient Capacity 
						     (TURN) */
    PJ_STUN_SC_GLOBAL_FAILURE	        = 600   /**< Global Failure	    */
} pj_stun_status;


/**
 * This structure describes STUN message header. A STUN message has the 
 * following format:
 *
 * \verbatim

        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |0 0|     STUN Message Type     |         Message Length        |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                         Magic Cookie                          |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                                Transaction ID
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                                                                       |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   \endverbatim
 */
typedef struct pj_stun_msg_hdr
{
    /**
     * STUN message type, which the first two bits must be zeroes.
     */
    pj_uint16_t		type;

    /**
     * The message length is the size, in bytes, of the message not
     * including the 20 byte STUN header.
     */
    pj_uint16_t		length;

    /**
     * The magic cookie is a fixed value, 0x2112A442 (PJ_STUN_MAGIC constant).
     * In the previous version of this specification [15] this field was part 
     * of the transaction ID.
     */
    pj_uint32_t		magic;

    /**
     * The transaction ID is a 96 bit identifier.  STUN transactions are
     * identified by their unique 96-bit transaction ID.  For request/
     * response transactions, the transaction ID is chosen by the STUN
     * client and MUST be unique for each new STUN transaction generated by
     * that STUN client.  The transaction ID MUST be uniformly and randomly
     * distributed between 0 and 2**96 - 1. 
     */
    pj_uint8_t		tsx_id[12];

} pj_stun_msg_hdr;


/**
 * This structre describes STUN attribute header. Each attribute is
 * TLV encoded, with a 16 bit type, 16 bit length, and variable value.
 * Each STUN attribute ends on a 32 bit boundary:
 *
 * \verbatim

        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |         Type                  |            Length             |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   \endverbatim
 */
typedef struct pj_stun_attr_hdr
{
    /**
     * STUN attribute type.
     */
    pj_uint16_t		type;

    /**
     * The Length refers to the length of the actual useful content of the
     * Value portion of the attribute, measured in bytes. The value
     * in the Length field refers to the length of the Value part of the
     * attribute prior to padding - i.e., the useful content.
     */
    pj_uint16_t		length;

} pj_stun_attr_hdr;


/**
 * This structure describes STUN generic IP address attribute, used for
 * example to represent STUN MAPPED-ADDRESS attribute.
 *
 * The generic IP address attribute indicates the transport address.
 * It consists of an eight bit address family, and a sixteen bit port,
 * followed by a fixed length value representing the IP address.  If the
 * address family is IPv4, the address is 32 bits, in network byte
 * order.  If the address family is IPv6, the address is 128 bits in
 * network byte order.
 *
 * The format of the generic IP address attribute is:
 *
 * \verbatim

        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |x x x x x x x x|    Family     |           Port                |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                   Address  (variable)
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   \endverbatim
 */
typedef struct pj_stun_sockaddr_attr
{
    /**
     * Standard STUN attribute header.
     */
    pj_stun_attr_hdr	hdr;

    /**
     * Flag to indicate whether this attribute should be sent in XOR-ed
     * format, or has been received in XOR-ed format.
     */
    pj_bool_t		xor_ed;

    /**
     * The socket address
     */
    pj_sockaddr		sockaddr;

} pj_stun_sockaddr_attr;


/**
 * This structure represents a generic STUN attributes with no payload,
 * and it is used for example by ICE USE-CANDIDATE attribute.
 */
typedef struct pj_stun_empty_attr
{
    /**
     * Standard STUN attribute header.
     */
    pj_stun_attr_hdr	hdr;

} pj_stun_empty_attr;


/**
 * This structure represents generic STUN string attributes, such as STUN
 * USERNAME, PASSWORD, SOFTWARE, REALM, and NONCE attributes.
 */
typedef struct pj_stun_string_attr
{
    /**
     * Standard STUN attribute header.
     */
    pj_stun_attr_hdr	hdr;

    /**
     * The string value.
     */
    pj_str_t		value;

} pj_stun_string_attr;


/**
 * This structure represents a generic STUN attributes with 32bit (unsigned)
 * integer value, such as STUN FINGERPRINT and REFRESH-INTERVAL attributes.
 */
typedef struct pj_stun_uint_attr
{
    /**
     * Standard STUN attribute header.
     */
    pj_stun_attr_hdr	hdr;
    
    /**
     * The 32bit value, in host byte order.
     */
    pj_uint32_t		value;

} pj_stun_uint_attr;


/**
 * This structure represents a generic STUN attributes with 64bit (unsigned)
 * integer value, such as ICE-CONTROLLED and ICE-CONTROLLING attributes.
 */
typedef struct pj_stun_uint64_attr
{
    /**
     * Standard STUN attribute header.
     */
    pj_stun_attr_hdr	hdr;
    
    /**
     * The 64bit value, in host byte order, represented with pj_timestamp.
     */
    pj_timestamp	value;

} pj_stun_uint64_attr;


/**
 * This structure represents generic STUN attributes to hold a raw binary
 * data.
 */
typedef struct pj_stun_binary_attr
{
    /**
     * Standard STUN attribute header.
     */
    pj_stun_attr_hdr	hdr;
    
    /**
     * Special signature to indicate that this is a valid attribute even
     * though we don't have meta-data to describe this attribute.
     */
    pj_uint32_t		magic;

    /**
     * Length of the data.
     */
    unsigned		length;

    /**
     * The raw data.
     */
    pj_uint8_t	       *data;

} pj_stun_binary_attr;


/**
 * This structure describes STUN MESSAGE-INTEGRITY attribute.
 * The MESSAGE-INTEGRITY attribute contains an HMAC-SHA1 [10] of the
 * STUN message.  The MESSAGE-INTEGRITY attribute can be present in any
 * STUN message type.  Since it uses the SHA1 hash, the HMAC will be 20
 * bytes.
 */
typedef struct pj_stun_msgint_attr
{
    /**
     * Standard STUN attribute header.
     */
    pj_stun_attr_hdr	hdr;

    /**
     * The 20 bytes hmac value.
     */
    pj_uint8_t		hmac[20];

} pj_stun_msgint_attr;


/**
 * This structure describes STUN FINGERPRINT attribute. The FINGERPRINT 
 * attribute can be present in all STUN messages.  It is computed as 
 * the CRC-32 of the STUN message up to (but excluding) the FINGERPRINT 
 * attribute itself, xor-d with the 32 bit value 0x5354554e
 */
typedef struct pj_stun_uint_attr pj_stun_fingerprint_attr;


/**
 * This structure represents STUN ERROR-CODE attribute. The ERROR-CODE 
 * attribute is present in the Binding Error Response and Shared Secret 
 * Error Response.  It is a numeric value in the range of 100 to 699 
 * plus a textual reason phrase encoded in UTF-8
 *
 * \verbatim

        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                   0                     |Class|     Number    |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |      Reason Phrase (variable)                                ..
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

 \endverbatim
 */
typedef struct pj_stun_errcode_attr
{
    /**
     * Standard STUN attribute header.
     */
    pj_stun_attr_hdr	hdr;

    /**
     * STUN error code.
     */
    int			err_code;

    /**
     * The reason phrase.
     */
    pj_str_t		reason;

} pj_stun_errcode_attr;


/**
 * This describes STUN REALM attribute.
 * The REALM attribute is present in requests and responses.  It
 * contains text which meets the grammar for "realm" as described in RFC
 * 3261 [11], and will thus contain a quoted string (including the
 * quotes).
 */
typedef struct pj_stun_string_attr pj_stun_realm_attr;


/**
 * This describes STUN NONCE attribute. 
 * The NONCE attribute is present in requests and in error responses.
 * It contains a sequence of qdtext or quoted-pair, which are defined in
 * RFC 3261 [11].  See RFC 2617 [7] for guidance on selection of nonce
 * values in a server.
 */
typedef struct pj_stun_string_attr pj_stun_nonce_attr;


/**
 * This describes STUN UNKNOWN-ATTRIBUTES attribute.
 * The UNKNOWN-ATTRIBUTES attribute is present only in an error response
 * when the response code in the ERROR-CODE attribute is 420.
 * The attribute contains a list of 16 bit values, each of which
 * represents an attribute type that was not understood by the server.
 * If the number of unknown attributes is an odd number, one of the
 * attributes MUST be repeated in the list, so that the total length of
 * the list is a multiple of 4 bytes.
 */
typedef struct pj_stun_unknown_attr
{
    /**
     * Standard STUN attribute header.
     */
    pj_stun_attr_hdr	hdr;

    /**
     * Number of unknown attributes in the array.
     */
    unsigned		attr_count;

    /**
     * Array of unknown attribute IDs.
     */
    pj_uint16_t	        attrs[PJ_STUN_MAX_ATTR];

} pj_stun_unknown_attr;


/**
 * This structure describes STUN MAPPED-ADDRESS attribute.
 * The MAPPED-ADDRESS attribute indicates the mapped transport address.
 */
typedef struct pj_stun_sockaddr_attr pj_stun_mapped_addr_attr;


/**
 * This describes STUN XOR-MAPPED-ADDRESS attribute (which has the same
 * format as STUN MAPPED-ADDRESS attribute).
 * The XOR-MAPPED-ADDRESS attribute is present in responses.  It
 * provides the same information that would present in the MAPPED-
 * ADDRESS attribute but because the NAT's public IP address is
 * obfuscated through the XOR function, STUN messages are able to pass
 * through NATs which would otherwise interfere with STUN.
 */
typedef struct pj_stun_sockaddr_attr pj_stun_xor_mapped_addr_attr;


/**
 * This describes STUN SOFTWARE attribute.
 * The SOFTWARE attribute contains a textual description of the software
 * being used by the agent sending the message.  It is used by clients
 * and servers.  Its value SHOULD include manufacturer and version
 * number. */
typedef struct pj_stun_string_attr pj_stun_software_attr;


/**
 * This describes STUN ALTERNATE-SERVER attribute.
 * The alternate server represents an alternate transport address for a
 * different STUN server to try.  It is encoded in the same way as
 * MAPPED-ADDRESS.
 */
typedef struct pj_stun_sockaddr_attr pj_stun_alt_server_attr;


/**
 * This describes STUN REFRESH-INTERVAL attribute.
 * The REFRESH-INTERVAL indicates the number of milliseconds that the
 * server suggests the client should use between refreshes of the NAT
 * bindings between the client and server.
 */
typedef struct pj_stun_uint_attr pj_stun_refresh_interval_attr;


/**
 * This structure describes STUN RESPONSE-ADDRESS attribute.
 * The RESPONSE-ADDRESS attribute indicates where the response to a
 * Binding Request should be sent.  Its syntax is identical to MAPPED-
 * ADDRESS.
 *
 * Note that the usage of this attribute has been deprecated by the 
 * RFC 3489-bis standard.
 */
typedef struct pj_stun_sockaddr_attr pj_stun_response_addr_attr;


/**
 * This structure describes STUN CHANGED-ADDRESS attribute.
 * The CHANGED-ADDRESS attribute indicates the IP address and port where
 * responses would have been sent from if the "change IP" and "change
 * port" flags had been set in the CHANGE-REQUEST attribute of the
 * Binding Request.  The attribute is always present in a Binding
 * Response, independent of the value of the flags.  Its syntax is
 * identical to MAPPED-ADDRESS.
 *
 * Note that the usage of this attribute has been deprecated by the 
 * RFC 3489-bis standard.
 */
typedef struct pj_stun_sockaddr_attr pj_stun_changed_addr_attr;


/**
 * This structure describes STUN CHANGE-REQUEST attribute.
 * The CHANGE-REQUEST attribute is used by the client to request that
 * the server use a different address and/or port when sending the
 * response. 
 *
 * Bit 29 of the value is the "change IP" flag.  If true, it requests 
 * the server to send the Binding Response with a different IP address 
 * than the one the Binding Request was received on.
 *
 * Bit 30 of the value is the "change port" flag.  If true, it requests 
 * the server to send the Binding Response with a different port than 
 * the one the Binding Request was received on.
 *
 * Note that the usage of this attribute has been deprecated by the 
 * RFC 3489-bis standard.
 */
typedef struct pj_stun_uint_attr pj_stun_change_request_attr;

/**
 * This structure describes STUN SOURCE-ADDRESS attribute.
 * The SOURCE-ADDRESS attribute is present in Binding Responses.  It
 * indicates the source IP address and port that the server is sending
 * the response from.  Its syntax is identical to that of MAPPED-
 * ADDRESS.
 *
 * Note that the usage of this attribute has been deprecated by the 
 * RFC 3489-bis standard.
 */
typedef struct pj_stun_sockaddr_attr pj_stun_src_addr_attr;


/**
 * This describes the STUN REFLECTED-FROM attribute.
 * The REFLECTED-FROM attribute is present only in Binding Responses,
 * when the Binding Request contained a RESPONSE-ADDRESS attribute.  The
 * attribute contains the identity (in terms of IP address) of the
 * source where the request came from.  Its purpose is to provide
 * traceability, so that a STUN server cannot be used as a reflector for
 * denial-of-service attacks.
 */
typedef struct pj_stun_sockaddr_attr pj_stun_reflected_from_attr;


/**
 * This describes STUN USERNAME attribute.
 * The USERNAME attribute is used for message integrity.  It identifies
 * the shared secret used in the message integrity check.  Consequently,
 * the USERNAME MUST be included in any request that contains the
 * MESSAGE-INTEGRITY attribute.
 */
typedef struct pj_stun_string_attr pj_stun_username_attr;


/**
 * This describes STUN PASSWORD attribute.
 * If the message type is Shared Secret Response it MUST include the
 * PASSWORD attribute.
 */
typedef struct pj_stun_string_attr pj_stun_password_attr;


/**
 * This describes TURN CHANNEL-NUMBER attribute. In this library,
 * this attribute is represented with 32bit integer. Application may
 * use #PJ_STUN_GET_CH_NB() and #PJ_STUN_SET_CH_NB() to extract/set
 * channel number value from the 32bit integral value.
 *
 * The CHANNEL-NUMBER attribute contains the number of the channel.
 * It is a 16-bit unsigned integer, followed by a two-octet RFFU field
 * which MUST be set to 0 on transmission and ignored on reception.
  
 \verbatim
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |        Channel Number         |         RFFU                  |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 \endverbatim
 */
typedef struct pj_stun_uint_attr pj_stun_channel_number_attr;

/**
 * Get 16bit channel number from 32bit integral value.
 * Note that uint32 attributes are always stored in host byte order
 * after they have been parsed from the PDU, so no need to do ntohs()
 * here.
 */
#define PJ_STUN_GET_CH_NB(u32)	    ((pj_uint16_t)(u32>>16))

/**
 * Convert 16bit channel number into 32bit integral value.
 * Note that uint32 attributes will be converted to network byte order
 * when the attribute is written to packet, so no need to do htons()
 * here.
 */
#define PJ_STUN_SET_CH_NB(chnum)    (((pj_uint32_t)chnum) << 16)


/**
 * This describes STUN LIFETIME attribute.
 * The lifetime attribute represents the duration for which the server
 * will maintain an allocation in the absence of data traffic either
 * from or to the client.  It is a 32 bit value representing the number
 * of seconds remaining until expiration.
 */
typedef struct pj_stun_uint_attr pj_stun_lifetime_attr;


/**
 * This describes STUN BANDWIDTH attribute.
 * The bandwidth attribute represents the peak bandwidth, measured in
 * kbits per second, that the client expects to use on the binding.  The
 * value represents the sum in the receive and send directions.
 */
typedef struct pj_stun_uint_attr pj_stun_bandwidth_attr;


/**
 * This describes the STUN XOR-PEER-ADDRESS attribute.
 * The XOR-PEER-ADDRESS specifies the address and port of the peer as seen
 * from the TURN server.  It is encoded in the same way as XOR-MAPPED-
 * ADDRESS. 
 */
typedef struct pj_stun_sockaddr_attr pj_stun_xor_peer_addr_attr;


/**
 * This describes the STUN DATA attribute.
 * The DATA attribute is present in Send Indications and Data
 * Indications.  It contains raw payload data that is to be sent (in the
 * case of a Send Request) or was received (in the case of a Data
 * Indication)..
 */
typedef struct pj_stun_binary_attr pj_stun_data_attr;


/**
 * This describes the STUN XOR-RELAYED-ADDRESS attribute. The 
 * XOR-RELAYED-ADDRESS is present in Allocate responses.  It specifies the
 * address and port that the server allocated to the client.  It is
 * encoded in the same way as XOR-MAPPED-ADDRESS.
 */
typedef struct pj_stun_sockaddr_attr pj_stun_xor_relayed_addr_attr;


/**
 * This describes the REQUESTED-ADDRESS-TYPE attribute.
 * The REQUESTED-ADDRESS-TYPE attribute is used by clients to request
 * the allocation of a specific address type from a server.  The
 * following is the format of the REQUESTED-ADDRESS-TYPE attribute.

 \verbatim

      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |        Family                 |           Reserved            |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

 \endverbatim
 */
typedef struct pj_stun_uint_attr pj_stun_req_addr_type_attr;


/**
 * This describes the TURN REQUESTED-TRANSPORT attribute, encoded in
 * STUN generic integer attribute.
 *
 * This attribute allows the client to request that the port in the
 * relayed-transport-address be even, and (optionally) that the server
 * reserve the next-higher port number.  The attribute is 8 bits long.
 * Its format is:

\verbatim
      0
      0 1 2 3 4 5 6 7
     +-+-+-+-+-+-+-+-+
     |R|    RFFU     |
     +-+-+-+-+-+-+-+-+

\endverbatim

 * The attribute contains a single 1-bit flag:
 * 
 * R: If 1, the server is requested to reserve the next higher port
 *    number (on the same IP address) for a subsequent allocation.  If
 *    0, no such reservation is requested.
 * 
 * The other 7 bits of the attribute must be set to zero on transmission
 * and ignored on reception.
 */
typedef struct pj_stun_uint_attr pj_stun_even_port_attr;


/**
 * This describes the TURN REQUESTED-TRANSPORT attribute, encoded in
 * STUN generic integer attribute.
 *
 * This attribute is used by the client to request a specific transport
 * protocol for the allocated transport address.  It has the following
 * format:

 \verbatim

      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |    Protocol   |                    RFFU                       |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


 \endverbatim

 * The Protocol field specifies the desired protocol.  The codepoints
 * used in this field are taken from those allowed in the Protocol field
 * in the IPv4 header and the NextHeader field in the IPv6 header
 * [Protocol-Numbers].  This specification only allows the use of
 * codepoint 17 (User Datagram Protocol).
 * 
 * The RFFU field is set to zero on transmission and ignored on
 * receiption.  It is reserved for future uses.
 */
typedef struct pj_stun_uint_attr pj_stun_req_transport_attr;

/**
 * Get protocol value from 32bit TURN REQUESTED-TRANSPORT attribute.
 */
#define PJ_STUN_GET_RT_PROTO(u32)   (u32 >> 24)

/**
 * Convert protocol value to be placed in 32bit TURN REQUESTED-TRANSPORT
 * attribute.
 */
#define PJ_STUN_SET_RT_PROTO(proto)   (((pj_uint32_t)(proto)) << 24)


/**
 * This describes the TURN DONT-FRAGMENT attribute.
 *
 * This attribute is used by the client to request that the server set
 * the DF (Don't Fragment) bit in the IP header when relaying the
 * application data onward to the peer.  This attribute has no value
 * part and thus the attribute length field is 0.
 */
typedef struct pj_stun_empty_attr pj_stun_dont_fragment_attr;


/**
 * This describes the TURN RESERVATION-TOKEN attribute.
 * The RESERVATION-TOKEN attribute contains a token that uniquely
 * identifies a relayed transport address being held in reserve by the
 * server.  The server includes this attribute in a success response to
 * tell the client about the token, and the client includes this
 * attribute in a subsequent Allocate request to request the server use
 * that relayed transport address for the allocation.
 * 
 * The attribute value is a 64-bit-long field containing the token
 * value. 
 */
typedef struct pj_stun_uint64_attr pj_stun_res_token_attr;

/**
 * This describes the XOR-REFLECTED-FROM attribute, as described by
 * draft-macdonald-behave-nat-behavior-discovery-00.
 * The XOR-REFLECTED-FROM attribute is used in place of the REFLECTED-
 * FROM attribute.  It provides the same information, but because the
 * NAT's public address is obfuscated through the XOR function, It can
 * pass through a NAT that would otherwise attempt to translate it to
 * the private network address.  XOR-REFLECTED-FROM has identical syntax
 * to XOR-MAPPED-ADDRESS.
 */
typedef struct pj_stun_sockaddr_attr pj_stun_xor_reflected_from_attr;

/**
 * This describes the PRIORITY attribute from draft-ietf-mmusic-ice-13.
 * The PRIORITY attribute indicates the priority that is to be
 * associated with a peer reflexive candidate, should one be discovered
 * by this check.  It is a 32 bit unsigned integer, and has an attribute
 * type of 0x0024.
 */
typedef struct pj_stun_uint_attr pj_stun_priority_attr;

/**
 * This describes the USE-CANDIDATE attribute from draft-ietf-mmusic-ice-13.
 * The USE-CANDIDATE attribute indicates that the candidate pair
 * resulting from this check should be used for transmission of media.
 * The attribute has no content (the Length field of the attribute is
 * zero); it serves as a flag.
 */
typedef struct pj_stun_empty_attr pj_stun_use_candidate_attr;

/**
 * This describes the STUN TIMER-VAL attribute.
 * The TIMER-VAL attribute is used only in conjunction with the Set
 * Active Destination response.  It conveys from the server, to the
 * client, the value of the timer used in the server state machine.
 */
typedef struct pj_stun_uint_attr pj_stun_timer_val_attr;

/**
 * This describes ICE-CONTROLLING attribute.
 */
typedef struct pj_stun_uint64_attr pj_stun_ice_controlling_attr;

/**
 * This describes ICE-CONTROLLED attribute.
 */
typedef struct pj_stun_uint64_attr pj_stun_ice_controlled_attr;

/**
 * This describes TURN ICMP attribute
 */
typedef struct pj_stun_uint_attr pj_stun_icmp_attr;

/**
 * This structure describes a parsed STUN message. All integral fields
 * in this structure (including IP addresses) will be in the host
 * byte order.
 */
typedef struct pj_stun_msg
{
    /**
     * STUN message header.
     */
    pj_stun_msg_hdr     hdr;

    /**
     * Number of attributes in the STUN message.
     */
    unsigned		attr_count;

    /**
     * Array of STUN attributes.
     */
    pj_stun_attr_hdr   *attr[PJ_STUN_MAX_ATTR];

} pj_stun_msg;


/** STUN decoding options */
enum pj_stun_decode_options
{
    /** 
     * Tell the decoder that the message was received from datagram
     * oriented transport (such as UDP).
     */
    PJ_STUN_IS_DATAGRAM	    = 1,

    /**
     * Tell pj_stun_msg_decode() to check the validity of the STUN
     * message by calling pj_stun_msg_check() before starting to
     * decode the packet.
     */
    PJ_STUN_CHECK_PACKET    = 2,

    /**
     * This option current is only valid for #pj_stun_session_on_rx_pkt().
     * When specified, it tells the session NOT to authenticate the
     * message.
     */
    PJ_STUN_NO_AUTHENTICATE = 4,

    /**
     * Disable FINGERPRINT verification. This option can be used when calling
     * #pj_stun_msg_check() and #pj_stun_msg_decode() to disable the 
     * verification of FINGERPRINT, for example when the STUN usage says when
     * FINGERPRINT mechanism shall not be used.
     */
    PJ_STUN_NO_FINGERPRINT_CHECK = 8
};


/**
 * Get STUN message method name.
 *
 * @param msg_type	The STUN message type (in host byte order)
 *
 * @return		The STUN message method name string.
 */
PJ_DECL(const char*) pj_stun_get_method_name(unsigned msg_type);


/**
 * Get STUN message class name.
 *
 * @param msg_type	The STUN message type (in host byte order)
 *
 * @return		The STUN message class name string.
 */
PJ_DECL(const char*) pj_stun_get_class_name(unsigned msg_type);


/**
 * Get STUN attribute name.
 *
 * @return attr_type	The STUN attribute type (in host byte order).
 *
 * @return		The STUN attribute type name string.
 */
PJ_DECL(const char*) pj_stun_get_attr_name(unsigned attr_type);


/**
 * Get STUN standard reason phrase for the specified error code.
 *
 * @param err_code	The STUN error code.
 *
 * @return		The STUN error reason phrase.
 */
PJ_DECL(pj_str_t) pj_stun_get_err_reason(int err_code);


/**
 * Internal: set the padding character for string attribute.
 * The default padding character is PJ_STUN_STRING_ATTR_PAD_CHR.
 *
 * @return		The previous padding character.
 */
PJ_DECL(int) pj_stun_set_padding_char(int chr);


/**
 * Initialize a generic STUN message.
 *
 * @param msg		The message structure to be initialized.
 * @param msg_type	The 14bit message type (see pj_stun_msg_type 
 *			constants).
 * @param magic		Magic value to be put to the mesage; for requests,
 *			the value normally should be PJ_STUN_MAGIC.
 * @param tsx_id	Optional transaction ID, or NULL to let the
 *			function generates a random transaction ID.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pj_stun_msg_init(pj_stun_msg *msg,
				      unsigned msg_type,
				      pj_uint32_t magic,
				      const pj_uint8_t tsx_id[12]);

/**
 * Create a generic STUN message.
 *
 * @param pool		Pool to create the STUN message.
 * @param msg_type	The 14bit message type.
 * @param magic		Magic value to be put to the mesage; for requests,
 *			the value should be PJ_STUN_MAGIC.
 * @param tsx_id	Optional transaction ID, or NULL to let the
 *			function generates a random transaction ID.
 * @param p_msg		Pointer to receive the message.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pj_stun_msg_create(pj_pool_t *pool,
					unsigned msg_type,
					pj_uint32_t magic,
					const pj_uint8_t tsx_id[12],
					pj_stun_msg **p_msg);

/**
 * Clone a STUN message with all of its attributes.
 *
 * @param pool		Pool to allocate memory for the new message.
 * @param msg		The message to be cloned.
 *
 * @return		The duplicate message.
 */
PJ_DECL(pj_stun_msg*) pj_stun_msg_clone(pj_pool_t *pool,
					const pj_stun_msg *msg);

/**
 * Create STUN response message. 
 *
 * @param pool		Pool to create the mesage.
 * @param req_msg	The request message.
 * @param err_code	STUN error code. If this value is not zero,
 *			then error response will be created, otherwise
 *			successful response will be created.
 * @param err_msg	Optional error message to explain err_code.
 *			If this value is NULL and err_code is not zero,
 *			the error string will be taken from the default
 *			STUN error message.
 * @param p_response	Pointer to receive the response.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error.
 */
PJ_DECL(pj_status_t) pj_stun_msg_create_response(pj_pool_t *pool,
						 const pj_stun_msg *req_msg,
						 unsigned err_code,
						 const pj_str_t *err_msg,
						 pj_stun_msg **p_response);


/**
 * Add STUN attribute to STUN message.
 *
 * @param msg		The STUN message.
 * @param attr		The STUN attribute to be added to the message.
 *
 * @return		PJ_SUCCESS on success, or PJ_ETOOMANY if there are
 *			already too many attributes in the message.
 */
PJ_DECL(pj_status_t) pj_stun_msg_add_attr(pj_stun_msg *msg,
					  pj_stun_attr_hdr *attr);


/**
 * Print the STUN message structure to a packet buffer, ready to be 
 * sent to remote destination. This function will take care about 
 * calculating the MESSAGE-INTEGRITY digest as well as FINGERPRINT
 * value, if these attributes are present in the message.
 *
 * If application wants to apply credential to the message, it MUST
 * include a blank MESSAGE-INTEGRITY attribute in the message as the
 * last attribute or the attribute before FINGERPRINT. This function will
 * calculate the HMAC digest from the message using  the supplied key in
 * the parameter. The key should be set to the password if short term 
 * credential is used, or calculated from the MD5 hash of the realm, 
 * username, and password using #pj_stun_create_key() if long term 
 * credential is used.
 *
 * If FINGERPRINT attribute is present, this function will calculate
 * the FINGERPRINT CRC attribute for the message. The FINGERPRINT MUST
 * be added as the last attribute of the message.
 *
 * @param msg		The STUN message to be printed. Upon return,
 *			some fields in the header (such as message
 *			length) will be updated.
 * @param pkt_buf	The buffer to be filled with the packet.
 * @param buf_size	Size of the buffer.
 * @param options	Options, which currently must be zero.
 * @param key		Authentication key to calculate MESSAGE-INTEGRITY
 *			value. Application can create this key by using
 *			#pj_stun_create_key() function.
 * @param p_msg_len	Upon return, it will be filed with the size of 
 *			the packet in bytes, or negative value on error.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_msg_encode(pj_stun_msg *msg,
				        pj_uint8_t *pkt_buf,
				        pj_size_t buf_size,
				        unsigned options,
					const pj_str_t *key,
				        pj_size_t *p_msg_len);

/**
 * Check that the PDU is potentially a valid STUN message. This function
 * is useful when application needs to multiplex STUN packets with other
 * application traffic. When this function returns PJ_SUCCESS, there is a
 * big chance that the packet is a STUN packet.
 *
 * Note that we cannot be sure that the PDU is a really valid STUN message 
 * until we actually parse the PDU.
 *
 * @param pdu		The packet buffer.
 * @param pdu_len	The length of the packet buffer.
 * @param options	Additional options to be applied in the checking,
 *			which can be taken from pj_stun_decode_options. One 
 *			of the useful option is PJ_STUN_IS_DATAGRAM which 
 *			means that the pdu represents a whole STUN packet.
 *
 * @return		PJ_SUCCESS if the PDU is a potentially valid STUN
 *			message.
 */
PJ_DECL(pj_status_t) pj_stun_msg_check(const pj_uint8_t *pdu, 
				       pj_size_t pdu_len, unsigned options);


/**
 * Decode incoming packet into STUN message.
 *
 * @param pool		Pool to allocate the message.
 * @param pdu		The incoming packet to be parsed.
 * @param pdu_len	The length of the incoming packet.
 * @param options	Parsing flags, according to pj_stun_decode_options.
 * @param p_msg		Pointer to receive the parsed message.
 * @param p_parsed_len	Optional pointer to receive how many bytes have
 *			been parsed for the STUN message. This is useful
 *			when the packet is received over stream oriented
 *			transport.
 * @param p_response	Optional pointer to receive an instance of response
 *			message, if one can be created. If the packet being
 *			decoded is a request message, and it contains error,
 *			and a response can be created, then the STUN 
 *			response message will be returned on this argument.
 *
 * @return		PJ_SUCCESS if a STUN message has been successfully
 *			decoded.
 */
PJ_DECL(pj_status_t) pj_stun_msg_decode(pj_pool_t *pool,
				        const pj_uint8_t *pdu,
				        pj_size_t pdu_len,
				        unsigned options,
				        pj_stun_msg **p_msg,
					pj_size_t *p_parsed_len,
				        pj_stun_msg **p_response);

/**
 * Dump STUN message to a printable string output.
 *
 * @param msg		The STUN message
 * @param buffer	Buffer where the printable string output will
 *			be printed on.
 * @param length	Specify the maximum length of the buffer.
 * @param printed_len	Optional pointer, which on output will be filled
 *			up with the actual length of the output string.
 *
 * @return		The message string output.
 */
#if PJ_LOG_MAX_LEVEL > 0
PJ_DECL(char*) pj_stun_msg_dump(const pj_stun_msg *msg,
			        char *buffer,
			        unsigned length,
				unsigned *printed_len);
#else
#   define pj_stun_msg_dump(msg, buf, length, printed_len)  ""
#endif


/**
 * Find STUN attribute in the STUN message, starting from the specified
 * index.
 *
 * @param msg		The STUN message.
 * @param attr_type	The attribute type to be found, from pj_stun_attr_type.
 * @param start_index	The start index of the attribute in the message.
 *			Specify zero to start searching from the first
 *			attribute.
 *
 * @return		The attribute instance, or NULL if it cannot be
 *			found.
 */
PJ_DECL(pj_stun_attr_hdr*) pj_stun_msg_find_attr(const pj_stun_msg *msg,
						 int attr_type,
						 unsigned start_index);


/**
 * Clone a STUN attribute.
 *
 * @param pool		Pool to allocate memory.
 * @param attr		Attribute to clone.
 *
 * @return		Duplicate attribute.
 */
PJ_DECL(pj_stun_attr_hdr*) pj_stun_attr_clone(pj_pool_t *pool,
					      const pj_stun_attr_hdr *attr);


/**
 * Initialize generic STUN IP address attribute. The \a addr_len and
 * \a addr parameters specify whether the address is IPv4 or IPv4
 * address.
 *
 * @param attr		The socket address attribute to initialize.
 * @param attr_type	Attribute type, from #pj_stun_attr_type.
 * @param xor_ed	If non-zero, the port and address will be XOR-ed
 *			with magic, to make the XOR-MAPPED-ADDRESS attribute.
 * @param addr		A pj_sockaddr_in or pj_sockaddr_in6 structure.
 * @param addr_len	Length of \a addr parameter.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_sockaddr_attr_init(pj_stun_sockaddr_attr *attr,
						int attr_type, 
						pj_bool_t xor_ed,
						const pj_sockaddr_t *addr,
						unsigned addr_len);

/**
 * Create a generic STUN IP address attribute. The \a addr_len and
 * \a addr parameters specify whether the address is IPv4 or IPv4
 * address.
 *
 * @param pool		The pool to allocate memory from.
 * @param attr_type	Attribute type, from #pj_stun_attr_type.
 * @param xor_ed	If non-zero, the port and address will be XOR-ed
 *			with magic, to make the XOR-MAPPED-ADDRESS attribute.
 * @param addr		A pj_sockaddr_in or pj_sockaddr_in6 structure.
 * @param addr_len	Length of \a addr parameter.
 * @param p_attr	Pointer to receive the attribute.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_sockaddr_attr_create(pj_pool_t *pool,
						int attr_type, 
						pj_bool_t xor_ed,
						const pj_sockaddr_t *addr,
						unsigned addr_len,
						pj_stun_sockaddr_attr **p_attr);


/**
 * Create and add generic STUN IP address attribute to a STUN message.
 * The \a addr_len and \a addr parameters specify whether the address is 
 * IPv4 or IPv4 address.
 *
 * @param pool		The pool to allocate memory from.
 * @param msg		The STUN message.
 * @param attr_type	Attribute type, from #pj_stun_attr_type.
 * @param xor_ed	If non-zero, the port and address will be XOR-ed
 *			with magic, to make the XOR-MAPPED-ADDRESS attribute.
 * @param addr		A pj_sockaddr_in or pj_sockaddr_in6 structure.
 * @param addr_len	Length of \a addr parameter.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_msg_add_sockaddr_attr(pj_pool_t *pool,
						  pj_stun_msg *msg,
						  int attr_type, 
						  pj_bool_t xor_ed,
						  const pj_sockaddr_t *addr,
						  unsigned addr_len);

/**
 * Initialize a STUN generic string attribute.
 *
 * @param attr		The string attribute to be initialized.
 * @param pool		Pool to duplicate the value into the attribute,
 *			if value is not NULL or empty.
 * @param attr_type	Attribute type, from #pj_stun_attr_type.
 * @param value		The string value to be assigned to the attribute.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_string_attr_init(pj_stun_string_attr *attr,
					      pj_pool_t *pool,
					      int attr_type,
					      const pj_str_t *value);

/**
 * Create a STUN generic string attribute.
 *
 * @param pool		The pool to allocate memory from.
 * @param attr_type	Attribute type, from #pj_stun_attr_type.
 * @param value		The string value to be assigned to the attribute.
 * @param p_attr	Pointer to receive the attribute.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_string_attr_create(pj_pool_t *pool,
					        int attr_type,
					        const pj_str_t *value,
					        pj_stun_string_attr **p_attr);

/**
 * Create and add STUN generic string attribute to the message.
 *
 * @param pool		The pool to allocate memory from.
 * @param msg		The STUN message.
 * @param attr_type	Attribute type, from #pj_stun_attr_type.
 * @param value		The string value to be assigned to the attribute.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_msg_add_string_attr(pj_pool_t *pool,
						 pj_stun_msg *msg,
						 int attr_type,
						 const pj_str_t *value);

/**
 * Create a STUN generic 32bit value attribute.
 *
 * @param pool		The pool to allocate memory from.
 * @param attr_type	Attribute type, from #pj_stun_attr_type.
 * @param value		The 32bit value to be assigned to the attribute.
 * @param p_attr	Pointer to receive the attribute.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_uint_attr_create(pj_pool_t *pool,
					      int attr_type,
					      pj_uint32_t value,
					      pj_stun_uint_attr **p_attr);

/**
 * Create and add STUN generic 32bit value attribute to the message.
 *
 * @param pool		The pool to allocate memory from.
 * @param msg		The STUN message
 * @param attr_type	Attribute type, from #pj_stun_attr_type.
 * @param value		The 32bit value to be assigned to the attribute.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_msg_add_uint_attr(pj_pool_t *pool,
					       pj_stun_msg *msg,
					       int attr_type,
					       pj_uint32_t value);


/**
 * Create a STUN generic 64bit value attribute.
 *
 * @param pool		Pool to allocate memory from.
 * @param attr_type	Attribute type, from #pj_stun_attr_type.
 * @param value		Optional value to be assigned.
 * @param p_attr	Pointer to receive the attribute.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t)  pj_stun_uint64_attr_create(pj_pool_t *pool,
					        int attr_type,
					        const pj_timestamp *value,
					        pj_stun_uint64_attr **p_attr);


/**
 *  Create and add STUN generic 64bit value attribute to the message. 
 *
 * @param pool		The pool to allocate memory from.
 * @param msg		The STUN message
 * @param attr_type	Attribute type, from #pj_stun_attr_type.
 * @param value		The 64bit value to be assigned to the attribute.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t)  pj_stun_msg_add_uint64_attr(pj_pool_t *pool,
					          pj_stun_msg *msg,
					          int attr_type,
					          const pj_timestamp *value);

/**
 * Create a STUN MESSAGE-INTEGRITY attribute.
 *
 * @param pool		The pool to allocate memory from.
 * @param p_attr	Pointer to receive the attribute.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_msgint_attr_create(pj_pool_t *pool,
						pj_stun_msgint_attr **p_attr);

/** 
 * Create and add STUN MESSAGE-INTEGRITY attribute.
 *
 * @param pool		The pool to allocate memory from.
 * @param msg		The STUN message
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_msg_add_msgint_attr(pj_pool_t *pool,
						 pj_stun_msg *msg);

/**
 * Create a STUN ERROR-CODE attribute.
 *
 * @param pool		The pool to allocate memory from.
 * @param err_code	STUN error code.
 * @param err_reason	Optional STUN error reason. If NULL is given, the
 *			standard error reason will be given.
 * @param p_attr	Pointer to receive the attribute.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_errcode_attr_create(pj_pool_t *pool,
						int err_code,
						const pj_str_t *err_reason,
						pj_stun_errcode_attr **p_attr);


/**
 * Create and add STUN ERROR-CODE attribute to the message.
 *
 * @param pool		The pool to allocate memory from.
 * @param msg		The STUN mesage.
 * @param err_code	STUN error code.
 * @param err_reason	Optional STUN error reason. If NULL is given, the
 *			standard error reason will be given.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_msg_add_errcode_attr(pj_pool_t *pool,
						  pj_stun_msg *msg,
						  int err_code,
						  const pj_str_t *err_reason);

/**
 * Create instance of STUN UNKNOWN-ATTRIBUTES attribute and copy the
 * unknown attribute array to the attribute.
 *
 * @param pool		The pool to allocate memory from.
 * @param attr_cnt	Number of attributes in the array (can be zero).
 * @param attr		Optional array of attributes.
 * @param p_attr	Pointer to receive the attribute.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_unknown_attr_create(pj_pool_t *pool,
						unsigned attr_cnt,
						const pj_uint16_t attr[],
						pj_stun_unknown_attr **p_attr);

/**
 * Create and add STUN UNKNOWN-ATTRIBUTES attribute to the message.
 *
 * @param pool		The pool to allocate memory from.
 * @param msg		The STUN message.
 * @param attr_cnt	Number of attributes in the array (can be zero).
 * @param attr		Optional array of attribute types.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_msg_add_unknown_attr(pj_pool_t *pool,
						  pj_stun_msg *msg,
						  unsigned attr_cnt,
						  const pj_uint16_t attr[]);

/**
 * Initialize STUN binary attribute.
 *
 * @param attr		The attribute to be initialized.
 * @param pool		Pool to copy data, if the data and length are set.
 * @param attr_type	The attribute type, from #pj_stun_attr_type.
 * @param data		Data to be coped to the attribute, or NULL
 *			if no data to be copied now.
 * @param length	Length of data, or zero if no data is to be
 *			copied now.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_binary_attr_init(pj_stun_binary_attr *attr,
					      pj_pool_t *pool,
					      int attr_type,
					      const pj_uint8_t *data,
					      unsigned length);

/**
 * Create STUN binary attribute.
 *
 * @param pool		The pool to allocate memory from.
 * @param attr_type	The attribute type, from #pj_stun_attr_type.
 * @param data		Data to be coped to the attribute, or NULL
 *			if no data to be copied now.
 * @param length	Length of data, or zero if no data is to be
 *			copied now.
 * @param p_attr	Pointer to receive the attribute.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_binary_attr_create(pj_pool_t *pool,
					        int attr_type,
					        const pj_uint8_t *data,
					        unsigned length,
					        pj_stun_binary_attr **p_attr);

/**
 * Create STUN binary attribute and add the attribute to the message.
 *
 * @param pool		The pool to allocate memory from.
 * @param msg		The STUN message.
 * @param attr_type	The attribute type, from #pj_stun_attr_type.
 * @param data		Data to be coped to the attribute, or NULL
 *			if no data to be copied now.
 * @param length	Length of data, or zero if no data is to be
 *			copied now.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_msg_add_binary_attr(pj_pool_t *pool,
						 pj_stun_msg *msg,
						 int attr_type,
						 const pj_uint8_t *data,
						 unsigned length);

/**
 * Create STUN empty attribute.
 *
 * @param pool		The pool to allocate memory from.
 * @param attr_type	The attribute type, from #pj_stun_attr_type.
 * @param p_attr	Pointer to receive the attribute.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_empty_attr_create(pj_pool_t *pool,
					       int attr_type,
					       pj_stun_empty_attr **p_attr);

/**
 * Create STUN empty attribute and add the attribute to the message.
 *
 * @param pool		The pool to allocate memory from.
 * @param msg		The STUN message.
 * @param attr_type	The attribute type, from #pj_stun_attr_type.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_stun_msg_add_empty_attr(pj_pool_t *pool,
						pj_stun_msg *msg,
						int attr_type);

/**
 * @}
 */


PJ_END_DECL


#endif	/* __PJNATH_STUN_MSG_H__ */

