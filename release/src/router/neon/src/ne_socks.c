/*
   SOCKS proxy support for neon
   Copyright (C) 2008, Joe Orton <joe@manyfish.co.uk>

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

#include "config.h"

#include "ne_internal.h"
#include "ne_string.h"
#include "ne_socket.h"
#include "ne_utils.h"

#include <string.h>

/* SOCKS protocol reference:
   v4:  http://www.ufasoft.com/doc/socks4_protocol.htm
   v4a  http://www.smartftp.com/Products/SmartFTP/RFC/socks4a.protocol
   v5:  http://tools.ietf.org/html/rfc1928
   ...v5 auth: http://tools.ietf.org/html/rfc1929
*/

#define V5_REPLY_OK 0
#define V5_REPLY_FAIL 1
#define V5_REPLY_DISALLOW 2
#define V5_REPLY_NET_UNREACH 3
#define V5_REPLY_HOST_UNREACH 4
#define V5_REPLY_CONN_REFUSED 5
#define V5_REPLY_TTL_EXPIRED 6
#define V5_REPLY_CMD_UNSUPPORTED 7
#define V5_REPLY_TYPE_UNSUPPORTED 8

#define V5_VERSION   0x05
#define V5_ADDR_IPV4 0x01
#define V5_ADDR_FQDN 0x03
#define V5_ADDR_IPV6 0x04

#define V5_CMD_CONNECT 0x01

#define V5_AUTH_NONE   0x00
#define V5_AUTH_USER   0x02
#define V5_AUTH_NOMETH 0xFF

/* Fail with given V5 error code in given context. */
static int v5fail(ne_socket *sock, unsigned int code, const char *context)
{
    const char *err;

    switch (code) {
    case V5_REPLY_FAIL:
        err = _("failure");
        break;
    case V5_REPLY_DISALLOW:
        err = _("connection not permitted"); 
        break;
    case V5_REPLY_NET_UNREACH:
        err = _("network unreachable");
        break;
    case V5_REPLY_HOST_UNREACH: 
        err = _("host unreachable");
        break;
    case V5_REPLY_TTL_EXPIRED:
        err = _("TTL expired");
        break;
    case V5_REPLY_CMD_UNSUPPORTED:
        err = _("command not supported");
        break;
    case V5_REPLY_TYPE_UNSUPPORTED: 
        err = _("address type not supported");
        break;
    default:
        ne_sock_set_error(sock, _("%s: unrecognized error (%u)"), context, code);
        return NE_SOCK_ERROR;
    }
    
    ne_sock_set_error(sock, "%s: %s", context, err);
    return NE_SOCK_ERROR;
}

/* Fail with given error string. */
static int fail(ne_socket *sock, const char *error)
{
    ne_sock_set_error(sock, "%s", error);
    return NE_SOCK_ERROR;
}

/* Fail with given NE_SOCK_* error code and given context. */
static int sofail(ne_socket *sock, ssize_t ret, const char *context)
{
    char *err = ne_strdup(ne_sock_error(sock));
    ne_sock_set_error(sock, "%s: %s", context, err);
    ne_free(err);
    return NE_SOCK_ERROR;
}

/* SOCKSv5 proxy. */
static int v5_proxy(ne_socket *sock, const ne_inet_addr *addr,
                    const char *hostname, unsigned int port,
                    const char *username, const char *password)
{
    unsigned char msg[1024], *p;
    unsigned int len;
    int ret;
    ssize_t n;

    p = msg;
    *p++ = V5_VERSION;
    *p++ = 2; /* Two supported auth protocols; none and user. */
    *p++ = V5_AUTH_NONE;
    *p++ = V5_AUTH_USER;

    ret = ne_sock_fullwrite(sock, (char *)msg, p - msg);
    if (ret) {
        return sofail(sock, ret, _("Could not send message to proxy"));
    }

    n = ne_sock_fullread(sock, (char *)msg, 2);
    if (n) {
        return sofail(sock, ret, _("Could not read initial response from proxy"));
    }
    else if (msg[0] != V5_VERSION) {
        return fail(sock, _("Invalid version in proxy response"));
    }
    
    /* Authenticate, if necessary. */
    switch (msg[1]) {
    case V5_AUTH_NONE:
        break;
    case V5_AUTH_USER:
        p = msg;
        *p++ = 0x01;
        len = strlen(username) & 0xff;
        *p++ = len;
        memcpy(p, username, len);
        p += len;
        len = strlen(password) & 0xff;
        *p++ = len;
        memcpy(p, password, len);
        p += len;

        ret = ne_sock_fullwrite(sock, (char *)msg, p - msg);
        if (ret) {
            return sofail(sock, ret, _("Could not send login message"));
        }
        
        n = ne_sock_fullread(sock, (char *)msg, 2);
        if (n) {
            return sofail(sock, ret, _("Could not read login reply"));
        }
        else if (msg[0] != 1) {
            return fail(sock, _("Invalid version in login reply"));
        }
        else if (msg[1] != 0) {
            return fail(sock, _("Authentication failed"));
        }
        break;
    case V5_AUTH_NOMETH:
        return fail(sock, _("No acceptable authentication method"));
    default:
        return fail(sock, _("Unexpected authentication method chosen"));
    }
    
    /* Send the CONNECT command. */
    p = msg;
    *p++ = V5_VERSION;
    *p++ = V5_CMD_CONNECT;
    *p++ = 0; /* reserved */
    if (addr) {
        unsigned char raw[16];

        if (ne_iaddr_typeof(addr) == ne_iaddr_ipv4) {
            len = 4;
            *p++ = V5_ADDR_IPV4;
        }
        else {
            len = 16;
            *p++ = V5_ADDR_IPV6;
        }
        
        memcpy(p, ne_iaddr_raw(addr, raw), len);
        p += len;
    }
    else {
        len = strlen(hostname) & 0xff;
        *p++ = V5_ADDR_FQDN;
        *p++ = len;
        memcpy(p, hostname, len);
        p += len;
    }

    *p++ = (port >> 8) & 0xff;
    *p++ = port & 0xff;

    ret = ne_sock_fullwrite(sock, (char *)msg, p - msg);
    if (ret) {
        return sofail(sock, ret, _("Could not send connect request"));
    }

    n = ne_sock_fullread(sock, (char *)msg, 4);
    if (n) {
        return sofail(sock, n, _("Could not read connect reply"));
    }
    if (msg[0] != V5_VERSION) {
        return fail(sock, _("Invalid version in connect reply"));
    }
    if (msg[1] != V5_REPLY_OK) {
        return v5fail(sock, msg[1], _("Could not connect"));
    }
    
    switch (msg[3]) {
    case V5_ADDR_IPV4:
        len = 4;
        break;
    case V5_ADDR_IPV6:
        len = 16;
        break;
    case V5_ADDR_FQDN:
        n = ne_sock_read(sock, (char *)msg, 1);
        if (n != 1) {
            return sofail(sock, n, 
                            _("Could not read FQDN length in connect reply"));
        }
        len = msg[0];
        break;
    default:
        return fail(sock, _("Unknown address type in connect reply"));
    }

    n = ne_sock_fullread(sock, (char *)msg, len + 2);
    if (n) {
        return sofail(sock, n, _("Could not read address in connect reply"));
    }

    return 0;
}

#define V4_VERSION 0x04
#define V4_CMD_STREAM 0x01

#define V4_REP_OK      0x5a /* request granted */
#define V4_REP_FAIL    0x5b /* request rejected or failed */
#define V4_REP_NOIDENT 0x5c /* request failed, could connect to identd */
#define V4_REP_IDFAIL  0x5d /* request failed, identd denial */

/* Fail for given SOCKSv4 error code. */
static int v4fail(ne_socket *sock, unsigned int code, const char *context)
{
    const char *err;

    switch (code) {
    case V4_REP_FAIL:
        err = _("request rejected or failed");
        break;
    case V4_REP_NOIDENT:
        err = _("could not establish connection to identd");
        break;
    case V4_REP_IDFAIL:
        err = _("rejected due to identd user mismatch");
        break;
    default:
        ne_sock_set_error(sock, _("%s: unrecognized failure (%u)"),
                          context, code);
        return NE_SOCK_ERROR;
    }
    
    ne_sock_set_error(sock, "%s: %s", context, err);
    return NE_SOCK_ERROR;
}

/* SOCKS v4 or v4A proxy. */
static int v4_proxy(ne_socket *sock, enum ne_sock_sversion vers,
                    const ne_inet_addr *addr, const char *hostname, 
                    unsigned int port, const char *username)
{
    unsigned char msg[1024], raw[16], *p;
    ssize_t n;
    int ret;

    p = msg;
    *p++ = V4_VERSION;
    *p++ = V4_CMD_STREAM;
    *p++ = (port >> 8) & 0xff;
    *p++ = port & 0xff;

    if (vers == NE_SOCK_SOCKSV4A) {
        /* A bogus address is used to signify use of the hostname,
         * 0.0.0.X where X != 0. */
        memcpy(p, "\x00\x00\x00\xff", 4);
    } 
    else {
        /* API precondition that addr is IPv4; if it's not this will
         * just copy out the first four bytes of the v6 address;
         * garbage in => garbage out. */
        memcpy(p, ne_iaddr_raw(addr, raw), 4);
    }
    p += 4;

    if (username) {
        unsigned int len = strlen(username) & 0xff;
        memcpy(p, username, len);
        p += len;
    }
    *p++ = '\0';
    
    if (vers == NE_SOCK_SOCKSV4A) {
        unsigned int len = strlen(hostname) & 0xff;
        memcpy(p, hostname, len);
        p += len;
        *p++ = '\0';
    }    

    ret = ne_sock_fullwrite(sock, (char *)msg, p - msg);
    if (ret) {
        return sofail(sock, ret, _("Could not send message to proxy"));
    }

    n = ne_sock_fullread(sock, (char *)msg, 8);
    if (n) {
        return sofail(sock, ret, _("Could not read response from proxy"));
    }
    
    if (msg[1] != V4_REP_OK) {
        return v4fail(sock, ret, _("Could not connect"));
    }

    return 0;
}

int ne_sock_proxy(ne_socket *sock, enum ne_sock_sversion vers,
                  const ne_inet_addr *addr, const char *hostname, 
                  unsigned int port,
                  const char *username, const char *password)
{
    if (vers == NE_SOCK_SOCKSV5) {
        return v5_proxy(sock, addr, hostname, port, username, password);
    }
    else {
        return v4_proxy(sock, vers, addr, hostname, port, username);
    }
}
