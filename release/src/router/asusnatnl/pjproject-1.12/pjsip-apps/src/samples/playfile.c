/* $Id: playfile.c 3553 2011-05-05 06:14:19Z nanang $ */
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

#include <pjmedia.h>
#include <pjlib-util.h>
#include <pjlib.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.h"


/**
 * \page page_pjmedia_samples_playfile_c Samples: Playing WAV File to Sound Device
 *
 * This is a very simple example to use the @ref PJMEDIA_FILE_PLAY and
 * @ref PJMED_SND_PORT. In this example, we open both the file and sound
 * device, and connect the two of them, and voila! Sound will be playing
 * the contents of the file.
 *
 * @see page_pjmedia_samples_recfile_c
 *
 * This file is pjsip-apps/src/samples/playfile.c
 *
 * \includelineno playfile.c
 */


/*
 * playfile.c
 *
 * PURPOSE:
 *  Play a WAV file to sound player device.
 *
 * USAGE:
 *  playfile FILE.WAV
 *
 *  The WAV file could have mono or stereo channels with arbitrary
 *  sampling rate, but MUST contain uncompressed (i.e. 16bit) PCM.
 *
 */


/* For logging purpose. */
#define THIS_FILE   "playfile.c"


static const char *desc = 
" FILE		    						    \n"
"		    						    \n"
"  playfile.c	    						    \n"
"		    						    \n"
" PURPOSE	    						    \n"
"		    						    \n"
"  Demonstrate how to play a WAV file.				    \n"
"		    						    \n"
" USAGE		    						    \n"
"		    						    \n"
"  playfile FILE.WAV						    \n"
"		    						    \n"
"  The WAV file could have mono or stereo channels with arbitrary   \n"
"  sampling rate, but MUST contain uncompressed (i.e. 16bit) PCM.   \n";


/*
 * main()
 */
int main(int argc, char *argv[])
{
    pj_caching_pool cp;
    pjmedia_endpt *med_endpt;
    pj_pool_t *pool;
    pjmedia_port *file_port;
    pjmedia_snd_port *snd_port;
    char tmp[10];
    pj_status_t status;


    if (argc != 2) {
    	puts("Error: filename required");
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
	// charles modified
    //status = pjmedia_endpt_create(&cp.factory, NULL, 1, &med_endpt);
    status = pjmedia_endpt_create(0, &cp.factory, NULL, 1, 0, &med_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Create memory pool for our file player */
    pool = pj_pool_create( &cp.factory,	    /* pool factory	    */
			   "wav",	    /* pool name.	    */
			   4000,	    /* init size	    */
			   4000,	    /* increment size	    */
			   NULL		    /* callback on error    */
			   );

    /* Create file media port from the WAV file */
    status = pjmedia_wav_player_port_create(  pool,	/* memory pool	    */
					      argv[1],	/* file to play	    */
					      20,	/* ptime.	    */
					      0,	/* flags	    */
					      0,	/* default buffer   */
					      &file_port/* returned port    */
					      );
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to use WAV file", status);
	return 1;
    }

    /* Create sound player port. */
    status = pjmedia_snd_port_create_player( 
		 pool,				    /* pool		    */
		 -1,				    /* use default dev.	    */
		 file_port->info.clock_rate,	    /* clock rate.	    */
		 file_port->info.channel_count,	    /* # of channels.	    */
		 file_port->info.samples_per_frame, /* samples per frame.   */
		 file_port->info.bits_per_sample,   /* bits per sample.	    */
		 0,				    /* options		    */
		 &snd_port			    /* returned port	    */
		 );
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to open sound device", status);
	return 1;
    }

    /* Connect file port to the sound player.
     * Stream playing will commence immediately.
     */
    status = pjmedia_snd_port_connect( snd_port, file_port);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);



    /* 
     * File should be playing and looping now, using sound device's thread. 
     */


    /* Sleep to allow log messages to flush */
    pj_thread_sleep(100);


    printf("Playing %s..\n", argv[1]);
    puts("");
    puts("Press <ENTER> to stop playing and quit");

    if (fgets(tmp, sizeof(tmp), stdin) == NULL) {
	puts("EOF while reading stdin, will quit now..");
    }

    
    /* Start deinitialization: */

    /* Disconnect sound port from file port */
    status = pjmedia_snd_port_disconnect(snd_port);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Without this sleep, Windows/DirectSound will repeteadly
     * play the last frame during destroy.
     */
    pj_thread_sleep(100);

    /* Destroy sound device */
    status = pjmedia_snd_port_destroy( snd_port );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* Destroy file port */
    status = pjmedia_port_destroy( file_port );
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

