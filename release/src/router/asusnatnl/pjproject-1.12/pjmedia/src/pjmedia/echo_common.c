/* $Id: echo_common.c 3567 2011-05-15 12:54:28Z ming $ */
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
#include <pjmedia/delaybuf.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/list.h>
#include <pj/log.h>
#include <pj/math.h>
#include <pj/pool.h>
#include "echo_internal.h"

#define THIS_FILE   "echo_common.c"

typedef struct ec_operations ec_operations;

struct frame
{
    PJ_DECL_LIST_MEMBER(struct frame);
    short   buf[1];
};

struct pjmedia_echo_state
{
    pj_pool_t	    *pool;
    char	    *obj_name;
    unsigned	     samples_per_frame;
    void	    *state;
    ec_operations   *op;

    pj_bool_t	     lat_ready;	    /* lat_buf has been filled in.	    */
    struct frame     lat_buf;	    /* Frame queue for delayed playback	    */
    struct frame     lat_free;	    /* Free frame list.			    */

    pjmedia_delay_buf	*delay_buf;
    pj_int16_t	    *frm_buf;
};


struct ec_operations
{
    const char *name;

    pj_status_t (*ec_create)(pj_pool_t *pool,
			     unsigned clock_rate,
			     unsigned channel_count,
			     unsigned samples_per_frame,
			     unsigned tail_ms,
			     unsigned options,
			     void **p_state );
    pj_status_t (*ec_destroy)(void *state );
    void        (*ec_reset)(void *state );
    pj_status_t (*ec_cancel)(void *state,
			     pj_int16_t *rec_frm,
			     const pj_int16_t *play_frm,
			     unsigned options,
			     void *reserved );
};


static struct ec_operations echo_supp_op = 
{
    "Echo suppressor",
    &echo_supp_create,
    &echo_supp_destroy,
    &echo_supp_reset,
    &echo_supp_cancel_echo
};



/*
 * Speex AEC prototypes
 */
#if defined(PJMEDIA_HAS_SPEEX_AEC) && PJMEDIA_HAS_SPEEX_AEC!=0
static struct ec_operations speex_aec_op = 
{
    "AEC",
    &speex_aec_create,
    &speex_aec_destroy,
    &speex_aec_reset,
    &speex_aec_cancel_echo
};
#endif


/*
 * IPP AEC prototypes
 */
#if defined(PJMEDIA_HAS_INTEL_IPP_AEC) && PJMEDIA_HAS_INTEL_IPP_AEC!=0
static struct ec_operations ipp_aec_op = 
{
    "IPP AEC",
    &ipp_aec_create,
    &ipp_aec_destroy,
    &ipp_aec_reset,
    &ipp_aec_cancel_echo
};
#endif

/*
 * Create the echo canceller. 
 */
PJ_DEF(pj_status_t) pjmedia_echo_create( pj_pool_t *pool,
					 unsigned clock_rate,
					 unsigned samples_per_frame,
					 unsigned tail_ms,
					 unsigned latency_ms,
					 unsigned options,
					 pjmedia_echo_state **p_echo )
{
    return pjmedia_echo_create2(pool, clock_rate, 1, samples_per_frame,
				tail_ms, latency_ms, options, p_echo);
}

/*
 * Create the echo canceller. 
 */
PJ_DEF(pj_status_t) pjmedia_echo_create2(pj_pool_t *pool,
					 unsigned clock_rate,
					 unsigned channel_count,
					 unsigned samples_per_frame,
					 unsigned tail_ms,
					 unsigned latency_ms,
					 unsigned options,
					 pjmedia_echo_state **p_echo )
{
    unsigned ptime, lat_cnt;
    unsigned delay_buf_opt = 0;
    pjmedia_echo_state *ec;
    pj_status_t status;

    /* Create new pool and instantiate and init the EC */
    pool = pj_pool_create(pool->factory, "ec%p", 256, 256, NULL);
    ec = PJ_POOL_ZALLOC_T(pool, struct pjmedia_echo_state);
    ec->pool = pool;
    ec->obj_name = pool->obj_name;
    ec->samples_per_frame = samples_per_frame;
    ec->frm_buf = (pj_int16_t*)pj_pool_alloc(pool, samples_per_frame<<1);
    pj_list_init(&ec->lat_buf);
    pj_list_init(&ec->lat_free);

    /* Select the backend algorithm */
    if (0) {
	/* Dummy */
	;
#if defined(PJMEDIA_HAS_SPEEX_AEC) && PJMEDIA_HAS_SPEEX_AEC!=0
    } else if ((options & PJMEDIA_ECHO_ALGO_MASK) == PJMEDIA_ECHO_SPEEX ||
	       (options & PJMEDIA_ECHO_ALGO_MASK) == PJMEDIA_ECHO_DEFAULT) 
    {
	ec->op = &speex_aec_op;
#endif

#if defined(PJMEDIA_HAS_INTEL_IPP_AEC) && PJMEDIA_HAS_INTEL_IPP_AEC!=0
    } else if ((options & PJMEDIA_ECHO_ALGO_MASK) == PJMEDIA_ECHO_IPP ||
	       (options & PJMEDIA_ECHO_ALGO_MASK) == PJMEDIA_ECHO_DEFAULT)
    {
	ec->op = &ipp_aec_op;

#endif

    } else {
	ec->op = &echo_supp_op;
    }

    PJ_LOG(5,(ec->obj_name, "Creating %s", ec->op->name));

    /* Instantiate EC object */
    status = (*ec->op->ec_create)(pool, clock_rate, channel_count, 
				  samples_per_frame, tail_ms, 
				  options, &ec->state);
    if (status != PJ_SUCCESS) {
	pj_pool_release(pool);
	return status;
    }

    /* Create latency buffers */
    ptime = samples_per_frame * 1000 / clock_rate;
    if (latency_ms > ptime) {
	/* Normalize latency with delaybuf/WSOLA latency */
	latency_ms -= PJ_MIN(ptime, PJMEDIA_WSOLA_DELAY_MSEC);
    }
    if (latency_ms < ptime) {
	/* Give at least one frame delay to simplify programming */
	latency_ms = ptime;
    }
    lat_cnt = latency_ms / ptime;
    while (lat_cnt--)  {
	struct frame *frm;

	frm = (struct frame*) pj_pool_alloc(pool, (samples_per_frame<<1) +
						  sizeof(struct frame));
	pj_list_push_back(&ec->lat_free, frm);
    }

    /* Create delay buffer to compensate drifts */
    if (options & PJMEDIA_ECHO_USE_SIMPLE_FIFO)
        delay_buf_opt |= PJMEDIA_DELAY_BUF_SIMPLE_FIFO;
    status = pjmedia_delay_buf_create(ec->pool, ec->obj_name, clock_rate, 
				      samples_per_frame, channel_count,
				      (PJMEDIA_SOUND_BUFFER_COUNT+1) * ptime,
				      delay_buf_opt, &ec->delay_buf);
    if (status != PJ_SUCCESS) {
	pj_pool_release(pool);
	return status;
    }

    PJ_LOG(4,(ec->obj_name, 
	      "%s created, clock_rate=%d, channel=%d, "
	      "samples per frame=%d, tail length=%d ms, "
	      "latency=%d ms", 
	      ec->op->name, clock_rate, channel_count, samples_per_frame,
	      tail_ms, latency_ms));

    /* Done */
    *p_echo = ec;

    return PJ_SUCCESS;
}


/*
 * Destroy the Echo Canceller. 
 */
PJ_DEF(pj_status_t) pjmedia_echo_destroy(pjmedia_echo_state *echo )
{
    (*echo->op->ec_destroy)(echo->state);

    if (echo->delay_buf) {
	pjmedia_delay_buf_destroy(echo->delay_buf);
	echo->delay_buf = NULL;
    }

    pj_pool_release(echo->pool);
    return PJ_SUCCESS;
}


/*
 * Reset the echo canceller.
 */
PJ_DEF(pj_status_t) pjmedia_echo_reset(pjmedia_echo_state *echo )
{
    while (!pj_list_empty(&echo->lat_buf)) {
	struct frame *frm;
	frm = echo->lat_buf.next;
	pj_list_erase(frm);
	pj_list_push_back(&echo->lat_free, frm);
    }
    echo->lat_ready = PJ_FALSE;
    pjmedia_delay_buf_reset(echo->delay_buf);
    echo->op->ec_reset(echo->state);
    return PJ_SUCCESS;
}


/*
 * Let the Echo Canceller know that a frame has been played to the speaker.
 */
PJ_DEF(pj_status_t) pjmedia_echo_playback( pjmedia_echo_state *echo,
					   pj_int16_t *play_frm )
{
    /* Playing frame should be stored, as it will be used by echo_capture() 
     * as reference frame, delay buffer is used for storing the playing frames
     * as in case there was clock drift between mic & speaker.
     *
     * Ticket #830:
     * Note that pjmedia_delay_buf_put() may modify the input frame and those
     * modified frames may not be smooth, i.e: if there were two or more
     * consecutive pjmedia_delay_buf_get() before next pjmedia_delay_buf_put(),
     * so we'll just feed the delay buffer with the copy of playing frame,
     * instead of the original playing frame. However this will cause the EC 
     * uses slight 'different' frames (for reference) than actually played 
     * by the speaker.
     */
    pjmedia_copy_samples(echo->frm_buf, play_frm, 
			 echo->samples_per_frame);
    pjmedia_delay_buf_put(echo->delay_buf, echo->frm_buf);

    if (!echo->lat_ready) {
	/* We've not built enough latency in the buffer, so put this frame
	 * in the latency buffer list.
	 */
	struct frame *frm;

	if (pj_list_empty(&echo->lat_free)) {
	    echo->lat_ready = PJ_TRUE;
	    PJ_LOG(5,(echo->obj_name, "Latency bufferring complete"));
	    return PJ_SUCCESS;
	}
	    
	frm = echo->lat_free.prev;
	pj_list_erase(frm);

	/* Move one frame from delay buffer to the latency buffer. */
	pjmedia_delay_buf_get(echo->delay_buf, echo->frm_buf);
	pjmedia_copy_samples(frm->buf, echo->frm_buf, echo->samples_per_frame);
	pj_list_push_back(&echo->lat_buf, frm);
    }

    return PJ_SUCCESS;
}


/*
 * Let the Echo Canceller knows that a frame has been captured from 
 * the microphone.
 */
PJ_DEF(pj_status_t) pjmedia_echo_capture( pjmedia_echo_state *echo,
					  pj_int16_t *rec_frm,
					  unsigned options )
{
    struct frame *oldest_frm;
    pj_status_t status, rc;

    if (!echo->lat_ready) {
	/* Prefetching to fill in the desired latency */
	PJ_LOG(5,(echo->obj_name, "Prefetching.."));
	return PJ_SUCCESS;
    }

    /* Retrieve oldest frame from the latency buffer */
    oldest_frm = echo->lat_buf.next;
    pj_list_erase(oldest_frm);

    /* Cancel echo using this reference frame */
    status = pjmedia_echo_cancel(echo, rec_frm, oldest_frm->buf, 
				 options, NULL);

    /* Move one frame from delay buffer to the latency buffer. */
    rc = pjmedia_delay_buf_get(echo->delay_buf, oldest_frm->buf);
    if (rc != PJ_SUCCESS) {
	/* Ooops.. no frame! */
	PJ_LOG(5,(echo->obj_name, 
		  "No frame from delay buffer. This will upset EC later"));
	pjmedia_zero_samples(oldest_frm->buf, echo->samples_per_frame);
    }
    pj_list_push_back(&echo->lat_buf, oldest_frm);
    
    return status;
}


/*
 * Perform echo cancellation.
 */
PJ_DEF(pj_status_t) pjmedia_echo_cancel( pjmedia_echo_state *echo,
					 pj_int16_t *rec_frm,
					 const pj_int16_t *play_frm,
					 unsigned options,
					 void *reserved )
{
    return (*echo->op->ec_cancel)( echo->state, rec_frm, play_frm, options, 
				   reserved);
}

