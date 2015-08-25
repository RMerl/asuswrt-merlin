/* $Id: echo_speex.c 3553 2011-05-05 06:14:19Z nanang $ */
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

#include <pjmedia/echo.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>

#if defined(PJMEDIA_HAS_SPEEX_AEC) && PJMEDIA_HAS_SPEEX_AEC != 0

#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>

#include "echo_internal.h"

typedef struct speex_ec
{
    SpeexEchoState	 *state;
    SpeexPreprocessState *preprocess;

    unsigned		  samples_per_frame;
    unsigned		  prefetch;
    unsigned		  options;
    pj_int16_t		 *tmp_frame;
} speex_ec;



/*
 * Create the AEC. 
 */
PJ_DEF(pj_status_t) speex_aec_create(pj_pool_t *pool,
				     unsigned clock_rate,
				     unsigned channel_count,
				     unsigned samples_per_frame,
				     unsigned tail_ms,
				     unsigned options,
				     void **p_echo )
{
    speex_ec *echo;
    int sampling_rate;

    *p_echo = NULL;

    echo = PJ_POOL_ZALLOC_T(pool, speex_ec);
    PJ_ASSERT_RETURN(echo != NULL, PJ_ENOMEM);

    echo->samples_per_frame = samples_per_frame;
    echo->options = options;

#if 0
    echo->state = speex_echo_state_init_mc(echo->samples_per_frame,
					   clock_rate * tail_ms / 1000,
					   channel_count, channel_count);
#else
    if (channel_count != 1) {
	PJ_LOG(2,("echo_speex.c", "Multichannel EC is not supported by this "
				  "echo canceller. It may not work."));
    }
    echo->state = speex_echo_state_init(echo->samples_per_frame,
    					clock_rate * tail_ms / 1000);
#endif
    if (echo->state == NULL) {
	return PJ_ENOMEM;
    }

    /* Set sampling rate */
    sampling_rate = clock_rate;
    speex_echo_ctl(echo->state, SPEEX_ECHO_SET_SAMPLING_RATE, 
		   &sampling_rate);

    echo->preprocess = speex_preprocess_state_init(echo->samples_per_frame,
						   clock_rate);
    if (echo->preprocess == NULL) {
	speex_echo_state_destroy(echo->state);
	return PJ_ENOMEM;
    }

    /* Disable all preprocessing, we only want echo cancellation */
#if 0
    disabled = 0;
    enabled = 1;
    speex_preprocess_ctl(echo->preprocess, SPEEX_PREPROCESS_SET_DENOISE, 
			 &enabled);
    speex_preprocess_ctl(echo->preprocess, SPEEX_PREPROCESS_SET_AGC, 
			 &disabled);
    speex_preprocess_ctl(echo->preprocess, SPEEX_PREPROCESS_SET_VAD, 
			 &disabled);
    speex_preprocess_ctl(echo->preprocess, SPEEX_PREPROCESS_SET_DEREVERB, 
			 &enabled);
#endif

    /* Control echo cancellation in the preprocessor */
   speex_preprocess_ctl(echo->preprocess, SPEEX_PREPROCESS_SET_ECHO_STATE, 
			echo->state);


    /* Create temporary frame for echo cancellation */
    echo->tmp_frame = (pj_int16_t*) pj_pool_zalloc(pool, 2*samples_per_frame);
    PJ_ASSERT_RETURN(echo->tmp_frame != NULL, PJ_ENOMEM);

    /* Done */
    *p_echo = echo;
    return PJ_SUCCESS;

}


/*
 * Destroy AEC
 */
PJ_DEF(pj_status_t) speex_aec_destroy(void *state )
{
    speex_ec *echo = (speex_ec*) state;

    PJ_ASSERT_RETURN(echo && echo->state, PJ_EINVAL);

    if (echo->state) {
	speex_echo_state_destroy(echo->state);
	echo->state = NULL;
    }

    if (echo->preprocess) {
	speex_preprocess_state_destroy(echo->preprocess);
	echo->preprocess = NULL;
    }

    return PJ_SUCCESS;
}


/*
 * Reset AEC
 */
PJ_DEF(void) speex_aec_reset(void *state )
{
    speex_ec *echo = (speex_ec*) state;
    speex_echo_state_reset(echo->state);
}


/*
 * Perform echo cancellation.
 */
PJ_DEF(pj_status_t) speex_aec_cancel_echo( void *state,
					   pj_int16_t *rec_frm,
					   const pj_int16_t *play_frm,
					   unsigned options,
					   void *reserved )
{
    speex_ec *echo = (speex_ec*) state;

    /* Sanity checks */
    PJ_ASSERT_RETURN(echo && rec_frm && play_frm && options==0 &&
		     reserved==NULL, PJ_EINVAL);

    /* Cancel echo, put output in temporary buffer */
    speex_echo_cancellation(echo->state, (const spx_int16_t*)rec_frm,
			    (const spx_int16_t*)play_frm,
			    (spx_int16_t*)echo->tmp_frame);


    /* Preprocess output */
    speex_preprocess_run(echo->preprocess, (spx_int16_t*)echo->tmp_frame);

    /* Copy temporary buffer back to original rec_frm */
    pjmedia_copy_samples(rec_frm, echo->tmp_frame, echo->samples_per_frame);

    return PJ_SUCCESS;

}

#endif
