/* $Id: ioq_tcp.c 4387 2013-02-27 10:16:08Z ming $ */
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
 * \page page_pjlib_ioqueue_tcp_test Test: I/O Queue (TCP)
 *
 * This file provides implementation to test the
 * functionality of the I/O queue when TCP socket is used.
 *
 *
 * This file is <b>pjlib-test/ioq_tcp.c</b>
 *
 * \include pjlib-test/ioq_tcp.c
 */


#if INCLUDE_TCP_IOQUEUE_TEST

#include <pjlib.h>

#if PJ_HAS_TCP

#define THIS_FILE	    "test_tcp"
#define NON_EXISTANT_PORT   50123
#define LOOP		    100
#define BUF_MIN_SIZE	    32
#define BUF_MAX_SIZE	    2048
#define SOCK_INACTIVE_MIN   (4-2)
#define SOCK_INACTIVE_MAX   (PJ_IOQUEUE_MAX_HANDLES - 2)
#define POOL_SIZE	    (2*BUF_MAX_SIZE + SOCK_INACTIVE_MAX*128 + 2048)

static pj_ssize_t	     callback_read_size,
                             callback_write_size,
                             callback_accept_status,
                             callback_connect_status;
static unsigned		     callback_call_count;
static pj_ioqueue_key_t     *callback_read_key,
                            *callback_write_key,
                            *callback_accept_key,
                            *callback_connect_key;
static pj_ioqueue_op_key_t  *callback_read_op,
                            *callback_write_op,
                            *callback_accept_op;

static void on_ioqueue_read(pj_ioqueue_key_t *key, 
                            pj_ioqueue_op_key_t *op_key,
                            pj_ssize_t bytes_read)
{
    callback_read_key = key;
    callback_read_op = op_key;
    callback_read_size = bytes_read;
    callback_call_count++;
}

static void on_ioqueue_write(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key,
                             pj_ssize_t bytes_written)
{
    callback_write_key = key;
    callback_write_op = op_key;
    callback_write_size = bytes_written;
    callback_call_count++;
}

static void on_ioqueue_accept(pj_ioqueue_key_t *key, 
                              pj_ioqueue_op_key_t *op_key,
                              pj_sock_t sock, 
                              int status)
{
    if (sock == PJ_INVALID_SOCKET) {

	if (status != PJ_SUCCESS) {
	    /* Ignore. Could be blocking error */
	    app_perror(".....warning: received error in on_ioqueue_accept() callback",
		       status);
	} else {
	    callback_accept_status = -61;
	    PJ_LOG(3,("", "..... on_ioqueue_accept() callback was given "
			  "invalid socket and status is %d", status));
	}
    } else {
        pj_sockaddr addr;
        int client_addr_len;

        client_addr_len = sizeof(addr);
        status = pj_sock_getsockname(sock, &addr, &client_addr_len);
        if (status != PJ_SUCCESS) {
            app_perror("...ERROR in pj_sock_getsockname()", status);
        }

	callback_accept_key = key;
	callback_accept_op = op_key;
	callback_accept_status = status;
	callback_call_count++;
    }
}

static void on_ioqueue_connect(pj_ioqueue_key_t *key, int status)
{
    callback_connect_key = key;
    callback_connect_status = status;
    callback_call_count++;
}

static pj_ioqueue_callback test_cb = 
{
    &on_ioqueue_read,
    &on_ioqueue_write,
    &on_ioqueue_accept,
    &on_ioqueue_connect,
};

static int send_recv_test(pj_ioqueue_t *ioque,
			  pj_ioqueue_key_t *skey,
			  pj_ioqueue_key_t *ckey,
			  void *send_buf,
			  void *recv_buf,
			  pj_ssize_t bufsize,
			  pj_timestamp *t_elapsed)
{
    pj_status_t status;
    pj_ssize_t bytes;
    pj_time_val timeout;
    pj_timestamp t1, t2;
    int pending_op = 0;
    pj_ioqueue_op_key_t read_op, write_op;

    // Start reading on the server side.
    bytes = bufsize;
    status = pj_ioqueue_recv(skey, &read_op, recv_buf, &bytes, 0);
    if (status != PJ_SUCCESS && status != PJ_EPENDING) {
        app_perror("...pj_ioqueue_recv error", status);
	return -100;
    }
    
    if (status == PJ_EPENDING)
        ++pending_op;
    else {
        /* Does not expect to return error or immediate data. */
        return -115;
    }

    // Randomize send buffer.
    pj_create_random_string((char*)send_buf, bufsize);

    // Starts send on the client side.
    bytes = bufsize;
    status = pj_ioqueue_send(ckey, &write_op, send_buf, &bytes, 0);
    if (status != PJ_SUCCESS && bytes != PJ_EPENDING) {
	return -120;
    }
    if (status == PJ_EPENDING) {
	++pending_op;
    }

    // Begin time.
    pj_get_timestamp(&t1);

    // Reset indicators
    callback_read_size = callback_write_size = 0;
    callback_read_key = callback_write_key = NULL;
    callback_read_op = callback_write_op = NULL;

    // Poll the queue until we've got completion event in the server side.
    status = 0;
    while (pending_op > 0) {
        timeout.sec = 1; timeout.msec = 0;
#ifdef PJ_SYMBIAN
	PJ_UNUSED_ARG(ioque);
	status = pj_symbianos_poll(-1, 1000);
#else
	status = pj_ioqueue_poll(ioque, &timeout);
#endif
	if (status > 0) {
            if (callback_read_size) {
                if (callback_read_size != bufsize)
                    return -160;
                if (callback_read_key != skey)
                    return -161;
                if (callback_read_op != &read_op)
                    return -162;
            }
            if (callback_write_size) {
                if (callback_write_key != ckey)
                    return -163;
                if (callback_write_op != &write_op)
                    return -164;
            }
	    pending_op -= status;
	}
        if (status == 0) {
            PJ_LOG(3,("", "...error: timed out"));
        }
	if (status < 0) {
	    return -170;
	}
    }

    // Pending op is zero.
    // Subsequent poll should yield zero too.
    timeout.sec = timeout.msec = 0;
#ifdef PJ_SYMBIAN
    status = pj_symbianos_poll(-1, 1);
#else
    status = pj_ioqueue_poll(ioque, &timeout);
#endif
    if (status != 0)
        return -173;

    // End time.
    pj_get_timestamp(&t2);
    t_elapsed->u32.lo += (t2.u32.lo - t1.u32.lo);

    // Compare recv buffer with send buffer.
    if (pj_memcmp(send_buf, recv_buf, bufsize) != 0) {
	return -180;
    }

    // Success
    return 0;
}


/*
 * Compliance test for success scenario.
 */
static int compliance_test_0(pj_bool_t allow_concur)
{
    pj_sock_t ssock=-1, csock0=-1, csock1=-1;
    pj_sockaddr_in addr, client_addr, rmt_addr;
    int client_addr_len;
    pj_pool_t *pool = NULL;
    char *send_buf, *recv_buf;
    pj_ioqueue_t *ioque = NULL;
    pj_ioqueue_key_t *skey=NULL, *ckey0=NULL, *ckey1=NULL;
    pj_ioqueue_op_key_t accept_op;
    int bufsize = BUF_MIN_SIZE;
    pj_ssize_t status = -1;
    int pending_op = 0;
    pj_timestamp t_elapsed;
    pj_str_t s;
    pj_status_t rc;

    // Create pool.
    pool = pj_pool_create(mem, NULL, POOL_SIZE, 4000, NULL);

    // Allocate buffers for send and receive.
    send_buf = (char*)pj_pool_alloc(pool, bufsize);
    recv_buf = (char*)pj_pool_alloc(pool, bufsize);

    // Create server socket and client socket for connecting
    rc = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, &ssock);
    if (rc != PJ_SUCCESS) {
        app_perror("...error creating socket", rc);
        status=-1; goto on_error;
    }

    rc = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, &csock1);
    if (rc != PJ_SUCCESS) {
        app_perror("...error creating socket", rc);
	status=-1; goto on_error;
    }

    // Bind server socket.
    pj_sockaddr_in_init(&addr, 0, 0);
    if ((rc=pj_sock_bind(ssock, &addr, sizeof(addr))) != 0 ) {
        app_perror("...bind error", rc);
	status=-10; goto on_error;
    }

    // Get server address.
    client_addr_len = sizeof(addr);
    rc = pj_sock_getsockname(ssock, &addr, &client_addr_len);
    if (rc != PJ_SUCCESS) {
        app_perror("...ERROR in pj_sock_getsockname()", rc);
	status=-15; goto on_error;
    }
    addr.sin_addr = pj_inet_addr(pj_cstr(&s, "127.0.0.1"));

    // Create I/O Queue.
    rc = pj_ioqueue_create(pool, PJ_IOQUEUE_MAX_HANDLES, &ioque);
    if (rc != PJ_SUCCESS) {
        app_perror("...ERROR in pj_ioqueue_create()", rc);
	status=-20; goto on_error;
    }

    // Concurrency
    rc = pj_ioqueue_set_default_concurrency(ioque, allow_concur);
    if (rc != PJ_SUCCESS) {
        app_perror("...ERROR in pj_ioqueue_set_default_concurrency()", rc);
	status=-21; goto on_error;
    }

    // Register server socket and client socket.
    rc = pj_ioqueue_register_sock(pool, ioque, ssock, NULL, &test_cb, &skey);
    if (rc == PJ_SUCCESS)
        rc = pj_ioqueue_register_sock(pool, ioque, csock1, NULL, &test_cb, 
                                      &ckey1);
    else
        ckey1 = NULL;
    if (rc != PJ_SUCCESS) {
        app_perror("...ERROR in pj_ioqueue_register_sock()", rc);
	status=-23; goto on_error;
    }

    // Server socket listen().
    if (pj_sock_listen(ssock, 5)) {
        app_perror("...ERROR in pj_sock_listen()", rc);
	status=-25; goto on_error;
    }

    // Server socket accept()
    client_addr_len = sizeof(pj_sockaddr_in);
    status = pj_ioqueue_accept(skey, &accept_op, &csock0, 
                               &client_addr, &rmt_addr, &client_addr_len);
    if (status != PJ_EPENDING) {
        app_perror("...ERROR in pj_ioqueue_accept()", rc);
	status=-30; goto on_error;
    }
    if (status==PJ_EPENDING) {
	++pending_op;
    }

    // Client socket connect()
    status = pj_ioqueue_connect(ckey1, &addr, sizeof(addr));
    if (status!=PJ_SUCCESS && status != PJ_EPENDING) {
        app_perror("...ERROR in pj_ioqueue_connect()", rc);
	status=-40; goto on_error;
    }
    if (status==PJ_EPENDING) {
	++pending_op;
    }

    // Poll until connected
    callback_read_size = callback_write_size = 0;
    callback_accept_status = callback_connect_status = -2;
    callback_call_count = 0;

    callback_read_key = callback_write_key = 
        callback_accept_key = callback_connect_key = NULL;
    callback_accept_op = callback_read_op = callback_write_op = NULL;

    while (pending_op) {
	pj_time_val timeout = {1, 0};

#ifdef PJ_SYMBIAN
	callback_call_count = 0;
	pj_symbianos_poll(-1, PJ_TIME_VAL_MSEC(timeout));
	status = callback_call_count;
#else
	status = pj_ioqueue_poll(ioque, &timeout);
#endif
	if (status > 0) {
            if (callback_accept_status != -2) {
                if (callback_accept_status != 0) {
                    status=-41; goto on_error;
                }
                if (callback_accept_key != skey) {
                    status=-42; goto on_error;
                }
                if (callback_accept_op != &accept_op) {
                    status=-43; goto on_error;
                }
                callback_accept_status = -2;
            }

            if (callback_connect_status != -2) {
                if (callback_connect_status != 0) {
                    status=-50; goto on_error;
                }
                if (callback_connect_key != ckey1) {
                    status=-51; goto on_error;
                }
                callback_connect_status = -2;
            }

	    if (status > pending_op) {
		PJ_LOG(3,(THIS_FILE,
			  "...error: pj_ioqueue_poll() returned %d "
			  "(only expecting %d)",
			  status, pending_op));
		return -52;
	    }
	    pending_op -= status;

	    if (pending_op == 0) {
		status = 0;
	    }
	}
    }

    // There's no pending operation.
    // When we poll the ioqueue, there must not be events.
    if (pending_op == 0) {
        pj_time_val timeout = {1, 0};
#ifdef PJ_SYMBIAN
	status = pj_symbianos_poll(-1, PJ_TIME_VAL_MSEC(timeout));
#else
        status = pj_ioqueue_poll(ioque, &timeout);
#endif
        if (status != 0) {
            status=-60; goto on_error;
        }
    }

    // Check accepted socket.
    if (csock0 == PJ_INVALID_SOCKET) {
	status = -69;
        app_perror("...accept() error", pj_get_os_error());
	goto on_error;
    }

    // Register newly accepted socket.
    rc = pj_ioqueue_register_sock(pool, ioque, csock0, NULL, 
                                  &test_cb, &ckey0);
    if (rc != PJ_SUCCESS) {
        app_perror("...ERROR in pj_ioqueue_register_sock", rc);
	status = -70;
	goto on_error;
    }

    // Test send and receive.
    t_elapsed.u32.lo = 0;
    status = send_recv_test(ioque, ckey0, ckey1, send_buf, 
                            recv_buf, bufsize, &t_elapsed);
    if (status != 0) {
	goto on_error;
    }

    // Success
    status = 0;

on_error:
    if (skey != NULL)
    	pj_ioqueue_unregister(skey);
    else if (ssock != PJ_INVALID_SOCKET)
	pj_sock_close(ssock);
    
    if (ckey1 != NULL)
    	pj_ioqueue_unregister(ckey1);
    else if (csock1 != PJ_INVALID_SOCKET)
	pj_sock_close(csock1);
    
    if (ckey0 != NULL)
    	pj_ioqueue_unregister(ckey0);
    else if (csock0 != PJ_INVALID_SOCKET)
	pj_sock_close(csock0);
    
    if (ioque != NULL)
	pj_ioqueue_destroy(ioque);
    pj_pool_release(pool);
    return status;

}

/*
 * Compliance test for failed scenario.
 * In this case, the client connects to a non-existant service.
 */
static int compliance_test_1(pj_bool_t allow_concur)
{
    pj_sock_t csock1=PJ_INVALID_SOCKET;
    pj_sockaddr_in addr;
    pj_pool_t *pool = NULL;
    pj_ioqueue_t *ioque = NULL;
    pj_ioqueue_key_t *ckey1 = NULL;
    pj_ssize_t status = -1;
    int pending_op = 0;
    pj_str_t s;
    pj_status_t rc;

    // Create pool.
    pool = pj_pool_create(mem, NULL, POOL_SIZE, 4000, NULL);

    // Create I/O Queue.
    rc = pj_ioqueue_create(pool, PJ_IOQUEUE_MAX_HANDLES, &ioque);
    if (!ioque) {
	status=-20; goto on_error;
    }

    // Concurrency
    rc = pj_ioqueue_set_default_concurrency(ioque, allow_concur);
    if (rc != PJ_SUCCESS) {
	status=-21; goto on_error;
    }

    // Create client socket
    rc = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, &csock1);
    if (rc != PJ_SUCCESS) {
        app_perror("...ERROR in pj_sock_socket()", rc);
	status=-1; goto on_error;
    }

    // Register client socket.
    rc = pj_ioqueue_register_sock(pool, ioque, csock1, NULL, 
                                  &test_cb, &ckey1);
    if (rc != PJ_SUCCESS) {
        app_perror("...ERROR in pj_ioqueue_register_sock()", rc);
	status=-23; goto on_error;
    }

    // Initialize remote address.
    pj_sockaddr_in_init(&addr, pj_cstr(&s, "127.0.0.1"), NON_EXISTANT_PORT);

    // Client socket connect()
    status = pj_ioqueue_connect(ckey1, &addr, sizeof(addr));
    if (status==PJ_SUCCESS) {
	// unexpectedly success!
	status = -30;
	goto on_error;
    }
    if (status != PJ_EPENDING) {
	// success
    } else {
	++pending_op;
    }

    callback_connect_status = -2;
    callback_connect_key = NULL;

    // Poll until we've got result
    while (pending_op) {
	pj_time_val timeout = {1, 0};

#ifdef PJ_SYMBIAN
	callback_call_count = 0;
	pj_symbianos_poll(-1, PJ_TIME_VAL_MSEC(timeout));
	status = callback_call_count;
#else
	status = pj_ioqueue_poll(ioque, &timeout);
#endif
	if (status > 0) {
            if (callback_connect_key==ckey1) {
		if (callback_connect_status == 0) {
		    // unexpectedly connected!
		    status = -50;
		    goto on_error;
		}
	    }

	    if (status > pending_op) {
		PJ_LOG(3,(THIS_FILE,
			  "...error: pj_ioqueue_poll() returned %d "
			  "(only expecting %d)",
			  status, pending_op));
		return -552;
	    }

	    pending_op -= status;
	    if (pending_op == 0) {
		status = 0;
	    }
	}
    }

    // There's no pending operation.
    // When we poll the ioqueue, there must not be events.
    if (pending_op == 0) {
        pj_time_val timeout = {1, 0};
#ifdef PJ_SYMBIAN
	status = pj_symbianos_poll(-1, PJ_TIME_VAL_MSEC(timeout));
#else
        status = pj_ioqueue_poll(ioque, &timeout);
#endif
        if (status != 0) {
            status=-60; goto on_error;
        }
    }

    // Success
    status = 0;

on_error:
    if (ckey1 != NULL)
    	pj_ioqueue_unregister(ckey1);
    else if (csock1 != PJ_INVALID_SOCKET)
	pj_sock_close(csock1);
    
    if (ioque != NULL)
	pj_ioqueue_destroy(ioque);
    pj_pool_release(pool);
    return status;
}


/*
 * Repeated connect/accept on the same listener socket.
 */
static int compliance_test_2(pj_bool_t allow_concur)
{
#if defined(PJ_SYMBIAN) && PJ_SYMBIAN!=0
    enum { MAX_PAIR = 1, TEST_LOOP = 2 };
#else
    enum { MAX_PAIR = 4, TEST_LOOP = 2 };
#endif

    struct listener
    {
	pj_sock_t	     sock;
	pj_ioqueue_key_t    *key;
	pj_sockaddr_in	     addr;
	int		     addr_len;
    } listener;

    struct server
    {
	pj_sock_t	     sock;
	pj_ioqueue_key_t    *key;
	pj_sockaddr_in	     local_addr;
	pj_sockaddr_in	     rem_addr;
	int		     rem_addr_len;
	pj_ioqueue_op_key_t  accept_op;
    } server[MAX_PAIR];

    struct client
    {
	pj_sock_t	     sock;
	pj_ioqueue_key_t    *key;
    } client[MAX_PAIR];

    pj_pool_t *pool = NULL;
    char *send_buf, *recv_buf;
    pj_ioqueue_t *ioque = NULL;
    int i, bufsize = BUF_MIN_SIZE;
    pj_ssize_t status;
    int test_loop, pending_op = 0;
    pj_timestamp t_elapsed;
    pj_str_t s;
    pj_status_t rc;

    listener.sock = PJ_INVALID_SOCKET;
    listener.key = NULL;
    
    for (i=0; i<MAX_PAIR; ++i) {
    	server[i].sock = PJ_INVALID_SOCKET;
    	server[i].key = NULL;
    }
    
    for (i=0; i<MAX_PAIR; ++i) {
    	client[i].sock = PJ_INVALID_SOCKET;
    	client[i].key = NULL;	
    }
    
    // Create pool.
    pool = pj_pool_create(mem, NULL, POOL_SIZE, 4000, NULL);


    // Create I/O Queue.
    rc = pj_ioqueue_create(pool, PJ_IOQUEUE_MAX_HANDLES, &ioque);
    if (rc != PJ_SUCCESS) {
        app_perror("...ERROR in pj_ioqueue_create()", rc);
	return -10;
    }


    // Concurrency
    rc = pj_ioqueue_set_default_concurrency(ioque, allow_concur);
    if (rc != PJ_SUCCESS) {
        app_perror("...ERROR in pj_ioqueue_set_default_concurrency()", rc);
	return -11;
    }

    // Allocate buffers for send and receive.
    send_buf = (char*)pj_pool_alloc(pool, bufsize);
    recv_buf = (char*)pj_pool_alloc(pool, bufsize);

    // Create listener socket
    rc = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, &listener.sock);
    if (rc != PJ_SUCCESS) {
        app_perror("...error creating socket", rc);
        status=-20; goto on_error;
    }

    // Bind listener socket.
    pj_sockaddr_in_init(&listener.addr, 0, 0);
    if ((rc=pj_sock_bind(listener.sock, &listener.addr, sizeof(listener.addr))) != 0 ) {
        app_perror("...bind error", rc);
	status=-30; goto on_error;
    }

    // Get listener address.
    listener.addr_len = sizeof(listener.addr);
    rc = pj_sock_getsockname(listener.sock, &listener.addr, &listener.addr_len);
    if (rc != PJ_SUCCESS) {
        app_perror("...ERROR in pj_sock_getsockname()", rc);
	status=-40; goto on_error;
    }
    listener.addr.sin_addr = pj_inet_addr(pj_cstr(&s, "127.0.0.1"));


    // Register listener socket.
    rc = pj_ioqueue_register_sock(pool, ioque, listener.sock, NULL, &test_cb, 
				  &listener.key);
    if (rc != PJ_SUCCESS) {
	app_perror("...ERROR", rc);
	status=-50; goto on_error;
    }


    // Listener socket listen().
    if (pj_sock_listen(listener.sock, 5)) {
        app_perror("...ERROR in pj_sock_listen()", rc);
	status=-60; goto on_error;
    }


    for (test_loop=0; test_loop < TEST_LOOP; ++test_loop) {
	// Client connect and server accept.
	for (i=0; i<MAX_PAIR; ++i) {
	    rc = pj_sock_socket(pj_AF_INET(), pj_SOCK_STREAM(), 0, &client[i].sock);
	    if (rc != PJ_SUCCESS) {
		app_perror("...error creating socket", rc);
		status=-70; goto on_error;
	    }

	    rc = pj_ioqueue_register_sock(pool, ioque, client[i].sock, NULL, 
					  &test_cb, &client[i].key);
	    if (rc != PJ_SUCCESS) {
		app_perror("...error ", rc);
		status=-80; goto on_error;
	    }

	    // Server socket accept()
	    pj_ioqueue_op_key_init(&server[i].accept_op, 
				   sizeof(server[i].accept_op));
	    server[i].rem_addr_len = sizeof(pj_sockaddr_in);
	    status = pj_ioqueue_accept(listener.key, &server[i].accept_op, 
				       &server[i].sock, &server[i].local_addr, 
				       &server[i].rem_addr, 
				       &server[i].rem_addr_len);
	    if (status!=PJ_SUCCESS && status != PJ_EPENDING) {
		app_perror("...ERROR in pj_ioqueue_accept()", rc);
		status=-90; goto on_error;
	    }
	    if (status==PJ_EPENDING) {
		++pending_op;
	    }


	    // Client socket connect()
	    status = pj_ioqueue_connect(client[i].key, &listener.addr, 
					sizeof(listener.addr));
	    if (status!=PJ_SUCCESS && status != PJ_EPENDING) {
		app_perror("...ERROR in pj_ioqueue_connect()", rc);
		status=-100; goto on_error;
	    }
	    if (status==PJ_EPENDING) {
		++pending_op;
	    }

	    // Poll until connection of this pair established
	    while (pending_op) {
		pj_time_val timeout = {1, 0};

#ifdef PJ_SYMBIAN
		status = pj_symbianos_poll(-1, PJ_TIME_VAL_MSEC(timeout));
#else
		status = pj_ioqueue_poll(ioque, &timeout);
#endif
		if (status > 0) {
		    if (status > pending_op) {
			PJ_LOG(3,(THIS_FILE,
				  "...error: pj_ioqueue_poll() returned %d "
				  "(only expecting %d)",
				  status, pending_op));
			return -110;
		    }
		    pending_op -= status;

		    if (pending_op == 0) {
			status = 0;
		    }
		}
	    }
	}

	// There's no pending operation.
	// When we poll the ioqueue, there must not be events.
	if (pending_op == 0) {
	    pj_time_val timeout = {1, 0};
#ifdef PJ_SYMBIAN
	    status = pj_symbianos_poll(-1, PJ_TIME_VAL_MSEC(timeout));
#else
	    status = pj_ioqueue_poll(ioque, &timeout);
#endif
	    if (status != 0) {
		status=-120; goto on_error;
	    }
	}

	for (i=0; i<MAX_PAIR; ++i) {
	    // Check server socket.
	    if (server[i].sock == PJ_INVALID_SOCKET) {
		status = -130;
		app_perror("...accept() error", pj_get_os_error());
		goto on_error;
	    }

	    // Check addresses
	    if (server[i].local_addr.sin_family != pj_AF_INET() ||
		server[i].local_addr.sin_addr.s_addr == 0 ||
		server[i].local_addr.sin_port == 0)
	    {
		app_perror("...ERROR address not set", rc);
		status = -140;
		goto on_error;
	    }

	    if (server[i].rem_addr.sin_family != pj_AF_INET() ||
		server[i].rem_addr.sin_addr.s_addr == 0 ||
		server[i].rem_addr.sin_port == 0)
	    {
		app_perror("...ERROR address not set", rc);
		status = -150;
		goto on_error;
	    }


	    // Register newly accepted socket.
	    rc = pj_ioqueue_register_sock(pool, ioque, server[i].sock, NULL,
					  &test_cb, &server[i].key);
	    if (rc != PJ_SUCCESS) {
		app_perror("...ERROR in pj_ioqueue_register_sock", rc);
		status = -160;
		goto on_error;
	    }

	    // Test send and receive.
	    t_elapsed.u32.lo = 0;
	    status = send_recv_test(ioque, server[i].key, client[i].key, 
				    send_buf, recv_buf, bufsize, &t_elapsed);
	    if (status != 0) {
		goto on_error;
	    }
	}

	// Success
	status = 0;

	for (i=0; i<MAX_PAIR; ++i) {
	    if (server[i].key != NULL) {
		pj_ioqueue_unregister(server[i].key);
		server[i].key = NULL;
		server[i].sock = PJ_INVALID_SOCKET;
	    } else if (server[i].sock != PJ_INVALID_SOCKET) {
		pj_sock_close(server[i].sock);
		server[i].sock = PJ_INVALID_SOCKET;
	    }

	    if (client[i].key != NULL) {
		pj_ioqueue_unregister(client[i].key);
		client[i].key = NULL;
		client[i].sock = PJ_INVALID_SOCKET;
	    } else if (client[i].sock != PJ_INVALID_SOCKET) {
		pj_sock_close(client[i].sock);
		client[i].sock = PJ_INVALID_SOCKET;
	    }
	}
    }

    status = 0;

on_error:
    for (i=0; i<MAX_PAIR; ++i) {
	if (server[i].key != NULL) {
	    pj_ioqueue_unregister(server[i].key);
	    server[i].key = NULL;
	    server[i].sock = PJ_INVALID_SOCKET;
	} else if (server[i].sock != PJ_INVALID_SOCKET) {
	    pj_sock_close(server[i].sock);
	    server[i].sock = PJ_INVALID_SOCKET;
	}

	if (client[i].key != NULL) {
	    pj_ioqueue_unregister(client[i].key);
	    client[i].key = NULL;
	    server[i].sock = PJ_INVALID_SOCKET;
	} else if (client[i].sock != PJ_INVALID_SOCKET) {
	    pj_sock_close(client[i].sock);
	    client[i].sock = PJ_INVALID_SOCKET;
	}
    }

    if (listener.key) {
	pj_ioqueue_unregister(listener.key);
	listener.key = NULL;
    } else if (listener.sock != PJ_INVALID_SOCKET) {
	pj_sock_close(listener.sock);
	listener.sock = PJ_INVALID_SOCKET;
    }

    if (ioque != NULL)
	pj_ioqueue_destroy(ioque);
    pj_pool_release(pool);
    return status;

}


static int tcp_ioqueue_test_impl(pj_bool_t allow_concur)
{
    int status;

    PJ_LOG(3,(THIS_FILE, "..testing with concurency=%d", allow_concur));

    PJ_LOG(3, (THIS_FILE, "..%s compliance test 0 (success scenario)",
	       pj_ioqueue_name()));
    if ((status=compliance_test_0(allow_concur)) != 0) {
	PJ_LOG(1, (THIS_FILE, "....FAILED (status=%d)\n", status));
	return status;
    }
    PJ_LOG(3, (THIS_FILE, "..%s compliance test 1 (failed scenario)",
               pj_ioqueue_name()));
    if ((status=compliance_test_1(allow_concur)) != 0) {
	PJ_LOG(1, (THIS_FILE, "....FAILED (status=%d)\n", status));
	return status;
    }

    PJ_LOG(3, (THIS_FILE, "..%s compliance test 2 (repeated accept)",
               pj_ioqueue_name()));
    if ((status=compliance_test_2(allow_concur)) != 0) {
	PJ_LOG(1, (THIS_FILE, "....FAILED (status=%d)\n", status));
	return status;
    }

    return 0;
}

int tcp_ioqueue_test()
{
    int rc;

    rc = tcp_ioqueue_test_impl(PJ_TRUE);
    if (rc != 0)
	return rc;

    rc = tcp_ioqueue_test_impl(PJ_FALSE);
    if (rc != 0)
	return rc;

    return 0;
}

#endif	/* PJ_HAS_TCP */


#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_uiq_tcp;
#endif	/* INCLUDE_TCP_IOQUEUE_TEST */

