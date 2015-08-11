/* $Id: resample_libsamplerate.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia/resample.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>

/*
 * HOW TO ACTIVATE LIBSAMPLERATE (a.k.a SRC/Secret Rabbit Code) AS
 * PJMEDIA'S SAMPLE RATE CONVERSION BACKEND
 *
 * See README.txt in third_party/samplerate directory.
 */


#if PJMEDIA_RESAMPLE_IMP==PJMEDIA_RESAMPLE_LIBSAMPLERATE

#include "../../third_party/libsamplerate/src/samplerate.h"

#define THIS_FILE   "resample_libsamplerate.c"

#if defined(_MSC_VER)
#   ifdef _DEBUG
#	pragma comment( lib, "../../third_party/lib/libsamplerate-i386-win32-vc-debug.lib")
#   else
#	pragma comment( lib, "../../third_party/lib/libsamplerate-i386-win32-vc-release.lib")
#   endif
#endif


struct pjmedia_resample
{
    SRC_STATE	*state;
    unsigned	 in_samples;
    unsigned	 out_samples;
    float	*frame_in, *frame_out;
    unsigned	 in_extra, out_extra;
    double	 ratio;
};


PJ_DEF(pj_status_t) pjmedia_resample_create( pj_pool_t *pool,
					     pj_bool_t high_quality,
					     pj_bool_t large_filter,
					     unsigned channel_count,
					     unsigned rate_in,
					     unsigned rate_out,
					     unsigned samples_per_frame,
					     pjmedia_resample **p_resample)
{
    pjmedia_resample *resample;
    int type, err;

    PJ_ASSERT_RETURN(pool && p_resample && rate_in &&
		     rate_out && samples_per_frame, PJ_EINVAL);

    resample = PJ_POOL_ZALLOC_T(pool, pjmedia_resample);
    PJ_ASSERT_RETURN(resample, PJ_ENOMEM);

    /* Select conversion type */
    if (high_quality) {
	type = large_filter ? SRC_SINC_BEST_QUALITY : SRC_SINC_MEDIUM_QUALITY;
    } else {
	type = large_filter ? SRC_SINC_FASTEST : SRC_LINEAR;
    }

    /* Create converter */
    resample->state = src_new(type, channel_count, &err);
    if (resample->state == NULL) {
	PJ_LOG(4,(THIS_FILE, "Error creating resample: %s", 
		  src_strerror(err)));
	return PJMEDIA_ERROR;
    }

    /* Calculate ratio */
    resample->ratio = rate_out * 1.0 / rate_in;

    /* Calculate number of samples for input and output */
    resample->in_samples = samples_per_frame;
    resample->out_samples = rate_out / (rate_in / samples_per_frame);

    resample->frame_in = (float*) 
			 pj_pool_calloc(pool, 
					resample->in_samples + 8, 
					sizeof(float));

    resample->frame_out = (float*) 
			  pj_pool_calloc(pool, 
					 resample->out_samples + 8, 
					 sizeof(float));

    /* Set the converter ratio */
    err = src_set_ratio(resample->state, resample->ratio);
    if (err != 0) {
	PJ_LOG(4,(THIS_FILE, "Error creating resample: %s", 
		  src_strerror(err)));
	return PJMEDIA_ERROR;
    }

    /* Done */

    PJ_LOG(5,(THIS_FILE, 
	      "Resample using libsamplerate %s, type=%s (%s), "
	      "ch=%d, in/out rate=%d/%d", 
	      src_get_version(),
	      src_get_name(type), src_get_description(type),
	      channel_count, rate_in, rate_out));

    *p_resample = resample;

    return PJ_SUCCESS;
}


PJ_DEF(void) pjmedia_resample_run( pjmedia_resample *resample,
				   const pj_int16_t *input,
				   pj_int16_t *output )
{
    SRC_DATA src_data;

    /* Convert samples to float */
    src_short_to_float_array(input, resample->frame_in, 
			     resample->in_samples);

    if (resample->in_extra) {
	unsigned i;

	for (i=0; i<resample->in_extra; ++i)
	    resample->frame_in[resample->in_samples+i] =
		resample->frame_in[resample->in_samples-1];
    }

    /* Prepare SRC_DATA */
    pj_bzero(&src_data, sizeof(src_data));
    src_data.data_in = resample->frame_in;
    src_data.data_out = resample->frame_out;
    src_data.input_frames = resample->in_samples + resample->in_extra;
    src_data.output_frames = resample->out_samples + resample->out_extra;
    src_data.src_ratio = resample->ratio;

    /* Process! */
    src_process(resample->state, &src_data);

    /* Convert output back to short */
    src_float_to_short_array(resample->frame_out, output,
			     src_data.output_frames_gen);

    /* Replay last sample if conversion couldn't fill up the whole 
     * frame. This could happen for example with 22050 to 16000 conversion.
     */
    if (src_data.output_frames_gen < (int)resample->out_samples) {
	unsigned i;

	if (resample->in_extra < 4)
	    resample->in_extra++;

	for (i=src_data.output_frames_gen; 
	     i<resample->out_samples; ++i)
	{
	    output[i] = output[src_data.output_frames_gen-1];
	}
    }
}


PJ_DEF(unsigned) pjmedia_resample_get_input_size(pjmedia_resample *resample)
{
    PJ_ASSERT_RETURN(resample != NULL, 0);
    return resample->in_samples;
}


PJ_DEF(void) pjmedia_resample_destroy(pjmedia_resample *resample)
{
    PJ_ASSERT_ON_FAIL(resample, return);
    if (resample->state) {
	src_delete(resample->state);
	resample->state = NULL;

	PJ_LOG(5,(THIS_FILE, "Resample destroyed"));
    }
}

#else /* PJMEDIA_RESAMPLE_IMP==PJMEDIA_RESAMPLE_LIBSAMPLERATE */

int pjmedia_libsamplerate_excluded;

#endif	/* PJMEDIA_RESAMPLE_IMP==PJMEDIA_RESAMPLE_LIBSAMPLERATE */

