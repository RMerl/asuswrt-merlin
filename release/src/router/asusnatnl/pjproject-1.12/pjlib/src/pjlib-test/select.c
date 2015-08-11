/* $Id: select.c 3553 2011-05-05 06:14:19Z nanang $ */
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

/**
 * \page page_pjlib_select_test Test: Socket Select()
 *
 * This file provides implementation of \b select_test(). It tests the
 * functionality of the pj_sock_select() API.
 *
 *
 * This file is <b>pjlib-test/select.c</b>
 *
 * \include pjlib-test/select.c
 */


#if INCLUDE_SELECT_TEST

#include <pj/sock.h>
#include <pj/sock_select.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/assert.h>
#include <pj/os.h>
#include <pj/errno.h>

enum
{
    READ_FDS,
    WRITE_FDS,
    EXCEPT_FDS
};

#define UDP_PORT    51232
#define THIS_FILE   "select_test"

/*
 * do_select()
 *
 * Perform pj_sock_select() and find out which sockets
 * are signalled.
 */    
static int do_select( pj_sock_t sock1, pj_sock_t sock2,
		      int setcount[])
{
    pj_fd_set_t fds[3];
    pj_time_val timeout;
    int i, n;
    
    for (i=0; i<3; ++i) {
        PJ_FD_ZERO(&fds[i]);
        PJ_FD_SET(sock1, &fds[i]);
        PJ_FD_SET(sock2, &fds[i]);
        setcount[i] = 0;
    }

    timeout.sec = 1;
    timeout.msec = 0;

    n = pj_sock_select(PJ_IOQUEUE_MAX_HANDLES, &fds[0], &fds[1], &fds[2],
		       &timeout);
    if (n < 0)
        return n;
    if (n == 0)
        return 0;

    for (i=0; i<3; ++i) {
        if (PJ_FD_ISSET(sock1, &fds[i]))
            setcount[i]++;
        if (PJ_FD_ISSET(sock2, &fds[i]))
	    setcount[i]++;
    }

    return n;
}

/*
 * select_test()
 *
 * Test main entry.
 */
int select_test()
{
    pj_sock_t udp1=PJ_INVALID_SOCKET, udp2=PJ_INVALID_SOCKET;
    pj_sockaddr_in udp_addr;
    int status;
    int setcount[3];
    pj_str_t s;
    const char data[] = "hello";
    const int datalen = 5;
    pj_ssize_t sent, received;
    char buf[10];
    pj_status_t rc;

    PJ_LOG(3, (THIS_FILE, "...Testing simple UDP select()"));
    
    // Create two UDP sockets.
    rc = pj_sock_socket( pj_AF_INET(), pj_SOCK_DGRAM(), 0, &udp1);
    if (rc != PJ_SUCCESS) {
        app_perror("...error: unable to create socket", rc);
	status=-10; goto on_return;
    }
    rc = pj_sock_socket( pj_AF_INET(), pj_SOCK_DGRAM(), 0, &udp2);
    if (udp2 == PJ_INVALID_SOCKET) {
        app_perror("...error: unable to create socket", rc);
	status=-20; goto on_return;
    }

    // Bind one of the UDP socket.
    pj_bzero(&udp_addr, sizeof(udp_addr));
    udp_addr.sin_family = pj_AF_INET();
    udp_addr.sin_port = UDP_PORT;
    udp_addr.sin_addr = pj_inet_addr(pj_cstr(&s, "127.0.0.1"));

    if (pj_sock_bind(udp2, &udp_addr, sizeof(udp_addr))) {
	status=-30; goto on_return;
    }

    // Send data.
    sent = datalen;
    rc = pj_sock_sendto(udp1, data, &sent, 0, &udp_addr, sizeof(udp_addr));
    if (rc != PJ_SUCCESS || sent != datalen) {
        app_perror("...error: sendto() error", rc);
	status=-40; goto on_return;
    }

    // Sleep a bit. See http://trac.pjsip.org/repos/ticket/890
    pj_thread_sleep(10);

    // Check that socket is marked as reable.
    // Note that select() may also report that sockets are writable.
    status = do_select(udp1, udp2, setcount);
    if (status < 0) {
	char errbuf[128];
        pj_strerror(pj_get_netos_error(), errbuf, sizeof(errbuf));
	PJ_LOG(1,(THIS_FILE, "...error: %s", errbuf));
	status=-50; goto on_return;
    }
    if (status == 0) {
	status=-60; goto on_return;
    }

    if (setcount[READ_FDS] != 1) {
	status=-70; goto on_return;
    }
    if (setcount[WRITE_FDS] != 0) {
	if (setcount[WRITE_FDS] == 2) {
	    PJ_LOG(3,(THIS_FILE, "...info: system reports writable sockets"));
	} else {
	    status=-80; goto on_return;
	}
    } else {
	PJ_LOG(3,(THIS_FILE, 
		  "...info: system doesn't report writable sockets"));
    }
    if (setcount[EXCEPT_FDS] != 0) {
	status=-90; goto on_return;
    }

    // Read the socket to clear readable sockets.
    received = sizeof(buf);
    rc = pj_sock_recv(udp2, buf, &received, 0);
    if (rc != PJ_SUCCESS || received != 5) {
	status=-100; goto on_return;
    }
    
    status = 0;

    // Test timeout on the read part.
    // This won't necessarily return zero, as select() may report that
    // sockets are writable.
    setcount[0] = setcount[1] = setcount[2] = 0;
    status = do_select(udp1, udp2, setcount);
    if (status != 0 && status != setcount[WRITE_FDS]) {
	PJ_LOG(3,(THIS_FILE, "...error: expecting timeout but got %d sks set",
			     status));
	PJ_LOG(3,(THIS_FILE, "          rdset: %d, wrset: %d, exset: %d",
			     setcount[0], setcount[1], setcount[2]));
	status = -110; goto on_return;
    }
    if (setcount[READ_FDS] != 0) {
	PJ_LOG(3,(THIS_FILE, "...error: readable socket not expected"));
	status = -120; goto on_return;
    }

    status = 0;

on_return:
    if (udp1 != PJ_INVALID_SOCKET)
	pj_sock_close(udp1);
    if (udp2 != PJ_INVALID_SOCKET)
	pj_sock_close(udp2);
    return status;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_select_test;
#endif	/* INCLUDE_SELECT_TEST */


