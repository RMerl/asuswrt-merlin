/* $Id: strerror.c 3553 2011-05-05 06:14:19Z nanang $ */
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

/**
 * \page page_strerror_c Samples: Print out error message
 *
 * This file is pjsip-apps/src/samples/strerror.c
 *
 * \includelineno strerror.c
 */


#include <pjlib.h>
#include <pjlib-util.h>
#include <pjsip.h>
#include <pjmedia.h>
#include <pjnath.h>
#include <pjsip_simple.h>

/*
 * main()
 */
int main(int argc, char *argv[])
{
    pj_caching_pool cp;
    pjmedia_endpt *med_ept;
    pjsip_endpoint *sip_ept;
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_status_t code;

    if (argc != 2) {
	puts("Usage: strerror ERRNUM");
	return 1;
    }

    pj_log_set_level(0, 3);

    pj_init(0);
    pj_caching_pool_init(0, &cp, NULL, 0);
    pjlib_util_init();
    pjnath_init();
    //pjmedia_endpt_create(&cp.factory, NULL, 0, &med_ept);
    pjmedia_endpt_create(0, &cp.factory, NULL, 0, 0, &med_ept);
    pjsip_endpt_create(&cp.factory, "localhost", &sip_ept);
    pjsip_evsub_init_module(sip_ept);

    code = atoi(argv[1]);
    pj_strerror(code, errmsg, sizeof(errmsg));

    printf("Status %d: %s\n", code, errmsg);

    pj_shutdown(0);
    return 0;
}

