/* $Id: encdec.c 3816 2011-10-14 04:15:15Z bennylp $ */
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
 * \page page_pjmedia_samples_encdec_c Samples: Encoding and Decoding
 *
 * This sample shows how to use codec.
 *
 * This file is pjsip-apps/src/samples/encdec.c
 *
 * \includelineno encdec.c
 */

#include <pjlib.h>
#include <pjmedia.h>
#include <pjmedia-codec.h>

#define THIS_FILE   "encdec.c"

static const char *desc = 
 " encdec								\n"
 "									\n"
 " PURPOSE:								\n"
 "  Encode input WAV with a codec, and decode the result to another WAV \n"
 "\n"
 "\n"
 " USAGE:								\n"
 "  encdec codec input.wav output.wav                                   \n"
 "\n"
 "\n"
 " where:\n"
 "  codec         Set the codec name.                                   \n"
 "  input.wav     Set the input WAV filename.                           \n"
 "  output.wav    Set the output WAV filename.                          \n"

 "\n"
;

//#undef PJ_TRACE
//#define PJ_TRACE 1

#ifndef PJ_TRACE
#	define PJ_TRACE 0
#endif

#if PJ_TRACE
#   define TRACE_(expr)	    PJ_LOG(4,expr)
#else
#   define TRACE_(expr)
#endif


static void err(const char *op, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(3,("", "%s error: %s", op, errmsg));
}

#define CHECK(op)   do { \
			status = op; \
			if (status != PJ_SUCCESS) { \
			    err(#op, status); \
			    return status; \
			} \
		    } \
		    while (0)

static pjmedia_endpt *mept;
static unsigned file_msec_duration;

static pj_status_t enc_dec_test(const char *codec_id,
				const char *filein,
			        const char *fileout)
{
    pj_pool_t *pool;
    pjmedia_codec_mgr *cm;
    pjmedia_codec *codec;
    const pjmedia_codec_info *pci;
    pjmedia_codec_param param;
    unsigned cnt, samples_per_frame;
    pj_str_t tmp;
    pjmedia_port *wavin, *wavout;
    unsigned lost_pct;
    pj_status_t status;

#define T   file_msec_duration/1000, file_msec_duration%1000
    
    pool = pjmedia_endpt_create_pool(mept, "encdec", 1000, 1000);

    cm = pjmedia_endpt_get_codec_mgr(mept);

#ifdef LOST_PCT
    lost_pct = LOST_PCT;
#else
    lost_pct = 0;
#endif
    
    cnt = 1;
    CHECK( pjmedia_codec_mgr_find_codecs_by_id(cm, pj_cstr(&tmp, codec_id), 
					       &cnt, &pci, NULL) );
    CHECK( pjmedia_codec_mgr_get_default_param(cm, pci, &param) );

    samples_per_frame = param.info.clock_rate * param.info.frm_ptime / 1000;

    /* Control VAD */
    param.setting.vad = 1;

    /* Open wav for reading */
    CHECK( pjmedia_wav_player_port_create(pool, filein, 
					  param.info.frm_ptime, 
					  PJMEDIA_FILE_NO_LOOP, 0, &wavin) );

    /* Open wav for writing */
    CHECK( pjmedia_wav_writer_port_create(pool, fileout,
					  param.info.clock_rate, 
					  param.info.channel_cnt,
					  samples_per_frame,
					  16, 0, 0, &wavout) );

    /* Alloc codec */
    CHECK( pjmedia_codec_mgr_alloc_codec(cm, pci, &codec) );
    CHECK( codec->op->init(codec, pool) );
    CHECK( codec->op->open(codec, &param) );
    
    for (;;) {
	pjmedia_frame frm_pcm, frm_bit, out_frm, frames[4];
	pj_int16_t pcmbuf[320];
	pj_timestamp ts;
	pj_uint8_t bitstream[160];

	frm_pcm.buf = (char*)pcmbuf;
	frm_pcm.size = samples_per_frame * 2;

	/* Read from WAV */
	if (pjmedia_port_get_frame(wavin, &frm_pcm) != PJ_SUCCESS)
	    break;
	if (frm_pcm.type != PJMEDIA_FRAME_TYPE_AUDIO)
	    break;;

	/* Update duration */
	file_msec_duration += samples_per_frame * 1000 / 
			      param.info.clock_rate;

	/* Encode */
	frm_bit.buf = bitstream;
	frm_bit.size = sizeof(bitstream);
	CHECK(codec->op->encode(codec, &frm_pcm, sizeof(bitstream), &frm_bit));

	/* On DTX, write zero frame to wavout to maintain duration */
	if (frm_bit.size == 0 || frm_bit.type != PJMEDIA_FRAME_TYPE_AUDIO) {
	    out_frm.buf = (char*)pcmbuf;
	    out_frm.size = 160;
	    CHECK( pjmedia_port_put_frame(wavout, &out_frm) );
	    TRACE_((THIS_FILE, "%d.%03d read: %u, enc: %u",
		    T, frm_pcm.size, frm_bit.size));
	    continue;
	}
	
	/* Parse the bitstream (not really necessary for this case
	 * since we always decode 1 frame, but it's still good
	 * for testing)
	 */
	ts.u64 = 0;
	cnt = PJ_ARRAY_SIZE(frames);
	CHECK( codec->op->parse(codec, bitstream, frm_bit.size, &ts, &cnt, 
			        frames) );
	CHECK( (cnt==1 ? PJ_SUCCESS : -1) );

	/* Decode or simulate packet loss */
	out_frm.buf = (char*)pcmbuf;
	out_frm.size = sizeof(pcmbuf);
	
	if ((pj_rand() % 100) < (int)lost_pct) {
	    /* Simulate loss */
	    CHECK( codec->op->recover(codec, sizeof(pcmbuf), &out_frm) );
	    TRACE_((THIS_FILE, "%d.%03d Packet lost", T));
	} else {
	    /* Decode */
	    CHECK( codec->op->decode(codec, &frames[0], sizeof(pcmbuf), 
				     &out_frm) );
	}

	/* Write to WAV */
	CHECK( pjmedia_port_put_frame(wavout, &out_frm) );

	TRACE_((THIS_FILE, "%d.%03d read: %u, enc: %u, dec/write: %u",
		T, frm_pcm.size, frm_bit.size, out_frm.size));
    }

    /* Close wavs */
    pjmedia_port_destroy(wavout);
    pjmedia_port_destroy(wavin);

    /* Close codec */
    codec->op->close(codec);
    pjmedia_codec_mgr_dealloc_codec(cm, codec);

    /* Release pool */
    pj_pool_release(pool);

    return PJ_SUCCESS;
}


int main(int argc, char *argv[])
{
    pj_caching_pool cp;
    pj_time_val t0, t1;
    pj_status_t status;

    if (argc != 4) {
	puts(desc);
	return 1;
    }

    CHECK( pj_init(0) );
    
    pj_caching_pool_init(0, &cp, NULL, 0);

//    CHECK( pjmedia_endpt_create(&cp.factory, NULL, 1, &mept) );
    CHECK( pjmedia_endpt_create(0, &cp.factory, NULL, 1, 0, &mept) );

    /* Register all codecs */
#if PJMEDIA_HAS_G711_CODEC
    CHECK( pjmedia_codec_g711_init(mept) );
#endif
#if PJMEDIA_HAS_GSM_CODEC
    CHECK( pjmedia_codec_gsm_init(mept) );
#endif
#if PJMEDIA_HAS_ILBC_CODEC
    CHECK( pjmedia_codec_ilbc_init(mept, 30) );
#endif
#if PJMEDIA_HAS_SPEEX_CODEC
    CHECK( pjmedia_codec_speex_init(mept, 0, 5, 5) );
#endif
#if PJMEDIA_HAS_G722_CODEC
    CHECK( pjmedia_codec_g722_init(mept) );
#endif
#if PJMEDIA_HAS_OPENCORE_AMRNB_CODEC
    CHECK( pjmedia_codec_opencore_amrnb_init(mept) );
#endif

    pj_gettimeofday(&t0);
    status = enc_dec_test(argv[1], argv[2], argv[3]);
    pj_gettimeofday(&t1);
    PJ_TIME_VAL_SUB(t1, t0);

    pjmedia_endpt_destroy(mept);
    pj_caching_pool_destroy(&cp);
    pj_shutdown(0);

    if (status == PJ_SUCCESS) {
	puts("");
	puts("Success");
	printf("Duration: %ds.%03d\n", file_msec_duration/1000, 
				       file_msec_duration%1000);
	printf("Time: %lds.%03ld\n", t1.sec, t1.msec);
    }

    return 0;
}

