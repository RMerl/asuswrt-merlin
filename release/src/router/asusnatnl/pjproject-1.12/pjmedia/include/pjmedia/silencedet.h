/* $Id: silencedet.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_SILENCE_DET_H__
#define __PJMEDIA_SILENCE_DET_H__


/**
 * @file silencedet.h
 * @brief Adaptive silence detector.
 */
#include <pjmedia/types.h>


/**
 * @defgroup PJMEDIA_SILENCEDET Adaptive Silence Detection
 * @ingroup PJMEDIA_FRAME_OP
 * @brief Adaptive Silence Detector
 * @{
 */


PJ_BEGIN_DECL


/**
 * Opaque declaration for silence detector.
 */
typedef struct pjmedia_silence_det pjmedia_silence_det;



/**
 * Create voice activity detector with default settings. The default settings
 * are set to adaptive silence detection with the default threshold.
 *
 * @param pool		    Pool for allocating the structure.
 * @param clock_rate	    Clock rate.
 * @param samples_per_frame Number of samples per frame. The clock_rate and
 *			    samples_per_frame is only used to calculate the
 *			    frame time, from which some timing parameters
 *			    are calculated from.
 * @param p_sd		    Pointer to receive the silence detector instance.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_silence_det_create( pj_pool_t *pool,
						 unsigned clock_rate,
						 unsigned samples_per_frame,
						 pjmedia_silence_det **p_sd );


/**
 * Set silence detector name to identify the particular silence detector
 * instance in the log.
 *
 * @param sd		    The silence detector.
 * @param name		    Name.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_silence_det_set_name(pjmedia_silence_det *sd,
						  const char *name);


/**
 * Set the sd to operate in fixed threshold mode. With fixed threshold mode,
 * the threshold will not be changed adaptively.
 *
 * @param sd		    The silence detector
 * @param threshold	    The silence threshold, or -1 to use default
 *			    threshold.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_silence_det_set_fixed( pjmedia_silence_det *sd,
						    int threshold );

/**
 * Set the sd to operate in adaptive mode. This is the default mode
 * when the silence detector is created.
 *
 * @param sd		    The silence detector
 * @param threshold	    Initial threshold to be set, or -1 to use default
 *			    threshold.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_silence_det_set_adaptive(pjmedia_silence_det *sd,
						      int threshold);

/**
 * Set other silence detector parameters.
 *
 * @param sd		    The silence detector
 * @param before_silence    Minimum duration of silence (in msec) before 
 *			    silence is reported. If -1 is specified, then
 *			    the default value will be used. The default is
 *			    400 msec.
 * @param recalc_time1	    The interval (in msec) to recalculate threshold
 *			    in non-silence condition when adaptive silence 
 *			    detection is set. If -1 is specified, then the 
 *			    default value will be used. The default is 4000
 *			    (msec).
 * @param recalc_time2	    The interval (in msec) to recalculate threshold
 *			    in silence condition when adaptive silence detection
 *			    is set. If -1 is specified, then the default value 
 *			    will be used. The default value is 2000 (msec).
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_silence_det_set_params( pjmedia_silence_det *sd,
						     int before_silence,
						     int recalc_time1,
						     int recalc_time2);


/**
 * Disable the silence detector.
 *
 * @param sd		The silence detector
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_silence_det_disable( pjmedia_silence_det *sd );


/**
 * Perform voice activity detection on the given input samples. This
 * function uses #pjmedia_calc_avg_signal() and #pjmedia_silence_det_apply()
 * for its calculation.
 *
 * @param sd		The silence detector instance.
 * @param samples	Pointer to 16-bit PCM input samples.
 * @param count		Number of samples in the input.
 * @param p_level	Optional pointer to receive average signal level
 *			of the input samples.
 *
 * @return		Non zero if signal is silence.
 */
PJ_DECL(pj_bool_t) pjmedia_silence_det_detect( pjmedia_silence_det *sd,
					       const pj_int16_t samples[],
					       pj_size_t count,
					       pj_int32_t *p_level);


/**
 * Calculate average signal level for the given samples.
 *
 * @param samples	Pointer to 16-bit PCM samples.
 * @param count		Number of samples in the input.
 *
 * @return		The average signal level, which simply is total level
 *			divided by number of samples.
 */
PJ_DECL(pj_int32_t) pjmedia_calc_avg_signal( const pj_int16_t samples[],
					     pj_size_t count );



/**
 * Perform voice activity detection, given the specified average signal
 * level.
 *
 * @param sd		The silence detector instance.
 * @param level		Signal level.
 *
 * @return		Non zero if signal is silence.
 */
PJ_DECL(pj_bool_t) pjmedia_silence_det_apply( pjmedia_silence_det *sd,
					      pj_uint32_t level);



PJ_END_DECL


/**
 * @}
 */


#endif	/* __PJMEDIA_SILENCE_DET_H__ */

