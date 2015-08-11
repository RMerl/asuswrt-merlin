/* $Id: audiotest.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia-audiodev/audiotest.h>
#include <pjmedia-audiodev/audiodev.h>
#include <pjlib.h>
#include <pjlib-util.h>
 
#define THIS_FILE	    "audiotest.c"

/* Test duration in msec */
#define DURATION	    10000

/* Skip the first msec from the calculation */
#define SKIP_DURATION	    1000

/* Division helper */
#define DIV_ROUND_UP(a,b) (((a) + ((b) - 1)) / (b))
#define DIV_ROUND(a,b) (((a) + ((b)/2 - 1)) / (b))

struct stream_data
{
    pj_uint32_t	    first_timestamp;
    pj_uint32_t	    last_timestamp;
    pj_timestamp    last_called;
    pj_math_stat    delay;
};

struct test_data 
{
    pj_pool_t			   *pool;
    const pjmedia_aud_param	   *param;
    pjmedia_aud_test_results	   *result;
    pj_bool_t			    running;
    pj_bool_t			    has_error;
    pj_mutex_t			   *mutex;

    struct stream_data		    capture_data;
    struct stream_data		    playback_data;
};

static pj_status_t play_cb(void *user_data, pjmedia_frame *frame)
{
    struct test_data *test_data = (struct test_data *)user_data;
    struct stream_data *strm_data = &test_data->playback_data;

    pj_mutex_lock(test_data->mutex);

    /* Skip frames when test is not started or test has finished */
    if (!test_data->running) {
	pj_bzero(frame->buf, frame->size);
	pj_mutex_unlock(test_data->mutex);
	return PJ_SUCCESS;
    }

    /* Save last timestamp seen (to calculate drift) */
    strm_data->last_timestamp = frame->timestamp.u32.lo;

    if (strm_data->last_called.u64 == 0) {
	/* Init vars. */
	pj_get_timestamp(&strm_data->last_called);
	pj_math_stat_init(&strm_data->delay);
	strm_data->first_timestamp = frame->timestamp.u32.lo;
    } else {
	pj_timestamp now;
	unsigned delay;

	/* Calculate frame interval */
	pj_get_timestamp(&now);
	delay = pj_elapsed_usec(&strm_data->last_called, &now);
	strm_data->last_called = now;

	/* Update frame interval statistic */
	pj_math_stat_update(&strm_data->delay, delay);
    }

    pj_bzero(frame->buf, frame->size);

    pj_mutex_unlock(test_data->mutex);

    return PJ_SUCCESS;
}

static pj_status_t rec_cb(void *user_data, pjmedia_frame *frame)
{
    struct test_data *test_data = (struct test_data*)user_data;
    struct stream_data *strm_data = &test_data->capture_data;

    pj_mutex_lock(test_data->mutex);

    /* Skip frames when test is not started or test has finished */
    if (!test_data->running) {
	pj_mutex_unlock(test_data->mutex);
	return PJ_SUCCESS;
    }

    /* Save last timestamp seen (to calculate drift) */
    strm_data->last_timestamp = frame->timestamp.u32.lo;

    if (strm_data->last_called.u64 == 0) {
	/* Init vars. */
	pj_get_timestamp(&strm_data->last_called);
	pj_math_stat_init(&strm_data->delay);
	strm_data->first_timestamp = frame->timestamp.u32.lo;
    } else {
	pj_timestamp now;
	unsigned delay;

	/* Calculate frame interval */
	pj_get_timestamp(&now);
	delay = pj_elapsed_usec(&strm_data->last_called, &now);
	strm_data->last_called = now;

	/* Update frame interval statistic */
	pj_math_stat_update(&strm_data->delay, delay);
    }

    pj_mutex_unlock(test_data->mutex);
    return PJ_SUCCESS;
}

static void app_perror(const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));	
    printf( "%s: %s (err=%d)\n",
	    title, errmsg, status);
}


PJ_DEF(pj_status_t) pjmedia_aud_test( const pjmedia_aud_param *param,
				      pjmedia_aud_test_results *result)
{
    pj_status_t status = PJ_SUCCESS;
    pjmedia_aud_stream *strm;
    struct test_data test_data;
    unsigned ptime, tmp;
    
    /*
     * Init test parameters
     */
    pj_bzero(&test_data, sizeof(test_data));
    test_data.param = param;
    test_data.result = result;

    test_data.pool = pj_pool_create(pjmedia_aud_subsys_get_pool_factory(),
				    "audtest", 1000, 1000, NULL);
    pj_mutex_create_simple(test_data.pool, "sndtest", &test_data.mutex); 

    /*
     * Open device.
     */
    status = pjmedia_aud_stream_create(test_data.param, &rec_cb, &play_cb,
				       &test_data, &strm);
    if (status != PJ_SUCCESS) {
        app_perror("Unable to open device", status);
	pj_pool_release(test_data.pool);
        return status;
    }


    /* Sleep for a while to let sound device "settles" */
    pj_thread_sleep(200);

    /*
     * Start the stream.
     */
    status = pjmedia_aud_stream_start(strm);
    if (status != PJ_SUCCESS) {
        app_perror("Unable to start capture stream", status);
	pjmedia_aud_stream_destroy(strm);
	pj_pool_release(test_data.pool);
        return status;
    }

    PJ_LOG(3,(THIS_FILE,
	      " Please wait while test is in progress (~%d secs)..",
	      (DURATION+SKIP_DURATION)/1000));

    /* Let the stream runs for few msec/sec to get stable result.
     * (capture normally begins with frames available simultaneously).
     */
    pj_thread_sleep(SKIP_DURATION);


    /* Begin gather data */
    test_data.running = 1;

    /* 
     * Let the test runs for a while.
     */
    pj_thread_sleep(DURATION);


    /*
     * Close stream.
     */
    test_data.running = 0;
    pjmedia_aud_stream_destroy(strm);
    pj_pool_release(test_data.pool);


    /* 
     * Gather results
     */
    ptime = param->samples_per_frame * 1000 / param->clock_rate;

    tmp = pj_math_stat_get_stddev(&test_data.capture_data.delay);
    result->rec.frame_cnt = test_data.capture_data.delay.n;
    result->rec.min_interval = DIV_ROUND(test_data.capture_data.delay.min, 1000);
    result->rec.max_interval = DIV_ROUND(test_data.capture_data.delay.max, 1000);
    result->rec.avg_interval = DIV_ROUND(test_data.capture_data.delay.mean, 1000);
    result->rec.dev_interval = DIV_ROUND(tmp, 1000);
    result->rec.max_burst    = DIV_ROUND_UP(result->rec.max_interval, ptime);

    tmp = pj_math_stat_get_stddev(&test_data.playback_data.delay);
    result->play.frame_cnt = test_data.playback_data.delay.n;
    result->play.min_interval = DIV_ROUND(test_data.playback_data.delay.min, 1000);
    result->play.max_interval = DIV_ROUND(test_data.playback_data.delay.max, 1000);
    result->play.avg_interval = DIV_ROUND(test_data.playback_data.delay.mean, 1000);
    result->play.dev_interval = DIV_ROUND(tmp, 1000);
    result->play.max_burst    = DIV_ROUND_UP(result->play.max_interval, ptime);

    /* Check drifting */
    if (param->dir == PJMEDIA_DIR_CAPTURE_PLAYBACK) {
	int play_diff, cap_diff, drift;

	play_diff = test_data.playback_data.last_timestamp -
		    test_data.playback_data.first_timestamp;
	cap_diff  = test_data.capture_data.last_timestamp -
		    test_data.capture_data.first_timestamp;
	drift = play_diff > cap_diff? play_diff - cap_diff :
		cap_diff - play_diff;

	/* Allow one frame tolerance for clock drift detection */
	if (drift < (int)param->samples_per_frame) {
	    result->rec_drift_per_sec = 0;
	} else {
	    unsigned msec_dur;

	    msec_dur = (test_data.capture_data.last_timestamp - 
		       test_data.capture_data.first_timestamp) * 1000 /
		       test_data.param->clock_rate;

	    result->rec_drift_per_sec = drift * 1000 / msec_dur;

	}
    }

    return test_data.has_error? PJ_EUNKNOWN : PJ_SUCCESS;
}

