/* $Id: systest.c 4074 2012-04-24 07:38:41Z ming $ */
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
#include "systest.h"
#include "gui.h"

#define THIS_FILE "systest.c"

unsigned    test_item_count;
test_item_t test_items[SYSTEST_MAX_TEST];
char	    doc_path[PATH_LENGTH] = {0};
char	    res_path[PATH_LENGTH] = {0};
char	    fpath[PATH_LENGTH];

#define USER_ERROR  "User used said not okay"

static void systest_wizard(void);
static void systest_list_audio_devs(void);
static void systest_display_settings(void);
static void systest_play_tone(void);
static void systest_play_wav1(void);
static void systest_play_wav2(void);
static void systest_rec_audio(void);
static void systest_audio_test(void);
static void systest_latency_test(void);
static void systest_aec_test(void);
static void exit_app(void);

/* Menus */
static gui_menu menu_exit = { "Exit", &exit_app };

static gui_menu menu_wizard =  { "Run test wizard", &systest_wizard };
static gui_menu menu_playtn = { "Play Tone", &systest_play_tone };
static gui_menu menu_playwv1 = { "Play WAV File1", &systest_play_wav1 };
static gui_menu menu_playwv2 = { "Play WAV File2", &systest_play_wav2 };
static gui_menu menu_recaud  = { "Record Audio", &systest_rec_audio };
static gui_menu menu_audtest = { "Device Test", &systest_audio_test };
static gui_menu menu_calclat = { "Latency Test", &systest_latency_test };
static gui_menu menu_sndaec = { "AEC/AES Test", &systest_aec_test };

static gui_menu menu_listdev = { "View Devices", &systest_list_audio_devs };
static gui_menu menu_getsets = { "View Settings", &systest_display_settings };

static gui_menu menu_tests = { 
    "Tests", NULL, 
    10, 
    {
	&menu_wizard,
	&menu_audtest, 
	&menu_playtn,
	&menu_playwv1,
	&menu_playwv2,
	&menu_recaud,
	&menu_calclat,
	&menu_sndaec,
	NULL, 
	&menu_exit
    }
};

static gui_menu menu_options = { 
    "Options", NULL, 
    2, 
    {
	&menu_listdev,
	&menu_getsets,
    }
};

static gui_menu root_menu = { 
    "Root", NULL, 2, {&menu_tests, &menu_options} 
};

/*****************************************************************/

PJ_INLINE(char *) add_path(const char *path, const char *fname)
{
    strncpy(fpath, path, PATH_LENGTH);
    strncat(fpath, fname, PATH_LENGTH);
    return fpath;
}

static void exit_app(void)
{
    systest_save_result(add_path(doc_path, RESULT_OUT_PATH));
    gui_destroy();
}


#include <pjsua-lib/pjsua.h>
#include <pjmedia_audiodev.h>

typedef struct systest_t
{
    pjsua_config	    ua_cfg;
    pjsua_media_config	    media_cfg;
    pjmedia_aud_dev_index   rec_id;
    pjmedia_aud_dev_index   play_id;
} systest_t;

static systest_t systest;
static char textbuf[600];

/* Device ID to test */
int systest_cap_dev_id = PJMEDIA_AUD_DEFAULT_CAPTURE_DEV;
int systest_play_dev_id = PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV;

static void systest_perror(const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    char themsg[PJ_ERR_MSG_SIZE + 100];

    if (status != PJ_SUCCESS)
	pj_strerror(status, errmsg, sizeof(errmsg));
    else
	errmsg[0] = '\0';
    
    strcpy(themsg, title);
    strncat(themsg, errmsg, sizeof(themsg)-1);
    themsg[sizeof(themsg)-1] = '\0';

    gui_msgbox("Error", themsg, WITH_OK);
}

test_item_t *systest_alloc_test_item(const char *title)
{
    test_item_t *ti;

    if (test_item_count == SYSTEST_MAX_TEST) {
	gui_msgbox("Error", "You have done too many tests", WITH_OK);
	return NULL;
    }

    ti = &test_items[test_item_count++];
    pj_bzero(ti, sizeof(*ti));
    pj_ansi_strcpy(ti->title, title);

    return ti;
}

/*****************************************************************************
 * test: play simple ringback tone and hear it 
 */
static void systest_play_tone(void)
{
    /* Ringtones  */
    #define RINGBACK_FREQ1	    440	    /* 400 */
    #define RINGBACK_FREQ2	    480	    /* 450 */
    #define RINGBACK_ON		    3000    /* 400 */
    #define RINGBACK_OFF	    4000    /* 200 */
    #define RINGBACK_CNT	    1	    /* 2   */
    #define RINGBACK_INTERVAL	    4000    /* 2000 */

    unsigned i, samples_per_frame;
    pjmedia_tone_desc tone[RINGBACK_CNT];
    pj_pool_t *pool = NULL;
    pjmedia_port *ringback_port = NULL;
    enum gui_key key;
    int ringback_slot = -1;
    test_item_t *ti;
    pj_str_t name;
    const char *title = "Audio Tone Playback Test";
    pj_status_t status;

    ti = systest_alloc_test_item(title);
    if (!ti)
	return;

    key = gui_msgbox(title,
		     "This test will play simple ringback tone to "
		     "the speaker. Please listen carefully for audio "
		     "impairments such as stutter. You may need "
		     "to let this test running for a while to "
		     "make sure that everything is okay. Press "
		     "OK to start, CANCEL to skip",
		     WITH_OKCANCEL);
    if (key != KEY_OK) {
	ti->skipped = PJ_TRUE;
	return;
    }

    PJ_LOG(3,(THIS_FILE, "Running %s", title));

    pool = pjsua_pool_create(0, "ringback", 512, 512);
    samples_per_frame = systest.media_cfg.audio_frame_ptime * 
			systest.media_cfg.clock_rate *
			systest.media_cfg.channel_count / 1000;

    /* Ringback tone (call is ringing) */
    name = pj_str("ringback");
    status = pjmedia_tonegen_create2(pool, &name, 
				     systest.media_cfg.clock_rate,
				     systest.media_cfg.channel_count, 
				     samples_per_frame,
				     16, PJMEDIA_TONEGEN_LOOP, 
				     &ringback_port);
    if (status != PJ_SUCCESS)
	goto on_return;

    pj_bzero(&tone, sizeof(tone));
    for (i=0; i<RINGBACK_CNT; ++i) {
	tone[i].freq1 = RINGBACK_FREQ1;
	tone[i].freq2 = RINGBACK_FREQ2;
	tone[i].on_msec = RINGBACK_ON;
	tone[i].off_msec = RINGBACK_OFF;
    }
    tone[RINGBACK_CNT-1].off_msec = RINGBACK_INTERVAL;

    status = pjmedia_tonegen_play(ringback_port, RINGBACK_CNT, tone,
				  PJMEDIA_TONEGEN_LOOP);
    if (status != PJ_SUCCESS)
	goto on_return;

    status = pjsua_conf_add_port(0, pool, ringback_port, &ringback_slot);
    if (status != PJ_SUCCESS)
	goto on_return;

    status = pjsua_conf_connect(0, ringback_slot, 0);
    if (status != PJ_SUCCESS)
	goto on_return;

    key = gui_msgbox(title,
		     "Ringback tone should be playing now in the "
		     "speaker. Press OK to stop. ", WITH_OK);

    status = PJ_SUCCESS;

on_return:
    if (ringback_slot != -1)
	pjsua_conf_remove_port(0, ringback_slot);
    if (ringback_port)
	pjmedia_port_destroy(ringback_port);
    if (pool)
	pj_pool_release(pool);

    if (status != PJ_SUCCESS) {
	systest_perror("Sorry we encounter error when initializing "
		       "the tone generator: ", status);
	ti->success = PJ_FALSE;
	pj_strerror(status, ti->reason, sizeof(ti->reason));
    } else {
	key = gui_msgbox(title, "Is the audio okay?", WITH_YESNO);
	ti->success = (key == KEY_YES);
	if (!ti->success)
	    pj_ansi_strcpy(ti->reason, USER_ERROR);
    }
    return;
}

/* Util: create file player, each time trying different paths until we get
 * the file.
 */
static pj_status_t create_player(unsigned path_cnt, const char *paths[], 
				 pjsua_player_id *p_id)
{
    pj_str_t name;
    pj_status_t status = PJ_ENOTFOUND;
    unsigned i;

    for (i=0; i<path_cnt; ++i) {
	status = pjsua_player_create(0, pj_cstr(&name, paths[i]), 0, p_id);
	if (status == PJ_SUCCESS)
	    return PJ_SUCCESS;
    }

	PJ_LOG(4, (THIS_FILE, "create_player() player not found."));
    return status;
}

/*****************************************************************************
 * test: play WAV file
 */
static void systest_play_wav(unsigned path_cnt, const char *paths[])
{
    pjsua_player_id play_id = PJSUA_INVALID_ID;
    enum gui_key key;
    test_item_t *ti;
    const char *title = "WAV File Playback Test";
    pj_status_t status;

    ti = systest_alloc_test_item(title);
    if (!ti)
	return;

    pj_ansi_snprintf(textbuf, sizeof(textbuf),
		     "This test will play %s file to "
		     "the speaker. Please listen carefully for audio "
		     "impairments such as stutter. Let this test run "
		     "for a while to make sure that everything is okay."
		     " Press OK to start, CANCEL to skip",
		     paths[0]);

    key = gui_msgbox(title, textbuf,
		     WITH_OKCANCEL);
    if (key != KEY_OK) {
	ti->skipped = PJ_TRUE;
	return;
    }

    PJ_LOG(3,(THIS_FILE, "Running %s", title));

    /* WAV port */
    status = create_player(path_cnt, paths, &play_id);
    if (status != PJ_SUCCESS)
	goto on_return;

    status = pjsua_conf_connect(0, pjsua_player_get_conf_port(0, play_id), 0);
    if (status != PJ_SUCCESS)
	goto on_return;

    key = gui_msgbox(title,
		     "WAV file should be playing now in the "
		     "speaker. Press OK to stop. ", WITH_OK);

    status = PJ_SUCCESS;

on_return:
    if (play_id != -1)
	pjsua_player_destroy(0, play_id);

    if (status != PJ_SUCCESS) {
	systest_perror("Sorry we've encountered error", status);
	ti->success = PJ_FALSE;
	pj_strerror(status, ti->reason, sizeof(ti->reason));
    } else {
	key = gui_msgbox(title, "Is the audio okay?", WITH_YESNO);
	ti->success = (key == KEY_YES);
	if (!ti->success)
	    pj_ansi_strcpy(ti->reason, USER_ERROR);
    }
    return;
}

static void systest_play_wav1(void)
{
    const char *paths[] = { add_path(res_path, WAV_PLAYBACK_PATH),
			    ALT_PATH1 WAV_PLAYBACK_PATH };
    systest_play_wav(PJ_ARRAY_SIZE(paths), paths);
}

static void systest_play_wav2(void)
{
    const char *paths[] = { add_path(res_path, WAV_TOCK8_PATH),
			    ALT_PATH1 WAV_TOCK8_PATH};
    systest_play_wav(PJ_ARRAY_SIZE(paths), paths);
}


/*****************************************************************************
 * test: record audio 
 */
static void systest_rec_audio(void)
{
    const pj_str_t filename = pj_str(add_path(doc_path, WAV_REC_OUT_PATH));
    pj_pool_t *pool = NULL;
    enum gui_key key;
    pjsua_recorder_id rec_id = PJSUA_INVALID_ID;
    pjsua_player_id play_id = PJSUA_INVALID_ID;
    pjsua_conf_port_id rec_slot = PJSUA_INVALID_ID;
    pjsua_conf_port_id play_slot = PJSUA_INVALID_ID;
    pj_status_t status = PJ_SUCCESS;
    const char *title = "Audio Recording";
    test_item_t *ti;

    ti = systest_alloc_test_item(title);
    if (!ti)
	return;

    key = gui_msgbox(title,
		     "This test will allow you to record audio "
		     "from the microphone, and playback the "
		     "audio to the speaker. Press OK to start recording, "
		     "CANCEL to skip.", 
		     WITH_OKCANCEL);
    if (key != KEY_OK) {
	ti->skipped = PJ_TRUE;
	return;
    }

    PJ_LOG(3,(THIS_FILE, "Running %s", title));

    pool = pjsua_pool_create(0, "rectest", 512, 512);

    status = pjsua_recorder_create(0, &filename, 0, NULL, -1, 0, &rec_id);
    if (status != PJ_SUCCESS)
	goto on_return;

    rec_slot = pjsua_recorder_get_conf_port(0, rec_id);

    status = pjsua_conf_connect(0, 0, rec_slot);
    if (status != PJ_SUCCESS)
	goto on_return;

    key = gui_msgbox(title,
		     "Recording is in progress now, please say "
		     "something in the microphone. Press OK "
		     "to stop recording", WITH_OK);

    pjsua_conf_disconnect(0, 0, rec_slot);
    rec_slot = PJSUA_INVALID_ID;
    pjsua_recorder_destroy(0, rec_id);
    rec_id = PJSUA_INVALID_ID;

    status = pjsua_player_create(0, &filename, 0, &play_id);
    if (status != PJ_SUCCESS)
	goto on_return;

    play_slot = pjsua_player_get_conf_port(0, play_id);

    status = pjsua_conf_connect(0, play_slot, 0);
    if (status != PJ_SUCCESS)
	goto on_return;

    key = gui_msgbox(title,
		     "Recording has been stopped. "
		     "The recorded audio is being played now to "
		     "the speaker device, in a loop. Listen for "
		     "any audio impairments. Press OK to stop.", 
		     WITH_OK);

on_return:
    if (rec_slot != PJSUA_INVALID_ID)
	pjsua_conf_disconnect(0, 0, rec_slot);
    if (rec_id != PJSUA_INVALID_ID)
	pjsua_recorder_destroy(0, rec_id);
    if (play_slot != PJSUA_INVALID_ID)
	pjsua_conf_disconnect(0, play_slot, 0);
    if (play_id != PJSUA_INVALID_ID)
	pjsua_player_destroy(0, play_id);
    if (pool)
	pj_pool_release(pool);

    if (status != PJ_SUCCESS) {
	systest_perror("Sorry we encountered an error: ", status);
	ti->success = PJ_FALSE;
	pj_strerror(status, ti->reason, sizeof(ti->reason));
    } else {
	key = gui_msgbox(title, "Is the audio okay?", WITH_YESNO);
	ti->success = (key == KEY_YES);
	if (!ti->success) {
	    pj_ansi_snprintf(textbuf, sizeof(textbuf),
			     "You will probably need to copy the recorded "
			     "WAV file %s to a desktop computer and analyze "
			     "it, to find out whether it's a recording "
			     "or playback problem.",
			     WAV_REC_OUT_PATH);
	    gui_msgbox(title, textbuf, WITH_OK);
	    pj_ansi_strcpy(ti->reason, USER_ERROR);
	}
    }
}


/****************************************************************************
 * test: audio system test
 */
static void systest_audio_test(void)
{
    enum {
	GOOD_MAX_INTERVAL = 5,
    };
    const pjmedia_dir dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
    pjmedia_aud_param param;
    pjmedia_aud_test_results result;
    int textbufpos;
    enum gui_key key;
    unsigned problem_count = 0;
    const char *problems[16];
    char drifttext[120];
    test_item_t *ti;
    const char *title = "Audio Device Test";
    pj_status_t status;

    ti = systest_alloc_test_item(title);
    if (!ti)
	return;

    key = gui_msgbox(title,
		     "This will run an automated test for about "
		     "ten seconds or so, and display some "
		     "statistics about your sound device. "
		     "Please don't do anything until the test completes. "
		     "Press OK to start, or CANCEL to skip this test.",
		     WITH_OKCANCEL);
    if (key != KEY_OK) {
	ti->skipped = PJ_TRUE;
	return;
    }

    PJ_LOG(3,(THIS_FILE, "Running %s", title));

    /* Disable sound device in pjsua first */
    pjsua_set_no_snd_dev(0);

    /* Setup parameters */
    status = pjmedia_aud_dev_default_param(systest.play_id, &param);
    if (status != PJ_SUCCESS) {
	systest_perror("Sorry we had error in pjmedia_aud_dev_default_param()", status);
	pjsua_set_snd_dev(0, systest.rec_id, systest.play_id);
	ti->success = PJ_FALSE;
	pj_strerror(status, ti->reason, sizeof(ti->reason));
	ti->reason[sizeof(ti->reason)-1] = '\0';
	return;
    }

    param.dir = dir;
    param.rec_id = systest.rec_id;
    param.play_id = systest.play_id;
    param.clock_rate = systest.media_cfg.snd_clock_rate;
    param.channel_count = systest.media_cfg.channel_count;
    param.samples_per_frame = param.clock_rate * param.channel_count * 
			      systest.media_cfg.audio_frame_ptime / 1000;

    /* Latency settings */
    param.flags |= (PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY | 
		    PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY);
    param.input_latency_ms = systest.media_cfg.snd_rec_latency;
    param.output_latency_ms = systest.media_cfg.snd_play_latency;

    /* Run the test */
    status = pjmedia_aud_test(&param, &result);
    if (status != PJ_SUCCESS) {
	systest_perror("Sorry we encountered error with the test", status);
	pjsua_set_snd_dev(0, systest.rec_id, systest.play_id);
	ti->success = PJ_FALSE;
	pj_strerror(status, ti->reason, sizeof(ti->reason));
	ti->reason[sizeof(ti->reason)-1] = '\0';
	return;
    }

    /* Restore pjsua sound device */
    pjsua_set_snd_dev(0, systest.rec_id, systest.play_id);

    /* Analyze the result! */
    strcpy(textbuf, "Here are the audio statistics:\r\n");
    textbufpos = strlen(textbuf);

    if (result.rec.frame_cnt==0) {
	problems[problem_count++] = 
	    "No audio frames were captured from the microphone. "
	    "This means the audio device is not working properly.";
    } else {
	pj_ansi_snprintf(textbuf+textbufpos, 
			 sizeof(textbuf)-textbufpos,
			 "Rec : interval (min/max/avg/dev)=\r\n"
			 "         %u/%u/%u/%u (ms)\r\n"
			 "      max burst=%u\r\n",
			 result.rec.min_interval,
			 result.rec.max_interval,
			 result.rec.avg_interval,
			 result.rec.dev_interval,
			 result.rec.max_burst);
	textbufpos = strlen(textbuf);

	if (result.rec.max_burst > GOOD_MAX_INTERVAL) {
	    problems[problem_count++] = 
		"Recording max burst is quite high";
	}
    }

    if (result.play.frame_cnt==0) {
	problems[problem_count++] = 
	    "No audio frames were played to the speaker. "
	    "This means the audio device is not working properly.";
    } else {
	pj_ansi_snprintf(textbuf+textbufpos, 
			 sizeof(textbuf)-textbufpos,
			 "Play: interval (min/max/avg/dev)=\r\n"
			 "         %u/%u/%u/%u (ms)\r\n"
			 "      burst=%u\r\n",
			 result.play.min_interval,
			 result.play.max_interval,
			 result.play.avg_interval,
			 result.play.dev_interval,
			 result.play.max_burst);
	textbufpos = strlen(textbuf);

	if (result.play.max_burst > GOOD_MAX_INTERVAL) {
	    problems[problem_count++] = 
		"Playback max burst is quite high";
	}
    }

    if (result.rec_drift_per_sec) {
	const char *which = result.rec_drift_per_sec>=0 ? "faster" : "slower";
	unsigned drift = result.rec_drift_per_sec>=0 ? 
			    result.rec_drift_per_sec :
			    -result.rec_drift_per_sec;

	pj_ansi_snprintf(drifttext, sizeof(drifttext),
			"Clock drifts detected. Capture "
			"is %d samples/sec %s "
			"than the playback device",
			drift, which);
	problems[problem_count++] = drifttext;
    }

    if (problem_count == 0) {
	pj_ansi_snprintf(textbuf+textbufpos, 
			 sizeof(textbuf)-textbufpos,
			 "\r\nThe sound device seems to be okay!");
	textbufpos = strlen(textbuf);

	key = gui_msgbox("Audio Device Test", textbuf, WITH_OK);
    } else {
	unsigned i;

	pj_ansi_snprintf(textbuf+textbufpos,
			 sizeof(textbuf)-textbufpos, 
			 "There could be %d problem(s) with the "
			 "sound device:\r\n",
			 problem_count);
	textbufpos = strlen(textbuf);

	for (i=0; i<problem_count; ++i) {
	    pj_ansi_snprintf(textbuf+textbufpos,
			     sizeof(textbuf)-textbufpos, 
			     " %d: %s\r\n", i+1, problems[i]);
	    textbufpos = strlen(textbuf);
	}

	key = gui_msgbox(title, textbuf, WITH_OK);
    }

    ti->success = PJ_TRUE;
    pj_ansi_strncpy(ti->reason, textbuf, sizeof(ti->reason));
    ti->reason[sizeof(ti->reason)-1] = '\0';
}


/****************************************************************************
 * sound latency test
 */
static int calculate_latency(pj_pool_t *pool, pjmedia_port *wav,
			     unsigned *lat_sum, unsigned *lat_cnt, 
			     unsigned *lat_min, unsigned *lat_max)
{
    pjmedia_frame frm;
    short *buf;
    unsigned i, samples_per_frame, read, len;
    unsigned start_pos;
    pj_bool_t first;
    pj_status_t status;

    *lat_sum = 0;
    *lat_cnt = 0;
    *lat_min = 10000;
    *lat_max = 0;

    samples_per_frame = wav->info.samples_per_frame;
    frm.buf = pj_pool_alloc(pool, samples_per_frame * 2);
    frm.size = samples_per_frame * 2;
    len = pjmedia_wav_player_get_len(wav);
    buf = pj_pool_alloc(pool, len + samples_per_frame);

    /* Read the whole file */
    read = 0;
    while (read < len/2) {
	status = pjmedia_port_get_frame(wav, &frm);
	if (status != PJ_SUCCESS)
	    break;

	pjmedia_copy_samples(buf+read, (short*)frm.buf, samples_per_frame);
	read += samples_per_frame;
    }

    if (read < 2 * wav->info.clock_rate) {
	systest_perror("The WAV file is too short", PJ_SUCCESS);
	return -1;
    }

    /* Zero the first 500ms to remove loud click noises 
     * (keypad press, etc.)
     */
    pjmedia_zero_samples(buf, wav->info.clock_rate / 2);

    /* Loop to calculate latency */
    start_pos = 0;
    first = PJ_TRUE;
    while (start_pos < len/2 - wav->info.clock_rate) {
	int max_signal = 0;
	unsigned max_signal_pos = start_pos;
	unsigned max_echo_pos = 0;
	unsigned pos;
	unsigned lat;

	/* Get the largest signal in the next 0.7s */
	for (i=start_pos; i<start_pos + wav->info.clock_rate * 700 / 1000; ++i) {
	    if (abs(buf[i]) > max_signal) {
		max_signal = abs(buf[i]);
		max_signal_pos = i;
	    }
	}

	/* Advance 10ms from max_signal_pos */
	pos = max_signal_pos + 10 * wav->info.clock_rate / 1000;

	/* Get the largest signal in the next 800ms */
	max_signal = 0;
	max_echo_pos = pos;
	for (i=pos; i<pos+wav->info.clock_rate * 8 / 10; ++i) {
	    if (abs(buf[i]) > max_signal) {
		max_signal = abs(buf[i]);
		max_echo_pos = i;
	    }
	}

	lat = (max_echo_pos - max_signal_pos) * 1000 / wav->info.clock_rate;

#if 0
	PJ_LOG(4,(THIS_FILE, "Signal at %dms, echo at %d ms, latency %d ms",
		  max_signal_pos * 1000 / wav->info.clock_rate,
		  max_echo_pos * 1000 / wav->info.clock_rate,
		  lat));
#endif

	*lat_sum += lat;
	(*lat_cnt)++;
	if (lat < *lat_min)
	    *lat_min = lat;
	if (lat > *lat_max)
	    *lat_max = lat;

	/* Advance next loop */
	if (first) {
	    start_pos = max_signal_pos + wav->info.clock_rate * 9 / 10;
	    first = PJ_FALSE;
	} else {
	    start_pos += wav->info.clock_rate;
	}
    }

    return 0;
}


static void systest_latency_test(void)
{
    const char *ref_wav_paths[] = { add_path(res_path, WAV_TOCK8_PATH), ALT_PATH1 WAV_TOCK8_PATH };
    pj_str_t rec_wav_file;
    pjsua_player_id play_id = PJSUA_INVALID_ID;
    pjsua_conf_port_id play_slot = PJSUA_INVALID_ID;
    pjsua_recorder_id rec_id = PJSUA_INVALID_ID;
    pjsua_conf_port_id rec_slot = PJSUA_INVALID_ID;
    pj_pool_t *pool = NULL;
    pjmedia_port *wav_port = NULL;
    unsigned lat_sum=0, lat_cnt=0, lat_min=0, lat_max=0;
    enum gui_key key;
    test_item_t *ti;
    const char *title = "Audio Latency Test";
    pj_status_t status;

    ti = systest_alloc_test_item(title);
    if (!ti)
	return;

    key = gui_msgbox(title,
		     "This test will try to find the audio device's "
		     "latency. We will play a special WAV file to the "
		     "speaker for ten seconds, then at the end "
		     "calculate the latency. Please don't do anything "
		     "until the test is done.", WITH_OKCANCEL);
    if (key != KEY_OK) {
	ti->skipped = PJ_TRUE;
	return;
    }
    key = gui_msgbox(title, 
		     "For this test to work, we must be able to capture "
		     "the audio played in the speaker (the echo), and only"
		     " that audio (i.e. you must be in relatively quiet "
		     "place to run this test). "
		     "Press OK to start, or CANCEL to skip.",
		     WITH_OKCANCEL);
    if (key != KEY_OK) {
	ti->skipped = PJ_TRUE;
	return;
    }

    PJ_LOG(3,(THIS_FILE, "Running %s", title));

    status = create_player(PJ_ARRAY_SIZE(ref_wav_paths), ref_wav_paths, 
			   &play_id);
    if (status != PJ_SUCCESS)
	goto on_return;

    play_slot = pjsua_player_get_conf_port(0, play_id);

    rec_wav_file = pj_str(add_path(doc_path, WAV_LATENCY_OUT_PATH));
    status = pjsua_recorder_create(0, &rec_wav_file, 0, NULL, -1, 0, &rec_id);
    if (status != PJ_SUCCESS)
	goto on_return;

    rec_slot = pjsua_recorder_get_conf_port(0, rec_id);

    /* Setup the test */
    //status = pjsua_conf_connect(0, 0);
    status = pjsua_conf_connect(0, play_slot, 0);
    status = pjsua_conf_connect(0, 0, rec_slot);
    status = pjsua_conf_connect(0, play_slot, rec_slot);
    

    /* We're running */
    PJ_LOG(3,(THIS_FILE, "Please wait while test is running (~10 sec)"));
    gui_sleep(10);

    /* Done with the test */
    //status = pjsua_conf_disconnect(0, 0);
    status = pjsua_conf_disconnect(0, play_slot, rec_slot);
    status = pjsua_conf_disconnect(0, 0, rec_slot);
    status = pjsua_conf_disconnect(0, play_slot, 0);

    pjsua_recorder_destroy(0, rec_id);
    rec_id = PJSUA_INVALID_ID;

    pjsua_player_destroy(0, play_id);
    play_id = PJSUA_INVALID_ID;

    /* Confirm that echo is heard */
    gui_msgbox(title,
	       "Test is done. Now we need to confirm that we indeed "
	       "captured the echo. We will play the captured audio "
	       "and please confirm that you can hear the 'tock' echo.",
	       WITH_OK);

    status = pjsua_player_create(0, &rec_wav_file, 0, &play_id);
    if (status != PJ_SUCCESS)
	goto on_return;

    play_slot = pjsua_player_get_conf_port(0, play_id);

    status = pjsua_conf_connect(0, play_slot, 0);
    if (status != PJ_SUCCESS)
	goto on_return;

    key = gui_msgbox(title,
		     "The captured audio is being played back now. "
		     "Can you hear the 'tock' echo?",
		     WITH_YESNO);

    pjsua_player_destroy(0, play_id);
    play_id = PJSUA_INVALID_ID;

    if (key != KEY_YES)
	goto on_return;

    /* Now analyze the latency */
    pool = pjsua_pool_create(0, "latency", 512, 512);

    status = pjmedia_wav_player_port_create(pool, rec_wav_file.ptr, 0, 0, 0, &wav_port);
    if (status != PJ_SUCCESS)
	goto on_return;

    status = calculate_latency(pool, wav_port, &lat_sum, &lat_cnt, 
			       &lat_min, &lat_max);
    if (status != PJ_SUCCESS)
	goto on_return;

on_return:
    if (wav_port)
	pjmedia_port_destroy(wav_port);
    if (pool)
	pj_pool_release(pool);
    if (play_id != PJSUA_INVALID_ID)
	pjsua_player_destroy(0, play_id);
    if (rec_id != PJSUA_INVALID_ID)
	pjsua_recorder_destroy(0, rec_id);

    if (status != PJ_SUCCESS) {
	systest_perror("Sorry we encountered an error: ", status);
	ti->success = PJ_FALSE;
	pj_strerror(status, ti->reason, sizeof(ti->reason));
    } else if (key != KEY_YES) {
	ti->success = PJ_FALSE;
	if (!ti->success) {
	    pj_ansi_strcpy(ti->reason, USER_ERROR);
	}
    } else {
	char msg[200];
	int msglen;

	pj_ansi_snprintf(msg, sizeof(msg),
			 "The sound device latency:\r\n"
			 " Min=%u, Max=%u, Avg=%u\r\n",
			 lat_min, lat_max, lat_sum/lat_cnt);
	msglen = strlen(msg);

	if (lat_sum/lat_cnt > 500) {
	    pj_ansi_snprintf(msg+msglen, sizeof(msg)-msglen,
			     "The latency is huge!\r\n");
	    msglen = strlen(msg);
	} else if (lat_sum/lat_cnt > 200) {
	    pj_ansi_snprintf(msg+msglen, sizeof(msg)-msglen,
			     "The latency is quite high\r\n");
	    msglen = strlen(msg);
	}
	
	key = gui_msgbox(title, msg, WITH_OK);

	ti->success = PJ_TRUE;
	pj_ansi_strncpy(ti->reason, msg, sizeof(ti->reason));
	ti->reason[sizeof(ti->reason)-1] = '\0';
    }
}


static void systest_aec_test(void)
{
    const char *ref_wav_paths[] = { add_path(res_path, WAV_PLAYBACK_PATH),
				    ALT_PATH1 WAV_PLAYBACK_PATH };
    pjsua_player_id player_id = PJSUA_INVALID_ID;
    pjsua_recorder_id writer_id = PJSUA_INVALID_ID;
    enum gui_key key;
    test_item_t *ti;
    const char *title = "AEC/AES Test";
    unsigned last_ec_tail = 0;
    pj_status_t status;
    pj_str_t tmp;

    ti = systest_alloc_test_item(title);
    if (!ti)
	return;

    key = gui_msgbox(title,
		     "This test will try to find whether the AEC/AES "
		     "works good on this system. Test will play a file "
		     "while recording from mic. The recording will be "
		     "played back later so you can check if echo is there. "
		     "Press OK to start.",
		     WITH_OKCANCEL);
    if (key != KEY_OK) {
	ti->skipped = PJ_TRUE;
	return;
    }

    /* Save current EC tail */
    status = pjsua_get_ec_tail(0, &last_ec_tail);
    if (status != PJ_SUCCESS)
	goto on_return;

    /* Set EC tail setting to default */
    status = pjsua_set_ec(0, PJSUA_DEFAULT_EC_TAIL_LEN, 0);
    if (status != PJ_SUCCESS)
	goto on_return;

    /*
     * Create player and recorder
     */
    status = create_player(PJ_ARRAY_SIZE(ref_wav_paths), ref_wav_paths, 
			   &player_id);
    if (status != PJ_SUCCESS) {
	PJ_PERROR(1,(THIS_FILE, status, "Error opening WAV file %s",
		     WAV_PLAYBACK_PATH));
	goto on_return;
    }

    status = pjsua_recorder_create(0,
                 pj_cstr(&tmp, add_path(doc_path, AEC_REC_PATH)), 0, 0, -1,
                 0, &writer_id);
    if (status != PJ_SUCCESS) {
	PJ_PERROR(1,(THIS_FILE, status, "Error writing WAV file %s",
		     AEC_REC_PATH));
	goto on_return;
    }

    /*
     * Start playback and recording.
     */
    pjsua_conf_connect(0, pjsua_player_get_conf_port(0, player_id), 0);
    pj_thread_sleep(100);
    pjsua_conf_connect(0, 0, pjsua_recorder_get_conf_port(0, writer_id));

    /* Wait user signal */
    gui_msgbox(title, "AEC/AES test is running. Press OK to stop this test.",
	       WITH_OK);

    /*
     * Stop and close playback and recorder
     */
    pjsua_conf_disconnect(0, 0, pjsua_recorder_get_conf_port(0, writer_id));
    pjsua_conf_disconnect(0, pjsua_player_get_conf_port(0, player_id), 0);
    pjsua_recorder_destroy(0, writer_id);
    pjsua_player_destroy(0, player_id);
    player_id = PJSUA_INVALID_ID;
    writer_id = PJSUA_INVALID_ID;

    /*
     * Play the result.
     */
    status = pjsua_player_create(0,
                 pj_cstr(&tmp, add_path(doc_path, AEC_REC_PATH)),
                 0, &player_id);
    if (status != PJ_SUCCESS) {
	PJ_PERROR(1,(THIS_FILE, status, "Error opening WAV file %s", AEC_REC_PATH));
	goto on_return;
    }
    pjsua_conf_connect(0, pjsua_player_get_conf_port(0, player_id), 0);

    /* Wait user signal */
    gui_msgbox(title, "We are now playing the captured audio from the mic. "
		      "Check if echo (of the audio played back previously) is "
		      "present in the audio. The recording is stored in " 
		      AEC_REC_PATH " for offline analysis. "
		      "Press OK to stop.",
		      WITH_OK);

    pjsua_conf_disconnect(0, pjsua_player_get_conf_port(0, player_id), 0);

    key = gui_msgbox(title,
		     "Did you notice any echo in the recording?",
		     WITH_YESNO);


on_return:
    if (player_id != PJSUA_INVALID_ID)
	pjsua_player_destroy(0, player_id);
    if (writer_id != PJSUA_INVALID_ID)
	pjsua_recorder_destroy(0, writer_id);

    /* Wait until sound device closed before restoring back EC tail setting */
    while (pjsua_snd_is_active(0))
	pj_thread_sleep(10);
    pjsua_set_ec(0, last_ec_tail, 0);


    if (status != PJ_SUCCESS) {
	systest_perror("Sorry we encountered an error: ", status);
	ti->success = PJ_FALSE;
	pj_strerror(status, ti->reason, sizeof(ti->reason));
    } else if (key == KEY_YES) {
	ti->success = PJ_FALSE;
	if (!ti->success) {
	    pj_ansi_strcpy(ti->reason, USER_ERROR);
	}
    } else {
	char msg[200];

	pj_ansi_snprintf(msg, sizeof(msg), "Test succeeded.\r\n");

	ti->success = PJ_TRUE;
	pj_ansi_strncpy(ti->reason, msg, sizeof(ti->reason));
	ti->reason[sizeof(ti->reason)-1] = '\0';
    }
}


/****************************************************************************
 * configurations
 */
static void systest_list_audio_devs()
{
    unsigned i, dev_count, len=0;
    pj_status_t status;
    test_item_t *ti;
    enum gui_key key;
    const char *title = "Audio Device List";
    
    ti = systest_alloc_test_item(title);
    if (!ti)
	return;

    PJ_LOG(3,(THIS_FILE, "Running %s", title));

    dev_count = pjmedia_aud_dev_count();
    if (dev_count == 0) {
	key = gui_msgbox(title, 
			 "No audio devices are found", WITH_OK);
	ti->success = PJ_FALSE;
	pj_ansi_strcpy(ti->reason, "No device found");
	return;
    }

    pj_ansi_snprintf(ti->reason+len, sizeof(ti->reason)-len,
		     "Found %u devices\r\n", dev_count);
    len = strlen(ti->reason);

    for (i=0; i<dev_count; ++i) {
	pjmedia_aud_dev_info info;

	status = pjmedia_aud_dev_get_info(i, &info);
	if (status != PJ_SUCCESS) {
	    systest_perror("Error retrieving device info: ", status);
	    ti->success = PJ_FALSE;
	    pj_strerror(status, ti->reason, sizeof(ti->reason));
	    return;
	}

	pj_ansi_snprintf(ti->reason+len, sizeof(ti->reason)-len,
			 " %2d: %s [%s] (%d/%d)\r\n",
		          i, info.driver, info.name, 
			  info.input_count, info.output_count);
	len = strlen(ti->reason);
    }

    ti->reason[len] = '\0';
    key = gui_msgbox(title, ti->reason, WITH_OK);

    ti->success = PJ_TRUE;
}

static void systest_display_settings(void)
{
    pjmedia_aud_dev_info di;
    int len = 0;
    enum gui_key key;
    test_item_t *ti;
    const char *title = "Audio Settings";
    pj_status_t status;

    ti = systest_alloc_test_item(title);
    if (!ti)
	return;

    PJ_LOG(3,(THIS_FILE, "Running %s", title));

    pj_ansi_snprintf(textbuf+len, sizeof(textbuf)-len, "Version: %s\r\n",
		     pj_get_version());
    len = strlen(textbuf);

    pj_ansi_snprintf(textbuf+len, sizeof(textbuf)-len, "Test clock rate: %d\r\n",
		     systest.media_cfg.clock_rate);
    len = strlen(textbuf);

    pj_ansi_snprintf(textbuf+len, sizeof(textbuf)-len, "Device clock rate: %d\r\n",
		     systest.media_cfg.snd_clock_rate);
    len = strlen(textbuf);

    pj_ansi_snprintf(textbuf+len, sizeof(textbuf)-len, "Aud frame ptime: %d\r\n",
		     systest.media_cfg.audio_frame_ptime);
    len = strlen(textbuf);

    pj_ansi_snprintf(textbuf+len, sizeof(textbuf)-len, "Channel count: %d\r\n",
		     systest.media_cfg.channel_count);
    len = strlen(textbuf);

    pj_ansi_snprintf(textbuf+len, sizeof(textbuf)-len, "Audio switching: %s\r\n",
	    (PJMEDIA_CONF_USE_SWITCH_BOARD ? "Switchboard" : "Conf bridge"));
    len = strlen(textbuf);

    pj_ansi_snprintf(textbuf+len, sizeof(textbuf)-len, "Snd buff count: %d\r\n",
		     PJMEDIA_SOUND_BUFFER_COUNT);
    len = strlen(textbuf);

    /* Capture device */
    status = pjmedia_aud_dev_get_info(systest.rec_id, &di);
    if (status != PJ_SUCCESS) {
	systest_perror("Error querying device info", status);
	ti->success = PJ_FALSE;
	pj_strerror(status, ti->reason, sizeof(ti->reason));
	return;
    }

    pj_ansi_snprintf(textbuf+len, sizeof(textbuf)-len,
		     "Rec dev : %d (%s) [%s]\r\n",
		     systest.rec_id,
		     di.name,
		     di.driver);
    len = strlen(textbuf);

    pj_ansi_snprintf(textbuf+len, sizeof(textbuf)-len,
		     "Rec  buf : %d msec\r\n",
		     systest.media_cfg.snd_rec_latency);
    len = strlen(textbuf);

    /* Playback device */
    status = pjmedia_aud_dev_get_info(systest.play_id, &di);
    if (status != PJ_SUCCESS) {
	systest_perror("Error querying device info", status);
	return;
    }

    pj_ansi_snprintf(textbuf+len, sizeof(textbuf)-len,
		     "Play dev: %d (%s) [%s]\r\n",
		     systest.play_id,
		     di.name,
		     di.driver);
    len = strlen(textbuf);

    pj_ansi_snprintf(textbuf+len, sizeof(textbuf)-len,
		     "Play buf: %d msec\r\n",
		     systest.media_cfg.snd_play_latency);
    len = strlen(textbuf);

    ti->success = PJ_TRUE;
    pj_ansi_strncpy(ti->reason, textbuf, sizeof(ti->reason));
    ti->reason[sizeof(ti->reason)-1] = '\0';
    key = gui_msgbox(title, textbuf, WITH_OK);

}

/*****************************************************************/

int systest_init(void)
{
    pjsua_logging_config log_cfg;
    pj_status_t status = PJ_SUCCESS;

    status = pjsua_create();
    if (status != PJ_SUCCESS) {
	systest_perror("Sorry we've had error in pjsua_create(): ", status);
	return status;
    }

    pjsua_logging_config_default(&log_cfg);
    log_cfg.log_filename = pj_str(add_path(doc_path, LOG_OUT_PATH));

    pjsua_config_default(&systest.ua_cfg);
    pjsua_media_config_default(&systest.media_cfg);
    systest.media_cfg.clock_rate = TEST_CLOCK_RATE;
    systest.media_cfg.snd_clock_rate = DEV_CLOCK_RATE;
    if (OVERRIDE_AUD_FRAME_PTIME)
	systest.media_cfg.audio_frame_ptime = OVERRIDE_AUD_FRAME_PTIME;
    systest.media_cfg.channel_count = CHANNEL_COUNT;
    systest.rec_id = REC_DEV_ID;
    systest.play_id = PLAY_DEV_ID;
    systest.media_cfg.ec_tail_len = 0;
    systest.media_cfg.snd_auto_close_time = 0;

#if defined(OVERRIDE_AUDDEV_PLAY_LAT) && OVERRIDE_AUDDEV_PLAY_LAT!=0
    systest.media_cfg.snd_play_latency = OVERRIDE_AUDDEV_PLAY_LAT;
#endif

#if defined(OVERRIDE_AUDDEV_REC_LAT) && OVERRIDE_AUDDEV_REC_LAT!=0
    systest.media_cfg.snd_rec_latency = OVERRIDE_AUDDEV_REC_LAT;
#endif
	
    status = pjsua_init(0, &systest.ua_cfg, &log_cfg, &systest.media_cfg);
    if (status != PJ_SUCCESS) {
	pjsua_destroy(0);
	systest_perror("Sorry we've had error in pjsua_init(): ", status);
	return status;
    }

    status = pjsua_start(0);
    if (status != PJ_SUCCESS) {
	pjsua_destroy(0);
	systest_perror("Sorry we've had error in pjsua_start(): ", status);
	return status;
    }

    status = gui_init(&root_menu);
    if (status != 0)
	goto on_return;

    return 0;

on_return:
    gui_destroy();
    return status;
}


int systest_set_dev(int cap_dev, int play_dev)
{
    systest.rec_id = systest_cap_dev_id = cap_dev;
    systest.play_id = systest_play_dev_id = play_dev;
    return pjsua_set_snd_dev(0, cap_dev, play_dev);
}

static void systest_wizard(void)
{
    PJ_LOG(3,(THIS_FILE, "Running test wizard"));
    systest_list_audio_devs();
    systest_display_settings();
    systest_play_tone();
    systest_play_wav1();
    systest_rec_audio();
    systest_audio_test();
    systest_latency_test();
    systest_aec_test();
    gui_msgbox("Test wizard", "Test wizard complete.", WITH_OK);
}


int systest_run(void)
{
    gui_start(&root_menu);
    return 0;
}

void systest_save_result(const char *filename)
{
    unsigned i;
    pj_oshandle_t fd;
    pj_time_val tv;
    pj_parsed_time pt;
    pj_ssize_t size;
    const char *text;
    pj_status_t status;

    status = pj_file_open(NULL, filename, PJ_O_WRONLY | PJ_O_APPEND, &fd, NULL);
    if (status != PJ_SUCCESS) {
	pj_ansi_snprintf(textbuf, sizeof(textbuf),
			 "Error opening file %s",
			 filename);
	systest_perror(textbuf, status);
	return;
    }

    text = "\r\n\r\nPJSYSTEST Report\r\n";
    size = strlen(text);
    pj_file_write(fd, text, &size);

    /* Put timestamp */
    pj_gettimeofday(&tv);
    if (pj_time_decode(&tv, &pt) == PJ_SUCCESS) {
	pj_ansi_snprintf(textbuf, sizeof(textbuf),
			 "Time: %04d/%02d/%02d %02d:%02d:%02d\r\n",
			 pt.year, pt.mon+1, pt.day,
			 pt.hour, pt.min, pt.sec);
	size = strlen(textbuf);
	pj_file_write(fd, textbuf, &size);
    }

    pj_ansi_snprintf(textbuf, sizeof(textbuf),
		     "Tests invoked: %u\r\n"
		     "-----------------------------------------------\r\n",
		     test_item_count);
    size = strlen(textbuf);
    pj_file_write(fd, textbuf, &size);

    for (i=0; i<test_item_count; ++i) {
	test_item_t *ti = &test_items[i];
	pj_ansi_snprintf(textbuf, sizeof(textbuf),
			 "\r\nTEST %d: %s %s\r\n",
			 i, ti->title,
			 (ti->skipped? "Skipped" : (ti->success ? "Success" : "Failed")));
	size = strlen(textbuf);
	pj_file_write(fd, textbuf, &size);

	size = strlen(ti->reason);
	pj_file_write(fd, ti->reason, &size);

	size = 2;
	pj_file_write(fd, "\r\n", &size);
    }


    pj_file_close(fd);

    pj_ansi_snprintf(textbuf, sizeof(textbuf),
		     "Test result successfully appended to file %s",
		     filename);
    gui_msgbox("Test result saved", textbuf, WITH_OK);
}

void systest_deinit(void)
{
    gui_destroy();
    pjsua_destroy(0);
}
