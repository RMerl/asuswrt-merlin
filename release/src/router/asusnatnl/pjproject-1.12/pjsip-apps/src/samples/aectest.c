/* $Id: aectest.c 3553 2011-05-05 06:14:19Z nanang $ */
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
 * \page page_pjmedia_samples_aectest_c Samples: AEC Test (aectest.c)
 *
 * Play a file to speaker, run AEC, and record the microphone input
 * to see if echo is coming.
 *
 * This file is pjsip-apps/src/samples/aectest.c
 *
 * \includelineno aectest.c
 */
#include <pjmedia.h>
#include <pjlib-util.h>	/* pj_getopt */
#include <pjlib.h>

#define THIS_FILE   "aectest.c"
#define PTIME	    20
#define TAIL_LENGTH 200

static const char *desc = 
" FILE		    						    \n"
"		    						    \n"
"  aectest.c	    						    \n"
"		    						    \n"
" PURPOSE	    						    \n"
"		    						    \n"
"  Test the AEC effectiveness.					    \n"
"		    						    \n"
" USAGE		    						    \n"
"		    						    \n"
"  aectest [options] <PLAY.WAV> <REC.WAV> <OUTPUT.WAV>		    \n"
"		    						    \n"
"  <PLAY.WAV>   is the signal played to the speaker.		    \n"
"  <REC.WAV>    is the signal captured from the microphone.	    \n"
"  <OUTPUT.WAV> is the output file to store the test result	    \n"
"\n"
" options:\n"
"  -d  The delay between playback and capture in ms, at least 25 ms.\n"
"      Default is 25 ms. See note below.                            \n"
"  -l  Set the echo tail length in ms. Default is 200 ms	    \n"
"  -r  Set repeat count (default=1)                                 \n"
"  -a  Algorithm: 0=default, 1=speex, 3=echo suppress		    \n"
"  -i  Interactive						    \n"
"\n"
" Note that for the AEC internal buffering mechanism, it is required\n"
" that the echoed signal (in REC.WAV) is delayed from the           \n"
" corresponding reference signal (in PLAY.WAV) at least as much as  \n"
" frame time + PJMEDIA_WSOLA_DELAY_MSEC. In this application, frame \n"
" time is 20 ms and default PJMEDIA_WSOLA_DELAY_MSEC is 5 ms, hence \n"
" 25 ms delay is the minimum value.                                 \n";

/* 
 * Sample session:
 *
 * -d 100 -a 1 ../bin/orig8.wav ../bin/echo8.wav ../bin/result8.wav 
 */

static void app_perror(const char *sender, const char *title, pj_status_t st)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(st, errmsg, sizeof(errmsg));
    PJ_LOG(3,(sender, "%s: %s", title, errmsg));
}


/*
 * main()
 */
int main(int argc, char *argv[])
{
    pj_caching_pool cp;
    pjmedia_endpt *med_endpt;
    pj_pool_t	  *pool;
    pjmedia_port  *wav_play;
    pjmedia_port  *wav_rec;
    pjmedia_port  *wav_out;
    pj_status_t status;
    pjmedia_echo_state *ec;
    pjmedia_frame play_frame, rec_frame;
    unsigned opt = 0;
    unsigned latency_ms = 25;
    unsigned tail_ms = TAIL_LENGTH;
    pj_timestamp t0, t1;
    int i, repeat=1, interactive=0, c;

    pj_optind = 0;
    while ((c=pj_getopt(argc, argv, "d:l:a:r:i")) !=-1) {
	switch (c) {
	case 'd':
	    latency_ms = atoi(pj_optarg);
	    if (latency_ms < 25) {
		puts("Invalid delay");
		puts(desc);
	    }
	    break;
	case 'l':
	    tail_ms = atoi(pj_optarg);
	    break;
	case 'a':
	    {
		int alg = atoi(pj_optarg);
		switch (alg) {
		case 0:
		    opt = 0;
		case 1:
		    opt = PJMEDIA_ECHO_SPEEX;
		    break;
		case 3:
		    opt = PJMEDIA_ECHO_SIMPLE;
		    break;
		default:
		    puts("Invalid algorithm");
		    puts(desc);
		    return 1;
		}
	    }
	    break;
	case 'r':
	    repeat = atoi(pj_optarg);
	    if (repeat < 1) {
		puts("Invalid repeat count");
		puts(desc);
		return 1;
	    }
	    break;
	case 'i':
	    interactive = 1;
	    break;
	}
    }

    if (argc - pj_optind != 3) {
	puts("Error: missing argument(s)");
	puts(desc);
	return 1;
    }

    /* Must init PJLIB first: */
    status = pj_init(0);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Must create a pool factory before we can allocate any memory. */
    pj_caching_pool_init(0, &cp, &pj_pool_factory_default_policy, 0);

    /* 
     * Initialize media endpoint.
     * This will implicitly initialize PJMEDIA too.
     */
    //status = pjmedia_endpt_create(&cp.factory, NULL, 1, &med_endpt);
	// charles modified
    status = pjmedia_endpt_create(0, &cp.factory, NULL, 1, 0, &med_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Create memory pool for our file player */
    pool = pj_pool_create( &cp.factory,	    /* pool factory	    */
			   "wav",	    /* pool name.	    */
			   4000,	    /* init size	    */
			   4000,	    /* increment size	    */
			   NULL		    /* callback on error    */
			   );

    /* Open wav_play */
    status = pjmedia_wav_player_port_create(pool, argv[pj_optind], PTIME, 
					    PJMEDIA_FILE_NO_LOOP, 0, 
					    &wav_play);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Error opening playback WAV file", status);
	return 1;
    }
    
    /* Open recorded wav */
    status = pjmedia_wav_player_port_create(pool, argv[pj_optind+1], PTIME, 
					    PJMEDIA_FILE_NO_LOOP, 0, 
					    &wav_rec);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Error opening recorded WAV file", status);
	return 1;
    }

    /* play and rec WAVs must have the same clock rate */
    if (wav_play->info.clock_rate != wav_rec->info.clock_rate) {
	puts("Error: clock rate mismatch in the WAV files");
	return 1;
    }

    /* .. and channel count */
    if (wav_play->info.channel_count != wav_rec->info.channel_count) {
	puts("Error: clock rate mismatch in the WAV files");
	return 1;
    }

    /* Create output wav */
    status = pjmedia_wav_writer_port_create(pool, argv[pj_optind+2],
					    wav_play->info.clock_rate,
					    wav_play->info.channel_count,
					    wav_play->info.samples_per_frame,
					    wav_play->info.bits_per_sample,
					    0, 0, &wav_out);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Error opening output WAV file", status);
	return 1;
    }

    /* Create echo canceller */
    status = pjmedia_echo_create2(pool, wav_play->info.clock_rate,
				  wav_play->info.channel_count,
				  wav_play->info.samples_per_frame,
				  tail_ms, latency_ms,
				  opt, &ec);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Error creating EC", status);
	return 1;
    }


    /* Processing loop */
    play_frame.buf = pj_pool_alloc(pool, wav_play->info.samples_per_frame<<1);
    rec_frame.buf = pj_pool_alloc(pool, wav_play->info.samples_per_frame<<1);
    pj_get_timestamp(&t0);
    for (i=0; i < repeat; ++i) {
	for (;;) {
	    play_frame.size = wav_play->info.samples_per_frame << 1;
	    status = pjmedia_port_get_frame(wav_play, &play_frame);
	    if (status != PJ_SUCCESS)
		break;

	    status = pjmedia_echo_playback(ec, (short*)play_frame.buf);

	    rec_frame.size = wav_play->info.samples_per_frame << 1;
	    status = pjmedia_port_get_frame(wav_rec, &rec_frame);
	    if (status != PJ_SUCCESS)
		break;

	    status = pjmedia_echo_capture(ec, (short*)rec_frame.buf, 0);

	    //status = pjmedia_echo_cancel(ec, (short*)rec_frame.buf, 
	    //			     (short*)play_frame.buf, 0, NULL);

	    pjmedia_port_put_frame(wav_out, &rec_frame);
	}

	pjmedia_wav_player_port_set_pos(wav_play, 0);
	pjmedia_wav_player_port_set_pos(wav_rec, 0);
    }
    pj_get_timestamp(&t1);

    i = pjmedia_wav_writer_port_get_pos(wav_out) / sizeof(pj_int16_t) * 1000 / 
	(wav_out->info.clock_rate * wav_out->info.channel_count);
    PJ_LOG(3,(THIS_FILE, "Processed %3d.%03ds audio",
	      i / 1000, i % 1000));
    PJ_LOG(3,(THIS_FILE, "Completed in %u msec\n", pj_elapsed_msec(&t0, &t1)));

    /* Destroy file port(s) */
    status = pjmedia_port_destroy( wav_play );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    status = pjmedia_port_destroy( wav_rec );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    status = pjmedia_port_destroy( wav_out );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Destroy ec */
    pjmedia_echo_destroy(ec);

    /* Release application pool */
    pj_pool_release( pool );

    /* Destroy media endpoint. */
    pjmedia_endpt_destroy( med_endpt );

    /* Destroy pool factory */
    pj_caching_pool_destroy( &cp );

    /* Shutdown PJLIB */
    pj_shutdown(0);

    if (interactive) {
	char s[10], *dummy;
	puts("ENTER to quit");
	dummy = fgets(s, sizeof(s), stdin);
    }

    /* Done. */
    return 0;
}

