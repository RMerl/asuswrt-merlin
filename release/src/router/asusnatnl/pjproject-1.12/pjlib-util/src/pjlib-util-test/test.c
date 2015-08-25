/* $Id$ */
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
#include <pjlib-util.h>

void app_perror(const char *msg, pj_status_t rc)
{
    char errbuf[256];

    PJ_CHECK_STACK();

    pj_strerror(rc, errbuf, sizeof(errbuf));
    PJ_LOG(1,("test", "%s: [pj_status_t=%d] %s", msg, rc, errbuf));
}

#define DO_TEST(test)	do { \
			    PJ_LOG(3, ("test", "Running %s...", #test));  \
			    rc = test; \
			    PJ_LOG(3, ("test",  \
				       "%s(%d)",  \
				       (char*)(rc ? "..ERROR" : "..success"), rc)); \
			    if (rc!=0) goto on_return; \
			} while (0)


pj_pool_factory *mem;

int param_log_decor = PJ_LOG_HAS_NEWLINE | PJ_LOG_HAS_TIME | 
		      PJ_LOG_HAS_MICRO_SEC;

static int test_inner(void)
{
    pj_caching_pool caching_pool;
    int rc = 0;

    mem = &caching_pool.factory;

    pj_log_set_level(0, 3);
    pj_log_set_decor(param_log_decor);

    rc = pj_init(0);
    if (rc != 0) {
	app_perror("pj_init() error!!", rc);
	return rc;
    }

    rc = pjlib_util_init();
    pj_assert(rc == 0);

    pj_dump_config();
    pj_caching_pool_init( 0, &caching_pool, &pj_pool_factory_default_policy, 0 );

#if INCLUDE_XML_TEST
    DO_TEST(xml_test());
#endif

#if INCLUDE_ENCRYPTION_TEST
    DO_TEST(encryption_test());
    DO_TEST(encryption_benchmark());
#endif

#if INCLUDE_STUN_TEST
    DO_TEST(stun_test());
#endif

#if INCLUDE_RESOLVER_TEST
    DO_TEST(resolver_test());
#endif

#if INCLUDE_HTTP_CLIENT_TEST
    DO_TEST(http_client_test());
#endif

on_return:
    return rc;
}

int test_main(void)
{
    PJ_USE_EXCEPTION;

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

