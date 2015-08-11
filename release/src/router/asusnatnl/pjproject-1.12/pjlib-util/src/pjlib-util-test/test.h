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
#include <pj/types.h>

#define INCLUDE_XML_TEST	    1
#define INCLUDE_ENCRYPTION_TEST	    1
#define INCLUDE_STUN_TEST	    1
#define INCLUDE_RESOLVER_TEST	    1
#define INCLUDE_HTTP_CLIENT_TEST    1

extern int xml_test(void);
extern int encryption_test();
extern int encryption_benchmark();
extern int stun_test();
extern int test_main(void);
extern int resolver_test(void);
extern int http_client_test();

extern void app_perror(const char *title, pj_status_t rc);
extern pj_pool_factory *mem;

