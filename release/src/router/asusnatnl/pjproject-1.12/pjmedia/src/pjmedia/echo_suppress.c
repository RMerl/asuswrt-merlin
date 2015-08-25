/* $Id: echo_suppress.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia/types.h>
#include <pjmedia/alaw_ulaw.h>
#include <pjmedia/errno.h>
#include <pjmedia/silencedet.h>
#include <pj/array.h>
#include <pj/assert.h>
#include <pj/lock.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/pool.h>

#include "echo_internal.h"

#define THIS_FILE			    "echo_suppress.c"

/* Maximum float constant */
#define MAX_FLOAT		(float)1.701411e38

/* The effective learn duration (in seconds) before we declare that learning
 * is complete. The actual learning duration itself may be longer depending
 * on the conversation pattern (e.g. we can't detect echo if speaker is only
 * playing silence).
 */
#define MAX_CALC_DURATION_SEC	3

/* The internal audio segment length, in milliseconds. 10ms shold be good
 * and no need to change it.
 */
#define SEGMENT_PTIME		10

/* The length of the template signal in milliseconds. The longer the template,
 * the better correlation will be found, at the expense of more processing
 * and longer learning time.
 */
#define TEMPLATE_PTIME		200

/* How long to look back in the past to see if either mic or speaker is
 * active.
 */
#define SIGNAL_LOOKUP_MSEC	200

/* The minimum level value to be considered as talking, in uLaw complement
 * (0-255).
 */
#define MIN_SIGNAL_ULAW		35

/* The period (in seconds) on which the ES will analize it's effectiveness,
 * and it may trigger soft-reset to force recalculation.
 */
#define CHECK_PERIOD		30

/* Maximum signal level of average echo residue (in uLaw complement). When
 * the residue value exceeds this value, we force the ES to re-learn.
 */
#define MAX_RESIDUE		2.5


#if 0
#   define TRACE_(expr)	PJ_LOG(5,expr)
#else
#   define TRACE_(expr)
#endif

PJ_INLINE(float) FABS(float val)
{
    if (val < 0)
	return -val;
    else
	return val;
}


#if defined(PJ_HAS_FLOATING_POINT) && PJ_HAS_FLOATING_POINT!=0
    typedef float pj_ufloat_t;
#   define pj_ufloat_from_float(f)	(f)
#   define pj_ufloat_mul_u(val1, f)	((val1) * (f))
#   define pj_ufloat_mul_i(val1, f)	((val1) * (f))
#else
    typedef pj_uint32_t pj_ufloat_t;

    pj_ufloat_t pj_ufloat_from_float(float f)
    {
	return (pj_ufloat_t)(f * 65536);
    }

    unsigned pj_ufloat_mul_u(unsigned val1, pj_ufloat_t val2)
    {
	return (val1 * val2) >> 16;
    }

    int pj_ufloat_mul_i(int val1, pj_ufloat_t val2)
    {
	return (val1 * (pj_int32_t)val2) >> 16;
    }
#endif


/* Conversation state */
typedef enum talk_state
{
    ST_NULL,
    ST_LOCAL_TALK,
    ST_REM_SILENT,
    ST_DOUBLETALK,
    ST_REM_TALK
} talk_state_t;

const char *state_names[] = 
{
    "Null",
    "local talking",
    "remote silent",
    "doubletalk",
    "remote talking"
};


/* Description:

   The echo suppressor tries to find the position of echoed signal by looking
   at the correlation between signal played to the speaker (played signal) 
   and the signal captured from the microphone (recorded signal).

   To do this, it first divides the frames (from mic and speaker) into 
   segments, calculate the audio level of the segment, and save the level
   information in the playback and record history (play_hist and rec_hist
   respectively).

   In the history, the newest element (depicted as "t0" in the diagram belo)
   is put in the last position of the array.

   The record history size is as large as the template size (tmpl_cnt), since
   we will use the record history as the template to find the best matching 
   position in the playback history.

   Here is the record history buffer:

       <--templ_cnt-->
       +-------------+
       |   rec_hist  |
       +-------------+
    t-templ_cnt......t0

   As you can see, the newest frame ("t0") is put as the last element in the
   array.

   The playback history size is larger than record history, since we need to
   find the matching pattern in the past. The playback history size is
   "templ_cnt + tail_cnt", where "tail_cnt" is the number of segments equal
   to the maximum tail length. The maximum tail length is set when the ES
   is created.

   Here is the playback history buffer:

       <-----tail_cnt-----> <--templ_cnt-->
       +-------------------+--------------+
       |             play_hist            |
       +-------------------+--------------+
   t-play_hist_cnt...t-templ_cnt.......t0



   Learning:

   During the processing, the ES calculates the following values:
    - the correlation value, that is how similar the playback signal compared
      to the mic signal. The lower the correlation value the better (i.e. more
      similar) the signal is. The correlation value is done over the template
      duration.
    - the gain scaling factor, that is the ratio between mic signal and 
      speaker signal. The ES calculates both the minimum and average ratios.

   The ES calculates both the values above for every tail position in the
   playback history. The values are saved in arrays below:

     <-----tail_cnt----->
     +-------------------+
     |      corr_sum     |
     +-------------------+
     |     min_factor    |
     +-------------------+
     |     avg_factor    |
     +-------------------+

   At the end of processing, the ES iterates through the correlation array and
   picks the tail index with the lowest corr_sum value. This is the position
   where echo is most likely to be found.


   Processing:

   Once learning is done, the ES will change the level of the mic signal 
   depending on the state of the conversation and according to the ratio that
   has been found in the learning phase above.

 */

/*
 * The simple echo suppresor state
 */
typedef struct echo_supp
{
    unsigned	 clock_rate;	    /* Clock rate.			    */
    pj_uint16_t	 samples_per_frame; /* Frame length in samples		    */
    pj_uint16_t  samples_per_segment;/* Segment length in samples	    */
    pj_uint16_t  tail_ms;	    /* Tail length in milliseconds	    */
    pj_uint16_t  tail_samples;	    /* Tail length in samples.		    */

    pj_bool_t	 learning;	    /* Are we still learning yet?	    */
    talk_state_t talk_state;	    /* Current talking state		    */
    int		 tail_index;	    /* Echo location, -1 if not found	    */

    unsigned	 max_calc;	    /* # of calc before learning complete.
                                       (see MAX_CALC_DURATION_SEC)	    */
    unsigned	 calc_cnt;	    /* Number of calculations so far	    */

    unsigned	 update_cnt;	    /* # of updates			    */
    unsigned	 templ_cnt;	    /* Template length, in # of segments    */
    unsigned	 tail_cnt;	    /* Tail length, in # of segments	    */
    unsigned	 play_hist_cnt;	    /* # of segments in play_hist	    */
    pj_uint16_t *play_hist;	    /* Array of playback levels		    */
    pj_uint16_t *rec_hist;	    /* Array of rec levels		    */

    float	*corr_sum;	    /* Array of corr for each tail pos.	    */
    float	*tmp_corr;	    /* Temporary corr array calculation	    */
    float	 best_corr;	    /* Best correlation so far.		    */

    unsigned	 sum_rec_level;	    /* Running sum of level in rec_hist	    */
    float	 rec_corr;	    /* Running corr in rec_hist.	    */

    unsigned	 sum_play_level0;   /* Running sum of level for first pos   */
    float	 play_corr0;	    /* Running corr for first pos .	    */

    float	*min_factor;	    /* Array of minimum scaling factor	    */
    float	*avg_factor;	    /* Array of average scaling factor	    */
    float	*tmp_factor;	    /* Array to store provisional result    */

    unsigned	 running_cnt;	    /* Running duration in # of frames	    */
    float	 residue;	    /* Accummulated echo residue.	    */
    float	 last_factor;	    /* Last factor applied to mic signal    */
} echo_supp;



/*
 * Create. 
 */
PJ_DEF(pj_status_t) echo_supp_create( pj_pool_t *pool,
				      unsigned clock_rate,
				      unsigned channel_count,
				      unsigned samples_per_frame,
				      unsigned tail_ms,
				      unsigned options,
				      void **p_state )
{
    echo_supp *ec;

    PJ_UNUSED_ARG(channel_count);
    PJ_UNUSED_ARG(options);

    PJ_ASSERT_RETURN(samples_per_frame >= SEGMENT_PTIME * clock_rate / 1000,
		     PJ_ENOTSUP);

    ec = PJ_POOL_ZALLOC_T(pool, struct echo_supp);
    ec->clock_rate = clock_rate;
    ec->samples_per_frame = (pj_uint16_t)samples_per_frame;
    ec->samples_per_segment = (pj_uint16_t)(SEGMENT_PTIME * clock_rate / 1000);
    ec->tail_ms = (pj_uint16_t)tail_ms;
    ec->tail_samples = (pj_uint16_t)(tail_ms * clock_rate / 1000);

    ec->templ_cnt = TEMPLATE_PTIME / SEGMENT_PTIME;
    ec->tail_cnt = (pj_uint16_t)(tail_ms / SEGMENT_PTIME);
    ec->play_hist_cnt = (pj_uint16_t)(ec->tail_cnt+ec->templ_cnt);

    ec->max_calc = (pj_uint16_t)(MAX_CALC_DURATION_SEC * clock_rate / 
				 ec->samples_per_segment);

    ec->rec_hist = (pj_uint16_t*) 
		    pj_pool_alloc(pool, ec->templ_cnt *
					sizeof(ec->rec_hist[0]));

    /* Note: play history has twice number of elements */
    ec->play_hist = (pj_uint16_t*) 
		     pj_pool_alloc(pool, ec->play_hist_cnt *
					 sizeof(ec->play_hist[0]));

    ec->corr_sum = (float*)
		   pj_pool_alloc(pool, ec->tail_cnt * 
				       sizeof(ec->corr_sum[0]));
    ec->tmp_corr = (float*)
		   pj_pool_alloc(pool, ec->tail_cnt * 
				       sizeof(ec->tmp_corr[0]));
    ec->min_factor = (float*)
		     pj_pool_alloc(pool, ec->tail_cnt * 
				         sizeof(ec->min_factor[0]));
    ec->avg_factor = (float*)
		     pj_pool_alloc(pool, ec->tail_cnt * 
				         sizeof(ec->avg_factor[0]));
    ec->tmp_factor = (float*)
		     pj_pool_alloc(pool, ec->tail_cnt * 
				         sizeof(ec->tmp_factor[0]));
    echo_supp_reset(ec);

    *p_state = ec;
    return PJ_SUCCESS;
}


/*
 * Destroy. 
 */
PJ_DEF(pj_status_t) echo_supp_destroy(void *state)
{
    PJ_UNUSED_ARG(state);
    return PJ_SUCCESS;
}


/*
 * Hard reset
 */
PJ_DEF(void) echo_supp_reset(void *state)
{
    unsigned i;
    echo_supp *ec = (echo_supp*) state;

    pj_bzero(ec->rec_hist, ec->templ_cnt * sizeof(ec->rec_hist[0]));
    pj_bzero(ec->play_hist, ec->play_hist_cnt * sizeof(ec->play_hist[0]));

    for (i=0; i<ec->tail_cnt; ++i) {
	ec->corr_sum[i] = ec->avg_factor[i] = 0;
	ec->min_factor[i] = MAX_FLOAT;
    }

    ec->update_cnt = 0;
    ec->calc_cnt = 0;
    ec->learning = PJ_TRUE;
    ec->tail_index = -1;
    ec->best_corr = MAX_FLOAT;
    ec->talk_state = ST_NULL;
    ec->last_factor = 1.0;
    ec->residue = 0;
    ec->running_cnt = 0;
    ec->sum_rec_level = ec->sum_play_level0 = 0;
    ec->rec_corr = ec->play_corr0 = 0;
}

/*
 * Soft reset to force the EC to re-learn without having to discard all
 * rec and playback history.
 */
PJ_DEF(void) echo_supp_soft_reset(void *state)
{
    unsigned i;

    echo_supp *ec = (echo_supp*) state;

    for (i=0; i<ec->tail_cnt; ++i) {
	ec->corr_sum[i] = 0;
    }

    ec->update_cnt = 0;
    ec->calc_cnt = 0;
    ec->learning = PJ_TRUE;
    ec->best_corr = MAX_FLOAT;
    ec->residue = 0;
    ec->running_cnt = 0;
    ec->sum_rec_level = ec->sum_play_level0 = 0;
    ec->rec_corr = ec->play_corr0 = 0;

    PJ_LOG(4,(THIS_FILE, "Echo suppressor soft reset. Re-learning.."));
}


/* Set state */
static void echo_supp_set_state(echo_supp *ec, talk_state_t state, 
				unsigned level)
{
    PJ_UNUSED_ARG(level);

    if (state != ec->talk_state) {
	TRACE_((THIS_FILE, "[%03d.%03d] %s --> %s, level=%u", 
			   (ec->update_cnt * SEGMENT_PTIME / 1000), 
			   ((ec->update_cnt * SEGMENT_PTIME) % 1000),
			   state_names[ec->talk_state],
			   state_names[state], level));
	ec->talk_state = state;
    }
}

/*
 * Update EC state
 */
static void echo_supp_update(echo_supp *ec, pj_int16_t *rec_frm,
			     const pj_int16_t *play_frm)
{
    int prev_index;
    unsigned i, j, frm_level, sum_play_level, ulaw;
    pj_uint16_t old_rec_frm_level, old_play_frm_level;
    float play_corr;

    ++ec->update_cnt;
    if (ec->update_cnt > 0x7FFFFFFF)
	ec->update_cnt = 0x7FFFFFFF; /* Detect overflow */

    /* Calculate current play frame level */
    frm_level = pjmedia_calc_avg_signal(play_frm, ec->samples_per_segment);
    ++frm_level; /* to avoid division by zero */

    /* Save the oldest frame level for later */
    old_play_frm_level = ec->play_hist[0];

    /* Push current frame level to the back of the play history */
    pj_array_erase(ec->play_hist, sizeof(pj_uint16_t), ec->play_hist_cnt, 0);
    ec->play_hist[ec->play_hist_cnt-1] = (pj_uint16_t) frm_level;

    /* Calculate level of current mic frame */
    frm_level = pjmedia_calc_avg_signal(rec_frm, ec->samples_per_segment);
    ++frm_level; /* to avoid division by zero */

    /* Save the oldest frame level for later */
    old_rec_frm_level = ec->rec_hist[0];

    /* Push to the back of the rec history */
    pj_array_erase(ec->rec_hist, sizeof(pj_uint16_t), ec->templ_cnt, 0);
    ec->rec_hist[ec->templ_cnt-1] = (pj_uint16_t) frm_level;


    /* Can't do the calc until the play history is full. */
    if (ec->update_cnt < ec->play_hist_cnt)
	return;

    /* Skip if learning is done */
    if (!ec->learning)
	return;


    /* Calculate rec signal pattern */
    if (ec->sum_rec_level == 0) {
	/* Buffer has just been filled up, do full calculation */
	ec->rec_corr = 0;
	ec->sum_rec_level = 0;
	for (i=0; i < ec->templ_cnt-1; ++i) {
	    float corr;
	    corr = (float)ec->rec_hist[i+1] / ec->rec_hist[i];
	    ec->rec_corr += corr;
	    ec->sum_rec_level += ec->rec_hist[i];
	}
	ec->sum_rec_level += ec->rec_hist[i];
    } else {
	/* Update from previous calculation */
	ec->sum_rec_level = ec->sum_rec_level - old_rec_frm_level + 
			    ec->rec_hist[ec->templ_cnt-1];
	ec->rec_corr = ec->rec_corr - ((float)ec->rec_hist[0] / 
					      old_rec_frm_level) +
		       ((float)ec->rec_hist[ec->templ_cnt-1] /
			       ec->rec_hist[ec->templ_cnt-2]);
    }

    /* Iterate through the play history and calculate the signal correlation
     * for every tail position in the play_hist. Save the result in temporary
     * array since we may bail out early if the conversation state is not good
     * to detect echo.
     */
    /* 
     * First phase: do full calculation for the first position 
     */
    if (ec->sum_play_level0 == 0) {
	/* Buffer has just been filled up, do full calculation */
	sum_play_level = 0;
	play_corr = 0;
	for (j=0; j<ec->templ_cnt-1; ++j) {
	    float corr;
	    corr = (float)ec->play_hist[j+1] / ec->play_hist[j];
	    play_corr += corr;
	    sum_play_level += ec->play_hist[j];
	}
	sum_play_level += ec->play_hist[j];
	ec->sum_play_level0 = sum_play_level;
	ec->play_corr0 = play_corr;
    } else {
	/* Update from previous calculation */
	ec->sum_play_level0 = ec->sum_play_level0 - old_play_frm_level + 
			      ec->play_hist[ec->templ_cnt-1];
	ec->play_corr0 = ec->play_corr0 - ((float)ec->play_hist[0] / 
					          old_play_frm_level) +
		         ((float)ec->play_hist[ec->templ_cnt-1] /
			         ec->play_hist[ec->templ_cnt-2]);
	sum_play_level = ec->sum_play_level0;
	play_corr = ec->play_corr0;
    }
    ec->tmp_corr[0] = FABS(play_corr - ec->rec_corr);
    ec->tmp_factor[0] = (float)ec->sum_rec_level / sum_play_level;

    /* Bail out if remote isn't talking */
    ulaw = pjmedia_linear2ulaw(sum_play_level/ec->templ_cnt) ^ 0xFF;
    if (ulaw < MIN_SIGNAL_ULAW) {
	echo_supp_set_state(ec, ST_REM_SILENT, ulaw);
	return;
    }
    /* Bail out if local user is talking */
    if (ec->sum_rec_level >= sum_play_level) {
	echo_supp_set_state(ec, ST_LOCAL_TALK, ulaw);
	return;
    }

    /*
     * Second phase: do incremental calculation for the rest of positions
     */
    for (i=1; i < ec->tail_cnt; ++i) {
	unsigned end;

	end = i + ec->templ_cnt;

	sum_play_level = sum_play_level - ec->play_hist[i-1] +
			 ec->play_hist[end-1];
	play_corr = play_corr - ((float)ec->play_hist[i]/ec->play_hist[i-1]) +
		    ((float)ec->play_hist[end-1]/ec->play_hist[end-2]);

	/* Bail out if remote isn't talking */
	ulaw = pjmedia_linear2ulaw(sum_play_level/ec->templ_cnt) ^ 0xFF;
	if (ulaw < MIN_SIGNAL_ULAW) {
	    echo_supp_set_state(ec, ST_REM_SILENT, ulaw);
	    return;
	}

	/* Bail out if local user is talking */
	if (ec->sum_rec_level >= sum_play_level) {
	    echo_supp_set_state(ec, ST_LOCAL_TALK, ulaw);
	    return;
	}

#if 0
	// disabled: not a good idea if mic throws out loud echo
	/* Also bail out if we suspect there's a doubletalk */
	ulaw = pjmedia_linear2ulaw(ec->sum_rec_level/ec->templ_cnt) ^ 0xFF;
	if (ulaw > MIN_SIGNAL_ULAW) {
	    echo_supp_set_state(ec, ST_DOUBLETALK, ulaw);
	    return;
	}
#endif

	/* Calculate correlation and save to temporary array */
	ec->tmp_corr[i] = FABS(play_corr - ec->rec_corr);

	/* Also calculate the gain factor between mic and speaker level */
	ec->tmp_factor[i] = (float)ec->sum_rec_level / sum_play_level;
	pj_assert(ec->tmp_factor[i] < 1);
    }

    /* We seem to have good signal, we can update the EC state */
    echo_supp_set_state(ec, ST_REM_TALK, MIN_SIGNAL_ULAW);

    /* Accummulate the correlation value to the history and at the same
     * time find the tail index of the best correlation.
     */
    prev_index = ec->tail_index;
    for (i=1; i<ec->tail_cnt-1; ++i) {
	float *p = &ec->corr_sum[i], sum;

	/* Accummulate correlation value  for this tail position */
	ec->corr_sum[i] += ec->tmp_corr[i];

	/* Update the min and avg gain factor for this tail position */
	if (ec->tmp_factor[i] < ec->min_factor[i])
	    ec->min_factor[i] = ec->tmp_factor[i];
	ec->avg_factor[i] = ((ec->avg_factor[i] * ec->tail_cnt) + 
				    ec->tmp_factor[i]) /
			    (ec->tail_cnt + 1);

	/* To get the best correlation, also include the correlation
	 * value of the neighbouring tail locations.
	 */
	sum = *(p-1) + (*p)*2 + *(p+1);
	//sum = *p;

	/* See if we have better correlation value */
	if (sum < ec->best_corr) {
	    ec->tail_index = i;
	    ec->best_corr = sum;
	}
    }

    if (ec->tail_index != prev_index) {
	unsigned duration;
	int imin, iavg;

	duration = ec->update_cnt * SEGMENT_PTIME;
	imin = (int)(ec->min_factor[ec->tail_index] * 1000);
	iavg = (int)(ec->avg_factor[ec->tail_index] * 1000);

	PJ_LOG(4,(THIS_FILE, 
		  "Echo suppressor updated at t=%03d.%03ds, echo tail=%d msec"
		  ", factor min/avg=%d.%03d/%d.%03d",
		  (duration/1000), (duration%1000),
		  (ec->tail_cnt-ec->tail_index) * SEGMENT_PTIME,
		  imin/1000, imin%1000,
		  iavg/1000, iavg%1000));

    }

    ++ec->calc_cnt;

    if (ec->calc_cnt > ec->max_calc) {
	unsigned duration;
	int imin, iavg;


	ec->learning = PJ_FALSE;
	ec->running_cnt = 0;

	duration = ec->update_cnt * SEGMENT_PTIME;
	imin = (int)(ec->min_factor[ec->tail_index] * 1000);
	iavg = (int)(ec->avg_factor[ec->tail_index] * 1000);

	PJ_LOG(4,(THIS_FILE, 
	          "Echo suppressor learning done at t=%03d.%03ds, tail=%d ms"
		  ", factor min/avg=%d.%03d/%d.%03d",
		  (duration/1000), (duration%1000),
		  (ec->tail_cnt-ec->tail_index) * SEGMENT_PTIME,
		  imin/1000, imin%1000,
		  iavg/1000, iavg%1000));
    }

}


/* Amplify frame */
static void amplify_frame(pj_int16_t *frm, unsigned length, 
			  pj_ufloat_t factor)
{
    unsigned i;

    for (i=0; i<length; ++i) {
	frm[i] = (pj_int16_t)pj_ufloat_mul_i(frm[i], factor);
    }
}

/* 
 * Perform echo cancellation.
 */
PJ_DEF(pj_status_t) echo_supp_cancel_echo( void *state,
					   pj_int16_t *rec_frm,
					   const pj_int16_t *play_frm,
					   unsigned options,
					   void *reserved )
{
    unsigned i, N;
    echo_supp *ec = (echo_supp*) state;

    PJ_UNUSED_ARG(options);
    PJ_UNUSED_ARG(reserved);

    /* Calculate number of segments. This should be okay even if
     * samples_per_frame is not a multiply of samples_per_segment, since
     * we only calculate level.
     */
    N = ec->samples_per_frame / ec->samples_per_segment;
    pj_assert(N>0);
    for (i=0; i<N; ++i) {
	unsigned pos = i * ec->samples_per_segment;
	echo_supp_update(ec, rec_frm+pos, play_frm+pos);
    }

    if (ec->tail_index < 0) {
	/* Not ready */
    } else {
	unsigned lookup_cnt, rec_level=0, play_level=0;
	unsigned tail_cnt;
	float factor;

	/* How many previous segments to lookup */
	lookup_cnt = SIGNAL_LOOKUP_MSEC / SEGMENT_PTIME;
	if (lookup_cnt > ec->templ_cnt)
	    lookup_cnt = ec->templ_cnt;

	/* Lookup in recording history to get maximum mic level, to see
	 * if local user is currently talking
	 */
	for (i=ec->templ_cnt - lookup_cnt; i < ec->templ_cnt; ++i) {
	    if (ec->rec_hist[i] > rec_level)
		rec_level = ec->rec_hist[i];
	}
	rec_level = pjmedia_linear2ulaw(rec_level) ^ 0xFF;

	/* Calculate the detected tail length, in # of segments */
	tail_cnt = (ec->tail_cnt - ec->tail_index);

	/* Lookup in playback history to get max speaker level, to see
	 * if remote user is currently talking
	 */
	for (i=ec->play_hist_cnt -lookup_cnt -tail_cnt; 
	     i<ec->play_hist_cnt-tail_cnt; ++i) 
	{
	    if (ec->play_hist[i] > play_level)
		play_level = ec->play_hist[i];
	}
	play_level = pjmedia_linear2ulaw(play_level) ^ 0xFF;

	if (rec_level >= MIN_SIGNAL_ULAW) {
	    if (play_level < MIN_SIGNAL_ULAW) {
		/* Mic is talking, speaker is idle. Let mic signal pass as is.
		 */
		factor = 1.0;
		echo_supp_set_state(ec, ST_LOCAL_TALK, rec_level);
	    } else if (rec_level > play_level) {
		/* Seems that both are talking. Scale the mic signal
		 * down a little bit to reduce echo, while allowing both
		 * parties to talk at the same time.
		 */
		factor = (float)(ec->avg_factor[ec->tail_index] * 2);
		echo_supp_set_state(ec, ST_DOUBLETALK, rec_level);
	    } else {
		/* Speaker is active, but we've picked up large signal in
		 * the microphone. Assume that this is an echo, so bring 
		 * the level down to minimum too.
		 */
		factor = ec->min_factor[ec->tail_index] / 2;
		echo_supp_set_state(ec, ST_REM_TALK, play_level);
	    }
	} else {
	    if (play_level < MIN_SIGNAL_ULAW) {
		/* Both mic and speaker seems to be idle. Also scale the
		 * mic signal down with average factor to reduce low power
		 * echo.
		 */
		factor = ec->avg_factor[ec->tail_index] * 3 / 2;
		echo_supp_set_state(ec, ST_REM_SILENT, rec_level);
	    } else {
		/* Mic is idle, but there's something playing in speaker.
		 * Scale the mic down to minimum
		 */
		factor = ec->min_factor[ec->tail_index] / 2;
		echo_supp_set_state(ec, ST_REM_TALK, play_level);
	    }
	}

	/* Smoothen the transition */
	if (factor >= ec->last_factor)
	    factor = (factor + ec->last_factor) / 2;
	else
	    factor = (factor + ec->last_factor*19) / 20;

	/* Amplify frame */
	amplify_frame(rec_frm, ec->samples_per_frame, 
		      pj_ufloat_from_float(factor));
	ec->last_factor = factor;

	if (ec->talk_state == ST_REM_TALK) {
	    unsigned level, recalc_cnt;

	    /* Get the adjusted frame signal level */
	    level = pjmedia_calc_avg_signal(rec_frm, ec->samples_per_frame);
	    level = pjmedia_linear2ulaw(level) ^ 0xFF;

	    /* Accumulate average echo residue to see the ES effectiveness */
	    ec->residue = ((ec->residue * ec->running_cnt) + level) / 
			  (ec->running_cnt + 1);

	    ++ec->running_cnt;

	    /* Check if we need to re-learn */
	    recalc_cnt = CHECK_PERIOD * ec->clock_rate / ec->samples_per_frame;
	    if (ec->running_cnt > recalc_cnt) {
		int iresidue;

		iresidue = (int)(ec->residue*1000);

		PJ_LOG(5,(THIS_FILE, "Echo suppressor residue = %d.%03d",
			  iresidue/1000, iresidue%1000));

		if (ec->residue > MAX_RESIDUE && !ec->learning) {
		    echo_supp_soft_reset(ec);
		    ec->residue = 0;
		} else {
		    ec->running_cnt = 0;
		    ec->residue = 0;
		}
	    }
	}
    }

    return PJ_SUCCESS;
}

