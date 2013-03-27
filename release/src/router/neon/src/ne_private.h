/* 
   HTTP Request Handling
   Copyright (C) 1999-2009, Joe Orton <joe@manyfish.co.uk>

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

/* THIS IS NOT A PUBLIC INTERFACE. You CANNOT include this header file
 * from an application.  */
 
#ifndef NE_PRIVATE_H
#define NE_PRIVATE_H

#include "ne_request.h"
#include "ne_socket.h"
#include "ne_ssl.h"

struct host_info {
    /* Type of host represented: */
    enum proxy_type {
        PROXY_NONE = 0,
        PROXY_HTTP, /* an HTTP proxy */
        PROXY_SOCKS /* a SOCKS proxy */
    } proxy;
    unsigned int port;
    /* If hostname is non-NULL, host is identified by this hostname. */
    char *hostname, *hostport;
    /* If address is non-NULL, the result of resolving ->hostname. */
    ne_sock_addr *address;
    /* If current non-NULL, current network address used in ->address. */
    const ne_inet_addr *current;
    /* If override is non-NULL, the host is identified by this network
     * address. */
    const ne_inet_addr *network;
    struct host_info *next;
};

/* Store every registered callback in a generic container, and cast
 * the function pointer when calling it.  */
struct hook {
    void (*fn)(void);
    void *userdata;
    const char *id; /* non-NULL for accessors. */
    struct hook *next;
};

#define HAVE_HOOK(st,func) (st->hook->hooks->func != NULL)
#define HOOK_FUNC(st, func) (*st->hook->hooks->func)

/* Session support. */
struct ne_session_s {
    /* Connection information */
    ne_socket *socket;

    /* non-zero if connection has been established. */
    int connected;
    
    /* non-zero if connection has persisted beyond one request. */
    int persisted;

    int is_http11; /* >0 if connected server is known to be
		    * HTTP/1.1 compliant. */

    char *scheme;

    /* Server host details. */
    struct host_info server;
    /* Proxy host details, or NULL if not using a proxy. */
    struct host_info *proxies;
    /* Most recently used proxy server. */
    struct host_info *prev_proxy;

    /* Pointer to the active .server or .proxies as appropriate: */
    struct host_info *nexthop;

    /* Local address to which sockets should be bound. */
    const ne_inet_addr *local_addr;

    /* Settings */
    int use_ssl; /* whether a secure connection is required */
    int in_connect; /* doing a proxy CONNECT */
    int any_proxy_http; /* whether any configured proxy is an HTTP proxy */
    
    enum ne_sock_sversion socks_ver;
    char *socks_user, *socks_password;

    int flags[NE_SESSFLAG_LAST];

    ne_progress progress_cb;
    void *progress_ud;

    ne_notify_status notify_cb;
    void *notify_ud;

    int rdtimeout, cotimeout; /* read, connect timeouts. */

    struct hook *create_req_hooks, *pre_send_hooks, *post_send_hooks,
        *post_headers_hooks, *destroy_req_hooks, *destroy_sess_hooks, 
        *close_conn_hooks, *private;

    char *user_agent; /* full User-Agent: header field */

#ifdef NE_HAVE_SSL
    ne_ssl_client_cert *client_cert;
    ne_ssl_certificate *server_cert;
    ne_ssl_context *ssl_context;
    int ssl_cc_requested; /* set to non-zero if a client cert was
                           * requested during initial handshake, but
                           * none could be provided. */
#endif

    /* Server cert verification callback: */
    ne_ssl_verify_fn ssl_verify_fn;
    void *ssl_verify_ud;
    /* Client cert provider callback: */
    ne_ssl_provide_fn ssl_provide_fn;
    void *ssl_provide_ud;

    ne_session_status_info status;

    /* Error string */
    char error[512];
};

/* Pushes block of 'count' bytes at 'buf'. Returns non-zero on
 * error. */
typedef int (*ne_push_fn)(void *userdata, const char *buf, size_t count);

/* Do the SSL negotiation. */
NE_PRIVATE int ne__negotiate_ssl(ne_session *sess);

/* Set the session error appropriate for SSL verification failures. */
NE_PRIVATE void ne__ssl_set_verify_err(ne_session *sess, int failures);

/* Return non-zero if hostname from certificate (cn) matches hostname
 * used for session (hostname); follows RFC2818 logic. */
NE_PRIVATE int ne__ssl_match_hostname(const char *cn, size_t cnlen, 
                                      const char *hostname);

#endif /* HTTP_PRIVATE_H */
