/* $Id: transport_loop_test.c 3553 2011-05-05 06:14:19Z nanang $ */
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

#define THIS_FILE   "transport_loop_test.c"

static int datagram_loop_test()
{
    enum { LOOP = 8 };
    pjsip_transport *loop;
    int i, pkt_lost;
    pj_sockaddr_in addr;
    pj_status_t status;
    long ref_cnt;
    int rtt[LOOP], min_rtt;

    PJ_LOG(3,(THIS_FILE, "testing datagram loop transport"));

    /* Test acquire transport. */
    status = pjsip_endpt_acquire_transport( endpt, PJSIP_TRANSPORT_LOOP_DGRAM,
					    &addr, sizeof(addr), NULL, &loop);
    if (status != PJ_SUCCESS) {
	app_perror("   error: loop transport is not configured", status);
	return -20;
    }

    /* Get initial reference counter */
    ref_cnt = pj_atomic_get(loop->ref_cnt);

    /* Test basic transport attributes */
    status = generic_transport_test(loop);
    if (status != PJ_SUCCESS)
	return status;

    /* Basic transport's send/receive loopback test. */
    for (i=0; i<LOOP; ++i) {
	status = transport_send_recv_test(PJSIP_TRANSPORT_LOOP_DGRAM, loop, 
					  "sip:bob@130.0.0.1;transport=loop-dgram",
					  &rtt[i]);
	if (status != 0)
	    return status;
    }

    min_rtt = 0xFFFFFFF;
    for (i=0; i<LOOP; ++i)
	if (rtt[i] < min_rtt) min_rtt = rtt[i];

    report_ival("loop-rtt-usec", min_rtt, "usec",
		"Best Loopback transport round trip time, in microseconds "
		"(time from sending request until response is received. "
		"Tests were performed on local machine only)");


    /* Multi-threaded round-trip test. */
    status = transport_rt_test(PJSIP_TRANSPORT_LOOP_DGRAM, loop, 
			       "sip:bob@130.0.0.1;transport=loop-dgram",
			       &pkt_lost);
    if (status != 0)
	return status;

    if (pkt_lost != 0) {
	PJ_LOG(3,(THIS_FILE, "   error: %d packet(s) was lost", pkt_lost));
	return -40;
    }

    /* Put delay. */
    PJ_LOG(3,(THIS_FILE,"  setting network delay to 10 ms"));
    pjsip_loop_set_delay(loop, 10);

    /* Multi-threaded round-trip test. */
    status = transport_rt_test(PJSIP_TRANSPORT_LOOP_DGRAM, loop, 
			       "sip:bob@130.0.0.1;transport=loop-dgram",
			       &pkt_lost);
    if (status != 0)
	return status;

    if (pkt_lost != 0) {
	PJ_LOG(3,(THIS_FILE, "   error: %d packet(s) was lost", pkt_lost));
	return -50;
    }

    /* Restore delay. */
    pjsip_loop_set_delay(loop, 0);

    /* Check reference counter. */
    if (pj_atomic_get(loop->ref_cnt) != ref_cnt) {
	PJ_LOG(3,(THIS_FILE, "   error: ref counter is not %d (%d)", 
			     ref_cnt, pj_atomic_get(loop->ref_cnt)));
	return -51;
    }

    /* Decrement reference. */
    pjsip_transport_dec_ref(loop);

    return 0;
}

int transport_loop_test(void)
{
    int status;

    status = datagram_loop_test();
    if (status != 0)
	return status;

    return 0;
}
