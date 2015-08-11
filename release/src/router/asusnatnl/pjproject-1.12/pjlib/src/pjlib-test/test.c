/* $Id: test.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifdef _MSC_VER
#  pragma warning(disable:4127)
#endif

#define DO_TEST(test)	do { \
			    PJ_LOG(3, ("test", "Running %s...", #test));  \
			    rc = test; \
			    PJ_LOG(3, ("test",  \
				       "%s(%d)",  \
				       (rc ? "..ERROR" : "..success"), rc)); \
			    if (rc!=0) goto on_return; \
			} while (0)


pj_pool_factory *mem;

int param_echo_sock_type;
const char *param_echo_server = ECHO_SERVER_ADDRESS;
int param_echo_port = ECHO_SERVER_START_PORT;
int param_log_decor = PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_TIME | 
		      PJ_LOG_HAS_MICRO_SEC;

int null_func()
{
    return 0;
}

int test_inner(void)
{
    pj_caching_pool caching_pool;
    const char *filename;
    int line;
    int rc = 0;

    mem = &caching_pool.factory;

    pj_log_set_level(0, 3);
    pj_log_set_decor(param_log_decor);

    rc = pj_init(0);
    if (rc != 0) {
	app_perror("pj_init() error!!", rc);
	return rc;
    }
    
    //pj_dump_config();
    pj_caching_pool_init( 0, &caching_pool, NULL, 0 );

#if INCLUDE_ERRNO_TEST
    DO_TEST( errno_test() );
#endif

#if INCLUDE_EXCEPTION_TEST
    DO_TEST( exception_test(0) );
#endif

#if INCLUDE_OS_TEST
    DO_TEST( os_test() );
#endif

#if INCLUDE_RAND_TEST
    DO_TEST( rand_test() );
#endif

#if INCLUDE_LIST_TEST
    DO_TEST( list_test() );
#endif

#if INCLUDE_POOL_TEST
    DO_TEST( pool_test() );
#endif

#if INCLUDE_POOL_PERF_TEST
    DO_TEST( pool_perf_test() );
#endif

#if INCLUDE_STRING_TEST
    DO_TEST( string_test() );
#endif
    
#if INCLUDE_FIFOBUF_TEST
    DO_TEST( fifobuf_test() );
#endif

#if INCLUDE_RBTREE_TEST
    DO_TEST( rbtree_test() );
#endif

#if INCLUDE_HASH_TEST
    DO_TEST( hash_test() );
#endif

#if INCLUDE_TIMESTAMP_TEST
    DO_TEST( timestamp_test() );
#endif

#if INCLUDE_ATOMIC_TEST
    DO_TEST( atomic_test() );
#endif

#if INCLUDE_MUTEX_TEST
    DO_TEST( mutex_test() );
#endif

#if INCLUDE_TIMER_TEST
    DO_TEST( timer_test() );
#endif

#if INCLUDE_SLEEP_TEST
    DO_TEST( sleep_test() );
#endif

#if INCLUDE_THREAD_TEST
    DO_TEST( thread_test() );
#endif

#if INCLUDE_SOCK_TEST
    DO_TEST( sock_test() );
#endif

#if INCLUDE_SOCK_PERF_TEST
    DO_TEST( sock_perf_test() );
#endif

#if INCLUDE_SELECT_TEST
    DO_TEST( select_test() );
#endif

#if INCLUDE_UDP_IOQUEUE_TEST
    DO_TEST( udp_ioqueue_test() );
#endif

#if PJ_HAS_TCP && INCLUDE_TCP_IOQUEUE_TEST
    DO_TEST( tcp_ioqueue_test() );
#endif

#if INCLUDE_IOQUEUE_PERF_TEST
    DO_TEST( ioqueue_perf_test() );
#endif

#if INCLUDE_IOQUEUE_UNREG_TEST
    DO_TEST( udp_ioqueue_unreg_test() );
#endif

#if INCLUDE_ACTIVESOCK_TEST
    DO_TEST( activesock_test() );
#endif

#if INCLUDE_FILE_TEST
    DO_TEST( file_test() );
#endif

#if INCLUDE_SSLSOCK_TEST
    DO_TEST( ssl_sock_test() );
#endif

#if INCLUDE_ECHO_SERVER
    //echo_server();
    //echo_srv_sync();
    udp_echo_srv_ioqueue();

#elif INCLUDE_ECHO_CLIENT
    if (param_echo_sock_type == 0)
        param_echo_sock_type = pj_SOCK_DGRAM();

    echo_client( param_echo_sock_type, 
                 param_echo_server, 
                 param_echo_port);
#endif

    goto on_return;

on_return:

    pj_caching_pool_destroy( &caching_pool );

    PJ_LOG(3,("test", ""));
 
    pj_thread_get_stack_info(pj_thread_this(), &filename, &line);
    PJ_LOG(3,("test", "Stack max usage: %u, deepest: %s:%u", 
	              pj_thread_get_stack_max_usage(pj_thread_this()),
		      filename, line));
    if (rc == 0)
	PJ_LOG(3,("test", "Looks like everything is okay!.."));
    else
	PJ_LOG(3,("test", "Test completed with error(s)"));
    
    pj_shutdown(0);
    
    return 0;
}

#include <pj/sock.h>

int test_main(void)
{
    int i;
    PJ_USE_EXCEPTION;

    i = pj_AF_INET();

    PJ_TRY(0) {
        return test_inner();
    }
    PJ_CATCH_ANY {
        int id = PJ_GET_EXCEPTION();
        PJ_LOG(3,("test", "FATAL: unhandled exception id %d (%s)", 
                  id, pj_exception_id_name(0, id)));
    }
    PJ_END(0);

    return -1;
}
