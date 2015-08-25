/* $Id: transport_tcp_test.c 3553 2011-05-05 06:14:19Z nanang $ */
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

#define THIS_FILE   "transport_tcp_test.c"


/*
 * TCP transport test.
 */
#if PJ_HAS_TCP
int transport_tcp_test(void)
{
    enum { SEND_RECV_LOOP = 8 };
    pjsip_tpfactory *tpfactory;
    pjsip_transport *tcp;
    pj_sockaddr_in rem_addr;
    pj_status_t status;
    char url[PJSIP_MAX_URL_SIZE];
    int rtt[SEND_RECV_LOOP], min_rtt;
    int i, pkt_lost;

    /* Start TCP listener on arbitrary port. */
    status = pjsip_tcp_transport_start(endpt, NULL, 1, &tpfactory);
    if (status != PJ_SUCCESS) {
	app_perror("   Error: unable to start TCP transport", status);
	return -10;
    }


    /* Get the listener address */
    status = pj_sockaddr_in_init(&rem_addr, &tpfactory->addr_name.host,
				 (pj_uint16_t)tpfactory->addr_name.port);
    if (status != PJ_SUCCESS) {
	app_perror("   Error: possibly invalid TCP address name", status);
	return -14;
    }

    pj_ansi_sprintf(url, "sip:alice@%s:%d;transport=tcp",
		    pj_inet_ntoa(rem_addr.sin_addr),
		    pj_ntohs(rem_addr.sin_port));


    /* Acquire one TCP transport. */
    status = pjsip_endpt_acquire_transport(endpt, PJSIP_TRANSPORT_TCP, 
					   &rem_addr, sizeof(rem_addr),
					   NULL, &tcp);
    if (status != PJ_SUCCESS || tcp == NULL) {
	app_perror("   Error: unable to acquire TCP transport", status);
	return -17;
    }

    /* After pjsip_endpt_acquire_transport, TCP transport must have
     * reference counter 1. 
     */
    if (pj_atomic_get(tcp->ref_cnt) != 1)
	return -20;

    /* Test basic transport attributes */
    status = generic_transport_test(tcp);
    if (status != PJ_SUCCESS)
	return status;


    /* Check again that reference counter is 1. */
    if (pj_atomic_get(tcp->ref_cnt) != 1)
	return -40;

    /* Load test */
    if (transport_load_test(url) != 0)
	return -60;

    /* Basic transport's send/receive loopback test. */
    for (i=0; i<SEND_RECV_LOOP; ++i) {
	status = transport_send_recv_test(PJSIP_TRANSPORT_TCP, tcp, url, &rtt[i]);

	if (status != 0) {
	    pjsip_transport_dec_ref(tcp);
	    flush_events(500);
	    return -72;
	}
    }

    min_rtt = 0xFFFFFFF;
    for (i=0; i<SEND_RECV_LOOP; ++i)
	if (rtt[i] < min_rtt) min_rtt = rtt[i];

    report_ival("tcp-rtt-usec", min_rtt, "usec",
		"Best TCP transport round trip time, in microseconds "
		"(time from sending request until response is received. "
		"Tests were performed on local machine only, and after "
		"TCP socket has been established by previous test)");


    /* Multi-threaded round-trip test. */
    status = transport_rt_test(PJSIP_TRANSPORT_TCP, tcp, url, &pkt_lost);
    if (status != 0) {
	pjsip_transport_dec_ref(tcp);
	return status;
    }

    if (pkt_lost != 0)
	PJ_LOG(3,(THIS_FILE, "   note: %d packet(s) was lost", pkt_lost));

    /* Check again that reference counter is still 1. */
    if (pj_atomic_get(tcp->ref_cnt) != 1)
	return -80;

    /* Destroy this transport. */
    pjsip_transport_dec_ref(tcp);

    /* Force destroy this transport. */
    status = pjsip_transport_destroy(tcp);
    if (status != PJ_SUCCESS)
	return -90;

    /* Unregister factory */
    status = pjsip_tpmgr_unregister_tpfactory(pjsip_endpt_get_tpmgr(endpt), 
					      tpfactory);
    if (status != PJ_SUCCESS)
	return -95;

    /* Flush events. */
    PJ_LOG(3,(THIS_FILE, "   Flushing events, 1 second..."));
    flush_events(1000);

    /* Done */
    return 0;
}
#else	/* PJ_HAS_TCP */
int transport_tcp_test(void)
{
    return 0;
}
#endif	/* PJ_HAS_TCP */
