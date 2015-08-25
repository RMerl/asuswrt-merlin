/* $Id: confsample.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjlib-util.h>	/* pj_getopt */
#include <pjlib.h>

#include <stdlib.h>	/* atoi() */
#include <stdio.h>

#include "util.h"

/**
 * \page page_pjmedia_samples_confsample_c Samples: Using Conference Bridge
 *
 * Sample to mix multiple files in the conference bridge and play the
 * result to sound device.
 *
 * This file is pjsip-apps/src/samples/confsample.c
 *
 * \includelineno confsample.c
 */


/* For logging purpose. */
#define THIS_FILE   "confsample.c"


/* Shall we put recorder in the conference */
#define RECORDER    1


static const char *desc = 
 " FILE:								    \n"
 "									    \n"
 "  confsample.c							    \n"
 "									    \n"
 " PURPOSE:								    \n"
 "									    \n"
 "  Demonstrate how to use conference bridge.				    \n"
 "									    \n"
 " USAGE:								    \n"
 "									    \n"
 "  confsample [options] [file1.wav] [file2.wav] ...			    \n"
 "									    \n"
 " options:								    \n"
 SND_USAGE
 "									    \n"
 "  fileN.wav are optional WAV files to be connected to the conference      \n"
 "  bridge. The WAV files MUST have single channel (mono) and 16 bit PCM    \n"
 "  samples. It can have arbitrary sampling rate.			    \n"
 "									    \n"
 " DESCRIPTION:								    \n"
 "									    \n"
 "  Here we create a conference bridge, with at least one port (port zero   \n"
 "  is always created for the sound device).				    \n"
 "									    \n"
 "  If WAV files are specified, the WAV file player ports will be connected \n"
 "  to slot starting from number one in the bridge. The WAV files can have  \n"
 "  arbitrary sampling rate; the bridge will convert it to its clock rate.  \n"
 "  However, the files MUST have a single audio channel only (i.e. mono).  \n";


 
/* 
 * Prototypes: 
 */

/* List the ports in the conference bridge */
static void conf_list(pjmedia_conf *conf, pj_bool_t detail);

/* Display VU meter */
static void monitor_level(pjmedia_conf *conf, int slot, int dir, int dur);


/* Show usage */
static void usage(void)
{
    puts("");
    puts(desc);
}



/* Input simple string */
static pj_bool_t input(const char *title, char *buf, pj_size_t len)
{
    char *p;

    printf("%s (empty to cancel): ", title); fflush(stdout);
    if (fgets(buf, len, stdin) == NULL)
	return PJ_FALSE;

    /* Remove trailing newlines. */
    for (p=buf; ; ++p) {
	if (*p=='\r' || *p=='\n') *p='\0';
	else if (!*p) break;
    }

    if (!*buf)
	return PJ_FALSE;
    
    return PJ_TRUE;
}


/*****************************************************************************
 * main()
 */
int main(int argc, char *argv[])
{
    int dev_id = -1;
    int clock_rate = CLOCK_RATE;
    int channel_count = NCHANNELS;
    int samples_per_frame = NSAMPLES;
    int bits_per_sample = NBITS;

    pj_caching_pool cp;
    pjmedia_endpt *med_endpt;
    pj_pool_t *pool;
    pjmedia_conf *conf;

    int i, port_count, file_count;
    pjmedia_port **file_port;	/* Array of file ports */
    pjmedia_port *rec_port = NULL;  /* Wav writer port */

    char tmp[10];
    pj_status_t status;


    /* Must init PJLIB first: */
    status = pj_init(0);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Get command line options. */
    if (get_snd_options(THIS_FILE, argc, argv, &dev_id, &clock_rate,
			&channel_count, &samples_per_frame, &bits_per_sample))
    {
	usage();
	return 1;
    }

    /* Must create a pool factory before we can allocate any memory. */
    pj_caching_pool_init(0, &cp, &pj_pool_factory_default_policy, 0);

    /* 
     * Initialize media endpoint.
     * This will implicitly initialize PJMEDIA too.
     */
//    status = pjmedia_endpt_create(&cp.factory, NULL, 1, &med_endpt);
    status = pjmedia_endpt_create(0, &cp.factory, NULL, 1, 0, &med_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Create memory pool to allocate memory */
    pool = pj_pool_create( &cp.factory,	    /* pool factory	    */
			   "wav",	    /* pool name.	    */
			   4000,	    /* init size	    */
			   4000,	    /* increment size	    */
			   NULL		    /* callback on error    */
			   );


    file_count = argc - pj_optind;
    port_count = file_count + 1 + RECORDER;

    /* Create the conference bridge. 
     * With default options (zero), the bridge will create an instance of
     * sound capture and playback device and connect them to slot zero.
     */
    status = pjmedia_conf_create( pool,	    /* pool to use	    */
				  port_count,/* number of ports	    */
				  clock_rate,
				  channel_count,
				  samples_per_frame,
				  bits_per_sample,
				  0,	    /* options		    */
				  &conf	    /* result		    */
				  );
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to create conference bridge", status);
	return 1;
    }

#if RECORDER
    status = pjmedia_wav_writer_port_create(  pool, "confrecord.wav",
					      clock_rate, channel_count,
					      samples_per_frame, 
					      bits_per_sample, 0, 0, 
					      &rec_port);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to create WAV writer", status);
	return 1;
    }

    pjmedia_conf_add_port(conf, pool, rec_port, NULL, NULL);
#endif


    /* Create file ports. */
    file_port = pj_pool_alloc(pool, file_count * sizeof(pjmedia_port*));

    for (i=0; i<file_count; ++i) {

	/* Load the WAV file to file port. */
	status = pjmedia_wav_player_port_create( 
			pool,		    /* pool.	    */
			argv[i+pj_optind],  /* filename	    */
			0,		    /* use default ptime */
			0,		    /* flags	    */
			0,		    /* buf size	    */
			&file_port[i]	    /* result	    */
			);
	if (status != PJ_SUCCESS) {
	    char title[80];
	    pj_ansi_sprintf(title, "Unable to use %s", argv[i+pj_optind]);
	    app_perror(THIS_FILE, title, status);
	    usage();
	    return 1;
	}

	/* Add the file port to conference bridge */
	status = pjmedia_conf_add_port( conf,		/* The bridge	    */
					pool,		/* pool		    */
					file_port[i],	/* port to connect  */
					NULL,		/* Use port's name  */
					NULL		/* ptr for slot #   */
					);
	if (status != PJ_SUCCESS) {
	    app_perror(THIS_FILE, "Unable to add conference port", status);
	    return 1;
	}
    }


    /* 
     * All ports are set up in the conference bridge.
     * But at this point, no media will be flowing since no ports are
     * "connected". User must connect the port manually.
     */


    /* Dump memory usage */
    dump_pool_usage(THIS_FILE, &cp);

    /* Sleep to allow log messages to flush */
    pj_thread_sleep(100);


    /*
     * UI Menu: 
     */
    for (;;) {
	char tmp1[10];
	char tmp2[10];
	char *err;
	int src, dst, level, dur;

	puts("");
	conf_list(conf, 0);
	puts("");
	puts("Menu:");
	puts("  s    Show ports details");
	puts("  c    Connect one port to another");
	puts("  d    Disconnect port connection");
	puts("  t    Adjust signal level transmitted (tx) to a port");
	puts("  r    Adjust signal level received (rx) from a port");
	puts("  v    Display VU meter for a particular port");
	puts("  q    Quit");
	puts("");
	
	printf("Enter selection: "); fflush(stdout);

	if (fgets(tmp, sizeof(tmp), stdin) == NULL)
	    break;

	switch (tmp[0]) {
	case 's':
	    puts("");
	    conf_list(conf, 1);
	    break;

	case 'c':
	    puts("");
	    puts("Connect source port to destination port");
	    if (!input("Enter source port number", tmp1, sizeof(tmp1)) )
		continue;
	    src = strtol(tmp1, &err, 10);
	    if (*err || src < 0 || src >= port_count) {
		puts("Invalid slot number");
		continue;
	    }

	    if (!input("Enter destination port number", tmp2, sizeof(tmp2)) )
		continue;
	    dst = strtol(tmp2, &err, 10);
	    if (*err || dst < 0 || dst >= port_count) {
		puts("Invalid slot number");
		continue;
	    }

	    status = pjmedia_conf_connect_port(conf, src, dst, 0);
	    if (status != PJ_SUCCESS)
		app_perror(THIS_FILE, "Error connecting port", status);
	    
	    break;

	case 'd':
	    puts("");
	    puts("Disconnect port connection");
	    if (!input("Enter source port number", tmp1, sizeof(tmp1)) )
		continue;
	    src = strtol(tmp1, &err, 10);
	    if (*err || src < 0 || src >= port_count) {
		puts("Invalid slot number");
		continue;
	    }

	    if (!input("Enter destination port number", tmp2, sizeof(tmp2)) )
		continue;
	    dst = strtol(tmp2, &err, 10);
	    if (*err || dst < 0 || dst >= port_count) {
		puts("Invalid slot number");
		continue;
	    }

	    status = pjmedia_conf_disconnect_port(conf, src, dst);
	    if (status != PJ_SUCCESS)
		app_perror(THIS_FILE, "Error connecting port", status);
	    

	    break;

	case 't':
	    puts("");
	    puts("Adjust transmit level of a port");
	    if (!input("Enter port number", tmp1, sizeof(tmp1)) )
		continue;
	    src = strtol(tmp1, &err, 10);
	    if (*err || src < 0 || src >= port_count) {
		puts("Invalid slot number");
		continue;
	    }

	    if (!input("Enter level (-128 to >127, 0 for normal)", 
			      tmp2, sizeof(tmp2)) )
		continue;
	    level = strtol(tmp2, &err, 10);
	    if (*err || level < -128) {
		puts("Invalid level");
		continue;
	    }

	    status = pjmedia_conf_adjust_tx_level( conf, src, level);
	    if (status != PJ_SUCCESS)
		app_perror(THIS_FILE, "Error adjusting level", status);
	    break;


	case 'r':
	    puts("");
	    puts("Adjust receive level of a port");
	    if (!input("Enter port number", tmp1, sizeof(tmp1)) )
		continue;
	    src = strtol(tmp1, &err, 10);
	    if (*err || src < 0 || src >= port_count) {
		puts("Invalid slot number");
		continue;
	    }

	    if (!input("Enter level (-128 to >127, 0 for normal)", 
			      tmp2, sizeof(tmp2)) )
		continue;
	    level = strtol(tmp2, &err, 10);
	    if (*err || level < -128) {
		puts("Invalid level");
		continue;
	    }

	    status = pjmedia_conf_adjust_rx_level( conf, src, level);
	    if (status != PJ_SUCCESS)
		app_perror(THIS_FILE, "Error adjusting level", status);
	    break;

	case 'v':
	    puts("");
	    puts("Display VU meter");
	    if (!input("Enter port number to monitor", tmp1, sizeof(tmp1)) )
		continue;
	    src = strtol(tmp1, &err, 10);
	    if (*err || src < 0 || src >= port_count) {
		puts("Invalid slot number");
		continue;
	    }

	    if (!input("Enter r for rx level or t for tx level", tmp2, sizeof(tmp2)))
		continue;
	    if (tmp2[0] != 'r' && tmp2[0] != 't') {
		puts("Invalid option");
		continue;
	    }

	    if (!input("Duration to monitor (in seconds)", tmp1, sizeof(tmp1)) )
		continue;
	    dur = strtol(tmp1, &err, 10);
	    if (*err) {
		puts("Invalid duration number");
		continue;
	    }

	    monitor_level(conf, src, tmp2[0], dur);
	    break;

	case 'q':
	    goto on_quit;

	default:
	    printf("Invalid input character '%c'\n", tmp[0]);
	    break;
	}
    }

on_quit:
    
    /* Start deinitialization: */

    /* Destroy conference bridge */
    status = pjmedia_conf_destroy( conf );
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);


    /* Destroy file ports */
    for (i=0; i<file_count; ++i) {
	status = pjmedia_port_destroy( file_port[i]);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    }

    /* Destroy recorder port */
    if (rec_port)
	pjmedia_port_destroy(rec_port);

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


/*
 * List the ports in conference bridge
 */
static void conf_list(pjmedia_conf *conf, int detail)
{
    enum { MAX_PORTS = 32 };
    unsigned i, count;
    pjmedia_conf_port_info info[MAX_PORTS];

    printf("Conference ports:\n");

    count = PJ_ARRAY_SIZE(info);
    pjmedia_conf_get_ports_info(conf, &count, info);

    for (i=0; i<count; ++i) {
	char txlist[4*MAX_PORTS];
	unsigned j;
	pjmedia_conf_port_info *port_info = &info[i];	
	
	txlist[0] = '\0';
	for (j=0; j<port_info->listener_cnt; ++j) {
	    char s[10];
	    pj_ansi_sprintf(s, "#%d ", port_info->listener_slots[j]);
	    pj_ansi_strcat(txlist, s);

	}

	if (txlist[0] == '\0') {
	    txlist[0] = '-';
	    txlist[1] = '\0';
	}

	if (!detail) {
	    printf("Port #%02d %-25.*s  transmitting to: %s\n", 
		   port_info->slot, 
		   (int)port_info->name.slen, 
		   port_info->name.ptr,
		   txlist);
	} else {
	    unsigned tx_level, rx_level;

	    pjmedia_conf_get_signal_level(conf, port_info->slot,
					  &tx_level, &rx_level);

	    printf("Port #%02d:\n"
		   "  Name                    : %.*s\n"
		   "  Sampling rate           : %d Hz\n"
		   "  Samples per frame       : %d\n"
		   "  Frame time              : %d ms\n"
		   "  Signal level adjustment : tx=%d, rx=%d\n"
		   "  Current signal level    : tx=%u, rx=%u\n"
		   "  Transmitting to ports   : %s\n\n",
		   port_info->slot,
		   (int)port_info->name.slen,
		   port_info->name.ptr,
		   port_info->clock_rate,
		   port_info->samples_per_frame,
		   port_info->samples_per_frame*1000/port_info->clock_rate,
		   port_info->tx_adj_level,
		   port_info->rx_adj_level,
		   tx_level,
		   rx_level,
		   txlist);
	}

    }
    puts("");
}


/*
 * Display VU meter
 */
static void monitor_level(pjmedia_conf *conf, int slot, int dir, int dur)
{
    enum { SLEEP = 20, SAMP_CNT = 2};
    pj_status_t status;
    int i, total_count;
    unsigned level, samp_cnt;


    puts("");
    printf("Displaying VU meter for port %d for about %d seconds\n",
	   slot, dur);

    total_count = dur * 1000 / SLEEP;

    level = 0;
    samp_cnt = 0;

    for (i=0; i<total_count; ++i) {
	unsigned tx_level, rx_level;
	int j, length;
	char meter[21];

	/* Poll the volume every 20 msec */
	status = pjmedia_conf_get_signal_level(conf, slot, 
					       &tx_level, &rx_level);
	if (status != PJ_SUCCESS) {
	    app_perror(THIS_FILE, "Unable to read level", status);
	    return;
	}

	level += (dir=='r' ? rx_level : tx_level);
	++samp_cnt;

	/* Accumulate until we have enough samples */
	if (samp_cnt < SAMP_CNT) {
	    pj_thread_sleep(SLEEP);
	    continue;
	}

	/* Get average */
	level = level / samp_cnt;

	/* Draw bar */
	length = 20 * level / 255;
	for (j=0; j<length; ++j)
	    meter[j] = '#';
	for (; j<20; ++j)
	    meter[j] = ' ';
	meter[20] = '\0';

	printf("Port #%02d %cx level: [%s] %d  \r",
	       slot, dir, meter, level);

	/* Next.. */
	samp_cnt = 0;
	level = 0;

	pj_thread_sleep(SLEEP);
    }

    puts("");
}

