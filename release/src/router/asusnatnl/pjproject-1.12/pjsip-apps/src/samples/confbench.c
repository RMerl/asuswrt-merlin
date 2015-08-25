/* $Id: confbench.c 3553 2011-05-05 06:14:19Z nanang $ */
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
 * \page page_pjmedia_samples_confbench_c Samples: Benchmarking Conference Bridge
 *
 * Benchmarking pjmedia (conference bridge+resample). For my use only,
 * and it only works in Win32.
 *
 * This file is pjsip-apps/src/samples/confbench.c
 *
 * \includelineno confbench.c
 */


#include <pjmedia.h>
#include <pjlib-util.h>	/* pj_getopt */
#include <pjlib.h>
#include <stdlib.h>	/* atoi() */
#include <stdio.h>
#include <windows.h>

/* For logging purpose. */
#define THIS_FILE   "confsample.c"


/* Configurable:
 *   LARGE_SET will create in total of about 232 ports.
 *   HAS_RESAMPLE will activate resampling on about half
 *     the port.
 */
#define TEST_SET	    LARGE_SET
#define HAS_RESAMPLE	    0


#define SMALL_SET	    16
#define LARGE_SET	    100


#define PORT_COUNT	    254
#define CLOCK_RATE	    16000
#define SAMPLES_PER_FRAME   (CLOCK_RATE/100)
#if HAS_RESAMPLE
#  define SINE_CLOCK	    32000
#else
#  define SINE_CLOCK	    CLOCK_RATE
#endif
#define SINE_PTIME	    20
#define DURATION	    10

#define SINE_COUNT	    TEST_SET
#define NULL_COUNT	    TEST_SET
#define IDLE_COUNT	    32


static void app_perror(const char *sender, const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(1,(sender, "%s: %s", title, errmsg));
}


struct Times
{
    FILETIME	    kernel_time;
    ULARGE_INTEGER  u_kernel_time;
    FILETIME	    user_time;
    ULARGE_INTEGER  u_user_time;
    ULARGE_INTEGER  u_total;
};

static void process(struct Times *t)
{
    pj_memcpy(&t->u_kernel_time, &t->kernel_time, sizeof(FILETIME));
    pj_memcpy(&t->u_user_time, &t->user_time, sizeof(FILETIME));
    t->u_total.QuadPart = t->u_kernel_time.QuadPart + t->u_user_time.QuadPart;
}

static void benchmark(void)
{
    FILETIME creation_time, exit_time;
    struct Times start, end;
    DWORD ts, te;
    LARGE_INTEGER elapsed;
    BOOL rc;
    int i;
    double pct;

    puts("Test started!"); fflush(stdout);

    ts = GetTickCount();
    rc = GetProcessTimes(GetCurrentProcess(), &creation_time, &exit_time,
			 &start.kernel_time, &start.user_time);
    for (i=DURATION; i>0; --i) {
	printf("\r%d ", i); fflush(stdout);
	pj_thread_sleep(1000);
    }
    rc = GetProcessTimes(GetCurrentProcess(), &creation_time, &exit_time,
			 &end.kernel_time, &end.user_time);
    te = GetTickCount();

    process(&start);
    process(&end);

    elapsed.QuadPart = end.u_total.QuadPart - start.u_total.QuadPart;

    pct = elapsed.QuadPart * 100.0 / ((te-ts)*10000.0);

    printf("CPU usage=%6.4f%%\n", pct); fflush(stdout);
}



/* Struct attached to sine generator */
typedef struct
{
    pj_int16_t	*samples;	/* Sine samples.    */
} port_data;


/* This callback is called to feed more samples */
static pj_status_t sine_get_frame( pjmedia_port *port, 
				   pjmedia_frame *frame)
{
    port_data *sine = port->port_data.pdata;
    pj_int16_t *samples = frame->buf;
    unsigned i, count, left, right;

    /* Get number of samples */
    count = frame->size / 2 / port->info.channel_count;

    left = 0;
    right = 0;

    for (i=0; i<count; ++i) {
	*samples++ = sine->samples[left];
	++left;

	if (port->info.channel_count == 2) {
	    *samples++ = sine->samples[right];
	    right += 2; /* higher pitch so we can distinguish left and right. */
	    if (right >= count)
		right = 0;
	}
    }

    /* Must set frame->type correctly, otherwise the sound device
     * will refuse to play.
     */
    frame->type = PJMEDIA_FRAME_TYPE_AUDIO;

    return PJ_SUCCESS;
}

#ifndef M_PI
#define M_PI  (3.14159265)
#endif

/*
 * Create a media port to generate sine wave samples.
 */
static pj_status_t create_sine_port(pj_pool_t *pool,
				    unsigned sampling_rate,
				    unsigned channel_count,
				    pjmedia_port **p_port)
{
    pjmedia_port *port;
    unsigned i;
    unsigned count;
    port_data *sine;

    PJ_ASSERT_RETURN(pool && channel_count > 0 && channel_count <= 2, 
		     PJ_EINVAL);

    port = pj_pool_zalloc(pool, sizeof(pjmedia_port));
    PJ_ASSERT_RETURN(port != NULL, PJ_ENOMEM);

    /* Fill in port info. */
    port->info.bits_per_sample = 16;
    port->info.channel_count = channel_count;
    port->info.encoding_name = pj_str("pcm");
    port->info.has_info = 1;
    port->info.name = pj_str("sine generator");
    port->info.need_info = 0;
    port->info.pt = 0xFF;
    port->info.clock_rate = sampling_rate;
    port->info.samples_per_frame = sampling_rate * SINE_PTIME / 1000 * channel_count;
    port->info.bytes_per_frame = port->info.samples_per_frame * 2;
    port->info.type = PJMEDIA_TYPE_AUDIO;
    
    /* Set the function to feed frame */
    port->get_frame = &sine_get_frame;

    /* Create sine port data */
    port->port_data.pdata = sine = pj_pool_zalloc(pool, sizeof(port_data));

    /* Create samples */
    count = port->info.samples_per_frame / channel_count;
    sine->samples = pj_pool_alloc(pool, count * sizeof(pj_int16_t));
    PJ_ASSERT_RETURN(sine->samples != NULL, PJ_ENOMEM);

    /* initialise sinusoidal wavetable */
    for( i=0; i<count; i++ )
    {
        sine->samples[i] = (pj_int16_t) (10000.0 * 
		sin(((double)i/(double)count) * M_PI * 8.) );
    }

    *p_port = port;

    return PJ_SUCCESS;
}

int main()
{
    pj_caching_pool cp;
    pjmedia_endpt *med_endpt;
    pj_pool_t *pool;
    pjmedia_conf *conf;
    int i;
    pjmedia_port *sine_port[SINE_COUNT], *null_port, *conf_port;
    pjmedia_port *nulls[NULL_COUNT];
    unsigned null_slots[NULL_COUNT];
    pjmedia_master_port *master_port;
    pj_status_t status;


    pj_log_set_level(0, 3);

    status = pj_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);
    pool = pj_pool_create( &cp.factory,	    /* pool factory	    */
			   "wav",	    /* pool name.	    */
			   4000,	    /* init size	    */
			   4000,	    /* increment size	    */
			   NULL		    /* callback on error    */
			   );

    status = pjmedia_endpt_create(&cp.factory, NULL, 1, &med_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);



    status = pjmedia_conf_create( pool,
				  PORT_COUNT,
				  CLOCK_RATE,
				  1, SAMPLES_PER_FRAME, 16,
				  PJMEDIA_CONF_NO_DEVICE,
				  &conf);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Unable to create conference bridge", status);
	return 1;
    }

    printf("Resampling is %s\n", (HAS_RESAMPLE?"active":"disabled"));

    /* Create Null ports */
    printf("Creating %d null ports..\n", NULL_COUNT);
    for (i=0; i<NULL_COUNT; ++i) {
	status = pjmedia_null_port_create(pool, CLOCK_RATE, 1, SAMPLES_PER_FRAME*2, 16, &nulls[i]);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

	status = pjmedia_conf_add_port(conf, pool, nulls[i], NULL, &null_slots[i]);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    }

    /* Create sine ports. */
    printf("Creating %d sine generator ports..\n", SINE_COUNT);
    for (i=0; i<SINE_COUNT; ++i) {
	unsigned j, slot;

	/* Load the WAV file to file port. */
	status = create_sine_port(pool, SINE_CLOCK, 1, &sine_port[i]);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

	/* Add the file port to conference bridge */
	status = pjmedia_conf_add_port( conf,		/* The bridge	    */
					pool,		/* pool		    */
					sine_port[i],	/* port to connect  */
					NULL,		/* Use port's name  */
					&slot		/* ptr for slot #   */
					);
	if (status != PJ_SUCCESS) {
	    app_perror(THIS_FILE, "Unable to add conference port", status);
	    return 1;
	}

	status = pjmedia_conf_connect_port(conf, slot, 0, 0);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

	for (j=0; j<NULL_COUNT; ++j) {
	    status = pjmedia_conf_connect_port(conf, slot, null_slots[j], 0);
	    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
	}
    }

    /* Create idle ports */
    printf("Creating %d idle ports..\n", IDLE_COUNT);
    for (i=0; i<IDLE_COUNT; ++i) {
	pjmedia_port *dummy;
	status = pjmedia_null_port_create(pool, CLOCK_RATE, 1, SAMPLES_PER_FRAME, 16, &dummy);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
	status = pjmedia_conf_add_port(conf, pool, dummy, NULL, NULL);
	PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    }

    /* Create null port */
    status = pjmedia_null_port_create(pool, CLOCK_RATE, 1, SAMPLES_PER_FRAME, 16,
				      &null_port);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    conf_port = pjmedia_conf_get_master_port(conf);

    /* Create master port */
    status = pjmedia_master_port_create(pool, null_port, conf_port, 0, &master_port);


    pjmedia_master_port_start(master_port);

    puts("Waiting to settle.."); fflush(stdout);
    pj_thread_sleep(5000);


    benchmark();


    /* Done. */
    return 0;
}


