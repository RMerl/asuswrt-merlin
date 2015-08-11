/* $Id: auddemo.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia-audiodev/audiodev.h>
#include <pjmedia-audiodev/audiotest.h>
#include <pjmedia.h>
#include <pjlib.h>
#include <pjlib-util.h>

#define THIS_FILE	"auddemo.c"
#define MAX_DEVICES	64
#define WAV_FILE	"auddemo.wav"


static unsigned dev_count;
static unsigned playback_lat = PJMEDIA_SND_DEFAULT_PLAY_LATENCY;
static unsigned capture_lat = PJMEDIA_SND_DEFAULT_REC_LATENCY;

static void app_perror(const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));	
    printf( "%s: %s (err=%d)\n",
	    title, errmsg, status);
}

static void list_devices(void)
{
    unsigned i;
    pj_status_t status;
    
    dev_count = pjmedia_aud_dev_count();
    if (dev_count == 0) {
	PJ_LOG(3,(THIS_FILE, "No devices found"));
	return;
    }

    PJ_LOG(3,(THIS_FILE, "Found %d devices:", dev_count));

    for (i=0; i<dev_count; ++i) {
	pjmedia_aud_dev_info info;

	status = pjmedia_aud_dev_get_info(i, &info);
	if (status != PJ_SUCCESS)
	    continue;

	PJ_LOG(3,(THIS_FILE," %2d: %s [%s] (%d/%d)",
	          i, info.driver, info.name, info.input_count, info.output_count));
    }
}

static const char *decode_caps(unsigned caps)
{
    static char text[200];
    unsigned i;

    text[0] = '\0';

    for (i=0; i<31; ++i) {
	if ((1 << i) & caps) {
	    const char *capname;
	    capname = pjmedia_aud_dev_cap_name((pjmedia_aud_dev_cap)(1 << i), 
					       NULL);
	    strcat(text, capname);
	    strcat(text, " ");
	}
    }

    return text;
}

static void show_dev_info(unsigned index)
{
#define H   "%-20s"
    pjmedia_aud_dev_info info;
    char formats[200];
    pj_status_t status;

    if (index >= dev_count) {
	PJ_LOG(1,(THIS_FILE, "Error: invalid index %u", index));
	return;
    }

    status = pjmedia_aud_dev_get_info(index, &info);
    if (status != PJ_SUCCESS) {
	app_perror("pjmedia_aud_dev_get_info() error", status);
	return;
    }

    PJ_LOG(3, (THIS_FILE, "Device at index %u:", index));
    PJ_LOG(3, (THIS_FILE, "-------------------------"));

    PJ_LOG(3, (THIS_FILE, H": %u (0x%x)", "ID", index, index));
    PJ_LOG(3, (THIS_FILE, H": %s", "Name", info.name));
    PJ_LOG(3, (THIS_FILE, H": %s", "Driver", info.driver));
    PJ_LOG(3, (THIS_FILE, H": %u", "Input channels", info.input_count));
    PJ_LOG(3, (THIS_FILE, H": %u", "Output channels", info.output_count));
    PJ_LOG(3, (THIS_FILE, H": %s", "Capabilities", decode_caps(info.caps)));

    formats[0] = '\0';
    if (info.caps & PJMEDIA_AUD_DEV_CAP_EXT_FORMAT) {
	unsigned i;

	for (i=0; i<info.ext_fmt_cnt; ++i) {
	    char bitrate[32];

	    switch (info.ext_fmt[i].id) {
	    case PJMEDIA_FORMAT_L16:
		strcat(formats, "L16/");
		break;
	    case PJMEDIA_FORMAT_PCMA:
		strcat(formats, "PCMA/");
		break;
	    case PJMEDIA_FORMAT_PCMU:
		strcat(formats, "PCMU/");
		break;
	    case PJMEDIA_FORMAT_AMR:
		strcat(formats, "AMR/");
		break;
	    case PJMEDIA_FORMAT_G729:
		strcat(formats, "G729/");
		break;
	    case PJMEDIA_FORMAT_ILBC:
		strcat(formats, "ILBC/");
		break;
	    default:
		strcat(formats, "unknown/");
		break;
	    }
	    sprintf(bitrate, "%u", info.ext_fmt[i].bitrate);
	    strcat(formats, bitrate);
	    strcat(formats, " ");
	}
    }
    PJ_LOG(3, (THIS_FILE, H": %s", "Extended formats", formats));

#undef H
}

static void test_device(pjmedia_dir dir, unsigned rec_id, unsigned play_id, 
			unsigned clock_rate, unsigned ptime, 
			unsigned chnum)
{
    pjmedia_aud_param param;
    pjmedia_aud_test_results result;
    pj_status_t status;

    if (dir & PJMEDIA_DIR_CAPTURE) {
	status = pjmedia_aud_dev_default_param(rec_id, &param);
    } else {
	status = pjmedia_aud_dev_default_param(play_id, &param);
    }

    if (status != PJ_SUCCESS) {
	app_perror("pjmedia_aud_dev_default_param()", status);
	return;
    }

    param.dir = dir;
    param.rec_id = rec_id;
    param.play_id = play_id;
    param.clock_rate = clock_rate;
    param.channel_count = chnum;
    param.samples_per_frame = clock_rate * chnum * ptime / 1000;

    /* Latency settings */
    param.flags |= (PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY | 
		    PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY);
    param.input_latency_ms = capture_lat;
    param.output_latency_ms = playback_lat;

    PJ_LOG(3,(THIS_FILE, "Performing test.."));

    status = pjmedia_aud_test(&param, &result);
    if (status != PJ_SUCCESS) {
	app_perror("Test has completed with error", status);
	return;
    }

    PJ_LOG(3,(THIS_FILE, "Done. Result:"));

    if (dir & PJMEDIA_DIR_CAPTURE) {
	if (result.rec.frame_cnt==0) {
	    PJ_LOG(1,(THIS_FILE, "Error: no frames captured!"));
	} else {
	    PJ_LOG(3,(THIS_FILE, "  %-20s: interval (min/max/avg/dev)=%u/%u/%u/%u, burst=%u",
		      "Recording result",
		      result.rec.min_interval,
		      result.rec.max_interval,
		      result.rec.avg_interval,
		      result.rec.dev_interval,
		      result.rec.max_burst));
	}
    }

    if (dir & PJMEDIA_DIR_PLAYBACK) {
	if (result.play.frame_cnt==0) {
	    PJ_LOG(1,(THIS_FILE, "Error: no playback!"));
	} else {
	    PJ_LOG(3,(THIS_FILE, "  %-20s: interval (min/max/avg/dev)=%u/%u/%u/%u, burst=%u",
		      "Playback result",
		      result.play.min_interval,
		      result.play.max_interval,
		      result.play.avg_interval,
		      result.play.dev_interval,
		      result.play.max_burst));
	}
    }

    if (dir==PJMEDIA_DIR_CAPTURE_PLAYBACK) {
	if (result.rec_drift_per_sec == 0) {
	    PJ_LOG(3,(THIS_FILE, " No clock drift detected"));
	} else {
	    const char *which = result.rec_drift_per_sec>=0 ? "faster" : "slower";
	    unsigned drift = result.rec_drift_per_sec>=0 ? 
				result.rec_drift_per_sec :
				-result.rec_drift_per_sec;

	    PJ_LOG(3,(THIS_FILE, " Clock drifts detected. Capture device "
				 "is running %d samples per second %s "
				 "than the playback device",
				 drift, which));
	}
    }
}


static pj_status_t wav_rec_cb(void *user_data, pjmedia_frame *frame)
{
    return pjmedia_port_put_frame((pjmedia_port*)user_data, frame);
}

static void record(unsigned rec_index, const char *filename)
{
    pj_pool_t *pool = NULL;
    pjmedia_port *wav = NULL;
    pjmedia_aud_param param;
    pjmedia_aud_stream *strm = NULL;
    char line[10], *dummy;
    pj_status_t status;

    if (filename == NULL)
	filename = WAV_FILE;

    pool = pj_pool_create(pjmedia_aud_subsys_get_pool_factory(), "wav",
			  1000, 1000, NULL);

    status = pjmedia_wav_writer_port_create(pool, filename, 16000, 
					    1, 320, 16, 0, 0, &wav);
    if (status != PJ_SUCCESS) {
	app_perror("Error creating WAV file", status);
	goto on_return;
    }

    status = pjmedia_aud_dev_default_param(rec_index, &param);
    if (status != PJ_SUCCESS) {
	app_perror("pjmedia_aud_dev_default_param()", status);
	goto on_return;
    }

    param.dir = PJMEDIA_DIR_CAPTURE;
    param.clock_rate = wav->info.clock_rate;
    param.samples_per_frame = wav->info.samples_per_frame;
    param.channel_count = wav->info.channel_count;
    param.bits_per_sample = wav->info.bits_per_sample;

    status = pjmedia_aud_stream_create(&param, &wav_rec_cb, NULL, wav,
				       &strm);
    if (status != PJ_SUCCESS) {
	app_perror("Error opening the sound device", status);
	goto on_return;
    }

    status = pjmedia_aud_stream_start(strm);
    if (status != PJ_SUCCESS) {
	app_perror("Error starting the sound device", status);
	goto on_return;
    }

    PJ_LOG(3,(THIS_FILE, "Recording started, press ENTER to stop"));
    dummy = fgets(line, sizeof(line), stdin);

on_return:
    if (strm) {
	pjmedia_aud_stream_stop(strm);
	pjmedia_aud_stream_destroy(strm);
    }
    if (wav)
	pjmedia_port_destroy(wav);
    if (pool)
	pj_pool_release(pool);
}


static pj_status_t wav_play_cb(void *user_data, pjmedia_frame *frame)
{
    return pjmedia_port_get_frame((pjmedia_port*)user_data, frame);
}


static void play_file(unsigned play_index, const char *filename)
{
    pj_pool_t *pool = NULL;
    pjmedia_port *wav = NULL;
    pjmedia_aud_param param;
    pjmedia_aud_stream *strm = NULL;
    char line[10], *dummy;
    pj_status_t status;

    if (filename == NULL)
	filename = WAV_FILE;

    pool = pj_pool_create(pjmedia_aud_subsys_get_pool_factory(), "wav",
			  1000, 1000, NULL);

    status = pjmedia_wav_player_port_create(pool, filename, 20, 0, 0, &wav);
    if (status != PJ_SUCCESS) {
	app_perror("Error opening WAV file", status);
	goto on_return;
    }

    status = pjmedia_aud_dev_default_param(play_index, &param);
    if (status != PJ_SUCCESS) {
	app_perror("pjmedia_aud_dev_default_param()", status);
	goto on_return;
    }

    param.dir = PJMEDIA_DIR_PLAYBACK;
    param.clock_rate = wav->info.clock_rate;
    param.samples_per_frame = wav->info.samples_per_frame;
    param.channel_count = wav->info.channel_count;
    param.bits_per_sample = wav->info.bits_per_sample;

    status = pjmedia_aud_stream_create(&param, NULL, &wav_play_cb, wav,
				       &strm);
    if (status != PJ_SUCCESS) {
	app_perror("Error opening the sound device", status);
	goto on_return;
    }

    status = pjmedia_aud_stream_start(strm);
    if (status != PJ_SUCCESS) {
	app_perror("Error starting the sound device", status);
	goto on_return;
    }

    PJ_LOG(3,(THIS_FILE, "Playback started, press ENTER to stop"));
    dummy = fgets(line, sizeof(line), stdin);

on_return:
    if (strm) {
	pjmedia_aud_stream_stop(strm);
	pjmedia_aud_stream_destroy(strm);
    }
    if (wav)
	pjmedia_port_destroy(wav);
    if (pool)
	pj_pool_release(pool);
}


static void print_menu(void)
{
    puts("");
    puts("Audio demo menu:");
    puts("-------------------------------");
    puts("  l                        List devices");
    puts("  R                        Refresh devices");
    puts("  i ID                     Show device info for device ID");
    puts("  t RID PID CR PTIM [CH]   Perform test on the device:");
    puts("                             RID:  record device ID (-1 for no)");
    puts("                             PID:  playback device ID (-1 for no)");
    puts("                             CR:   clock rate");
    puts("                             PTIM: ptime in ms");
    puts("                             CH:   # of channels");
    puts("  r RID [FILE]             Record capture device RID to WAV file");
    puts("  p PID [FILE]             Playback WAV file to device ID PID");
    puts("  d [RLAT PLAT]            Get/set sound device latencies (in ms):");
    puts("                             Specify no param to get current latencies setting");
    puts("                             RLAT: record latency (-1 for default)");
    puts("                             PLAT: playback latency (-1 for default)");
    puts("  v                        Toggle log verbosity");
    puts("  q                        Quit");
    puts("");
    printf("Enter selection: ");
    fflush(stdout);
}

int main()
{
    pj_caching_pool cp;
    pj_bool_t done = PJ_FALSE;
    pj_status_t status;

    /* Init pjlib */
    status = pj_init(0);
    PJ_ASSERT_RETURN(status==PJ_SUCCESS, 1);
    
    pj_log_set_decor(PJ_LOG_HAS_NEWLINE);

    /* Must create a pool factory before we can allocate any memory. */
    pj_caching_pool_init(0, &cp, &pj_pool_factory_default_policy, 0);

    status = pjmedia_aud_subsys_init(&cp.factory);
    if (status != PJ_SUCCESS) {
	app_perror("pjmedia_aud_subsys_init()", status);
	pj_caching_pool_destroy(&cp);
	pj_shutdown(0);
	return 1;
    }

    list_devices();

    while (!done) {
	char line[80];

	print_menu();

	if (fgets(line, sizeof(line), stdin)==NULL)
	    break;

	switch (line[0]) {
	case 'l':
	    list_devices();
	    break;

	case 'R':
	    pjmedia_aud_dev_refresh();
	    puts("Audio device list refreshed.");
	    break;

	case 'i':
	    {
		unsigned dev_index;
		if (sscanf(line+2, "%u", &dev_index) != 1) {
		    puts("error: device ID required");
		    break;
		}
		show_dev_info(dev_index);
	    }
	    break;

	case 't':
	    {
		pjmedia_dir dir;
		int rec_id, play_id;
		unsigned clock_rate, ptime, chnum;
		int cnt;

		cnt = sscanf(line+2, "%d %d %u %u %u", &rec_id, &play_id, 
			     &clock_rate, &ptime, &chnum);
		if (cnt < 4) {
		    puts("error: not enough parameters");
		    break;
		}
		if (clock_rate < 8000 || clock_rate > 128000) {
		    puts("error: invalid clock rate");
		    break;
		}
		if (ptime < 10 || ptime > 500) {
		    puts("error: invalid ptime");
		    break;
		}
		if (cnt==5) {
		    if (chnum < 1 || chnum > 4) {
			puts("error: invalid number of channels");
			break;
		    }
		} else {
		    chnum = 1;
		}

		if (rec_id >= 0 && rec_id < (int)dev_count) {
		    if (play_id >= 0 && play_id < (int)dev_count)
			dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
		    else
			dir = PJMEDIA_DIR_CAPTURE;
		} else if (play_id >= 0 && play_id < (int)dev_count) {
		    dir = PJMEDIA_DIR_PLAYBACK;
		} else {
		    puts("error: at least one valid device index required");
		    break;
		}

		test_device(dir, rec_id, play_id, clock_rate, ptime, chnum);
		
	    }
	    break;

	case 'r':
	    /* record */
	    {
		int index;
		char filename[80];
		int count;

		count = sscanf(line+2, "%d %s", &index, filename);
		if (count==1)
		    record(index, NULL);
		else if (count==2)
		    record(index, filename);
		else
		    puts("error: invalid command syntax");
	    }
	    break;

	case 'p':
	    /* playback */
	    {
		int index;
		char filename[80];
		int count;

		count = sscanf(line+2, "%d %s", &index, filename);
		if (count==1)
		    play_file(index, NULL);
		else if (count==2)
		    play_file(index, filename);
		else
		    puts("error: invalid command syntax");
	    }
	    break;

	case 'd':
	    /* latencies */
	    {
		int rec_lat, play_lat;

		if (sscanf(line+2, "%d %d", &rec_lat, &play_lat) == 2) {
		    capture_lat = (unsigned) 
			 (rec_lat>=0? rec_lat:PJMEDIA_SND_DEFAULT_REC_LATENCY);
		    playback_lat = (unsigned)
			 (play_lat >= 0? play_lat : PJMEDIA_SND_DEFAULT_PLAY_LATENCY);
		    printf("Recording latency=%ums, playback latency=%ums",
			   capture_lat, playback_lat);
		} else {
		    printf("Current latencies: record=%ums, playback=%ums",
			   capture_lat, playback_lat);
		}
		puts("");
	    }
	    break;

	case 'v':
	    if (pj_log_get_level(0) <= 3) {
		pj_log_set_level(0, 5);
		puts("Logging set to detail");
	    } else {
		pj_log_set_level(0, 3);
		puts("Logging set to quiet");
	    }
	    break;

	case 'q':
	    done = PJ_TRUE;
	    break;
	}
    }

    pj_caching_pool_destroy(&cp);
    pj_shutdown(0);
    return 0;
}


