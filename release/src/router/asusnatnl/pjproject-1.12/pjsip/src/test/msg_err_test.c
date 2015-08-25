/* $Id: msg_err_test.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjsip.h>
#include <pjlib.h>

#define THIS_FILE   "msg_err_test.c"


static pj_bool_t verify_success(pjsip_msg *msg,
				pjsip_parser_err_report *err_list)
{
    PJ_UNUSED_ARG(msg);
    PJ_UNUSED_ARG(err_list);

    return PJ_TRUE;
}

static struct test_entry
{
    char	msg[1024];
    pj_bool_t (*verify)(pjsip_msg *msg,
		        pjsip_parser_err_report *err_list);

} test_entries[] = 
{
    /* Syntax error in status line */
    {
	"SIP/2.0 200\r\n"
	"H-Name: H-Value\r\n"
	"\r\n",
	&verify_success
    },

    /* Syntax error in header */
    {
	"SIP/2.0 200 OK\r\n"
	"Via: SIP/2.0\r\n"
	"H-Name: H-Value\r\n"
	"\r\n",
	&verify_success
    },

    /* Multiple syntax errors in headers */
    {
	"SIP/2.0 200 OK\r\n"
	"Via: SIP/2.0\r\n"
	"H-Name: H-Value\r\n"
	"Via: SIP/2.0\r\n"
	"\r\n",
	&verify_success
    }
};


int msg_err_test(void)
{
    pj_pool_t *pool;
    unsigned i;

    PJ_LOG(3,(THIS_FILE, "Testing parsing error"));

    pool = pjsip_endpt_create_pool(endpt, "msgerrtest", 4000, 4000);

    for (i=0; i<PJ_ARRAY_SIZE(test_entries); ++i) {
	pjsip_parser_err_report err_list, *e;
	pjsip_msg *msg;

	PJ_LOG(3,(THIS_FILE, "  Parsing msg %d", i));
	pj_list_init(&err_list);
	msg = pjsip_parse_msg(0, pool, test_entries[i].msg,
			      strlen(test_entries[i].msg), &err_list);

	e = err_list.next;
	while (e != &err_list) {
	    PJ_LOG(3,(THIS_FILE, 
		      "   reported syntax error at line %d col %d for %.*s",
		      e->line, e->col,
		      (int)e->hname.slen,
		      e->hname.ptr));
	    e = e->next;
	}
    }

    pj_pool_release(pool);
    return 0;
}
