/* $Id: coreaudio_dev.c 4079 2012-04-24 10:26:07Z ming $ */
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
#include <pjmedia-audiodev/audiodev_imp.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/os.h>

#if PJMEDIA_AUDIO_DEV_HAS_COREAUDIO

#include "TargetConditionals.h"
#if TARGET_OS_IPHONE
    #define COREAUDIO_MAC 0
#else
    #define COREAUDIO_MAC 1
#endif

#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioConverter.h>
#if !COREAUDIO_MAC
    #include <AudioToolbox/AudioServices.h>

    #define AudioDeviceID unsigned

    /**
     * As in iOS SDK 4 or later, audio route change property listener is
     * no longer necessary. Just make surethat yca_factory_initour application can receive
     * remote control events by adding the code:
     *     [[UIApplication sharedApplication] 
     *      beginReceivingRemoteControlEvents];
     * Otherwise audio route change (such as headset plug/unplug) will not be
     * processed while your application is in the background mode.
     */
    #define USE_AUDIO_ROUTE_CHANGE_PROP_LISTENER 0

#endif

/* For Mac OS 10.5.x and earlier */
#if AUDIO_UNIT_VERSION < 1060
    #define AudioComponent Component
    #define AudioComponentDescription ComponentDescription
    #define AudioComponentInstance ComponentInstance
    #define AudioComponentFindNext FindNextComponent
    #define AudioComponentInstanceNew OpenAComponent
    #define AudioComponentInstanceDispose CloseComponent
#endif


#define THIS_FILE		"coreaudio_dev.c"

/* coreaudio device info */
struct coreaudio_dev_info
{
    pjmedia_aud_dev_info	 info;
    AudioDeviceID		 dev_id;
};

/* linked list of streams */
struct stream_list
{
    PJ_DECL_LIST_MEMBER(struct stream_list);
    struct coreaudio_stream	*stream;
};

/* coreaudio factory */
struct coreaudio_factory
{
    pjmedia_aud_dev_factory	 base;
    pj_pool_t			*base_pool;
    pj_pool_t			*pool;
    pj_pool_factory		*pf;
    pj_mutex_t			*mutex;

    unsigned			 dev_count;
    struct coreaudio_dev_info	*dev_info;

    AudioComponent		 io_comp;
    struct stream_list		 streams;
};

/* Sound stream. */
struct coreaudio_stream
{
    pjmedia_aud_stream	 	 base;   	 /**< Base stream  	  */
    pjmedia_aud_param	 	 param;		 /**< Settings	          */
    pj_pool_t			*pool;           /**< Memory pool.        */
    struct coreaudio_factory	*cf;
    struct stream_list		 list_entry;

    pjmedia_aud_rec_cb   	 rec_cb;         /**< Capture callback.   */
    pjmedia_aud_play_cb  	 play_cb;        /**< Playback callback.  */
    void                	*user_data;      /**< Application data.   */

    pj_timestamp	 	 play_timestamp;
    pj_timestamp	 	 rec_timestamp;

    pj_int16_t			*rec_buf;
    unsigned		 	 rec_buf_count;
    pj_int16_t			*play_buf;
    unsigned		 	 play_buf_count;

    pj_bool_t			 interrupted;
    pj_bool_t		 	 quit_flag;
    pj_bool_t			 running;

    pj_bool_t		 	 rec_thread_initialized;
    pj_thread_desc	 	 rec_thread_desc;
    pj_thread_t			*rec_thread;

    pj_bool_t		 	 play_thread_initialized;
    pj_thread_desc	 	 play_thread_desc;
    pj_thread_t			*play_thread;

    AudioUnit		 	 io_units[2];
    AudioStreamBasicDescription  streamFormat;
    AudioBufferList		*audio_buf;

    AudioConverterRef            resample;
    pj_int16_t			*resample_buf;
    void			*resample_buf_ptr;
    unsigned		 	 resample_buf_count;
    unsigned		 	 resample_buf_size;
};

/* Static variable */
static struct coreaudio_factory *cf_instance = NULL;

/* Prototypes */
static pj_status_t ca_factory_init(pjmedia_aud_dev_factory *f);
static pj_status_t ca_factory_destroy(pjmedia_aud_dev_factory *f);
static pj_status_t ca_factory_refresh(pjmedia_aud_dev_factory *f);
static unsigned    ca_factory_get_dev_count(pjmedia_aud_dev_factory *f);
static pj_status_t ca_factory_get_dev_info(pjmedia_aud_dev_factory *f,
					   unsigned index,
					   pjmedia_aud_dev_info *info);
static pj_status_t ca_factory_default_param(pjmedia_aud_dev_factory *f,
					    unsigned index,
					    pjmedia_aud_param *param);
static pj_status_t ca_factory_create_stream(pjmedia_aud_dev_factory *f,
					    const pjmedia_aud_param *param,
					    pjmedia_aud_rec_cb rec_cb,
					    pjmedia_aud_play_cb play_cb,
					    void *user_data,
					    pjmedia_aud_stream **p_aud_strm);

static pj_status_t ca_stream_get_param(pjmedia_aud_stream *strm,
				       pjmedia_aud_param *param);
static pj_status_t ca_stream_get_cap(pjmedia_aud_stream *strm,
				     pjmedia_aud_dev_cap cap,
				     void *value);
static pj_status_t ca_stream_set_cap(pjmedia_aud_stream *strm,
				     pjmedia_aud_dev_cap cap,
				     const void *value);
static pj_status_t ca_stream_start(pjmedia_aud_stream *strm);
static pj_status_t ca_stream_stop(pjmedia_aud_stream *strm);
static pj_status_t ca_stream_destroy(pjmedia_aud_stream *strm);
static pj_status_t create_audio_unit(AudioComponent io_comp,
				     AudioDeviceID dev_id,
				     pjmedia_dir dir,
				     struct coreaudio_stream *strm,
				     AudioUnit *io_unit);
#if !COREAUDIO_MAC
static void interruptionListener(int inst_id, void *inClientData, UInt32 inInterruption);
static void propListener(void *                 inClientData,
                         AudioSessionPropertyID inID,
                         UInt32                 inDataSize,
                         const void *           inData);
#endif

/* Operations */
static pjmedia_aud_dev_factory_op factory_op =
{
    &ca_factory_init,
    &ca_factory_destroy,
    &ca_factory_get_dev_count,
    &ca_factory_get_dev_info,
    &ca_factory_default_param,
    &ca_factory_create_stream,
    &ca_factory_refresh
};

static pjmedia_aud_stream_op stream_op =
{
    &ca_stream_get_param,
    &ca_stream_get_cap,
    &ca_stream_set_cap,
    &ca_stream_start,
    &ca_stream_stop,
    &ca_stream_destroy
};


/****************************************************************************
 * Factory operations
 */
/*
 * Init coreaudio audio driver.
 */
pjmedia_aud_dev_factory* pjmedia_coreaudio_factory(pj_pool_factory *pf)
{
    struct coreaudio_factory *f;
    pj_pool_t *pool;

    pool = pj_pool_create(pf, "core audio base", 1000, 1000, NULL);
    f = PJ_POOL_ZALLOC_T(pool, struct coreaudio_factory);
    f->pf = pf;
    f->base_pool = pool;
    f->base.op = &factory_op;

    return &f->base;
}


/* API: init factory */
static pj_status_t ca_factory_init(pjmedia_aud_dev_factory *f)
{
    struct coreaudio_factory *cf = (struct coreaudio_factory*)f;
    AudioComponentDescription desc;
    pj_status_t status;
#if !COREAUDIO_MAC
    unsigned i;
    OSStatus ostatus;
#endif

    pj_list_init(&cf->streams);
    status = pj_mutex_create_recursive(cf->base_pool,
				       "coreaudio",
				       &cf->mutex);
    if (status != PJ_SUCCESS)
	return status;

    desc.componentType = kAudioUnitType_Output;
#if COREAUDIO_MAC
    desc.componentSubType = kAudioUnitSubType_HALOutput;
#else
    desc.componentSubType = kAudioUnitSubType_RemoteIO;
#endif
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    cf->io_comp = AudioComponentFindNext(NULL, &desc);
    if (cf->io_comp == NULL)
	return PJMEDIA_EAUD_INIT; // cannot find IO unit;

    status = ca_factory_refresh(f);
    if (status != PJ_SUCCESS)
	return status;

#if !COREAUDIO_MAC
    cf->pool = pj_pool_create(cf->pf, "core audio", 1000, 1000, NULL);
    cf->dev_count = 1;
    cf->dev_info = (struct coreaudio_dev_info*)
   		   pj_pool_calloc(cf->pool, cf->dev_count,
   		   sizeof(struct coreaudio_dev_info));
    for (i = 0; i < cf->dev_count; i++) {
 	struct coreaudio_dev_info *cdi;

 	cdi = &cf->dev_info[i];
 	pj_bzero(cdi, sizeof(*cdi));
 	cdi->dev_id = 0;
 	strcpy(cdi->info.name, "iPhone IO device");
 	strcpy(cdi->info.driver, "apple");
 	cdi->info.input_count = 1;
 	cdi->info.output_count = 1;
 	cdi->info.default_samples_per_sec = 8000;

	/* Set the device capabilities here */
 	cdi->info.caps = PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY |
			 PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY |
			 PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING |
			 PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE |
			 PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE |
			 PJMEDIA_AUD_DEV_CAP_EC;
 	cdi->info.routes = PJMEDIA_AUD_DEV_ROUTE_LOUDSPEAKER |
			   PJMEDIA_AUD_DEV_ROUTE_EARPIECE |
			   PJMEDIA_AUD_DEV_ROUTE_BLUETOOTH;

	PJ_LOG(4, (THIS_FILE, " dev_id %d: %s  (in=%d, out=%d) %dHz",
		   i,
		   cdi->info.name,
		   cdi->info.input_count,
		   cdi->info.output_count,
		   cdi->info.default_samples_per_sec));
    }

    /* Initialize the Audio Session */
    ostatus = AudioSessionInitialize(NULL, NULL, interruptionListener, NULL);
    if (ostatus != kAudioSessionNoError) {
	PJ_LOG(4, (THIS_FILE,
		   "Warning: cannot initialize audio session services (%i)",
		   ostatus));
    }

    /* Listen for audio routing change notifications. */
#if USE_AUDIO_ROUTE_CHANGE_PROP_LISTENER != 0
    ostatus = AudioSessionAddPropertyListener(
	          kAudioSessionProperty_AudioRouteChange,
		  propListener, cf);
    if (ostatus != kAudioSessionNoError) {
	PJ_LOG(4, (THIS_FILE,
		   "Warning: cannot listen for audio route change "
		   "notifications (%i)", ostatus));
    }
#endif

    cf_instance = cf;
#endif

    PJ_LOG(4, (THIS_FILE, "core audio initialized"));

    return PJ_SUCCESS;
}

/* API: destroy factory */
static pj_status_t ca_factory_destroy(pjmedia_aud_dev_factory *f)
{
    struct coreaudio_factory *cf = (struct coreaudio_factory*)f;
    pj_pool_t *pool;

    pj_assert(cf);
    pj_assert(cf->base_pool);
    pj_assert(pj_list_empty(&cf->streams));

#if !COREAUDIO_MAC
#if USE_AUDIO_ROUTE_CHANGE_PROP_LISTENER != 0
    AudioSessionRemovePropertyListenerWithUserData(
        kAudioSessionProperty_AudioRouteChange, propListener, cf);
#endif
#endif
    
    if (cf->pool) {
	pj_pool_release(cf->pool);
	cf->pool = NULL;
    }

    if (cf->mutex) {
	pj_mutex_lock(cf->mutex);
	cf_instance = NULL;
	pj_mutex_unlock(cf->mutex);
	pj_mutex_destroy(cf->mutex);
	cf->mutex = NULL;
    }

    pool = cf->base_pool;
    cf->base_pool = NULL;
    pj_pool_release(pool);

    return PJ_SUCCESS;
}

/* API: refresh the device list */
static pj_status_t ca_factory_refresh(pjmedia_aud_dev_factory *f)
{
#if !COREAUDIO_MAC
    /* iPhone doesn't support refreshing the device list */
    PJ_UNUSED_ARG(f);
    return PJ_SUCCESS;
#else
    struct coreaudio_factory *cf = (struct coreaudio_factory*)f;
    unsigned i;
    unsigned dev_count;
    AudioObjectPropertyAddress addr;
    AudioDeviceID *dev_ids;
    UInt32 buf_size, dev_size, size = sizeof(AudioDeviceID);
    AudioBufferList *buf = NULL;
    OSStatus ostatus;

    if (cf->pool != NULL) {
	pj_pool_release(cf->pool);
	cf->pool = NULL;
    }

    cf->dev_count = 0;
    cf->pool = pj_pool_create(cf->pf, "core audio", 1000, 1000, NULL);

    /* Find out how many audio devices there are */
    addr.mSelector = kAudioHardwarePropertyDevices;
    addr.mScope = kAudioObjectPropertyScopeGlobal;
    addr.mElement = kAudioObjectPropertyElementMaster;
    ostatus = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr,
                                             0, NULL, &dev_size);
    if (ostatus != noErr) {
	dev_size = 0;
    }

    /* Calculate the number of audio devices available */
    dev_count = dev_size / size;
    if (dev_count==0) {
  	PJ_LOG(4,(THIS_FILE, "core audio found no sound devices"));
  	/* Enabling this will cause pjsua-lib initialization to fail when
  	 * there is no sound device installed in the system, even when pjsua
  	 * has been run with --null-audio. Moreover, it might be better to
  	 * think that the core audio backend initialization is successful,
  	 * regardless there is no audio device installed, as later application
  	 * can check it using get_dev_count().
  	return PJMEDIA_EAUD_NODEV;
  	 */
  	return PJ_SUCCESS;
    }
    PJ_LOG(4, (THIS_FILE, "core audio detected %d devices",
	       dev_count));

    /* Get all the audio device IDs */
    dev_ids = (AudioDeviceID *)pj_pool_calloc(cf->pool, dev_size, size);
    if (!dev_ids)
	return PJ_ENOMEM;
    pj_bzero(dev_ids, dev_count);
    ostatus = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr,
					 0, NULL,
				         &dev_size, (void *)dev_ids);
    if (ostatus != noErr ) {
	/* This should not happen since we have successfully retrieved
	 * the property data size before
	 */
	return PJMEDIA_EAUD_INIT;
    }
    
    if (dev_size > 1) {
	AudioDeviceID dev_id = kAudioObjectUnknown;
	unsigned idx = 0;
	
	/* Find default audio input device */
	addr.mSelector = kAudioHardwarePropertyDefaultInputDevice;
	addr.mScope = kAudioObjectPropertyScopeGlobal;
	addr.mElement = kAudioObjectPropertyElementMaster;
	size = sizeof(dev_id);
	
	ostatus = AudioObjectGetPropertyData(kAudioObjectSystemObject,
					     &addr, 0, NULL,
					     &size, (void *)&dev_id);
	if (ostatus != noErr && dev_id != dev_ids[idx]) {
	    AudioDeviceID temp_id = dev_ids[idx];
	    
	    for (i = idx + 1; i < dev_size; i++) {
		if (dev_ids[i] == dev_id) {
		    dev_ids[idx++] = dev_id;
		    dev_ids[i] = temp_id;
		    break;
		}
	    }
	}

	/* Find default audio output device */
	addr.mSelector = kAudioHardwarePropertyDefaultOutputDevice;	
	ostatus = AudioObjectGetPropertyData(kAudioObjectSystemObject,
					     &addr, 0, NULL,
					     &size, (void *)&dev_id);
	if (ostatus != noErr && dev_id != dev_ids[idx]) {
	    AudioDeviceID temp_id = dev_ids[idx];
	    
	    for (i = idx + 1; i < dev_size; i++) {
		if (dev_ids[i] == dev_id) {
		    dev_ids[idx] = dev_id;
		    dev_ids[i] = temp_id;
		    break;
		}
	    }
	}
    }

    /* Build the devices' info */
    cf->dev_info = (struct coreaudio_dev_info*)
  		   pj_pool_calloc(cf->pool, dev_count,
  		   sizeof(struct coreaudio_dev_info));
    buf_size = 0;
    for (i = 0; i < dev_count; i++) {
	struct coreaudio_dev_info *cdi;
	Float64 sampleRate;

	cdi = &cf->dev_info[i];
	pj_bzero(cdi, sizeof(*cdi));
	cdi->dev_id = dev_ids[i];

	/* Get device name */
	addr.mSelector = kAudioDevicePropertyDeviceName;
	addr.mScope = kAudioObjectPropertyScopeGlobal;
	addr.mElement = kAudioObjectPropertyElementMaster;
	size = sizeof(cdi->info.name);
	AudioObjectGetPropertyData(cdi->dev_id, &addr,
				   0, NULL,
			           &size, (void *)cdi->info.name);

	strcpy(cdi->info.driver, "core audio");

        /* Get the number of input channels */
	addr.mSelector = kAudioDevicePropertyStreamConfiguration;
	addr.mScope = kAudioDevicePropertyScopeInput;
	size = 0;
	ostatus = AudioObjectGetPropertyDataSize(cdi->dev_id, &addr,
	                                         0, NULL, &size);
	if (ostatus == noErr && size > 0) {

	    if (size > buf_size) {
		buf = pj_pool_alloc(cf->pool, size);
		buf_size = size;
	    }
	    if (buf) {
		UInt32 idx;

		/* Get the input stream configuration */
		ostatus = AudioObjectGetPropertyData(cdi->dev_id, &addr,
						     0, NULL,
						     &size, buf);
		if (ostatus == noErr) {
		    /* Count the total number of input channels in
		     * the stream
		     */
		    for (idx = 0; idx < buf->mNumberBuffers; idx++) {
			cdi->info.input_count +=
			    buf->mBuffers[idx].mNumberChannels;
		    }
		}
	    }
	}

        /* Get the number of output channels */
	addr.mScope = kAudioDevicePropertyScopeOutput;
	size = 0;
	ostatus = AudioObjectGetPropertyDataSize(cdi->dev_id, &addr,
	                                         0, NULL, &size);
	if (ostatus == noErr && size > 0) {

	    if (size > buf_size) {
		buf = pj_pool_alloc(cf->pool, size);
		buf_size = size;
	    }
	    if (buf) {
		UInt32 idx;

		/* Get the output stream configuration */
		ostatus = AudioObjectGetPropertyData(cdi->dev_id, &addr,
						     0, NULL,
						     &size, buf);
		if (ostatus == noErr) {
		    /* Count the total number of output channels in
		     * the stream
		     */
		    for (idx = 0; idx < buf->mNumberBuffers; idx++) {
			cdi->info.output_count +=
			    buf->mBuffers[idx].mNumberChannels;
		    }
		}
	    }
	}

	/* Get default sample rate */
	addr.mSelector = kAudioDevicePropertyNominalSampleRate;
	addr.mScope = kAudioObjectPropertyScopeGlobal;
	size = sizeof(Float64);
	ostatus = AudioObjectGetPropertyData (cdi->dev_id, &addr,
		                              0, NULL,
		                              &size, &sampleRate);
	cdi->info.default_samples_per_sec = (ostatus == noErr ?
					    sampleRate:
					    16000);

	/* Set device capabilities here */
	if (cdi->info.input_count > 0) {
	    cdi->info.caps |= PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY;
	}
	if (cdi->info.output_count > 0) {
	    cdi->info.caps |= PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY;
	    addr.mSelector = kAudioDevicePropertyVolumeScalar;
	    addr.mScope = kAudioDevicePropertyScopeOutput;
	    if (AudioObjectHasProperty(cdi->dev_id, &addr)) {
		cdi->info.caps |= PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING;
	    }
	}

	cf->dev_count++;

	PJ_LOG(4, (THIS_FILE, " dev_id %d: %s  (in=%d, out=%d) %dHz",
	       i,
	       cdi->info.name,
	       cdi->info.input_count,
	       cdi->info.output_count,
	       cdi->info.default_samples_per_sec));
    }

    return PJ_SUCCESS;
#endif
}

/* API: get number of devices */
static unsigned ca_factory_get_dev_count(pjmedia_aud_dev_factory *f)
{
    struct coreaudio_factory *cf = (struct coreaudio_factory*)f;
    return cf->dev_count;
}

/* API: get device info */
static pj_status_t ca_factory_get_dev_info(pjmedia_aud_dev_factory *f,
					   unsigned index,
					   pjmedia_aud_dev_info *info)
{
    struct coreaudio_factory *cf = (struct coreaudio_factory*)f;

    PJ_ASSERT_RETURN(index < cf->dev_count, PJMEDIA_EAUD_INVDEV);

    pj_memcpy(info, &cf->dev_info[index].info, sizeof(*info));

    return PJ_SUCCESS;
}

/* API: create default device parameter */
static pj_status_t ca_factory_default_param(pjmedia_aud_dev_factory *f,
					    unsigned index,
					    pjmedia_aud_param *param)
{
    struct coreaudio_factory *cf = (struct coreaudio_factory*)f;
    struct coreaudio_dev_info *di = &cf->dev_info[index];

    PJ_ASSERT_RETURN(index < cf->dev_count, PJMEDIA_EAUD_INVDEV);

    pj_bzero(param, sizeof(*param));
    if (di->info.input_count && di->info.output_count) {
	param->dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
	param->rec_id = index;
	param->play_id = index;
    } else if (di->info.input_count) {
	param->dir = PJMEDIA_DIR_CAPTURE;
	param->rec_id = index;
	param->play_id = PJMEDIA_AUD_INVALID_DEV;
    } else if (di->info.output_count) {
	param->dir = PJMEDIA_DIR_PLAYBACK;
	param->play_id = index;
	param->rec_id = PJMEDIA_AUD_INVALID_DEV;
    } else {
	return PJMEDIA_EAUD_INVDEV;
    }

    /* Set the mandatory settings here */
    param->clock_rate = di->info.default_samples_per_sec;
    param->channel_count = 1;
    param->samples_per_frame = di->info.default_samples_per_sec * 20 / 1000;
    param->bits_per_sample = 16;

    /* Set the param for device capabilities here */
    param->flags = PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY |
		   PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY;
    param->input_latency_ms = PJMEDIA_SND_DEFAULT_REC_LATENCY;
    param->output_latency_ms = PJMEDIA_SND_DEFAULT_PLAY_LATENCY;

    return PJ_SUCCESS;
}

OSStatus resampleProc(AudioConverterRef             inAudioConverter,
		      UInt32                        *ioNumberDataPackets,
		      AudioBufferList               *ioData,
		      AudioStreamPacketDescription  **outDataPacketDescription,
		      void                          *inUserData)
{
    struct coreaudio_stream *strm = (struct coreaudio_stream*)inUserData;

    if (*ioNumberDataPackets > strm->resample_buf_size)
	*ioNumberDataPackets = strm->resample_buf_size;

    ioData->mNumberBuffers = 1;
    ioData->mBuffers[0].mNumberChannels = strm->streamFormat.mChannelsPerFrame;
    ioData->mBuffers[0].mData = strm->resample_buf_ptr;
    ioData->mBuffers[0].mDataByteSize = *ioNumberDataPackets *
					strm->streamFormat.mChannelsPerFrame *
					strm->param.bits_per_sample >> 3;

    return noErr;
}

static OSStatus resample_callback(void                       *inRefCon,
				  AudioUnitRenderActionFlags *ioActionFlags,
				  const AudioTimeStamp       *inTimeStamp,
				  UInt32                      inBusNumber,
				  UInt32                      inNumberFrames,
				  AudioBufferList            *ioData)
{
    struct coreaudio_stream *strm = (struct coreaudio_stream*)inRefCon;
    OSStatus ostatus;
    pj_status_t status = 0;
    unsigned nsamples;
    AudioBufferList *buf = strm->audio_buf;
    pj_int16_t *input;
    UInt32 resampleSize;

    pj_assert(!strm->quit_flag);

    /* Known cases of callback's thread:
     * - The thread may be changed in the middle of a session
     *   it happens when plugging/unplugging headphone.
     * - The same thread may be reused in consecutive sessions. The first
     *   session will leave TLS set, but release the TLS data address,
     *   so the second session must re-register the callback's thread.
     */
    if (strm->rec_thread_initialized == 0 || !pj_thread_is_registered(strm->pool->factory->inst_id))
    {
	pj_bzero(strm->rec_thread_desc, sizeof(pj_thread_desc));
	status = pj_thread_register(strm->pool->factory->inst_id, "ca_rec", strm->rec_thread_desc,
				    &strm->rec_thread);
	strm->rec_thread_initialized = 1;
	PJ_LOG(5,(THIS_FILE, "Recorder thread started, (%i frames)", 
		  inNumberFrames));
    }

    buf->mBuffers[0].mData = NULL;
    buf->mBuffers[0].mDataByteSize = inNumberFrames *
				     strm->streamFormat.mChannelsPerFrame;
    /* Render the unit to get input data */
    ostatus = AudioUnitRender(strm->io_units[0],
			      ioActionFlags,
			      inTimeStamp,
			      inBusNumber,
			      inNumberFrames,
			      buf);

    if (ostatus != noErr) {
	PJ_LOG(5, (THIS_FILE, "Core audio unit render error %i", ostatus));
	goto on_break;
    }
    input = (pj_int16_t *)buf->mBuffers[0].mData;

    resampleSize = strm->resample_buf_size;
    nsamples = inNumberFrames * strm->param.channel_count +
	       strm->resample_buf_count;

    if (nsamples >= resampleSize) {
	pjmedia_frame frame;
	UInt32 resampleOutput = strm->param.samples_per_frame /
				strm->streamFormat.mChannelsPerFrame;
	AudioBufferList ab;

	frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
	frame.buf = (void*) strm->rec_buf;
	frame.size = strm->param.samples_per_frame *
		     strm->param.bits_per_sample >> 3;
	frame.bit_info = 0;
	
	ab.mNumberBuffers = 1;
	ab.mBuffers[0].mNumberChannels = strm->streamFormat.mChannelsPerFrame;
	ab.mBuffers[0].mData = strm->rec_buf;
	ab.mBuffers[0].mDataByteSize = frame.size;

	/* If buffer is not empty, combine the buffer with the just incoming
	 * samples, then call put_frame.
	 */
	if (strm->resample_buf_count) {
	    unsigned chunk_count = resampleSize - strm->resample_buf_count;
	    pjmedia_copy_samples(strm->resample_buf + strm->resample_buf_count,
				 input, chunk_count);

	    /* Do the resample */

	    strm->resample_buf_ptr = strm->resample_buf;
	    ostatus = AudioConverterFillComplexBuffer(strm->resample,
						      resampleProc,
						      strm,
						      &resampleOutput,
						      &ab,
						      NULL);
	    if (ostatus != noErr) {
		goto on_break;
	    }
	    frame.timestamp.u64 = strm->rec_timestamp.u64;

	    status = (*strm->rec_cb)(strm->user_data, &frame);

	    input = input + chunk_count;
	    nsamples -= resampleSize;
	    strm->resample_buf_count = 0;
	    strm->rec_timestamp.u64 += strm->param.samples_per_frame /
				       strm->param.channel_count;
	}
	
	
 	/* Give all frames we have */
 	while (nsamples >= resampleSize && status == 0) {
 	    frame.timestamp.u64 = strm->rec_timestamp.u64;
	    
	    /* Do the resample */
	    strm->resample_buf_ptr = input;
	    ab.mBuffers[0].mDataByteSize = frame.size;
	    resampleOutput = strm->param.samples_per_frame /
			     strm->streamFormat.mChannelsPerFrame;
	    ostatus = AudioConverterFillComplexBuffer(strm->resample,
						      resampleProc,
						      strm,
						      &resampleOutput,
						      &ab,
						      NULL);
	    if (ostatus != noErr) {
		goto on_break;
	    }	    
	    
 	    status = (*strm->rec_cb)(strm->user_data, &frame);
	    
 	    input = (pj_int16_t*) input + resampleSize;
 	    nsamples -= resampleSize;
 	    strm->rec_timestamp.u64 += strm->param.samples_per_frame /
				       strm->param.channel_count;
 	}

	/* Store the remaining samples into the buffer */
	if (nsamples && status == 0) {
	    strm->resample_buf_count = nsamples;
	    pjmedia_copy_samples(strm->resample_buf, input,
				 nsamples);
	}

    } else {
	/* Not enough samples, let's just store them in the buffer */
	pjmedia_copy_samples(strm->resample_buf + strm->resample_buf_count,
			     input,
			     inNumberFrames * strm->param.channel_count);
	strm->resample_buf_count += inNumberFrames *
				    strm->param.channel_count;
    }

    return noErr;

on_break:
    return -1;
}

static OSStatus input_callback(void                       *inRefCon,
                               AudioUnitRenderActionFlags *ioActionFlags,
                               const AudioTimeStamp       *inTimeStamp,
                               UInt32                      inBusNumber,
                               UInt32                      inNumberFrames,
                               AudioBufferList            *ioData)
{
    struct coreaudio_stream *strm = (struct coreaudio_stream*)inRefCon;
    OSStatus ostatus;
    pj_status_t status = 0;
    unsigned nsamples;
    AudioBufferList *buf = strm->audio_buf;
    pj_int16_t *input;

    pj_assert(!strm->quit_flag);

    /* Known cases of callback's thread:
     * - The thread may be changed in the middle of a session
     *   it happens when plugging/unplugging headphone.
     * - The same thread may be reused in consecutive sessions. The first
     *   session will leave TLS set, but release the TLS data address,
     *   so the second session must re-register the callback's thread.
     */
    if (strm->rec_thread_initialized == 0 || !pj_thread_is_registered(strm->pool->factory->inst_id))
    {
	pj_bzero(strm->rec_thread_desc, sizeof(pj_thread_desc));
	status = pj_thread_register(strm->pool->factory->inst_id, "ca_rec", strm->rec_thread_desc,
				    &strm->rec_thread);
	strm->rec_thread_initialized = 1;
	PJ_LOG(5,(THIS_FILE, "Recorder thread started, (%i frames)",
		  inNumberFrames));
    }

    buf->mBuffers[0].mData = NULL;
    buf->mBuffers[0].mDataByteSize = inNumberFrames *
				     strm->streamFormat.mChannelsPerFrame;
    /* Render the unit to get input data */
    ostatus = AudioUnitRender(strm->io_units[0],
			      ioActionFlags,
			      inTimeStamp,
			      inBusNumber,
			      inNumberFrames,
			      buf);

    if (ostatus != noErr) {
	PJ_LOG(5, (THIS_FILE, "Core audio unit render error %i", ostatus));
	goto on_break;
    }
    input = (pj_int16_t *)buf->mBuffers[0].mData;

    /* Calculate number of samples we've got */
    nsamples = inNumberFrames * strm->param.channel_count +
	       strm->rec_buf_count;
    if (nsamples >= strm->param.samples_per_frame) {
	pjmedia_frame frame;

	frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
	frame.size = strm->param.samples_per_frame *
		     strm->param.bits_per_sample >> 3;
	frame.bit_info = 0;

 	/* If buffer is not empty, combine the buffer with the just incoming
 	 * samples, then call put_frame.
 	 */
 	if (strm->rec_buf_count) {
 	    unsigned chunk_count = 0;

 	    chunk_count = strm->param.samples_per_frame - strm->rec_buf_count;
 	    pjmedia_copy_samples(strm->rec_buf + strm->rec_buf_count,
 				 input, chunk_count);

 	    frame.buf = (void*) strm->rec_buf;
 	    frame.timestamp.u64 = strm->rec_timestamp.u64;

 	    status = (*strm->rec_cb)(strm->user_data, &frame);

 	    input = input + chunk_count;
 	    nsamples -= strm->param.samples_per_frame;
 	    strm->rec_buf_count = 0;
 	    strm->rec_timestamp.u64 += strm->param.samples_per_frame /
				       strm->param.channel_count;
 	}

 	/* Give all frames we have */
 	while (nsamples >= strm->param.samples_per_frame && status == 0) {
 	    frame.buf = (void*) input;
 	    frame.timestamp.u64 = strm->rec_timestamp.u64;

 	    status = (*strm->rec_cb)(strm->user_data, &frame);

 	    input = (pj_int16_t*) input + strm->param.samples_per_frame;
 	    nsamples -= strm->param.samples_per_frame;
 	    strm->rec_timestamp.u64 += strm->param.samples_per_frame /
				       strm->param.channel_count;
 	}

 	/* Store the remaining samples into the buffer */
 	if (nsamples && status == 0) {
 	    strm->rec_buf_count = nsamples;
 	    pjmedia_copy_samples(strm->rec_buf, input,
 			         nsamples);
 	}

     } else {
 	/* Not enough samples, let's just store them in the buffer */
 	pjmedia_copy_samples(strm->rec_buf + strm->rec_buf_count,
 			     input,
 			     inNumberFrames * strm->param.channel_count);
 	strm->rec_buf_count += inNumberFrames * strm->param.channel_count;
     }

    return noErr;

on_break:
    return -1;
}

static OSStatus output_renderer(void                       *inRefCon,
                                AudioUnitRenderActionFlags *ioActionFlags,
                                const AudioTimeStamp       *inTimeStamp,
                                UInt32                      inBusNumber,
                                UInt32                      inNumberFrames,
                                AudioBufferList            *ioData)
{
    struct coreaudio_stream *stream = (struct coreaudio_stream*)inRefCon;
    pj_status_t status = 0;
    unsigned nsamples_req = inNumberFrames * stream->param.channel_count;
    pj_int16_t *output = ioData->mBuffers[0].mData;

    pj_assert(!stream->quit_flag);

    /* Known cases of callback's thread:
     * - The thread may be changed in the middle of a session
     *   it happens when plugging/unplugging headphone.
     * - The same thread may be reused in consecutive sessions. The first
     *   session will leave TLS set, but release the TLS data address,
     *   so the second session must re-register the callback's thread.
     */
    if (stream->play_thread_initialized == 0 || !pj_thread_is_registered(stream->pool->factory->inst_id))
    {
	pj_bzero(stream->play_thread_desc, sizeof(pj_thread_desc));
	status = pj_thread_register(stream->pool->factory->inst_id, "coreaudio", stream->play_thread_desc,
				    &stream->play_thread);
	stream->play_thread_initialized = 1;
	PJ_LOG(5,(THIS_FILE, "Player thread started, (%i frames)",
		  inNumberFrames));
    }


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

	    return noErr;
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
	pjmedia_frame frame;

	frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
	frame.size = stream->param.samples_per_frame *
		     stream->param.bits_per_sample >> 3;
	frame.timestamp.u64 = stream->play_timestamp.u64;
	frame.bit_info = 0;

	if (nsamples_req >= stream->param.samples_per_frame) {
	    frame.buf = output;
	    status = (*stream->play_cb)(stream->user_data, &frame);
	    if (status != PJ_SUCCESS)
		goto on_break;

	    if (frame.type != PJMEDIA_FRAME_TYPE_AUDIO)
		pj_bzero(frame.buf, frame.size);

	    nsamples_req -= stream->param.samples_per_frame;
	    output = (pj_int16_t*)output + stream->param.samples_per_frame;
	} else {
	    frame.buf = stream->play_buf;
	    status = (*stream->play_cb)(stream->user_data, &frame);
	    if (status != PJ_SUCCESS)
		goto on_break;

	    if (frame.type != PJMEDIA_FRAME_TYPE_AUDIO)
		pj_bzero(frame.buf, frame.size);

	    pjmedia_copy_samples((pj_int16_t*)output, stream->play_buf,
				 nsamples_req);
	    stream->play_buf_count = stream->param.samples_per_frame -
		                     nsamples_req;
	    pjmedia_move_samples(stream->play_buf,
				 stream->play_buf+nsamples_req,
				 stream->play_buf_count);
	    nsamples_req = 0;
	}

	stream->play_timestamp.u64 += stream->param.samples_per_frame /
				      stream->param.channel_count;
    }

    return noErr;

on_break:
    return -1;
}

#if !COREAUDIO_MAC
static void propListener(void 			*inClientData,
			 AudioSessionPropertyID	inID,
			 UInt32                 inDataSize,
			 const void *           inData)
{
    struct coreaudio_factory *cf = (struct coreaudio_factory*)inClientData;
    struct stream_list *it, *itBegin;
    CFDictionaryRef routeDictionary;
    CFNumberRef reason;
    SInt32 reasonVal;
    pj_assert(cf);

    if (inID != kAudioSessionProperty_AudioRouteChange)
	return;

    routeDictionary = (CFDictionaryRef)inData;
    reason = (CFNumberRef)
	     CFDictionaryGetValue(
	         routeDictionary, 
		 CFSTR(kAudioSession_AudioRouteChangeKey_Reason));
    CFNumberGetValue(reason, kCFNumberSInt32Type, &reasonVal);

    if (reasonVal != kAudioSessionRouteChangeReason_OldDeviceUnavailable) {
	PJ_LOG(3, (THIS_FILE, "ignoring audio route change..."));
	return;
    }

    PJ_LOG(3, (THIS_FILE, "audio route changed"));

    pj_mutex_lock(cf->mutex);
    itBegin = &cf->streams;
    for (it = itBegin->next; it != itBegin; it = it->next) {
	if (it->stream->interrupted)
	    continue;

	/*
	status = ca_stream_stop((pjmedia_aud_stream *)it->stream);
	status = ca_stream_start((pjmedia_aud_stream *)it->stream);
	if (status != PJ_SUCCESS) {
	    PJ_LOG(3, (THIS_FILE,
		       "Error: failed to restart the audio unit (%i)",
		       status));
	    continue;
	}
	PJ_LOG(3, (THIS_FILE, "core audio unit successfully restarted"));
	*/
    }
    pj_mutex_unlock(cf->mutex);
}

static void interruptionListener(int inst_id, void *inClientData, UInt32 inInterruption)
{
    struct stream_list *it, *itBegin;
    pj_status_t status;
    pj_thread_desc thread_desc;
    pj_thread_t *thread;
    
    /* Register the thread with PJLIB, this is must for any external threads
     * which need to use the PJLIB framework.
     */
    if (!pj_thread_is_registered(inst_id)) {
	pj_bzero(thread_desc, sizeof(pj_thread_desc));
	status = pj_thread_register(inst_id, "intListener", thread_desc, &thread);
    }
    
    PJ_LOG(3, (THIS_FILE, "Session interrupted! --- %s ---",
	   inInterruption == kAudioSessionBeginInterruption ?
	   "Begin Interruption" : "End Interruption"));

    if (!cf_instance)
	return;
    
    pj_mutex_lock(cf_instance->mutex);
    itBegin = &cf_instance->streams;
    for (it = itBegin->next; it != itBegin; it = it->next) {
	if (inInterruption == kAudioSessionEndInterruption &&
	    it->stream->interrupted == PJ_TRUE)
	{
	    UInt32 audioCategory;
	    OSStatus ostatus;

	    /* Make sure that your application can receive remote control
	     * events by adding the code:
	     *     [[UIApplication sharedApplication] 
	     *      beginReceivingRemoteControlEvents];
	     * Otherwise audio unit will fail to restart while your
	     * application is in the background mode.
	     */
	    /* Make sure we set the correct audio category before restarting */
	    audioCategory = kAudioSessionCategory_PlayAndRecord;
	    ostatus = AudioSessionSetProperty(kAudioSessionProperty_AudioCategory,
					      sizeof(audioCategory),
					      &audioCategory);
	    if (ostatus != kAudioSessionNoError) {
		PJ_LOG(4, (THIS_FILE,
			   "Warning: cannot set the audio session category (%i)",
			   ostatus));
	    }
	    
	    /* Restart the stream */
	    status = ca_stream_start((pjmedia_aud_stream*)it->stream);
	    if (status != PJ_SUCCESS) {
		PJ_LOG(3, (THIS_FILE,
			   "Error: failed to restart the audio unit (%i)",
			   status));
		continue;
	    }
	    PJ_LOG(3, (THIS_FILE, "core audio unit successfully resumed"
		       " after interruption"));
	} else if (inInterruption == kAudioSessionBeginInterruption &&
		   it->stream->running == PJ_TRUE)
	{
	    status = ca_stream_stop((pjmedia_aud_stream*)it->stream);
	    it->stream->interrupted = PJ_TRUE;
	}
    }
    pj_mutex_unlock(cf_instance->mutex);
}

#endif

#if COREAUDIO_MAC
/* Internal: create audio converter for resampling the recorder device */
static pj_status_t create_audio_resample(struct coreaudio_stream     *strm,
					 AudioStreamBasicDescription *desc)
{
    OSStatus ostatus;

    pj_assert(strm->streamFormat.mSampleRate != desc->mSampleRate);
    pj_assert(NULL == strm->resample);
    pj_assert(NULL == strm->resample_buf);

    /* Create the audio converter */
    ostatus = AudioConverterNew(desc, &strm->streamFormat, &strm->resample);
    if (ostatus != noErr) {
	return PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(ostatus);
    }
    
    /*
     * Allocate the buffer required to hold enough input data
     */
    strm->resample_buf_size =  (unsigned)(desc->mSampleRate *
					  strm->param.samples_per_frame /
					  strm->param.clock_rate);
    strm->resample_buf = (pj_int16_t*)
			 pj_pool_alloc(strm->pool,
				       strm->resample_buf_size *
				       strm->param.bits_per_sample >> 3);
    if (!strm->resample_buf)
	return PJ_ENOMEM;
    strm->resample_buf_count = 0;

    return PJ_SUCCESS;
}
#endif

/* Internal: create audio unit for recorder/playback device */
static pj_status_t create_audio_unit(AudioComponent io_comp,
				     AudioDeviceID dev_id,
				     pjmedia_dir dir,
				     struct coreaudio_stream *strm,
				     AudioUnit *io_unit)
{
    OSStatus ostatus;
#if !COREAUDIO_MAC
    UInt32 audioCategory = kAudioSessionCategory_PlayAndRecord;
    /* We want to be able to open playback and recording streams */
    ostatus = AudioSessionSetProperty(kAudioSessionProperty_AudioCategory,
				      sizeof(audioCategory),
				      &audioCategory);
    if (ostatus != kAudioSessionNoError) {
	PJ_LOG(4, (THIS_FILE,
		   "Warning: cannot set the audio session category (%i)",
		   ostatus));
    }    
#endif
    
    /* Create an audio unit to interface with the device */
    ostatus = AudioComponentInstanceNew(io_comp, io_unit);
    if (ostatus != noErr) {
	return PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(ostatus);
    }

    /* Set audio unit's properties for capture device */
    if (dir & PJMEDIA_DIR_CAPTURE) {
	UInt32 enable = 1;

	/* Enable input */
	ostatus = AudioUnitSetProperty(*io_unit,
	                               kAudioOutputUnitProperty_EnableIO,
	                               kAudioUnitScope_Input,
	                               1,
	                               &enable,
	                               sizeof(enable));
	if (ostatus != noErr) {
	    PJ_LOG(4, (THIS_FILE,
		   "Warning: cannot enable IO of capture device %d",
		   dev_id));
	}

	/* Disable output */
	if (!(dir & PJMEDIA_DIR_PLAYBACK)) {
	    enable = 0;
	    ostatus = AudioUnitSetProperty(*io_unit,
					   kAudioOutputUnitProperty_EnableIO,
					   kAudioUnitScope_Output,
					   0,
					   &enable,
					   sizeof(enable));
	    if (ostatus != noErr) {
		PJ_LOG(4, (THIS_FILE,
		       "Warning: cannot disable IO of capture device %d",
		       dev_id));
	    }
	}
    }

    /* Set audio unit's properties for playback device */
    if (dir & PJMEDIA_DIR_PLAYBACK) {
	UInt32 enable = 1;

	/* Enable output */
	ostatus = AudioUnitSetProperty(*io_unit,
	                               kAudioOutputUnitProperty_EnableIO,
	                               kAudioUnitScope_Output,
	                               0,
	                               &enable,
	                               sizeof(enable));
	if (ostatus != noErr) {
	    PJ_LOG(4, (THIS_FILE,
		   "Warning: cannot enable IO of playback device %d",
		   dev_id));
	}

    }

#if COREAUDIO_MAC
    PJ_LOG(5, (THIS_FILE, "Opening device %d", dev_id));
    ostatus = AudioUnitSetProperty(*io_unit,
			           kAudioOutputUnitProperty_CurrentDevice,
			           kAudioUnitScope_Global,
			           0,
			           &dev_id,
			           sizeof(dev_id));
    if (ostatus != noErr) {
	return PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(ostatus);
    }
#endif

    if (dir & PJMEDIA_DIR_CAPTURE) {
#if COREAUDIO_MAC
	AudioStreamBasicDescription deviceFormat;
	UInt32 size;

	/*
	 * Keep the sample rate from the device, otherwise we will confuse
	 * AUHAL
	 */
	size = sizeof(AudioStreamBasicDescription);
	ostatus = AudioUnitGetProperty(*io_unit,
				       kAudioUnitProperty_StreamFormat,
				       kAudioUnitScope_Input,
				       1,
				       &deviceFormat,
				       &size);
	if (ostatus != noErr) {
	    return PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(ostatus);
	}
	strm->streamFormat.mSampleRate = deviceFormat.mSampleRate;
#endif

	/* When setting the stream format, we have to make sure the sample
	 * rate is supported. Setting an unsupported sample rate will cause
	 * AudioUnitRender() to fail later.
	 */
	ostatus = AudioUnitSetProperty(*io_unit,
				       kAudioUnitProperty_StreamFormat,
				       kAudioUnitScope_Output,
				       1,
				       &strm->streamFormat,
				       sizeof(strm->streamFormat));
	if (ostatus != noErr) {
	    return PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(ostatus);
	}

#if COREAUDIO_MAC
	strm->streamFormat.mSampleRate = strm->param.clock_rate;
	size = sizeof(AudioStreamBasicDescription);
	ostatus = AudioUnitGetProperty (*io_unit,
					kAudioUnitProperty_StreamFormat,
					kAudioUnitScope_Output,
					1,
					&deviceFormat,
					&size);
	if (ostatus == noErr) {
	    if (strm->streamFormat.mSampleRate != deviceFormat.mSampleRate) {
		pj_status_t rc = create_audio_resample(strm, &deviceFormat);
		if (PJ_SUCCESS != rc)
		    return rc;
	    }
	} else {
	    return PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(ostatus);
	}
#endif
    }

    if (dir & PJMEDIA_DIR_PLAYBACK) {
	AURenderCallbackStruct output_cb;

	/* Set the stream format */
	ostatus = AudioUnitSetProperty(*io_unit,
	                               kAudioUnitProperty_StreamFormat,
	                               kAudioUnitScope_Input,
	                               0,
	                               &strm->streamFormat,
	                               sizeof(strm->streamFormat));
	if (ostatus != noErr) {
	    PJ_LOG(4, (THIS_FILE,
		   "Warning: cannot set playback stream format of dev %d",
		   dev_id));
	}

	/* Set render callback */
	output_cb.inputProc = output_renderer;
	output_cb.inputProcRefCon = strm;
	ostatus = AudioUnitSetProperty(*io_unit,
				       kAudioUnitProperty_SetRenderCallback,
				       kAudioUnitScope_Input,
				       0,
				       &output_cb,
				       sizeof(output_cb));
	if (ostatus != noErr) {
	    return PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(ostatus);
	}

	/* Allocate playback buffer */
	strm->play_buf = (pj_int16_t*)pj_pool_alloc(strm->pool,
			 strm->param.samples_per_frame *
			 strm->param.bits_per_sample >> 3);
	if (!strm->play_buf)
	    return PJ_ENOMEM;
	strm->play_buf_count = 0;
    }

    if (dir & PJMEDIA_DIR_CAPTURE) {
	AURenderCallbackStruct input_cb;
#if COREAUDIO_MAC
	AudioBuffer *ab;
	UInt32 size, buf_size;
#endif

	/* Set input callback */
	input_cb.inputProc = strm->resample ? resample_callback :
			     input_callback;
	input_cb.inputProcRefCon = strm;
	ostatus = AudioUnitSetProperty(
		      *io_unit,
		      kAudioOutputUnitProperty_SetInputCallback,
		      kAudioUnitScope_Global,
		      0,
		      &input_cb,
		      sizeof(input_cb));
	if (ostatus != noErr) {
	    return PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(ostatus);
	}

#if COREAUDIO_MAC
	/* Get device's buffer frame size */
	size = sizeof(UInt32);
	ostatus = AudioUnitGetProperty(*io_unit,
		                       kAudioDevicePropertyBufferFrameSize,
		                       kAudioUnitScope_Global,
		                       0,
		                       &buf_size,
		                       &size);
	if (ostatus != noErr)
	{
	    return PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(ostatus);
	}

	/* Allocate audio buffer */
	strm->audio_buf = (AudioBufferList*)pj_pool_alloc(strm->pool,
		          sizeof(AudioBufferList) + sizeof(AudioBuffer));
	if (!strm->audio_buf)
	    return PJ_ENOMEM;

	strm->audio_buf->mNumberBuffers = 1;
	ab = &strm->audio_buf->mBuffers[0];
	ab->mNumberChannels = strm->streamFormat.mChannelsPerFrame;
	ab->mDataByteSize = buf_size * ab->mNumberChannels *
			    strm->param.bits_per_sample >> 3;
	ab->mData = pj_pool_alloc(strm->pool,
				  ab->mDataByteSize);
	if (!ab->mData)
	    return PJ_ENOMEM;

#else
	/* We will let AudioUnitRender() to allocate the buffer
	 * for us later
	 */
	strm->audio_buf = (AudioBufferList*)pj_pool_alloc(strm->pool,
		          sizeof(AudioBufferList) + sizeof(AudioBuffer));
	if (!strm->audio_buf)
	    return PJ_ENOMEM;

	strm->audio_buf->mNumberBuffers = 1;
	strm->audio_buf->mBuffers[0].mNumberChannels =
		strm->streamFormat.mChannelsPerFrame;
	
#endif

	/* Allocate recording buffer */
	strm->rec_buf = (pj_int16_t*)pj_pool_alloc(strm->pool,
			strm->param.samples_per_frame *
			strm->param.bits_per_sample >> 3);
	if (!strm->rec_buf)
	    return PJ_ENOMEM;
	strm->rec_buf_count = 0;
    }

    /* Initialize the audio unit */
    ostatus = AudioUnitInitialize(*io_unit);
    if (ostatus != noErr) {
 	return PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(ostatus);
    }

    return PJ_SUCCESS;
}

/* API: create stream */
static pj_status_t ca_factory_create_stream(pjmedia_aud_dev_factory *f,
					    const pjmedia_aud_param *param,
					    pjmedia_aud_rec_cb rec_cb,
					    pjmedia_aud_play_cb play_cb,
					    void *user_data,
					    pjmedia_aud_stream **p_aud_strm)
{
    struct coreaudio_factory *cf = (struct coreaudio_factory*)f;
    pj_pool_t *pool;
    struct coreaudio_stream *strm;
    pj_status_t status;

    /* Create and Initialize stream descriptor */
    pool = pj_pool_create(cf->pf, "coreaudio-dev", 1000, 1000, NULL);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    strm = PJ_POOL_ZALLOC_T(pool, struct coreaudio_stream);
    pj_list_init(&strm->list_entry);
    strm->list_entry.stream = strm;
    strm->cf = cf;
    pj_memcpy(&strm->param, param, sizeof(*param));
    strm->pool = pool;
    strm->rec_cb = rec_cb;
    strm->play_cb = play_cb;
    strm->user_data = user_data;

    /* Set the stream format */
    strm->streamFormat.mSampleRate       = param->clock_rate;
    strm->streamFormat.mFormatID         = kAudioFormatLinearPCM;
    strm->streamFormat.mFormatFlags      = kLinearPCMFormatFlagIsSignedInteger
  					   | kLinearPCMFormatFlagIsPacked;
    strm->streamFormat.mBitsPerChannel   = strm->param.bits_per_sample;
    strm->streamFormat.mChannelsPerFrame = param->channel_count;
    strm->streamFormat.mBytesPerFrame    = strm->streamFormat.mChannelsPerFrame
	                                   * strm->param.bits_per_sample >> 3;
    strm->streamFormat.mFramesPerPacket  = 1;
    strm->streamFormat.mBytesPerPacket   = strm->streamFormat.mBytesPerFrame *
					   strm->streamFormat.mFramesPerPacket;

    /* Apply input/output routes settings before we create the audio units */
    if (param->flags & PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE) {
	ca_stream_set_cap(&strm->base,
		          PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE,
		          &param->input_route);
    }
    if (param->flags & PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE) {
	ca_stream_set_cap(&strm->base,
		          PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE,
		          &param->output_route);
    }
    if (param->flags & PJMEDIA_AUD_DEV_CAP_EC) {
	ca_stream_set_cap(&strm->base,
		          PJMEDIA_AUD_DEV_CAP_EC,
		          &param->ec_enabled);
    } else {
	pj_bool_t ec = PJ_FALSE;
	ca_stream_set_cap(&strm->base,
		          PJMEDIA_AUD_DEV_CAP_EC, &ec);
    }

    strm->io_units[0] = strm->io_units[1] = NULL;
    if (param->dir == PJMEDIA_DIR_CAPTURE_PLAYBACK &&
	param->rec_id == param->play_id)
    {
	/* If both input and output are on the same device, only create
	 * one audio unit to interface with the device.
	 */
	status = create_audio_unit(cf->io_comp,
		                   cf->dev_info[param->rec_id].dev_id,
		                   param->dir, strm, &strm->io_units[0]);
	if (status != PJ_SUCCESS)
	    goto on_error;
    } else {
	unsigned nunits = 0;

	if (param->dir & PJMEDIA_DIR_CAPTURE) {
	    status = create_audio_unit(cf->io_comp,
				       cf->dev_info[param->rec_id].dev_id,
				       PJMEDIA_DIR_CAPTURE,
				       strm, &strm->io_units[nunits++]);
	    if (status != PJ_SUCCESS)
		goto on_error;
	}
	if (param->dir & PJMEDIA_DIR_PLAYBACK) {

	    status = create_audio_unit(cf->io_comp,
				       cf->dev_info[param->play_id].dev_id,
				       PJMEDIA_DIR_PLAYBACK,
				       strm, &strm->io_units[nunits++]);
	    if (status != PJ_SUCCESS)
		goto on_error;
	}
    }

    /* Apply the remaining settings */
    if (param->flags & PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY) {
	ca_stream_get_cap(&strm->base,
		          PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY,
		          &strm->param.input_latency_ms);
    }
    if (param->flags & PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY) {
	ca_stream_get_cap(&strm->base,
		          PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY,
		          &strm->param.output_latency_ms);
    }
    if (param->flags & PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING) {
	ca_stream_set_cap(&strm->base,
		          PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING,
		          &param->output_vol);
    }

    pj_mutex_lock(strm->cf->mutex);
    pj_assert(pj_list_empty(&strm->list_entry));
    pj_list_insert_after(&strm->cf->streams, &strm->list_entry);
    pj_mutex_unlock(strm->cf->mutex);

    /* Done */
    strm->base.op = &stream_op;
    *p_aud_strm = &strm->base;

    return PJ_SUCCESS;

 on_error:
    ca_stream_destroy((pjmedia_aud_stream *)strm);
    return status;
}

/* API: Get stream info. */
static pj_status_t ca_stream_get_param(pjmedia_aud_stream *s,
				       pjmedia_aud_param *pi)
{
    struct coreaudio_stream *strm = (struct coreaudio_stream*)s;

    PJ_ASSERT_RETURN(strm && pi, PJ_EINVAL);

    pj_memcpy(pi, &strm->param, sizeof(*pi));

    /* Update the device capabilities' values */
    if (ca_stream_get_cap(s, PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY,
		          &pi->input_latency_ms) == PJ_SUCCESS)
    {
    	pi->flags |= PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY;
    }
    if (ca_stream_get_cap(s, PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY,
			  &pi->output_latency_ms) == PJ_SUCCESS)
    {
	pi->flags |= PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY;
    }
    if (ca_stream_get_cap(s, PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING,
			  &pi->output_vol) == PJ_SUCCESS)
    {
        pi->flags |= PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING;
    }
    if (ca_stream_get_cap(s, PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE,
			  &pi->input_route) == PJ_SUCCESS)
    {
        pi->flags |= PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE;
    }
    if (ca_stream_get_cap(s, PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE,
			  &pi->output_route) == PJ_SUCCESS)
    {
        pi->flags |= PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE;
    }
    if (ca_stream_get_cap(s, PJMEDIA_AUD_DEV_CAP_EC,
			  &pi->ec_enabled) == PJ_SUCCESS)
    {
        pi->flags |= PJMEDIA_AUD_DEV_CAP_EC;
    }

    return PJ_SUCCESS;
}

/* API: get capability */
static pj_status_t ca_stream_get_cap(pjmedia_aud_stream *s,
				     pjmedia_aud_dev_cap cap,
				     void *pval)
{
    struct coreaudio_stream *strm = (struct coreaudio_stream*)s;

    PJ_UNUSED_ARG(strm);

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

    if (cap==PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY &&
	(strm->param.dir & PJMEDIA_DIR_CAPTURE))
    {
#if COREAUDIO_MAC
	UInt32 latency, size = sizeof(UInt32);

	/* Recording latency */
	if (AudioUnitGetProperty (strm->io_units[0],
				  kAudioDevicePropertyLatency,
				  kAudioUnitScope_Input,
				  1,
				  &latency,
				  &size) == noErr)
	{
	    UInt32 latency2;
	    if (AudioUnitGetProperty (strm->io_units[0],
				      kAudioDevicePropertyBufferFrameSize,
				      kAudioUnitScope_Input,
				      1,
				      &latency2,
				      &size) == noErr)
	    {
		strm->param.input_latency_ms = (latency + latency2) * 1000 /
					       strm->param.clock_rate;
		strm->param.input_latency_ms++;
	    }
	}
#else
	Float32 latency, latency2;
	UInt32 size = sizeof(Float32);

	if ((AudioSessionGetProperty(
	    kAudioSessionProperty_CurrentHardwareInputLatency,
	    &size, &latency) == kAudioSessionNoError) &&
	    (AudioSessionGetProperty(
	    kAudioSessionProperty_CurrentHardwareIOBufferDuration,
	    &size, &latency2) == kAudioSessionNoError))
	{
	    strm->param.input_latency_ms = (unsigned)
					   ((latency + latency2) * 1000);
	    strm->param.input_latency_ms++;
	}
#endif

	*(unsigned*)pval = strm->param.input_latency_ms;
	return PJ_SUCCESS;
    } else if (cap==PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY  &&
	       (strm->param.dir & PJMEDIA_DIR_PLAYBACK))
    {
#if COREAUDIO_MAC
	UInt32 latency, size = sizeof(UInt32);
	AudioUnit *io_unit = strm->io_units[1] ? &strm->io_units[1] :
			     &strm->io_units[0];

	/* Playback latency */
	if (AudioUnitGetProperty (*io_unit,
				  kAudioDevicePropertyLatency,
				  kAudioUnitScope_Output,
				  0,
				  &latency,
				  &size) == noErr)
	{
	    UInt32 latency2;
	    if (AudioUnitGetProperty (*io_unit,
				      kAudioDevicePropertyBufferFrameSize,
				      kAudioUnitScope_Output,
				      0,
				      &latency2,
				      &size) == noErr)
	    {
		strm->param.output_latency_ms = (latency + latency2) * 1000 /
						strm->param.clock_rate;
		strm->param.output_latency_ms++;
	    }
	}
#else
	Float32 latency, latency2;
	UInt32 size = sizeof(Float32);

	if ((AudioSessionGetProperty(
	    kAudioSessionProperty_CurrentHardwareOutputLatency,
	    &size, &latency) == kAudioSessionNoError) &&
	    (AudioSessionGetProperty(
	    kAudioSessionProperty_CurrentHardwareIOBufferDuration,
	    &size, &latency2) == kAudioSessionNoError))
	{
	    strm->param.output_latency_ms = (unsigned)
					    ((latency + latency2) * 1000);
	    strm->param.output_latency_ms++;
	}
#endif
	*(unsigned*)pval = (++strm->param.output_latency_ms * 2);
	return PJ_SUCCESS;
    } else if (cap==PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING &&
	       (strm->param.dir & PJMEDIA_DIR_PLAYBACK))
    {
	OSStatus ostatus;
	Float32 volume;
	UInt32 size = sizeof(Float32);

	/* Output volume setting */
#if COREAUDIO_MAC
	ostatus = AudioUnitGetProperty (strm->io_units[1] ? strm->io_units[1] :
					strm->io_units[0],
					kAudioDevicePropertyVolumeScalar,
	                                kAudioUnitScope_Output,
	                                0,
	                                &volume,
	                                &size);
	if (ostatus != noErr)
	    return PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(ostatus);
#else
	ostatus = AudioSessionGetProperty(
		  kAudioSessionProperty_CurrentHardwareOutputVolume,
		  &size, &volume);
	if (ostatus != kAudioSessionNoError) {
	    return PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(ostatus);
	}
#endif

	*(unsigned*)pval = (unsigned)(volume * 100);
	return PJ_SUCCESS;
#if !COREAUDIO_MAC
    } else if (cap==PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE &&
	       (strm->param.dir & PJMEDIA_DIR_CAPTURE))
    {
	UInt32 btooth, size = sizeof(UInt32);
	OSStatus ostatus;

	ostatus = AudioSessionGetProperty (
	    kAudioSessionProperty_OverrideCategoryEnableBluetoothInput,
	    &size, &btooth);
	if (ostatus != kAudioSessionNoError) {
	    return PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(ostatus);
	}

	*(pjmedia_aud_dev_route*)pval = btooth?
		                        PJMEDIA_AUD_DEV_ROUTE_BLUETOOTH:
					PJMEDIA_AUD_DEV_ROUTE_DEFAULT;
	return PJ_SUCCESS;
    } else if (cap==PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE &&
	       (strm->param.dir & PJMEDIA_DIR_PLAYBACK))
    {
	CFStringRef route;
	UInt32 size = sizeof(CFStringRef);
	OSStatus ostatus;

	ostatus = AudioSessionGetProperty (kAudioSessionProperty_AudioRoute,
					   &size, &route);
	if (ostatus != kAudioSessionNoError) {
	    return PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(ostatus);
	}

	if (!route) {
	    *(pjmedia_aud_dev_route*)pval = PJMEDIA_AUD_DEV_ROUTE_DEFAULT;
	} else if (CFStringHasPrefix(route, CFSTR("Headset"))) {
	    *(pjmedia_aud_dev_route*)pval = PJMEDIA_AUD_DEV_ROUTE_EARPIECE;
	} else {
	    *(pjmedia_aud_dev_route*)pval = PJMEDIA_AUD_DEV_ROUTE_DEFAULT;
	}

	CFRelease(route);

	return PJ_SUCCESS;
    } else if (cap==PJMEDIA_AUD_DEV_CAP_EC) {
	AudioComponentDescription desc;
	OSStatus ostatus;

	ostatus = AudioComponentGetDescription(strm->cf->io_comp, &desc);
	if (ostatus != noErr) {
	    return PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(ostatus);
	}

	*(pj_bool_t*)pval = (desc.componentSubType ==
		            kAudioUnitSubType_VoiceProcessingIO);
	return PJ_SUCCESS;
#endif
    } else {
	return PJMEDIA_EAUD_INVCAP;
    }
}

/* API: set capability */
static pj_status_t ca_stream_set_cap(pjmedia_aud_stream *s,
				     pjmedia_aud_dev_cap cap,
				     const void *pval)
{
    struct coreaudio_stream *strm = (struct coreaudio_stream*)s;

    PJ_ASSERT_RETURN(s && pval, PJ_EINVAL);

#if COREAUDIO_MAC
    if (cap==PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING &&
	(strm->param.dir & PJMEDIA_DIR_PLAYBACK))
    {
	OSStatus ostatus;
	Float32 volume = *(unsigned*)pval;

	/* Output volume setting */
	volume /= 100.0;
	ostatus = AudioUnitSetProperty (strm->io_units[1] ? strm->io_units[1] :
					strm->io_units[0],
					kAudioDevicePropertyVolumeScalar,
	                                kAudioUnitScope_Output,
	                                0,
	                                &volume,
	                                sizeof(Float32));
	if (ostatus != noErr) {
	    return PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(ostatus);
	}
	strm->param.output_vol = *(unsigned*)pval;
	return PJ_SUCCESS;
    }

#else

    if ((cap==PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY &&
	 (strm->param.dir & PJMEDIA_DIR_CAPTURE)) ||
	(cap==PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY &&
	 (strm->param.dir & PJMEDIA_DIR_PLAYBACK)))
    {
	Float32 bufferDuration = *(unsigned *)pval;
	OSStatus ostatus;
	unsigned latency;
	
	/* For low-latency audio streaming, you can set this value to
	 * as low as 5 ms (the default is 23ms). However, lowering the
	 * latency may cause a decrease in audio quality.
	 */
	bufferDuration /= 1000;
	ostatus = AudioSessionSetProperty(
		      kAudioSessionProperty_PreferredHardwareIOBufferDuration,
		      sizeof(bufferDuration), &bufferDuration);
	if (ostatus != kAudioSessionNoError) {
	    PJ_LOG(4, (THIS_FILE,
		       "Error: cannot set the preferred buffer duration (%i)",
		       ostatus));
	    return PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(ostatus);
	}
	
	ca_stream_get_cap(s, PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY, &latency);
	ca_stream_get_cap(s, PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY, &latency);
	
	return PJ_SUCCESS;
    } else if (cap==PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE &&
	       (strm->param.dir & PJMEDIA_DIR_CAPTURE))
    {
	UInt32 btooth = *(pjmedia_aud_dev_route*)pval ==
		        PJMEDIA_AUD_DEV_ROUTE_BLUETOOTH ? 1 : 0;
	OSStatus ostatus;

	ostatus = AudioSessionSetProperty (
	    kAudioSessionProperty_OverrideCategoryEnableBluetoothInput,
	    sizeof(btooth), &btooth);
	if (ostatus != kAudioSessionNoError) {
	    return PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(ostatus);
	}
	strm->param.input_route = *(pjmedia_aud_dev_route*)pval;
	return PJ_SUCCESS;
    } else if (cap==PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE &&
	       (strm->param.dir & PJMEDIA_DIR_PLAYBACK))
    {
	OSStatus ostatus;
	UInt32 route = *(pjmedia_aud_dev_route*)pval ==
		       PJMEDIA_AUD_DEV_ROUTE_LOUDSPEAKER ?
		       kAudioSessionOverrideAudioRoute_Speaker :
		       kAudioSessionOverrideAudioRoute_None;

	ostatus = AudioSessionSetProperty (
	    kAudioSessionProperty_OverrideAudioRoute,
	    sizeof(route), &route);
	if (ostatus != kAudioSessionNoError) {
	    return PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(ostatus);
	}
	strm->param.output_route = *(pjmedia_aud_dev_route*)pval;
	return PJ_SUCCESS;
    } else if (cap==PJMEDIA_AUD_DEV_CAP_EC) {
	AudioComponentDescription desc;
	AudioComponent io_comp;

	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = (*(pj_bool_t*)pval)?
				kAudioUnitSubType_VoiceProcessingIO :
				kAudioUnitSubType_RemoteIO;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

	io_comp = AudioComponentFindNext(NULL, &desc);
	if (io_comp == NULL)
	    return PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(-1);
	strm->cf->io_comp = io_comp;
	strm->param.ec_enabled = *(pj_bool_t*)pval;

        PJ_LOG(4, (THIS_FILE, "Using %s audio unit", 
                              (desc.componentSubType ==
                               kAudioUnitSubType_RemoteIO? "RemoteIO":
                               "VoiceProcessingIO")));
        
	return PJ_SUCCESS;
    }
#endif

    return PJMEDIA_EAUD_INVCAP;
}

/* API: Start stream. */
static pj_status_t ca_stream_start(pjmedia_aud_stream *strm)
{
    struct coreaudio_stream *stream = (struct coreaudio_stream*)strm;
    OSStatus ostatus;
    UInt32 i;

    if (stream->running)
	return PJ_SUCCESS;

    stream->quit_flag = 0;
    stream->interrupted = PJ_FALSE;
    stream->rec_buf_count = 0;
    stream->play_buf_count = 0;
    stream->resample_buf_count = 0;

    if (stream->resample) {
	ostatus = AudioConverterReset(stream->resample);
	if (ostatus != noErr)
	    return PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(ostatus);
    }

#if !COREAUDIO_MAC
    AudioSessionSetActive(true);
#endif
    
    for (i = 0; i < 2; i++) {
	if (stream->io_units[i] == NULL) break;
	ostatus = AudioOutputUnitStart(stream->io_units[i]);
	if (ostatus != noErr) {
	    if (i == 1)
		AudioOutputUnitStop(stream->io_units[0]);
	    return PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(ostatus);
	}
    }

    stream->running = PJ_TRUE;

    PJ_LOG(4, (THIS_FILE, "core audio stream started"));

    return PJ_SUCCESS;
}

/* API: Stop stream. */
static pj_status_t ca_stream_stop(pjmedia_aud_stream *strm)
{
    struct coreaudio_stream *stream = (struct coreaudio_stream*)strm;
    OSStatus ostatus;
    unsigned i;
    int should_deactivate;
    struct stream_list *it, *itBegin;

    if (!stream->running)
	return PJ_SUCCESS;

    for (i = 0; i < 2; i++) {
	if (stream->io_units[i] == NULL) break;
	ostatus = AudioOutputUnitStop(stream->io_units[i]);
	if (ostatus != noErr) {
	    if (i == 0 && stream->io_units[1])
		AudioOutputUnitStop(stream->io_units[1]);
	    return PJMEDIA_AUDIODEV_ERRNO_FROM_COREAUDIO(ostatus);
	}
    }

    /* Check whether we need to deactivate the audio session. */
    pj_mutex_lock(stream->cf->mutex);
    pj_assert(!pj_list_empty(&stream->cf->streams));
    pj_assert(!pj_list_empty(&stream->list_entry));
    stream->running = PJ_FALSE;
    should_deactivate = PJ_TRUE;
    itBegin = &stream->cf->streams;
    for (it = itBegin->next; it != itBegin; it = it->next) {
	if (it->stream->running) {
	    should_deactivate = PJ_FALSE;
	    break;
	}
    }
    pj_mutex_unlock(stream->cf->mutex);

#if !COREAUDIO_MAC
    if (should_deactivate)
	AudioSessionSetActive(false);
#endif

    stream->quit_flag = 1;
    stream->play_thread_initialized = 0;
    stream->rec_thread_initialized = 0;
    pj_bzero(stream->rec_thread_desc, sizeof(pj_thread_desc));
    pj_bzero(stream->play_thread_desc, sizeof(pj_thread_desc));

    PJ_LOG(4, (THIS_FILE, "core audio stream stopped"));

    return PJ_SUCCESS;
}


/* API: Destroy stream. */
static pj_status_t ca_stream_destroy(pjmedia_aud_stream *strm)
{
    struct coreaudio_stream *stream = (struct coreaudio_stream*)strm;
    unsigned i;

    PJ_ASSERT_RETURN(stream != NULL, PJ_EINVAL);

    ca_stream_stop(strm);

    for (i = 0; i < 2; i++) {
	if (stream->io_units[i]) {
	    AudioUnitUninitialize(stream->io_units[i]);
	    AudioComponentInstanceDispose(stream->io_units[i]);
	    stream->io_units[i] = NULL;
	}
    }

    if (stream->resample)
	AudioConverterDispose(stream->resample);

    pj_mutex_lock(stream->cf->mutex);
    if (!pj_list_empty(&stream->list_entry))
	pj_list_erase(&stream->list_entry);
    pj_mutex_unlock(stream->cf->mutex);

    pj_pool_release(stream->pool);

    return PJ_SUCCESS;
}

#endif	/* PJMEDIA_AUDIO_DEV_HAS_COREAUDIO */
