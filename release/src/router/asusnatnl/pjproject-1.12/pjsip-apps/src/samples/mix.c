/* $Id: mix.c 3553 2011-05-05 06:14:19Z nanang $ */
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
 * \page page_pjmedia_samples_mix_c Samples: Mixing WAV files
 *
 * This file is pjsip-apps/src/samples/mix.c
 *
 * \includelineno mix.c
 */

#include <pjlib.h>
#include <pjlib-util.h>
#include <pjmedia.h>

#define THIS_FILE   "mix.c"

static const char *desc = 
 " mix\n"
 "\n"
 " PURPOSE:\n"
 "  Mix input WAV files and save it to output WAV. Input WAV can have\n"
 "  different clock rate.\n"
 "\n"
 "\n"
 " USAGE:\n"
 "  mix [options] output.wav input1.wav [input2.wav] ...\n"
 "\n"
 " arguments:\n"
 "    output.wav    Set the output WAV filename.\n"
 "    input1.wav    Set the input WAV filename.\n"
 "    input2.wav    Set the input WAV filename.\n"
 "\n"
 " options:\n"
 "    -c N          Set clock rate to N Hz (default 16000)\n"
 "    -f            Force write (overwrite output without warning\n"
;

#define MAX_WAV	    16
#define PTIME	    20
#define APPEND	    1000

struct wav_input
{
    const char	    *fname;
    pjmedia_port    *port;
    unsigned	     slot;
};

static int err_ret(const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(3,(THIS_FILE, "%s error: %s", title, errmsg));
    return 1;
}

static void usage(void)
{
    puts(desc);
}

int main(int argc, char *argv[])
{
    pj_caching_pool cp;
    pj_pool_t *pool;
    pjmedia_endpt *med_ept;
    unsigned clock_rate = 16000;
    int c, force=0;
    const char *out_fname;
    pjmedia_conf *conf;
    pjmedia_port *wavout;
    struct wav_input wav_input[MAX_WAV];
    pj_size_t longest = 0, processed;
    unsigned i, input_cnt = 0;
    pj_status_t status;

#define CHECK(op)   do { \
			status = op; \
			if (status != PJ_SUCCESS) \
			    return err_ret(#op, status); \
		    } while (0)


    /* Parse arguments */
    while ((c=pj_getopt(argc, argv, "c:f")) != -1) {
	switch (c) {
	case 'c':
	    clock_rate = atoi(pj_optarg);
	    if (clock_rate < 1000) {
		puts("Error: invalid clock rate");
		usage();
		return -1;
	    }
	    break;
	case 'f':
	    force = 1;
	    break;
	}
    }

    /* Get output WAV name */
    if (pj_optind == argc) {
	puts("Error: no WAV output is specified");
	usage();
	return 1;
    }

    out_fname = argv[pj_optind++];
    if (force==0 && pj_file_exists(out_fname)) {
	char in[8];

	printf("File %s exists, overwrite? [Y/N] ", out_fname);
	fflush(stdout);
	if (fgets(in, sizeof(in), stdin) == NULL)
	    return 1;
	if (pj_tolower(in[0]) != 'y')
	    return 1;
    }

    /* Scan input file names */
    for (input_cnt=0 ; pj_optind<argc && input_cnt<MAX_WAV; 
	 ++pj_optind, ++input_cnt) 
    {
	if (!pj_file_exists(argv[pj_optind])) {
	    printf("Error: input file %s doesn't exist\n",
		   argv[pj_optind]);
	    return 1;
	}
	wav_input[input_cnt].fname = argv[pj_optind];
	wav_input[input_cnt].port = NULL;
	wav_input[input_cnt].slot = 0;
    }

    if (input_cnt == 0) {
	puts("Error: no input WAV is specified");
	return 0;
    }

    /* Initialialize */
    CHECK( pj_init(0) );
    CHECK( pjlib_util_init() );
    pj_caching_pool_init(0, &cp, NULL, 0);
    //CHECK( pjmedia_endpt_create(&cp.factory, NULL, 1, &med_ept) );
    CHECK( pjmedia_endpt_create(0, &cp.factory, NULL, 1, 0,&med_ept) );

    pool = pj_pool_create(&cp.factory, "mix", 1000, 1000, NULL);

    /* Create the bridge */
    CHECK( pjmedia_conf_create(pool, MAX_WAV+4, clock_rate, 1, 
			       clock_rate * PTIME / 1000, 16, 
			       PJMEDIA_CONF_NO_DEVICE, &conf) );

    /* Create the WAV output */
    CHECK( pjmedia_wav_writer_port_create(pool, out_fname, clock_rate, 1,
					  clock_rate * PTIME / 1000,
					  16, 0, 0, &wavout) );

    /* Create and register each WAV input to the bridge */
    for (i=0; i<input_cnt; ++i) {
	pj_ssize_t len;

	CHECK( pjmedia_wav_player_port_create(pool, wav_input[i].fname, 20,
					      PJMEDIA_FILE_NO_LOOP, 0, 
					      &wav_input[i].port) );
	len = pjmedia_wav_player_get_len(wav_input[i].port);
	len = (pj_ssize_t)(len * 1.0 * clock_rate / 
			    wav_input[i].port->info.clock_rate);
	if (len > (pj_ssize_t)longest)
	    longest = len;

	CHECK( pjmedia_conf_add_port(conf, pool, wav_input[i].port,
				     NULL, &wav_input[i].slot));

	CHECK( pjmedia_conf_connect_port(conf, wav_input[i].slot, 0, 0) );
    }

    /* Loop reading frame from the bridge and write it to WAV */
    processed = 0;
    while (processed < longest + clock_rate * APPEND * 2 / 1000) {
	pj_int16_t framebuf[PTIME * 48000 / 1000];
	pjmedia_port *cp = pjmedia_conf_get_master_port(conf);
	pjmedia_frame frame;

	frame.buf = framebuf;
	frame.size = cp->info.samples_per_frame * 2;
	pj_assert(frame.size <= sizeof(framebuf));
	
	CHECK( pjmedia_port_get_frame(cp, &frame) );

	if (frame.type != PJMEDIA_FRAME_TYPE_AUDIO) {
	    pj_bzero(frame.buf, frame.size);
	    frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
	}

	CHECK( pjmedia_port_put_frame(wavout, &frame));

	processed += frame.size;
    }

    PJ_LOG(3,(THIS_FILE, "Done. Output duration: %d.%03d",
	      (processed >> 2)/clock_rate,
	      ((processed >> 2)*1000/clock_rate) % 1000));

    /* Shutdown everything */
    CHECK( pjmedia_port_destroy(wavout) );
    for (i=0; i<input_cnt; ++i) {
	CHECK( pjmedia_conf_remove_port(conf, wav_input[i].slot) );
	CHECK( pjmedia_port_destroy(wav_input[i].port) );
    }

    CHECK(pjmedia_conf_destroy(conf));
    CHECK(pjmedia_endpt_destroy(med_ept));

    pj_pool_release(pool);
    pj_caching_pool_destroy(&cp);
    pj_shutdown(0);

    return 0;
}

