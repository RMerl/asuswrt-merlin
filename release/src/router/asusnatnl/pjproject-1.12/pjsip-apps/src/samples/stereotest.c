/* $Id: stereotest.c 3553 2011-05-05 06:14:19Z nanang $ */
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
 * \page page_pjmedia_samples_stereo_c Samples: Using Stereo Port
 *
 * This example demonstrates how to use @ref PJMEDIA_STEREO_PORT to
 * change the channel count of the media streams.
 *
 * This file is pjsip-apps/src/samples/stereotest.c
 *
 * \includelineno stereotest.c
 */

#include <pjmedia.h>
#include <pjlib-util.h>
#include <pjlib.h>

#include <stdio.h>
#include <stdlib.h>

#include "util.h"

#define REC_CLOCK_RATE	    16000
#define PTIME		    20

#define MODE_PLAY	    1
#define MODE_RECORD	    2


/* For logging purpose. */
#define THIS_FILE   "stereotest.c"


static const char *desc = 
" FILE		    						    \n"
"		    						    \n"
"  stereotest.c	    						    \n"
"		    						    \n"
" PURPOSE	    						    \n"
"		    						    \n"
"  Demonstrate how use stereo port to play a WAV file to sound	    \n"
"  device or record to a WAV file from sound device with different  \n"
"  channel count.    						    \n"
"		    						    \n"
" USAGE		    						    \n"
"		    						    \n"
"  stereotest [options]	WAV					    \n"
"		    						    \n"
"  Options:							    \n"
"  -m, --mode=N          Operation mode: 1 = playing, 2 = recording.\n"
"  -C, --rec-ch-cnt=N    Number of channel for recording file.	    \n"
"  -c, --snd-ch-cnt=N    Number of channel for opening sound device.\n"
"		    						    \n";

int main(int argc, char *argv[])
{
    pj_caching_pool cp;
    pjmedia_endpt *med_endpt;
    pj_pool_t *pool;

    pjmedia_port *file_port = NULL;
    pjmedia_port *stereo_port = NULL;
    pjmedia_snd_port *snd_port = NULL;

    int dev_id = -1;
    char tmp[10];
    pj_status_t status;

    char *wav_file = NULL;
    unsigned mode = 0;
    unsigned rec_ch_cnt = 1;
    unsigned snd_ch_cnt = 2;

    enum {
	OPT_MODE	= 'm',
	OPT_REC_CHANNEL = 'C',
	OPT_SND_CHANNEL = 'c',
    };

    struct pj_getopt_option long_options[] = {
	{ "mode",	    1, 0, OPT_MODE },
	{ "rec-ch-cnt",	    1, 0, OPT_REC_CHANNEL },
	{ "snd-ch-cnt",	    1, 0, OPT_SND_CHANNEL },
	{ NULL, 0, 0, 0 },
    };

    int c;
    int option_index;

    /* Must init PJLIB first: */
    status = pj_init(0);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Parse arguments */
    pj_optind = 0;
    while((c=pj_getopt_long(argc,argv, "m:C:c:", long_options, &option_index))!=-1) {

	switch (c) {
	case OPT_MODE:
	    if (mode) {
		app_perror(THIS_FILE, "Cannot record and play at once!", 
			   PJ_EINVAL);
		return 1;
	    }
	    mode = atoi(pj_optarg);
	    break;

	case OPT_REC_CHANNEL:
	    rec_ch_cnt = atoi(pj_optarg);
	    break;

	case OPT_SND_CHANNEL:
	    snd_ch_cnt = atoi(pj_optarg);
	    break;

	default:
	    printf("Invalid options %s\n", argv[pj_optind]);
	    puts(desc);
	    return 1;
	}

    }

    wav_file = argv[pj_optind];

    /* Verify arguments. */
    if (!wav_file) {
	app_perror(THIS_FILE, "WAV file not specified!", PJ_EINVAL);
	puts(desc);
	return 1;
    }
    if (!snd_ch_cnt || !rec_ch_cnt || rec_ch_cnt > 6) {
	app_perror(THIS_FILE, "Invalid or too many channel count!", PJ_EINVAL);
	puts(desc);
	return 1;
    }
    if (mode != MODE_RECORD && mode != MODE_PLAY) {
	app_perror(THIS_FILE, "Invalid operation mode!", PJ_EINVAL);
	puts(desc);
	return 1;
    }

    /* Must create a pool factory before we can allocate any memory. */
    pj_caching_pool_init(0, &cp, &pj_pool_factory_default_policy, 0);

    /* 
     * Initialize media endpoint.
     * This will implicitly initialize PJMEDIA too.
     */
    //status = pjmedia_endpt_create(&cp.factory, NULL, 1, &med_endpt);
    status = pjmedia_endpt_create(0, &cp.factory, NULL, 1, 0, &med_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Create memory pool for our file player */
    pool = pj_pool_create( &cp.factory,	    /* pool factory	    */
			   "app",	    /* pool name.	    */
			   4000,	    /* init size	    */
			   4000,	    /* increment size	    */
			   NULL		    /* callback on error    */
			   );

    if (mode == MODE_PLAY) {
	/* Create WAVE file player port. */
	status = pjmedia_wav_player_port_create( pool, wav_file, PTIME, 0,
						 0, &file_port);
	if (status != PJ_SUCCESS) {
	    app_perror(THIS_FILE, "Unable to open file", status);
	    return 1;
	}

	/* Create sound player port. */
	status = pjmedia_snd_port_create_player( 
		     pool,				/* pool		      */
		     dev_id,				/* device id.	      */
		     file_port->info.clock_rate,	/* clock rate.	      */
		     snd_ch_cnt,			/* # of channels.     */
		     snd_ch_cnt * PTIME *		/* samples per frame. */
		     file_port->info.clock_rate / 1000,
		     file_port->info.bits_per_sample,   /* bits per sample.   */
		     0,					/* options	      */
		     &snd_port				/* returned port      */
		     );
	if (status != PJ_SUCCESS) {
	    app_perror(THIS_FILE, "Unable to open sound device", status);
	    return 1;
	}

	if (snd_ch_cnt != file_port->info.channel_count) {
	    status = pjmedia_stereo_port_create( pool,
						 file_port,
						 snd_ch_cnt,
						 0,
						 &stereo_port);
	    if (status != PJ_SUCCESS) {
		app_perror(THIS_FILE, "Unable to create stereo port", status);
		return 1;
	    }

	    status = pjmedia_snd_port_connect(snd_port, stereo_port);
	} else {
	    status = pjmedia_snd_port_connect(snd_port, file_port);
	}

	if (status != PJ_SUCCESS) {
	    app_perror(THIS_FILE, "Unable to connect sound port", status);
	    return 1;
	}

    } else {
	/* Create WAVE file writer port. */
	status = pjmedia_wav_writer_port_create(pool, wav_file,
						REC_CLOCK_RATE,
						rec_ch_cnt,
						rec_ch_cnt * PTIME * 
						REC_CLOCK_RATE / 1000,
						NBITS,
						0, 0, 
						&file_port);
	if (status != PJ_SUCCESS) {
	    app_perror(THIS_FILE, "Unable to open file", status);
	    return 1;
	}

	/* Create sound player port. */
	status = pjmedia_snd_port_create_rec( 
			 pool,			    /* pool		    */
			 dev_id,		    /* device id.	    */
			 REC_CLOCK_RATE,	    /* clock rate.	    */
			 snd_ch_cnt,		    /* # of channels.	    */
			 snd_ch_cnt * PTIME * 
			 REC_CLOCK_RATE / 1000,	    /* samples per frame.   */
			 NBITS,			    /* bits per sample.	    */
			 0,			    /* options		    */
			 &snd_port		    /* returned port	    */
		     );
	if (status != PJ_SUCCESS) {
	    app_perror(THIS_FILE, "Unable to open sound device", status);
	    return 1;
	}

	if (rec_ch_cnt != snd_ch_cnt) {
	    status = pjmedia_stereo_port_create( pool,
						 file_port,
						 snd_ch_cnt,
						 0,
						 &stereo_port);
	    if (status != PJ_SUCCESS) {
		app_perror(THIS_FILE, "Unable to create stereo port", status);
		return 1;
	    }

	    status = pjmedia_snd_port_connect(snd_port, stereo_port);
	} else {
	    status = pjmedia_snd_port_connect(snd_port, file_port);
	}

	if (status != PJ_SUCCESS) {
	    app_perror(THIS_FILE, "Unable to connect sound port", status);
	    return 1;
	}
    }

    /* Dump memory usage */
    dump_pool_usage(THIS_FILE, &cp);

    /* 
     * File should be playing and looping now, using sound device's thread. 
     */


    /* Sleep to allow log messages to flush */
    pj_thread_sleep(100);

    printf("Mode = %s\n", (mode == MODE_PLAY? "playing" : "recording") );
    printf("File  port channel count = %d\n", file_port->info.channel_count);
    printf("Sound port channel count = %d\n", 
	   pjmedia_snd_port_get_port(snd_port)->info.channel_count);
    puts("");
    puts("Press <ENTER> to stop and quit");

    if (fgets(tmp, sizeof(tmp), stdin) == NULL) {
	puts("EOF while reading stdin, will quit now..");
    }
    
    /* Start deinitialization: */


    /* Destroy sound device */
    status = pjmedia_snd_port_destroy( snd_port );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* Destroy stereo port and file_port. 
     * Stereo port will destroy all downstream ports (e.g. the file port)
     */
    status = pjmedia_port_destroy( stereo_port? stereo_port : file_port);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* Release application pool */
    pj_pool_release( pool );

    /* Destroy media endpoint. */
    pjmedia_endpt_destroy( med_endpt );

    /* Destroy pool factory */
    pj_caching_pool_destroy( &cp );

    /* Shutdown PJLIB */
    pj_shutdown(0);


    /* Done. */
    return 0;

}



