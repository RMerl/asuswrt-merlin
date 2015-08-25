/* $Id: systest.h 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
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
#ifndef __SYSTEST_H__
#define __SYSTEST_H__

#include <pjlib.h>

/*
 * Overrideable parameters
 */
#define REC_DEV_ID			systest_cap_dev_id
#define PLAY_DEV_ID			systest_play_dev_id
//#define REC_DEV_ID			5
//#define PLAY_DEV_ID			5
#define OVERRIDE_AUDDEV_REC_LAT		0
#define OVERRIDE_AUDDEV_PLAY_LAT	0
#define OVERRIDE_AUD_FRAME_PTIME	0

/* Don't change this */
#define CHANNEL_COUNT			1

/* If you change CLOCK_RATE then the input WAV files need to be
 * changed, so normally don't need to change this.
 */
#define TEST_CLOCK_RATE			8000

/* You may change sound device's clock rate as long as resampling
 * is enabled.
 */
#define DEV_CLOCK_RATE			8000


#if defined(PJ_WIN32_WINCE) && PJ_WIN32_WINCE
    #define LOG_OUT_PATH		"\\PJSYSTEST.TXT"
    #define RESULT_OUT_PATH		"\\PJSYSTEST_RESULT.TXT"
    #define WAV_PLAYBACK_PATH		"\\Program Files\\pjsystest\\input.8.wav"
    #define WAV_REC_OUT_PATH		"\\PJSYSTEST_TESTREC.WAV"
    #define WAV_TOCK8_PATH		"\\Program Files\\pjsystest\\tock8.WAV"
    #define WAV_LATENCY_OUT_PATH	"\\PJSYSTEST_LATREC.WAV"
    #define ALT_PATH1			""
    #define AEC_REC_PATH		"\\PJSYSTEST_AECREC.WAV"
#else
    #define LOG_OUT_PATH		"PJSYSTEST.TXT"
    #define RESULT_OUT_PATH		"PJSYSTEST_RESULT.TXT"
    #define WAV_PLAYBACK_PATH		"input.8.wav"
    #define WAV_REC_OUT_PATH		"PJSYSTEST_TESTREC.WAV"
    #define WAV_TOCK8_PATH		"tock8.wav"
    #define WAV_LATENCY_OUT_PATH	"PJSYSTEST_LATREC.WAV"
    #define ALT_PATH1			"../../tests/pjsua/wavs/"
    #define AEC_REC_PATH		"PJSYSTEST_AECREC.WAV"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* API, to be called by main() */
int	    systest_init(void);
int	    systest_set_dev(int cap_dev, int play_dev);
int	    systest_run(void);
void	    systest_save_result(const char *filename);
void	    systest_deinit(void);

/* Device ID to test */
extern int systest_cap_dev_id;
extern int systest_play_dev_id;

/* Test item is used to record the test result */
typedef struct test_item_t
{
    char	title[80];
    pj_bool_t	skipped;
    pj_bool_t	success;
    char	reason[1024];
} test_item_t;

#define SYSTEST_MAX_TEST    32
extern unsigned	    test_item_count;
extern test_item_t  test_items[SYSTEST_MAX_TEST];
#define PATH_LENGTH	    PJ_MAXPATH
extern char	    doc_path[PATH_LENGTH];
extern char	    res_path[PATH_LENGTH];

test_item_t *systest_alloc_test_item(const char *title);

#ifdef __cplusplus
}
#endif

#endif	/* __SYSTEST_H__ */
