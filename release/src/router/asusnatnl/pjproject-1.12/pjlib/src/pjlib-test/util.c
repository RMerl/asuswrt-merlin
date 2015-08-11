/* $Id: util.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include "test.h"
#include <pjlib.h>

#define THIS_FILE "util.c"

void app_perror(const char *msg, pj_status_t rc)
{
    char errbuf[PJ_ERR_MSG_SIZE];

    PJ_CHECK_STACK();

    pj_strerror(rc, errbuf, sizeof(errbuf));
    PJ_LOG(3,("test", "%s: [pj_status_t=%d] %s", msg, rc, errbuf));
}

#define SERVER 0
#define CLIENT 1

pj_status_t app_socket(int family, int type, int proto, int port,
                       pj_sock_t *ptr_sock)
{
    pj_sockaddr_in addr;
    pj_sock_t sock;
    pj_status_t rc;

    rc = pj_sock_socket(family, type, proto, &sock);
    if (rc != PJ_SUCCESS)
        return rc;

    pj_bzero(&addr, sizeof(addr));
    addr.sin_family = (pj_uint16_t)family;
    addr.sin_port = (short)(port!=-1 ? pj_htons((pj_uint16_t)port) : 0);
    rc = pj_sock_bind(sock, &addr, sizeof(addr));
    if (rc != PJ_SUCCESS)
        return rc;
    
#if PJ_HAS_TCP
    if (type == pj_SOCK_STREAM()) {
        rc = pj_sock_listen(sock, 5);
        if (rc != PJ_SUCCESS)
            return rc;
    }
#endif

    *ptr_sock = sock;
    return PJ_SUCCESS;
}

pj_status_t app_socketpair(int family, int type, int protocol,
                           pj_sock_t *serverfd, pj_sock_t *clientfd)
{
    int i;
    static unsigned short port = 11000;
    pj_sockaddr_in addr;
    pj_str_t s;
    pj_status_t rc = 0;
    pj_sock_t sock[2];

    /* Create both sockets. */
    for (i=0; i<2; ++i) {
        rc = pj_sock_socket(family, type, protocol, &sock[i]);
        if (rc != PJ_SUCCESS) {
            if (i==1)
                pj_sock_close(sock[0]);
            return rc;
        }
    }

    /* Retry bind */
    pj_bzero(&addr, sizeof(addr));
    addr.sin_family = pj_AF_INET();
    for (i=0; i<5; ++i) {
        addr.sin_port = pj_htons(port++);
        rc = pj_sock_bind(sock[SERVER], &addr, sizeof(addr));
        if (rc == PJ_SUCCESS)
            break;
    }

    if (rc != PJ_SUCCESS)
        goto on_error;

    /* For TCP, listen the socket. */
#if PJ_HAS_TCP
    if (type == pj_SOCK_STREAM()) {
        rc = pj_sock_listen(sock[SERVER], PJ_SOMAXCONN);
        if (rc != PJ_SUCCESS)
            goto on_error;
    }
#endif

    /* Connect client socket. */
    addr.sin_addr = pj_inet_addr(pj_cstr(&s, "127.0.0.1"));
    rc = pj_sock_connect(sock[CLIENT], &addr, sizeof(addr));
    if (rc != PJ_SUCCESS)
        goto on_error;

    /* For TCP, must accept(), and get the new socket. */
#if PJ_HAS_TCP
    if (type == pj_SOCK_STREAM()) {
        pj_sock_t newserver;

        rc = pj_sock_accept(sock[SERVER], &newserver, NULL, NULL);
        if (rc != PJ_SUCCESS)
            goto on_error;

        /* Replace server socket with new socket. */
        pj_sock_close(sock[SERVER]);
        sock[SERVER] = newserver;
    }
#endif

    *serverfd = sock[SERVER];
    *clientfd = sock[CLIENT];

    return rc;

on_error:
    for (i=0; i<2; ++i)
        pj_sock_close(sock[i]);
    return rc;
}
