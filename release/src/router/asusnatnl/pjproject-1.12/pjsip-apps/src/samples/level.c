/* $Id: level.c 3553 2011-05-05 06:14:19Z nanang $ */
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
 * \page page_pjmedia_samples_level_c Samples: Reading from WAV File
 *
 * This is a very simple example to use the @ref PJMEDIA_FILE_PLAY, to
 * directly read the samples from the file.
 *
 * This file is pjsip-apps/src/samples/level.c
 *
 * \includelineno level.c
 */


static const char *desc = 
 " FILE:								    \n"
 "  level.c								    \n"
 "									    \n"
 " PURPOSE:								    \n"
 "  Read PCM WAV file and display the audio level the first 100 frames.     \n"
 "  Each frame is assumed to have 160 samples.				    \n"
 "									    \n"
 " USAGE:								    \n"
 "  level file.wav							    \n"
 "									    \n"
 "  The WAV file SHOULD have a 16bit mono samples.			    ";

#include <pjmedia.h>
#include <pjlib.h>

#include <stdio.h>

/* For logging purpose. */
#define THIS_FILE   "level.c"


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
 * main()
 */
int main(int argc, char *argv[])
{
    enum { NSAMPLES = 640, COUNT=100 };
    pj_caching_pool cp;
    pjmedia_endpt *med_endpt;
    pj_pool_t *pool;
    pjmedia_port *file_port;
    int i;
    pj_status_t status;


    /* Verify cmd line arguments. */
    if (argc != 2) {
	puts("");
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

    /* Create file media port from the WAV file */
    status = pjmedia_wav_player_port_create(  pool,	/* memory pool	    */
					      argv[1],	/* file to play	    */
					      0,	/* use default ptime*/
					      0,	/* flags	    */
					      0,	/* default buffer   */
					      &file_port/* returned port    */
					      );
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to use WAV file", status);
	return 1;
    }

    if (file_port->info.samples_per_frame > NSAMPLES) {
	app_perror(THIS_FILE, "WAV clock rate is too big", PJ_EINVAL);
	return 1;
    }

    puts("Time\tPCMU\tLinear");
    puts("------------------------");

    for (i=0; i<COUNT; ++i) {
	pj_int16_t framebuf[NSAMPLES];
	pjmedia_frame frm;
	pj_int32_t level32;
	unsigned ms;
	int level;

	frm.buf = framebuf;
	frm.size = sizeof(framebuf);
	
	pjmedia_port_get_frame(file_port, &frm);

	level32 = pjmedia_calc_avg_signal(framebuf, 
					  file_port->info.samples_per_frame);
	level = pjmedia_linear2ulaw(level32) ^ 0xFF;

	ms = i * 1000 * file_port->info.samples_per_frame /
			file_port->info.clock_rate;
	printf("%03d.%03d\t%7d\t%7d\n", 
	        ms/1000, ms%1000, level, level32);
    }
    puts("");
    

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

