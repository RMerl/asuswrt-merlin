/* $Id: latency.c 3553 2011-05-05 06:14:19Z nanang $ */
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

/* See http://trac.pjsip.org/repos/wiki/MeasuringSoundLatency on
 * how to use this program.
 */

#include <pjmedia.h>
#include <pjlib.h>

#include <stdio.h>

#define THIS_FILE   "lacency.c"


/* Util to display the error message for the specified error code  */
static int app_perror( const char *sender, const char *title, 
		       pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    PJ_UNUSED_ARG(sender);

    pj_strerror(status, errmsg, sizeof(errmsg));

    printf("%s: %s [code=%d]\n", title, errmsg, status);
    return 1;
}

/*
 * Find out latency
 */
static int calculate_latency(pj_pool_t *pool, pjmedia_port *wav)
{
    pjmedia_frame frm;
    short *buf;
    unsigned i, samples_per_frame, read, len;
    unsigned start_pos;
    pj_status_t status;

    unsigned lat_sum = 0,
	     lat_cnt = 0,
	     lat_min = 10000,
	     lat_max = 0;

    samples_per_frame = wav->info.samples_per_frame;
    frm.buf = pj_pool_alloc(pool, samples_per_frame * 2);
    frm.size = samples_per_frame * 2;
    len = pjmedia_wav_player_get_len(wav);
    buf = pj_pool_alloc(pool, len + samples_per_frame);

    read = 0;
    while (read < len/2) {
	status = pjmedia_port_get_frame(wav, &frm);
	if (status != PJ_SUCCESS)
	    break;

	pjmedia_copy_samples(buf+read, (short*)frm.buf, samples_per_frame);
	read += samples_per_frame;
    }

    if (read < 2 * wav->info.clock_rate) {
	puts("Error: too short");
	return -1;
    }

    start_pos = 0;
    while (start_pos < len/2 - wav->info.clock_rate) {
	int max_signal = 0;
	unsigned max_signal_pos = start_pos;
	unsigned max_echo_pos = 0;
	unsigned pos;
	unsigned lat;

	/* Get the largest signal in the next 0.7s */
	for (i=start_pos; i<start_pos + wav->info.clock_rate * 700 / 1000; ++i) {
	    if (abs(buf[i]) > max_signal) {
		max_signal = abs(buf[i]);
		max_signal_pos = i;
	    }
	}

	/* Advance 10ms from max_signal_pos */
	pos = max_signal_pos + 10 * wav->info.clock_rate / 1000;

	/* Get the largest signal in the next 500ms */
	max_signal = 0;
	max_echo_pos = pos;
	for (i=pos; i<pos+wav->info.clock_rate/2; ++i) {
	    if (abs(buf[i]) > max_signal) {
		max_signal = abs(buf[i]);
		max_echo_pos = i;
	    }
	}

	lat = (max_echo_pos - max_signal_pos) * 1000 / wav->info.clock_rate;
	
#if 0
	printf("Latency = %u\n", lat);
#endif

	lat_sum += lat;
	lat_cnt++;
	if (lat < lat_min)
	    lat_min = lat;
	if (lat > lat_max)
	    lat_max = lat;

	/* Advance next loop */
	start_pos += wav->info.clock_rate;
    }

    printf("Latency average = %u\n", lat_sum / lat_cnt);
    printf("Latency minimum = %u\n", lat_min);
    printf("Latency maximum = %u\n", lat_max);
    printf("Number of data  = %u\n", lat_cnt);
    return 0;
}


/*
 * main()
 */
int main(int argc, char *argv[])
{
    enum { NSAMPLES = 160, COUNT=100 };
    pj_caching_pool cp;
    pj_pool_t *pool;
    pjmedia_port *wav;
    pj_status_t status;


    /* Verify cmd line arguments. */
    if (argc != 2) {
	puts("Error: missing argument(s)");
	puts("Usage: latency REV.WAV");
	return 1;
    }

    pj_log_set_level(0, 0);

    status = pj_init(0);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    pj_caching_pool_init(0, &cp, &pj_pool_factory_default_policy, 0);

    pool = pj_pool_create( &cp.factory,	    /* pool factory	    */
			   "wav",	    /* pool name.	    */
			   4000,	    /* init size	    */
			   4000,	    /* increment size	    */
			   NULL		    /* callback on error    */
			   );

    status = pj_register_strerror(PJMEDIA_ERRNO_START, PJ_ERRNO_SPACE_SIZE, 
				  &pjmedia_strerror);
    pj_assert(status == PJ_SUCCESS);

    /* Wav */
    status = pjmedia_wav_player_port_create(  pool,	/* memory pool	    */
					      argv[1],	/* file to play	    */
					      0,	/* use default ptime*/
					      0,	/* flags	    */
					      0,	/* default buffer   */
					      &wav	/* returned port    */
					      );
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, argv[1], status);
	return 1;
    }

    status = calculate_latency(pool, wav);
    if (status != PJ_SUCCESS)
	return 1;

    status = pjmedia_port_destroy( wav );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    pj_pool_release( pool );
    pj_caching_pool_destroy( &cp );
    pj_shutdown(0);

    /* Done. */
    return 0;
}

