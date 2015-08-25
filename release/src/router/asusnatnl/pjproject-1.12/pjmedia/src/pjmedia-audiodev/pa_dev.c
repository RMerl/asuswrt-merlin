/* $Id: pa_dev.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia-audiodev/audiodev_imp.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>
#include <pj/string.h>

#if PJMEDIA_AUDIO_DEV_HAS_PORTAUDIO

#include <portaudio.h>

#define THIS_FILE	"pa_dev.c"
#define DRIVER_NAME	"PA"

/* Enable call to PaUtil_SetDebugPrintFunction, but this is not always
 * available across all PortAudio versions (?)
 */
/*#define USE_PA_DEBUG_PRINT */

struct pa_aud_factory
{
    pjmedia_aud_dev_factory	 base;
    pj_pool_factory		*pf;
    pj_pool_t			*pool;
};


/* 
 * Sound stream descriptor.
 * This struct may be used for both unidirectional or bidirectional sound
 * streams.
 */
struct pa_aud_stream
{
    pjmedia_aud_stream	 base;

    pj_pool_t		*pool;
    pj_str_t		 name;
    pjmedia_dir		 dir;
    int			 play_id;
    int			 rec_id;
    int			 bytes_per_sample;
    pj_uint32_t		 samples_per_sec;
    unsigned		 samples_per_frame;
    int			 channel_count;

    PaStream		*rec_strm;
    PaStream		*play_strm;

    void		*user_data;
    pjmedia_aud_rec_cb   rec_cb;
    pjmedia_aud_play_cb  play_cb;

    pj_timestamp	 play_timestamp;
    pj_timestamp	 rec_timestamp;
    pj_uint32_t		 underflow;
    pj_uint32_t		 overflow;

    pj_bool_t		 quit_flag;

    pj_bool_t		 rec_thread_exited;
    pj_bool_t		 rec_thread_initialized;
    pj_thread_desc	 rec_thread_desc;
    pj_thread_t		*rec_thread;

    pj_bool_t		 play_thread_exited;
    pj_bool_t		 play_thread_initialized;
    pj_thread_desc	 play_thread_desc;
    pj_thread_t		*play_thread;

    /* Sometime the record callback does not return framesize as configured
     * (e.g: in OSS), while this module must guarantee returning framesize
     * as configured in the creation settings. In this case, we need a buffer 
     * for the recorded samples.
     */
    pj_int16_t		*rec_buf;
    unsigned		 rec_buf_count;

    /* Sometime the player callback does not request framesize as configured
     * (e.g: in Linux OSS) while sound device will always get samples from 
     * the other component as many as configured samples_per_frame. 
     */
    pj_int16_t		*play_buf;
    unsigned		 play_buf_count;
};


/* Factory prototypes */
static pj_status_t  pa_init(pjmedia_aud_dev_factory *f);
static pj_status_t  pa_destroy(pjmedia_aud_dev_factory *f);
static pj_status_t  pa_refresh(pjmedia_aud_dev_factory *f);
static unsigned	    pa_get_dev_count(pjmedia_aud_dev_factory *f);
static pj_status_t  pa_get_dev_info(pjmedia_aud_dev_factory *f, 
				    unsigned index,
				    pjmedia_aud_dev_info *info);
static pj_status_t  pa_default_param(pjmedia_aud_dev_factory *f,
				     unsigned index,
				     pjmedia_aud_param *param);
static pj_status_t  pa_create_stream(pjmedia_aud_dev_factory *f,
				     const pjmedia_aud_param *param,
				     pjmedia_aud_rec_cb rec_cb,
				     pjmedia_aud_play_cb play_cb,
				     void *user_data,
				     pjmedia_aud_stream **p_aud_strm);

/* Stream prototypes */
static pj_status_t strm_get_param(pjmedia_aud_stream *strm,
				  pjmedia_aud_param *param);
static pj_status_t strm_get_cap(pjmedia_aud_stream *strm,
	 		        pjmedia_aud_dev_cap cap,
			        void *value);
static pj_status_t strm_set_cap(pjmedia_aud_stream *strm,
			        pjmedia_aud_dev_cap cap,
			        const void *value);
static pj_status_t strm_start(pjmedia_aud_stream *strm);
static pj_status_t strm_stop(pjmedia_aud_stream *strm);
static pj_status_t strm_destroy(pjmedia_aud_stream *strm);


static pjmedia_aud_dev_factory_op pa_op = 
{
    &pa_init,
    &pa_destroy,
    &pa_get_dev_count,
    &pa_get_dev_info,
    &pa_default_param,
    &pa_create_stream,
    &pa_refresh    
};

static pjmedia_aud_stream_op pa_strm_op = 
{
    &strm_get_param,
    &strm_get_cap,
    &strm_set_cap,
    &strm_start,
    &strm_stop,
    &strm_destroy
};



static int PaRecorderCallback(const void *input, 
			      void *output,
			      unsigned long frameCount,
			      const PaStreamCallbackTimeInfo* timeInfo,
			      PaStreamCallbackFlags statusFlags,
			      void *userData )
{
    struct pa_aud_stream *stream = (struct pa_aud_stream*) userData;
    pj_status_t status = 0;
    unsigned nsamples;

    PJ_UNUSED_ARG(output);
    PJ_UNUSED_ARG(timeInfo);

    if (stream->quit_flag)
	goto on_break;

    if (input == NULL)
	return paContinue;

    /* Known cases of callback's thread:
     * - The thread may be changed in the middle of a session, e.g: in MacOS 
     *   it happens when plugging/unplugging headphone.
     * - The same thread may be reused in consecutive sessions. The first
     *   session will leave TLS set, but release the TLS data address,
     *   so the second session must re-register the callback's thread.
     */
    if (stream->rec_thread_initialized == 0 || !pj_thread_is_registered(0))
    {
	pj_bzero(stream->rec_thread_desc, sizeof(pj_thread_desc));
	status = pj_thread_register(0, "pa_rec", stream->rec_thread_desc,
				    &stream->rec_thread);
	stream->rec_thread_initialized = 1;
	PJ_LOG(5,(THIS_FILE, "Recorder thread started"));
    }

    if (statusFlags & paInputUnderflow)
	++stream->underflow;
    if (statusFlags & paInputOverflow)
	++stream->overflow;

    /* Calculate number of samples we've got */
    nsamples = frameCount * stream->channel_count + stream->rec_buf_count;

    if (nsamples >= stream->samples_per_frame) 
    {
	/* If buffer is not empty, combine the buffer with the just incoming
	 * samples, then call put_frame.
	 */
	if (stream->rec_buf_count) {
	    unsigned chunk_count = 0;
	    pjmedia_frame frame;
	
	    chunk_count = stream->samples_per_frame - stream->rec_buf_count;
	    pjmedia_copy_samples(stream->rec_buf + stream->rec_buf_count,
				 (pj_int16_t*)input, chunk_count);

	    frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
	    frame.buf = (void*) stream->rec_buf;
	    frame.size = stream->samples_per_frame * stream->bytes_per_sample;
	    frame.timestamp.u64 = stream->rec_timestamp.u64;
	    frame.bit_info = 0;

	    status = (*stream->rec_cb)(stream->user_data, &frame);

	    input = (pj_int16_t*) input + chunk_count;
	    nsamples -= stream->samples_per_frame;
	    stream->rec_buf_count = 0;
	    stream->rec_timestamp.u64 += stream->samples_per_frame /
					 stream->channel_count;
	}

	/* Give all frames we have */
	while (nsamples >= stream->samples_per_frame && status == 0) {
	    pjmedia_frame frame;

	    frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
	    frame.buf = (void*) input;
	    frame.size = stream->samples_per_frame * stream->bytes_per_sample;
	    frame.timestamp.u64 = stream->rec_timestamp.u64;
	    frame.bit_info = 0;

	    status = (*stream->rec_cb)(stream->user_data, &frame);

	    input = (pj_int16_t*) input + stream->samples_per_frame;
	    nsamples -= stream->samples_per_frame;
	    stream->rec_timestamp.u64 += stream->samples_per_frame /
					 stream->channel_count;
	}

	/* Store the remaining samples into the buffer */
	if (nsamples && status == 0) {
	    stream->rec_buf_count = nsamples;
	    pjmedia_copy_samples(stream->rec_buf, (pj_int16_t*)input, 
			         nsamples);
	}

    } else {
	/* Not enough samples, let's just store them in the buffer */
	pjmedia_copy_samples(stream->rec_buf + stream->rec_buf_count,
			     (pj_int16_t*)input, 
			     frameCount * stream->channel_count);
	stream->rec_buf_count += frameCount * stream->channel_count;
    }

    if (status==0) 
	return paContinue;

on_break:
    stream->rec_thread_exited = 1;
    return paAbort;
}

static int PaPlayerCallback( const void *input, 
			     void *output,
			     unsigned long frameCount,
			     const PaStreamCallbackTimeInfo* timeInfo,
			     PaStreamCallbackFlags statusFlags,
			     void *userData )
{
    struct pa_aud_stream *stream = (struct pa_aud_stream*) userData;
    pj_status_t status = 0;
    unsigned nsamples_req = frameCount * stream->channel_count;

    PJ_UNUSED_ARG(input);
    PJ_UNUSED_ARG(timeInfo);

    if (stream->quit_flag)
	goto on_break;

    if (output == NULL)
	return paContinue;

    /* Known cases of callback's thread:
     * - The thread may be changed in the middle of a session, e.g: in MacOS 
     *   it happens when plugging/unplugging headphone.
     * - The same thread may be reused in consecutive sessions. The first
     *   session will leave TLS set, but release the TLS data address,
     *   so the second session must re-register the callback's thread.
     */
    if (stream->play_thread_initialized == 0 || !pj_thread_is_registered(0))
    {
	pj_bzero(stream->play_thread_desc, sizeof(pj_thread_desc));
	status = pj_thread_register(0, "portaudio", stream->play_thread_desc,
				    &stream->play_thread);
	stream->play_thread_initialized = 1;
	PJ_LOG(5,(THIS_FILE, "Player thread started"));
    }

    if (statusFlags & paOutputUnderflow)
	++stream->underflow;
    if (statusFlags & paOutputOverflow)
	++stream->overflow;


    /* Check if any buffered samples */
    if (stream->play_buf_count) {
	/* samples buffered >= requested by sound device */
	if (stream->play_buf_count >= nsamples_req) {
	    pjmedia_copy_samples((pj_int16_t*)output, stream->play_buf, 
				 nsamples_req);
	    stream->play_buf_count -= nsamples_req;
	    pjmedia_move_samples(stream->play_buf, 
				 stream->play_buf + nsamples_req,
				 stream->play_buf_count);
	    nsamples_req = 0;
	    
	    return paContinue;
	}

	/* samples buffered < requested by sound device */
	pjmedia_copy_samples((pj_int16_t*)output, stream->play_buf, 
			     stream->play_buf_count);
	nsamples_req -= stream->play_buf_count;
	output = (pj_int16_t*)output + stream->play_buf_count;
	stream->play_buf_count = 0;
    }

    /* Fill output buffer as requested */
    while (nsamples_req && status == 0) {
	if (nsamples_req >= stream->samples_per_frame) {
	    pjmedia_frame frame;

	    frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
	    frame.buf = output;
	    frame.size = stream->samples_per_frame *  stream->bytes_per_sample;
	    frame.timestamp.u64 = stream->play_timestamp.u64;
	    frame.bit_info = 0;

	    status = (*stream->play_cb)(stream->user_data, &frame);
	    if (status != PJ_SUCCESS)
		goto on_break;

	    if (frame.type != PJMEDIA_FRAME_TYPE_AUDIO)
		pj_bzero(frame.buf, frame.size);

	    nsamples_req -= stream->samples_per_frame;
	    output = (pj_int16_t*)output + stream->samples_per_frame;
	} else {
	    pjmedia_frame frame;

	    frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
	    frame.buf = stream->play_buf;
	    frame.size = stream->samples_per_frame *  stream->bytes_per_sample;
	    frame.timestamp.u64 = stream->play_timestamp.u64;
	    frame.bit_info = 0;

	    status = (*stream->play_cb)(stream->user_data, &frame);
	    if (status != PJ_SUCCESS)
		goto on_break;

	    if (frame.type != PJMEDIA_FRAME_TYPE_AUDIO)
		pj_bzero(frame.buf, frame.size);

	    pjmedia_copy_samples((pj_int16_t*)output, stream->play_buf, 
				 nsamples_req);
	    stream->play_buf_count = stream->samples_per_frame - nsamples_req;
	    pjmedia_move_samples(stream->play_buf, 
				 stream->play_buf+nsamples_req,
				 stream->play_buf_count);
	    nsamples_req = 0;
	}

	stream->play_timestamp.u64 += stream->samples_per_frame /
				      stream->channel_count;
    }
    
    if (status==0) 
	return paContinue;

on_break:
    stream->play_thread_exited = 1;
    return paAbort;
}


static int PaRecorderPlayerCallback( const void *input, 
				     void *output,
				     unsigned long frameCount,
				     const PaStreamCallbackTimeInfo* timeInfo,
				     PaStreamCallbackFlags statusFlags,
				     void *userData )
{
    int rc;

    rc = PaRecorderCallback(input, output, frameCount, timeInfo,
			    statusFlags, userData);
    if (rc != paContinue)
	return rc;

    rc = PaPlayerCallback(input, output, frameCount, timeInfo,
			  statusFlags, userData);
    return rc;
}

#ifdef USE_PA_DEBUG_PRINT
/* Logging callback from PA */
static void pa_log_cb(const char *log)
{
    PJ_LOG(5,(THIS_FILE, "PA message: %s", log));
}

/* We should include pa_debugprint.h for this, but the header
 * is not available publicly. :(
 */
typedef void (*PaUtilLogCallback ) (const char *log);
void PaUtil_SetDebugPrintFunction(PaUtilLogCallback  cb);
#endif


/*
 * Init PortAudio audio driver.
 */
pjmedia_aud_dev_factory* pjmedia_pa_factory(pj_pool_factory *pf)
{
    struct pa_aud_factory *f;
    pj_pool_t *pool;

    pool = pj_pool_create(pf, "portaudio", 64, 64, NULL);
    f = PJ_POOL_ZALLOC_T(pool, struct pa_aud_factory);
    f->pf = pf;
    f->pool = pool;
    f->base.op = &pa_op;

    return &f->base;
}


/* API: Init factory */
static pj_status_t pa_init(pjmedia_aud_dev_factory *f)
{
    int err;

    PJ_UNUSED_ARG(f);

#ifdef USE_PA_DEBUG_PRINT
    PaUtil_SetDebugPrintFunction(&pa_log_cb);
#endif

    err = Pa_Initialize();

    PJ_LOG(4,(THIS_FILE, 
	      "PortAudio sound library initialized, status=%d", err));
    PJ_LOG(4,(THIS_FILE, "PortAudio host api count=%d",
			 Pa_GetHostApiCount()));
    PJ_LOG(4,(THIS_FILE, "Sound device count=%d",
			 pa_get_dev_count(f)));

    return err ? PJMEDIA_AUDIODEV_ERRNO_FROM_PORTAUDIO(err) : PJ_SUCCESS;
}


/* API: Destroy factory */
static pj_status_t pa_destroy(pjmedia_aud_dev_factory *f)
{
    struct pa_aud_factory *pa = (struct pa_aud_factory*)f;
    pj_pool_t *pool;
    int err;

    PJ_LOG(4,(THIS_FILE, "PortAudio sound library shutting down.."));

    err = Pa_Terminate();

    pool = pa->pool;
    pa->pool = NULL;
    pj_pool_release(pool);
    
    return err ? PJMEDIA_AUDIODEV_ERRNO_FROM_PORTAUDIO(err) : PJ_SUCCESS;
}


/* API: Refresh the device list. */
static pj_status_t pa_refresh(pjmedia_aud_dev_factory *f)
{
    PJ_UNUSED_ARG(f);
    return PJ_ENOTSUP;
}


/* API: Get device count. */
static unsigned	pa_get_dev_count(pjmedia_aud_dev_factory *f)
{
    int count = Pa_GetDeviceCount();
    PJ_UNUSED_ARG(f);
    return count < 0 ? 0 : count;
}


/* API: Get device info. */
static pj_status_t  pa_get_dev_info(pjmedia_aud_dev_factory *f, 
				    unsigned index,
				    pjmedia_aud_dev_info *info)
{
    const PaDeviceInfo *pa_info;

    PJ_UNUSED_ARG(f);

    pa_info = Pa_GetDeviceInfo(index);
    if (!pa_info)
	return PJMEDIA_EAUD_INVDEV;

    pj_bzero(info, sizeof(*info));
    strncpy(info->name, pa_info->name, sizeof(info->name));
    info->name[sizeof(info->name)-1] = '\0';
    info->input_count = pa_info->maxInputChannels;
    info->output_count = pa_info->maxOutputChannels;
    info->default_samples_per_sec = (unsigned)pa_info->defaultSampleRate;
    strncpy(info->driver, DRIVER_NAME, sizeof(info->driver));
    info->driver[sizeof(info->driver)-1] = '\0';
    info->caps = PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY |
		 PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY;

    return PJ_SUCCESS;
}


/* API: fill in with default parameter. */
static pj_status_t  pa_default_param(pjmedia_aud_dev_factory *f,
				     unsigned index,
				     pjmedia_aud_param *param)
{
    pjmedia_aud_dev_info adi;
    pj_status_t status;

    PJ_UNUSED_ARG(f);

    status = pa_get_dev_info(f, index, &adi);
    if (status != PJ_SUCCESS)
	return status;

    pj_bzero(param, sizeof(*param));
    if (adi.input_count && adi.output_count) {
	param->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
	param->rec_id = index;
	param->play_id = index;
    } else if (adi.input_count) {
	param->dir = PJMEDIA_DIR_CAPTURE;
	param->rec_id = index;
	param->play_id = PJMEDIA_AUD_INVALID_DEV;
    } else if (adi.output_count) {
	param->dir = PJMEDIA_DIR_PLAYBACK;
	param->play_id = index;
	param->rec_id = PJMEDIA_AUD_INVALID_DEV;
    } else {
	return PJMEDIA_EAUD_INVDEV;
    }

    param->clock_rate = adi.default_samples_per_sec;
    param->channel_count = 1;
    param->samples_per_frame = adi.default_samples_per_sec * 20 / 1000;
    param->bits_per_sample = 16;
    param->flags = adi.caps;
    param->input_latency_ms = PJMEDIA_SND_DEFAULT_REC_LATENCY;
    param->output_latency_ms = PJMEDIA_SND_DEFAULT_PLAY_LATENCY;

    return PJ_SUCCESS;
}


/* Internal: Get PortAudio default input device ID */
static int pa_get_default_input_dev(int channel_count)
{
    int i, count;

    /* Special for Windows - try to use the DirectSound implementation
     * first since it provides better latency.
     */
#if PJMEDIA_PREFER_DIRECT_SOUND
    if (Pa_HostApiTypeIdToHostApiIndex(paDirectSound) >= 0) {
	const PaHostApiInfo *pHI;
	int index = Pa_HostApiTypeIdToHostApiIndex(paDirectSound);
	pHI = Pa_GetHostApiInfo(index);
	if (pHI) {
	    const PaDeviceInfo *paDevInfo = NULL;
	    paDevInfo = Pa_GetDeviceInfo(pHI->defaultInputDevice);
	    if (paDevInfo && paDevInfo->maxInputChannels >= channel_count)
		return pHI->defaultInputDevice;
	}
    }
#endif

    /* Enumerate the host api's for the default devices, and return
     * the device with suitable channels.
     */
    count = Pa_GetHostApiCount();
    for (i=0; i < count; ++i) {
	const PaHostApiInfo *pHAInfo;

	pHAInfo = Pa_GetHostApiInfo(i);
	if (!pHAInfo)
	    continue;

	if (pHAInfo->defaultInputDevice >= 0) {
	    const PaDeviceInfo *paDevInfo;

	    paDevInfo = Pa_GetDeviceInfo(pHAInfo->defaultInputDevice);

	    if (paDevInfo->maxInputChannels >= channel_count)
		return pHAInfo->defaultInputDevice;
	}
    }

    /* If still no device is found, enumerate all devices */
    count = Pa_GetDeviceCount();
    for (i=0; i<count; ++i) {
	const PaDeviceInfo *paDevInfo;

	paDevInfo = Pa_GetDeviceInfo(i);
	if (paDevInfo->maxInputChannels >= channel_count)
	    return i;
    }
    
    return -1;
}

/* Internal: Get PortAudio default output device ID */
static int pa_get_default_output_dev(int channel_count)
{
    int i, count;

    /* Special for Windows - try to use the DirectSound implementation
     * first since it provides better latency.
     */
#if PJMEDIA_PREFER_DIRECT_SOUND
    if (Pa_HostApiTypeIdToHostApiIndex(paDirectSound) >= 0) {
	const PaHostApiInfo *pHI;
	int index = Pa_HostApiTypeIdToHostApiIndex(paDirectSound);
	pHI = Pa_GetHostApiInfo(index);
	if (pHI) {
	    const PaDeviceInfo *paDevInfo = NULL;
	    paDevInfo = Pa_GetDeviceInfo(pHI->defaultOutputDevice);
	    if (paDevInfo && paDevInfo->maxOutputChannels >= channel_count)
		return pHI->defaultOutputDevice;
	}
    }
#endif

    /* Enumerate the host api's for the default devices, and return
     * the device with suitable channels.
     */
    count = Pa_GetHostApiCount();
    for (i=0; i < count; ++i) {
	const PaHostApiInfo *pHAInfo;

	pHAInfo = Pa_GetHostApiInfo(i);
	if (!pHAInfo)
	    continue;

	if (pHAInfo->defaultOutputDevice >= 0) {
	    const PaDeviceInfo *paDevInfo;

	    paDevInfo = Pa_GetDeviceInfo(pHAInfo->defaultOutputDevice);

	    if (paDevInfo->maxOutputChannels >= channel_count)
		return pHAInfo->defaultOutputDevice;
	}
    }

    /* If still no device is found, enumerate all devices */
    count = Pa_GetDeviceCount();
    for (i=0; i<count; ++i) {
	const PaDeviceInfo *paDevInfo;

	paDevInfo = Pa_GetDeviceInfo(i);
	if (paDevInfo->maxOutputChannels >= channel_count)
	    return i;
    }

    return -1;
}


/* Internal: create capture/recorder stream */
static pj_status_t create_rec_stream( struct pa_aud_factory *pa,
				      const pjmedia_aud_param *param,
				      pjmedia_aud_rec_cb rec_cb,
				      void *user_data,
				      pjmedia_aud_stream **p_snd_strm)
{
    pj_pool_t *pool;
    pjmedia_aud_dev_index rec_id;
    struct pa_aud_stream *stream;
    PaStreamParameters inputParam;
    int sampleFormat;
    const PaDeviceInfo *paDevInfo = NULL;
    const PaHostApiInfo *paHostApiInfo = NULL;
    unsigned paFrames, paRate, paLatency;
    const PaStreamInfo *paSI;
    PaError err;

    PJ_ASSERT_RETURN(rec_cb && p_snd_strm, PJ_EINVAL);

    rec_id = param->rec_id;
    if (rec_id < 0) {
	rec_id = pa_get_default_input_dev(param->channel_count);
	if (rec_id < 0) {
	    /* No such device. */
	    return PJMEDIA_EAUD_NODEFDEV;
	}
    }

    paDevInfo = Pa_GetDeviceInfo(rec_id);
    if (!paDevInfo) {
	/* Assumed it is "No such device" error. */
	return PJMEDIA_EAUD_INVDEV;
    }

    if (param->bits_per_sample == 8)
	sampleFormat = paUInt8;
    else if (param->bits_per_sample == 16)
	sampleFormat = paInt16;
    else if (param->bits_per_sample == 32)
	sampleFormat = paInt32;
    else
	return PJMEDIA_EAUD_SAMPFORMAT;
    
    pool = pj_pool_create(pa->pf, "recstrm", 1024, 1024, NULL);
    if (!pool)
	return PJ_ENOMEM;

    stream = PJ_POOL_ZALLOC_T(pool, struct pa_aud_stream);
    stream->pool = pool;
    pj_strdup2_with_null(pool, &stream->name, paDevInfo->name);
    stream->dir = PJMEDIA_DIR_CAPTURE;
    stream->rec_id = rec_id;
    stream->play_id = -1;
    stream->user_data = user_data;
    stream->samples_per_sec = param->clock_rate;
    stream->samples_per_frame = param->samples_per_frame;
    stream->bytes_per_sample = param->bits_per_sample / 8;
    stream->channel_count = param->channel_count;
    stream->rec_cb = rec_cb;

    stream->rec_buf = (pj_int16_t*)pj_pool_alloc(pool, 
		      stream->samples_per_frame * stream->bytes_per_sample);
    stream->rec_buf_count = 0;

    pj_bzero(&inputParam, sizeof(inputParam));
    inputParam.device = rec_id;
    inputParam.channelCount = param->channel_count;
    inputParam.hostApiSpecificStreamInfo = NULL;
    inputParam.sampleFormat = sampleFormat;
    if (param->flags & PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY)
	inputParam.suggestedLatency = param->input_latency_ms / 1000.0;
    else
	inputParam.suggestedLatency = PJMEDIA_SND_DEFAULT_REC_LATENCY / 1000.0;

    paHostApiInfo = Pa_GetHostApiInfo(paDevInfo->hostApi);

    /* Frames in PortAudio is number of samples in a single channel */
    paFrames = param->samples_per_frame / param->channel_count;

    err = Pa_OpenStream( &stream->rec_strm, &inputParam, NULL,
			 param->clock_rate, paFrames, 
			 paClipOff, &PaRecorderCallback, stream );
    if (err != paNoError) {
	pj_pool_release(pool);
	return PJMEDIA_AUDIODEV_ERRNO_FROM_PORTAUDIO(err);
    }

    paSI = Pa_GetStreamInfo(stream->rec_strm);
    paRate = (unsigned)paSI->sampleRate;
    paLatency = (unsigned)(paSI->inputLatency * 1000);

    PJ_LOG(5,(THIS_FILE, "Opened device %s (%s) for recording, sample "
			 "rate=%d, ch=%d, "
			 "bits=%d, %d samples per frame, latency=%d ms",
			 paDevInfo->name, paHostApiInfo->name,
			 paRate, param->channel_count,
			 param->bits_per_sample, param->samples_per_frame,
			 paLatency));

    *p_snd_strm = &stream->base;
    return PJ_SUCCESS;
}


/* Internal: create playback stream */
static pj_status_t create_play_stream(struct pa_aud_factory *pa,
				      const pjmedia_aud_param *param,
				      pjmedia_aud_play_cb play_cb,
				      void *user_data,
				      pjmedia_aud_stream **p_snd_strm)
{
    pj_pool_t *pool;
    pjmedia_aud_dev_index play_id;
    struct pa_aud_stream *stream;
    PaStreamParameters outputParam;
    int sampleFormat;
    const PaDeviceInfo *paDevInfo = NULL;
    const PaHostApiInfo *paHostApiInfo = NULL;
    const PaStreamInfo *paSI;
    unsigned paFrames, paRate, paLatency;
    PaError err;

    PJ_ASSERT_RETURN(play_cb && p_snd_strm, PJ_EINVAL);

    play_id = param->play_id;
    if (play_id < 0) {
	play_id = pa_get_default_output_dev(param->channel_count);
	if (play_id < 0) {
	    /* No such device. */
	    return PJMEDIA_EAUD_NODEFDEV;
	}
    } 

    paDevInfo = Pa_GetDeviceInfo(play_id);
    if (!paDevInfo) {
	/* Assumed it is "No such device" error. */
	return PJMEDIA_EAUD_INVDEV;
    }

    if (param->bits_per_sample == 8)
	sampleFormat = paUInt8;
    else if (param->bits_per_sample == 16)
	sampleFormat = paInt16;
    else if (param->bits_per_sample == 32)
	sampleFormat = paInt32;
    else
	return PJMEDIA_EAUD_SAMPFORMAT;
    
    pool = pj_pool_create(pa->pf, "playstrm", 1024, 1024, NULL);
    if (!pool)
	return PJ_ENOMEM;

    stream = PJ_POOL_ZALLOC_T(pool, struct pa_aud_stream);
    stream->pool = pool;
    pj_strdup2_with_null(pool, &stream->name, paDevInfo->name);
    stream->dir = PJMEDIA_DIR_PLAYBACK;
    stream->play_id = play_id;
    stream->rec_id = -1;
    stream->user_data = user_data;
    stream->samples_per_sec = param->clock_rate;
    stream->samples_per_frame = param->samples_per_frame;
    stream->bytes_per_sample = param->bits_per_sample / 8;
    stream->channel_count = param->channel_count;
    stream->play_cb = play_cb;

    stream->play_buf = (pj_int16_t*)pj_pool_alloc(pool, 
					    stream->samples_per_frame * 
					    stream->bytes_per_sample);
    stream->play_buf_count = 0;

    pj_bzero(&outputParam, sizeof(outputParam));
    outputParam.device = play_id;
    outputParam.channelCount = param->channel_count;
    outputParam.hostApiSpecificStreamInfo = NULL;
    outputParam.sampleFormat = sampleFormat;
    if (param->flags & PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY)
	outputParam.suggestedLatency=param->output_latency_ms / 1000.0;
    else
	outputParam.suggestedLatency=PJMEDIA_SND_DEFAULT_PLAY_LATENCY/1000.0;

    paHostApiInfo = Pa_GetHostApiInfo(paDevInfo->hostApi);

    /* Frames in PortAudio is number of samples in a single channel */
    paFrames = param->samples_per_frame / param->channel_count;

    err = Pa_OpenStream( &stream->play_strm, NULL, &outputParam,
			 param->clock_rate,  paFrames, 
			 paClipOff, &PaPlayerCallback, stream );
    if (err != paNoError) {
	pj_pool_release(pool);
	return PJMEDIA_AUDIODEV_ERRNO_FROM_PORTAUDIO(err);
    }

    paSI = Pa_GetStreamInfo(stream->play_strm);
    paRate = (unsigned)(paSI->sampleRate);
    paLatency = (unsigned)(paSI->outputLatency * 1000);

    PJ_LOG(5,(THIS_FILE, "Opened device %d: %s(%s) for playing, sample rate=%d"
			 ", ch=%d, "
			 "bits=%d, %d samples per frame, latency=%d ms",
			 play_id, paDevInfo->name, paHostApiInfo->name, 
			 paRate, param->channel_count,
		 	 param->bits_per_sample, param->samples_per_frame, 
			 paLatency));

    *p_snd_strm = &stream->base;

    return PJ_SUCCESS;
}


/* Internal: Create both player and recorder stream */
static pj_status_t create_bidir_stream(struct pa_aud_factory *pa,
				       const pjmedia_aud_param *param,
				       pjmedia_aud_rec_cb rec_cb,
				       pjmedia_aud_play_cb play_cb,
				       void *user_data,
				       pjmedia_aud_stream **p_snd_strm)
{
    pj_pool_t *pool;
    pjmedia_aud_dev_index rec_id, play_id;
    struct pa_aud_stream *stream;
    PaStream *paStream = NULL;
    PaStreamParameters inputParam;
    PaStreamParameters outputParam;
    int sampleFormat;
    const PaDeviceInfo *paRecDevInfo = NULL;
    const PaDeviceInfo *paPlayDevInfo = NULL;
    const PaHostApiInfo *paRecHostApiInfo = NULL;
    const PaHostApiInfo *paPlayHostApiInfo = NULL;
    const PaStreamInfo *paSI;
    unsigned paFrames, paRate, paInputLatency, paOutputLatency;
    PaError err;

    PJ_ASSERT_RETURN(play_cb && rec_cb && p_snd_strm, PJ_EINVAL);

    rec_id = param->rec_id;
    if (rec_id < 0) {
	rec_id = pa_get_default_input_dev(param->channel_count);
	if (rec_id < 0) {
	    /* No such device. */
	    return PJMEDIA_EAUD_NODEFDEV;
	}
    }

    paRecDevInfo = Pa_GetDeviceInfo(rec_id);
    if (!paRecDevInfo) {
	/* Assumed it is "No such device" error. */
	return PJMEDIA_EAUD_INVDEV;
    }

    play_id = param->play_id;
    if (play_id < 0) {
	play_id = pa_get_default_output_dev(param->channel_count);
	if (play_id < 0) {
	    /* No such device. */
	    return PJMEDIA_EAUD_NODEFDEV;
	}
    } 

    paPlayDevInfo = Pa_GetDeviceInfo(play_id);
    if (!paPlayDevInfo) {
	/* Assumed it is "No such device" error. */
	return PJMEDIA_EAUD_INVDEV;
    }


    if (param->bits_per_sample == 8)
	sampleFormat = paUInt8;
    else if (param->bits_per_sample == 16)
	sampleFormat = paInt16;
    else if (param->bits_per_sample == 32)
	sampleFormat = paInt32;
    else
	return PJMEDIA_EAUD_SAMPFORMAT;
    
    pool = pj_pool_create(pa->pf, "sndstream", 1024, 1024, NULL);
    if (!pool)
	return PJ_ENOMEM;

    stream = PJ_POOL_ZALLOC_T(pool, struct pa_aud_stream);
    stream->pool = pool;
    pj_strdup2_with_null(pool, &stream->name, paRecDevInfo->name);
    stream->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
    stream->play_id = play_id;
    stream->rec_id = rec_id;
    stream->user_data = user_data;
    stream->samples_per_sec = param->clock_rate;
    stream->samples_per_frame = param->samples_per_frame;
    stream->bytes_per_sample = param->bits_per_sample / 8;
    stream->channel_count = param->channel_count;
    stream->rec_cb = rec_cb;
    stream->play_cb = play_cb;

    stream->rec_buf = (pj_int16_t*)pj_pool_alloc(pool, 
		      stream->samples_per_frame * stream->bytes_per_sample);
    stream->rec_buf_count = 0;

    stream->play_buf = (pj_int16_t*)pj_pool_alloc(pool, 
		       stream->samples_per_frame * stream->bytes_per_sample);
    stream->play_buf_count = 0;

    pj_bzero(&inputParam, sizeof(inputParam));
    inputParam.device = rec_id;
    inputParam.channelCount = param->channel_count;
    inputParam.hostApiSpecificStreamInfo = NULL;
    inputParam.sampleFormat = sampleFormat;
    if (param->flags & PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY)
	inputParam.suggestedLatency = param->input_latency_ms / 1000.0;
    else
	inputParam.suggestedLatency = PJMEDIA_SND_DEFAULT_REC_LATENCY / 1000.0;

    paRecHostApiInfo = Pa_GetHostApiInfo(paRecDevInfo->hostApi);

    pj_bzero(&outputParam, sizeof(outputParam));
    outputParam.device = play_id;
    outputParam.channelCount = param->channel_count;
    outputParam.hostApiSpecificStreamInfo = NULL;
    outputParam.sampleFormat = sampleFormat;
    if (param->flags & PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY)
	outputParam.suggestedLatency=param->output_latency_ms / 1000.0;
    else
	outputParam.suggestedLatency=PJMEDIA_SND_DEFAULT_PLAY_LATENCY/1000.0;

    paPlayHostApiInfo = Pa_GetHostApiInfo(paPlayDevInfo->hostApi);

    /* Frames in PortAudio is number of samples in a single channel */
    paFrames = param->samples_per_frame / param->channel_count;

    /* If both input and output are on the same device, open a single stream
     * for both input and output.
     */
    if (rec_id == play_id) {
	err = Pa_OpenStream( &paStream, &inputParam, &outputParam,
			     param->clock_rate, paFrames, 
			     paClipOff, &PaRecorderPlayerCallback, stream );
	if (err == paNoError) {
	    /* Set play stream and record stream to the same stream */
	    stream->play_strm = stream->rec_strm = paStream;
	}
    } else {
	err = -1;
    }

    /* .. otherwise if input and output are on the same device, OR if we're
     * unable to open a bidirectional stream, then open two separate
     * input and output stream.
     */
    if (paStream == NULL) {
	/* Open input stream */
	err = Pa_OpenStream( &stream->rec_strm, &inputParam, NULL,
			     param->clock_rate, paFrames, 
			     paClipOff, &PaRecorderCallback, stream );
	if (err == paNoError) {
	    /* Open output stream */
	    err = Pa_OpenStream( &stream->play_strm, NULL, &outputParam,
				 param->clock_rate, paFrames, 
				 paClipOff, &PaPlayerCallback, stream );
	    if (err != paNoError)
		Pa_CloseStream(stream->rec_strm);
	}
    }

    if (err != paNoError) {
	pj_pool_release(pool);
	return PJMEDIA_AUDIODEV_ERRNO_FROM_PORTAUDIO(err);
    }

    paSI = Pa_GetStreamInfo(stream->rec_strm);
    paRate = (unsigned)(paSI->sampleRate);
    paInputLatency = (unsigned)(paSI->inputLatency * 1000);
    paSI = Pa_GetStreamInfo(stream->play_strm);
    paOutputLatency = (unsigned)(paSI->outputLatency * 1000);

    PJ_LOG(5,(THIS_FILE, "Opened device %s(%s)/%s(%s) for recording and "
			 "playback, sample rate=%d, ch=%d, "
			 "bits=%d, %d samples per frame, input latency=%d ms, "
			 "output latency=%d ms",
			 paRecDevInfo->name, paRecHostApiInfo->name,
			 paPlayDevInfo->name, paPlayHostApiInfo->name,
			 paRate, param->channel_count,
			 param->bits_per_sample, param->samples_per_frame,
			 paInputLatency, paOutputLatency));

    *p_snd_strm = &stream->base;

    return PJ_SUCCESS;
}


/* API: create stream */
static pj_status_t  pa_create_stream(pjmedia_aud_dev_factory *f,
				     const pjmedia_aud_param *param,
				     pjmedia_aud_rec_cb rec_cb,
				     pjmedia_aud_play_cb play_cb,
				     void *user_data,
				     pjmedia_aud_stream **p_aud_strm)
{
    struct pa_aud_factory *pa = (struct pa_aud_factory*)f;
    pj_status_t status;

    if (param->dir == PJMEDIA_DIR_CAPTURE) {
	status = create_rec_stream(pa, param, rec_cb, user_data, p_aud_strm);
    } else if (param->dir == PJMEDIA_DIR_PLAYBACK) {
	status = create_play_stream(pa, param, play_cb, user_data, p_aud_strm);
    } else if (param->dir == PJMEDIA_DIR_CAPTURE_PLAYBACK) {
	status = create_bidir_stream(pa, param, rec_cb, play_cb, user_data, 
				     p_aud_strm);
    } else {
	return PJ_EINVAL;
    }

    if (status != PJ_SUCCESS)
	return status;

    (*p_aud_strm)->op = &pa_strm_op;

    return PJ_SUCCESS;
}


/* API: Get stream parameters */
static pj_status_t strm_get_param(pjmedia_aud_stream *s,
				  pjmedia_aud_param *pi)
{
    struct pa_aud_stream *strm = (struct pa_aud_stream*)s;
    const PaStreamInfo *paPlaySI = NULL, *paRecSI = NULL;

    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);
    PJ_ASSERT_RETURN(strm->play_strm || strm->rec_strm, PJ_EINVALIDOP);

    if (strm->play_strm) {
	paPlaySI = Pa_GetStreamInfo(strm->play_strm);
    }
    if (strm->rec_strm) {
	paRecSI = Pa_GetStreamInfo(strm->rec_strm);
    }

    pj_bzero(pi, sizeof(*pi));
    pi->dir = strm->dir;
    pi->play_id = strm->play_id;
    pi->rec_id = strm->rec_id;
    pi->clock_rate = (unsigned)(paPlaySI ? paPlaySI->sampleRate : 
				paRecSI->sampleRate);
    pi->channel_count = strm->channel_count;
    pi->samples_per_frame = strm->samples_per_frame;
    pi->bits_per_sample = strm->bytes_per_sample * 8;
    if (paRecSI) {
	pi->flags |= PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY;
	pi->input_latency_ms = (unsigned)(paRecSI ? paRecSI->inputLatency * 
						    1000 : 0);
    }
    if (paPlaySI) {
	pi->flags |= PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY;
	pi->output_latency_ms = (unsigned)(paPlaySI? paPlaySI->outputLatency * 
						     1000 : 0);
    }

    return PJ_SUCCESS;
}


/* API: get capability */
static pj_status_t strm_get_cap(pjmedia_aud_stream *s,
	 		        pjmedia_aud_dev_cap cap,
			        void *pval)
{
    struct pa_aud_stream *strm = (struct pa_aud_stream*)s;

    PJ_ASSERT_RETURN(strm && pval, PJ_EINVAL);

    if (cap==PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY && strm->rec_strm) {
	const PaStreamInfo *si = Pa_GetStreamInfo(strm->rec_strm);
	if (!si)
	    return PJMEDIA_EAUD_SYSERR;

	*(unsigned*)pval = (unsigned)(si->inputLatency * 1000);
	return PJ_SUCCESS;
    } else if (cap==PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY && strm->play_strm) {
	const PaStreamInfo *si = Pa_GetStreamInfo(strm->play_strm);
	if (!si)
	    return PJMEDIA_EAUD_SYSERR;

	*(unsigned*)pval = (unsigned)(si->outputLatency * 1000);
	return PJ_SUCCESS;
    } else {
	return PJMEDIA_EAUD_INVCAP;
    }
}


/* API: set capability */
static pj_status_t strm_set_cap(pjmedia_aud_stream *strm,
			        pjmedia_aud_dev_cap cap,
			        const void *value)
{
    PJ_UNUSED_ARG(strm);
    PJ_UNUSED_ARG(cap);
    PJ_UNUSED_ARG(value);

    /* Nothing is supported */
    return PJMEDIA_EAUD_INVCAP;
}


/* API: start stream. */
static pj_status_t strm_start(pjmedia_aud_stream *s)
{
    struct pa_aud_stream *stream = (struct pa_aud_stream*)s;
    int err = 0;

    PJ_LOG(5,(THIS_FILE, "Starting %s stream..", stream->name.ptr));

    if (stream->play_strm)
	err = Pa_StartStream(stream->play_strm);

    if (err==0 && stream->rec_strm && stream->rec_strm != stream->play_strm) {
	err = Pa_StartStream(stream->rec_strm);
	if (err != 0)
	    Pa_StopStream(stream->play_strm);
    }

    PJ_LOG(5,(THIS_FILE, "Done, status=%d", err));

    return err ? PJMEDIA_AUDIODEV_ERRNO_FROM_PORTAUDIO(err) : PJ_SUCCESS;
}


/* API: stop stream. */
static pj_status_t strm_stop(pjmedia_aud_stream *s)
{
    struct pa_aud_stream *stream = (struct pa_aud_stream*)s;
    int i, err = 0;

    stream->quit_flag = 1;
    for (i=0; !stream->rec_thread_exited && i<100; ++i)
	pj_thread_sleep(10);
    for (i=0; !stream->play_thread_exited && i<100; ++i)
	pj_thread_sleep(10);

    pj_thread_sleep(1);

    PJ_LOG(5,(THIS_FILE, "Stopping stream.."));

    if (stream->play_strm)
	err = Pa_StopStream(stream->play_strm);

    if (stream->rec_strm && stream->rec_strm != stream->play_strm)
	err = Pa_StopStream(stream->rec_strm);

    stream->play_thread_initialized = 0;
    stream->rec_thread_initialized = 0;

    PJ_LOG(5,(THIS_FILE, "Done, status=%d", err));

    return err ? PJMEDIA_AUDIODEV_ERRNO_FROM_PORTAUDIO(err) : PJ_SUCCESS;
}


/* API: destroy stream. */
static pj_status_t strm_destroy(pjmedia_aud_stream *s)
{
    struct pa_aud_stream *stream = (struct pa_aud_stream*)s;
    int i, err = 0;

    stream->quit_flag = 1;
    for (i=0; !stream->rec_thread_exited && i<100; ++i) {
	pj_thread_sleep(1);
    }
    for (i=0; !stream->play_thread_exited && i<100; ++i) {
	pj_thread_sleep(1);
    }

    PJ_LOG(5,(THIS_FILE, "Closing %.*s: %lu underflow, %lu overflow",
			 (int)stream->name.slen,
			 stream->name.ptr,
			 stream->underflow, stream->overflow));

    if (stream->play_strm)
	err = Pa_CloseStream(stream->play_strm);

    if (stream->rec_strm && stream->rec_strm != stream->play_strm)
	err = Pa_CloseStream(stream->rec_strm);

    pj_pool_release(stream->pool);

    return err ? PJMEDIA_AUDIODEV_ERRNO_FROM_PORTAUDIO(err) : PJ_SUCCESS;
}

#endif	/* PJMEDIA_AUDIO_DEV_HAS_PORTAUDIO */

