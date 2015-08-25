/* $Id: ioq_unreg.c 3553 2011-05-05 06:14:19Z nanang $ */
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

#if INCLUDE_IOQUEUE_UNREG_TEST
/*
 * This tests the thread safety of ioqueue unregistration operation.
 */

#include <pj/errno.h>
#include <pj/ioqueue.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <pj/sock.h>
#include <pj/compat/socket.h>
#include <pj/string.h>


#define THIS_FILE   "ioq_unreg.c"


enum test_method
{
    UNREGISTER_IN_APP,
    UNREGISTER_IN_CALLBACK,
};

static int thread_quitting;
static enum test_method test_method;
static pj_time_val time_to_unregister;

struct sock_data
{
    pj_sock_t		 sock;
    pj_sock_t		 csock;
    pj_pool_t		*pool;
    pj_ioqueue_key_t	*key;
    pj_mutex_t		*mutex;
    pj_ioqueue_op_key_t	*op_key;
    char		*buffer;
    pj_size_t		 bufsize;
    pj_bool_t		 unregistered;
    unsigned		 received;
} sock_data;

static void on_read_complete(pj_ioqueue_key_t *key, 
                             pj_ioqueue_op_key_t *op_key, 
                             pj_ssize_t bytes_read)
{
    pj_ssize_t size;
    char *sendbuf = "Hello world";
    pj_status_t status;

    if (sock_data.unregistered)
	return;

    pj_mutex_lock(sock_data.mutex);

    if (sock_data.unregistered) {
	pj_mutex_unlock(sock_data.mutex);
	return;
    }

    if (bytes_read < 0) {
	if (-bytes_read != PJ_STATUS_FROM_OS(PJ_BLOCKING_ERROR_VAL))
	    app_perror("ioqueue reported recv error", -bytes_read);
    } else {
	sock_data.received += bytes_read;
    }

    if (test_method == UNREGISTER_IN_CALLBACK) {
	pj_time_val now;

	pj_gettimeofday(&now);
	if (PJ_TIME_VAL_GTE(now, time_to_unregister)) { 
	    sock_data.unregistered = 1;
	    pj_ioqueue_unregister(key);
	    pj_mutex_unlock(sock_data.mutex);
	    return;
	}
    }
 
    do { 
	size = sock_data.bufsize;
	status = pj_ioqueue_recv(key, op_key, sock_data.buffer, &size, 0);
	if (status != PJ_EPENDING && status != PJ_SUCCESS)
	    app_perror("recv() error", status);

    } while (status == PJ_SUCCESS);

    pj_mutex_unlock(sock_data.mutex);

    size = pj_ansi_strlen(sendbuf);
    status = pj_sock_send(sock_data.csock, sendbuf, &size, 0);
    if (status != PJ_SUCCESS)
	app_perror("send() error", status);

    size = pj_ansi_strlen(sendbuf);
    status = pj_sock_send(sock_data.csock, sendbuf, &size, 0);
    if (status != PJ_SUCCESS)
	app_perror("send() error", status);

} 

static int worker_thread(void *arg)
{
    pj_ioqueue_t *ioqueue = (pj_ioqueue_t*) arg;

    while (!thread_quitting) {
	pj_time_val timeout = { 0, 20 };
	pj_ioqueue_poll(ioqueue, &timeout);
    }

    return 0;
}

/*
 * Perform unregistration test.
 *
 * This will create ioqueue and register a server socket. Depending
 * on the test method, either the callback or the main thread will
 * unregister and destroy the server socket after some period of time.
 */
static int perform_unreg_test(pj_ioqueue_t *ioqueue,
			      pj_pool_t *test_pool,
			      const char *title, 
			      pj_bool_t other_socket)
{
    enum { WORKER_CNT = 1, MSEC = 500, QUIT_MSEC = 500 };
    int i;
    pj_thread_t *thread[WORKER_CNT];
    struct sock_data osd;
    pj_ioqueue_callback callback;
    pj_time_val end_time;
    pj_status_t status;


    /* Sometimes its important to have other sockets registered to
     * the ioqueue, because when no sockets are registered, the ioqueue
     * will return from the poll early.
     */
    if (other_socket) {
	status = app_socket(pj_AF_INET(), pj_SOCK_DGRAM(), 0, 56127, &osd.sock);
	if (status != PJ_SUCCESS) {
	    app_perror("Error creating other socket", status);
	    return -12;
	}

	pj_bzero(&callback, sizeof(callback));
	status = pj_ioqueue_register_sock(test_pool, ioqueue, osd.sock,
					  NULL, &callback, &osd.key);
	if (status != PJ_SUCCESS) {
	    app_perror("Error registering other socket", status);
	    return -13;
	}

    } else {
	osd.key = NULL;
	osd.sock = PJ_INVALID_SOCKET;
    }

    /* Init both time duration of testing */
    thread_quitting = 0;
    pj_gettimeofday(&time_to_unregister);
    time_to_unregister.msec += MSEC;
    pj_time_val_normalize(&time_to_unregister);

    end_time = time_to_unregister;
    end_time.msec += QUIT_MSEC;
    pj_time_val_normalize(&end_time);

    
    /* Create polling thread */
    for (i=0; i<WORKER_CNT; ++i) {
	status = pj_thread_create(test_pool, "unregtest", &worker_thread,
				   ioqueue, 0, 0, &thread[i]);
	if (status != PJ_SUCCESS) {
	    app_perror("Error creating thread", status);
	    return -20;
	}
    }

    /* Create pair of client/server sockets */
    status = app_socketpair(pj_AF_INET(), pj_SOCK_DGRAM(), 0, 
			    &sock_data.sock, &sock_data.csock);
    if (status != PJ_SUCCESS) {
	app_perror("app_socketpair error", status);
	return -30;
    }


    /* Initialize test data */
    sock_data.pool = pj_pool_create(mem, "sd", 1000, 1000, NULL);
    sock_data.buffer = (char*) pj_pool_alloc(sock_data.pool, 128);
    sock_data.bufsize = 128;
    sock_data.op_key = (pj_ioqueue_op_key_t*) 
    		       pj_pool_alloc(sock_data.pool, 
				     sizeof(*sock_data.op_key));
    sock_data.received = 0;
    sock_data.unregistered = 0;

    pj_ioqueue_op_key_init(sock_data.op_key, sizeof(*sock_data.op_key));

    status = pj_mutex_create_simple(sock_data.pool, "sd", &sock_data.mutex);
    if (status != PJ_SUCCESS) {
	app_perror("create_mutex() error", status);
	return -35;
    }

    /* Register socket to ioqueue */
    pj_bzero(&callback, sizeof(callback));
    callback.on_read_complete = &on_read_complete;
    status = pj_ioqueue_register_sock(sock_data.pool, ioqueue, sock_data.sock,
				      NULL, &callback, &sock_data.key);
    if (status != PJ_SUCCESS) {
	app_perror("pj_ioqueue_register error", status);
	return -40;
    }

    /* Bootstrap the first send/receive */
    on_read_complete(sock_data.key, sock_data.op_key, 0);

    /* Loop until test time ends */
    for (;;) {
	pj_time_val now, timeout;
	int n;

	pj_gettimeofday(&now);

	if (test_method == UNREGISTER_IN_APP && 
	    PJ_TIME_VAL_GTE(now, time_to_unregister) &&
	    !sock_data.unregistered) 
	{
	    sock_data.unregistered = 1;
	    /* Wait (as much as possible) for callback to complete */
	    pj_mutex_lock(sock_data.mutex);
	    pj_mutex_unlock(sock_data.mutex);
	    pj_ioqueue_unregister(sock_data.key);
	}

	if (PJ_TIME_VAL_GT(now, end_time) && sock_data.unregistered)
	    break;

	timeout.sec = 0; timeout.msec = 10;
	n = pj_ioqueue_poll(ioqueue, &timeout);
	if (n < 0) {
	    app_perror("pj_ioqueue_poll error", -n);
	    pj_thread_sleep(1);
	}
    }

    thread_quitting = 1;

    for (i=0; i<WORKER_CNT; ++i) {
	pj_thread_join(thread[i]);
	pj_thread_destroy(thread[i]);
    }

    /* Destroy data */
    pj_mutex_destroy(sock_data.mutex);
    pj_pool_release(sock_data.pool);
    sock_data.pool = NULL;

    if (other_socket) {
	pj_ioqueue_unregister(osd.key);
    }

    pj_sock_close(sock_data.csock);

    PJ_LOG(3,(THIS_FILE, "....%s: done (%d KB/s)",
	      title, sock_data.received * 1000 / MSEC / 1000));
    return 0;
}

static int udp_ioqueue_unreg_test_imp(pj_bool_t allow_concur)
{
    enum { LOOP = 10 };
    int i, rc;
    char title[30];
    pj_ioqueue_t *ioqueue;
    pj_pool_t *test_pool;
	
    PJ_LOG(3,(THIS_FILE, "..testing with concurency=%d", allow_concur));

    test_method = UNREGISTER_IN_APP;

    test_pool = pj_pool_create(mem, "unregtest", 4000, 4000, NULL);

    rc = pj_ioqueue_create(test_pool, 16, &ioqueue);
    if (rc != PJ_SUCCESS) {
	app_perror("Error creating ioqueue", rc);
	return -10;
    }

    rc = pj_ioqueue_set_default_concurrency(ioqueue, allow_concur);
    if (rc != PJ_SUCCESS) {
	app_perror("Error in pj_ioqueue_set_default_concurrency()", rc);
	return -12;
    }

    PJ_LOG(3, (THIS_FILE, "...ioqueue unregister stress test 0/3, unregister in app (%s)", 
	       pj_ioqueue_name()));
    for (i=0; i<LOOP; ++i) {
	pj_ansi_sprintf(title, "repeat %d/%d", i, LOOP);
	rc = perform_unreg_test(ioqueue, test_pool, title, 0);
	if (rc != 0)
	    return rc;
    }


    PJ_LOG(3, (THIS_FILE, "...ioqueue unregister stress test 1/3, unregister in app (%s)",
	       pj_ioqueue_name()));
    for (i=0; i<LOOP; ++i) {
	pj_ansi_sprintf(title, "repeat %d/%d", i, LOOP);
	rc = perform_unreg_test(ioqueue, test_pool, title, 1);
	if (rc != 0)
	    return rc;
    }

    test_method = UNREGISTER_IN_CALLBACK;

    PJ_LOG(3, (THIS_FILE, "...ioqueue unregister stress test 2/3, unregister in cb (%s)", 
	       pj_ioqueue_name()));
    for (i=0; i<LOOP; ++i) {
	pj_ansi_sprintf(title, "repeat %d/%d", i, LOOP);
	rc = perform_unreg_test(ioqueue, test_pool, title, 0);
	if (rc != 0)
	    return rc;
    }


    PJ_LOG(3, (THIS_FILE, "...ioqueue unregister stress test 3/3, unregister in cb (%s)", 
	       pj_ioqueue_name()));
    for (i=0; i<LOOP; ++i) {
	pj_ansi_sprintf(title, "repeat %d/%d", i, LOOP);
	rc = perform_unreg_test(ioqueue, test_pool, title, 1);
	if (rc != 0)
	    return rc;
    }

    pj_ioqueue_destroy(ioqueue);
    pj_pool_release(test_pool);

    return 0;
}

int udp_ioqueue_unreg_test(void)
{
    int rc;

    rc = udp_ioqueue_unreg_test_imp(PJ_TRUE);
    if (rc != 0)
    	return rc;

    rc = udp_ioqueue_unreg_test_imp(PJ_FALSE);
    if (rc != 0)
	return rc;

    return 0;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_uiq_unreg;
#endif	/* INCLUDE_IOQUEUE_UNREG_TEST */


