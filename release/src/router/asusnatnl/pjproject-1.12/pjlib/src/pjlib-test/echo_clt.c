/* $Id: echo_clt.c 3553 2011-05-05 06:14:19Z nanang $ */
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

#if INCLUDE_ECHO_CLIENT

enum { BUF_SIZE = 512 };

struct client
{
    int sock_type;
    const char *server;
    int port;
};

static pj_atomic_t *totalBytes;
static pj_atomic_t *timeout_counter;
static pj_atomic_t *invalid_counter;

#define MSEC_PRINT_DURATION 1000

static int wait_socket(pj_sock_t sock, unsigned msec_timeout)
{
    pj_fd_set_t fdset;
    pj_time_val timeout;

    timeout.sec = 0;
    timeout.msec = msec_timeout;
    pj_time_val_normalize(&timeout);

    PJ_FD_ZERO(&fdset);
    PJ_FD_SET(sock, &fdset);
    
    return pj_sock_select(FD_SETSIZE, &fdset, NULL, NULL, &timeout);
}

static int echo_client_thread(void *arg)
{
    pj_sock_t sock;
    char send_buf[BUF_SIZE];
    char recv_buf[BUF_SIZE];
    pj_sockaddr_in addr;
    pj_str_t s;
    pj_status_t rc;
    pj_uint32_t buffer_id;
    pj_uint32_t buffer_counter;
    struct client *client = arg;
    pj_status_t last_recv_err = PJ_SUCCESS, last_send_err = PJ_SUCCESS;
    unsigned counter = 0;

    rc = app_socket(pj_AF_INET(), client->sock_type, 0, -1, &sock);
    if (rc != PJ_SUCCESS) {
        app_perror("...unable to create socket", rc);
        return -10;
    }

    rc = pj_sockaddr_in_init( &addr, pj_cstr(&s, client->server), 
                              (pj_uint16_t)client->port);
    if (rc != PJ_SUCCESS) {
        app_perror("...unable to resolve server", rc);
        return -15;
    }

    rc = pj_sock_connect(sock, &addr, sizeof(addr));
    if (rc != PJ_SUCCESS) {
        app_perror("...connect() error", rc);
        pj_sock_close(sock);
        return -20;
    }

    PJ_LOG(3,("", "...socket connected to %s:%d", 
		  pj_inet_ntoa(addr.sin_addr),
		  pj_ntohs(addr.sin_port)));

    pj_memset(send_buf, 'A', BUF_SIZE);
    send_buf[BUF_SIZE-1]='\0';

    /* Give other thread chance to initialize themselves! */
    pj_thread_sleep(200);

    //PJ_LOG(3,("", "...thread %p running", pj_thread_this()));

    buffer_id = (pj_uint32_t) pj_thread_this();
    buffer_counter = 0;

    *(pj_uint32_t*)send_buf = buffer_id;

    for (;;) {
        int rc;
        pj_ssize_t bytes;

	++counter;

	//while (wait_socket(sock,0) > 0)
	//    ;

        /* Send a packet. */
        bytes = BUF_SIZE;
	*(pj_uint32_t*)(send_buf+4) = ++buffer_counter;
        rc = pj_sock_send(sock, send_buf, &bytes, 0);
        if (rc != PJ_SUCCESS || bytes != BUF_SIZE) {
            if (rc != last_send_err) {
                app_perror("...send() error", rc);
                PJ_LOG(3,("", "...ignoring subsequent error.."));
                last_send_err = rc;
                pj_thread_sleep(100);
            }
            continue;
        }

        rc = wait_socket(sock, 500);
        if (rc == 0) {
            PJ_LOG(3,("", "...timeout"));
	    bytes = 0;
	    pj_atomic_inc(timeout_counter);
	} else if (rc < 0) {
	    rc = pj_get_netos_error();
	    app_perror("...select() error", rc);
	    break;
        } else {
            /* Receive back the original packet. */
            bytes = 0;
            do {
                pj_ssize_t received = BUF_SIZE - bytes;
                rc = pj_sock_recv(sock, recv_buf+bytes, &received, 0);
                if (rc != PJ_SUCCESS || received == 0) {
                    if (rc != last_recv_err) {
                        app_perror("...recv() error", rc);
                        PJ_LOG(3,("", "...ignoring subsequent error.."));
                        last_recv_err = rc;
                        pj_thread_sleep(100);
                    }
                    bytes = 0;
		    received = 0;
                    break;
                }
                bytes += received;
            } while (bytes != BUF_SIZE && bytes != 0);
        }

        if (bytes == 0)
            continue;

        if (pj_memcmp(send_buf, recv_buf, BUF_SIZE) != 0) {
	    recv_buf[BUF_SIZE-1] = '\0';
            PJ_LOG(3,("", "...error: buffer %u has changed!\n"
			  "send_buf=%s\n"
			  "recv_buf=%s\n", 
			  counter, send_buf, recv_buf));
	    pj_atomic_inc(invalid_counter);
        }

        /* Accumulate total received. */
	pj_atomic_add(totalBytes, bytes);
    }

    pj_sock_close(sock);
    return 0;
}

int echo_client(int sock_type, const char *server, int port)
{
    pj_pool_t *pool;
    pj_thread_t *thread[ECHO_CLIENT_MAX_THREADS];
    pj_status_t rc;
    struct client client;
    int i;
    pj_atomic_value_t last_received;
    pj_timestamp last_report;

    client.sock_type = sock_type;
    client.server = server;
    client.port = port;

    pool = pj_pool_create( mem, NULL, 4000, 4000, NULL );

    rc = pj_atomic_create(pool, 0, &totalBytes);
    if (rc != PJ_SUCCESS) {
        PJ_LOG(3,("", "...error: unable to create atomic variable", rc));
        return -30;
    }
    rc = pj_atomic_create(pool, 0, &invalid_counter);
    rc = pj_atomic_create(pool, 0, &timeout_counter);

    PJ_LOG(3,("", "Echo client started"));
    PJ_LOG(3,("", "  Destination: %s:%d", 
                  ECHO_SERVER_ADDRESS, ECHO_SERVER_START_PORT));
    PJ_LOG(3,("", "  Press Ctrl-C to exit"));

    for (i=0; i<ECHO_CLIENT_MAX_THREADS; ++i) {
        rc = pj_thread_create( pool, NULL, &echo_client_thread, &client, 
                               PJ_THREAD_DEFAULT_STACK_SIZE, 0,
                               &thread[i]);
        if (rc != PJ_SUCCESS) {
            app_perror("...error: unable to create thread", rc);
            return -10;
        }
    }

    last_received = 0;
    pj_get_timestamp(&last_report);

    for (;;) {
	pj_timestamp now;
	unsigned long received, cur_received;
	unsigned msec;
	pj_highprec_t bw;
	pj_time_val elapsed;
	unsigned bw32;
	pj_uint32_t timeout, invalid;

	pj_thread_sleep(1000);

	pj_get_timestamp(&now);
	elapsed = pj_elapsed_time(&last_report, &now);
	msec = PJ_TIME_VAL_MSEC(elapsed);

	received = pj_atomic_get(totalBytes);
	cur_received = received - last_received;
	
	bw = cur_received;
	pj_highprec_mul(bw, 1000);
	pj_highprec_div(bw, msec);

	bw32 = (unsigned)bw;
	
	last_report = now;
	last_received = received;

	timeout = pj_atomic_get(timeout_counter);
	invalid = pj_atomic_get(invalid_counter);

        PJ_LOG(3,("", 
	          "...%d threads, total bandwidth: %d KB/s, "
		  "timeout=%d, invalid=%d", 
                  ECHO_CLIENT_MAX_THREADS, bw32/1000,
		  timeout, invalid));
    }

    for (i=0; i<ECHO_CLIENT_MAX_THREADS; ++i) {
        pj_thread_join( thread[i] );
    }

    pj_pool_release(pool);
    return 0;
}


#else
int dummy_echo_client;
#endif  /* INCLUDE_ECHO_CLIENT */
