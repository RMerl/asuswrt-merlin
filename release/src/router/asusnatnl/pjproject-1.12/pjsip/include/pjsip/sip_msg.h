/* $Id: sip_msg.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJSIP_SIP_MSG_H__
#define __PJSIP_SIP_MSG_H__

/**
 * @file pjsip/sip_msg.h
 * @brief SIP Message Structure.
 */

#include <pjsip/sip_types.h>
#include <pjsip/sip_uri.h>
#include <pj/list.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_MSG Messaging Elements
 * @ingroup PJSIP_CORE
 * @brief Various SIP message elements such as methods, headers, URIs, etc.
 * @{
 */

/* **************************************************************************/
/**
 * @defgroup PJSIP_MSG_METHOD Methods
 * @brief Method names and manipulation.
 * @ingroup PJSIP_MSG
 * @{
 */

/**
 * This enumeration declares SIP methods as described by RFC3261. Additional
 * methods do exist, and they are described by corresponding RFCs for the SIP
 * extentensions. Since they won't alter the characteristic of the processing
 * of the message, they don't need to be explicitly mentioned here.
 */
typedef enum pjsip_method_e
{
    PJSIP_INVITE_METHOD,    /**< INVITE method, for establishing dialogs.   */
    PJSIP_CANCEL_METHOD,    /**< CANCEL method, for cancelling request.	    */
    PJSIP_ACK_METHOD,	    /**< ACK method.				    */
    PJSIP_BYE_METHOD,	    /**< BYE method, for terminating dialog.	    */
    PJSIP_REGISTER_METHOD,  /**< REGISTER method.			    */
    PJSIP_OPTIONS_METHOD,   /**< OPTIONS method.			    */

    PJSIP_OTHER_METHOD	    /**< Other method.				    */

} pjsip_method_e;



/**
 * This structure represents a SIP method.
 * Application must always use either #pjsip_method_init or #pjsip_method_set
 * to make sure that method name is initialized correctly. This way, the name
 * member will always contain a valid method string regardless whether the ID
 * is recognized or not.
 */
struct pjsip_method
{
    pjsip_method_e id;	    /**< Method ID, from \a pjsip_method_e. */
    pj_str_t	   name;    /**< Method name, which will always contain the 
			         method string. */
};


/*
 * For convenience, standard method structures are defined in the library.
 */
/** INVITE method constant. @see pjsip_get_invite_method() */
PJ_DECL_DATA(const pjsip_method) pjsip_invite_method;

/** CANCEL method constant. @see pjsip_get_cancel_method() */
PJ_DECL_DATA(const pjsip_method) pjsip_cancel_method;

/** ACK method constant. @see pjsip_get_ack_method() */
PJ_DECL_DATA(const pjsip_method) pjsip_ack_method;

/** BYE method constant. @see pjsip_get_bye_method() */
PJ_DECL_DATA(const pjsip_method) pjsip_bye_method;

/** REGISTER method constant. @see pjsip_get_register_method() */
PJ_DECL_DATA(const pjsip_method) pjsip_register_method;

/** OPTIONS method constant. @see pjsip_get_options_method() */
PJ_DECL_DATA(const pjsip_method) pjsip_options_method;

/*
 * Accessor functions for standard SIP methods.
 */
/** Get INVITE method constant. */
PJ_DECL(const pjsip_method*) pjsip_get_invite_method(void);
/** Get CANCEL method constant. */
PJ_DECL(const pjsip_method*) pjsip_get_cancel_method(void);
/** Get ACK method constant. */
PJ_DECL(const pjsip_method*) pjsip_get_ack_method(void);
/** Get BYE method constant. */
PJ_DECL(const pjsip_method*) pjsip_get_bye_method(void);
/** Get REGISTER method constant.*/
PJ_DECL(const pjsip_method*) pjsip_get_register_method(void);
/** Get OPTIONS method constant. */
PJ_DECL(const pjsip_method*) pjsip_get_options_method(void);


/*
 * Accessor functions
 */

/** 
 * Initialize the method structure from a string. 
 * This function will check whether the method is a known method then set
 * both the id and name accordingly.
 *
 * @param m	The method to initialize.
 * @param pool	Pool where memory allocation will be allocated from, if required.
 * @param str	The method string.
 */
PJ_DECL(void) pjsip_method_init( pjsip_method *m, 
				 pj_pool_t *pool, 
				 const pj_str_t *str);

/** 
 * Initialize the method structure from a string, without cloning the string.
 * See #pjsip_method_init.
 *
 * @param m	The method structure to be initialized.
 * @param str	The method string.
 */
PJ_DECL(void) pjsip_method_init_np( pjsip_method *m,
				    pj_str_t *str);

/** 
 * Set the method with the predefined method ID. 
 * This function will also set the name member of the structure to the correct
 * string according to the method.
 *
 * @param m	The method structure.
 * @param id	The method ID.
 */
PJ_DECL(void) pjsip_method_set( pjsip_method *m, pjsip_method_e id );


/** 
 * Copy one method structure to another. If the method is of the known methods,
 * then memory allocation is not required.
 *
 * @param pool	    Pool to allocate memory from, if required.
 * @param method    The destination method to copy to.
 * @param rhs	    The source method to copy from.
 */
PJ_DECL(void) pjsip_method_copy( pj_pool_t *pool,
				 pjsip_method *method,
				 const pjsip_method *rhs );

/** 
 * Compare one method with another, and conveniently determine whether the 
 * first method is equal, less than, or greater than the second method.
 *
 * @param m1	The first method.
 * @param m2	The second method.
 *
 * @return	Zero if equal, otherwise will return -1 if less or +1 if greater.
 */
PJ_DECL(int) pjsip_method_cmp( const pjsip_method *m1, const pjsip_method *m2);

/**
 * @}
 */

/* **************************************************************************/
/** 
 * @defgroup PJSIP_MSG_HDR Header Fields
 * @brief Declarations for various SIP header fields.
 * @ingroup PJSIP_MSG
 * @{
 */

/**
 * Header types, as defined by RFC3261.
 */
typedef enum pjsip_hdr_e
{
    /*
     * These are the headers documented in RFC3261. Headers not documented
     * there must have type PJSIP_H_OTHER, and the header type itself is 
     * recorded in the header name string.
     *
     * DO NOT CHANGE THE VALUE/ORDER OF THE HEADER IDs!!!.
     */
    PJSIP_H_ACCEPT,
    PJSIP_H_ACCEPT_ENCODING_UNIMP,	/* N/A, use pjsip_generic_string_hdr */
    PJSIP_H_ACCEPT_LANGUAGE_UNIMP,	/* N/A, use pjsip_generic_string_hdr */
    PJSIP_H_ALERT_INFO_UNIMP,		/* N/A, use pjsip_generic_string_hdr */
    PJSIP_H_ALLOW,
    PJSIP_H_AUTHENTICATION_INFO_UNIMP,	/* N/A, use pjsip_generic_string_hdr */
    PJSIP_H_AUTHORIZATION,
    PJSIP_H_CALL_ID,
    PJSIP_H_CALL_INFO_UNIMP,		/* N/A, use pjsip_generic_string_hdr */
    PJSIP_H_CONTACT,
    PJSIP_H_CONTENT_DISPOSITION_UNIMP,	/* N/A, use pjsip_generic_string_hdr */
    PJSIP_H_CONTENT_ENCODING_UNIMP,	/* N/A, use pjsip_generic_string_hdr */
    PJSIP_H_CONTENT_LANGUAGE_UNIMP,	/* N/A, use pjsip_generic_string_hdr */
    PJSIP_H_CONTENT_LENGTH,
    PJSIP_H_CONTENT_TYPE,
    PJSIP_H_CSEQ,
    PJSIP_H_DATE_UNIMP,			/* N/A, use pjsip_generic_string_hdr */
    PJSIP_H_ERROR_INFO_UNIMP,		/* N/A, use pjsip_generic_string_hdr */
    PJSIP_H_EXPIRES,
    PJSIP_H_FROM,
    PJSIP_H_IN_REPLY_TO_UNIMP,		/* N/A, use pjsip_generic_string_hdr */
    PJSIP_H_MAX_FORWARDS,
    PJSIP_H_MIME_VERSION_UNIMP,		/* N/A, use pjsip_generic_string_hdr */
    PJSIP_H_MIN_EXPIRES,
    PJSIP_H_ORGANIZATION_UNIMP,		/* N/A, use pjsip_generic_string_hdr */
    PJSIP_H_PRIORITY_UNIMP,		/* N/A, use pjsip_generic_string_hdr */
    PJSIP_H_PROXY_AUTHENTICATE,
    PJSIP_H_PROXY_AUTHORIZATION,
    PJSIP_H_PROXY_REQUIRE_UNIMP,	/* N/A, use pjsip_generic_string_hdr */
    PJSIP_H_RECORD_ROUTE,
    PJSIP_H_REPLY_TO_UNIMP,		/* N/A, use pjsip_generic_string_hdr */
    PJSIP_H_REQUIRE,
    PJSIP_H_RETRY_AFTER,
    PJSIP_H_ROUTE,
    PJSIP_H_SERVER_UNIMP,		/* N/A, use pjsip_generic_string_hdr */
    PJSIP_H_SUBJECT_UNIMP,		/* N/A, use pjsip_generic_string_hdr */
    PJSIP_H_SUPPORTED,
    PJSIP_H_TIMESTAMP_UNIMP,		/* N/A, use pjsip_generic_string_hdr */
    PJSIP_H_TO,
	PJSIP_H_UNSUPPORTED,
    PJSIP_H_USER_AGENT,
    PJSIP_H_VIA,
    PJSIP_H_WARNING_UNIMP,		/* N/A, use pjsip_generic_string_hdr */
	PJSIP_H_WWW_AUTHENTICATE,
	PJSIP_H_TNL_SUPPORTED,

    PJSIP_H_OTHER

} pjsip_hdr_e;

/**
 * This structure provides the pointer to basic functions that are needed
 * for generic header operations. All header fields will have pointer to
 * this structure, so that they can be manipulated uniformly.
 */
typedef struct pjsip_hdr_vptr
{
    /** 
     * Function to clone the header. 
     *
     * @param pool  Memory pool to allocate the new header.
     * @param hdr   Header to clone.
     *
     * @return A new instance of the header.
     */
    void *(*clone)(pj_pool_t *pool, const void *hdr);

    /** 
     * Pointer to function to shallow clone the header. 
     * Shallow cloning will just make a memory copy of the original header,
     * thus all pointers in original header will be kept intact. Because the
     * function does not need to perform deep copy, the operation should be
     * faster, but the application must make sure that the original header
     * is still valid throughout the lifetime of new header.
     *
     * @param pool  Memory pool to allocate the new header.
     * @param hdr   The header to clone.
     */
    void *(*shallow_clone)(pj_pool_t *pool, const void *hdr);

    /** Pointer to function to print the header to the specified buffer.
     *	Returns the length of string written, or -1 if the remaining buffer
     *	is not enough to hold the header.
     *
     *  @param hdr  The header to print.
     *  @param buf  The buffer.
     *  @param len  The size of the buffer.
     *
     *  @return	    The size copied to buffer, or -1 if there's not enough space.
     */
    int (*print_on)(void *hdr, char *buf, pj_size_t len);

} pjsip_hdr_vptr;


/**
 * Generic fields for all SIP headers are declared using this macro, to make
 * sure that all headers will have exactly the same layout in their start of
 * the storage. This behaves like C++ inheritance actually.
 */
#define PJSIP_DECL_HDR_MEMBER(hdr)   \
    /** List members. */	\
    PJ_DECL_LIST_MEMBER(hdr);	\
    /** Header type */		\
    pjsip_hdr_e	    type;	\
    /** Header name. */		\
    pj_str_t	    name;	\
    /** Header short name version. */	\
    pj_str_t	    sname;		\
    /** Virtual function table. */	\
    pjsip_hdr_vptr *vptr


/**
 * Generic SIP header structure, for generic manipulation for headers in the
 * message. All header fields can be typecasted to this type.
 */
struct pjsip_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_hdr);
};


/**
 * This generic function will clone any header, by calling "clone" function
 * in header's virtual function table.
 *
 * @param pool	    The pool to allocate memory from.
 * @param hdr	    The header to clone.
 *
 * @return	    A new instance copied from the original header.
 */
PJ_DECL(void*) pjsip_hdr_clone( pj_pool_t *pool, const void *hdr );


/**
 * This generic function will clone any header, by calling "shallow_clone" 
 * function in header's virtual function table.
 *
 * @param pool	    The pool to allocate memory from.
 * @param hdr	    The header to clone.
 *
 * @return	    A new instance copied from the original header.
 */
PJ_DECL(void*) pjsip_hdr_shallow_clone( pj_pool_t *pool, const void *hdr );

/**
 * This generic function will print any header, by calling "print" 
 * function in header's virtual function table.
 *
 * @param hdr  The header to print.
 * @param buf  The buffer.
 * @param len  The size of the buffer.
 *
 * @return	The size copied to buffer, or -1 if there's not enough space.
 */
PJ_DECL(int) pjsip_hdr_print_on( void *hdr, char *buf, pj_size_t len);

/**
 * @}
 */

/* **************************************************************************/
/**
 * @defgroup PJSIP_MSG_LINE Request and Status Line.
 * @brief Request and status line structures and manipulation.
 * @ingroup PJSIP_MSG
 * @{
 */

/**
 * This structure describes SIP request line.
 */
typedef struct pjsip_request_line 
{
    pjsip_method    method; /**< Method for this request line. */
    pjsip_uri *uri;    /**< URI for this request line. */
} pjsip_request_line;


/**
 * This structure describes SIP status line.
 */
typedef struct pjsip_status_line 
{
    int		code;	    /**< Status code. */
    pj_str_t	reason;	    /**< Reason string. */
} pjsip_status_line;


/**
 * This enumeration lists standard SIP status codes according to RFC 3261.
 * In addition, it also declares new status class 7xx for errors generated
 * by the stack. This status class however should not get transmitted on the 
 * wire.
 */
typedef enum pjsip_status_code
{
    PJSIP_SC_TRYING = 100,
    PJSIP_SC_RINGING = 180,
    PJSIP_SC_CALL_BEING_FORWARDED = 181,
    PJSIP_SC_QUEUED = 182,
    PJSIP_SC_PROGRESS = 183,

    PJSIP_SC_OK = 200,
    PJSIP_SC_ACCEPTED = 202,

    PJSIP_SC_MULTIPLE_CHOICES = 300,
    PJSIP_SC_MOVED_PERMANENTLY = 301,
    PJSIP_SC_MOVED_TEMPORARILY = 302,
    PJSIP_SC_USE_PROXY = 305,
    PJSIP_SC_ALTERNATIVE_SERVICE = 380,

    PJSIP_SC_BAD_REQUEST = 400,
    PJSIP_SC_UNAUTHORIZED = 401,
    PJSIP_SC_PAYMENT_REQUIRED = 402,
    PJSIP_SC_FORBIDDEN = 403,
    PJSIP_SC_NOT_FOUND = 404,
    PJSIP_SC_METHOD_NOT_ALLOWED = 405,
    PJSIP_SC_NOT_ACCEPTABLE = 406,
    PJSIP_SC_PROXY_AUTHENTICATION_REQUIRED = 407,
    PJSIP_SC_REQUEST_TIMEOUT = 408,
    PJSIP_SC_GONE = 410,
    PJSIP_SC_REQUEST_ENTITY_TOO_LARGE = 413,
    PJSIP_SC_REQUEST_URI_TOO_LONG = 414,
    PJSIP_SC_UNSUPPORTED_MEDIA_TYPE = 415,
    PJSIP_SC_UNSUPPORTED_URI_SCHEME = 416,
    PJSIP_SC_BAD_EXTENSION = 420,
    PJSIP_SC_EXTENSION_REQUIRED = 421,
    PJSIP_SC_SESSION_TIMER_TOO_SMALL = 422,
    PJSIP_SC_INTERVAL_TOO_BRIEF = 423,
    PJSIP_SC_TEMPORARILY_UNAVAILABLE = 480,
    PJSIP_SC_CALL_TSX_DOES_NOT_EXIST = 481,
    PJSIP_SC_LOOP_DETECTED = 482,
    PJSIP_SC_TOO_MANY_HOPS = 483,
    PJSIP_SC_ADDRESS_INCOMPLETE = 484,
    PJSIP_AC_AMBIGUOUS = 485,
    PJSIP_SC_BUSY_HERE = 486,
    PJSIP_SC_REQUEST_TERMINATED = 487,
    PJSIP_SC_NOT_ACCEPTABLE_HERE = 488,
    PJSIP_SC_BAD_EVENT = 489,
    PJSIP_SC_REQUEST_UPDATED = 490,
    PJSIP_SC_REQUEST_PENDING = 491,
    PJSIP_SC_UNDECIPHERABLE = 493,

    PJSIP_SC_INTERNAL_SERVER_ERROR = 500,
    PJSIP_SC_NOT_IMPLEMENTED = 501,
    PJSIP_SC_BAD_GATEWAY = 502,
    PJSIP_SC_SERVICE_UNAVAILABLE = 503,
    PJSIP_SC_SERVER_TIMEOUT = 504,
    PJSIP_SC_VERSION_NOT_SUPPORTED = 505,
    PJSIP_SC_MESSAGE_TOO_LARGE = 513,
    PJSIP_SC_PRECONDITION_FAILURE = 580,

    PJSIP_SC_BUSY_EVERYWHERE = 600,
    PJSIP_SC_DECLINE = 603,
    PJSIP_SC_DOES_NOT_EXIST_ANYWHERE = 604,
    PJSIP_SC_NOT_ACCEPTABLE_ANYWHERE = 606,

    PJSIP_SC_TSX_TIMEOUT = PJSIP_SC_REQUEST_TIMEOUT,
    /*PJSIP_SC_TSX_RESOLVE_ERROR = 702,*/
    PJSIP_SC_TSX_TRANSPORT_ERROR = PJSIP_SC_SERVICE_UNAVAILABLE,

    /* This is not an actual status code, but rather a constant
     * to force GCC to use 32bit to represent this enum, since
     * we have a code in PJSUA-LIB that assigns an integer
     * to this enum (see pjsua_acc_get_info() function).
     */
    PJSIP_SC__force_32bit = 0x7FFFFFFF

} pjsip_status_code;

/**
 * Get the default status text for the status code.
 *
 * @param status_code	    SIP Status Code
 *
 * @return		    textual message for the status code.
 */ 
PJ_DECL(const pj_str_t*) pjsip_get_status_text(int status_code);

/**
 * This macro returns non-zero (TRUE) if the specified status_code is
 * in the same class as the code_class.
 *
 * @param status_code	The status code.
 * @param code_class	The status code in the class (for example 100, 200).
 */
#define PJSIP_IS_STATUS_IN_CLASS(status_code, code_class)    \
	    (status_code/100 == code_class/100)

/**
 * @}
 */

/* **************************************************************************/
/**
 * @addtogroup PJSIP_MSG_MEDIA Media/MIME Type
 * @brief Media/MIME type declaration and manipulations.
 * @ingroup PJSIP_MSG
 * @{
 */

/**
 * This structure describes SIP media type, as used for example in 
 * Accept and Content-Type header..
 */
typedef struct pjsip_media_type
{
    pj_str_t type;	    /**< Media type. */
    pj_str_t subtype;	    /**< Media subtype. */
    pjsip_param param;	    /**< Media type parameters */
} pjsip_media_type;


/**
 * Initialize the media type with the specified type and subtype string.
 *
 * @param mt		The media type.
 * @param type		Optionally specify the media type.
 * @param subtype	Optionally specify the media subtype.
 */
PJ_DECL(void) pjsip_media_type_init(pjsip_media_type *mt,
				    pj_str_t *type,
				    pj_str_t *subtype);

/**
 * Initialize the media type with the specified type and subtype string.
 *
 * @param mt		The media type.
 * @param type		Optionally specify the media type.
 * @param subtype	Optionally specify the media subtype.
 */
PJ_DECL(void) pjsip_media_type_init2(pjsip_media_type *mt,
				     char *type,
				     char *subtype);

/**
 * Compare two media types.
 *
 * @param mt1		The first media type.
 * @param mt2		The second media type.
 * @param cmp_param	Specify how to compare the media type parameters:
 * 			 - 0: do not compare parameters
 * 			 - 1: compare parameters but ignore parameters that
 * 			      only appear in one of the media type.
 * 			 - 2: compare the parameters.
 *
 * @return		Zero if both media types are equal, -1 if mt1 < mt2,
 * 			1 if mt1 > mt2.
 */
PJ_DECL(int) pjsip_media_type_cmp(const pjsip_media_type *mt1,
				  const pjsip_media_type *mt2,
				  int cmp_param);

/**
 * Copy SIP media type to another.
 *
 * @param pool	    Pool to duplicate strings.
 * @param dst	    Destination structure.
 * @param src	    Source structure.
 */
PJ_DECL(void) pjsip_media_type_cp(pj_pool_t *pool,
				  pjsip_media_type *dst,
				  const pjsip_media_type *src);

/**
 * Print media type to the specified buffer.
 *
 * @param buf		Destination buffer.
 * @param len		Length of the buffer.
 * @param mt		The media type to be printed.
 *
 * @return		The number of characters printed to the buffer, or -1
 * 			if there's not enough space in the buffer.
 */
PJ_DECL(int) pjsip_media_type_print(char *buf, unsigned len,
				    const pjsip_media_type *mt);

/**
 * @}
 */

/* **************************************************************************/
/**
 * @addtogroup PJSIP_MSG_BODY Message Body
 * @brief SIP message body structures and manipulation.
 * @ingroup PJSIP_MSG
 * @{
 */

/**
 * Generic abstraction to message body.
 * When an incoming message is parsed (pjsip_parse_msg()), the parser fills in
 * all members with the appropriate value. The 'data' and 'len' member will
 * describe portion of incoming packet which denotes the message body.
 * When application needs to attach message body to outgoing SIP message, it
 * must fill in all members of this structure. 
 */
struct pjsip_msg_body
{
    /** MIME content type. 
     *  For incoming messages, the parser will fill in this member with the
     *  content type found in Content-Type header.
     *
     *  For outgoing messages, application may fill in this member with
     *  appropriate value, because the stack will generate Content-Type header
     *  based on the value specified here.
     *
     *  If the content_type is empty, no Content-Type AND Content-Length header
     *  will be added to the message. The stack assumes that application adds
     *  these headers themselves.
     */
    pjsip_media_type content_type;

    /** Pointer to buffer which holds the message body data. 
     *  For incoming messages, the parser will fill in this member with the
     *  pointer to the body string.
     *
     *  When sending outgoing message, this member doesn't need to point to the
     *  actual message body string. It can be assigned with arbitrary pointer,
     *  because the value will only need to be understood by the print_body()
     *  function. The stack itself will not try to interpret this value, but
     *  instead will always call the print_body() whenever it needs to get the
     *  actual body string.
     */
    void *data;

    /** The length of the data. 
     *  For incoming messages, the parser will fill in this member with the
     *  actual length of message body.
     *
     *  When sending outgoing message, again just like the "data" member, the
     *  "len" member doesn't need to point to the actual length of the body 
     *  string.
     */
    unsigned len;

    /** Pointer to function to print this message body. 
     *  Application must set a proper function here when sending outgoing 
     *  message.
     *
     *  @param msg_body	    This structure itself.
     *  @param buf	    The buffer.
     *  @param size	    The buffer size.
     *
     *  @return		    The length of the string printed, or -1 if there is
     *			    not enough space in the buffer to print the whole
     *			    message body.
     */
    int (*print_body)(struct pjsip_msg_body *msg_body, 
		      char *buf, pj_size_t size);

    /** Clone the data part only of this message body. Note that this only
     *  duplicates the data part of the body instead of the whole message
     *  body. If application wants to duplicate the entire message body
     *  structure, it must call #pjsip_msg_body_clone().
     *
     *  @param pool	    Pool used to clone the data.
     *  @param data	    The data inside message body, to be cloned.
     *  @param len	    The length of the data.
     *
     *  @return		    New data duplicated from the original data.
     */
    void* (*clone_data)(pj_pool_t *pool, const void *data, unsigned len);

};

/**
 * General purpose function to textual data in a SIP body. Attach this function
 * in a SIP message body only if the data in pjsip_msg_body is a textual 
 * message ready to be embedded in a SIP message. If the data in the message
 * body is not a textual body, then application must supply a custom function
 * to print that body.
 *
 * @param msg_body	The message body.
 * @param buf		Buffer to copy the message body to.
 * @param size		The size of the buffer.
 *
 * @return		The length copied to the buffer, or -1.
 */
PJ_DECL(int) pjsip_print_text_body( pjsip_msg_body *msg_body, 
				    char *buf, pj_size_t size);

/**
 * General purpose function to clone textual data in a SIP body. Attach this
 * function as "clone_data" member of the SIP body only if the data type
 * is a text (i.e. C string, not pj_str_t), and the length indicates the
 * length of the text.
 *
 *  @param pool		Pool used to clone the data.
 *  @param data		Textual data.
 *  @param len		The length of the string.
 *
 *  @return		New text duplicated from the original text.
 */
PJ_DECL(void*) pjsip_clone_text_data( pj_pool_t *pool, const void *data,
				      unsigned len);


/**
 * Clone the message body in src_body to the dst_body. This will duplicate
 * the contents of the message body using the \a clone_data member of the
 * source message body.
 *
 * @param pool		Pool to use to duplicate the message body.
 * @param dst_body	Destination message body.
 * @param src_body	Source message body to duplicate.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjsip_msg_body_copy( pj_pool_t *pool,
					  pjsip_msg_body *dst_body,
					  const pjsip_msg_body *src_body );
					   

/**
 * Create cloned message body. This will duplicate the contents of the message
 * body using the \a clone_data member of the source message body.
 *
 * @param pool		Pool to use to duplicate the message body.
 * @param body		Source message body to duplicate.
 *
 * @return		The cloned message body on successfull.
 */
PJ_DECL(pjsip_msg_body*) pjsip_msg_body_clone( pj_pool_t *pool,
					       const pjsip_msg_body *body );
					   

/**
 * Create a text message body. Use this function to create message body when
 * the content is a simple text. For non-text message body (e.g. 
 * pjmedia_sdp_session or pj_xml_node), application must construct the message
 * manually.
 *
 * @param pool		Pool to allocate message body and its contents.
 * @param type		MIME type (e.g. "text").
 * @param subtype	MIME subtype (e.g. "plain").
 * @param text		The text content to be put in the message body.
 *
 * @return		A new message body with the specified Content-Type and
 *			text.
 */
PJ_DECL(pjsip_msg_body*) pjsip_msg_body_create( pj_pool_t *pool,
					        const pj_str_t *type,
						const pj_str_t *subtype,
						const pj_str_t *text );

/**
 * @}
 */

/* **************************************************************************/
/**
 * @defgroup PJSIP_MSG_MSG Message Structure
 * @brief SIP message (request and response) structure and operations.
 * @ingroup PJSIP_MSG
 * @{
 */

/**
 * Message type (request or response).
 */
typedef enum pjsip_msg_type_e
{
    PJSIP_REQUEST_MSG,	    /**< Indicates request message. */
    PJSIP_RESPONSE_MSG	    /**< Indicates response message. */
} pjsip_msg_type_e;


/**
 * This structure describes a SIP message.
 */
struct pjsip_msg
{
    /** Message type (ie request or response). */
    pjsip_msg_type_e  type;

    /** The first line of the message can be either request line for request
     *	messages, or status line for response messages. It is represented here
     *  as a union.
     */
    union
    {
	/** Request Line. */
	struct pjsip_request_line   req;

	/** Status Line. */
	struct pjsip_status_line    status;
    } line;

    /** List of message headers. */
    pjsip_hdr hdr;

    /** Pointer to message body, or NULL if no message body is attached to
     *	this mesage. 
     */
    pjsip_msg_body *body;
};


/** 
 * Create new request or response message.
 *
 * @param pool	    The pool.
 * @param type	    Message type.
 * @return	    New message, or THROW exception if failed.
 */
PJ_DECL(pjsip_msg*)  pjsip_msg_create( pj_pool_t *pool, pjsip_msg_type_e type);


/**
 * Perform a deep clone of a SIP message.
 *
 * @param pool	    The pool for creating the new message.
 * @param msg	    The message to be duplicated.
 *
 * @return	    New message, which is duplicated from the original 
 *		    message.
 */
PJ_DECL(pjsip_msg*) pjsip_msg_clone( pj_pool_t *pool, const pjsip_msg *msg);


/** 
 * Find a header in the message by the header type.
 *
 * @param msg	    The message.
 * @param type	    The header type to find.
 * @param start	    The first header field where the search should begin.
 *		    If NULL is specified, then the search will begin from the
 *		    first header, otherwise the search will begin at the
 *		    specified header.
 *
 * @return	    The header field, or NULL if no header with the specified 
 *		    type is found.
 */
PJ_DECL(void*)  pjsip_msg_find_hdr( const pjsip_msg *msg, 
				    pjsip_hdr_e type, const void *start);

/** 
 * Find a header in the message by its name.
 *
 * @param msg	    The message.
 * @param name	    The header name to find.
 * @param start	    The first header field where the search should begin.
 *		    If NULL is specified, then the search will begin from the
 *		    first header, otherwise the search will begin at the
 *		    specified header.
 *
 * @return	    The header field, or NULL if no header with the specified 
 *		    type is found.
 */
PJ_DECL(void*)  pjsip_msg_find_hdr_by_name( const pjsip_msg *msg, 
					    const pj_str_t *name, 
					    const void *start);

/** 
 * Find a header in the message by its name and short name version.
 *
 * @param msg	    The message.
 * @param name	    The header name to find.
 * @param sname	    The short name version of the header name.
 * @param start	    The first header field where the search should begin.
 *		    If NULL is specified, then the search will begin from the
 *		    first header, otherwise the search will begin at the
 *		    specified header.
 *
 * @return	    The header field, or NULL if no header with the specified 
 *		    type is found.
 */
PJ_DECL(void*)  pjsip_msg_find_hdr_by_names(const pjsip_msg *msg, 
					    const pj_str_t *name, 
					    const pj_str_t *sname,
					    const void *start);

/** 
 * Find and remove a header in the message. 
 *
 * @param msg	    The message.
 * @param hdr	    The header type to find.
 * @param start	    The first header field where the search should begin,
 *		    or NULL to search from the first header in the message.
 *
 * @return	    The header field, or NULL if not found.
 */
PJ_DECL(void*)  pjsip_msg_find_remove_hdr( pjsip_msg *msg, 
					   pjsip_hdr_e hdr, void *start);

/** 
 * Add a header to the message, putting it last in the header list.
 *
 * @param msg	    The message.
 * @param hdr	    The header to add.
 *
 * @bug Once the header is put in a list (or message), it can not be put in 
 *      other list (or message). Otherwise Real Bad Thing will happen.
 */
PJ_INLINE(void) pjsip_msg_add_hdr( pjsip_msg *msg, pjsip_hdr *hdr )
{
    pj_list_insert_before(&msg->hdr, hdr);
}

/** 
 * Add header field to the message, putting it in the front of the header list.
 *
 * @param msg	The message.
 * @param hdr	The header to add.
 *
 * @bug Once the header is put in a list (or message), it can not be put in 
 *      other list (or message). Otherwise Real Bad Thing will happen.
 */
PJ_INLINE(void) pjsip_msg_insert_first_hdr( pjsip_msg *msg, pjsip_hdr *hdr )
{
    pj_list_insert_after(&msg->hdr, hdr);
}

/** 
 * Print the message to the specified buffer. 
 *
 * @param msg	The message to print.
 * @param buf	The buffer
 * @param size	The size of the buffer.
 *
 * @return	The length of the printed characters (in bytes), or NEGATIVE
 *		value if the message is too large for the specified buffer.
 */
PJ_DECL(pj_ssize_t) pjsip_msg_print(const pjsip_msg *msg, 
				    char *buf, pj_size_t size);


/*
 * Some usefull macros to find common headers.
 */


/**
 * Find Call-ID header.
 *
 * @param msg	The message.
 * @return	Call-ID header instance.
 */
#define PJSIP_MSG_CID_HDR(msg) \
	    ((pjsip_cid_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_CALL_ID, NULL))

/**
 * Find CSeq header.
 *
 * @param msg	The message.
 * @return	CSeq header instance.
 */
#define PJSIP_MSG_CSEQ_HDR(msg) \
	    ((pjsip_cseq_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_CSEQ, NULL))

/**
 * Find From header.
 *
 * @param msg	The message.
 * @return	From header instance.
 */
#define PJSIP_MSG_FROM_HDR(msg) \
	    ((pjsip_from_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_FROM, NULL))

/**
 * Find To header.
 *
 * @param msg	The message.
 * @return	To header instance.
 */
#define PJSIP_MSG_TO_HDR(msg) \
	    ((pjsip_to_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_TO, NULL))


/**
 * DEAN Added
 * Find User-Agent header.
 *
 * @param msg	The message.
 * @return	User-Agent header instance.
 */
#define PJSIP_MSG_USER_AGENT_HDR(msg) \
	    ((pjsip_user_agent_hdr*)pjsip_msg_find_hdr(msg, PJSIP_H_USER_AGENT, NULL))


/**
 * @}
 */

/* **************************************************************************/
/**
 * @addtogroup PJSIP_MSG_HDR
 * @{
 */

/**
 * Generic SIP header, which contains hname and a string hvalue.
 * Note that this header is not supposed to be used as 'base' class for headers.
 */
typedef struct pjsip_generic_string_hdr
{
    /** Standard header field. */
    PJSIP_DECL_HDR_MEMBER(struct pjsip_generic_string_hdr);
    /** hvalue */
    pj_str_t hvalue;
} pjsip_generic_string_hdr;


/**
 * Create a new instance of generic header. A generic header can have an
 * arbitrary header name.
 *
 * @param pool	    The pool.
 * @param hname	    The header name to be assigned to the header, or NULL to
 *		    assign the header name with some string.
 * @param hvalue    Optional string to be assigned as the value.
 *
 * @return	    The header, or THROW exception.
 */
PJ_DECL(pjsip_generic_string_hdr*) 
pjsip_generic_string_hdr_create( pj_pool_t *pool, 
				 const pj_str_t *hname,
				 const pj_str_t *hvalue);


/**
 * Initialize a preallocated memory with the header structure. This function
 * should only be called when application uses its own memory allocation to
 * allocate memory block for the specified header (e.g. in C++, when the 
 * header is allocated with "new" operator).
 * For normal applications, they should use pjsip_xxx_hdr_create() instead,
 * which allocates memory and initialize it in one go.
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 * @param hname	    The header name to be assigned to the header, or NULL to
 *		    assign the header name with some string later.
 * @param hvalue    Optional string to be assigned as the value.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_generic_string_hdr*) 
pjsip_generic_string_hdr_init( pj_pool_t *pool,
			       void *mem,
			       const pj_str_t *hname,
			       const pj_str_t *hvalue);


/**
 * Construct a generic string header without allocating memory from the pool.
 * This function is useful to create a temporary header which life-time is
 * very short (for example, creating the header in the stack to be passed
 * as argument to a function which will copy the header).
 *
 * @param h	    The header to be initialized.
 * @param hname	    The header name to be assigned to the header, or NULL to
 *		    assign the header name with some string.
 * @param hvalue    Optional string to be assigned as the value.
 *
 * @return	    The header, or THROW exception.
 */
PJ_DECL(void) pjsip_generic_string_hdr_init2(pjsip_generic_string_hdr *h,
					     pj_str_t *hname,
					     pj_str_t *hvalue);


/* **************************************************************************/

/**
 * Generic SIP header, which contains hname and a string hvalue.
 */
typedef struct pjsip_generic_int_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_generic_int_hdr); /**< Standard header field. */
    pj_int32_t ivalue;				    /**< ivalue */
} pjsip_generic_int_hdr;


/**
 * Create a new instance of generic header. A generic header can have an
 * arbitrary header name.
 *
 * @param pool	    The pool.
 * @param hname	    The header name to be assigned to the header, or NULL to
 *		    assign the header name with some string.
 * @param hvalue    The value to be assigned to the header.
 *
 * @return	    The header, or THROW exception.
 */
PJ_DECL(pjsip_generic_int_hdr*) pjsip_generic_int_hdr_create( pj_pool_t *pool,
						      const pj_str_t *hname,
						      int hvalue );


/**
 * Initialize a preallocated memory with the header structure. This function
 * should only be called when application uses its own memory allocation to
 * allocate memory block for the specified header (e.g. in C++, when the 
 * header is allocated with "new" operator).
 * For normal applications, they should use pjsip_xxx_hdr_create() instead,
 * which allocates memory and initialize it in one go.
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 * @param hname	    The header name to be assigned to the header, or NULL to
 *		    assign the header name with some string later.
 * @param value	    Value to be assigned to the header.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_generic_int_hdr*) pjsip_generic_int_hdr_init( pj_pool_t *pool,
							    void *mem,
							    const pj_str_t *hname,
							    int value );

/* **************************************************************************/

/** Maximum elements in the header array. */
#define PJSIP_GENERIC_ARRAY_MAX_COUNT	32

/**
 * Generic array of string header.
 */
typedef struct pjsip_generic_array_hdr
{
    /** Standard header fields. */
    PJSIP_DECL_HDR_MEMBER(struct pjsip_generic_array_hdr);

    /** Number of tags/elements. */
    unsigned	count;

    /** Tags/elements. */
    pj_str_t	values[PJSIP_GENERIC_ARRAY_MAX_COUNT];

} pjsip_generic_array_hdr;

/**
 * Create generic array header.
 *
 * @param pool	    Pool to allocate memory from.
 * @param hname	    Header name.
 *
 * @return	    New generic array header.
 */
PJ_DECL(pjsip_generic_array_hdr*) pjsip_generic_array_hdr_create(pj_pool_t *pool,
							     const pj_str_t *hname);

/**
 * Initialize a preallocated memory with the header structure. This function
 * should only be called when application uses its own memory allocation to
 * allocate memory block for the specified header (e.g. in C++, when the 
 * header is allocated with "new" operator).
 * For normal applications, they should use pjsip_xxx_hdr_create() instead,
 * which allocates memory and initialize it in one go.
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 * @param hname	    The header name to be assigned to the header, or NULL to
 *		    assign the header name with some string later.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_generic_array_hdr*) pjsip_generic_array_hdr_init(pj_pool_t *pool,
							       void *mem,
							       const pj_str_t *hname);


/* **************************************************************************/

/** Accept header. */
typedef pjsip_generic_array_hdr pjsip_accept_hdr;

/** Maximum fields in Accept header. */
#define PJSIP_MAX_ACCEPT_COUNT	PJSIP_GENERIC_ARRAY_MAX_COUNT

/**
 * Create new Accept header instance.
 *
 * @param pool	    The pool.
 *
 * @return	    New Accept header instance.
 */
PJ_DECL(pjsip_accept_hdr*) pjsip_accept_hdr_create(pj_pool_t *pool);

/**
 * Initialize a preallocated memory with the header structure. This function
 * should only be called when application uses its own memory allocation to
 * allocate memory block for the specified header (e.g. in C++, when the 
 * header is allocated with "new" operator).
 * For normal applications, they should use pjsip_xxx_hdr_create() instead,
 * which allocates memory and initialize it in one go.
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_accept_hdr*) pjsip_accept_hdr_init( pj_pool_t *pool,
						  void *mem );


/* **************************************************************************/

/**
 * Allow header.
 */
typedef pjsip_generic_array_hdr pjsip_allow_hdr;

/**
 * Create new Allow header instance.
 *
 * @param pool	    The pool.
 *
 * @return	    New Allow header instance.
 */
PJ_DECL(pjsip_allow_hdr*) pjsip_allow_hdr_create(pj_pool_t *pool);



/**
 * Initialize a preallocated memory with the header structure. This function
 * should only be called when application uses its own memory allocation to
 * allocate memory block for the specified header (e.g. in C++, when the 
 * header is allocated with "new" operator).
 * For normal applications, they should use pjsip_xxx_hdr_create() instead,
 * which allocates memory and initialize it in one go.
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_allow_hdr*) pjsip_allow_hdr_init( pj_pool_t *pool,
						void *mem );

/* **************************************************************************/

/**
 * Call-ID header.
 */
typedef struct pjsip_cid_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_cid_hdr);
    pj_str_t id;	    /**< Call-ID string. */
} pjsip_cid_hdr;


/**
 * Create new Call-ID header.
 *
 * @param pool	The pool.
 *
 * @return	new Call-ID header.
 */
PJ_DECL(pjsip_cid_hdr*) pjsip_cid_hdr_create( pj_pool_t *pool );


/**
 * Initialize a preallocated memory with the header structure. This function
 * should only be called when application uses its own memory allocation to
 * allocate memory block for the specified header (e.g. in C++, when the 
 * header is allocated with "new" operator).
 * For normal applications, they should use pjsip_xxx_hdr_create() instead,
 * which allocates memory and initialize it in one go.
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_cid_hdr*) pjsip_cid_hdr_init( pj_pool_t *pool,
					    void *mem );



/* **************************************************************************/
/**
 * Content-Length header.
 */
typedef struct pjsip_clen_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_clen_hdr);
    int len;	/**< Content length. */
} pjsip_clen_hdr;

/**
 * Create new Content-Length header.
 *
 * @param pool	the pool.
 * @return	A new Content-Length header instance.
 */
PJ_DECL(pjsip_clen_hdr*) pjsip_clen_hdr_create( pj_pool_t *pool );

/**
 * Initialize a preallocated memory with the header structure. This function
 * should only be called when application uses its own memory allocation to
 * allocate memory block for the specified header (e.g. in C++, when the 
 * header is allocated with "new" operator).
 * For normal applications, they should use pjsip_xxx_hdr_create() instead,
 * which allocates memory and initialize it in one go.
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_clen_hdr*) pjsip_clen_hdr_init( pj_pool_t *pool,
					      void *mem );


/* **************************************************************************/
/**
 * CSeq header.
 */
typedef struct pjsip_cseq_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_cseq_hdr);
    pj_int32_t	    cseq;	/**< CSeq number. */
    pjsip_method    method;	/**< CSeq method. */
} pjsip_cseq_hdr;


/** Create new  CSeq header. 
 *
 *  @param pool	The pool.
 *  @return A new CSeq header instance.
 */
PJ_DECL(pjsip_cseq_hdr*) pjsip_cseq_hdr_create( pj_pool_t *pool );

/**
 * Initialize a preallocated memory with the header structure. This function
 * should only be called when application uses its own memory allocation to
 * allocate memory block for the specified header (e.g. in C++, when the 
 * header is allocated with "new" operator).
 * For normal applications, they should use pjsip_xxx_hdr_create() instead,
 * which allocates memory and initialize it in one go.
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_cseq_hdr*) pjsip_cseq_hdr_init( pj_pool_t *pool,
					      void *mem );

/* **************************************************************************/
/**
 * Contact header.
 * In this library, contact header only contains single URI. If a message has
 * multiple URI in the Contact header, the URI will be put in separate Contact
 * headers.
 */
typedef struct pjsip_contact_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_contact_hdr);
    int		    star;	    /**< The contact contains only a '*' character */
    pjsip_uri	   *uri;	    /**< URI in the contact. */
    int		    q1000;	    /**< The "q" value times 1000 (to avoid float) */
    pj_int32_t	    expires;	    /**< Expires parameter, otherwise -1 if not present. */
    pjsip_param	    other_param;    /**< Other parameters, concatenated in a single string. */
} pjsip_contact_hdr;


/**
 * Create a new Contact header.
 *
 * @param pool	The pool.
 * @return	A new instance of Contact header.
 */
PJ_DECL(pjsip_contact_hdr*) pjsip_contact_hdr_create( pj_pool_t *pool );

/**
 * Initialize a preallocated memory with the header structure. This function
 * should only be called when application uses its own memory allocation to
 * allocate memory block for the specified header (e.g. in C++, when the 
 * header is allocated with "new" operator).
 * For normal applications, they should use pjsip_xxx_hdr_create() instead,
 * which allocates memory and initialize it in one go.
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_contact_hdr*) pjsip_contact_hdr_init( pj_pool_t *pool,
						    void *mem );


/* **************************************************************************/
/**
 * Content-Type.
 */
typedef struct pjsip_ctype_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_ctype_hdr);
    pjsip_media_type media; /**< Media type. */
} pjsip_ctype_hdr;


/**
 * Create a nwe Content Type header.
 *
 * @param pool	The pool.
 * @return	A new Content-Type header.
 */
PJ_DECL(pjsip_ctype_hdr*) pjsip_ctype_hdr_create( pj_pool_t *pool );

/**
 * Initialize a preallocated memory with the header structure. This function
 * should only be called when application uses its own memory allocation to
 * allocate memory block for the specified header (e.g. in C++, when the 
 * header is allocated with "new" operator).
 * For normal applications, they should use pjsip_xxx_hdr_create() instead,
 * which allocates memory and initialize it in one go.
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_ctype_hdr*) pjsip_ctype_hdr_init( pj_pool_t *pool,
						void *mem );

/* **************************************************************************/
/** Expires header. */
typedef pjsip_generic_int_hdr pjsip_expires_hdr;

/**
 * Create a new Expires header.
 *
 * @param pool	    The pool.
 * @param value	    The expiration value.
 *
 * @return	    A new Expires header.
 */
PJ_DECL(pjsip_expires_hdr*) pjsip_expires_hdr_create( pj_pool_t *pool,
						      int value);

/**
 * Initialize a preallocated memory with the header structure. This function
 * should only be called when application uses its own memory allocation to
 * allocate memory block for the specified header (e.g. in C++, when the 
 * header is allocated with "new" operator).
 * For normal applications, they should use pjsip_xxx_hdr_create() instead,
 * which allocates memory and initialize it in one go.
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 * @param value	    The expiration value.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_expires_hdr*) pjsip_expires_hdr_init( pj_pool_t *pool,
						    void *mem,
						    int value );



/* **************************************************************************/
/**
 * To or From header.
 */
typedef struct pjsip_fromto_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_fromto_hdr);
    pjsip_uri	    *uri;	    /**< URI in From/To header. */
    pj_str_t	     tag;	    /**< Header "tag" parameter. */
    pjsip_param	     other_param;   /**< Other params, concatenated as a single string. */
} pjsip_fromto_hdr;

/** Alias for From header. */
typedef pjsip_fromto_hdr pjsip_from_hdr;

/** Alias for To header. */
typedef pjsip_fromto_hdr pjsip_to_hdr;

/**
 * Create a From header.
 *
 * @param pool	The pool.
 * @return	New instance of From header.
 */
PJ_DECL(pjsip_from_hdr*) pjsip_from_hdr_create( pj_pool_t *pool );

/**
 * Initialize a preallocated memory with the header structure. This function
 * should only be called when application uses its own memory allocation to
 * allocate memory block for the specified header (e.g. in C++, when the 
 * header is allocated with "new" operator).
 * For normal applications, they should use pjsip_xxx_hdr_create() instead,
 * which allocates memory and initialize it in one go.
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_from_hdr*) pjsip_from_hdr_init( pj_pool_t *pool,
					      void *mem );

/**
 * Create a To header.
 *
 * @param pool	The pool.
 * @return	New instance of To header.
 */
PJ_DECL(pjsip_to_hdr*)   pjsip_to_hdr_create( pj_pool_t *pool );

/**
 * Initialize a preallocated memory with the header structure. This function
 * should only be called when application uses its own memory allocation to
 * allocate memory block for the specified header (e.g. in C++, when the 
 * header is allocated with "new" operator).
 * For normal applications, they should use pjsip_xxx_hdr_create() instead,
 * which allocates memory and initialize it in one go.
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_to_hdr*) pjsip_to_hdr_init( pj_pool_t *pool,
					  void *mem );

/**
 * Convert the header to a From header.
 *
 * @param hdr	    The generic from/to header.
 * @return	    "From" header.
 */
PJ_DECL(pjsip_from_hdr*) pjsip_fromto_hdr_set_from( pjsip_fromto_hdr *hdr );

/**
 * Convert the header to a To header.
 *
 * @param hdr	    The generic from/to header.
 * @return	    "To" header.
 */
PJ_DECL(pjsip_to_hdr*)   pjsip_fromto_hdr_set_to( pjsip_fromto_hdr *hdr );


/* **************************************************************************/
/**
 * Max-Forwards header.
 */
typedef pjsip_generic_int_hdr pjsip_max_fwd_hdr;

/**
 * Create new Max-Forwards header instance.
 *
 * @param pool	    The pool.
 * @param value	    The Max-Forwards value.
 *
 * @return	    New Max-Forwards header instance.
 */
PJ_DECL(pjsip_max_fwd_hdr*) 
pjsip_max_fwd_hdr_create(pj_pool_t *pool, int value);


/**
 * Initialize a preallocated memory with the header structure. This function
 * should only be called when application uses its own memory allocation to
 * allocate memory block for the specified header (e.g. in C++, when the 
 * header is allocated with "new" operator).
 * For normal applications, they should use pjsip_xxx_hdr_create() instead,
 * which allocates memory and initialize it in one go.
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 * @param value	    The Max-Forwards value.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_max_fwd_hdr*) 
pjsip_max_fwd_hdr_init( pj_pool_t *pool, void *mem, int value );


/* **************************************************************************/
/**
 * Min-Expires header.
 */
typedef pjsip_generic_int_hdr pjsip_min_expires_hdr;

/**
 * Create new Min-Expires header instance.
 *
 * @param pool	    The pool.
 * @param value	    The Min-Expires value.
 *
 * @return	    New Min-Expires header instance.
 */
PJ_DECL(pjsip_min_expires_hdr*) pjsip_min_expires_hdr_create(pj_pool_t *pool,
							     int value);


/**
 * Initialize a preallocated memory with the header structure. This function
 * should only be called when application uses its own memory allocation to
 * allocate memory block for the specified header (e.g. in C++, when the 
 * header is allocated with "new" operator).
 * For normal applications, they should use pjsip_xxx_hdr_create() instead,
 * which allocates memory and initialize it in one go.
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 * @param value	    The Min-Expires value.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_min_expires_hdr*) pjsip_min_expires_hdr_init( pj_pool_t *pool,
							    void *mem,
							    int value );


/* **************************************************************************/
/**
 * Record-Route and Route headers.
 */
typedef struct pjsip_routing_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_routing_hdr);  /**< Generic header fields. */
    pjsip_name_addr  name_addr;	  /**< The URL in the Route/Record-Route header. */
    pjsip_param	     other_param; /**< Other parameter. */
} pjsip_routing_hdr;

/** Alias for Record-Route header. */
typedef pjsip_routing_hdr pjsip_rr_hdr;

/** Alias for Route header. */
typedef pjsip_routing_hdr pjsip_route_hdr;


/** 
 * Create new Record-Route header from the pool. 
 *
 * @param pool	The pool.
 * @return	A new instance of Record-Route header.
 */
PJ_DECL(pjsip_rr_hdr*)	    pjsip_rr_hdr_create( pj_pool_t *pool );

/**
 * Initialize a preallocated memory with the header structure. This function
 * should only be called when application uses its own memory allocation to
 * allocate memory block for the specified header (e.g. in C++, when the 
 * header is allocated with "new" operator).
 * For normal applications, they should use pjsip_xxx_hdr_create() instead,
 * which allocates memory and initialize it in one go.
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_rr_hdr*) pjsip_rr_hdr_init( pj_pool_t *pool,
					  void *mem );

/** 
 * Create new Route header from the pool. 
 *
 * @param pool	The pool.
 * @return	A new instance of "Route" header.
 */
PJ_DECL(pjsip_route_hdr*)   pjsip_route_hdr_create( pj_pool_t *pool );

/**
 * Initialize a preallocated memory with the header structure. This function
 * should only be called when application uses its own memory allocation to
 * allocate memory block for the specified header (e.g. in C++, when the 
 * header is allocated with "new" operator).
 * For normal applications, they should use pjsip_xxx_hdr_create() instead,
 * which allocates memory and initialize it in one go.
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_route_hdr*) pjsip_route_hdr_init( pj_pool_t *pool,
					        void *mem );

/** 
 * Convert generic routing header to Record-Route header. 
 *
 * @param r	The generic routing header, or a "Routing" header.
 * @return	Record-Route header.
 */
PJ_DECL(pjsip_rr_hdr*)	    pjsip_routing_hdr_set_rr( pjsip_routing_hdr *r );

/** 
 * Convert generic routing header to "Route" header. 
 *
 * @param r	The generic routing header, or a "Record-Route" header.
 * @return	"Route" header.
 */
PJ_DECL(pjsip_route_hdr*)   pjsip_routing_hdr_set_route( pjsip_routing_hdr *r );

/* **************************************************************************/
/**
 * Require header.
 */
typedef pjsip_generic_array_hdr pjsip_require_hdr;

/**
 * Create new Require header instance.
 *
 * @param pool	    The pool.
 *
 * @return	    New Require header instance.
 */
PJ_DECL(pjsip_require_hdr*) pjsip_require_hdr_create(pj_pool_t *pool);

/**
 * Initialize a preallocated memory with the header structure. This function
 * should only be called when application uses its own memory allocation to
 * allocate memory block for the specified header (e.g. in C++, when the 
 * header is allocated with "new" operator).
 * For normal applications, they should use pjsip_xxx_hdr_create() instead,
 * which allocates memory and initialize it in one go.
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_require_hdr*) pjsip_require_hdr_init( pj_pool_t *pool,
						    void *mem );


/* **************************************************************************/
/**
 * Retry-After header.
 */
typedef struct pjsip_retry_after_hdr
{
    /** Standard header field. */
    PJSIP_DECL_HDR_MEMBER(struct pjsip_retry_after_hdr); 
    pj_int32_t	ivalue;		/**< Retry-After value	    */
    pjsip_param	param;		/**< Optional parameters    */
    pj_str_t	comment;	/**< Optional comments.	    */
} pjsip_retry_after_hdr;


/**
 * Create new Retry-After header instance.
 *
 * @param pool	    The pool.
 * @param value	    The Retry-After value.
 *
 * @return	    New Retry-After header instance.
 */
PJ_DECL(pjsip_retry_after_hdr*) pjsip_retry_after_hdr_create(pj_pool_t *pool,
							     int value);

/**
 * Initialize a preallocated memory with the header structure. 
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 * @param value	    The Retry-After value.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_retry_after_hdr*) pjsip_retry_after_hdr_init( pj_pool_t *pool,
							    void *mem,
							    int value );


/* **************************************************************************/
/**
 * Supported header.
 */
typedef pjsip_generic_array_hdr pjsip_supported_hdr;

/**
 * Create new Supported header instance.
 *
 * @param pool	    The pool.
 *
 * @return	    New Supported header instance.
 */
PJ_DECL(pjsip_supported_hdr*) pjsip_supported_hdr_create(pj_pool_t *pool);

/**
 * Initialize a preallocated memory with the header structure. 
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_supported_hdr*) pjsip_supported_hdr_init( pj_pool_t *pool,
							void *mem );

/* **************************************************************************/
/**
 * Unsupported header.
 */
typedef pjsip_generic_array_hdr pjsip_unsupported_hdr;

/**
 * Create new Unsupported header instance.
 *
 * @param pool	    The pool.
 *
 * @return	    New Unsupported header instance.
 */
PJ_DECL(pjsip_unsupported_hdr*) pjsip_unsupported_hdr_create(pj_pool_t *pool);

/**
 * Initialize a preallocated memory with the header structure. 
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_unsupported_hdr*) pjsip_unsupported_hdr_init( pj_pool_t *pool,
							    void *mem );

/* **************************************************************************/
/**
 * Tnl-Supported header.
 */
typedef pjsip_generic_array_hdr pjsip_tnl_supported_hdr;

/**
 * Create new Tnl-Supported header instance.
 *
 * @param pool	    The pool.
 *
 * @return	    New Tnl-Supported header instance.
 */
PJ_DECL(pjsip_tnl_supported_hdr*) pjsip_tnl_supported_hdr_create(pj_pool_t *pool);

/**
 * Initialize a preallocated memory with the header structure. 
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_tnl_supported_hdr*) pjsip_tnl_supported_hdr_init( pj_pool_t *pool,
							    void *mem );

/* **************************************************************************/
/**
 * SIP Via header.
 * In this implementation, Via header can only have one element in each header.
 * If a message arrives with multiple elements in a single Via, then they will
 * be split up into multiple Via headers.
 */
typedef struct pjsip_via_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_via_hdr);
    pj_str_t	     transport;	    /**< Transport type. */
    pjsip_host_port  sent_by;	    /**< Host and optional port */
    int		     ttl_param;	    /**< TTL parameter, or -1 if it's not specified. */
    int		     rport_param;   /**< "rport" parameter, 0 to specify without
					 port number, -1 means doesn't exist. */
    pj_str_t	     maddr_param;   /**< "maddr" parameter. */
    pj_str_t	     recvd_param;   /**< "received" parameter. */
    pj_str_t	     branch_param;  /**< "branch" parameter. */
    pjsip_param	     other_param;   /**< Other parameters, concatenated as single string. */
    pj_str_t	     comment;	    /**< Comment. */
} pjsip_via_hdr;

/**
 * Create a new Via header.
 *
 * @param pool	    The pool.
 * @return	    A new "Via" header instance.
 */
PJ_DECL(pjsip_via_hdr*) pjsip_via_hdr_create( pj_pool_t *pool );

/**
 * Initialize a preallocated memory with the header structure. 
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_via_hdr*) pjsip_via_hdr_init( pj_pool_t *pool,
					    void *mem );

/* **************************************************************************/
/**
 * SIP Warning header.
 * In this version, Warning header is just a typedef for generic string 
 * header.
 */
typedef pjsip_generic_string_hdr pjsip_warning_hdr;

/**
 * Create a warning header with the specified contents.
 *
 * @param pool	    Pool to allocate memory from.
 * @param code	    Warning code, 300-399.
 * @param host	    The host portion of the Warning header.
 * @param text	    The warning text, which MUST not be quoted with
 *		    double quote.
 *
 * @return	    The Warning header field.
 */
PJ_DECL(pjsip_warning_hdr*) pjsip_warning_hdr_create( pj_pool_t *pool,
						      int code,
						      const pj_str_t *host,
						      const pj_str_t *text);

/**
 * Create a warning header and initialize the contents from the error
 * message for the specified status code. The warning code will be
 * set to 399.
 *
 * @param pool	    Pool to allocate memory from.
 * @param host	    The host portion of the Warning header.
 * @param status    The error status code, which error text will be
 *		    put in as the Warning text.
 *
 * @return	    The Warning header field.
 */
PJ_DECL(pjsip_warning_hdr*) 
pjsip_warning_hdr_create_from_status( pj_pool_t *pool,
				      const pj_str_t *host,
				      pj_status_t status);

/* **************************************************************************/

/**
 * Dean Added
 * User-Agent header.
 */
typedef struct pjsip_user_agent_hdr
{
    PJSIP_DECL_HDR_MEMBER(struct pjsip_user_agent_hdr);
    pj_str_t user_agent;	    /**< User-Agent string. */
} pjsip_user_agent_hdr;


/**
 * Dean Added
 * Create new User-Agent header.
 *
 * @param pool	The pool.
 *
 * @return	new User-Agent header.
 */
PJ_DECL(pjsip_user_agent_hdr*) pjsip_user_agent_hdr_create( pj_pool_t *pool );


/**
 * Dean Added
 * Initialize a preallocated memory with the header structure. This function
 * should only be called when application uses its own memory allocation to
 * allocate memory block for the specified header (e.g. in C++, when the 
 * header is allocated with "new" operator).
 * For normal applications, they should use pjsip_xxx_hdr_create() instead,
 * which allocates memory and initialize it in one go.
 *
 * @param pool	    Pool for additional memory allocation if required.
 * @param mem	    Pre-allocated memory to be initialized as the header.
 *
 * @return	    The header instance, which points to the same memory 
 *		    location as the mem argument.
 */
PJ_DECL(pjsip_user_agent_hdr*) pjsip_user_agent_hdr_init( pj_pool_t *pool,
					    void *mem );

/* **************************************************************************/
/** Accept-Encoding header. */
typedef pjsip_generic_string_hdr pjsip_accept_encoding_hdr;

/** Create Accept-Encoding header. */
#define pjsip_accept_encoding_hdr_create pjsip_generic_string_hdr_create

/** Accept-Language header. */
typedef pjsip_generic_string_hdr pjsip_accept_lang_hdr;

/** Create Accept-Language header. */
#define pjsip_accept_lang_hdr_create pjsip_generic_string_hdr_create

/** Alert-Info header. */
typedef pjsip_generic_string_hdr pjsip_alert_info_hdr;

/** Create Alert-Info header. */
#define pjsip_alert_info_hdr_create pjsip_generic_string_hdr_create

/** Authentication-Info header. */
typedef pjsip_generic_string_hdr pjsip_auth_info_hdr;

/** Create Authentication-Info header. */
#define pjsip_auth_info_hdr_create pjsip_generic_string_hdr_create

/** Call-Info header. */
typedef pjsip_generic_string_hdr pjsip_call_info_hdr;

/** Create Call-Info header. */
#define pjsip_call_info_hdr_create pjsip_generic_string_hdr_create

/** Content-Disposition header. */
typedef pjsip_generic_string_hdr pjsip_content_disposition_hdr;

/** Create Content-Disposition header. */
#define pjsip_content_disposition_hdr_create pjsip_generic_string_hdr_create

/** Content-Encoding header. */
typedef pjsip_generic_string_hdr pjsip_content_encoding_hdr;

/** Create Content-Encoding header. */
#define pjsip_content_encoding_hdr_create pjsip_generic_string_hdr_create

/** Content-Language header. */
typedef pjsip_generic_string_hdr pjsip_content_lang_hdr;

/** Create Content-Language header. */
#define pjsip_content_lang_hdr_create pjsip_generic_string_hdr_create

/** Date header. */
typedef pjsip_generic_string_hdr pjsip_date_hdr;

/** Create Date header. */
#define pjsip_date_hdr_create pjsip_generic_string_hdr_create

/** Error-Info header. */
typedef pjsip_generic_string_hdr pjsip_err_info_hdr;

/** Create Error-Info header. */
#define pjsip_err_info_hdr_create pjsip_generic_string_hdr_create

/** In-Reply-To header. */
typedef pjsip_generic_string_hdr pjsip_in_reply_to_hdr;

/** Create In-Reply-To header. */
#define pjsip_in_reply_to_hdr_create pjsip_generic_string_hdr_create

/** MIME-Version header. */
typedef pjsip_generic_string_hdr pjsip_mime_version_hdr;

/** Create MIME-Version header. */
#define pjsip_mime_version_hdr_create pjsip_generic_string_hdr_create

/** Organization header. */
typedef pjsip_generic_string_hdr pjsip_organization_hdr;

/** Create Organization header. */
#define pjsip_organization_hdr_create pjsip_genric_string_hdr_create

/** Priority header. */
typedef pjsip_generic_string_hdr pjsip_priority_hdr;

/** Create Priority header. */
#define pjsip_priority_hdr_create pjsip_generic_string_hdr_create

/** Proxy-Require header. */
typedef pjsip_generic_string_hdr pjsip_proxy_require_hdr;

/** Reply-To header. */
typedef pjsip_generic_string_hdr pjsip_reply_to_hdr;

/** Create Reply-To header. */
#define pjsip_reply_to_hdr_create pjsip_generic_string_hdr_create

/** Server header. */
typedef pjsip_generic_string_hdr pjsip_server_hdr;

/** Create Server header. */
#define pjsip_server_hdr_create pjsip_generic_string_hdr_create

/** Subject header. */
typedef pjsip_generic_string_hdr pjsip_subject_hdr;

/** Create Subject header. */
#define pjsip_subject_hdr_create pjsip_generic_string_hdr_create

/** Timestamp header. */
typedef pjsip_generic_string_hdr pjsip_timestamp_hdr;

/** Create Timestamp header. */
#define pjsip_timestamp_hdr_create pjsip_generic_string_hdr_create


/**
 * @}
 */

/**
 * @}  PJSIP_MSG
 */


PJ_END_DECL

#endif	/* __PJSIP_SIP_MSG_H__ */

