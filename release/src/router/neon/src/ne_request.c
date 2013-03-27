/* 
   HTTP request/response handling
   Copyright (C) 1999-2010, Joe Orton <joe@manyfish.co.uk>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA

*/

/* This is the HTTP client request/response implementation.
 * The goal of this code is to be modular and simple.
 */

#include "config.h"

#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif 
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "ne_internal.h"

#include "ne_alloc.h"
#include "ne_request.h"
#include "ne_string.h" /* for ne_buffer */
#include "ne_utils.h"
#include "ne_socket.h"
#include "ne_uri.h"

#include "ne_private.h"
#include "webdav_base.h"   //add by alan

#define SOCK_ERR(req, op, msg) do { ssize_t sret = (op); \
if (sret < 0) return aborted(req, msg, sret); } while (0)

#define EOL "\r\n"

struct body_reader {
    ne_block_reader handler;
    ne_accept_response accept_response;
    unsigned int use;
    void *userdata;
    struct body_reader *next;
};

struct field {
    char *name, *value;
    size_t vlen;
    struct field *next;
};

/* Maximum number of header fields per response: */
#define MAX_HEADER_FIELDS (100)
/* Size of hash table; 43 is the smallest prime for which the common
 * header names hash uniquely using the *33 hash function. */
#define HH_HASHSIZE (43)
/* Hash iteration step: *33 known to be a good hash for ASCII, see RSE. */
#define HH_ITERATE(hash, ch) (((hash)*33 + (unsigned char)(ch)) % HH_HASHSIZE)

/* pre-calculated hash values for given header names: */
#define HH_HV_CONNECTION        (0x14)
#define HH_HV_PROXY_CONNECTION  (0x1A)
#define HH_HV_CONTENT_LENGTH    (0x13)
#define HH_HV_TRANSFER_ENCODING (0x07)

struct ne_request_s {
    char *method, *uri; /* method and Request-URI */

    ne_buffer *headers; /* request headers */

    /* Request body. */
    ne_provide_body body_cb;
    void *body_ud;

    /* Request body source: file or buffer (if not callback). */
    union {
        struct {
            int fd;
            ne_off_t offset, length;
            ne_off_t remain; /* remaining bytes to send. */
        } file;
	struct {
            /* length bytes @ buffer = whole body.
             * remain bytes @ pnt = remaining bytes to send */
	    const char *buffer, *pnt;
	    size_t length, remain;
	} buf;
    } body;
	    
    ne_off_t body_length; /* length of request body */

    /* temporary store for response lines. */
    char respbuf[NE_BUFSIZ];

    /**** Response ***/

    /* The transfer encoding types */
    struct ne_response {
	enum {
	    R_TILLEOF = 0, /* read till eof */
	    R_NO_BODY, /* implicitly no body (HEAD, 204, 304) */
	    R_CHUNKED, /* using chunked transfer-encoding */
	    R_CLENGTH  /* using given content-length */
	} mode;
        union {
            /* clen: used if mode == R_CLENGTH; total and bytes
             * remaining to be read of response body. */
            struct {
                ne_off_t total, remain;
            } clen;
            /* chunk: used if mode == R_CHUNKED; total and bytes
             * remaining to be read of current chunk */
            struct {
                size_t total, remain;
            } chunk;
        } body;
        ne_off_t progress; /* number of bytes read of response */
    } resp;
    
    struct hook *private;

    /* response header fields */
    struct field *response_headers[HH_HASHSIZE];
    
    unsigned int current_index; /* response_headers cursor for iterator */

    /* List of callbacks which are passed response body blocks */
    struct body_reader *body_readers;

    /*** Miscellaneous ***/
    unsigned int method_is_head;
    unsigned int can_persist;

    int flags[NE_REQFLAG_LAST];

    ne_session *session;
    ne_status status;
};

static int open_connection(ne_session *sess);

/* Returns hash value for header 'name', converting it to lower-case
 * in-place. */
static inline unsigned int hash_and_lower(char *name)
{
    char *pnt;
    unsigned int hash = 0;

    for (pnt = name; *pnt != '\0'; pnt++) {
	*pnt = ne_tolower(*pnt);
	hash = HH_ITERATE(hash,*pnt);
    }

    return hash;
}

/* Abort a request due to an non-recoverable HTTP protocol error,
 * whilst doing 'doing'.  'code', if non-zero, is the socket error
 * code, NE_SOCK_*, or if zero, is ignored. */
static int aborted(ne_request *req, const char *doing, ssize_t code)
{
    ne_session *sess = req->session;
    int ret = NE_ERROR;

    NE_DEBUG(NE_DBG_HTTP, "Aborted request (%" NE_FMT_SSIZE_T "): %s\n",
	     code, doing);

    switch(code) {
    case NE_SOCK_CLOSED:
	if (sess->nexthop->proxy != PROXY_NONE) {
	    ne_set_error(sess, _("%s: connection was closed by proxy server"),
			 doing);
	} else {
	    ne_set_error(sess, _("%s: connection was closed by server"),
			 doing);
	}
	break;
    case NE_SOCK_TIMEOUT:
	ne_set_error(sess, _("%s: connection timed out"), doing);
	ret = NE_TIMEOUT;
	break;
    case NE_SOCK_ERROR:
    case NE_SOCK_RESET:
    case NE_SOCK_TRUNC:
        ne_set_error(sess, "%s: %s", doing, ne_sock_error(sess->socket));
        break;
    case 0:
	ne_set_error(sess, "%s", doing);
	break;
    }

    ne_close_connection(sess);
    return ret;
}

static void notify_status(ne_session *sess, ne_session_status status)
{
    if (sess->notify_cb) {
	sess->notify_cb(sess->notify_ud, status, &sess->status);
    }
}

static void *get_private(const struct hook *hk, const char *id)
{
    for (; hk != NULL; hk = hk->next)
	if (strcmp(hk->id, id) == 0)
	    return hk->userdata;
    return NULL;
}

void *ne_get_request_private(ne_request *req, const char *id)
{
    return get_private(req->private, id);
}

void *ne_get_session_private(ne_session *sess, const char *id)
{
    return get_private(sess->private, id);
}

void ne_set_request_private(ne_request *req, const char *id, void *userdata)
{
    struct hook *hk = ne_malloc(sizeof (struct hook)), *pos;

    if (req->private != NULL) {
	for (pos = req->private; pos->next != NULL; pos = pos->next)
	    /* nullop */;
	pos->next = hk;
    } else {
	req->private = hk;
    }

    hk->id = id;
    hk->fn = NULL;
    hk->userdata = userdata;
    hk->next = NULL;
}

static ssize_t body_string_send(void *userdata, char *buffer, size_t count)
{
    ne_request *req = userdata;
    
    if (count == 0) {
	req->body.buf.remain = req->body.buf.length;
	req->body.buf.pnt = req->body.buf.buffer;
    } else {
	/* if body_left == 0 we fall through and return 0. */
	if (req->body.buf.remain < count)
	    count = req->body.buf.remain;

	memcpy(buffer, req->body.buf.pnt, count);
	req->body.buf.pnt += count;
	req->body.buf.remain -= count;
    }

    return count;
}    

static ssize_t body_fd_send(void *userdata, char *buffer, size_t count)
{
    ne_request *req = userdata;


    if (count) {
        ssize_t ret;

        if (req->body.file.remain == 0)
            return 0;

        /* Casts here are necessary for LFS platforms for safe and
         * warning-free assignment/comparison between 32-bit size_t
         * and 64-bit off64_t: */
        if ((ne_off_t)count > req->body.file.remain)
            count = (size_t)req->body.file.remain;
        
        ret = read(req->body.file.fd, buffer, count);
        if (ret > 0) {
            req->body.file.remain -= ret;
            return ret;
        }
        else if (ret == 0) {
            ne_set_error(req->session, 
                         _("Premature EOF in request body file"));
        }
        else if (ret < 0) {
            char err[200];
            int errnum = errno;

            ne_set_error(req->session, 
                         _("Failed reading request body file: %s"),
                         ne_strerror(errnum, err, sizeof err));
        }

        return -1;
    } else {
        ne_off_t newoff;

        /* rewind for next send. */
        newoff = ne_lseek(req->body.file.fd, req->body.file.offset, SEEK_SET);
        if (newoff == req->body.file.offset) {
            req->body.file.remain = req->body.file.length;
            return 0;
        } else {
            char err[200], offstr[20];

            if (newoff == -1) {
                /* errno was set */
                ne_strerror(errno, err, sizeof err);
            } else {
                strcpy(err, _("offset invalid"));
            }
            ne_snprintf(offstr, sizeof offstr, "%" FMT_NE_OFF_T,
                        req->body.file.offset);
            ne_set_error(req->session, 
                         _("Could not seek to offset %s"
                           " of request body file: %s"), 
                           offstr, err);
            return -1;
        }
    }
}

/* For accurate persistent connection handling, for any write() or
 * read() operation for a new request on an already-open connection,
 * an EOF or RST error MUST be treated as a persistent connection
 * timeout, and the request retried on a new connection.  Once a
 * read() operation has succeeded, any subsequent error MUST be
 * treated as fatal.  A 'retry' flag is used; retry=1 represents the
 * first case, retry=0 the latter. */

/* RETRY_RET() crafts a function return value given the 'retry' flag,
 * the socket error 'code', and the return value 'acode' from the
 * aborted() function. */
#define RETRY_RET(retry, code, acode) \
((((code) == NE_SOCK_CLOSED || (code) == NE_SOCK_RESET || \
 (code) == NE_SOCK_TRUNC) && retry) ? NE_RETRY : (acode))

/* Sends the request body; returns 0 on success or an NE_* error code.
 * If retry is non-zero; will return NE_RETRY on persistent connection
 * timeout.  On error, the session error string is set and the
 * connection is closed. */
static int send_request_body(ne_request *req, int retry)
{
    ne_session *const sess = req->session;
    char buffer[NE_BUFSIZ];
    ssize_t bytes;

    NE_DEBUG(NE_DBG_HTTP, "Sending request body:\n");

    req->session->status.sr.progress = 0;
    req->session->status.sr.total = req->body_length;
    notify_status(sess, ne_status_sending);
    
    /* tell the source to start again from the beginning. */
    if (req->body_cb(req->body_ud, NULL, 0) != 0) {
        ne_close_connection(sess);
        return NE_ERROR;
    }
    
    while ((bytes = req->body_cb(req->body_ud, buffer, sizeof buffer)) > 0) {
    	//next 3 rows add by alan
    	if(exit_loop == 1){
    		return NE_WEBDAV_QUIT;
    	}
	int ret = ne_sock_fullwrite(sess->socket, buffer, bytes);
        if (ret < 0) {
            int aret = aborted(req, _("Could not send request body"), ret);
            return RETRY_RET(retry, ret, aret);
        }

	NE_DEBUG(NE_DBG_HTTPBODY, 
		 "Body block (%" NE_FMT_SSIZE_T " bytes):\n[%.*s]\n",
		 bytes, (int)bytes, buffer);

        /* invoke progress callback */
        req->session->status.sr.progress += bytes;
        notify_status(sess, ne_status_sending);
    }

    if (bytes == 0) {
        return NE_OK;
    } else {
        NE_DEBUG(NE_DBG_HTTP, "Request body provider failed with "
                 "%" NE_FMT_SSIZE_T "\n", bytes);
        ne_close_connection(sess);
        return NE_ERROR;
    }
}

/* Lob the User-Agent, connection and host headers in to the request
 * headers */
static void add_fixed_headers(ne_request *req) 
{
    ne_session *const sess = req->session;

    if (sess->user_agent) {
        ne_buffer_zappend(req->headers, sess->user_agent);
    }

    /* If persistent connections are disabled, just send Connection:
     * close; otherwise, send Connection: Keep-Alive to pre-1.1 origin
     * servers to try harder to get a persistent connection, except if
     * using a proxy as per 2068§19.7.1.  Always add TE: trailers. */
    if (!sess->flags[NE_SESSFLAG_PERSIST]) {
       ne_buffer_czappend(req->headers, "Connection: TE, close" EOL);
    } 
    else if (!sess->is_http11 && !sess->any_proxy_http) {
        ne_buffer_czappend(req->headers, 
                           "Keep-Alive: " EOL
                          "Connection: TE, Keep-Alive" EOL);
    } 
    else if (!req->session->is_http11 && !sess->any_proxy_http) {
        ne_buffer_czappend(req->headers, 
                           "Keep-Alive: " EOL
                           "Proxy-Connection: Keep-Alive" EOL
                           "Connection: TE" EOL);
    } 
    else {
        ne_buffer_czappend(req->headers, "Connection: TE" EOL);
    }

    ne_buffer_concat(req->headers, "TE: trailers" EOL "Host: ", 
                     req->session->server.hostport, EOL, NULL);
}

int ne_accept_always(void *userdata, ne_request *req, const ne_status *st)
{
    return 1;
}				   

int ne_accept_2xx(void *userdata, ne_request *req, const ne_status *st)
{
    return (st->klass == 2);
}

ne_request *ne_request_create(ne_session *sess,
			      const char *method, const char *path) 
{
    ne_request *req = ne_calloc(sizeof *req);

    req->session = sess;
    req->headers = ne_buffer_create();
    
    /* Presume the method is idempotent by default. */
    req->flags[NE_REQFLAG_IDEMPOTENT] = 1;
    /* Expect-100 default follows the corresponding session flag. */
    req->flags[NE_REQFLAG_EXPECT100] = sess->flags[NE_SESSFLAG_EXPECT100];

    /* Add in the fixed headers */
    add_fixed_headers(req);

    /* Set the standard stuff */
    req->method = ne_strdup(method);
    req->method_is_head = (strcmp(method, "HEAD") == 0);

    /* Only use an absoluteURI here when we might be using an HTTP
     * proxy, and SSL is in use: some servers can't parse them. */
    if (sess->any_proxy_http && !req->session->use_ssl && path[0] == '/')
	req->uri = ne_concat(req->session->scheme, "://", 
                             req->session->server.hostport, path, NULL);
    else
	req->uri = ne_strdup(path);

    {
	struct hook *hk;

	for (hk = sess->create_req_hooks; hk != NULL; hk = hk->next) {
	    ne_create_request_fn fn = (ne_create_request_fn)hk->fn;
	    fn(req, hk->userdata, req->method, req->uri);
	}
    }

    return req;
}

/* Set the request body length to 'length' */
static void set_body_length(ne_request *req, ne_off_t length)
{
    req->body_length = length;
    ne_print_request_header(req, "Content-Length", "%" FMT_NE_OFF_T, length);
}

void ne_set_request_body_buffer(ne_request *req, const char *buffer,
				size_t size)
{
    req->body.buf.buffer = buffer;
    req->body.buf.length = size;
    req->body_cb = body_string_send;
    req->body_ud = req;
    set_body_length(req, size);
}

void ne_set_request_body_provider(ne_request *req, ne_off_t bodysize,
				  ne_provide_body provider, void *ud)
{
    req->body_cb = provider;
    req->body_ud = ud;
    set_body_length(req, bodysize);
}

void ne_set_request_body_fd(ne_request *req, int fd,
                            ne_off_t offset, ne_off_t length)
{
    req->body.file.fd = fd;
    req->body.file.offset = offset;
    req->body.file.length = length;
    req->body_cb = body_fd_send;
    req->body_ud = req;
    set_body_length(req, length);
}

void ne_set_request_flag(ne_request *req, ne_request_flag flag, int value)
{
    if (flag < NE_SESSFLAG_LAST) {
        req->flags[flag] = value;
    }
}

int ne_get_request_flag(ne_request *req, ne_request_flag flag)
{
    if (flag < NE_REQFLAG_LAST) {
        return req->flags[flag];
    }
    return -1;
}

void ne_add_request_header(ne_request *req, const char *name, 
			   const char *value)
{
    ne_buffer_concat(req->headers, name, ": ", value, EOL, NULL);
}

void ne_print_request_header(ne_request *req, const char *name,
			     const char *format, ...)
{
    va_list params;
    char buf[NE_BUFSIZ];
    
    va_start(params, format);
    ne_vsnprintf(buf, sizeof buf, format, params);
    va_end(params);
    
    ne_buffer_concat(req->headers, name, ": ", buf, EOL, NULL);
}

/* Returns the value of the response header 'name', for which the hash
 * value is 'h', or NULL if the header is not found. */
static inline char *get_response_header_hv(ne_request *req, unsigned int h,
                                           const char *name)
{
    struct field *f;

    for (f = req->response_headers[h]; f; f = f->next)
        if (strcmp(f->name, name) == 0)
            return f->value;

    return NULL;
}

const char *ne_get_response_header(ne_request *req, const char *name)
{
    char *lcname = ne_strdup(name);
    unsigned int hash = hash_and_lower(lcname);
    char *value = get_response_header_hv(req, hash, lcname);
    ne_free(lcname);
    return value;
}

/* The return value of the iterator function is a pointer to the
 * struct field of the previously returned header. */
void *ne_response_header_iterate(ne_request *req, void *iterator,
                                 const char **name, const char **value)
{
    struct field *f = iterator;
    unsigned int n;

    if (f == NULL) {
        n = 0;
    } else if ((f = f->next) == NULL) {
        n = req->current_index + 1;
    }

    if (f == NULL) {
        while (n < HH_HASHSIZE && req->response_headers[n] == NULL)
            n++;
        if (n == HH_HASHSIZE)
            return NULL; /* no more headers */
        f = req->response_headers[n];
        req->current_index = n;
    }
    
    *name = f->name;
    *value = f->value;
    return f;
}

/* Removes the response header 'name', which has hash value 'hash'. */
static void remove_response_header(ne_request *req, const char *name, 
                                   unsigned int hash)
{
    struct field **ptr = req->response_headers + hash;

    while (*ptr) {
        struct field *const f = *ptr;

        if (strcmp(f->name, name) == 0) {
            *ptr = f->next;
            ne_free(f->name);
            ne_free(f->value);
            ne_free(f);
            return;
        }
        
        ptr = &f->next;
    }
}

/* Free all stored response headers. */
static void free_response_headers(ne_request *req)
{
    int n;

    for (n = 0; n < HH_HASHSIZE; n++) {
        struct field **ptr = req->response_headers + n;

        while (*ptr) {
            struct field *const f = *ptr;
            *ptr = f->next;
            ne_free(f->name);
            ne_free(f->value);
            ne_free(f);
	}
    }
}

void ne_add_response_body_reader(ne_request *req, ne_accept_response acpt,
				 ne_block_reader rdr, void *userdata)
{
    struct body_reader *new = ne_malloc(sizeof *new);
    new->accept_response = acpt;
    new->handler = rdr;
    new->userdata = userdata;
    new->next = req->body_readers;
    req->body_readers = new;
}

void ne_request_destroy(ne_request *req) 
{
    struct body_reader *rdr, *next_rdr;
    struct hook *hk, *next_hk;

    ne_free(req->uri);
    ne_free(req->method);

    for (rdr = req->body_readers; rdr != NULL; rdr = next_rdr) {
	next_rdr = rdr->next;
	ne_free(rdr);
    }

    free_response_headers(req);

    ne_buffer_destroy(req->headers);

    NE_DEBUG(NE_DBG_HTTP, "Running destroy hooks.\n");
    for (hk = req->session->destroy_req_hooks; hk; hk = next_hk) {
	ne_destroy_req_fn fn = (ne_destroy_req_fn)hk->fn;
        next_hk = hk->next;
	fn(req, hk->userdata);
    }

    for (hk = req->private; hk; hk = next_hk) {
	next_hk = hk->next;
	ne_free(hk);
    }

    if (req->status.reason_phrase)
	ne_free(req->status.reason_phrase);

    NE_DEBUG(NE_DBG_HTTP, "Request ends.\n");
    ne_free(req);
}


/* Reads a block of the response into BUFFER, which is of size
 * *BUFLEN.  Returns zero on success or non-zero on error.  On
 * success, *BUFLEN is updated to be the number of bytes read into
 * BUFFER (which will be 0 to indicate the end of the repsonse).  On
 * error, the connection is closed and the session error string is
 * set.  */
static int read_response_block(ne_request *req, struct ne_response *resp, 
			       char *buffer, size_t *buflen) 
{
    ne_socket *const sock = req->session->socket;
    size_t willread;
    ssize_t readlen;
    
    switch (resp->mode) {
    case R_CHUNKED:
        /* Chunked transfer-encoding: chunk syntax is "SIZE CRLF CHUNK
         * CRLF SIZE CRLF CHUNK CRLF ..." followed by zero-length
         * chunk: "CHUNK CRLF 0 CRLF".  resp.chunk.remain contains the
         * number of bytes left to read in the current chunk. */
	if (resp->body.chunk.remain == 0) {
	    unsigned long chunk_len;
	    char *ptr;

            /* Read the chunk size line into a temporary buffer. */
            SOCK_ERR(req,
                     ne_sock_readline(sock, req->respbuf, sizeof req->respbuf),
                     _("Could not read chunk size"));
            NE_DEBUG(NE_DBG_HTTP, "[chunk] < %s", req->respbuf);
            chunk_len = strtoul(req->respbuf, &ptr, 16);
	    /* limit chunk size to <= UINT_MAX, so it will probably
	     * fit in a size_t. */
	    if (ptr == req->respbuf || 
		chunk_len == ULONG_MAX || chunk_len > UINT_MAX) {
		return aborted(req, _("Could not parse chunk size"), 0);
	    }
	    NE_DEBUG(NE_DBG_HTTP, "Got chunk size: %lu\n", chunk_len);
	    resp->body.chunk.remain = chunk_len;
	}
	willread = resp->body.chunk.remain > *buflen
            ? *buflen : resp->body.chunk.remain;
	break;
    case R_CLENGTH:
	willread = resp->body.clen.remain > (off_t)*buflen 
            ? *buflen : (size_t)resp->body.clen.remain;
	break;
    case R_TILLEOF:
	willread = *buflen;
	break;
    case R_NO_BODY:
    default:
	willread = 0;
	break;
    }
    if (willread == 0) {
	*buflen = 0;
	return 0;
    }
    NE_DEBUG(NE_DBG_HTTP,
	     "Reading %" NE_FMT_SIZE_T " bytes of response body.\n", willread);
    readlen = ne_sock_read(sock, buffer, willread);

    /* EOF is only valid when response body is delimited by it.
     * Strictly, an SSL truncation should not be treated as an EOF in
     * any case, but SSL servers are just too buggy.  */
    if (resp->mode == R_TILLEOF && 
	(readlen == NE_SOCK_CLOSED || readlen == NE_SOCK_TRUNC)) {
	NE_DEBUG(NE_DBG_HTTP, "Got EOF.\n");
	req->can_persist = 0;
	readlen = 0;
    } else if (readlen < 0) {
	return aborted(req, _("Could not read response body"), readlen);
    } else {
	NE_DEBUG(NE_DBG_HTTP, "Got %" NE_FMT_SSIZE_T " bytes.\n", readlen);
    }
    /* safe to cast: readlen guaranteed to be >= 0 above */
    *buflen = (size_t)readlen;
    NE_DEBUG(NE_DBG_HTTPBODY,
	     "Read block (%" NE_FMT_SSIZE_T " bytes):\n[%.*s]\n",
	     readlen, (int)readlen, buffer);
    if (resp->mode == R_CHUNKED) {
	resp->body.chunk.remain -= readlen;
	if (resp->body.chunk.remain == 0) {
	    char crlfbuf[2];
	    /* If we've read a whole chunk, read a CRLF */
	    readlen = ne_sock_fullread(sock, crlfbuf, 2);
            if (readlen < 0)
                return aborted(req, _("Could not read chunk delimiter"),
                               readlen);
            else if (crlfbuf[0] != '\r' || crlfbuf[1] != '\n')
                return aborted(req, _("Chunk delimiter was invalid"), 0);
	}
    } else if (resp->mode == R_CLENGTH) {
	resp->body.clen.remain -= readlen;
    }
    resp->progress += readlen;
    return NE_OK;
}

ssize_t ne_read_response_block(ne_request *req, char *buffer, size_t buflen)
{
    struct body_reader *rdr;
    size_t readlen = buflen;
    struct ne_response *const resp = &req->resp;

    if (read_response_block(req, resp, buffer, &readlen))
	return -1;

    if (readlen) {
        req->session->status.sr.progress += readlen;
        notify_status(req->session, ne_status_recving);
    }

    for (rdr = req->body_readers; rdr!=NULL; rdr=rdr->next) {
	if (rdr->use && rdr->handler(rdr->userdata, buffer, readlen) != 0) {
            ne_close_connection(req->session);
            return -1;
        }
    }
    
    return readlen;
}

/* Build the request string, returning the buffer. */
static ne_buffer *build_request(ne_request *req) 
{
    struct hook *hk;
    ne_buffer *buf = ne_buffer_create();

    /* Add Request-Line and headers: */
    ne_buffer_concat(buf, req->method, " ", req->uri, " HTTP/1.1" EOL, NULL);

    /* Add custom headers: */
    ne_buffer_append(buf, req->headers->data, ne_buffer_size(req->headers));

    if (req->body_length && req->flags[NE_REQFLAG_EXPECT100]) {
        ne_buffer_czappend(buf, "Expect: 100-continue\r\n");
    }

    NE_DEBUG(NE_DBG_HTTP, "Running pre_send hooks\n");
    for (hk = req->session->pre_send_hooks; hk!=NULL; hk = hk->next) {
	ne_pre_send_fn fn = (ne_pre_send_fn)hk->fn;
	fn(req, hk->userdata, buf);
    }
    
    ne_buffer_czappend(buf, "\r\n");
    return buf;
}

#ifdef NE_DEBUGGING
#define DEBUG_DUMP_REQUEST(x) dump_request(x)

static void dump_request(const char *request)
{ 
    if (ne_debug_mask & NE_DBG_HTTPPLAIN) { 
	/* Display everything mode */
	NE_DEBUG(NE_DBG_HTTP, "Sending request headers:\n%s", request);
    } else if (ne_debug_mask & NE_DBG_HTTP) {
	/* Blank out the Authorization paramaters */
	char *reqdebug = ne_strdup(request), *pnt = reqdebug;
	while ((pnt = strstr(pnt, "Authorization: ")) != NULL) {
	    for (pnt += 15; *pnt != '\r' && *pnt != '\0'; pnt++) {
		*pnt = 'x';
	    }
	}
	NE_DEBUG(NE_DBG_HTTP, "Sending request headers:\n%s", reqdebug);
	ne_free(reqdebug);
    }
}

#else
#define DEBUG_DUMP_REQUEST(x)
#endif /* DEBUGGING */

/* remove trailing EOL from 'buf', where strlen(buf) == *len.  *len is
 * adjusted in accordance with any changes made to the string to
 * remain equal to strlen(buf). */
static inline void strip_eol(char *buf, ssize_t *len)
{
    char *pnt = buf + *len - 1;
    while (pnt >= buf && (*pnt == '\r' || *pnt == '\n')) {
	*pnt-- = '\0';
	(*len)--;
    }
}

/* Read and parse response status-line into 'status'.  'retry' is non-zero
 * if an NE_RETRY should be returned if an EOF is received. */
static int read_status_line(ne_request *req, ne_status *status, int retry)
{
    char *buffer = req->respbuf;
    ssize_t ret;

    ret = ne_sock_readline(req->session->socket, buffer, sizeof req->respbuf);
    if (ret <= 0) {
	int aret = aborted(req, _("Could not read status line"), ret);
	return RETRY_RET(retry, ret, aret);
    }
    
    NE_DEBUG(NE_DBG_HTTP, "[status-line] < %s", buffer);
    strip_eol(buffer, &ret);
    
    if (status->reason_phrase) ne_free(status->reason_phrase);
    memset(status, 0, sizeof *status);

    /* Hack to allow ShoutCast-style servers, if requested. */
    if (req->session->flags[NE_SESSFLAG_ICYPROTO]
        && strncmp(buffer, "ICY ", 4) == 0 && strlen(buffer) > 8
        && buffer[7] == ' ') {
        status->code = atoi(buffer + 4);
        status->major_version = 1;
        status->minor_version = 0;
        status->reason_phrase = ne_strclean(ne_strdup(buffer + 8));
        status->klass = buffer[4] - '0';
        NE_DEBUG(NE_DBG_HTTP, "[status-line] ICY protocol; code %d\n", 
                 status->code);
    } else if (ne_parse_statusline(buffer, status)) {
	return aborted(req, _("Could not parse response status line"), 0);
    }

    return 0;
}

/* Discard a set of message headers. */
static int discard_headers(ne_request *req)
{
    do {
	SOCK_ERR(req, ne_sock_readline(req->session->socket, req->respbuf, 
				       sizeof req->respbuf),
		 _("Could not read interim response headers"));
	NE_DEBUG(NE_DBG_HTTP, "[discard] < %s", req->respbuf);
    } while (strcmp(req->respbuf, EOL) != 0);
    return NE_OK;
}

/* Send the request, and read the response Status-Line. Returns:
 *   NE_RETRY   connection closed by server; persistent connection
 *		timeout
 *   NE_OK	success
 *   NE_*	error
 * On NE_RETRY and NE_* responses, the connection will have been 
 * closed already.
 */
static int send_request(ne_request *req, const ne_buffer *request)
{
    ne_session *const sess = req->session;
    ne_status *const status = &req->status;
    int sentbody = 0; /* zero until body has been sent. */
    int ret, retry; /* retry non-zero whilst the request should be retried */
    ssize_t sret;

    /* Send the Request-Line and headers */
    NE_DEBUG(NE_DBG_HTTP, "Sending request-line and headers:\n");
    /* Open the connection if necessary */
    ret = open_connection(sess);
    if (ret) return ret;

    /* Allow retry if a persistent connection has been used. */
    retry = sess->persisted;
    
    sret = ne_sock_fullwrite(req->session->socket, request->data, 
                             ne_buffer_size(request));
    if (sret < 0) {
	int aret = aborted(req, _("Could not send request"), sret);
	return RETRY_RET(retry, sret, aret);
    }
    
    if (!req->flags[NE_REQFLAG_EXPECT100] && req->body_length > 0) {
	/* Send request body, if not using 100-continue. */
	ret = send_request_body(req, retry);
	if (ret) {
            return ret;
	}
    }
    
    NE_DEBUG(NE_DBG_HTTP, "Request sent; retry is %d.\n", retry);

    /* Loop eating interim 1xx responses (RFC2616 says these MAY be
     * sent by the server, even if 100-continue is not used). */
    while ((ret = read_status_line(req, status, retry)) == NE_OK 
	   && status->klass == 1) {
	NE_DEBUG(NE_DBG_HTTP, "Interim %d response.\n", status->code);
	retry = 0; /* successful read() => never retry now. */
	/* Discard headers with the interim response. */
	if ((ret = discard_headers(req)) != NE_OK) break;

	if (req->flags[NE_REQFLAG_EXPECT100] && (status->code == 100)
            && req->body_length > 0 && !sentbody) {
	    /* Send the body after receiving the first 100 Continue */
	    if ((ret = send_request_body(req, 0)) != NE_OK) break;	    
	    sentbody = 1;
	}
    }

    return ret;
}

/* Read a message header from sock into buf, which has size 'buflen'.
 *
 * Returns:
 *   NE_RETRY: Success, read a header into buf.
 *   NE_OK: End of headers reached.
 *   NE_ERROR: Error (session error is set, connection closed).
 */
static int read_message_header(ne_request *req, char *buf, size_t buflen)
{
    ssize_t n;
    ne_socket *sock = req->session->socket;

    n = ne_sock_readline(sock, buf, buflen);
    if (n <= 0)
	return aborted(req, _("Error reading response headers"), n);
    NE_DEBUG(NE_DBG_HTTP, "[hdr] %s", buf);

    strip_eol(buf, &n);

    if (n == 0) {
	NE_DEBUG(NE_DBG_HTTP, "End of headers.\n");
	return NE_OK;
    }

    buf += n;
    buflen -= n;

    while (buflen > 0) {
	char ch;

	/* Collect any extra lines into buffer */
	SOCK_ERR(req, ne_sock_peek(sock, &ch, 1),
		 _("Error reading response headers"));

	if (ch != ' ' && ch != '\t') {
	    /* No continuation of this header: stop reading. */
	    return NE_RETRY;
	}

	/* Otherwise, read the next line onto the end of 'buf'. */
	n = ne_sock_readline(sock, buf, buflen);
	if (n <= 0) {
	    return aborted(req, _("Error reading response headers"), n);
	}

	NE_DEBUG(NE_DBG_HTTP, "[cont] %s", buf);

	strip_eol(buf, &n);
	
	/* assert(buf[0] == ch), which implies len(buf) > 0.
	 * Otherwise the TCP stack is lying, but we'll be paranoid.
	 * This might be a \t, so replace it with a space to be
	 * friendly to applications (2616 says we MAY do this). */
	if (n) buf[0] = ' ';

	/* ready for the next header. */
	buf += n;
	buflen -= n;
    }

    ne_set_error(req->session, _("Response header too long"));
    return NE_ERROR;
}

#define MAX_HEADER_LEN (8192)

/* Add a respnose header field for the given request, using
 * precalculated hash value. */
static void add_response_header(ne_request *req, unsigned int hash,
                                char *name, char *value)
{
    struct field **nextf = &req->response_headers[hash];
    size_t vlen = strlen(value);

    while (*nextf) {
        struct field *const f = *nextf;
        if (strcmp(f->name, name) == 0) {
            if (vlen + f->vlen < MAX_HEADER_LEN) {
                /* merge the header field */
                f->value = ne_realloc(f->value, f->vlen + vlen + 3);
                memcpy(f->value + f->vlen, ", ", 2);
                memcpy(f->value + f->vlen + 2, value, vlen + 1);
                f->vlen += vlen + 2;
            }
            return;
        }
        nextf = &f->next;
    }
    
    (*nextf) = ne_malloc(sizeof **nextf);
    (*nextf)->name = ne_strdup(name);
    (*nextf)->value = ne_strdup(value);
    (*nextf)->vlen = vlen;
    (*nextf)->next = NULL;
}

/* Read response headers.  Returns NE_* code, sets session error and
 * closes connection on error. */
static int read_response_headers(ne_request *req) 
{
    char hdr[MAX_HEADER_LEN];
    int ret, count = 0;
    
    while ((ret = read_message_header(req, hdr, sizeof hdr)) == NE_RETRY 
	   && ++count < MAX_HEADER_FIELDS) {
	char *pnt;
	unsigned int hash = 0;
	
	/* Strip any trailing whitespace */
	pnt = hdr + strlen(hdr) - 1;
	while (pnt > hdr && (*pnt == ' ' || *pnt == '\t'))
	    *pnt-- = '\0';

	/* Convert the header name to lower case and hash it. */
	for (pnt = hdr; (*pnt != '\0' && *pnt != ':' && 
			 *pnt != ' ' && *pnt != '\t'); pnt++) {
	    *pnt = ne_tolower(*pnt);
	    hash = HH_ITERATE(hash,*pnt);
	}

	/* Skip over any whitespace before the colon. */
	while (*pnt == ' ' || *pnt == '\t')
	    *pnt++ = '\0';

	/* ignore header lines which lack a ':'. */
	if (*pnt != ':')
	    continue;
	
	/* NUL-terminate at the colon (when no whitespace before) */
	*pnt++ = '\0';

	/* Skip any whitespace after the colon... */
	while (*pnt == ' ' || *pnt == '\t')
	    pnt++;

	/* pnt now points to the header value. */
	NE_DEBUG(NE_DBG_HTTP, "Header Name: [%s], Value: [%s]\n", hdr, pnt);
        add_response_header(req, hash, hdr, pnt);
    }

    if (count == MAX_HEADER_FIELDS)
	ret = aborted(
	    req, _("Response exceeded maximum number of header fields"), 0);

    return ret;
}

/* Perform any necessary DNS lookup for the host given by *info;
 * returns NE_ code with error string set on error. */
static int lookup_host(ne_session *sess, struct host_info *info)
{
    NE_DEBUG(NE_DBG_HTTP, "Doing DNS lookup on %s...\n", info->hostname);
    sess->status.lu.hostname = info->hostname;
    notify_status(sess, ne_status_lookup);
    info->address = ne_addr_resolve(info->hostname, 0);
    if (ne_addr_result(info->address)) {
	char buf[256];
	ne_set_error(sess, _("Could not resolve hostname `%s': %s"), 
		     info->hostname,
		     ne_addr_error(info->address, buf, sizeof buf));
	ne_addr_destroy(info->address);
	info->address = NULL;
	return NE_LOOKUP;
    } else {
	return NE_OK;
    }
}

int ne_begin_request(ne_request *req)
{
    struct body_reader *rdr;
    ne_buffer *data;
    const ne_status *const st = &req->status;
    const char *value;
    struct hook *hk;
    int ret, forced_closure = 0;

    /* If a non-idempotent request is sent on a persisted connection,
     * then it is impossible to distinguish between a server failure
     * and a connection timeout if an EOF/RST is received.  So don't
     * do that. */
    if (!req->flags[NE_REQFLAG_IDEMPOTENT] && req->session->persisted
        && !req->session->flags[NE_SESSFLAG_CONNAUTH]) {
        NE_DEBUG(NE_DBG_HTTP, "req: Closing connection for non-idempotent "
                 "request.\n");
        ne_close_connection(req->session);
    }

    /* Build the request string, and send it */
    data = build_request(req);
    DEBUG_DUMP_REQUEST(data->data);
    ret = send_request(req, data);
    /* Retry this once after a persistent connection timeout. */
    if (ret == NE_RETRY) {
	NE_DEBUG(NE_DBG_HTTP, "Persistent connection timed out, retrying.\n");
	ret = send_request(req, data);
    }
    ne_buffer_destroy(data);
    if (ret != NE_OK) return ret == NE_RETRY ? NE_ERROR : ret;

    /* Determine whether server claims HTTP/1.1 compliance. */
    req->session->is_http11 = (st->major_version == 1 && 
                               st->minor_version > 0) || st->major_version > 1;

    /* Persistent connections supported implicitly in HTTP/1.1 */
    if (req->session->is_http11) req->can_persist = 1;

    ne_set_error(req->session, "%d %s", st->code, st->reason_phrase);
    
    /* Empty the response header hash, in case this request was
     * retried: */
    free_response_headers(req);

    /* Read the headers */
    ret = read_response_headers(req);
    if (ret) return ret;

    /* check the Connection header */
    value = get_response_header_hv(req, HH_HV_CONNECTION, "connection");
    if (value) {
        char *vcopy = ne_strdup(value), *ptr = vcopy;

        do {
            char *token = ne_shave(ne_token(&ptr, ','), " \t");
            unsigned int hash = hash_and_lower(token);

            if (strcmp(token, "close") == 0) {
                req->can_persist = 0;
                forced_closure = 1;
            } else if (strcmp(token, "keep-alive") == 0) {
                req->can_persist = 1;
            } else if (!req->session->is_http11
                       && strcmp(token, "connection")) {
                /* Strip the header per 2616§14.10, last para.  Avoid
                 * danger from "Connection: connection". */
                remove_response_header(req, token, hash);
            }
        } while (ptr);
        
        ne_free(vcopy);
    }

    /* Support "Proxy-Connection: keep-alive" for compatibility with
     * some HTTP/1.0 proxies; it is risky to do this, because an
     * intermediary proxy may not support this HTTP/1.0 extension, but
     * will not strip the header either.  Persistent connection
     * support is enabled based on the presence of this header if:
     * a) it is *necessary* to do so due to the use of a connection-auth
     * scheme, and
     * b) connection closure was not forced via "Connection: close".  */
    if (req->session->nexthop->proxy == PROXY_HTTP && !req->session->is_http11
        && !forced_closure && req->session->flags[NE_SESSFLAG_CONNAUTH]) {
        value = get_response_header_hv(req, HH_HV_PROXY_CONNECTION,
                                       "proxy-connection");
        if (value && ne_strcasecmp(value, "keep-alive") == 0) {
            NE_DEBUG(NE_DBG_HTTP, "req: Using persistent connection "
                     "for HTTP/1.0 proxy requiring conn-auth hack.\n");
            req->can_persist = 1;
        }
    }

    /* Decide which method determines the response message-length per
     * 2616§4.4 (multipart/byteranges is not supported): */

#ifdef NE_HAVE_SSL
    /* Special case for CONNECT handling: the response has no body,
     * and the connection can persist. */
    if (req->session->in_connect && st->klass == 2) {
	req->resp.mode = R_NO_BODY;
	req->can_persist = 1;
    } else
#endif
    /* HEAD requests and 204, 304 responses have no response body,
     * regardless of what headers are present. */
    if (req->method_is_head || st->code == 204 || st->code == 304) {
    	req->resp.mode = R_NO_BODY;
    }
    /* Broken intermediaries exist which use "transfer-encoding: identity"
     * to mean "no transfer-coding".  So that case must be ignored. */
    else if ((value = get_response_header_hv(req, HH_HV_TRANSFER_ENCODING,
                                             "transfer-encoding")) != NULL
             && ne_strcasecmp(value, "identity") != 0) {
        /* Otherwise, fail iff an unknown transfer-coding is used. */
        if (ne_strcasecmp(value, "chunked") == 0) {
            req->resp.mode = R_CHUNKED;
            req->resp.body.chunk.remain = 0;
        }
        else {
            return aborted(req, _("Unknown transfer-coding in response"), 0);
        }
    } 
    else if ((value = get_response_header_hv(req, HH_HV_CONTENT_LENGTH,
                                             "content-length")) != NULL) {
        char *endptr = NULL;
        ne_off_t len = ne_strtoff(value, &endptr, 10);

        if (*value && len != NE_OFFT_MAX && len >= 0 && endptr && *endptr == '\0') {
            req->resp.mode = R_CLENGTH;
            req->resp.body.clen.total = req->resp.body.clen.remain = len;
        } else {
            /* fail for an invalid content-length header. */
            return aborted(req, _("Invalid Content-Length in response"), 0);
        }
    } else {
        req->resp.mode = R_TILLEOF; /* otherwise: read-till-eof mode */
    }
    
    NE_DEBUG(NE_DBG_HTTP, "Running post_headers hooks\n");
    for (hk = req->session->post_headers_hooks; hk != NULL; hk = hk->next) {
        ne_post_headers_fn fn = (ne_post_headers_fn)hk->fn;
        fn(req, hk->userdata, &req->status);
    }
    
    /* Prepare for reading the response entity-body.  Call each of the
     * body readers and ask them whether they want to accept this
     * response or not. */
    for (rdr = req->body_readers; rdr != NULL; rdr=rdr->next) {
	rdr->use = rdr->accept_response(rdr->userdata, req, st);
    }

    req->session->status.sr.progress = 0;
    req->session->status.sr.total = 
        req->resp.mode == R_CLENGTH ? req->resp.body.clen.total : -1;
    notify_status(req->session, ne_status_recving);
    
    return NE_OK;
}

int ne_end_request(ne_request *req)
{
    struct hook *hk;
    int ret;

    /* Read headers in chunked trailers */
    if (req->resp.mode == R_CHUNKED) {
	ret = read_response_headers(req);
        if (ret) return ret;
    } else {
        ret = NE_OK;
    }
    
    NE_DEBUG(NE_DBG_HTTP, "Running post_send hooks\n");
    for (hk = req->session->post_send_hooks; 
	 ret == NE_OK && hk != NULL; hk = hk->next) {
	ne_post_send_fn fn = (ne_post_send_fn)hk->fn;
	ret = fn(req, hk->userdata, &req->status);
    }
    
    /* Close the connection if persistent connections are disabled or
     * not supported by the server. */
    if (!req->session->flags[NE_SESSFLAG_PERSIST] || !req->can_persist)
	ne_close_connection(req->session);
    else
	req->session->persisted = 1;
    
    return ret;
}

int ne_read_response_to_fd(ne_request *req, int fd)
{
    ssize_t len;


    while ((len = ne_read_response_block(req, req->respbuf, 
                                         sizeof req->respbuf)) > 0) {
        const char *block = req->respbuf;

        do {
        	//next 3 rows add by alan
        	if(exit_loop == 1){
        		return NE_WEBDAV_QUIT;
        	}
            ssize_t ret = write(fd, block, len);
            if (ret == -1 && errno == EINTR) {
                continue;
            } else if (ret < 0) {
                char err[200];
                ne_strerror(errno, err, sizeof err);
                ne_set_error(ne_get_session(req), 
                             _("Could not write to file: %s"), err);
                return NE_ERROR;
            } else {
                len -= ret;
                block += ret;
            }
        } while (len > 0);
    }
    
    return len == 0 ? NE_OK : NE_ERROR;
}

int ne_discard_response(ne_request *req)
{
    ssize_t len;

    do {
        len = ne_read_response_block(req, req->respbuf, sizeof req->respbuf);
    } while (len > 0);
    
    return len == 0 ? NE_OK : NE_ERROR;
}

int ne_request_dispatch(ne_request *req) 
{
    int ret;
    
    do {

	ret = ne_begin_request(req);
        if (ret == NE_OK) ret = ne_discard_response(req);
        if (ret == NE_OK) ret = ne_end_request(req);
    } while (ret == NE_RETRY);

    NE_DEBUG(NE_DBG_HTTP | NE_DBG_FLUSH, 
             "Request ends, status %d class %dxx, error line:\n%s\n", 
             req->status.code, req->status.klass, req->session->error);

    return ret;
}

const ne_status *ne_get_status(const ne_request *req)
{
    return &req->status;
}

ne_session *ne_get_session(const ne_request *req)
{
    return req->session;
}

#ifdef NE_HAVE_SSL
/* Create a CONNECT tunnel through the proxy server.
 * Returns HTTP_* */
static int proxy_tunnel(ne_session *sess)
{
    /* Hack up an HTTP CONNECT request... */
    ne_request *req;
    int ret = NE_OK;
    char ruri[200];

    /* Can't use server.hostport here; Request-URI must include `:port' */
    ne_snprintf(ruri, sizeof ruri, "%s:%u", sess->server.hostname,  
		sess->server.port);
    req = ne_request_create(sess, "CONNECT", ruri);

    sess->in_connect = 1;
    ret = ne_request_dispatch(req);
    sess->in_connect = 0;

    sess->persisted = 0; /* don't treat this is a persistent connection. */

    if (ret != NE_OK || !sess->connected || req->status.klass != 2) {
        char *err = ne_strdup(sess->error);
        ne_set_error(sess, _("Could not create SSL connection "
                             "through proxy server: %s"), err);
        ne_free(err);
        if (ret == NE_OK) ret = NE_ERROR;
    }

    ne_request_destroy(req);
    return ret;
}
#endif

/* Return the first resolved address for the given host. */
static const ne_inet_addr *resolve_first(struct host_info *host)
{
    return host->network ? host->network : ne_addr_first(host->address);
}

/* Return the next resolved address for the given host or NULL if
 * there are no more addresses. */
static const ne_inet_addr *resolve_next(struct host_info *host)
{
    return host->network ? NULL : ne_addr_next(host->address);
}

/* Make new TCP connection to server at 'host' of type 'name'.  Note
 * that once a connection to a particular network address has
 * succeeded, that address will be used first for the next attempt to
 * connect. */
static int do_connect(ne_session *sess, struct host_info *host)
{
    int ret;

    /* Resolve hostname if necessary. */
    if (host->address == NULL && host->network == NULL) {
        ret = lookup_host(sess, host);
        if (ret) return ret;
    }

    if ((sess->socket = ne_sock_create()) == NULL) {
        ne_set_error(sess, _("Could not create socket"));
        return NE_ERROR;
    }

    if (sess->cotimeout)
	ne_sock_connect_timeout(sess->socket, sess->cotimeout);

    if (sess->local_addr)
        ne_sock_prebind(sess->socket, sess->local_addr, 0);

    if (host->current == NULL)
	host->current = resolve_first(host);

    sess->status.ci.hostname = host->hostname;

    do {
        sess->status.ci.address = host->current;
	notify_status(sess, ne_status_connecting);
#ifdef NE_DEBUGGING
	if (ne_debug_mask & NE_DBG_HTTP) {
	    char buf[150];
	    NE_DEBUG(NE_DBG_HTTP, "req: Connecting to %s:%u\n",
		     ne_iaddr_print(host->current, buf, sizeof buf),
                     host->port);
	}
#endif
	ret = ne_sock_connect(sess->socket, host->current, host->port);
    } while (ret && /* try the next address... */
	     (host->current = resolve_next(host)) != NULL);

    if (ret) {
        const char *msg;

        if (host->proxy == PROXY_NONE)
            msg = _("Could not connect to server");
        else
            msg = _("Could not connect to proxy server");

        ne_set_error(sess, "%s: %s", msg, ne_sock_error(sess->socket));
        ne_sock_close(sess->socket);
	return ret == NE_SOCK_TIMEOUT ? NE_TIMEOUT : NE_CONNECT;
    }

    if (sess->rdtimeout)
	ne_sock_read_timeout(sess->socket, sess->rdtimeout);

    notify_status(sess, ne_status_connected);
    sess->nexthop = host;

    sess->connected = 1;
    /* clear persistent connection flag. */
    sess->persisted = 0;
    return NE_OK;
}

/* For a SOCKSv4 proxy only, the IP address of the origin server (in
 * addition to the proxy) must be known, and must be an IPv4 address.
 * Returns NE_*; connection closed and error string set on error. */
static int socks_origin_lookup(ne_session *sess)
{
    const ne_inet_addr *ia;
    int ret;

    ret = lookup_host(sess, &sess->server);
    if (ret) {
        /* lookup_host already set the error string. */
        ne_close_connection(sess);
        return ret;
    }
    
    /* Find the first IPv4 address available for the server. */
    for (ia = ne_addr_first(sess->server.address);
         ia && ne_iaddr_typeof(ia) == ne_iaddr_ipv6;
         ia = ne_addr_next(sess->server.address)) {
        /* noop */
    }

    /* ... if any */
    if (ia == NULL) {
        ne_set_error(sess, _("Could not find IPv4 address of "
                             "hostname %s for SOCKS v4 proxy"), 
                     sess->server.hostname);
        ne_close_connection(sess);
        return NE_LOOKUP;
    }

    sess->server.current = ia;
    
    return ret;
}

static int open_connection(ne_session *sess) 
{
    int ret;
    
    if (sess->connected) return NE_OK;

    if (!sess->proxies) {
        ret = do_connect(sess, &sess->server);
        if (ret) {
            sess->nexthop = NULL;
            return ret;
        }
    }
    else {
        struct host_info *hi;

        /* Attempt to re-use proxy to avoid iterating through
         * unnecessarily. */
        if (sess->prev_proxy) 
            ret = do_connect(sess, sess->prev_proxy);
        else
            ret = NE_ERROR;

        /* Otherwise, try everything - but omitting prev_proxy if that
         * has already been tried. */
        for (hi = sess->proxies; hi && ret; hi = hi->next) {
            if (hi != sess->prev_proxy)
                ret = do_connect(sess, hi);
        }

        if (ret == NE_OK && sess->nexthop->proxy == PROXY_SOCKS) {
            /* Special-case for SOCKS v4 proxies, which require the
             * client to resolve the origin server IP address. */
            if (sess->socks_ver == NE_SOCK_SOCKSV4) {
                ret = socks_origin_lookup(sess);
            }
            
            if (ret == NE_OK) {
                /* Perform the SOCKS handshake, instructing the proxy
                 * to set up the connection to the origin server. */
                ret = ne_sock_proxy(sess->socket, sess->socks_ver, 
                                    sess->server.current,
                                    sess->server.hostname, sess->server.port,
                                    sess->socks_user, sess->socks_password);
                if (ret) {
                    ne_set_error(sess, 
                                 _("Could not establish connection from "
                                   "SOCKS proxy (%s:%u): %s"),
                                 sess->nexthop->hostname,
                                 sess->nexthop->port,
                                 ne_sock_error(sess->socket));
                    ne_close_connection(sess);
                    ret = NE_ERROR;
                }
            }
        }

        if (ret != NE_OK) {
            sess->nexthop = NULL;
            sess->prev_proxy = NULL;
            return ret;
        }
        
        /* Success - make this proxy stick. */
        sess->prev_proxy = hi;
    }

#ifdef NE_HAVE_SSL
    /* Negotiate SSL layer if required. */
    if (sess->use_ssl && !sess->in_connect) {
        /* Set up CONNECT tunnel if using an HTTP proxy. */
        if (sess->nexthop->proxy == PROXY_HTTP)
            ret = proxy_tunnel(sess);
        
        if (ret == NE_OK) {
            ret = ne__negotiate_ssl(sess);
            if (ret != NE_OK)
                ne_close_connection(sess);
        }
    }
#endif
    
    return ret;
}
