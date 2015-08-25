/* $Id: udp_echo_srv_sync.c 4105 2012-04-26 23:45:09Z bennylp $ */
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

static pj_atomic_t *total_bytes;
static pj_bool_t thread_quit_flag = 0;

static int worker_thread(void *arg)
{
    pj_sock_t    sock = (pj_sock_t)arg;
    char         buf[512];
    pj_status_t  last_recv_err = PJ_SUCCESS, last_write_err = PJ_SUCCESS;

    while (!thread_quit_flag) {
        pj_ssize_t len;
        pj_status_t rc;
        pj_sockaddr_in addr;
        int addrlen;

        len = sizeof(buf);
        addrlen = sizeof(addr);
        rc = pj_sock_recvfrom(sock, buf, &len, 0, &addr, &addrlen);
        if (rc != 0) {
            if (rc != last_recv_err) {
                app_perror("...recv error", rc);
                last_recv_err = rc;
            }
            continue;
        }

        pj_atomic_add(total_bytes, len);

        rc = pj_sock_sendto(sock, buf, &len, 0, &addr, addrlen);
        if (rc != PJ_SUCCESS) {
            if (rc != last_write_err) {
                app_perror("...send error", rc);
                last_write_err = rc;
            }
            continue;
        }
    }
    return 0;
}


int echo_srv_sync(void)
{
    pj_pool_t *pool;
    pj_sock_t sock;
    pj_thread_t *thread[ECHO_SERVER_MAX_THREADS];
    pj_status_t rc;
    int i;

    pool = pj_pool_create(mem, NULL, 4000, 4000, NULL);
    if (!pool)
        return -5;

    rc = pj_atomic_create(pool, 0, &total_bytes);
    if (rc != PJ_SUCCESS) {
        app_perror("...unable to create atomic_var", rc);
        return -6;
    }

    rc = app_socket(pj_AF_INET(), pj_SOCK_DGRAM(),0, ECHO_SERVER_START_PORT, &sock);
    if (rc != PJ_SUCCESS) {
        app_perror("...socket error", rc);
        return -10;
    }

    for (i=0; i<ECHO_SERVER_MAX_THREADS; ++i) {
        rc = pj_thread_create(pool, NULL, &worker_thread, (void*)sock,
                              PJ_THREAD_DEFAULT_STACK_SIZE, 0,
                              &thread[i]);
        if (rc != PJ_SUCCESS) {
            app_perror("...unable to create thread", rc);
            return -20;
        }
    }

    PJ_LOG(3,("", "...UDP echo server running with %d threads at port %d",
                  ECHO_SERVER_MAX_THREADS, ECHO_SERVER_START_PORT));
    PJ_LOG(3,("", "...Press Ctrl-C to abort"));

    echo_srv_common_loop(total_bytes);
    return 0;
}


int echo_srv_common_loop(pj_atomic_t *bytes_counter)
{
    pj_highprec_t last_received, avg_bw, highest_bw;
    pj_time_val last_print;
    unsigned count;
    const char *ioqueue_name;

    last_received = 0;
    pj_gettimeofday(&last_print);
    avg_bw = highest_bw = 0;
    count = 0;

    ioqueue_name = pj_ioqueue_name();

    for (;;) {
        pj_highprec_t received, cur_received, bw;
        unsigned msec;
        pj_time_val now, duration;

        pj_thread_sleep(1000);

        received = cur_received = pj_atomic_get(bytes_counter);
        cur_received = cur_received - last_received;

        pj_gettimeofday(&now);
        duration = now;
        PJ_TIME_VAL_SUB(duration, last_print);
        msec = PJ_TIME_VAL_MSEC(duration);
        
        bw = cur_received;
        pj_highprec_mul(bw, 1000);
        pj_highprec_div(bw, msec);

        last_print = now;
        last_received = received;

        avg_bw = avg_bw + bw;
        count++;

        PJ_LOG(3,("", "%s UDP (%d threads): %u KB/s (avg=%u KB/s) %s", 
		  ioqueue_name,
                  ECHO_SERVER_MAX_THREADS, 
                  (unsigned)(bw / 1000),
                  (unsigned)(avg_bw / count / 1000),
                  (count==20 ? "<ses avg>" : "")));

        if (count==20) {
            if (avg_bw/count > highest_bw)
                highest_bw = avg_bw/count;

            count = 0;
            avg_bw = 0;

            PJ_LOG(3,("", "Highest average bandwidth=%u KB/s",
                          (unsigned)(highest_bw/1000)));
        }
    }
    PJ_UNREACHED(return 0;)
}

