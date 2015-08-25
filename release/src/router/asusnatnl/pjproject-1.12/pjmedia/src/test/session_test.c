/* $Id: session_test.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia/mediamgr.h>
#include <pjmedia/session.h>
#include <pj/sock.h>
#include <pj/pool.h>
#include <stdio.h>
#include <pj/string.h>

pj_status_t session_test (pj_pool_factory *pf)
{
    pj_med_mgr_t *mm;
    pj_media_session_t *s1, *s2;
    pj_pool_t *pool;
    pjsdp_session_desc *sdp;
    pj_media_stream_info sd_info;
    char buf[1024];
    int len;
    pj_media_stream_stat tx_stat, rx_stat;

    pool = pj_pool_create(pf, "test", 4096, 1024, NULL);

    // Init media manager.
    mm = pj_med_mgr_create ( pf );

    // Create caller session.
    // THIS WILL DEFINITELY CRASH (NULL as argument)!
    s1 = pj_media_session_create (mm, NULL);

    // Set caller's media to send-only.
    sd_info.dir = PJMEDIA_DIR_ENCODING;
    pj_media_session_modify_stream (s1, 0, PJMEDIA_STREAM_MODIFY_DIR, &sd_info);

    // Create caller SDP.
    sdp = pj_media_session_create_sdp (s1, pool, 0);
    len = pjsdp_print (sdp, buf, sizeof(buf));
    buf[len] = '\0';
    printf("Caller's initial SDP:\n<BEGIN>\n%s\n<END>\n", buf);

    // Parse SDP from caller.
    sdp = pjsdp_parse (buf, len, pool);

    // Create callee session based on caller's SDP.
    // THIS WILL DEFINITELY CRASH (NULL as argument)!
    s2 = pj_media_session_create_from_sdp (mm, sdp, NULL);
    
    // Create callee SDP
    sdp = pj_media_session_create_sdp (s2, pool, 0);
    len = pjsdp_print (sdp, buf, sizeof(buf));
    buf[len] = '\0';
    printf("Callee's SDP:\n<BEGIN>\n%s\n<END>\n", buf);

    // Parse SDP from callee.
    sdp = pjsdp_parse (buf, len, pool);

    // Update caller
    pj_media_session_update (s1, sdp);
    sdp = pj_media_session_create_sdp (s1, pool, 0);
    pjsdp_print (sdp, buf, sizeof(buf));
    printf("Caller's SDP after update:\n<BEGIN>\n%s\n<END>\n", buf);

    // Now start media.
    pj_media_session_activate (s2);
    pj_media_session_activate (s1);

    // Wait
    for (;;) {
	int has_stat;

	printf("Enter q to exit, 1 or 2 to print statistics.\n");
	fgets (buf, 10, stdin);
	has_stat = 0;

	switch (buf[0]) {
	case 'q':
	case 'Q':
	    goto done;
	    break;
	case '1':
	    pj_media_session_get_stat (s1, 0, &tx_stat, &rx_stat);
	    has_stat = 1;
	    break;
	case '2':
	    pj_media_session_get_stat (s2, 0, &tx_stat, &rx_stat);
	    has_stat = 1;
	    break;
	}

	if (has_stat) {
	    pj_media_stream_stat *stat[2] = { &tx_stat, &rx_stat };
	    const char *statname[2] = { "TX", "RX" };
	    int i;

	    for (i=0; i<2; ++i) {
		printf("%s statistics:\n", statname[i]);
		printf(" Pkt      TX=%d RX=%d\n", stat[i]->pkt_tx, stat[i]->pkt_rx);
		printf(" Octets   TX=%d RX=%d\n", stat[i]->oct_tx, stat[i]->oct_rx);
		printf(" Jitter   %d ms\n", stat[i]->jitter);
		printf(" Pkt lost %d\n", stat[i]->pkt_lost);
	    }
	    printf("\n");
	}
    }

done:

    // Done.
    pj_pool_release (pool);
    pj_media_session_destroy (s2);
    pj_media_session_destroy (s1);
    pj_med_mgr_destroy (mm);

    return 0;
}
