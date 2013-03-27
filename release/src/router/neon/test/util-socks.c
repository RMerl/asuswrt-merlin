/* 
   SOCKS server utils.
   Copyright (C) 2008, 2009, Joe Orton <joe@manyfish.co.uk>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "config.h"

#include <sys/types.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <time.h> /* for time() */

#include "ne_socket.h"
#include "ne_utils.h"
#include "ne_alloc.h"

#include "child.h"
#include "tests.h"
#include "utils.h"

#define V5_METH_NONE 0x00
#define V5_METH_AUTH 0x02
#define V5_ADDR_IPV4 0x01
#define V5_ADDR_FQDN 0x03
#define V5_ADDR_IPV6 0x04

static int read_socks_string(ne_socket *sock, const char *ctx,
                             unsigned char *buf, unsigned int *olen)
{
    unsigned char len;
    ssize_t ret;

    ret = ne_sock_read(sock, (char *)&len, 1);
    ONV(ret != 1, ("%s length read failed: %s", ctx, ne_sock_error(sock)));

    ONV(len == 0, ("%s gave zero-length string", ctx));

    ret = ne_sock_fullread(sock, (char *)buf, len);
    ONV(ret != 0, ("%s string read failed, got %" NE_FMT_SSIZE_T 
                   " bytes (%s)", ctx, ret, ne_sock_error(sock)));
    
    *olen = len;

    return OK;    
}

static int read_socks_byte(ne_socket *sock, const char *ctx,
                           unsigned char *buf)
{
    ONV(ne_sock_read(sock, (char *)buf, 1) != 1, 
        ("%s byte read failed: %s", ctx, ne_sock_error(sock)));
    return OK;    
}

static int expect_socks_byte(ne_socket *sock, const char *ctx,
                             unsigned char c)
{
    unsigned char b;

    CALL(read_socks_byte(sock, ctx, &b));

    ONV(b != c, ("%s got byte %hx not %hx", ctx, b, c));
    
    return OK;
}    

static int read_socks_0string(ne_socket *sock, const char *ctx,
                              unsigned char *buf, unsigned *len)
{
    unsigned char *end = buf + *len, *p = buf;

    while (p < end) {
        CALL(read_socks_byte(sock, ctx, p));

        if (*p == '\0')
            break;
        p++;
        
    } 

    *len = p - buf;

    return OK;
}

int socks_server(ne_socket *sock, void *userdata)
{
    struct socks_server *srv = userdata;
    unsigned char buf[1024];
    unsigned int len, port, version;
    unsigned char atype;
    ssize_t ret;

    version = srv->version == NE_SOCK_SOCKSV5 ? 5 : 4;

    ne_sock_read_timeout(sock, 5);

    CALL(expect_socks_byte(sock, "client version", version));

    if (version != 5) {
        unsigned char raw[16];

        CALL(expect_socks_byte(sock, "v4 command", 0x01));

        ret = ne_sock_fullread(sock, (char *)buf, 6);
        ONV(ret != 0,
            ("v4 address read failed with %" NE_FMT_SSIZE_T
             " (%s)", ret, ne_sock_error(sock)));

        ONN("bad v4A bogus address",
            srv->version == NE_SOCK_SOCKSV4A && srv->expect_addr == NULL
            && memcmp(buf + 2, "\0\0\0", 3) != 0 && buf[6] != 0);

        ONN("v4 server with no expected address! fail",
            srv->version == NE_SOCK_SOCKSV4 && srv->expect_addr == NULL);

        if (srv->expect_addr) {
            ONN("v4 address mismatch", 
                memcmp(ne_iaddr_raw(srv->expect_addr, raw), buf + 2, 4) != 0);        
        }

        port = (buf[0] << 8) | buf[1];
        ONV(port != srv->expect_port,
            ("got bad v4 port %u, expected %u", port, srv->expect_port));
        
        len = sizeof buf;
        CALL(read_socks_0string(sock, "v4 username read", buf, &len));

        ONV(srv->username == NULL && len, ("unexpected v4 username %s", buf));
        ONV(srv->username && !len, 
            ("no v4 username given, expected %s", srv->username));
        ONV(srv->username && len && strcmp(srv->username, (char *)buf),
            ("bad v4 username, expected %s got %s", srv->username, buf));
        
        if (srv->expect_addr == NULL) {
            len = sizeof buf;
            CALL(read_socks_0string(sock, "v4A hostname read", buf, &len));
            ONV(strcmp(srv->expect_fqdn, (char *)buf) != 0,
                ("bad v4A hostname: %s not %s", buf, srv->expect_fqdn));
        }

        { 
            static const char msg[] = "\x00\x5A"
                "\x00\x00" "\x00\x00\x00\x00"
                "ok!\n";
        
            if (srv->say_hello)
                CALL(full_write(sock, msg, 12));
            else
                CALL(full_write(sock, msg, 8));
        }
    
        return srv->server(sock, srv->userdata);
    }

    CALL(read_socks_string(sock, "client method list", buf, &len));

    if (srv->failure == fail_init_vers) {
        CALL(full_write(sock, "\x01\x02", 2));
        return OK;
    }
    else if (srv->failure == fail_init_close) {
        return OK;
    }
    else if (srv->failure == fail_init_trunc) {
        CALL(full_write(sock, "\x05", 1));
        return OK;
    }
    else if (srv->failure == fail_no_auth) {
        CALL(full_write(sock, "\x05\xff", 2));
        return OK;
    }
    else if (srv->failure == fail_bogus_auth) {
        CALL(full_write(sock, "\x05\xfe", 2));
        return OK;
    }

    ONN("client did not advertise no-auth method",
        memchr(buf, V5_METH_NONE, len) == NULL);
    
    if (srv->username) {
        int match = 0;
         
        ONN("client did not advertise authn method",
            memchr(buf, V5_METH_AUTH, len) == NULL);
        
        CALL(full_write(sock, "\x05\x02", 2));
        
        CALL(expect_socks_byte(sock, "client auth version", 0x01));

        CALL(read_socks_string(sock, "client username", buf, &len));
        
        match = len == strlen(srv->username)
            && memcmp(buf, srv->username, len) == 0;
        
        CALL(read_socks_string(sock, "client password", buf, &len));
        
        match = match && len == strlen(srv->password)
            && memcmp(buf, srv->password, len) == 0;

        if (srv->failure == fail_auth_close) {
            return OK;
        }

        if (match && srv->failure != fail_auth_denied) {
            CALL(full_write(sock, "\x01\x00", 2));
        }
        else {
            CALL(full_write(sock, "\x01\x01", 2));
        }
        
        if (srv->failure == fail_auth_denied) {
            return OK;
        }
    }
    else {
        CALL(full_write(sock, "\x05\x00", 2));
    }
    
    CALL(expect_socks_byte(sock, "command version", version));

    CALL(expect_socks_byte(sock, "command number", 0x01));
    CALL(read_socks_byte(sock, "reserved byte", buf));

    CALL(read_socks_byte(sock, "address type", &atype));

    ONN("bad address type byte", 
        (atype != V5_ADDR_IPV4 && atype != V5_ADDR_IPV6
         && atype != V5_ADDR_FQDN));

    if (atype == V5_ADDR_FQDN) {
        ONN("unexpected FQDN from client", srv->expect_fqdn == NULL);
        CALL(read_socks_string(sock, "read FQDN", buf, &len));
        ONV(len != strlen(srv->expect_fqdn)
            || memcmp(srv->expect_fqdn, buf, len) != 0,
            ("FQDN mismatch: %.*s not %s", len, buf, 
             srv->expect_fqdn));
    }
    else {
        unsigned char raw[16];

        ONN("unexpected IP literal from client", srv->expect_addr == NULL);

        ONV((atype == V5_ADDR_IPV4
             && ne_iaddr_typeof(srv->expect_addr) != ne_iaddr_ipv4)
            || (atype == V5_ADDR_IPV6
                && ne_iaddr_typeof(srv->expect_addr) != ne_iaddr_ipv6),
            ("address type mismatch: %hx not %d",
             atype, ne_iaddr_typeof(srv->expect_addr)));

        len = atype == V5_ADDR_IPV4 ? 4 : 16;
        ret = ne_sock_fullread(sock, (char *)buf, len);
        ONV(ret != 0,
            ("address read failed with %" NE_FMT_SSIZE_T
             " (%s)", ret, ne_sock_error(sock)));

        ne_iaddr_raw(srv->expect_addr, raw);
        
        ONN("address mismatch", memcmp(raw, buf, len) != 0);        
    }

    CALL(read_socks_byte(sock, "port high byte", buf));
    CALL(read_socks_byte(sock, "port low byte", buf + 1));
    
    port = (buf[0] << 8) | buf[1];
    ONV(port != srv->expect_port,
        ("got bad port %u, expected %u", port, srv->expect_port));

    {
        static const char msg[] = 
            "\x05\x00\x00"
            "\x01" "\x00\x00\x00\x00"
            "\x00\x00"
            "ok!\n";

        if (srv->say_hello)
            CALL(full_write(sock, msg, 14));
        else
            CALL(full_write(sock, msg, 10));
    }
        
    
    return srv->server(sock, srv->userdata);
}
