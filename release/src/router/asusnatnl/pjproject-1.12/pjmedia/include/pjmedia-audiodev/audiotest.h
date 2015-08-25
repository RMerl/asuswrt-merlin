/* $Id: audiotest.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_AUDIODEV_AUDIOTEST_H__
#define __PJMEDIA_AUDIODEV_AUDIOTEST_H__

/**
 * @file audiotest.h
 * @brief Audio test utility.
 */
#include <pjmedia-audiodev/audiodev.h>


PJ_BEGIN_DECL

/**
 * @defgroup s30_audio_test_utility Audio tests utility.
 * @ingroup audio_device_api
 * @brief Audio test utility.
 * @{
 */

/**
 * Statistic for each direction.
 */
typedef struct pjmedia_aud_test_stat
{
    /**
     * Number of frames processed during the test.
     */
    unsigned frame_cnt;

    /** 
     * Minimum inter-frame arrival time, in milliseconds 
     */
    unsigned min_interval;

    /** 
     * Maximum inter-frame arrival time, in milliseconds 
     */
    unsigned max_interval;

    /** 
     * Average inter-frame arrival time, in milliseconds 
     */
    unsigned avg_interval;

    /** 
     * Standard deviation of inter-frame arrival time, in milliseconds 
     */
    unsigned dev_interval;

    /** 
     * Maximum number of frame burst 
     */
    unsigned max_burst;

} pjmedia_aud_test_stat;


/**
 * Test results.
 */
typedef struct pjmedia_aud_test_results
{
    /**
     * Recording statistic.
     */
    pjmedia_aud_test_stat rec;

    /**
     * Playback statistic.
     */
    pjmedia_aud_test_stat play;

    /** 
     * Clock drifts per second, in samples. Positive number indicates rec
     * device is running faster than playback device.
     */
    pj_int32_t rec_drift_per_sec;

} pjmedia_aud_test_results;


/**
 * Perform audio device testing.
 */
PJ_DECL(pj_status_t) pjmedia_aud_test(const pjmedia_aud_param *param,
				      pjmedia_aud_test_results *result);

/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJMEDIA_AUDIODEV_AUDIOTEST_H__ */


