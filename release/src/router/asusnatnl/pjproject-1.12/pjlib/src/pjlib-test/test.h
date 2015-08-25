/* $Id: test.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJLIB_TEST_H__
#define __PJLIB_TEST_H__

#include <pj/types.h>

#define GROUP_LIBC                  1
#define GROUP_OS                    1
#define GROUP_DATA_STRUCTURE        1
#define GROUP_NETWORK               1
#if defined(PJ_SYMBIAN)
#   define GROUP_FILE               0
#else
#   define GROUP_FILE               1
#endif

#define INCLUDE_ERRNO_TEST          GROUP_LIBC
#define INCLUDE_TIMESTAMP_TEST      GROUP_OS
#define INCLUDE_EXCEPTION_TEST	    GROUP_LIBC
#define INCLUDE_RAND_TEST	    GROUP_LIBC
#define INCLUDE_LIST_TEST	    GROUP_DATA_STRUCTURE
#define INCLUDE_HASH_TEST	    GROUP_DATA_STRUCTURE
#define INCLUDE_POOL_TEST	    GROUP_LIBC
#define INCLUDE_POOL_PERF_TEST	    GROUP_LIBC
#define INCLUDE_STRING_TEST	    GROUP_DATA_STRUCTURE
#define INCLUDE_FIFOBUF_TEST	    0	// GROUP_DATA_STRUCTURE
#define INCLUDE_RBTREE_TEST	    GROUP_DATA_STRUCTURE
#define INCLUDE_TIMER_TEST	    GROUP_DATA_STRUCTURE
#define INCLUDE_ATOMIC_TEST         GROUP_OS
#define INCLUDE_MUTEX_TEST	    (PJ_HAS_THREADS && GROUP_OS)
#define INCLUDE_SLEEP_TEST          GROUP_OS
#define INCLUDE_OS_TEST             GROUP_OS
#define INCLUDE_THREAD_TEST         (PJ_HAS_THREADS && GROUP_OS)
#define INCLUDE_SOCK_TEST	    GROUP_NETWORK
#define INCLUDE_SOCK_PERF_TEST	    GROUP_NETWORK
#define INCLUDE_SELECT_TEST	    GROUP_NETWORK
#define INCLUDE_UDP_IOQUEUE_TEST    GROUP_NETWORK
#define INCLUDE_TCP_IOQUEUE_TEST    GROUP_NETWORK
#define INCLUDE_ACTIVESOCK_TEST	    GROUP_NETWORK
#define INCLUDE_SSLSOCK_TEST	    (PJ_HAS_SSL_SOCK && GROUP_NETWORK)
#define INCLUDE_IOQUEUE_PERF_TEST   (PJ_HAS_THREADS && GROUP_NETWORK)
#define INCLUDE_IOQUEUE_UNREG_TEST  (PJ_HAS_THREADS && GROUP_NETWORK)
#define INCLUDE_FILE_TEST           GROUP_FILE

#define INCLUDE_ECHO_SERVER         0
#define INCLUDE_ECHO_CLIENT         0


#define ECHO_SERVER_MAX_THREADS     2
#define ECHO_SERVER_START_PORT      65000
#define ECHO_SERVER_ADDRESS         "compaq.home"
#define ECHO_SERVER_DURATION_MSEC   (60*60*1000)

#define ECHO_CLIENT_MAX_THREADS     6

PJ_BEGIN_DECL

extern int errno_test(void);
extern int timestamp_test(void);
extern int exception_test(int inst_id);
extern int rand_test(void);
extern int list_test(void);
extern int hash_test(void);
extern int os_test(void);
extern int pool_test(void);
extern int pool_perf_test(void);
extern int string_test(void);
extern int fifobuf_test(void);
extern int timer_test(void);
extern int rbtree_test(void);
extern int atomic_test(void);
extern int mutex_test(void);
extern int sleep_test(void);
extern int thread_test(void);
extern int sock_test(void);
extern int sock_perf_test(void);
extern int select_test(void);
extern int udp_ioqueue_test(void);
extern int udp_ioqueue_unreg_test(void);
extern int tcp_ioqueue_test(void);
extern int ioqueue_perf_test(void);
extern int activesock_test(void);
extern int file_test(void);
extern int ssl_sock_test(void);

extern int echo_server(void);
extern int echo_client(int sock_type, const char *server, int port);

extern int echo_srv_sync(void);
extern int udp_echo_srv_ioqueue(void);
extern int echo_srv_common_loop(pj_atomic_t *bytes_counter);


extern pj_pool_factory *mem;

extern int          test_main(void);
extern void         app_perror(const char *msg, pj_status_t err);
extern pj_status_t  app_socket(int family, int type, int proto, int port,
                               pj_sock_t *ptr_sock);
extern pj_status_t  app_socketpair(int family, int type, int protocol,
                                   pj_sock_t *server, pj_sock_t *client);
extern int	    null_func(void);

//#define TRACE_(expr) PJ_LOG(3,expr)
#define TRACE_(expr)
#define HALT(msg)   { PJ_LOG(3,(THIS_FILE,"%s halted",msg)); for(;;) sleep(1); }

PJ_END_DECL

#endif	/* __PJLIB_TEST_H__ */

