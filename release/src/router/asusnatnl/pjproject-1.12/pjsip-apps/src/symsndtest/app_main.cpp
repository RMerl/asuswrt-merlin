/* $Id: app_main.cpp 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia/delaybuf.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/os.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/unicode.h>
#include <e32cons.h>

#define THIS_FILE		"app_main.cpp"
#define CLOCK_RATE		8000
#define CHANNEL_COUNT		1
#define PTIME			20
#define SAMPLES_PER_FRAME	(CLOCK_RATE*PTIME/1000)
#define BITS_PER_SAMPLE		16

extern CConsoleBase* console;

static pj_caching_pool cp;
static pjmedia_aud_stream *strm;
static unsigned rec_cnt, play_cnt;
static pj_time_val t_start;
static pjmedia_aud_param param;
static pj_pool_t *pool;
static pjmedia_delay_buf *delaybuf;
static char frame_buf[256];

static void copy_frame_ext(pjmedia_frame_ext *f_dst, 
                           const pjmedia_frame_ext *f_src) 
{
    pj_bzero(f_dst, sizeof(*f_dst));
    if (f_src->subframe_cnt) {
	f_dst->base.type = PJMEDIA_FRAME_TYPE_EXTENDED;
	for (unsigned i = 0; i < f_src->subframe_cnt; ++i) {
	    pjmedia_frame_ext_subframe *sf;
	    sf = pjmedia_frame_ext_get_subframe(f_src, i);
	    pjmedia_frame_ext_append_subframe(f_dst, sf->data, sf->bitlen, 
					      param.samples_per_frame);
	}
    } else {
	f_dst->base.type = PJMEDIA_FRAME_TYPE_NONE;
    }
}

/* Logging callback */
static void log_writer(int level, const char *buf, unsigned len)
{
    static wchar_t buf16[PJ_LOG_MAX_SIZE];

    PJ_UNUSED_ARG(level);

    pj_ansi_to_unicode(buf, len, buf16, PJ_ARRAY_SIZE(buf16));

    TPtrC16 aBuf((const TUint16*)buf16, (TInt)len);
    console->Write(aBuf);
}

/* perror util */
static void app_perror(const char *title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(1,(THIS_FILE, "Error: %s: %s", title, errmsg));
}

/* Application init */
static pj_status_t app_init()
{
    unsigned i, count;
    pj_status_t status;

    /* Redirect log */
    pj_log_set_log_func((void (*)(int,const char*,int)) &log_writer);
    pj_log_set_decor(PJ_LOG_HAS_NEWLINE);
    pj_log_set_level(3);

    /* Init pjlib */
    status = pj_init();
    if (status != PJ_SUCCESS) {
    	app_perror("pj_init()", status);
    	return status;
    }

    pj_caching_pool_init(&cp, NULL, 0);

    /* Init sound subsystem */
    status = pjmedia_aud_subsys_init(&cp.factory);
    if (status != PJ_SUCCESS) {
    	app_perror("pjmedia_snd_init()", status);
        pj_caching_pool_destroy(&cp);
    	pj_shutdown();
    	return status;
    }

    count = pjmedia_aud_dev_count();
    PJ_LOG(3,(THIS_FILE, "Device count: %d", count));
    for (i=0; i<count; ++i) {
    	pjmedia_aud_dev_info info;
    	pj_status_t status;

    	status = pjmedia_aud_dev_get_info(i, &info);
    	pj_assert(status == PJ_SUCCESS);
    	PJ_LOG(3, (THIS_FILE, "%d: %s %d/%d %dHz",
    		   i, info.name, info.input_count, info.output_count,
    		   info.default_samples_per_sec));
    	
	unsigned j;

	/* Print extended formats supported by this audio device */
	PJ_LOG(3, (THIS_FILE, "   Extended formats supported:"));
	for (j = 0; j < info.ext_fmt_cnt; ++j) {
	    const char *fmt_name = NULL;
	    
	    switch (info.ext_fmt[j].id) {
	    case PJMEDIA_FORMAT_PCMA:
		fmt_name = "PCMA";
		break;
	    case PJMEDIA_FORMAT_PCMU:
		fmt_name = "PCMU";
		break;
	    case PJMEDIA_FORMAT_AMR:
		fmt_name = "AMR-NB";
		break;
	    case PJMEDIA_FORMAT_G729:
		fmt_name = "G729";
		break;
	    case PJMEDIA_FORMAT_ILBC:
		fmt_name = "ILBC";
		break;
	    case PJMEDIA_FORMAT_PCM:
		fmt_name = "PCM";
		break;
	    default:
		fmt_name = "Unknown";
		break;
	    }
	    PJ_LOG(3, (THIS_FILE, "   - %s", fmt_name));
	}
    }

    /* Create pool */
    pool = pj_pool_create(&cp.factory, THIS_FILE, 512, 512, NULL);
    if (pool == NULL) {
    	app_perror("pj_pool_create()", status);
        pj_caching_pool_destroy(&cp);
    	pj_shutdown();
    	return status;
    }

    /* Init delay buffer */
    status = pjmedia_delay_buf_create(pool, THIS_FILE, CLOCK_RATE,
				      SAMPLES_PER_FRAME, CHANNEL_COUNT,
				      0, 0, &delaybuf);
    if (status != PJ_SUCCESS) {
    	app_perror("pjmedia_delay_buf_create()", status);
        //pj_caching_pool_destroy(&cp);
    	//pj_shutdown();
    	//return status;
    }

    return PJ_SUCCESS;
}


/* Sound capture callback */
static pj_status_t rec_cb(void *user_data,
			  pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(user_data);

    if (param.ext_fmt.id == PJMEDIA_FORMAT_PCM) {
	pjmedia_delay_buf_put(delaybuf, (pj_int16_t*)frame->buf);
    
	if (frame->size != SAMPLES_PER_FRAME*2) {
		    PJ_LOG(3, (THIS_FILE, "Size captured = %u",
			       frame->size));
	}
    } else {
	pjmedia_frame_ext *f_src = (pjmedia_frame_ext*)frame;
	pjmedia_frame_ext *f_dst = (pjmedia_frame_ext*)frame_buf;
	
	copy_frame_ext(f_dst, f_src);
    }

    ++rec_cnt;
    return PJ_SUCCESS;
}

/* Play cb */
static pj_status_t play_cb(void *user_data,
			   pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(user_data);

    if (param.ext_fmt.id == PJMEDIA_FORMAT_PCM) {
	pjmedia_delay_buf_get(delaybuf, (pj_int16_t*)frame->buf);
	frame->size = SAMPLES_PER_FRAME*2;
	frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
    } else {
	pjmedia_frame_ext *f_src = (pjmedia_frame_ext*)frame_buf;
	pjmedia_frame_ext *f_dst = (pjmedia_frame_ext*)frame;

	copy_frame_ext(f_dst, f_src);
    }

    ++play_cnt;
    return PJ_SUCCESS;
}

/* Start sound */
static pj_status_t snd_start(unsigned flag)
{
    pj_status_t status;

    if (strm != NULL) {
    	app_perror("snd already open", PJ_EINVALIDOP);
    	return PJ_EINVALIDOP;
    }

    pjmedia_aud_dev_default_param(0, &param);
    param.channel_count = CHANNEL_COUNT;
    param.clock_rate = CLOCK_RATE;
    param.samples_per_frame = SAMPLES_PER_FRAME;
    param.dir = (pjmedia_dir) flag;
    param.ext_fmt.id = PJMEDIA_FORMAT_AMR;
    param.ext_fmt.bitrate = 12200;
    param.output_route = PJMEDIA_AUD_DEV_ROUTE_LOUDSPEAKER;

    status = pjmedia_aud_stream_create(&param, &rec_cb, &play_cb, NULL, &strm);
    if (status != PJ_SUCCESS) {
    	app_perror("snd open", status);
    	return status;
    }

    rec_cnt = play_cnt = 0;
    pj_gettimeofday(&t_start);

    pjmedia_delay_buf_reset(delaybuf);

    status = pjmedia_aud_stream_start(strm);
    if (status != PJ_SUCCESS) {
    	app_perror("snd start", status);
    	pjmedia_aud_stream_destroy(strm);
    	strm = NULL;
    	return status;
    }

    return PJ_SUCCESS;
}

/* Stop sound */
static pj_status_t snd_stop()
{
    pj_time_val now;
    pj_status_t status;

    if (strm == NULL) {
    	app_perror("snd not open", PJ_EINVALIDOP);
    	return PJ_EINVALIDOP;
    }

    status = pjmedia_aud_stream_stop(strm);
    if (status != PJ_SUCCESS) {
    	app_perror("snd failed to stop", status);
    }
    status = pjmedia_aud_stream_destroy(strm);
    strm = NULL;

    pj_gettimeofday(&now);
    PJ_TIME_VAL_SUB(now, t_start);

    PJ_LOG(3,(THIS_FILE, "Duration: %d.%03d", now.sec, now.msec));
    PJ_LOG(3,(THIS_FILE, "Captured: %d", rec_cnt));
    PJ_LOG(3,(THIS_FILE, "Played: %d", play_cnt));

    return status;
}

/* Shutdown application */
static void app_fini()
{
    if (strm)
    	snd_stop();

    pjmedia_aud_subsys_shutdown();
    pjmedia_delay_buf_destroy(delaybuf);
    pj_pool_release(pool);
    pj_caching_pool_destroy(&cp);
    pj_shutdown();
}


////////////////////////////////////////////////////////////////////////////
/*
 * The interractive console UI
 */
#include <e32base.h>

class ConsoleUI : public CActive
{
public:
    ConsoleUI(CConsoleBase *con);

    // Run console UI
    void Run();

    // Stop
    void Stop();

protected:
    // Cancel asynchronous read.
    void DoCancel();

    // Implementation: called when read has completed.
    void RunL();

private:
    CConsoleBase *con_;
};


ConsoleUI::ConsoleUI(CConsoleBase *con)
: CActive(EPriorityUserInput), con_(con)
{
    CActiveScheduler::Add(this);
}

// Run console UI
void ConsoleUI::Run()
{
    con_->Read(iStatus);
    SetActive();
}

// Stop console UI
void ConsoleUI::Stop()
{
    DoCancel();
}

// Cancel asynchronous read.
void ConsoleUI::DoCancel()
{
    con_->ReadCancel();
}

static void PrintMenu()
{
    PJ_LOG(3, (THIS_FILE, "\n\n"
	    "Menu:\n"
	    "  a    Start bidir sound\n"
	    "  t    Start recorder\n"
	    "  p    Start player\n"
	    "  d    Stop & close sound\n"
	    "  w    Quit\n"));
}

// Implementation: called when read has completed.
void ConsoleUI::RunL()
{
    TKeyCode kc = con_->KeyCode();
    pj_bool_t reschedule = PJ_TRUE;

    switch (kc) {
    case 'w':
	    snd_stop();
	    CActiveScheduler::Stop();
	    reschedule = PJ_FALSE;
	    break;
    case 'a':
    	snd_start(PJMEDIA_DIR_CAPTURE_PLAYBACK);
	break;
    case 't':
    	snd_start(PJMEDIA_DIR_CAPTURE);
	break;
    case 'p':
    	snd_start(PJMEDIA_DIR_PLAYBACK);
    break;
    case 'd':
    	snd_stop();
	break;
    default:
	    PJ_LOG(3,(THIS_FILE, "Keycode '%c' (%d) is pressed",
		      kc, kc));
	    break;
    }

    PrintMenu();

    if (reschedule)
	Run();
}


////////////////////////////////////////////////////////////////////////////
int app_main()
{
    if (app_init() != PJ_SUCCESS)
        return -1;

    // Run the UI
    ConsoleUI *con = new ConsoleUI(console);

    con->Run();

    PrintMenu();
    CActiveScheduler::Start();

    delete con;

    app_fini();
    return 0;
}

