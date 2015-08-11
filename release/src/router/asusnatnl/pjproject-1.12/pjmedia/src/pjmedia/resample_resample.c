/* $Id: resample_resample.c 3553 2011-05-05 06:14:19Z nanang $ */
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


#if PJMEDIA_RESAMPLE_IMP==PJMEDIA_RESAMPLE_LIBRESAMPLE

#include <third_party/resample/include/resamplesubs.h>

#define THIS_FILE   "resample.c"



struct pjmedia_resample
{
    double	 factor;	/* Conversion factor = rate_out / rate_in.  */
    pj_bool_t	 large_filter;	/* Large filter?			    */
    pj_bool_t	 high_quality;	/* Not fast?				    */
    unsigned	 xoff;		/* History and lookahead size, in samples   */
    unsigned	 frame_size;	/* Samples per frame.			    */
    unsigned	 channel_cnt;	/* Channel count.			    */

    /* Buffer for monochannel */
    pj_int16_t	*buffer;	/* Input buffer.			    */

    /* Buffer for multichannel */
    pj_int16_t **in_buffer;	/* Array of input buffer for each channel.  */
    pj_int16_t  *tmp_buffer;	/* Temporary output buffer for processing.  */
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

    PJ_ASSERT_RETURN(pool && p_resample && rate_in &&
		     rate_out && samples_per_frame, PJ_EINVAL);

    resample = PJ_POOL_ZALLOC_T(pool, pjmedia_resample);
    PJ_ASSERT_RETURN(resample, PJ_ENOMEM);

    /*
     * If we're downsampling, always use the fast algorithm since it seems
     * to yield the same quality.
     */
    if (rate_out < rate_in) {
	//no this is not a good idea. It sounds pretty good with speech,
	//but very poor with background noise etc.
	//high_quality = 0;
    }

    resample->factor = rate_out * 1.0 / rate_in;
    resample->large_filter = large_filter;
    resample->high_quality = high_quality;
    resample->channel_cnt = channel_count;
    resample->frame_size = samples_per_frame;

    if (high_quality) {
	/* This is a bug in xoff calculation, thanks Stephane Lussier
	 * of Macadamian dot com.
	 *   resample->xoff = large_filter ? 32 : 6;
	 */
	resample->xoff = res_GetXOFF(resample->factor, (char)large_filter);
    } else {
	resample->xoff = 1;
    }

    if (channel_count == 1) {
	unsigned size;

	/* Allocate input buffer */
	size = (samples_per_frame + 2*resample->xoff) * sizeof(pj_int16_t);
	resample->buffer = (pj_int16_t*) pj_pool_alloc(pool, size);
	PJ_ASSERT_RETURN(resample->buffer, PJ_ENOMEM);

	pjmedia_zero_samples(resample->buffer, resample->xoff*2);

    } else if (channel_count > 1) {
	unsigned i, size;

	/* Allocate input buffer table */
	size = channel_count * sizeof(pj_int16_t*);
	resample->in_buffer = (pj_int16_t**)pj_pool_alloc(pool, size);

	/* Allocate input buffer */
	size = (samples_per_frame/channel_count + 2*resample->xoff) * 
	       sizeof(pj_int16_t);
	for (i = 0; i < channel_count; ++i) {
	    resample->in_buffer[i] = (pj_int16_t*)pj_pool_alloc(pool, size);
	    PJ_ASSERT_RETURN(resample->in_buffer, PJ_ENOMEM);
	    pjmedia_zero_samples(resample->in_buffer[i], resample->xoff*2);
	}

	/* Allocate temporary output buffer */
	size = (unsigned) (resample->frame_size * sizeof(pj_int16_t) * 
			   resample->factor / channel_count + 0.5);
	resample->tmp_buffer = (pj_int16_t*) pj_pool_alloc(pool, size);
	PJ_ASSERT_RETURN(resample->tmp_buffer, PJ_ENOMEM);
    }

    *p_resample = resample;

    PJ_LOG(5,(THIS_FILE, "resample created: %s qualiy, %s filter, in/out "
			  "rate=%d/%d", 
			  (high_quality?"high":"low"),
			  (large_filter?"large":"small"),
			  rate_in, rate_out));
    return PJ_SUCCESS;
}



PJ_DEF(void) pjmedia_resample_run( pjmedia_resample *resample,
				   const pj_int16_t *input,
				   pj_int16_t *output )
{
    PJ_ASSERT_ON_FAIL(resample, return);

    /* Okay chaps, here's how we do resampling.
     *
     * The original resample algorithm requires xoff samples *before* the
     * input buffer as history, and another xoff samples *after* the
     * end of the input buffer as lookahead. Since application can only
     * supply framesize buffer on each run, PJMEDIA needs to arrange the
     * buffer to meet these requirements.
     *
     * So here comes the trick.
     *
     * First of all, because of the history and lookahead requirement, 
     * resample->buffer need to accomodate framesize+2*xoff samples in its
     * buffer. This is done when the buffer is created.
     *
     * On the first run, the input frame (supplied by application) is
     * copied to resample->buffer at 2*xoff position. The first 2*xoff
     * samples are initially zeroed (in the initialization). The resample
     * algorithm then invoked at resample->buffer+xoff ONLY, thus giving
     * it one xoff at the beginning as zero, and one xoff at the end
     * as the end of the original input. The resample algorithm will see
     * that the first xoff samples in the input as zero.
     *
     * So here's the layout of resample->buffer on the first run.
     *
     * run 0 
     *     +------+------+--------------+
     *     | 0000 | 0000 |  frame0...   |
     *     +------+------+--------------+
     *     ^      ^      ^              ^
	 *     0    xoff  2*xoff       size+2*xoff 
	 *
     * (Note again: resample algorithm is called at resample->buffer+xoff)
     *
     * At the end of the run, 2*xoff samples from the end of 
     * resample->buffer are copied to the beginning of resample->buffer.
     * The first xoff part of this will be used as history for the next
     * run, and the second xoff part of this is actually the start of
     * resampling for the next run.
     *
     * And the first run completes, the function returns.
     *
     * 
     * On the next run, the input frame supplied by application is again
     * copied at 2*xoff position in the resample->buffer, and the 
     * resample algorithm is again invoked at resample->buffer+xoff 
     * position. So effectively, the resample algorithm will start its
     * operation on the last xoff from the previous frame, and gets the
     * history from the last 2*xoff of the previous frame, and the look-
     * ahead from the last xoff of current frame.
     *
     * So on this run, the buffer layout is:
     *
     * run 1
     *     +------+------+--------------+
     *     | frm0 | frm0 |  frame1...   |
     *     +------+------+--------------+
     *     ^      ^      ^              ^
	 *     0    xoff  2*xoff       size+2*xoff 
     *
     * As you can see from above diagram, the resampling algorithm is
     * actually called from the last xoff part of previous frame (frm0).
     *
     * And so on the process continues for the next frame, and the next,
     * and the next, ...
     *
     */
    if (resample->channel_cnt == 1) {
	pj_int16_t *dst_buf;
	const pj_int16_t *src_buf;

	/* Prepare input frame */
	dst_buf = resample->buffer + resample->xoff*2;
	pjmedia_copy_samples(dst_buf, input, resample->frame_size);

	/* Resample */
	if (resample->high_quality) {
	    res_Resample(resample->buffer + resample->xoff, output,
			 resample->factor, (pj_uint16_t)resample->frame_size,
			 (char)resample->large_filter, (char)PJ_TRUE);
	} else {
	    res_SrcLinear(resample->buffer + resample->xoff, output, 
			  resample->factor, (pj_uint16_t)resample->frame_size);
	}

	/* Update history */
	dst_buf = resample->buffer;
	src_buf = input + resample->frame_size - resample->xoff*2;
	pjmedia_copy_samples(dst_buf, src_buf, resample->xoff * 2);

    } else { /* Multichannel */
	unsigned i, j;

	for (i = 0; i < resample->channel_cnt; ++i) {
	    pj_int16_t *dst_buf;
	    const pj_int16_t *src_buf;
	    unsigned mono_frm_sz_in;
	    unsigned mono_frm_sz_out;
    	
	    mono_frm_sz_in  = resample->frame_size / resample->channel_cnt;
	    mono_frm_sz_out = (unsigned)(mono_frm_sz_in * resample->factor + 0.5);

	    /* Deinterleave input */
	    dst_buf = resample->in_buffer[i] + resample->xoff*2;
	    src_buf = input + i;
	    for (j = 0; j < mono_frm_sz_in; ++j) {
		*dst_buf++ = *src_buf;
		src_buf += resample->channel_cnt;
	    }

	    /* Resample this channel */
	    if (resample->high_quality) {
		res_Resample(resample->in_buffer[i] + resample->xoff,
			     resample->tmp_buffer, resample->factor,
			     (pj_uint16_t)mono_frm_sz_in,
			     (char)resample->large_filter, (char)PJ_TRUE);
	    } else {
		res_SrcLinear( resample->in_buffer[i],
			       resample->tmp_buffer, 
			       resample->factor, 
			       (pj_uint16_t)mono_frm_sz_in);
	    }

	    /* Update history */
	    dst_buf = resample->in_buffer[i];
	    src_buf = resample->in_buffer[i] + mono_frm_sz_in;
	    pjmedia_copy_samples(dst_buf, src_buf, resample->xoff * 2);

	    /* Reinterleave output */
	    dst_buf = output + i;
	    src_buf = resample->tmp_buffer;
	    for (j = 0; j < mono_frm_sz_out; ++j) {
		*dst_buf = *src_buf++;
		dst_buf += resample->channel_cnt;
	    }
	}
    }
}

PJ_DEF(unsigned) pjmedia_resample_get_input_size(pjmedia_resample *resample)
{
    PJ_ASSERT_RETURN(resample != NULL, 0);
    return resample->frame_size;
}

PJ_DEF(void) pjmedia_resample_destroy(pjmedia_resample *resample)
{
    PJ_UNUSED_ARG(resample);
}


#elif PJMEDIA_RESAMPLE_IMP==PJMEDIA_RESAMPLE_NONE

/*
 * This is the configuration when sample rate conversion is disabled.
 */
PJ_DEF(pj_status_t) pjmedia_resample_create( pj_pool_t *pool,
					     pj_bool_t high_quality,
					     pj_bool_t large_filter,
					     unsigned channel_count,
					     unsigned rate_in,
					     unsigned rate_out,
					     unsigned samples_per_frame,
					     pjmedia_resample **p_resample) 
{
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(high_quality);
    PJ_UNUSED_ARG(large_filter);
    PJ_UNUSED_ARG(channel_count);
    PJ_UNUSED_ARG(rate_in);
    PJ_UNUSED_ARG(rate_out);
    PJ_UNUSED_ARG(samples_per_frame);
    PJ_UNUSED_ARG(p_resample);

    return PJ_EINVALIDOP;
}

PJ_DEF(void) pjmedia_resample_run( pjmedia_resample *resample,
				   const pj_int16_t *input,
				   pj_int16_t *output ) 
{
    PJ_UNUSED_ARG(resample);
    PJ_UNUSED_ARG(input);
    PJ_UNUSED_ARG(output);
}

PJ_DEF(unsigned) pjmedia_resample_get_input_size(pjmedia_resample *resample) 
{
    PJ_UNUSED_ARG(resample);
    return 0;
}

PJ_DEF(void) pjmedia_resample_destroy(pjmedia_resample *resample) 
{
    PJ_UNUSED_ARG(resample);
}

#endif	/* PJMEDIA_RESAMPLE_IMP */

