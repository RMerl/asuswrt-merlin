/* $Id: util.h 3550 2011-05-05 05:33:27Z nanang $ */
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
#include <stdlib.h>	/* strtol() */

/* Util to display the error message for the specified error code  */
static int app_perror( const char *sender, const char *title, 
		       pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));

    PJ_LOG(3,(sender, "%s: %s [code=%d]", title, errmsg, status));
    return 1;
}



/* Constants */
#define CLOCK_RATE	44100
#define NSAMPLES	(CLOCK_RATE * 20 / 1000)
#define NCHANNELS	1
#define NBITS		16

/*
 * Common sound options.
 */
#define SND_USAGE   \
"  -d, --dev=NUM        Sound device use device id NUM (default=-1)	 \n"\
"  -r, --rate=HZ        Set clock rate in samples per sec (default=44100)\n"\
"  -c, --channel=NUM    Set # of channels (default=1 for mono).		 \n"\
"  -f, --frame=NUM      Set # of samples per frame (default equival 20ms)\n"\
"  -b, --bit=NUM        Set # of bits per sample (default=16)		 \n"


/*
 * This utility function parses the command line and look for
 * common sound options.
 */
pj_status_t get_snd_options(const char *app_name,
			    int argc, 
			    char *argv[],
			    int *dev_id,
			    int *clock_rate,
			    int *channel_count,
			    int *samples_per_frame,
			    int *bits_per_sample)
{
    struct pj_getopt_option long_options[] = {
	{ "dev",	1, 0, 'd' },
	{ "rate",	1, 0, 'r' },
	{ "channel",	1, 0, 'c' },
	{ "frame",	1, 0, 'f' },
	{ "bit",	1, 0, 'b' },
	{ NULL, 0, 0, 0 },
    };
    int c;
    int option_index;
    long val;
    char *err;

    *samples_per_frame = 0;

    pj_optind = 0;
    while((c=pj_getopt_long(argc,argv, "d:r:c:f:b:", 
			    long_options, &option_index))!=-1) 
    {

	switch (c) {
	case 'd':
	    /* device */
	    val = strtol(pj_optarg, &err, 10);
	    if (*err) {
		PJ_LOG(3,(app_name, "Error: invalid value for device id"));
		return PJ_EINVAL;
	    }
	    *dev_id = val;
	    break;

	case 'r':
	    /* rate */
	    val = strtol(pj_optarg, &err, 10);
	    if (*err) {
		PJ_LOG(3,(app_name, "Error: invalid value for clock rate"));
		return PJ_EINVAL;
	    }
	    *clock_rate = val;
	    break;

	case 'c':
	    /* channel count */
	    val = strtol(pj_optarg, &err, 10);
	    if (*err) {
		PJ_LOG(3,(app_name, "Error: invalid channel count"));
		return PJ_EINVAL;
	    }
	    *channel_count = val;
	    break;

	case 'f':
	    /* frame count/samples per frame */
	    val = strtol(pj_optarg, &err, 10);
	    if (*err) {
		PJ_LOG(3,(app_name, "Error: invalid samples per frame"));
		return PJ_EINVAL;
	    }
	    *samples_per_frame = val;
	    break;

	case 'b':
	    /* bit per sample */
	    val = strtol(pj_optarg, &err, 10);
	    if (*err) {
		PJ_LOG(3,(app_name, "Error: invalid samples bits per sample"));
		return PJ_EINVAL;
	    }
	    *bits_per_sample = val;
	    break;

	default:
	    /* Unknown options */
	    PJ_LOG(3,(app_name, "Error: unknown options '%c'", pj_optopt));
	    return PJ_EINVAL;
	}

    }

    if (*samples_per_frame == 0) {
	*samples_per_frame = *clock_rate * *channel_count * 20 / 1000;
    }

    return 0;
}


/* Dump memory pool usage. */
void dump_pool_usage( const char *app_name, pj_caching_pool *cp )
{
#if !defined(PJ_HAS_POOL_ALT_API) || PJ_HAS_POOL_ALT_API==0
    pj_pool_t   *p;
    unsigned     total_alloc = 0;
    unsigned     total_used = 0;

    /* Accumulate memory usage in active list. */
    p = cp->used_list.next;
    while (p != (pj_pool_t*) &cp->used_list) {
	total_alloc += pj_pool_get_capacity(p);
	total_used += pj_pool_get_used_size(p);
	p = p->next;
    }

    PJ_LOG(3, (app_name, "Total pool memory allocated=%d KB, used=%d KB",
	       total_alloc / 1000,
	       total_used / 1000));
#endif
}
