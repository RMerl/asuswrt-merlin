/* $Id: resampleplay.c 3553 2011-05-05 06:14:19Z nanang $ */
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
 * \page page_pjmedia_samples_resampleplay_c Samples: Using Resample Port
 *
 * This example demonstrates how to use @ref PJMEDIA_RESAMPLE_PORT to
 * change the sampling rate of the media streams.
 *
 * This file is pjsip-apps/src/samples/resampleplay.c
 *
 * \includelineno resampleplay.c
 */

#include <pjmedia.h>
#include <pjlib-util.h>
#include <pjlib.h>

#include <stdio.h>
#include <stdlib.h>

#include "util.h"

/* For logging purpose. */
#define THIS_FILE   "resampleplay.c"


static const char *desc = 
" FILE		    						    \n"
"		    						    \n"
"  resampleplay.c	    					    \n"
"		    						    \n"
" PURPOSE	    						    \n"
"		    						    \n"
"  Demonstrate how use resample port to play a WAV file to sound    \n"
"  device using different sampling rate.			    \n"
"		    						    \n"
" USAGE		    						    \n"
"		    						    \n"
"  resampleplay [options] FILE.WAV				    \n"
"		    						    \n"
" where options:						    \n"
SND_USAGE
"		    						    \n"
"  The WAV file could have mono or stereo channels with arbitrary   \n"
"  sampling rate, but MUST contain uncompressed (i.e. 16bit) PCM.   \n";


int main(int argc, char *argv[])
{
    pj_caching_pool cp;
    pjmedia_endpt *med_endpt;
    pj_pool_t *pool;
    pjmedia_port *file_port;
    pjmedia_port *resample_port;
    pjmedia_snd_port *snd_port;
    char tmp[10];
    pj_status_t status;

    int dev_id = -1;
    int sampling_rate = CLOCK_RATE;
    int channel_count = NCHANNELS;
    int samples_per_frame = NSAMPLES;
    int bits_per_sample = NBITS;
    //int ptime;
    //int down_samples;

    /* Must init PJLIB first: */
    status = pj_init(0);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* Get options */
    if (get_snd_options(THIS_FILE, argc, argv, &dev_id, &sampling_rate,
			&channel_count, &samples_per_frame, &bits_per_sample))
    {
	puts("");
	puts(desc);
	return 1;
    }

    if (!argv[pj_optind]) {
	puts("Error: no file is specified");
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
    status = pjmedia_endpt_create(0, &cp.factory, NULL, 1, 0,&med_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Create memory pool for our file player */
    pool = pj_pool_create( &cp.factory,	    /* pool factory	    */
			   "app",	    /* pool name.	    */
			   4000,	    /* init size	    */
			   4000,	    /* increment size	    */
			   NULL		    /* callback on error    */
			   );

    /* Create the file port. */
    status = pjmedia_wav_player_port_create( pool, argv[pj_optind], 0, 0,
					     0, &file_port);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to open file", status);
	return 1;
    }

    /* File must have same number of channels. */
    if (file_port->info.channel_count != (unsigned)channel_count) {
	PJ_LOG(3,(THIS_FILE, "Error: file has different number of channels. "
			     "Perhaps you'd need -c option?"));
	pjmedia_port_destroy(file_port);
	return 1;
    }

    /* Calculate number of samples per frame to be taken from file port */
    //ptime = samples_per_frame * 1000 / sampling_rate;

    /* Create the resample port. */
    status = pjmedia_resample_port_create( pool, file_port,
					   sampling_rate, 0,
					   &resample_port);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to create resample port", status);
	return 1;
    }

    /* Create sound player port. */
    status = pjmedia_snd_port_create( 
		 pool,			/* pool			    */
		 dev_id,		/* device		    */
		 dev_id,		/* device		    */
		 sampling_rate,		/* clock rate.		    */
		 channel_count,		/* # of channels.	    */
		 samples_per_frame,	/* samples per frame.	    */
		 bits_per_sample,	/* bits per sample.	    */
		 0,			/* options		    */
		 &snd_port		/* returned port	    */
		 );
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to open sound device", status);
	return 1;
    }

    /* Connect resample port to sound device */
    status = pjmedia_snd_port_connect( snd_port, resample_port);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Error connecting sound ports", status);
	return 1;
    }


    /* Dump memory usage */
    dump_pool_usage(THIS_FILE, &cp);

    /* 
     * File should be playing and looping now, using sound device's thread. 
     */


    /* Sleep to allow log messages to flush */
    pj_thread_sleep(100);


    printf("Playing %s at sampling rate %d (original file sampling rate=%d)\n",
	   argv[pj_optind], sampling_rate, file_port->info.clock_rate);
    puts("");
    puts("Press <ENTER> to stop playing and quit");

    if (fgets(tmp, sizeof(tmp), stdin) == NULL) {
	puts("EOF while reading stdin, will quit now..");
    }
    
    /* Start deinitialization: */


    /* Destroy sound device */
    status = pjmedia_snd_port_destroy( snd_port );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* Destroy resample port. 
     * This will destroy all downstream ports (e.g. the file port)
     */
    status = pjmedia_port_destroy( resample_port );
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



