/* $Id: http_client.c 3811 2011-10-11 04:40:50Z bennylp $ */
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

#include <pjlib-util/http_client.h>
#include <pj/activesock.h>
#include <pj/assert.h>
#include <pj/ctype.h>
#include <pj/errno.h>
#include <pj/except.h>
#include <pj/pool.h>
#include <pj/string.h>
#include <pj/timer.h>
#include <pj/rand.h>
#include <pjlib-util/base64.h>
#include <pjlib-util/errno.h>
#include <pjlib-util/md5.h>
#include <pjlib-util/scanner.h>
#include <pjlib-util/string.h>

#define THIS_FILE   "http_client.c"

#if 0
    /* Enable some tracing */
    #define TRACE_(arg)	PJ_LOG(3,arg)
#else
    #define TRACE_(arg)
#endif

#define NUM_PROTOCOL            2
#define HTTP_1_0                "1.0"
#define HTTP_1_1                "1.1"
#define CONTENT_LENGTH          "Content-Length"
/* Buffer size for sending/receiving messages. */
#define BUF_SIZE                2048
/* Initial data buffer size to store the data in case content-
 * length is not specified in the server's response.
 */
#define INITIAL_DATA_BUF_SIZE   2048
#define INITIAL_POOL_SIZE       1024
#define POOL_INCREMENT_SIZE     512

enum http_protocol
{
    PROTOCOL_HTTP,
    PROTOCOL_HTTPS
};

static const char *http_protocol_names[NUM_PROTOCOL] =
{
    "HTTP",
    "HTTPS"
};

static const unsigned int http_default_port[NUM_PROTOCOL] =
{
    80,
    443
};

enum http_method
{
    HTTP_GET,
    HTTP_PUT,
    HTTP_DELETE
};

static const char *http_method_names[3] =
{
    "GET",
    "PUT",
    "DELETE"
};

enum http_state
{
    IDLE,
    CONNECTING,
    SENDING_REQUEST,
    SENDING_REQUEST_BODY,
    REQUEST_SENT,
    READING_RESPONSE,
    READING_DATA,
    READING_COMPLETE,
    ABORTING,
};

enum auth_state
{
    AUTH_NONE,		/* Not authenticating */
    AUTH_RETRYING,	/* New request with auth has been submitted */
    AUTH_DONE		/* Done retrying the request with auth. */
};

struct pj_http_req
{
	int inst_id;
    pj_str_t                url;        /* Request URL */
    pj_http_url             hurl;       /* Parsed request URL */
    pj_sockaddr             addr;       /* The host's socket address */
    pj_http_req_param       param;      /* HTTP request parameters */
    pj_pool_t               *pool;      /* Pool to allocate memory from */
    pj_timer_heap_t         *timer;     /* Timer for timeout management */
    pj_ioqueue_t            *ioqueue;   /* Ioqueue to use */
    pj_http_req_callback    cb;         /* Callbacks */
    pj_activesock_t         *asock;     /* Active socket */
    pj_status_t             error;      /* Error status */
    pj_str_t                buffer;     /* Buffer to send/receive msgs */
    enum http_state         state;      /* State of the HTTP request */
    enum auth_state	    auth_state; /* Authentication state */
    pj_timer_entry          timer_entry;/* Timer entry */
    pj_bool_t               resolved;   /* Whether URL's host is resolved */
    pj_http_resp            response;   /* HTTP response */
    pj_ioqueue_op_key_t	    op_key;
    struct tcp_state
    {
        /* Total data sent so far if the data is sent in segments (i.e.
         * if on_send_data() is not NULL and if param.reqdata.total_size > 0)
         */
        pj_size_t tot_chunk_size;
        /* Size of data to be sent (in a single activesock operation).*/
        pj_size_t send_size;
        /* Data size sent so far. */
        pj_size_t current_send_size;
        /* Total data received so far. */
        pj_size_t current_read_size;
    } tcp_state;
};

/* Start sending the request */
static pj_status_t http_req_start_sending(pj_http_req *hreq);
/* Start reading the response */
static pj_status_t http_req_start_reading(pj_http_req *hreq);
/* End the request */
static pj_status_t http_req_end_request(pj_http_req *hreq);
/* Parse the header data and populate the header fields with the result. */
static pj_status_t http_headers_parse(int inst_id,
									  char *hdata, pj_size_t size, 
                                      pj_http_headers *headers);
/* Parse the response */
static pj_status_t http_response_parse(int inst_id,
									   pj_pool_t *pool,
                                       pj_http_resp *response,
                                       void *data, pj_size_t size,
                                       pj_size_t *remainder);
/* Restart the request with authentication */
static void restart_req_with_auth(pj_http_req *hreq);
/* Parse authentication challenge */
static pj_status_t parse_auth_chal(int inst_id, pj_pool_t *pool, pj_str_t *input,
				   pj_http_auth_chal *chal);

static pj_uint16_t get_http_default_port(const pj_str_t *protocol)
{
    int i;

    for (i = 0; i < NUM_PROTOCOL; i++) {
        if (!pj_stricmp2(protocol, http_protocol_names[i])) {
            return (pj_uint16_t)http_default_port[i];
        }
    }
    return 0;
}

static const char * get_protocol(const pj_str_t *protocol)
{
    int i;

    for (i = 0; i < NUM_PROTOCOL; i++) {
        if (!pj_stricmp2(protocol, http_protocol_names[i])) {
            return http_protocol_names[i];
        }
    }

    /* Should not happen */
    pj_assert(0);
    return NULL;
}


/* Syntax error handler for parser. */
static void on_syntax_error(pj_scanner *scanner)
{
    PJ_THROW(scanner->inst_id, PJ_EINVAL);  // syntax error
}

/* Callback when connection is established to the server */
static pj_bool_t http_on_connect(pj_activesock_t *asock,
				 pj_status_t status)
{
    pj_http_req *hreq = (pj_http_req*) pj_activesock_get_user_data(asock);

    if (hreq->state == ABORTING || hreq->state == IDLE)
        return PJ_FALSE;

    if (status != PJ_SUCCESS) {
        hreq->error = status;
        pj_http_req_cancel(hreq, PJ_TRUE);
        return PJ_FALSE;
    }

    /* OK, we are connected. Start sending the request */
    hreq->state = SENDING_REQUEST;
    http_req_start_sending(hreq);
    return PJ_TRUE;
}

static pj_bool_t http_on_data_sent(pj_activesock_t *asock,
 				   pj_ioqueue_op_key_t *op_key,
				   pj_ssize_t sent)
{
    pj_http_req *hreq = (pj_http_req*) pj_activesock_get_user_data(asock);

    PJ_UNUSED_ARG(op_key);

    if (hreq->state == ABORTING || hreq->state == IDLE)
        return PJ_FALSE;

    if (sent <= 0) {
        hreq->error = (sent < 0 ? -sent : PJLIB_UTIL_EHTTPLOST);
        pj_http_req_cancel(hreq, PJ_TRUE);
        return PJ_FALSE;
    } 

    hreq->tcp_state.current_send_size += sent;
    TRACE_((THIS_FILE, "\nData sent: %d out of %d bytes", 
           hreq->tcp_state.current_send_size, hreq->tcp_state.send_size));
    if (hreq->tcp_state.current_send_size == hreq->tcp_state.send_size) {
        /* Find out whether there is a request body to send. */
        if (hreq->param.reqdata.total_size > 0 || 
            hreq->param.reqdata.size > 0) 
        {
            if (hreq->state == SENDING_REQUEST) {
                /* Start sending the request body */
                hreq->state = SENDING_REQUEST_BODY;
                hreq->tcp_state.tot_chunk_size = 0;
                pj_assert(hreq->param.reqdata.total_size == 0 ||
                          (hreq->param.reqdata.total_size > 0 && 
                          hreq->param.reqdata.size == 0));
            } else {
                /* Continue sending the next chunk of the request body */
                hreq->tcp_state.tot_chunk_size += hreq->tcp_state.send_size;
                if (hreq->tcp_state.tot_chunk_size == 
                    hreq->param.reqdata.total_size ||
                    hreq->param.reqdata.total_size == 0) 
                {
                    /* Finish sending all the chunks, start reading 
                     * the response.
                     */
                    hreq->state = REQUEST_SENT;
                    http_req_start_reading(hreq);
                    return PJ_TRUE;
                }
            }
            if (hreq->param.reqdata.total_size > 0 &&
                hreq->cb.on_send_data) 
            {
                /* Call the callback for the application to provide
                 * the next chunk of data to be sent.
                 */
                (*hreq->cb.on_send_data)(hreq, &hreq->param.reqdata.data, 
                                         &hreq->param.reqdata.size);
                /* Make sure the total data size given by the user does not
                 * exceed what the user originally said.
                 */
                pj_assert(hreq->tcp_state.tot_chunk_size + 
                          hreq->param.reqdata.size <=
                          hreq->param.reqdata.total_size);
            }
            http_req_start_sending(hreq);
        } else {
            /* No request body, proceed to reading the server's response. */
            hreq->state = REQUEST_SENT;
            http_req_start_reading(hreq);
        }
    }
    return PJ_TRUE;
}

static pj_bool_t http_on_data_read(pj_activesock_t *asock,
				  void *data,
				  pj_size_t size,
				  pj_status_t status,
				  pj_size_t *remainder)
{
    pj_http_req *hreq = (pj_http_req*) pj_activesock_get_user_data(asock);

    TRACE_((THIS_FILE, "\nData received: %d bytes", size));

    if (hreq->state == ABORTING || hreq->state == IDLE)
        return PJ_FALSE;

    if (hreq->state == READING_RESPONSE) {
        pj_status_t st;
        pj_size_t rem;

        if (status != PJ_SUCCESS && status != PJ_EPENDING) {
            hreq->error = status;
            pj_http_req_cancel(hreq, PJ_TRUE);
            return PJ_FALSE;
        }

        /* Parse the response. */
        st = http_response_parse(hreq->inst_id, hreq->pool, &hreq->response,
                                 data, size, &rem);
        if (st == PJLIB_UTIL_EHTTPINCHDR) {
            /* If we already use up all our buffer and still
             * hasn't received the whole header, return error
             */
            if (size == BUF_SIZE) {
                hreq->error = PJ_ETOOBIG; // response header size is too big
                pj_http_req_cancel(hreq, PJ_TRUE);
                return PJ_FALSE;
            }
            /* Keep the data if we do not get the whole response header */
            *remainder = size;
        } else {
            hreq->state = READING_DATA;
            if (st != PJ_SUCCESS) {
                /* Server replied with an invalid (or unknown) response 
                 * format. We'll just pass the whole (unparsed) response 
                 * to the user.
                 */
                hreq->response.data = data;
                hreq->response.size = size - rem;
            }

            /* If code is 401 or 407, find and parse WWW-Authenticate or
             * Proxy-Authenticate header
             */
            if (hreq->response.status_code == 401 ||
        	hreq->response.status_code == 407)
            {
        	const pj_str_t STR_WWW_AUTH = { "WWW-Authenticate", 16 };
        	const pj_str_t STR_PROXY_AUTH = { "Proxy-Authenticate", 18 };
        	pj_http_resp *response = &hreq->response;
        	pj_http_headers *hdrs = &response->headers;
        	unsigned i;

        	status = PJ_ENOTFOUND;
        	for (i = 0; i < hdrs->count; i++) {
        	    if (!pj_stricmp(&hdrs->header[i].name, &STR_WWW_AUTH) ||
        		!pj_stricmp(&hdrs->header[i].name, &STR_PROXY_AUTH))
        	    {
        		status = parse_auth_chal(hreq->inst_id, hreq->pool,
        					 &hdrs->header[i].value,
        					 &response->auth_chal);
        		break;
        	    }
        	}

                /* Check if we should perform authentication */
                if (status == PJ_SUCCESS &&
		    hreq->auth_state == AUTH_NONE &&
		    hreq->response.auth_chal.scheme.slen &&
		    hreq->param.auth_cred.username.slen &&
		    (hreq->param.auth_cred.scheme.slen == 0 ||
		     !pj_stricmp(&hreq->response.auth_chal.scheme,
				 &hreq->param.auth_cred.scheme)) &&
		    (hreq->param.auth_cred.realm.slen == 0 ||
		     !pj_stricmp(&hreq->response.auth_chal.realm,
				 &hreq->param.auth_cred.realm))
		    )
		    {
		    /* Yes, authentication is required and we have been
		     * configured with credential.
		     */
		    restart_req_with_auth(hreq);
		    if (hreq->auth_state == AUTH_RETRYING) {
			/* We'll be resending the request with auth. This
			 * connection has been closed.
			 */
			return PJ_FALSE;
		    }
                }
            }

            /* We already received the response header, call the 
             * appropriate callback.
             */
            if (hreq->cb.on_response)
                (*hreq->cb.on_response)(hreq, &hreq->response);
            hreq->response.data = NULL;
            hreq->response.size = 0;

	    if (rem > 0 || hreq->response.content_length == 0)
		return http_on_data_read(asock, (rem == 0 ? NULL:
		   	                 (char *)data + size - rem),
				         rem, PJ_SUCCESS, NULL);
        }

        return PJ_TRUE;
    }

    if (hreq->state != READING_DATA)
	return PJ_FALSE;
    if (hreq->cb.on_data_read) {
        /* If application wishes to receive the data once available, call
         * its callback.
         */
        if (size > 0)
            (*hreq->cb.on_data_read)(hreq, data, size);
    } else {
        if (hreq->response.size == 0) {
            /* If we know the content length, allocate the data based
             * on that, otherwise we'll use initial buffer size and grow 
             * it later if necessary.
             */
            hreq->response.size = (hreq->response.content_length == -1 ? 
                                   INITIAL_DATA_BUF_SIZE : 
                                   hreq->response.content_length);
            hreq->response.data = pj_pool_alloc(hreq->pool, 
                                                hreq->response.size);
        }

        /* If the size of data received exceeds its current size,
         * grow the buffer by a factor of 2.
         */
        if (hreq->tcp_state.current_read_size + size > 
            hreq->response.size) 
        {
            void *olddata = hreq->response.data;

            hreq->response.data = pj_pool_alloc(hreq->pool, 
                                                hreq->response.size << 1);
            pj_memcpy(hreq->response.data, olddata, hreq->response.size);
            hreq->response.size <<= 1;
        }

        /* Append the response data. */
        pj_memcpy((char *)hreq->response.data + 
                  hreq->tcp_state.current_read_size, data, size);
    }
    hreq->tcp_state.current_read_size += size;

    /* If the total data received so far is equal to the content length
     * or if it's already EOF.
     */
    if ((hreq->response.content_length >=0 &&
	(pj_ssize_t)hreq->tcp_state.current_read_size >=
        hreq->response.content_length) ||
        (status == PJ_EEOF && hreq->response.content_length == -1)) 
    {
	/* Finish reading */
        http_req_end_request(hreq);
        hreq->response.size = hreq->tcp_state.current_read_size;

        /* HTTP request is completed, call the callback. */
        if (hreq->cb.on_complete) {
            (*hreq->cb.on_complete)(hreq, PJ_SUCCESS, &hreq->response);
        }

        return PJ_FALSE;
    }

    /* Error status or premature EOF. */
    if ((status != PJ_SUCCESS && status != PJ_EPENDING && status != PJ_EEOF)
        || (status == PJ_EEOF && hreq->response.content_length > -1)) 
    {
        hreq->error = status;
        pj_http_req_cancel(hreq, PJ_TRUE);
        return PJ_FALSE;
    }
    
    return PJ_TRUE;
}

/* Callback to be called when query has timed out */
static void on_timeout( pj_timer_heap_t *timer_heap,
			struct pj_timer_entry *entry)
{
    pj_http_req *hreq = (pj_http_req *) entry->user_data;

    PJ_UNUSED_ARG(timer_heap);

    /* Recheck that the request is still not completed, since there is a 
     * slight possibility of race condition (timer elapsed while at the 
     * same time response arrives).
     */
    if (hreq->state == READING_COMPLETE) {
	/* Yeah, we finish on time */
	return;
    }

    /* Invalidate id. */
    hreq->timer_entry.id = 0;

    /* Request timed out. */
    hreq->error = PJ_ETIMEDOUT;
    pj_http_req_cancel(hreq, PJ_TRUE);
}

/* Parse authentication challenge */
static pj_status_t parse_auth_chal(int inst_id,
				   pj_pool_t *pool, pj_str_t *input,
				   pj_http_auth_chal *chal)
{
    pj_scanner scanner;
    const pj_str_t REALM_STR 	=    { "realm", 5},
    		NONCE_STR 	=    { "nonce", 5},
    		ALGORITHM_STR 	=    { "algorithm", 9 },
    		STALE_STR 	=    { "stale", 5},
    		QOP_STR 	=    { "qop", 3},
    		OPAQUE_STR 	=    { "opaque", 6};
    pj_status_t status = PJ_SUCCESS;
    PJ_USE_EXCEPTION ;

    pj_scan_init(inst_id, &scanner, input->ptr, input->slen, PJ_SCAN_AUTOSKIP_WS,
		 &on_syntax_error);
    PJ_TRY(inst_id) {
	/* Get auth scheme */
	if (*scanner.curptr == '"') {
	    pj_scan_get_quote(&scanner, '"', '"', &chal->scheme);
	    chal->scheme.ptr++;
	    chal->scheme.slen -= 2;
	} else {
	    pj_scan_get_until_chr(&scanner, " \t\r\n", &chal->scheme);
	}

	/* Loop parsing all parameters */
	for (;;) {
	    const char *end_param = ", \t\r\n;";
	    pj_str_t name, value;

	    /* Get pair of parameter name and value */
	    value.ptr = NULL;
	    value.slen = 0;
	    pj_scan_get_until_chr(&scanner, "=, \t\r\n", &name);
	    if (*scanner.curptr == '=') {
		pj_scan_get_char(&scanner);
		if (!pj_scan_is_eof(&scanner)) {
		    if (*scanner.curptr == '"' || *scanner.curptr == '\'') {
			int quote_char = *scanner.curptr;
			pj_scan_get_quote(&scanner, quote_char, quote_char,
					  &value);
			value.ptr++;
			value.slen -= 2;
		    } else if (!strchr(end_param, *scanner.curptr)) {
			pj_scan_get_until_chr(&scanner, end_param, &value);
		    }
		}
		value = pj_str_unescape(pool, &value);
	    }

	    if (!pj_stricmp(&name, &REALM_STR)) {
		chal->realm = value;

	    } else if (!pj_stricmp(&name, &NONCE_STR)) {
		chal->nonce = value;

	    } else if (!pj_stricmp(&name, &ALGORITHM_STR)) {
		chal->algorithm = value;

	    } else if (!pj_stricmp(&name, &OPAQUE_STR)) {
		chal->opaque = value;

	    } else if (!pj_stricmp(&name, &QOP_STR)) {
		chal->qop = value;

	    } else if (!pj_stricmp(&name, &STALE_STR)) {
		chal->stale = value.slen &&
			      (*value.ptr != '0') &&
			      (*value.ptr != 'f') &&
			      (*value.ptr != 'F');

	    }

	    /* Eat comma */
	    if (!pj_scan_is_eof(&scanner) && *scanner.curptr == ',')
		pj_scan_get_char(&scanner);
	    else
		break;
	}

    }
    PJ_CATCH_ANY {
	status = PJ_GET_EXCEPTION();
	pj_bzero(chal, sizeof(*chal));
	TRACE_((THIS_FILE, "Error: parsing of auth header failed"));
    }
    PJ_END(inst_id);
    pj_scan_fini(&scanner);
    return status;
}

/* The same as #pj_http_headers_add_elmt() with char * as
 * its parameters.
 */
PJ_DEF(pj_status_t) pj_http_headers_add_elmt2(pj_http_headers *headers, 
                                              char *name, char *val)
{
    pj_str_t f, v;
    pj_cstr(&f, name);
    pj_cstr(&v, val);
    return pj_http_headers_add_elmt(headers, &f, &v);
}   

PJ_DEF(pj_status_t) pj_http_headers_add_elmt(pj_http_headers *headers, 
                                             pj_str_t *name, 
                                             pj_str_t *val)
{
    PJ_ASSERT_RETURN(headers && name && val, PJ_FALSE);
    if (headers->count >= PJ_HTTP_HEADER_SIZE)
        return PJ_ETOOMANY;
    pj_strassign(&headers->header[headers->count].name, name);
    pj_strassign(&headers->header[headers->count++].value, val);
    return PJ_SUCCESS;
}

static pj_status_t http_response_parse(int inst_id, 
									   pj_pool_t *pool,
                                       pj_http_resp *response,
                                       void *data, pj_size_t size,
                                       pj_size_t *remainder)
{
    pj_size_t i;
    char *cptr;
    char *end_status, *newdata;
    pj_scanner scanner;
    pj_str_t s;
    const pj_str_t STR_CONTENT_LENGTH = { CONTENT_LENGTH, 14 };
    pj_status_t status;

    PJ_USE_EXCEPTION;

    PJ_ASSERT_RETURN(response, PJ_EINVAL);
    if (size < 2)
        return PJLIB_UTIL_EHTTPINCHDR;
    /* Detect whether we already receive the response's status-line
     * and its headers. We're looking for a pair of CRLFs. A pair of
     * LFs is also supported although it is not RFC standard.
     */
    cptr = (char *)data;
    for (i = 1, cptr++; i < size; i++, cptr++) {
        if (*cptr == '\n') {
            if (*(cptr - 1) == '\n')
                break;
            if (*(cptr - 1) == '\r') {
                if (i >= 3 && *(cptr - 2) == '\n' && *(cptr - 3) == '\r')
                    break;
            }
        }
    }
    if (i == size)
        return PJLIB_UTIL_EHTTPINCHDR;
    *remainder = size - 1 - i;

    pj_bzero(response, sizeof(response));
    response->content_length = -1;

    newdata = (char*) pj_pool_alloc(pool, i);
    pj_memcpy(newdata, data, i);

    /* Parse the status-line. */
    pj_scan_init(inst_id, &scanner, newdata, i, 0, &on_syntax_error);
    PJ_TRY(inst_id) {
        pj_scan_get_until_ch(&scanner, ' ', &response->version);
        pj_scan_advance_n(&scanner, 1, PJ_FALSE);
        pj_scan_get_until_ch(&scanner, ' ', &s);
        response->status_code = (pj_uint16_t)pj_strtoul(&s);
        pj_scan_advance_n(&scanner, 1, PJ_FALSE);
        pj_scan_get_until_ch(&scanner, '\n', &response->reason);
        if (response->reason.ptr[response->reason.slen-1] == '\r')
            response->reason.slen--;
    }
    PJ_CATCH_ANY {
        pj_scan_fini(&scanner);
        return PJ_GET_EXCEPTION();
    }
    PJ_END(inst_id);

    end_status = scanner.curptr;
    pj_scan_fini(&scanner);

    /* Parse the response headers. */
    size = i - 2 - (end_status - newdata);
    if (size > 0) {
        status = http_headers_parse(inst_id, end_status + 1, size,
                                    &response->headers);
    } else {
        status = PJ_SUCCESS;
    }

    /* Find content-length header field. */
    for (i = 0; i < response->headers.count; i++) {
        if (!pj_stricmp(&response->headers.header[i].name,
                        &STR_CONTENT_LENGTH))
        {
            response->content_length = 
                pj_strtoul(&response->headers.header[i].value);
            /* If content length is zero, make sure that it is because the
             * header value is really zero and not due to parsing error.
             */
            if (response->content_length == 0) {
                if (pj_strcmp2(&response->headers.header[i].value, "0")) {
                    response->content_length = -1;
                }
            }
            break;
        }
    }

    return status;
}

static pj_status_t http_headers_parse(int inst_id,
									  char *hdata, pj_size_t size, 
                                      pj_http_headers *headers)
{
    pj_scanner scanner;
    pj_str_t s, s2;
    pj_status_t status;
    PJ_USE_EXCEPTION;

    PJ_ASSERT_RETURN(headers, PJ_EINVAL);

    pj_scan_init(inst_id, &scanner, hdata, size, 0, &on_syntax_error);

    /* Parse each line of header field consisting of header field name and
     * value, separated by ":" and any number of white spaces.
     */
    PJ_TRY(inst_id) {
        do {
            pj_scan_get_until_chr(&scanner, ":\n", &s);
            if (*scanner.curptr == ':') {
                pj_scan_advance_n(&scanner, 1, PJ_TRUE);
                pj_scan_get_until_ch(&scanner, '\n', &s2);
                if (s2.ptr[s2.slen-1] == '\r')
                    s2.slen--;
                status = pj_http_headers_add_elmt(headers, &s, &s2);
                if (status != PJ_SUCCESS)
                    PJ_THROW(scanner.inst_id, status);
            }
            pj_scan_advance_n(&scanner, 1, PJ_TRUE);
            /* Finish parsing */
            if (pj_scan_is_eof(&scanner))
                break;
        } while (1);
    }
    PJ_CATCH_ANY {
        pj_scan_fini(&scanner);
        return PJ_GET_EXCEPTION();
    }
    PJ_END(inst_id);

    pj_scan_fini(&scanner);

    return PJ_SUCCESS;
}

PJ_DEF(void) pj_http_req_param_default(pj_http_req_param *param)
{
    pj_assert(param);
    pj_bzero(param, sizeof(*param));
    param->addr_family = pj_AF_INET();
    pj_strset2(&param->method, (char*)http_method_names[HTTP_GET]);
    pj_strset2(&param->version, (char*)HTTP_1_0);
    param->timeout.msec = PJ_HTTP_DEFAULT_TIMEOUT;
    pj_time_val_normalize(&param->timeout);
    param->max_retries = 3;
}

/* Get the location of '@' character to indicate the end of
 * user:passwd part of an URI. If user:passwd part is not
 * present, NULL will be returned.
 */
static char *get_url_at_pos(const char *str, long len)
{
    const char *end = str + len;
    const char *p = str;

    /* skip scheme: */
    while (p!=end && *p!='/') ++p;
    if (p!=end && *p=='/') ++p;
    if (p!=end && *p=='/') ++p;
    if (p==end) return NULL;

    for (; p!=end; ++p) {
	switch (*p) {
	case '/':
	    return NULL;
	case '@':
	    return (char*)p;
	}
    }

    return NULL;
}


PJ_DEF(pj_status_t) pj_http_req_parse_url(int inst_id, const pj_str_t *url, 
                                          pj_http_url *hurl)
{
    pj_scanner scanner;
    int len = url->slen;
    PJ_USE_EXCEPTION;

    if (!len) return -1;
    
    pj_bzero(hurl, sizeof(*hurl));
    pj_scan_init(inst_id, &scanner, url->ptr, url->slen, 0, &on_syntax_error);

    PJ_TRY(inst_id) {
	pj_str_t s;

        /* Exhaust any whitespaces. */
        pj_scan_skip_whitespace(&scanner);

        /* Parse the protocol */
        pj_scan_get_until_ch(&scanner, ':', &s);
        if (!pj_stricmp2(&s, http_protocol_names[PROTOCOL_HTTP])) {
            pj_strset2(&hurl->protocol,
        	       (char*)http_protocol_names[PROTOCOL_HTTP]);
        } else if (!pj_stricmp2(&s, http_protocol_names[PROTOCOL_HTTPS])) {
            pj_strset2(&hurl->protocol,
		       (char*)http_protocol_names[PROTOCOL_HTTPS]);
        } else {
            PJ_THROW(scanner.inst_id, PJ_ENOTSUP); // unsupported protocol
        }

        if (pj_scan_strcmp(&scanner, "://", 3)) {
            PJ_THROW(scanner.inst_id, PJLIB_UTIL_EHTTPINURL); // no "://" after protocol name
        }
        pj_scan_advance_n(&scanner, 3, PJ_FALSE);

        if (get_url_at_pos(url->ptr, url->slen)) {
            /* Parse username and password */
            pj_scan_get_until_chr(&scanner, ":@", &hurl->username);
            if (*scanner.curptr == ':') {
        	pj_scan_get_char(&scanner);
        	pj_scan_get_until_chr(&scanner, "@", &hurl->passwd);
            } else {
        	hurl->passwd.slen = 0;
            }
            pj_scan_get_char(&scanner);
        }

        /* Parse the host and port number (if any) */
        pj_scan_get_until_chr(&scanner, ":/", &s);
        pj_strassign(&hurl->host, &s);
        if (hurl->host.slen==0)
            PJ_THROW(scanner.inst_id, PJ_EINVAL);
        if (pj_scan_is_eof(&scanner) || *scanner.curptr == '/') {
            /* No port number specified */
            /* Assume default http/https port number */
            hurl->port = get_http_default_port(&hurl->protocol);
            pj_assert(hurl->port > 0);
        } else {
            pj_scan_advance_n(&scanner, 1, PJ_FALSE);
            pj_scan_get_until_ch(&scanner, '/', &s);
            /* Parse the port number */
            hurl->port = (pj_uint16_t)pj_strtoul(&s);
            if (!hurl->port)
                PJ_THROW(scanner.inst_id, PJLIB_UTIL_EHTTPINPORT); // invalid port number
        }

        if (!pj_scan_is_eof(&scanner)) {
            hurl->path.ptr = scanner.curptr;
            hurl->path.slen = scanner.end - scanner.curptr;
        } else {
            /* no path, append '/' */
            pj_cstr(&hurl->path, "/");
        }
    }
    PJ_CATCH_ANY {
        pj_scan_fini(&scanner);
	return PJ_GET_EXCEPTION();
    }
    PJ_END(inst_id);

    pj_scan_fini(&scanner);
    return PJ_SUCCESS;
}

PJ_DEF(void) pj_http_req_set_timeout(pj_http_req *http_req,
                                     const pj_time_val* timeout)
{
    pj_memcpy(&http_req->param.timeout, timeout, sizeof(*timeout));
}

PJ_DEF(pj_status_t) pj_http_req_create(int inst_id, 
									   pj_pool_t *pool,
                                       const pj_str_t *url,
                                       pj_timer_heap_t *timer,
                                       pj_ioqueue_t *ioqueue,
                                       const pj_http_req_param *param,
                                       const pj_http_req_callback *hcb,
                                       pj_http_req **http_req)
{
    pj_pool_t *own_pool;
    pj_http_req *hreq;
    char *at_pos;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && url && timer && ioqueue && 
                     hcb && http_req, PJ_EINVAL);

    *http_req = NULL;
    own_pool = pj_pool_create(pool->factory, NULL, INITIAL_POOL_SIZE, 
                              POOL_INCREMENT_SIZE, NULL);
    hreq = PJ_POOL_ZALLOC_T(own_pool, struct pj_http_req);
    if (!hreq)
        return PJ_ENOMEM;

    /* Initialization */
    hreq->pool = own_pool;
    hreq->ioqueue = ioqueue;
    hreq->timer = timer;
    hreq->asock = NULL;
    pj_memcpy(&hreq->cb, hcb, sizeof(*hcb));
    hreq->state = IDLE;
    hreq->resolved = PJ_FALSE;
    hreq->buffer.ptr = NULL;
	hreq->inst_id = inst_id;
    pj_timer_entry_init(&hreq->timer_entry, 0, hreq, &on_timeout);

    /* Initialize parameter */
    if (param) {
        pj_memcpy(&hreq->param, param, sizeof(*param));
        /* TODO: validate the param here
         * Should we validate the method as well? If yes, based on all HTTP
         * methods or based on supported methods only? For the later, one 
         * drawback would be that you can't use this if the method is not 
         * officially supported
         */
        PJ_ASSERT_RETURN(hreq->param.addr_family==PJ_AF_UNSPEC || 
                         hreq->param.addr_family==PJ_AF_INET || 
                         hreq->param.addr_family==PJ_AF_INET6, PJ_EAFNOTSUP);
        PJ_ASSERT_RETURN(!pj_strcmp2(&hreq->param.version, HTTP_1_0) || 
                         !pj_strcmp2(&hreq->param.version, HTTP_1_1), 
                         PJ_ENOTSUP); 
        pj_time_val_normalize(&hreq->param.timeout);
    } else {
        pj_http_req_param_default(&hreq->param);
    }

    /* Parse the URL */
    if (!pj_strdup_with_null(hreq->pool, &hreq->url, url)) {
	pj_pool_release(hreq->pool);
        return PJ_ENOMEM;
    }
    status = pj_http_req_parse_url(inst_id, &hreq->url, &hreq->hurl);
    if (status != PJ_SUCCESS) {
	pj_pool_release(hreq->pool);
        return status; // Invalid URL supplied
    }

    /* If URL contains username/password, move them to credential and
     * remove them from the URL.
     */
    if ((at_pos=get_url_at_pos(hreq->url.ptr, hreq->url.slen)) != NULL) {
	pj_str_t tmp;
	char *user_pos = pj_strchr(&hreq->url, '/');
	int removed_len;

	/* Save credential first, unescape the string */
	tmp = pj_str_unescape(hreq->pool, &hreq->hurl.username);;
	pj_strdup(hreq->pool, &hreq->param.auth_cred.username, &tmp);

	tmp = pj_str_unescape(hreq->pool, &hreq->hurl.passwd);
	pj_strdup(hreq->pool, &hreq->param.auth_cred.data, &tmp);

	hreq->hurl.username.ptr = hreq->hurl.passwd.ptr = NULL;
	hreq->hurl.username.slen = hreq->hurl.passwd.slen = 0;

	/* Remove "username:password@" from the URL */
	pj_assert(user_pos != 0 && user_pos < at_pos);
	user_pos += 2;
	removed_len = at_pos + 1 - user_pos;
	pj_memmove(user_pos, at_pos+1, hreq->url.ptr+hreq->url.slen-at_pos-1);
	hreq->url.slen -= removed_len;

	/* Need to adjust hostname and path pointers due to memmove*/
	if (hreq->hurl.host.ptr > user_pos &&
	    hreq->hurl.host.ptr < user_pos + hreq->url.slen)
	{
	    hreq->hurl.host.ptr -= removed_len;
	}
	/* path may come from a string constant, don't shift it if so */
	if (hreq->hurl.path.ptr > user_pos &&
	    hreq->hurl.path.ptr < user_pos + hreq->url.slen)
	{
	    hreq->hurl.path.ptr -= removed_len;
	}
    }

    *http_req = hreq;
    return PJ_SUCCESS;
}

PJ_DEF(pj_bool_t) pj_http_req_is_running(const pj_http_req *http_req)
{
   PJ_ASSERT_RETURN(http_req, PJ_FALSE);
   return (http_req->state != IDLE);
}

PJ_DEF(void*) pj_http_req_get_user_data(pj_http_req *http_req)
{
    PJ_ASSERT_RETURN(http_req, NULL);
    return http_req->param.user_data;
}

static pj_status_t start_http_req(pj_http_req *http_req,
                                  pj_bool_t notify_on_fail)
{
    pj_sock_t sock = PJ_INVALID_SOCKET;
    pj_status_t status;
    pj_activesock_cb asock_cb;
    int retry = 0;

    PJ_ASSERT_RETURN(http_req, PJ_EINVAL);
    /* Http request is not idle, a request was initiated before and 
     * is still in progress
     */
    PJ_ASSERT_RETURN(http_req->state == IDLE, PJ_EBUSY);

    /* Reset few things to make sure restarting works */
    http_req->error = 0;
    http_req->response.headers.count = 0;
    pj_bzero(&http_req->tcp_state, sizeof(http_req->tcp_state));

    if (!http_req->resolved) {
        /* Resolve the Internet address of the host */
        status = pj_sockaddr_init(http_req->param.addr_family, 
                                  &http_req->addr, &http_req->hurl.host,
				  http_req->hurl.port);
	if (status != PJ_SUCCESS ||
	    !pj_sockaddr_has_addr(&http_req->addr) ||
	    (http_req->param.addr_family==pj_AF_INET() &&
	     http_req->addr.ipv4.sin_addr.s_addr==PJ_INADDR_NONE))
	{
	    goto on_return;
        }
        http_req->resolved = PJ_TRUE;
    }

    status = pj_sock_socket(http_req->param.addr_family, pj_SOCK_STREAM(), 
                            0, &sock);
    if (status != PJ_SUCCESS)
        goto on_return; // error creating socket

    pj_bzero(&asock_cb, sizeof(asock_cb));
    asock_cb.on_data_read = &http_on_data_read;
    asock_cb.on_data_sent = &http_on_data_sent;
    asock_cb.on_connect_complete = &http_on_connect;
	
    do
    {
	pj_sockaddr_in bound_addr;
	pj_uint16_t port = 0;

	/* If we are using port restriction.
	 * Get a random port within the range
	 */
	if (http_req->param.source_port_range_start != 0) {
	    port = (pj_uint16_t)
		   (http_req->param.source_port_range_start +
		    (pj_rand() % http_req->param.source_port_range_size));
	}

	pj_sockaddr_in_init(&bound_addr, NULL, port);
	status = pj_sock_bind(sock, &bound_addr, sizeof(bound_addr));

    } while (status != PJ_SUCCESS && (retry++ < http_req->param.max_retries));

    if (status != PJ_SUCCESS) {
	PJ_PERROR(1,(THIS_FILE, status,
		     "Unable to bind to the requested port"));
	pj_sock_close(sock);
	goto on_return;
    }

    // TODO: should we set whole data to 0 by default?
    // or add it in the param?
    status = pj_activesock_create(http_req->pool, sock, pj_SOCK_STREAM(), 
                                  NULL, http_req->ioqueue,
				  &asock_cb, http_req, &http_req->asock);
    if (status != PJ_SUCCESS) {
        pj_sock_close(sock);
	goto on_return; // error creating activesock
    }

    /* Schedule timeout timer for the request */
    pj_assert(http_req->timer_entry.id == 0);
    http_req->timer_entry.id = 1;
    status = pj_timer_heap_schedule(http_req->timer, &http_req->timer_entry, 
                                    &http_req->param.timeout);
    if (status != PJ_SUCCESS) {
        http_req->timer_entry.id = 0;
	goto on_return; // error scheduling timer
    }

    /* Connect to host */
	http_req->state = CONNECTING;
    status = pj_activesock_start_connect(http_req->asock, http_req->pool, 
                                         (pj_sock_t *)&(http_req->addr), 
                                         pj_sockaddr_get_len(&http_req->addr));
    if (status == PJ_SUCCESS) {
        http_req->state = SENDING_REQUEST;
        status =  http_req_start_sending(http_req);
        if (status != PJ_SUCCESS)
            goto on_return;
    } else if (status != PJ_EPENDING) {
        goto on_return; // error connecting
    }

    return PJ_SUCCESS;

on_return:
    http_req->error = status;
    if (notify_on_fail)
	pj_http_req_cancel(http_req, PJ_TRUE);
    else
	http_req_end_request(http_req);

    return status;
}

/* Starts an asynchronous HTTP request to the URL specified. */
PJ_DEF(pj_status_t) pj_http_req_start(pj_http_req *http_req)
{
    return start_http_req(http_req, PJ_FALSE);
}

/* Respond to basic authentication challenge */
static pj_status_t auth_respond_basic(pj_http_req *hreq)
{
    /* Basic authentication:
     *      credentials       = "Basic" basic-credentials
     *      basic-credentials = base64-user-pass
     *      base64-user-pass  = <base64 [4] encoding of user-pass>
     *      user-pass         = userid ":" password
     *
     * Sample:
     *       Authorization: Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==
     */
    pj_str_t user_pass;
    pj_http_header_elmt *phdr;
    int len;

    /* Use send buffer to store userid ":" password */
    user_pass.ptr = hreq->buffer.ptr;
    pj_strcpy(&user_pass, &hreq->param.auth_cred.username);
    pj_strcat2(&user_pass, ":");
    pj_strcat(&user_pass, &hreq->param.auth_cred.data);

    /* Create Authorization header */
    phdr = &hreq->param.headers.header[hreq->param.headers.count++];
    pj_bzero(phdr, sizeof(*phdr));
    if (hreq->response.status_code == 401)
	phdr->name = pj_str("Authorization");
    else
	phdr->name = pj_str("Proxy-Authorization");

    len = PJ_BASE256_TO_BASE64_LEN(user_pass.slen) + 10;
    phdr->value.ptr = (char*)pj_pool_alloc(hreq->pool, len);
    phdr->value.slen = 0;

    pj_strcpy2(&phdr->value, "Basic ");
    len -= phdr->value.slen;
    pj_base64_encode((pj_uint8_t*)user_pass.ptr, (int)user_pass.slen,
		     phdr->value.ptr + phdr->value.slen, &len);
    phdr->value.slen += len;

    return PJ_SUCCESS;
}

/** Length of digest string. */
#define MD5_STRLEN 32
/* A macro just to get rid of type mismatch between char and unsigned char */
#define MD5_APPEND(pms,buf,len)	pj_md5_update(pms, (const pj_uint8_t*)buf, len)

/* Transform digest to string.
 * output must be at least PJSIP_MD5STRLEN+1 bytes.
 *
 * NOTE: THE OUTPUT STRING IS NOT NULL TERMINATED!
 */
static void digest2str(const unsigned char digest[], char *output)
{
    int i;
    for (i = 0; i<16; ++i) {
	pj_val_to_hex_digit(digest[i], output);
	output += 2;
    }
}

static void auth_create_digest_response(pj_str_t *result,
					const pj_http_auth_cred *cred,
				        const pj_str_t *nonce,
				        const pj_str_t *nc,
				        const pj_str_t *cnonce,
				        const pj_str_t *qop,
				        const pj_str_t *uri,
				        const pj_str_t *realm,
				        const pj_str_t *method)
{
    char ha1[MD5_STRLEN];
    char ha2[MD5_STRLEN];
    unsigned char digest[16];
    pj_md5_context pms;

    pj_assert(result->slen >= MD5_STRLEN);

    TRACE_((THIS_FILE, "Begin creating digest"));

    if (cred->data_type == 0) {
	/***
	 *** ha1 = MD5(username ":" realm ":" password)
	 ***/
	pj_md5_init(&pms);
	MD5_APPEND( &pms, cred->username.ptr, cred->username.slen);
	MD5_APPEND( &pms, ":", 1);
	MD5_APPEND( &pms, realm->ptr, realm->slen);
	MD5_APPEND( &pms, ":", 1);
	MD5_APPEND( &pms, cred->data.ptr, cred->data.slen);
	pj_md5_final(&pms, digest);

	digest2str(digest, ha1);

    } else if (cred->data_type == 1) {
	pj_assert(cred->data.slen == 32);
	pj_memcpy( ha1, cred->data.ptr, cred->data.slen );
    } else {
	pj_assert(!"Invalid data_type");
    }

    TRACE_((THIS_FILE, "  ha1=%.32s", ha1));

    /***
     *** ha2 = MD5(method ":" req_uri)
     ***/
    pj_md5_init(&pms);
    MD5_APPEND( &pms, method->ptr, method->slen);
    MD5_APPEND( &pms, ":", 1);
    MD5_APPEND( &pms, uri->ptr, uri->slen);
    pj_md5_final(&pms, digest);
    digest2str(digest, ha2);

    TRACE_((THIS_FILE, "  ha2=%.32s", ha2));

    /***
     *** When qop is not used:
     ***    response = MD5(ha1 ":" nonce ":" ha2)
     ***
     *** When qop=auth is used:
     ***    response = MD5(ha1 ":" nonce ":" nc ":" cnonce ":" qop ":" ha2)
     ***/
    pj_md5_init(&pms);
    MD5_APPEND( &pms, ha1, MD5_STRLEN);
    MD5_APPEND( &pms, ":", 1);
    MD5_APPEND( &pms, nonce->ptr, nonce->slen);
    if (qop && qop->slen != 0) {
	MD5_APPEND( &pms, ":", 1);
	MD5_APPEND( &pms, nc->ptr, nc->slen);
	MD5_APPEND( &pms, ":", 1);
	MD5_APPEND( &pms, cnonce->ptr, cnonce->slen);
	MD5_APPEND( &pms, ":", 1);
	MD5_APPEND( &pms, qop->ptr, qop->slen);
    }
    MD5_APPEND( &pms, ":", 1);
    MD5_APPEND( &pms, ha2, MD5_STRLEN);

    /* This is the final response digest. */
    pj_md5_final(&pms, digest);

    /* Convert digest to string and store in chal->response. */
    result->slen = MD5_STRLEN;
    digest2str(digest, result->ptr);

    TRACE_((THIS_FILE, "  digest=%.32s", result->ptr));
    TRACE_((THIS_FILE, "Digest created"));
}

/* Find out if qop offer contains "auth" token */
static pj_bool_t auth_has_qop( pj_pool_t *pool, const pj_str_t *qop_offer)
{
    pj_str_t qop;
    char *p;

    pj_strdup_with_null( pool, &qop, qop_offer);
    p = qop.ptr;
    while (*p) {
	*p = (char)pj_tolower(*p);
	++p;
    }

    p = qop.ptr;
    while (*p) {
	if (*p=='a' && *(p+1)=='u' && *(p+2)=='t' && *(p+3)=='h') {
	    int e = *(p+4);
	    if (e=='"' || e==',' || e==0)
		return PJ_TRUE;
	    else
		p += 4;
	} else {
	    ++p;
	}
    }

    return PJ_FALSE;
}

#define STR_PREC(s) (int)(s).slen, (s).ptr

/* Respond to digest authentication */
static pj_status_t auth_respond_digest(pj_http_req *hreq)
{
    const pj_http_auth_chal *chal = &hreq->response.auth_chal;
    const pj_http_auth_cred *cred = &hreq->param.auth_cred;
    pj_http_header_elmt *phdr;
    char digest_response_buf[MD5_STRLEN];
    int len;
    pj_str_t digest_response;

    /* Check algorithm is supported. We only support MD5 */
    if (chal->algorithm.slen!=0 &&
	pj_stricmp2(&chal->algorithm, "MD5"))
    {
	TRACE_((THIS_FILE, "Error: Unsupported digest algorithm \"%.*s\"",
		  chal->algorithm.slen, chal->algorithm.ptr));
	return PJ_ENOTSUP;
    }

    /* Add Authorization header */
    phdr = &hreq->param.headers.header[hreq->param.headers.count++];
    pj_bzero(phdr, sizeof(*phdr));
    if (hreq->response.status_code == 401)
	phdr->name = pj_str("Authorization");
    else
	phdr->name = pj_str("Proxy-Authorization");

    /* Allocate space for the header */
    len = 8 + /* Digest */
	  16 + hreq->param.auth_cred.username.slen + /* username= */
	  12 + chal->realm.slen + /* realm= */
	  12 + chal->nonce.slen + /* nonce= */
	  8 + hreq->hurl.path.slen + /* uri= */
	  16 + /* algorithm=MD5 */
	  16 + MD5_STRLEN + /* response= */
	  12 + /* qop=auth */
	  8 + /* nc=.. */
	  30 + /* cnonce= */
	  12 + chal->opaque.slen + /* opaque=".." */
	  0;
    phdr->value.ptr = (char*)pj_pool_alloc(hreq->pool, len);

    /* Configure buffer to temporarily store the digest */
    digest_response.ptr = digest_response_buf;
    digest_response.slen = MD5_STRLEN;

    if (chal->qop.slen == 0) {
	const pj_str_t STR_MD5 = { "MD5", 3 };

	/* Server doesn't require quality of protection. */
	auth_create_digest_response(&digest_response, cred,
				    &chal->nonce, NULL, NULL,  NULL,
				    &hreq->hurl.path, &chal->realm,
				    &hreq->param.method);

	len = pj_ansi_snprintf(
		phdr->value.ptr, len,
		"Digest username=\"%.*s\", "
		"realm=\"%.*s\", "
		"nonce=\"%.*s\", "
		"uri=\"%.*s\", "
		"algorithm=%.*s, "
		"response=\"%.*s\"",
		STR_PREC(cred->username),
		STR_PREC(chal->realm),
		STR_PREC(chal->nonce),
		STR_PREC(hreq->hurl.path),
		STR_PREC(STR_MD5),
		STR_PREC(digest_response));
	if (len < 0)
	    return PJ_ETOOSMALL;
	phdr->value.slen = len;

    } else if (auth_has_qop(hreq->pool, &chal->qop)) {
	/* Server requires quality of protection.
	 * We respond with selecting "qop=auth" protection.
	 */
	const pj_str_t STR_MD5 = { "MD5", 3 };
	const pj_str_t qop = pj_str("auth");
	const pj_str_t nc = pj_str("00000001");
	const pj_str_t cnonce = pj_str("b39971");

	auth_create_digest_response(&digest_response, cred,
				    &chal->nonce, &nc, &cnonce, &qop,
				    &hreq->hurl.path, &chal->realm,
				    &hreq->param.method);
	len = pj_ansi_snprintf(
		phdr->value.ptr, len,
		"Digest username=\"%.*s\", "
		"realm=\"%.*s\", "
		"nonce=\"%.*s\", "
		"uri=\"%.*s\", "
		"algorithm=%.*s, "
		"response=\"%.*s\", "
		"qop=%.*s, "
		"nc=%.*s, "
		"cnonce=\"%.*s\"",
		STR_PREC(cred->username),
		STR_PREC(chal->realm),
		STR_PREC(chal->nonce),
		STR_PREC(hreq->hurl.path),
		STR_PREC(STR_MD5),
		STR_PREC(digest_response),
		STR_PREC(qop),
		STR_PREC(nc),
		STR_PREC(cnonce));
	if (len < 0)
	    return PJ_ETOOSMALL;
	phdr->value.slen = len;

	if (chal->opaque.slen) {
	    pj_strcat2(&phdr->value, ", opaque=\"");
	    pj_strcat(&phdr->value, &chal->opaque);
	    pj_strcat2(&phdr->value, "\"");
	}

    } else {
	/* Server requires quality protection that we don't support. */
	TRACE_((THIS_FILE, "Error: Unsupported qop offer %.*s",
		chal->qop.slen, chal->qop.ptr));
	return PJ_ENOTSUP;
    }

    return PJ_SUCCESS;
}


static void restart_req_with_auth(pj_http_req *hreq)
{
    pj_http_auth_chal *chal = &hreq->response.auth_chal;
    pj_http_auth_cred *cred = &hreq->param.auth_cred;
    pj_status_t status;

    if (hreq->param.headers.count >= PJ_HTTP_HEADER_SIZE) {
	TRACE_((THIS_FILE, "Error: no place to put Authorization header"));
	hreq->auth_state = AUTH_DONE;
	return;
    }

    /* If credential specifies specific scheme, make sure they match */
    if (cred->scheme.slen && pj_stricmp(&chal->scheme, &cred->scheme)) {
	status = PJ_ENOTSUP;
	TRACE_((THIS_FILE, "Error: auth schemes mismatch"));
	goto on_error;
    }

    /* If credential specifies specific realm, make sure they match */
    if (cred->realm.slen && pj_stricmp(&chal->realm, &cred->realm)) {
	status = PJ_ENOTSUP;
	TRACE_((THIS_FILE, "Error: auth realms mismatch"));
	goto on_error;
    }

    if (!pj_stricmp2(&chal->scheme, "basic")) {
	status = auth_respond_basic(hreq);
    } else if (!pj_stricmp2(&chal->scheme, "digest")) {
	status = auth_respond_digest(hreq);
    } else {
	TRACE_((THIS_FILE, "Error: unsupported HTTP auth scheme"));
	status = PJ_ENOTSUP;
    }

    if (status != PJ_SUCCESS)
	goto on_error;

    http_req_end_request(hreq);

    status = start_http_req(hreq, PJ_TRUE);
    if (status != PJ_SUCCESS)
	goto on_error;

    hreq->auth_state = AUTH_RETRYING;
    return;

on_error:
    hreq->auth_state = AUTH_DONE;
}


/* snprintf() to a pj_str_t struct with an option to append the 
 * result at the back of the string.
 */
void str_snprintf(pj_str_t *s, size_t size, 
                  pj_bool_t append, const char *format, ...)
{
    va_list arg;
    int retval;

    va_start(arg, format);
    if (!append)
        s->slen = 0;
    size -= s->slen;
    retval = pj_ansi_vsnprintf(s->ptr + s->slen, 
                               size, format, arg);
    s->slen += ((retval < (int)size) ? retval : size - 1);
    va_end(arg);
}

static pj_status_t http_req_start_sending(pj_http_req *hreq)
{
    pj_status_t status;
    pj_str_t pkt;
    pj_ssize_t len;
    pj_size_t i;

    PJ_ASSERT_RETURN(hreq->state == SENDING_REQUEST || 
                     hreq->state == SENDING_REQUEST_BODY, PJ_EBUG);

    if (hreq->state == SENDING_REQUEST) {
        /* Prepare the request data */
        if (!hreq->buffer.ptr)
            hreq->buffer.ptr = (char*)pj_pool_alloc(hreq->pool, BUF_SIZE);
        pj_strassign(&pkt, &hreq->buffer);
        pkt.slen = 0;
        /* Start-line */
        str_snprintf(&pkt, BUF_SIZE, PJ_TRUE, "%.*s %.*s %s/%.*s\r\n",
                     STR_PREC(hreq->param.method), 
                     STR_PREC(hreq->hurl.path),
                     get_protocol(&hreq->hurl.protocol), 
                     STR_PREC(hreq->param.version));
        /* Header field "Host" */
        str_snprintf(&pkt, BUF_SIZE, PJ_TRUE, "Host: %.*s:%d\r\n",
                     STR_PREC(hreq->hurl.host), hreq->hurl.port);
        if (!pj_strcmp2(&hreq->param.method, http_method_names[HTTP_PUT])) {
            char buf[16];

            /* Header field "Content-Length" */
            pj_utoa(hreq->param.reqdata.total_size ? 
                    hreq->param.reqdata.total_size: 
                    hreq->param.reqdata.size, buf);
            str_snprintf(&pkt, BUF_SIZE, PJ_TRUE, "%s: %s\r\n",
                         CONTENT_LENGTH, buf);
        }

        /* Append user-specified headers */
        for (i = 0; i < hreq->param.headers.count; i++) {
            str_snprintf(&pkt, BUF_SIZE, PJ_TRUE, "%.*s: %.*s\r\n",
                         STR_PREC(hreq->param.headers.header[i].name),
                         STR_PREC(hreq->param.headers.header[i].value));
        }
        if (pkt.slen >= BUF_SIZE - 1) {
            status = PJLIB_UTIL_EHTTPINSBUF;
            goto on_return;
        }

        pj_strcat2(&pkt, "\r\n");
        pkt.ptr[pkt.slen] = 0;
        TRACE_((THIS_FILE, "%s", pkt.ptr));
    } else {
        pkt.ptr = (char*)hreq->param.reqdata.data;
        pkt.slen = hreq->param.reqdata.size;
    }

    /* Send the request */
    len = pj_strlen(&pkt);
    pj_ioqueue_op_key_init(&hreq->op_key, sizeof(hreq->op_key));
    hreq->tcp_state.send_size = len;
    hreq->tcp_state.current_send_size = 0;
    status = pj_activesock_send(hreq->asock, &hreq->op_key, 
                                pkt.ptr, &len, 0);

    if (status == PJ_SUCCESS) {
        http_on_data_sent(hreq->asock, &hreq->op_key, len);
    } else if (status != PJ_EPENDING) {
        goto on_return; // error sending data
    }

    return PJ_SUCCESS;

on_return:
    http_req_end_request(hreq);
    return status;
}

static pj_status_t http_req_start_reading(pj_http_req *hreq)
{
    pj_status_t status;

    PJ_ASSERT_RETURN(hreq->state == REQUEST_SENT, PJ_EBUG);

    /* Receive the response */
    hreq->state = READING_RESPONSE;
    hreq->tcp_state.current_read_size = 0;
    pj_assert(hreq->buffer.ptr);
    status = pj_activesock_start_read2(hreq->asock, hreq->pool, BUF_SIZE, 
                                       (void**)&hreq->buffer.ptr, 0);
    if (status != PJ_SUCCESS) {
        /* Error reading */
        http_req_end_request(hreq);
        return status;
    }

    return PJ_SUCCESS;
}

static pj_status_t http_req_end_request(pj_http_req *hreq)
{
    if (hreq->asock) {
	pj_activesock_close(hreq->asock);
        hreq->asock = NULL;
    }

    /* Cancel query timeout timer. */
    if (hreq->timer_entry.id != 0) {
        pj_timer_heap_cancel(hreq->timer, &hreq->timer_entry);
        /* Invalidate id. */
        hreq->timer_entry.id = 0;
    }

    hreq->state = IDLE;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_http_req_cancel(pj_http_req *http_req,
                                       pj_bool_t notify)
{
    http_req->state = ABORTING;

    http_req_end_request(http_req);

    if (notify && http_req->cb.on_complete) {
        (*http_req->cb.on_complete)(http_req, (!http_req->error? 
                                    PJ_ECANCELLED: http_req->error), NULL);
    }

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pj_http_req_destroy(pj_http_req *http_req)
{
    PJ_ASSERT_RETURN(http_req, PJ_EINVAL);
    
    /* If there is any pending request, cancel it */
    if (http_req->state != IDLE) {
        pj_http_req_cancel(http_req, PJ_FALSE);
    }

    pj_pool_release(http_req->pool);

    return PJ_SUCCESS;
}
