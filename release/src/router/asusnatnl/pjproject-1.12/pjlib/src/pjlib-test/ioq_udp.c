/* $Id: ioq_udp.c 4387 2013-02-27 10:16:08Z ming $ */
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
 * \page page_pjlib_ioqueue_udp_test Test: I/O Queue (UDP)
 *
 * This file provides implementation to test the
 * functionality of the I/O queue when UDP socket is used.
 *
 *
 * This file is <b>pjlib-test/ioq_udp.c</b>
 *
 * \include pjlib-test/ioq_udp.c
 */


#if INCLUDE_UDP_IOQUEUE_TEST

#include <pjlib.h>

#include <pj/compat/socket.h>

#define THIS_FILE	    "test_udp"
#define PORT		    51233
#define LOOP		    2
///#define LOOP		    2
#define BUF_MIN_SIZE	    32
#define BUF_MAX_SIZE	    2048
#define SOCK_INACTIVE_MIN   (1)
#define SOCK_INACTIVE_MAX   (PJ_IOQUEUE_MAX_HANDLES - 2)
#define POOL_SIZE	    (2*BUF_MAX_SIZE + SOCK_INACTIVE_MAX*128 + 2048)

#undef TRACE_
#define TRACE_(msg)	    PJ_LOG(3,(THIS_FILE,"....." msg))

#if 0
#  define TRACE__(args)	    PJ_LOG(3,args)
#else
#  define TRACE__(args)
#endif


static pj_ssize_t            callback_read_size,
                             callback_write_size,
                             callback_accept_status,
                             callback_connect_status;
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
    TRACE__((THIS_FILE, "     callback_read_key = %p, bytes=%d", 
	     key, bytes_read));
}

static void on_ioqueue_write(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key,
                             pj_ssize_t bytes_written)
{
    callback_write_key = key;
    callback_write_op = op_key;
    callback_write_size = bytes_written;
}

static void on_ioqueue_accept(pj_ioqueue_key_t *key, 
                              pj_ioqueue_op_key_t *op_key,
                              pj_sock_t sock, int status)
{
    PJ_UNUSED_ARG(sock);
    callback_accept_key = key;
    callback_accept_op = op_key;
    callback_accept_status = status;
}

static void on_ioqueue_connect(pj_ioqueue_key_t *key, int status)
{
    callback_connect_key = key;
    callback_connect_status = status;
}

static pj_ioqueue_callback test_cb = 
{
    &on_ioqueue_read,
    &on_ioqueue_write,
    &on_ioqueue_accept,
    &on_ioqueue_connect,
};

#ifdef PJ_WIN32
#  define S_ADDR S_un.S_addr
#else
#  define S_ADDR s_addr
#endif

/*
 * compliance_test()
 * To test that the basic IOQueue functionality works. It will just exchange
 * data between two sockets.
 */ 
static int compliance_test(pj_bool_t allow_concur)
{
    pj_sock_t ssock=-1, csock=-1;
    pj_sockaddr_in addr, dst_addr;
    int addrlen;
    pj_pool_t *pool = NULL;
    char *send_buf, *recv_buf;
    pj_ioqueue_t *ioque = NULL;
    pj_ioqueue_key_t *skey = NULL, *ckey = NULL;
    pj_ioqueue_op_key_t read_op, write_op;
    int bufsize = BUF_MIN_SIZE;
    pj_ssize_t bytes, status = -1;
    pj_str_t temp;
    pj_bool_t send_pending, recv_pending;
    pj_status_t rc;

    pj_set_os_error(PJ_SUCCESS);

    // Create pool.
    pool = pj_pool_create(mem, NULL, POOL_SIZE, 4000, NULL);

    // Allocate buffers for send and receive.
    send_buf = (char*)pj_pool_alloc(pool, bufsize);
    recv_buf = (char*)pj_pool_alloc(pool, bufsize);

    // Allocate sockets for sending and receiving.
    TRACE_("creating sockets...");
    rc = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &ssock);
    if (rc==PJ_SUCCESS)
        rc = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &csock);
    else
        csock = PJ_INVALID_SOCKET;
    if (rc != PJ_SUCCESS) {
        app_perror("...ERROR in pj_sock_socket()", rc);
	status=-1; goto on_error;
    }

    // Bind server socket.
    TRACE_("bind socket...");
    pj_bzero(&addr, sizeof(addr));
    addr.sin_family = pj_AF_INET();
    addr.sin_port = pj_htons(PORT);
    if (pj_sock_bind(ssock, &addr, sizeof(addr))) {
	status=-10; goto on_error;
    }

    // Create I/O Queue.
    TRACE_("create ioqueue...");
    rc = pj_ioqueue_create(pool, PJ_IOQUEUE_MAX_HANDLES, &ioque);
    if (rc != PJ_SUCCESS) {
	status=-20; goto on_error;
    }

    // Set concurrency
    TRACE_("set concurrency...");
    rc = pj_ioqueue_set_default_concurrency(ioque, allow_concur);
    if (rc != PJ_SUCCESS) {
	status=-21; goto on_error;
    }

    // Register server and client socket.
    // We put this after inactivity socket, hopefully this can represent the
    // worst waiting time.
    TRACE_("registering first sockets...");
    rc = pj_ioqueue_register_sock(pool, ioque, ssock, NULL, 
			          &test_cb, &skey);
    if (rc != PJ_SUCCESS) {
	app_perror("...error(10): ioqueue_register error", rc);
	status=-25; goto on_error;
    }
    TRACE_("registering second sockets...");
    rc = pj_ioqueue_register_sock( pool, ioque, csock, NULL, 
			           &test_cb, &ckey);
    if (rc != PJ_SUCCESS) {
	app_perror("...error(11): ioqueue_register error", rc);
	status=-26; goto on_error;
    }

    // Randomize send_buf.
    pj_create_random_string(send_buf, bufsize);

    // Register reading from ioqueue.
    TRACE_("start recvfrom...");
    pj_bzero(&addr, sizeof(addr));
    addrlen = sizeof(addr);
    bytes = bufsize;
    rc = pj_ioqueue_recvfrom(skey, &read_op, recv_buf, &bytes, 0,
			     &addr, &addrlen);
    if (rc != PJ_SUCCESS && rc != PJ_EPENDING) {
        app_perror("...error: pj_ioqueue_recvfrom", rc);
	status=-28; goto on_error;
    } else if (rc == PJ_EPENDING) {
	recv_pending = 1;
	PJ_LOG(3, (THIS_FILE, 
		   "......ok: recvfrom returned pending"));
    } else {
	PJ_LOG(3, (THIS_FILE, 
		   "......error: recvfrom returned immediate ok!"));
	status=-29; goto on_error;
    }

    // Set destination address to send the packet.
    TRACE_("set destination address...");
    temp = pj_str("127.0.0.1");
    if ((rc=pj_sockaddr_in_init(&dst_addr, &temp, PORT)) != 0) {
	app_perror("...error: unable to resolve 127.0.0.1", rc);
	status=-290; goto on_error;
    }

    // Write must return the number of bytes.
    TRACE_("start sendto...");
    bytes = bufsize;
    rc = pj_ioqueue_sendto(ckey, &write_op, send_buf, &bytes, 0, &dst_addr, 
			   sizeof(dst_addr));
    if (rc != PJ_SUCCESS && rc != PJ_EPENDING) {
        app_perror("...error: pj_ioqueue_sendto", rc);
	status=-30; goto on_error;
    } else if (rc == PJ_EPENDING) {
	send_pending = 1;
	PJ_LOG(3, (THIS_FILE, 
		   "......ok: sendto returned pending"));
    } else {
	send_pending = 0;
	PJ_LOG(3, (THIS_FILE, 
		   "......ok: sendto returned immediate success"));
    }

    // reset callback variables.
    callback_read_size = callback_write_size = 0;
    callback_accept_status = callback_connect_status = -2;
    callback_read_key = callback_write_key = 
        callback_accept_key = callback_connect_key = NULL;
    callback_read_op = callback_write_op = NULL;

    // Poll if pending.
    while (send_pending || recv_pending) {
	int rc;
	pj_time_val timeout = { 5, 0 };

	TRACE_("poll...");
#ifdef PJ_SYMBIAN
	rc = pj_symbianos_poll(-1, PJ_TIME_VAL_MSEC(timeout));
#else
	rc = pj_ioqueue_poll(ioque, &timeout);
#endif

	if (rc == 0) {
	    PJ_LOG(1,(THIS_FILE, "...ERROR: timed out..."));
	    status=-45; goto on_error;
        } else if (rc < 0) {
            app_perror("...ERROR in ioqueue_poll()", -rc);
	    status=-50; goto on_error;
	}

	if (callback_read_key != NULL) {
            if (callback_read_size != bufsize) {
                status=-61; goto on_error;
            }
            if (callback_read_key != skey) {
                status=-65; goto on_error;
            }
            if (callback_read_op != &read_op) {
                status=-66; goto on_error;
            }

	    if (pj_memcmp(send_buf, recv_buf, bufsize) != 0) {
		status=-67; goto on_error;
	    }
	    if (addrlen != sizeof(pj_sockaddr_in)) {
		status=-68; goto on_error;
	    }
	    if (addr.sin_family != pj_AF_INET()) {
		status=-69; goto on_error;
	    }


	    recv_pending = 0;
	} 

        if (callback_write_key != NULL) {
            if (callback_write_size != bufsize) {
                status=-73; goto on_error;
            }
            if (callback_write_key != ckey) {
                status=-75; goto on_error;
            }
            if (callback_write_op != &write_op) {
                status=-76; goto on_error;
            }

            send_pending = 0;
	}
    } 
    
    // Success
    status = 0;

on_error:
    if (skey)
    	pj_ioqueue_unregister(skey);
    else if (ssock != -1)
	pj_sock_close(ssock);
    
    if (ckey)
    	pj_ioqueue_unregister(ckey);
    else if (csock != -1)
	pj_sock_close(csock);
    
    if (ioque != NULL)
	pj_ioqueue_destroy(ioque);
    pj_pool_release(pool);
    return status;

}


static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read)
{
    unsigned *p_packet_cnt = (unsigned*) pj_ioqueue_get_user_data(key);

    PJ_UNUSED_ARG(op_key);
    PJ_UNUSED_ARG(bytes_read);

    (*p_packet_cnt)++;
}

/*
 * unregister_test()
 * Check if callback is still called after socket has been unregistered or 
 * closed.
 */ 
static int unregister_test(pj_bool_t allow_concur)
{
    enum { RPORT = 50000, SPORT = 50001 };
    pj_pool_t *pool;
    pj_ioqueue_t *ioqueue;
    pj_sock_t ssock;
    pj_sock_t rsock;
    int addrlen;
    pj_sockaddr_in addr;
    pj_ioqueue_key_t *key;
    pj_ioqueue_op_key_t opkey;
    pj_ioqueue_callback cb;
    unsigned packet_cnt;
    char sendbuf[10], recvbuf[10];
    pj_ssize_t bytes;
    pj_time_val timeout;
    pj_status_t status;

    pool = pj_pool_create(mem, "test", 4000, 4000, NULL);
    if (!pool) {
	app_perror("Unable to create pool", PJ_ENOMEM);
	return -100;
    }

    status = pj_ioqueue_create(pool, 16, &ioqueue);
    if (status != PJ_SUCCESS) {
	app_perror("Error creating ioqueue", status);
	return -110;
    }

    // Set concurrency
    TRACE_("set concurrency...");
    status = pj_ioqueue_set_default_concurrency(ioqueue, allow_concur);
    if (status != PJ_SUCCESS) {
	return -112;
    }

    /* Create sender socket */
    status = app_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, SPORT, &ssock);
    if (status != PJ_SUCCESS) {
	app_perror("Error initializing socket", status);
	return -120;
    }

    /* Create receiver socket. */
    status = app_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, RPORT, &rsock);
    if (status != PJ_SUCCESS) {
	app_perror("Error initializing socket", status);
	return -130;
    }

    /* Register rsock to ioqueue. */
    pj_bzero(&cb, sizeof(cb));
    cb.on_read_complete = &on_read_complete;
    packet_cnt = 0;
    status = pj_ioqueue_register_sock(pool, ioqueue, rsock, &packet_cnt,
				      &cb, &key);
    if (status != PJ_SUCCESS) {
	app_perror("Error registering to ioqueue", status);
	return -140;
    }

    /* Init operation key. */
    pj_ioqueue_op_key_init(&opkey, sizeof(opkey));

    /* Start reading. */
    bytes = sizeof(recvbuf);
    status = pj_ioqueue_recv( key, &opkey, recvbuf, &bytes, 0);
    if (status != PJ_EPENDING) {
	app_perror("Expecting PJ_EPENDING, but got this", status);
	return -150;
    }

    /* Init destination address. */
    addrlen = sizeof(addr);
    status = pj_sock_getsockname(rsock, &addr, &addrlen);
    if (status != PJ_SUCCESS) {
	app_perror("getsockname error", status);
	return -160;
    }

    /* Override address with 127.0.0.1, since getsockname will return
     * zero in the address field.
     */
    addr.sin_addr = pj_inet_addr2("127.0.0.1");

    /* Init buffer to send */
    pj_ansi_strcpy(sendbuf, "Hello0123");

    /* Send one packet. */
    bytes = sizeof(sendbuf);
    status = pj_sock_sendto(ssock, sendbuf, &bytes, 0,
			    &addr, sizeof(addr));

    if (status != PJ_SUCCESS) {
	app_perror("sendto error", status);
	return -170;
    }

    /* Check if packet is received. */
    timeout.sec = 1; timeout.msec = 0;
#ifdef PJ_SYMBIAN
    pj_symbianos_poll(-1, 1000);
#else
    pj_ioqueue_poll(ioqueue, &timeout);
#endif

    if (packet_cnt != 1) {
	return -180;
    }

    /* Just to make sure things are settled.. */
    pj_thread_sleep(100);

    /* Start reading again. */
    bytes = sizeof(recvbuf);
    status = pj_ioqueue_recv( key, &opkey, recvbuf, &bytes, 0);
    if (status != PJ_EPENDING) {
	app_perror("Expecting PJ_EPENDING, but got this", status);
	return -190;
    }

    /* Reset packet counter */
    packet_cnt = 0;

    /* Send one packet. */
    bytes = sizeof(sendbuf);
    status = pj_sock_sendto(ssock, sendbuf, &bytes, 0,
			    &addr, sizeof(addr));

    if (status != PJ_SUCCESS) {
	app_perror("sendto error", status);
	return -200;
    }

    /* Now unregister and close socket. */
    pj_ioqueue_unregister(key);

    /* Poll ioqueue. */
#ifdef PJ_SYMBIAN
    pj_symbianos_poll(-1, 1000);
#else
    timeout.sec = 1; timeout.msec = 0;
    pj_ioqueue_poll(ioqueue, &timeout);
#endif

    /* Must NOT receive any packets after socket is closed! */
    if (packet_cnt > 0) {
	PJ_LOG(3,(THIS_FILE, "....errror: not expecting to receive packet "
			     "after socket has been closed"));
	return -210;
    }

    /* Success */
    pj_sock_close(ssock);
    pj_ioqueue_destroy(ioqueue);

    pj_pool_release(pool);

    return 0;
}


/*
 * Testing with many handles.
 * This will just test registering PJ_IOQUEUE_MAX_HANDLES count
 * of sockets to the ioqueue.
 */
static int many_handles_test(pj_bool_t allow_concur)
{
    enum { MAX = PJ_IOQUEUE_MAX_HANDLES };
    pj_pool_t *pool;
    pj_ioqueue_t *ioqueue;
    pj_sock_t *sock;
    pj_ioqueue_key_t **key;
    pj_status_t rc;
    int count, i; /* must be signed */

    PJ_LOG(3,(THIS_FILE,"...testing with so many handles"));

    pool = pj_pool_create(mem, NULL, 4000, 4000, NULL);
    if (!pool)
	return PJ_ENOMEM;

    key = (pj_ioqueue_key_t**) 
    	  pj_pool_alloc(pool, MAX*sizeof(pj_ioqueue_key_t*));
    sock = (pj_sock_t*) pj_pool_alloc(pool, MAX*sizeof(pj_sock_t));
    
    /* Create IOQueue */
    rc = pj_ioqueue_create(pool, MAX, &ioqueue);
    if (rc != PJ_SUCCESS || ioqueue == NULL) {
	app_perror("...error in pj_ioqueue_create", rc);
	return -10;
    }

    // Set concurrency
    rc = pj_ioqueue_set_default_concurrency(ioqueue, allow_concur);
    if (rc != PJ_SUCCESS) {
	return -11;
    }

    /* Register as many sockets. */
    for (count=0; count<MAX; ++count) {
	sock[count] = PJ_INVALID_SOCKET;
	rc = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &sock[count]);
	if (rc != PJ_SUCCESS || sock[count] == PJ_INVALID_SOCKET) {
	    PJ_LOG(3,(THIS_FILE, "....unable to create %d-th socket, rc=%d", 
				 count, rc));
	    break;
	}
	key[count] = NULL;
	rc = pj_ioqueue_register_sock(pool, ioqueue, sock[count],
				      NULL, &test_cb, &key[count]);
	if (rc != PJ_SUCCESS || key[count] == NULL) {
	    PJ_LOG(3,(THIS_FILE, "....unable to register %d-th socket, rc=%d", 
				 count, rc));
	    return -30;
	}
    }

    /* Test complete. */

    /* Now deregister and close all handles. */ 

    /* NOTE for RTEMS:
     *  It seems that the order of close(sock) is pretty important here.
     *  If we close the sockets with the same order as when they were created,
     *  RTEMS doesn't seem to reuse the sockets, thus next socket created
     *  will have descriptor higher than the last socket created.
     *  If we close the sockets in the reverse order, then the descriptor will
     *  get reused.
     *  This used to cause problem with select ioqueue, since the ioqueue
     *  always gives FD_SETSIZE for the first select() argument. This ioqueue
     *  behavior can be changed with setting PJ_SELECT_NEEDS_NFDS macro.
     */
    for (i=count-1; i>=0; --i) {
    ///for (i=0; i<count; ++i) {
	rc = pj_ioqueue_unregister(key[i]);
	if (rc != PJ_SUCCESS) {
	    app_perror("...error in pj_ioqueue_unregister", rc);
	}
    }

    rc = pj_ioqueue_destroy(ioqueue);
    if (rc != PJ_SUCCESS) {
	app_perror("...error in pj_ioqueue_destroy", rc);
    }
    
    pj_pool_release(pool);

    PJ_LOG(3,(THIS_FILE,"....many_handles_test() ok"));

    return 0;
}

/*
 * Multi-operation test.
 */

/*
 * Benchmarking IOQueue
 */
static int bench_test(pj_bool_t allow_concur, int bufsize, 
		      int inactive_sock_count)
{
    pj_sock_t ssock=-1, csock=-1;
    pj_sockaddr_in addr;
    pj_pool_t *pool = NULL;
    pj_sock_t *inactive_sock=NULL;
    pj_ioqueue_op_key_t *inactive_read_op;
    char *send_buf, *recv_buf;
    pj_ioqueue_t *ioque = NULL;
    pj_ioqueue_key_t *skey, *ckey, *keys[SOCK_INACTIVE_MAX+2];
    pj_timestamp t1, t2, t_elapsed;
    int rc=0, i;    /* i must be signed */
    pj_str_t temp;
    char errbuf[PJ_ERR_MSG_SIZE];

    TRACE__((THIS_FILE, "   bench test %d", inactive_sock_count));

    // Create pool.
    pool = pj_pool_create(mem, NULL, POOL_SIZE, 4000, NULL);

    // Allocate buffers for send and receive.
    send_buf = (char*)pj_pool_alloc(pool, bufsize);
    recv_buf = (char*)pj_pool_alloc(pool, bufsize);

    // Allocate sockets for sending and receiving.
    rc = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &ssock);
    if (rc == PJ_SUCCESS) {
        rc = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &csock);
    } else
        csock = PJ_INVALID_SOCKET;
    if (rc != PJ_SUCCESS) {
	app_perror("...error: pj_sock_socket()", rc);
	goto on_error;
    }

    // Bind server socket.
    pj_bzero(&addr, sizeof(addr));
    addr.sin_family = pj_AF_INET();
    addr.sin_port = pj_htons(PORT);
    if (pj_sock_bind(ssock, &addr, sizeof(addr)))
	goto on_error;

    pj_assert(inactive_sock_count+2 <= PJ_IOQUEUE_MAX_HANDLES);

    // Create I/O Queue.
    rc = pj_ioqueue_create(pool, PJ_IOQUEUE_MAX_HANDLES, &ioque);
    if (rc != PJ_SUCCESS) {
	app_perror("...error: pj_ioqueue_create()", rc);
	goto on_error;
    }

    // Set concurrency
    rc = pj_ioqueue_set_default_concurrency(ioque, allow_concur);
    if (rc != PJ_SUCCESS) {
	app_perror("...error: pj_ioqueue_set_default_concurrency()", rc);
	goto on_error;
    }

    // Allocate inactive sockets, and bind them to some arbitrary address.
    // Then register them to the I/O queue, and start a read operation.
    inactive_sock = (pj_sock_t*)pj_pool_alloc(pool, 
				    inactive_sock_count*sizeof(pj_sock_t));
    inactive_read_op = (pj_ioqueue_op_key_t*)pj_pool_alloc(pool,
                              inactive_sock_count*sizeof(pj_ioqueue_op_key_t));
    pj_bzero(&addr, sizeof(addr));
    addr.sin_family = pj_AF_INET();
    for (i=0; i<inactive_sock_count; ++i) {
        pj_ssize_t bytes;

	rc = pj_sock_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, &inactive_sock[i]);
	if (rc != PJ_SUCCESS || inactive_sock[i] < 0) {
	    app_perror("...error: pj_sock_socket()", rc);
	    goto on_error;
	}
	if ((rc=pj_sock_bind(inactive_sock[i], &addr, sizeof(addr))) != 0) {
	    pj_sock_close(inactive_sock[i]);
	    inactive_sock[i] = PJ_INVALID_SOCKET;
	    app_perror("...error: pj_sock_bind()", rc);
	    goto on_error;
	}
	rc = pj_ioqueue_register_sock(pool, ioque, inactive_sock[i], 
			              NULL, &test_cb, &keys[i]);
	if (rc != PJ_SUCCESS) {
	    pj_sock_close(inactive_sock[i]);
	    inactive_sock[i] = PJ_INVALID_SOCKET;
	    app_perror("...error(1): pj_ioqueue_register_sock()", rc);
	    PJ_LOG(3,(THIS_FILE, "....i=%d", i));
	    goto on_error;
	}
        bytes = bufsize;
	rc = pj_ioqueue_recv(keys[i], &inactive_read_op[i], recv_buf, &bytes, 0);
	if (rc != PJ_EPENDING) {
	    pj_sock_close(inactive_sock[i]);
	    inactive_sock[i] = PJ_INVALID_SOCKET;
	    app_perror("...error: pj_ioqueue_read()", rc);
	    goto on_error;
	}
    }

    // Register server and client socket.
    // We put this after inactivity socket, hopefully this can represent the
    // worst waiting time.
    rc = pj_ioqueue_register_sock(pool, ioque, ssock, NULL, 
			          &test_cb, &skey);
    if (rc != PJ_SUCCESS) {
	app_perror("...error(2): pj_ioqueue_register_sock()", rc);
	goto on_error;
    }

    rc = pj_ioqueue_register_sock(pool, ioque, csock, NULL, 
			          &test_cb, &ckey);
    if (rc != PJ_SUCCESS) {
	app_perror("...error(3): pj_ioqueue_register_sock()", rc);
	goto on_error;
    }

    // Set destination address to send the packet.
    pj_sockaddr_in_init(&addr, pj_cstr(&temp, "127.0.0.1"), PORT);

    // Test loop.
    t_elapsed.u64 = 0;
    for (i=0; i<LOOP; ++i) {
	pj_ssize_t bytes;
        pj_ioqueue_op_key_t read_op, write_op;

	// Randomize send buffer.
	pj_create_random_string(send_buf, bufsize);

	// Start reading on the server side.
        bytes = bufsize;
	rc = pj_ioqueue_recv(skey, &read_op, recv_buf, &bytes, 0);
	if (rc != PJ_EPENDING) {
	    app_perror("...error: pj_ioqueue_read()", rc);
	    break;
	}

	// Starts send on the client side.
        bytes = bufsize;
	rc = pj_ioqueue_sendto(ckey, &write_op, send_buf, &bytes, 0,
			       &addr, sizeof(addr));
	if (rc != PJ_SUCCESS && rc != PJ_EPENDING) {
	    app_perror("...error: pj_ioqueue_write()", rc);
	    break;
	}
	if (rc == PJ_SUCCESS) {
	    if (bytes < 0) {
		app_perror("...error: pj_ioqueue_sendto()", -bytes);
		break;
	    }
	}

	// Begin time.
	pj_get_timestamp(&t1);

	// Poll the queue until we've got completion event in the server side.
        callback_read_key = NULL;
        callback_read_size = 0;
	TRACE__((THIS_FILE, "     waiting for key = %p", skey));
	do {
	    pj_time_val timeout = { 1, 0 };
#ifdef PJ_SYMBIAN
	    rc = pj_symbianos_poll(-1, PJ_TIME_VAL_MSEC(timeout));
#else
	    rc = pj_ioqueue_poll(ioque, &timeout);
#endif
	    TRACE__((THIS_FILE, "     poll rc=%d", rc));
	} while (rc >= 0 && callback_read_key != skey);

	// End time.
	pj_get_timestamp(&t2);
	t_elapsed.u64 += (t2.u64 - t1.u64);

	if (rc < 0) {
	    app_perror("   error: pj_ioqueue_poll", -rc);
	    break;
	}

	// Compare recv buffer with send buffer.
	if (callback_read_size != bufsize || 
	    pj_memcmp(send_buf, recv_buf, bufsize)) 
	{
	    rc = -10;
	    PJ_LOG(3,(THIS_FILE, "   error: size/buffer mismatch"));
	    break;
	}

	// Poll until all events are exhausted, before we start the next loop.
	do {
	    pj_time_val timeout = { 0, 10 };
#ifdef PJ_SYMBIAN
	    PJ_UNUSED_ARG(timeout);
	    rc = pj_symbianos_poll(-1, 100);
#else	    
	    rc = pj_ioqueue_poll(ioque, &timeout);
#endif
	} while (rc>0);

	rc = 0;
    }

    // Print results
    if (rc == 0) {
	pj_timestamp tzero;
	pj_uint32_t usec_delay;

	tzero.u32.hi = tzero.u32.lo = 0;
	usec_delay = pj_elapsed_usec( &tzero, &t_elapsed);

	PJ_LOG(3, (THIS_FILE, "...%10d %15d  % 9d", 
	           bufsize, inactive_sock_count, usec_delay));

    } else {
	PJ_LOG(2, (THIS_FILE, "...ERROR rc=%d (buf:%d, fds:%d)", 
			      rc, bufsize, inactive_sock_count+2));
    }

    // Cleaning up.
    for (i=inactive_sock_count-1; i>=0; --i) {
	pj_ioqueue_unregister(keys[i]);
    }

    pj_ioqueue_unregister(skey);
    pj_ioqueue_unregister(ckey);


    pj_ioqueue_destroy(ioque);
    pj_pool_release( pool);
    return rc;

on_error:
    PJ_LOG(1,(THIS_FILE, "...ERROR: %s", 
	      pj_strerror(pj_get_netos_error(), errbuf, sizeof(errbuf))));
    if (ssock)
	pj_sock_close(ssock);
    if (csock)
	pj_sock_close(csock);
    for (i=0; i<inactive_sock_count && inactive_sock && 
	      inactive_sock[i]!=PJ_INVALID_SOCKET; ++i) 
    {
	pj_sock_close(inactive_sock[i]);
    }
    if (ioque != NULL)
	pj_ioqueue_destroy(ioque);
    pj_pool_release( pool);
    return -1;
}

static int udp_ioqueue_test_imp(pj_bool_t allow_concur)
{
    int status;
    int bufsize, sock_count;

    PJ_LOG(3,(THIS_FILE, "..testing with concurency=%d", allow_concur));

    //goto pass1;

    PJ_LOG(3, (THIS_FILE, "...compliance test (%s)", pj_ioqueue_name()));
    if ((status=compliance_test(allow_concur)) != 0) {
	return status;
    }
    PJ_LOG(3, (THIS_FILE, "....compliance test ok"));


    PJ_LOG(3, (THIS_FILE, "...unregister test (%s)", pj_ioqueue_name()));
    if ((status=unregister_test(allow_concur)) != 0) {
	return status;
    }
    PJ_LOG(3, (THIS_FILE, "....unregister test ok"));

    if ((status=many_handles_test(allow_concur)) != 0) {
	return status;
    }
    
    //return 0;

    PJ_LOG(4, (THIS_FILE, "...benchmarking different buffer size:"));
    PJ_LOG(4, (THIS_FILE, "... note: buf=bytes sent, fds=# of fds, "
			  "elapsed=in timer ticks"));

//pass1:
    PJ_LOG(3, (THIS_FILE, "...Benchmarking poll times for %s:", pj_ioqueue_name()));
    PJ_LOG(3, (THIS_FILE, "...====================================="));
    PJ_LOG(3, (THIS_FILE, "...Buf.size   #inactive-socks  Time/poll"));
    PJ_LOG(3, (THIS_FILE, "... (bytes)                    (nanosec)"));
    PJ_LOG(3, (THIS_FILE, "...====================================="));

    //goto pass2;

    for (bufsize=BUF_MIN_SIZE; bufsize <= BUF_MAX_SIZE; bufsize *= 2) {
	if ((status=bench_test(allow_concur, bufsize, SOCK_INACTIVE_MIN)) != 0)
	    return status;
    }
//pass2:
    bufsize = 512;
    for (sock_count=SOCK_INACTIVE_MIN+2; 
	 sock_count<=SOCK_INACTIVE_MAX+2; 
	 sock_count *= 2) 
    {
	//PJ_LOG(3,(THIS_FILE, "...testing with %d fds", sock_count));
	if ((status=bench_test(allow_concur, bufsize, sock_count-2)) != 0)
	    return status;
    }
    return 0;
}

int udp_ioqueue_test()
{
    int rc;

    rc = udp_ioqueue_test_imp(PJ_TRUE);
    if (rc != 0)
	return rc;

    rc = udp_ioqueue_test_imp(PJ_FALSE);
    if (rc != 0)
	return rc;

    return 0;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_uiq_udp;
#endif	/* INCLUDE_UDP_IOQUEUE_TEST */

