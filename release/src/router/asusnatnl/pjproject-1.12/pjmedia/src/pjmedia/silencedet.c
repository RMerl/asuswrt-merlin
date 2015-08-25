/* $Id: silencedet.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia/silencedet.h>
#include <pjmedia/alaw_ulaw.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>

#define THIS_FILE   "silencedet.c"

#if 1
#   define TRACE_(x)	PJ_LOG(5,x)
#else
#   define TRACE_(x)
#endif

/**
 * This enumeration specifies operation mode of silence detector 
 */
typedef enum pjmedia_silence_det_mode {
    VAD_MODE_NONE,
    VAD_MODE_FIXED,
    VAD_MODE_ADAPTIVE
} pjmedia_silence_det_mode;

/**
 * Default settings
 */
#define DEF_RECALC_ON_VOICED	    4000 /* Time to recalculate threshold
					    in voiced condition, in ms	  */
#define DEF_RECALC_ON_SILENCE	    2000 /* Time to recalculate threshold
					    in silence condition, in ms.  */
#define DEF_BEFORE_SILENCE	    400	 /* Silence time before really changing
					    state into SILENCE, in ms.	  */
#define DEF_THRESHOLD		    1000 /* Default threshold.		  */

/**
 * This enumeration specifies the states of the silence detector.
 */
enum pjmedia_silence_det_state {
    STATE_SILENCE,
    STATE_START_SILENCE,
    STATE_VOICED
};

/**
 * This structure holds the silence detector state.
 */
struct pjmedia_silence_det
{
    char      objname[PJ_MAX_OBJ_NAME]; /**< VAD name.			    */

    int	      mode;			/**< VAD mode.			    */
    unsigned  ptime;			/**< Frame time, in msec.	    */

    unsigned  threshold;		/**< Current threshold level.	    */
    unsigned  sum_level;		/**< Total sum of recent level.	    */
    unsigned  sum_cnt;			/**< Number of level summed.	    */
    unsigned  silence_timer;		/**< Silence condition timer.	    */
    unsigned  voiced_timer;		/**< Voiced condition timer.	    */
    
    enum pjmedia_silence_det_state state;/**< Silence detector state.	    */
    unsigned  recalc_on_voiced;		/**< Setting of time to recalc 
					     threshold in voiced condition. */
    unsigned  recalc_on_silence;	/**< Setting of time to recalc 
					     threshold in silence condition.*/
    unsigned  before_silence;		/**< Setting of silence time before 
					     really changing state into SILENCE,
					     in ms.			    */
};



PJ_DEF(pj_status_t) pjmedia_silence_det_create( pj_pool_t *pool,
						unsigned clock_rate,
						unsigned samples_per_frame,
						pjmedia_silence_det **p_sd)
{
    pjmedia_silence_det *sd;

    PJ_ASSERT_RETURN(pool && p_sd, PJ_EINVAL);

    sd = PJ_POOL_ZALLOC_T(pool, pjmedia_silence_det);

    pj_ansi_snprintf(sd->objname, PJ_MAX_OBJ_NAME, "sd%p", sd);
    sd->objname[PJ_MAX_OBJ_NAME-1] = '\0';

    sd->ptime = samples_per_frame * 1000 / clock_rate;
     
    /* Default settings */
    pjmedia_silence_det_set_params(sd, -1, -1, -1);

    /* Restart in adaptive, silent mode */
    pjmedia_silence_det_set_adaptive( sd, -1 );

    *p_sd = sd;
    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_silence_det_set_name( pjmedia_silence_det *sd,
						  const char *name)
{
    PJ_ASSERT_RETURN(sd && name, PJ_EINVAL);

    pj_ansi_snprintf(sd->objname, PJ_MAX_OBJ_NAME, name, sd);
    sd->objname[PJ_MAX_OBJ_NAME-1] = '\0';
    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_silence_det_set_adaptive(pjmedia_silence_det *sd,
						     int threshold)
{
    PJ_ASSERT_RETURN(sd, PJ_EINVAL);

    if (threshold < 0)
	threshold = DEF_THRESHOLD;

    sd->mode = VAD_MODE_ADAPTIVE;
    sd->threshold = threshold;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_silence_det_set_fixed( pjmedia_silence_det *sd,
						   int threshold )
{
    PJ_ASSERT_RETURN(sd, PJ_EINVAL);

    if (threshold < 0)
	threshold = DEF_THRESHOLD;

    sd->mode = VAD_MODE_FIXED;
    sd->threshold = threshold;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjmedia_silence_det_set_params( pjmedia_silence_det *sd,
						    int before_silence,
						    int recalc_time1,
						    int recalc_time2)
{
    PJ_ASSERT_RETURN(sd, PJ_EINVAL);

    if (recalc_time1 < 0)
	recalc_time1 = DEF_RECALC_ON_VOICED;
    if (recalc_time2 < 0)
	recalc_time2 = DEF_RECALC_ON_SILENCE;
    if (before_silence < 0)
	before_silence = DEF_BEFORE_SILENCE;

    sd->recalc_on_voiced = recalc_time1;
    sd->recalc_on_silence = recalc_time2;
    sd->before_silence  = before_silence;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_silence_det_disable( pjmedia_silence_det *sd )
{
    PJ_ASSERT_RETURN(sd, PJ_EINVAL);

    sd->mode = VAD_MODE_NONE;

    return PJ_SUCCESS;
}


PJ_DEF(pj_int32_t) pjmedia_calc_avg_signal( const pj_int16_t samples[],
					    pj_size_t count)
{
    pj_uint32_t sum = 0;
    
    const pj_int16_t * pcm = samples;
    const pj_int16_t * end = samples + count;

    if (count==0)
	return 0;

    while (pcm != end) {
	if (*pcm < 0)
	    sum -= *pcm++;
	else
	    sum += *pcm++;
    }
    
    return (pj_int32_t)(sum / count);
}

PJ_DEF(pj_bool_t) pjmedia_silence_det_apply( pjmedia_silence_det *sd,
					     pj_uint32_t level)
{
    int avg_recent_level;

    if (sd->mode == VAD_MODE_NONE)
	return PJ_FALSE;

    if (sd->mode == VAD_MODE_FIXED)
	return (level < sd->threshold);

    /* Calculating recent level */
    sd->sum_level += level;
    ++sd->sum_cnt;
    avg_recent_level = (sd->sum_level / sd->sum_cnt);

    if (level > sd->threshold || 
	level >= PJMEDIA_SILENCE_DET_MAX_THRESHOLD)
    {
	sd->silence_timer = 0;
	sd->voiced_timer += sd->ptime;

	switch(sd->state) {
	    case STATE_VOICED:
		if (sd->voiced_timer > sd->recalc_on_voiced) {
		    /* Voiced for long time (>recalc_on_voiced), current 
		     * threshold seems to be too low.
		     */
		    sd->threshold = (avg_recent_level + sd->threshold) >> 1;
		    TRACE_((THIS_FILE,"Re-adjust threshold (in talk burst)"
			    "to %d", sd->threshold));

		    sd->voiced_timer = 0;

		    /* Reset sig_level */
		    sd->sum_level = avg_recent_level;
		    sd->sum_cnt = 1;
		}
		break;

	    case STATE_SILENCE:
		TRACE_((THIS_FILE,"Starting talk burst (level=%d threshold=%d)",
			level, sd->threshold));

	    case STATE_START_SILENCE:
		sd->state = STATE_VOICED;

		/* Reset sig_level */
		sd->sum_level = level;
		sd->sum_cnt = 1;

		break;

	    default:
		pj_assert(0);
		break;
	}
    } else {
	sd->voiced_timer = 0;
	sd->silence_timer += sd->ptime;

	switch(sd->state) {
	    case STATE_SILENCE:
		if (sd->silence_timer >= sd->recalc_on_silence) {
		    sd->threshold = avg_recent_level << 1;
		    TRACE_((THIS_FILE,"Re-adjust threshold (in silence)"
			    "to %d", sd->threshold));

		    sd->silence_timer = 0;

		    /* Reset sig_level */
		    sd->sum_level = avg_recent_level;
		    sd->sum_cnt = 1;
		}
		break;

	    case STATE_VOICED:
		sd->state = STATE_START_SILENCE;

		/* Reset sig_level */
		sd->sum_level = level;
		sd->sum_cnt = 1;

	    case STATE_START_SILENCE:
		if (sd->silence_timer >= sd->before_silence) {
		    sd->state = STATE_SILENCE;
		    sd->threshold = avg_recent_level << 1;
		    TRACE_((THIS_FILE,"Starting silence (level=%d "
			    "threshold=%d)", level, sd->threshold));

		    /* Reset sig_level */
		    sd->sum_level = avg_recent_level;
		    sd->sum_cnt = 1;
		}
		break;

	    default:
		pj_assert(0);
		break;
	}
    }

    return (sd->state == STATE_SILENCE);
}


PJ_DEF(pj_bool_t) pjmedia_silence_det_detect( pjmedia_silence_det *sd,
					      const pj_int16_t samples[],
					      pj_size_t count,
					      pj_int32_t *p_level)
{
    pj_uint32_t level;
    
    /* Calculate average signal level. */
    level = pjmedia_calc_avg_signal(samples, count);
    
    /* Report to caller, if required. */
    if (p_level)
	*p_level = level;

    return pjmedia_silence_det_apply(sd, level);
}

