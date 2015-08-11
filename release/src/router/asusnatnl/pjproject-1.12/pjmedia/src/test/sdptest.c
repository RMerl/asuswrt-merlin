/* $Id: sdptest.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia/sdp.h>
#include <pj/os.h>
#include <pj/pool.h>
#include <stdio.h>
#include <string.h>

static char *sdp[] = {
    /*
    "v=0\r\n"
    "o=mhandley 2890844526 2890842807 IN IP4 126.16.64.4\r\n"
    "s=SDP Seminar\r\n"
    "i=A Seminar on the session description protocol\r\n"
    "u=http://www.cs.ucl.ac.uk/staff/M.Handley/sdp.03.ps\r\n"
    "e=mjh@isi.edu (Mark Handley)\r\n"
    "c=IN IP4 224.2.17.12/127\r\n"
    "t=2873397496 2873404696\r\n"
    "a=recvonly\r\n"
    "m=audio 49170 RTP/AVP 0\r\n"
    "m=video 51372 RTP/AVP 31\r\n"
    "m=application 32416 udp wb\r\n"
    "a=orient:portrait\r\n"
    "m=audio 49230 RTP/AVP 96 97 98\r\n"
    "a=rtpmap:96 L8/8000\r\n"
    "a=rtpmap:97 L16/8000\r\n"
    "a=rtpmap:98 L16/11025/2\r\n",
    */
    "v=0\r\n"
    "o=usera 2890844526 2890844527 IN IP4 alice.example.com\r\n"
    "s=\r\n"
    "c=IN IP4 alice.example.com\r\n"
    "t=0 0\r\n"
    "m=message 7394 msrp/tcp *\r\n"
    "a=accept-types: message/cpim text/plain text/html\r\n"
    "a=path:msrp://alice.example.com:7394/2s93i9;tcp\r\n"
};

static int sdp_perform_test(pj_pool_factory *pf)
{
    pj_pool_t *pool;
    int inputlen, len;
    pjsdp_session_desc *ses;
    char buf[1500];
    enum { LOOP=1000000 };
    int i;
    pj_time_val start, end;

    printf("Parsing and printing %d SDP messages..\n", LOOP);

    pool = pj_pool_create(pf, "", 4096, 0, NULL);
    inputlen = strlen(sdp[0]);
    pj_gettimeofday(&start);
    for (i=0; i<LOOP; ++i) {
	ses = pjsdp_parse(sdp[0], inputlen, pool);
	len = pjsdp_print(ses, buf, sizeof(buf));
	buf[len] = '\0';
	pj_pool_reset(pool);
    }
    pj_gettimeofday(&end);

    printf("Original:\n%s\n", sdp[0]);
    printf("Parsed:\n%s\n", buf);

    PJ_TIME_VAL_SUB(end, start);
    printf("Time: %ld:%03lds\n", end.sec, end.msec);

    if (end.msec==0 && end.sec==0) end.msec=1;
    printf("Performance: %ld msg/sec\n", LOOP*1000/PJ_TIME_VAL_MSEC(end));
    puts("");

    pj_pool_release(pool);
    return 0;
}

static int sdp_conform_test(pj_pool_factory *pf)
{
    pj_pool_t *pool;
    pjsdp_session_desc *ses;
    int i, len;
    char buf[1500];

    for (i=0; i<sizeof(sdp)/sizeof(sdp[0]); ++i) {
	pool = pj_pool_create(pf, "sdp", 4096, 0, NULL);
	ses = pjsdp_parse(sdp[i], strlen(sdp[i]), pool);
	len = pjsdp_print(ses, buf, sizeof(buf)); 
	buf[len] = '\0';
	printf("%s\n", buf);
	pj_pool_release(pool);
    }

    return 0;
}

pj_status_t sdp_test(pj_pool_factory *pf)
{
    if (sdp_conform_test(pf) != 0)
	return -2;

    if (sdp_perform_test(pf) != 0)
	return -3;

    return 0;
}

