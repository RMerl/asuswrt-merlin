/* $Id: sip_parser.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJSIP_SIP_PARSER_H__
#define __PJSIP_SIP_PARSER_H__

/**
 * @file sip_parser.h
 * @brief SIP Message Parser
 */

#include <pjsip/sip_msg.h>
#include <pjlib-util/scanner.h>
#include <pj/list.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJSIP_PARSER Parser
 * @ingroup PJSIP_MSG
 * @brief Message and message elements parsing.
 * @{
 */

/**
 * URI Parsing options.
 */
enum
{
    /** If this option is specified, function #pjsip_parse_uri will return
     *  the URI object as pjsip_name_addr instead of the corresponding
     *  URI object.
     */
    PJSIP_PARSE_URI_AS_NAMEADDR = 1,

    /** If this option is specified, function #pjsip_parse_uri and other
     *  internal functions that this function calls will parse URI according
     *  to convention for parsing From/To/Contact header. For example, when 
     *  the URI is not enclosed in brackets ("<" and ">"), all parameters 
     *  are treated as header parameters (not URI parameters).
     */
    PJSIP_PARSE_URI_IN_FROM_TO_HDR = 2
};

/**
 * Parser syntax error exception value.
 */
extern int PJSIP_SYN_ERR_EXCEPTION[PJSUA_MAX_INSTANCES];

/**
 * This structure is used to get error reporting from parser.
 */
typedef struct pjsip_parser_err_report
{
    /** Standard header fields. */
    PJ_DECL_LIST_MEMBER(struct pjsip_parser_err_report);
    int		except_code;	/**< Error exception (e.g. PJSIP_SYN_ERR_EXCEPTION) */
    int		line;		/**< Line number. */
    int		col;		/**< Column number. */
    pj_str_t	hname;		/**< Header name, if any. */
} pjsip_parser_err_report;


/**
 * Parsing context, the default argument for parsing functions.
 */
typedef struct pjsip_parse_ctx
{
    pj_scanner      *scanner;   /**< The scanner.       */
    pj_pool_t       *pool;      /**< The pool.          */
    pjsip_rx_data   *rdata;     /**< Optional rdata.    */
} pjsip_parse_ctx;


/**
 * Type of function to parse header. The parsing function must follow these
 * specification:
 *   - It must not modify the input text.
 *   - The hname and HCOLON has been parsed prior to invoking the handler.
 *   - It returns the header instance on success.
 *   - For error reporting, it must throw PJSIP_SYN_ERR_EXCEPTION exception 
 *     instead of just returning NULL. 
 *     When exception is thrown, the return value is ignored.
 *   - It must read the header separator after finished reading the header
 *     body. The separator types are described below, and if they don't exist,
 *     exception must be thrown. Header separator can be a:
 *	- newline, such as when the header is part of a SIP message.
 *	- ampersand, such as when the header is part of an URI.
 *	- for the last header, these separator is optional since parsing
 *        can be terminated when seeing EOF.
 */
typedef pjsip_hdr* (pjsip_parse_hdr_func)(pjsip_parse_ctx *context);

/**
 * Type of function to parse URI scheme.
 * Most of the specification of header parser handler (pjsip_parse_hdr_func)
 * also applies here (except the separator part).
 */
typedef void* (pjsip_parse_uri_func)(pj_scanner *scanner, pj_pool_t *pool,
				     pj_bool_t parse_params);

/**
 * Register header parser handler. The parser handler MUST follow the 
 * specification of header parser handler function. New registration 
 * overwrites previous registration with the same name.
 *
 * @param inst_id   The instance id of pjsua.
 * @param hname		The header name.
 * @param hshortname	The short header name or NULL.
 * @param fptr		The pointer to function to parser the header.
 *
 * @return		PJ_SUCCESS if success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjsip_register_hdr_parser( int inst_id,
						const char *hname,
						const char *hshortname,
						pjsip_parse_hdr_func *fptr);

/**
 * Unregister previously registered header parser handler.
 * All the arguments MUST exactly equal to the value specified upon 
 * registration of the handler.
 *
 * @param hname		The header name registered.
 * @param hshortname	The short header name registered, or NULL.
 * @param fptr		Previously registered function to parse the header.
 *
 * @return		zero if unregistration was successfull.
 */
PJ_DECL(pj_status_t) pjsip_unregister_hdr_parser( const char *hname,
						  const char *hshortname,
						  pjsip_parse_hdr_func *fptr);

/**
 * Register URI scheme parser handler.
 *
 * @param inst_id   The instance id of pjsua.
 * @param scheme	The URI scheme registered.
 * @param func		The URI parser function.
 *
 * @return		zero on success.
 */
PJ_DECL(pj_status_t) pjsip_register_uri_parser( int inst_id,
							char *scheme,
					        pjsip_parse_uri_func *func);

/**
 * Unregister URI scheme parser handler.
 * All the arguments MUST exactly equal to the value specified upon 
 * registration of the handler.
 *
 * @param scheme	The URI scheme as registered previously.
 * @param func		The function handler as registered previously.
 *
 * @return		zero if the registration was successfull.
 */
PJ_DECL(pj_status_t) pjsip_unregister_uri_parser( const char *scheme,
						  pjsip_parse_uri_func *func);

/**
 * Parse an URI in the input and return the correct instance of URI.
 *
 * @param inst_id   The instance id of pjsua.
 * @param pool		The pool to get memory allocations.
 * @param buf		The input buffer, which MUST be NULL terminated.
 * @param size		The length of the string (not counting NULL terminator).
 * @param options	If no options are given (value is zero), the object 
 *			returned is dependent on the syntax of the URI, 
 *			eg. basic SIP URL, TEL URL, or name address. 
 *			If option PJSIP_PARSE_URI_AS_NAMEADDR is given,
 *			then the returned object is always name address object,
 *			with the relevant URI object contained in the name 
 *			address object.
 * @return		The URI or NULL when failed. No exception is thrown by 
 *			this function (or any public parser functions).
 */
PJ_DECL(pjsip_uri*) pjsip_parse_uri( int inst_id,
					 pj_pool_t *pool, 
				     char *buf, pj_size_t size,
				     unsigned options);

/**
 * Parse SIP status line.
 *
 * @param inst_id   The instance id of pjsua.
 * @param buf		Text buffer to parse, which MUST be NULL terminated.
 * @param size		The size of the buffer, excluding the NULL character.
 * @param status_line	Structure to receive the parsed elements.
 *
 * @return		PJ_SUCCESS if a status line is parsed successfully.
 */
PJ_DECL(pj_status_t) pjsip_parse_status_line(int inst_id,
						 char *buf, pj_size_t size,
					     pjsip_status_line *status_line);


/**
 * Parse a packet buffer and build a full SIP message from the packet. This
 * function parses all parts of the message, including request/status line,
 * all headers, and the message body. The message body however is only 
 * treated as a text block, ie. the function will not try to parse the content
 * of the body.
 *
 * @param inst_id   The instance id of pjusa.
 * @param pool		The pool to allocate memory.
 * @param buf		The input buffer, which MUST be NULL terminated.
 * @param size		The length of the string (not counting NULL terminator).
 * @param err_list	If this parameter is not NULL, then the parser will
 *			put error messages during parsing in this list.
 *
 * @return		The message or NULL when failed. No exception is thrown
 *			by this function (or any public parser functions).
 */
PJ_DECL(pjsip_msg *) pjsip_parse_msg( int inst_id,
					  pj_pool_t *pool, 
				      char *buf, pj_size_t size,
				      pjsip_parser_err_report *err_list);


/**
 * Parse a packet buffer and build a rdata. The resulting message will be
 * stored in \c msg field in the \c rdata. This behaves pretty much like
 * #pjsip_parse_msg(), except that it will also initialize the header fields
 * in the \c rdata.
 *
 * This function is normally called by the transport layer.
 *
 * @param inst_id   The instance id of pjusa.
 * @param buf		The input buffer, which MUST be NULL terminated.
 * @param size		The length of the string (not counting NULL terminator).
 * @param rdata         The receive data buffer to store the message and
 *                      its elements.
 *
 * @return              The message inside the rdata if successfull, or NULL.
 */
PJ_DECL(pjsip_msg *) pjsip_parse_rdata( int inst_id,
									   char *buf, pj_size_t size,
                                       pjsip_rx_data *rdata );

/**
 * Check incoming packet to see if a (probably) valid SIP message has been 
 * received.
 *
 * @param inst_id   The instance id of pjsua.
 * @param buf		The input buffer, which must be NULL terminated.
 * @param size		The buffer size.
 * @param is_datagram	Put non-zero if transport is datagram oriented.
 * @param msg_size	[out] If message is valid, this parameter will contain
 *			the size of the SIP message (including body, if any).
 *
 * @return		PJ_SUCCESS if a message is found, or an error code.
 */
PJ_DECL(pj_status_t) pjsip_find_msg(int inst_id,
									const char *buf, 
                                    pj_size_t size, 
				    pj_bool_t is_datagram, 
                                    pj_size_t *msg_size);

/**
 * Parse the content of a header and return the header instance.
 * This function parses the content of a header (ie. part after colon) according
 * to the expected name, and will return the correct instance of header.
 *
 * @param inst_id   The instance id of pjusa.
 * @param pool		Pool to allocate memory for the header.
 * @param hname		Header name which is used to find the correct function
 *			to parse the header.
 * @param line		Header content, which must be NULL terminated.
 * @param size		The length of the string (not counting NULL terminator,
 *			if any).
 * @param parsed_len	If the value is not NULL, then upon return the function
 *			will fill the pointer with the length of the string
 *			that has been parsed. This is usefull for two purposes,
 *			one is when the string may contain more than one header
 *			lines, and two when an error happen the value can
 *			pinpoint the location of the error in the buffer.
 *
 * @return		The instance of the header if parsing was successful,
 *			or otherwise a NULL pointer will be returned.
 */
PJ_DECL(void*) pjsip_parse_hdr( int inst_id,
				pj_pool_t *pool, const pj_str_t *hname,
				char *line, pj_size_t size,
				int *parsed_len);

/**
 * Parse header line(s). Multiple headers can be parsed by this function.
 * When there are multiple headers, the headers MUST be separated by either
 * a newline (as in SIP message) or ampersand mark (as in URI). This separator
 * is optional for the last header.
 *
 * @param inst_id   The instance id of pjusa.
 * @param pool		The pool.
 * @param input		The input text to parse, which must be NULL terminated.
 * @param size		The text length.
 * @param hlist		The header list to store the parsed headers.
 *			This list must have been initialized before calling 
 *			this function.
 * @param options	Specify 1 here to make parsing stop when error is
 * 			encountered when parsing the header. Otherwise the
 * 			error is silently ignored and parsing resumes to the
 * 			next line.
 * @return		zero if successfull, or -1 if error is encountered. 
 *			Upon error, the \a hlist argument MAY contain 
 *			successfully parsed headers.
 */
PJ_DECL(pj_status_t) pjsip_parse_headers( int inst_id, pj_pool_t *pool, char *input,
				          pj_size_t size, pjsip_hdr *hlist,
				          unsigned options);


/**
 * @}
 */


#ifdef _MSC_VER
#   pragma warning(push)
#   pragma warning(disable:4510) // default constructor could not be generated
#   pragma warning(disable:4512) // assignment operator could not be generated
#   pragma warning(disable:4610) // user defined constructor required
#endif

/**
 * Parser constants. @see pjsip_parser_const()
 */
typedef struct pjsip_parser_const_t
{
    const pj_str_t pjsip_USER_STR;	/**< "user" string constant.    */
    const pj_str_t pjsip_METHOD_STR;	/**< "method" string constant   */
    const pj_str_t pjsip_TRANSPORT_STR;	/**< "transport" string const.  */
    const pj_str_t pjsip_MADDR_STR;	/**< "maddr" string const.	*/
    const pj_str_t pjsip_LR_STR;	/**< "lr" string const.		*/
    const pj_str_t pjsip_SIP_STR;	/**< "sip" string constant.	*/
    const pj_str_t pjsip_SIPS_STR;	/**< "sips" string constant.    */
    const pj_str_t pjsip_TEL_STR;	/**< "tel" string constant.	*/
    const pj_str_t pjsip_BRANCH_STR;	/**< "branch" string constant.  */
    const pj_str_t pjsip_TTL_STR;	/**< "ttl" string constant.	*/
    const pj_str_t pjsip_RECEIVED_STR;	/**< "received" string const.   */
    const pj_str_t pjsip_Q_STR;		/**< "q" string constant.	*/
    const pj_str_t pjsip_EXPIRES_STR;	/**< "expires" string constant. */
	const pj_str_t pjsip_TAG_STR;	/**< "tag" string constant.	*/
	const pj_str_t pjsip_RPORT_STR;	/**< "rport" string const.	*/
	const pj_str_t pjsip_TIMEOUT_STR;	/**< "timeout" string const.	*/

    pj_cis_t pjsip_HOST_SPEC;		/**< For scanning host part.	*/
    pj_cis_t pjsip_DIGIT_SPEC;		/**< Decimal digits		*/
    pj_cis_t pjsip_ALPHA_SPEC;		/**< Alpha (A-Z, a-z)		*/
    pj_cis_t pjsip_ALNUM_SPEC;		/**< Decimal + Alpha.		*/
    pj_cis_t pjsip_TOKEN_SPEC;		/**< Token.			*/
    pj_cis_t pjsip_TOKEN_SPEC_ESC;	/**< Token without '%' character */
    pj_cis_t pjsip_VIA_PARAM_SPEC;	/**< Via param is token + ":" for
					     IPv6.			*/
    pj_cis_t pjsip_VIA_PARAM_SPEC_ESC;	/**< .. as above without '%'	*/
    pj_cis_t pjsip_HEX_SPEC;  		/**< Hexadecimal digits.	*/
    pj_cis_t pjsip_PARAM_CHAR_SPEC;	/**< For scanning pname (or pvalue
					     when it's  not quoted.) in URI */
    pj_cis_t pjsip_PARAM_CHAR_SPEC_ESC;	/**< Variant without the escape ('%')
					     char			*/
    pj_cis_t pjsip_HDR_CHAR_SPEC;	/**< Chars in hname/havalue in URL. */
    pj_cis_t pjsip_HDR_CHAR_SPEC_ESC;	/**< Variant without the escape ('%')
					     char			*/
    pj_cis_t pjsip_PROBE_USER_HOST_SPEC;/**< Hostname characters.	*/
    pj_cis_t pjsip_PASSWD_SPEC;		/**< Password.			*/
    pj_cis_t pjsip_PASSWD_SPEC_ESC;	/**< Variant without the escape ('%')
					     char			*/
    pj_cis_t pjsip_USER_SPEC;		/**< User */
    pj_cis_t pjsip_USER_SPEC_ESC;	/**< Variant without the escape ('%')
					     char			*/
    pj_cis_t pjsip_USER_SPEC_LENIENT;	/**< User, with additional '#' char */
    pj_cis_t pjsip_USER_SPEC_LENIENT_ESC;/**< pjsip_USER_SPEC_ESC with '#' */
    pj_cis_t pjsip_NOT_NEWLINE;		/**< For eating up header, basically
					     any chars except newlines or 
					     zero.			*/
    pj_cis_t pjsip_NOT_COMMA_OR_NEWLINE;/**< Array elements.		*/
    pj_cis_t pjsip_DISPLAY_SPEC;	/**< Used when searching for display
					     name.			*/
    pj_cis_t pjsip_OTHER_URI_CONTENT;	/**< Generic URI content.	*/

} pjsip_parser_const_t;

#ifdef _MSC_VER
#   pragma warning(pop)
#endif


/**
 * Get parser constants.
 */
PJ_DECL(const pjsip_parser_const_t*) pjsip_parser_const(void);


/*
 * Parser utilities.
 */
enum
{
    PJSIP_PARSE_REMOVE_QUOTE = 1
};

/* Parse parameter in header (matching the character as token) */
PJ_DECL(void) pjsip_parse_param_imp(pj_scanner *scanner, pj_pool_t *pool,
			     	    pj_str_t *pname, pj_str_t *pvalue,
			     	    unsigned opt);
/* Parse parameter in URL (matching the character as paramchar) */
PJ_DECL(void) pjsip_parse_uri_param_imp(pj_scanner *scanner, pj_pool_t *pool,
				 	pj_str_t *pname, pj_str_t *pvalue,
				 	unsigned opt);
PJ_DECL(void) pjsip_concat_param_imp(pj_str_t *param, pj_pool_t *pool, 
			     	     const pj_str_t *pname, 
				     const pj_str_t *pvalue, 
			     	     int sepchar);
PJ_DECL(void) pjsip_parse_end_hdr_imp ( pj_scanner *scanner );

PJ_END_DECL

#endif	/* __PJSIP_SIP_PARSER_H__ */

