/* $Id: resample_speex.c 3553 2011-05-05 06:14:19Z nanang $ */
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

#if PJMEDIA_RESAMPLE_IMP==PJMEDIA_RESAMPLE_SPEEX

#include <speex/speex_resampler.h>

#define THIS_FILE   "resample_speex.c"


struct pjmedia_resample
{
    SpeexResamplerState *state;
    unsigned		 in_samples_per_frame;
    unsigned		 out_samples_per_frame;
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
    int quality;
    int err;

    PJ_ASSERT_RETURN(pool && p_resample && rate_in &&
		     rate_out && samples_per_frame, PJ_EINVAL);

    resample = PJ_POOL_ZALLOC_T(pool, pjmedia_resample);
    PJ_ASSERT_RETURN(resample, PJ_ENOMEM);

    if (high_quality) {
	if (large_filter)
	    quality = 10;
	else
	    quality = 7;
    } else {
	quality = 3;
    }

    resample->in_samples_per_frame = samples_per_frame;
    resample->out_samples_per_frame = rate_out / (rate_in / samples_per_frame);
    resample->state = speex_resampler_init(channel_count,  rate_in, rate_out, 
                                           quality, &err);
    if (resample->state == NULL || err != RESAMPLER_ERR_SUCCESS)
	return PJ_ENOMEM;


    *p_resample = resample;

    PJ_LOG(5,(THIS_FILE, 
	      "resample created: quality=%d, ch=%d, in/out rate=%d/%d", 
	      quality, channel_count, rate_in, rate_out));
    return PJ_SUCCESS;
}


PJ_DEF(void) pjmedia_resample_run( pjmedia_resample *resample,
				   const pj_int16_t *input,
				   pj_int16_t *output )
{
    spx_uint32_t in_length, out_length;

    PJ_ASSERT_ON_FAIL(resample, return);

    in_length = resample->in_samples_per_frame;
    out_length = resample->out_samples_per_frame;

    speex_resampler_process_interleaved_int(resample->state,
					    (const __int16 *)input, &in_length,
					    (__int16 *)output, &out_length);

    pj_assert(in_length == resample->in_samples_per_frame);
    pj_assert(out_length == resample->out_samples_per_frame);
}


PJ_DEF(unsigned) pjmedia_resample_get_input_size(pjmedia_resample *resample)
{
    PJ_ASSERT_RETURN(resample != NULL, 0);
    return resample->in_samples_per_frame;
}


PJ_DEF(void) pjmedia_resample_destroy(pjmedia_resample *resample)
{
    PJ_ASSERT_ON_FAIL(resample, return);
    if (resample->state) {
	speex_resampler_destroy(resample->state);
	resample->state = NULL;
    }
}

#else /* PJMEDIA_RESAMPLE_IMP==PJMEDIA_RESAMPLE_SPEEX */

int pjmedia_resample_speex_excluded;

#endif	/* PJMEDIA_RESAMPLE_IMP==PJMEDIA_RESAMPLE_SPEEX */

