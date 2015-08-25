/* $Id: http_client.h 3810 2011-10-11 04:37:37Z bennylp $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJLIB_UTIL_HTTP_CLIENT_H__
#define __PJLIB_UTIL_HTTP_CLIENT_H__

/**
 * @file http_client.h
 * @brief Simple HTTP Client
 */
#include <pj/activesock.h>
#include <pjlib-util/types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_HTTP_CLIENT Simple HTTP Client
 * @ingroup PJ_PROTOCOLS
 * @{
 * This contains a simple HTTP client implementation.
 * Some known limitations: 
 * - Does not support chunked Transfer-Encoding.
 */

/**
 * This opaque structure describes the http request.
 */
typedef struct pj_http_req pj_http_req;

/**
 * Defines the maximum number of elements in a pj_http_headers
 * structure.
 */
#define PJ_HTTP_HEADER_SIZE 32

/**
 * HTTP header representation.
 */
typedef struct pj_http_header_elmt
{
    pj_str_t name;	/**< Header name */
    pj_str_t value;	/**< Header value */
} pj_http_header_elmt;

/**
 * This structure describes http request/response headers.
 * Application should call #pj_http_headers_add_elmt() to
 * add a header field.
 */
typedef struct pj_http_headers
{
    /**< Number of header fields */
    unsigned     count;

    /** Header elements/fields */
    pj_http_header_elmt header[PJ_HTTP_HEADER_SIZE];
} pj_http_headers;

/**
 * Structure to save HTTP authentication credential.
 */
typedef struct pj_http_auth_cred
{
    /**
     * Specify specific authentication schemes to be responded. Valid values
     * are "basic" and "digest". If this field is not set, any authentication
     * schemes will be responded.
     *
     * Default is empty.
     */
    pj_str_t	scheme;

    /**
     * Specify specific authentication realm to be responded. If this field
     * is set, only 401/407 response with matching realm will be responded.
     * If this field is not set, any realms will be responded.
     *
     * Default is empty.
     */
    pj_str_t	realm;

    /**
     * Specify authentication username.
     *
     * Default is empty.
     */
    pj_str_t	username;

    /**
     * The type of password in \a data field. Currently only 0 is
     * supported, meaning the \a data contains plain-text password.
     *
     * Default is 0.
     */
    unsigned	data_type;

    /**
     * Specify authentication password. The encoding of the password depends
     * on the value of \a data_type field above.
     *
     * Default is empty.
     */
    pj_str_t	data;

} pj_http_auth_cred;


/**
 * Parameters that can be given during http request creation. Application
 * must initialize this structure with #pj_http_req_param_default().
 */
typedef struct pj_http_req_param 
{
    /** 
     * The address family of the URL.
     *  Default is pj_AF_INET().
     */
    int             addr_family;

    /** 
     * The HTTP request method.
     * Default is GET.
     */
    pj_str_t        method;

    /** 
     * The HTTP protocol version ("1.0" or "1.1").
     * Default is "1.0".
     */
    pj_str_t        version;

    /** 
     * HTTP request operation timeout.
     * Default is PJ_HTTP_DEFAULT_TIMEOUT.
     */
    pj_time_val     timeout;

    /** 
     * User-defined data.
     * Default is NULL.
     */
    void            *user_data;

    /** 
     * HTTP request headers.
     * Default is empty.
     */
    pj_http_headers headers;

    /**
      * This structure describes the http request body. If application
      * specifies the data to send, the data must remain valid until 
      * the HTTP request is sent. Alternatively, application can choose
      * to specify total_size as the total data size to send instead
      * while leaving the data NULL (and its size 0). In this case,
      * HTTP request will then call on_send_data() callback once it is 
      * ready to send the request body. This will be useful if 
      * application does not wish to load the data into the buffer at 
      * once.
      * 
      * Default is empty.
      */
    struct pj_http_reqdata
    {
        void       *data;          /**< Request body data */
        pj_size_t  size;           /**< Request body size */
        pj_size_t  total_size;     /**< If total_size > 0, data */
                                   /**< will be provided later  */
    } reqdata;

    /**
     * Authentication credential needed to respond to 401/407 response.
     */
    pj_http_auth_cred	auth_cred;

    /**
     * Optional source port range to use when binding the socket.
     * This can be used if the source port needs to be within a certain range
     * for instance due to strict firewall settings. The port used will be
     * randomized within the range.
     *
     * Note that if authentication is configured, the authentication response
     * will be a new transaction
     *
     * Default is 0 (The OS will select the source port automatically)
     */
    pj_uint16_t		source_port_range_start;

    /**
     * Optional source port range to use when binding.
     * The size of the port restriction range
     *
     * Default is 0 (The OS will select the source port automatically))
     */
    pj_uint16_t		source_port_range_size;

    /**
     * Max number of retries if binding to a port fails.
     * Note that this does not adress the scenario where a request times out
     * or errors. This needs to be taken care of by the on_complete callback.
     *
     * Default is 3
     */
    pj_uint16_t		max_retries;

} pj_http_req_param;

/**
 * HTTP authentication challenge, parsed from WWW-Authenticate header.
 */
typedef struct pj_http_auth_chal
{
    pj_str_t	scheme;		/**< Auth scheme.		*/
    pj_str_t	realm;		/**< Realm for the challenge.	*/
    pj_str_t	domain;		/**< Domain.			*/
    pj_str_t	nonce;		/**< Nonce challenge.		*/
    pj_str_t	opaque;		/**< Opaque value.		*/
    int		stale;		/**< Stale parameter.		*/
    pj_str_t	algorithm;	/**< Algorithm parameter.	*/
    pj_str_t	qop;		/**< Quality of protection.	*/
} pj_http_auth_chal;

/**
 * This structure describes HTTP response.
 */
typedef struct pj_http_resp
{
    pj_str_t        version;        /**< HTTP version of the server */
    pj_uint16_t     status_code;    /**< Status code of the request */
    pj_str_t        reason;         /**< Reason phrase */
    pj_http_headers headers;        /**< Response headers */
    pj_http_auth_chal auth_chal;    /**< Parsed WWW-Authenticate header, if
				         any. */
    pj_int32_t      content_length; /**< The value of content-length header
					 field. -1 if not specified. */
    void            *data;          /**< Data received */
    pj_size_t       size;           /**< Data size */
} pj_http_resp;

/**
 * This structure describes HTTP URL.
 */
typedef struct pj_http_url
{
    pj_str_t	username;	    /**< Username part */
    pj_str_t	passwd;		    /**< Password part */
    pj_str_t    protocol;           /**< Protocol used */
    pj_str_t    host;               /**< Host name */
    pj_uint16_t port;               /**< Port number */
    pj_str_t    path;               /**< Path */
} pj_http_url;

/**
 * This structure describes the callbacks to be called by the HTTP request.
 */
typedef struct pj_http_req_callback
{
    /**
     * This callback is called when a complete HTTP response header 
     * is received.
     *
     * @param http_req	The http request.
     * @param resp	The response of the request.
     */
    void (*on_response)(pj_http_req *http_req, const pj_http_resp *resp);

    /**
     * This callback is called when the HTTP request is ready to send
     * its request body. Application may wish to use this callback if
     * it wishes to load the data at a later time or if it does not 
     * wish to load the whole data into memory. In order for this
     * callback to be called, application MUST set http_req_param.total_size
     * to a value greater than 0.
     *
     * @param http_req	The http request.
     * @param data	Pointer to the data that will be sent. Application
     *                  must set the pointer to the current data chunk/segment
     *                  to be sent. Data must remain valid until the next 
     *                  on_send_data() callback or for the last segment,
     *                  until it is sent.
     * @param size	Pointer to the data size that will be sent.
     */
    void (*on_send_data)(pj_http_req *http_req,
                         void **data, pj_size_t *size);

    /**
     * This callback is called when a segment of response body data
     * arrives. If this callback is specified (i.e. not NULL), the
     * on_complete() callback will be called with zero-length data
     * (within the response parameter), hence the application must 
     * store and manage its own data buffer, otherwise the 
     * on_complete() callback will be called with the response 
     * parameter containing the complete data. 
     * 
     * @param http_req	The http request.
     * @param data	The buffer containing the data.
     * @param size	The length of data in the buffer.
     */
    void (*on_data_read)(pj_http_req *http_req,
                         void *data, pj_size_t size);

    /**
     * This callback is called when the HTTP request is completed.
     * If the callback on_data_read() is specified, the variable
     * response->data will be set to NULL, otherwise it will
     * contain the complete data. Response data is allocated from
     * pj_http_req's internal memory pool so the data remain valid
     * as long as pj_http_req is not destroyed and application does
     * not start a new request.
     *
     * If no longer required, application may choose to destroy 
     * pj_http_req immediately by calling #pj_http_req_destroy() inside 
     * the callback.
     *
     * @param http_req	The http request.
     * @param status	The status of the request operation. PJ_SUCCESS
     *                  if the operation completed successfully
     *                  (connection-wise). To check the server's 
     *                  status-code response to the HTTP request, 
     *                  application should check resp->status_code instead.
     * @param resp	The response of the corresponding request. If 
     *			the status argument is non-PJ_SUCCESS, this 
     *			argument will be set to NULL.
     */
    void (*on_complete)(pj_http_req *http_req,
                        pj_status_t status,
                        const pj_http_resp *resp);

} pj_http_req_callback;


/**
 * Initialize the http request parameters with the default values.
 *
 * @param param		The parameter to be initialized.
 */
PJ_DECL(void) pj_http_req_param_default(pj_http_req_param *param);

/**
 * Add a header element/field. Application MUST make sure that 
 * name and val pointer remains valid until the HTTP request is sent.
 *
 * @param headers	The headers.
 * @param name	        The header field name.
 * @param value	        The header field value.
 *
 * @return	        PJ_SUCCESS if the operation has been successful,
 *		        or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_http_headers_add_elmt(pj_http_headers *headers, 
                                              pj_str_t *name, 
                                              pj_str_t *val);

/** 
 * The same as #pj_http_headers_add_elmt() with char * as
 * its parameters. Application MUST make sure that name and val pointer 
 * remains valid until the HTTP request is sent.
 *
 * @param headers	The headers.
 * @param name	        The header field name.
 * @param value	        The header field value.
 *
 * @return	        PJ_SUCCESS if the operation has been successful,
 *		        or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_http_headers_add_elmt2(pj_http_headers *headers, 
                                               char *name, char *val);

/**
 * Parse a http URL into its components.
 *
 * @param inst_id       The intance id of pjusa.
 * @param url	        The URL to be parsed.
 * @param hurl	        Pointer to receive the parsed result.
 *
 * @return	        PJ_SUCCESS if the operation has been successful,
 *		        or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_http_req_parse_url(int inst_id, 
										   const pj_str_t *url, 
                                           pj_http_url *hurl);

/**
 * Create the HTTP request.
 *
 * @param inst_id   The instance id of pjsua.
 * @param pool		Pool to use. HTTP request will use the pool's factory
 *                      to allocate its own memory pool.
 * @param url		HTTP URL request.
 * @param timer	        The timer to use.
 * @param ioqueue	The ioqueue to use.
 * @param param		Optional parameters. When this parameter is not 
 *                      specifed (NULL), the default values will be used.
 * @param hcb		Pointer to structure containing application
 *			callbacks.
 * @param http_req	Pointer to receive the http request instance.
 *
 * @return		PJ_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
PJ_DECL(pj_status_t) pj_http_req_create(int inst_id,
										pj_pool_t *pool,
                                        const pj_str_t *url,
					pj_timer_heap_t *timer,
					pj_ioqueue_t *ioqueue,
                                        const pj_http_req_param *param,
                                        const pj_http_req_callback *hcb,
                                        pj_http_req **http_req);

/**
 * Set the timeout of the HTTP request operation. Note that if the 
 * HTTP request is currently running, the timeout will only affect 
 * subsequent request operations.
 *
 * @param http_req  The http request.
 * @param timeout   Timeout value for HTTP request operation.
 */
PJ_DECL(void) pj_http_req_set_timeout(pj_http_req *http_req,
                                      const pj_time_val* timeout);

/**
 * Starts an asynchronous HTTP request to the URL specified.
 *
 * @param http_req  The http request.
 *
 * @return
 *  - PJ_SUCCESS    if success
 *  - non-zero      which indicates the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_http_req_start(pj_http_req *http_req);

/**
 * Cancel the asynchronous HTTP request. 
 *
 * @param http_req  The http request.
 * @param notify    If non-zero, the on_complete() callback will be 
 *                  called with status PJ_ECANCELLED to notify that 
 *                  the query has been cancelled.
 *
 * @return
 *  - PJ_SUCCESS    if success
 *  - non-zero      which indicates the appropriate error code.
 */
PJ_DECL(pj_status_t) pj_http_req_cancel(pj_http_req *http_req,
                                        pj_bool_t notify);

/**
 * Destroy the http request.
 *
 * @param http_req	The http request to be destroyed.
 *
 * @return              PJ_SUCCESS if success.
 */
PJ_DECL(pj_status_t) pj_http_req_destroy(pj_http_req *http_req);

/**
 * Find out whether the http request is running.
 *
 * @param http_req      The http request.
 *
 * @return	        PJ_TRUE if a request is pending, or
 *		        PJ_FALSE if idle
 */
PJ_DECL(pj_bool_t) pj_http_req_is_running(const pj_http_req *http_req);

/**
 * Retrieve the user data previously associated with this http
 * request.
 *
 * @param http_req  The http request.
 *
 * @return	    The user data.
 */
PJ_DECL(void *) pj_http_req_get_user_data(pj_http_req *http_req);

/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJLIB_UTIL_HTTP_CLIENT_H__ */
