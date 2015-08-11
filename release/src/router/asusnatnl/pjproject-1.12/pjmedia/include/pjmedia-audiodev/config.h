/* $Id: config.h 3748 2011-09-09 09:51:10Z nanang $ */
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
#ifndef __PJMEDIA_AUDIODEV_CONFIG_H__
#define __PJMEDIA_AUDIODEV_CONFIG_H__

/**
 * @file audiodev.h
 * @brief Audio device API.
 */
#include <pjmedia/types.h>
#include <pj/pool.h>


PJ_BEGIN_DECL

/**
 * @defgroup audio_device_api Audio Device API
 * @brief PJMEDIA audio device abstraction API.
 */

/**
 * @defgroup s1_audio_device_config Compile time configurations
 * @ingroup audio_device_api
 * @brief Compile time configurations
 * @{
 */

/**
 * This setting controls whether PortAudio support should be included.
 *
 * By default it is enabled except on Windows platforms (including
 * Windows Mobile) and Symbian.
 */
#ifndef PJMEDIA_AUDIO_DEV_HAS_PORTAUDIO
#   if (defined(PJ_WIN32) && PJ_WIN32!=0) || \
       (defined(PJ_SYMBIAN) && PJ_SYMBIAN!=0)
#	define PJMEDIA_AUDIO_DEV_HAS_PORTAUDIO	0
#   else
#	define PJMEDIA_AUDIO_DEV_HAS_PORTAUDIO	1
#   endif
#endif


/**
 * This setting controls whether native ALSA support should be included.
 */
#ifndef PJMEDIA_AUDIO_DEV_HAS_ALSA
#   define PJMEDIA_AUDIO_DEV_HAS_ALSA		0
#endif


/**
 * This setting controls whether null audio support should be included.
 */
#ifndef PJMEDIA_AUDIO_DEV_HAS_NULL_AUDIO
#   define PJMEDIA_AUDIO_DEV_HAS_NULL_AUDIO	0
#endif


/**
 * This setting controls whether coreaudio support should be included.
 */
#ifndef PJMEDIA_AUDIO_DEV_HAS_COREAUDIO
#   define PJMEDIA_AUDIO_DEV_HAS_COREAUDIO	0
#endif


/**
 * This setting controls whether WMME support should be included.
 */
#ifndef PJMEDIA_AUDIO_DEV_HAS_WMME
#   define PJMEDIA_AUDIO_DEV_HAS_WMME		1
#endif


/**
 * This setting controls whether Symbian APS support should be included.
 */
#ifndef PJMEDIA_AUDIO_DEV_HAS_SYMB_APS
#   define PJMEDIA_AUDIO_DEV_HAS_SYMB_APS	0
#endif


/**
 * This setting controls whether Symbian APS should perform codec
 * detection in its factory initalization. Note that codec detection 
 * may take few seconds and detecting more codecs will take more time.
 * Possible values are:
 * - 0: no codec detection, all APS codec (AMR-NB, G.711, G.729, and
 *      iLBC) will be assumed as supported.
 * - 1: minimal codec detection, i.e: only detect for AMR-NB and G.711,
 *      (G.729 and iLBC are considered to be supported/unsupported when
 *      G.711 is supported/unsupported).
 * - 2: full codec detection, i.e: detect AMR-NB, G.711, G.729, and iLBC.
 * 
 * Default: 1 (minimal codec detection)
 */
#ifndef PJMEDIA_AUDIO_DEV_SYMB_APS_DETECTS_CODEC
#   define PJMEDIA_AUDIO_DEV_SYMB_APS_DETECTS_CODEC 1
#endif


/**
 * This setting controls whether Symbian VAS support should be included.
 */
#ifndef PJMEDIA_AUDIO_DEV_HAS_SYMB_VAS
#   define PJMEDIA_AUDIO_DEV_HAS_SYMB_VAS	0
#endif

/**
 * This setting controls Symbian VAS version to be used. Currently, valid
 * values are only 1 (for VAS 1.0) and 2 (for VAS 2.0).
 *
 * Default: 1 (VAS version 1.0)
 */
#ifndef PJMEDIA_AUDIO_DEV_SYMB_VAS_VERSION
#   define PJMEDIA_AUDIO_DEV_SYMB_VAS_VERSION	1
#endif


/**
 * This setting controls whether Symbian audio (using built-in multimedia 
 * framework) support should be included.
 */
#ifndef PJMEDIA_AUDIO_DEV_HAS_SYMB_MDA
#   define PJMEDIA_AUDIO_DEV_HAS_SYMB_MDA	PJ_SYMBIAN
#endif


/**
 * This setting controls whether the Symbian audio with built-in multimedia
 * framework backend should be started synchronously. Note that synchronous
 * start will block the application/UI, e.g: about 40ms for each direction
 * on N95. While asynchronous start may cause invalid value (always zero)
 * returned in input/output volume query, if the query is performed when
 * the internal start procedure is not completely finished.
 * 
 * Default: 1 (yes)
 */
#ifndef PJMEDIA_AUDIO_DEV_MDA_USE_SYNC_START
#   define PJMEDIA_AUDIO_DEV_MDA_USE_SYNC_START	1
#endif


/**
 * This setting controls whether the Audio Device API should support
 * device implementation that is based on the old sound device API
 * (sound.h). 
 *
 * Enable this API if:
 *  - you have implemented your own sound device using the old sound
 *    device API (sound.h), and
 *  - you wish to be able to use your sound device implementation
 *    using the new Audio Device API.
 *
 * Please see http://trac.pjsip.org/repos/wiki/Audio_Dev_API for more
 * info.
 */
#ifndef PJMEDIA_AUDIO_DEV_HAS_LEGACY_DEVICE
#   define PJMEDIA_AUDIO_DEV_HAS_LEGACY_DEVICE	0
#endif


/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJMEDIA_AUDIODEV_CONFIG_H__ */

/*
 --------------------- DOCUMENTATION FOLLOWS ---------------------------
 */

/**
 * @addtogroup audio_device_api Audio Device API
 * @{

PJMEDIA Audio Device API is a cross-platform audio API appropriate for use with
VoIP applications and many other types of audio streaming applications. 

The API abstracts many different audio API's on various platforms, such as:
 - PortAudio back-end for Win32, Windows Mobile, Linux, Unix, dan MacOS X.
 - native WMME audio for Win32 and Windows Mobile devices
 - native Symbian audio streaming/multimedia framework (MMF) implementation
 - native Nokia Audio Proxy Server (APS) implementation
 - null-audio implementation
 - and more to be implemented in the future

The Audio Device API/library is an evolution from PJMEDIA @ref PJMED_SND and 
contains many enhancements:

 - Forward compatibility:
\n
   The new API has been designed to be extensible, it will support new API's as 
   well as new features that may be introduced in the future without breaking 
   compatibility with applications that use this API as well as compatibility 
   with existing device implementations. 

 - Device capabilities:
\n
   At the heart of the API is device capabilities management, where all possible
   audio capabilities of audio devices should be able to be handled in a generic
   manner. With this framework, new capabilities that may be discovered in the 
   future can be handled in manner without breaking existing applications. 

 - Built-in features:
\n
   The device capabilities framework enables applications to use and control 
   audio features built-in in the device, such as:
    - echo cancellation, 
    - built-in codecs, 
    - audio routing (e.g. to earpiece or loudspeaker),
    - volume control,
    - etc.

 - Codec support:
\n
   Some audio devices such as Nokia/Symbian Audio Proxy Server (APS) and Nokia 
   VoIP Audio Services (VAS) support built-in hardware audio codecs (e.g. G.729,
   iLBC, and AMR), and application can use the sound device in encoded mode to
   make use of these hardware codecs. 

 - Multiple backends:
\n
   The new API supports multiple audio backends (called factories or drivers in 
   the code) to be active simultaneously, and audio backends may be added or 
   removed during run-time. 


@section using Overview on using the API

@subsection getting_started Getting started

 -# <b>Configure the application's project settings</b>.\n
    Add the following 
    include:
    \code
    #include <pjmedia_audiodev.h>\endcode\n
    And add <b>pjmedia-audiodev</b> library to your application link 
    specifications.\n
 -# <b>Compile time settings</b>.\n
    Use the compile time settings to enable or
    disable specific audio drivers. For more information, please see
    \ref s1_audio_device_config.
 -# <b>API initialization and cleaning up</b>.\n
    Before anything else, application must initialize the API by calling:
    \code
    pjmedia_aud_subsys_init(pf);\endcode\n
    And add this in the application cleanup sequence
    \code
    pjmedia_aud_subsys_shutdown();\endcode

@subsection devices Working with devices

 -# The following code prints the list of audio devices detected
    in the system.
    \code
    int dev_count;
    pjmedia_aud_dev_index dev_idx;
    pj_status_t status;

    dev_count = pjmedia_aud_dev_count();
    printf("Got %d audio devices\n", dev_count);

    for (dev_idx=0; dev_idx<dev_count; ++i) {
	pjmedia_aud_dev_info info;

	status = pjmedia_aud_dev_get_info(dev_idx, &info);
	printf("%d. %s (in=%d, out=%d)\n",
	       dev_idx, info.name, 
	       info.input_count, info.output_count);
    }
    \endcode\n
 -# Info: The #PJMEDIA_AUD_DEFAULT_CAPTURE_DEV and #PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV
    constants are used to denote default capture and playback devices
    respectively.
 -# Info: You may save the device and driver's name in your application
    setting, for example to specify the prefered devices to be
    used by your application. You can then retrieve the device index
    for the device by calling:
    \code
	const char *drv_name = "WMME";
	const char *dev_name = "Wave mapper";
	pjmedia_aud_dev_index dev_idx;

        status = pjmedia_aud_dev_lookup(drv_name, dev_name, &dev_idx);
	if (status==PJ_SUCCESS)
	    printf("Device index is %d\n", dev_idx);
    \endcode

@subsection caps Device capabilities

Capabilities are encoded as #pjmedia_aud_dev_cap enumeration. Please see
#pjmedia_aud_dev_cap enumeration for more information.

 -# The following snippet prints the capabilities supported by the device:
    \code
    pjmedia_aud_dev_info info;
    pj_status_t status;

    status = pjmedia_aud_dev_get_info(PJMEDIA_AUD_DEFAULT_CAPTURE_DEV, &info);
    if (status == PJ_SUCCESS) {
	unsigned i;
	// Enumerate capability bits
	printf("Device capabilities: ");
	for (i=0; i<32; ++i) {
	    if (info.caps & (1 << i))
		printf("%s ", pjmedia_aud_dev_cap_name(1 << i, NULL));
	}
    }
    \endcode\n
 -# Info: You can set the device settings when opening audio stream by setting
    the flags and the appropriate setting in #pjmedia_aud_param when calling
    #pjmedia_aud_stream_create()\n
 -# Info: Once the audio stream is running, you can retrieve or change the stream 
    setting by specifying the capability in #pjmedia_aud_stream_get_cap()
    and #pjmedia_aud_stream_set_cap() respectively.


@subsection creating_stream Creating audio streams

The audio stream enables audio streaming to capture device, playback device,
or both.

 -# It is recommended to initialize the #pjmedia_aud_param with its default
    values before using it:
    \code
    pjmedia_aud_param param;
    pjmedia_aud_dev_index dev_idx;
    pj_status_t status;

    dev_idx = PJMEDIA_AUD_DEFAULT_CAPTURE_DEV;
    status = pjmedia_aud_dev_default_param(dev_idx, &param);
    \endcode\n
 -# Configure the mandatory parameters:
    \code
    param.dir = PJMEDIA_DIR_CAPTURE_PLAYBACK;
    param.rec_id = PJMEDIA_AUD_DEFAULT_CAPTURE_DEV;
    param.play_id = PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV;
    param.clock_rate = 8000;
    param.channel_count = 1;
    param.samples_per_frame = 160;
    param.bits_per_sample = 16;
    \endcode\n
 -# If you want the audio stream to use the device's built-in codec, specify
    the codec in the #pjmedia_aud_param. You must make sure that the codec
    is supported by the device, by looking at its supported format list in
    the #pjmedia_aud_dev_info.\n
    The snippet below sets the audio stream to use G.711 ULAW encoding:
    \code
    unsigned i;

    // Make sure Ulaw is supported
    if ((info.caps & PJMEDIA_AUD_DEV_CAP_EXT_FORMAT) == 0)
	error("Device does not support extended formats");
    for (i = 0; i < info.ext_fmt_cnt; ++i) {
	if (info.ext_fmt[i].id == PJMEDIA_FORMAT_ULAW)
	    break;
    }
    if (i == info.ext_fmt_cnt)
	error("Device does not support Ulaw format");

    // Set Ulaw format
    param.flags |= PJMEDIA_AUD_DEV_CAP_EXT_FORMAT;
    param.ext_fmt.id = PJMEDIA_FORMAT_ULAW;
    param.ext_fmt.bitrate = 64000;
    param.ext_fmt.vad = PJ_FALSE;
    \endcode\n
 -# Note that if non-PCM format is configured on the audio stream, the
    capture and/or playback functions (#pjmedia_aud_rec_cb and 
    #pjmedia_aud_play_cb respectively) will report the audio frame as
    #pjmedia_frame_ext structure instead of the #pjmedia_frame.
 -# Optionally configure other device's capabilities. The following snippet
    shows how to enable echo cancellation on the device (note that this
    snippet may not be necessary since the setting may have been enabled 
    when calling #pjmedia_aud_dev_default_param() above):
    \code
    if (info.caps & PJMEDIA_AUD_DEV_CAP_EC) {
	param.flags |= PJMEDIA_AUD_DEV_CAP_EC;
	param.ec_enabled = PJ_TRUE;
    }
    \endcode
 -# Open the audio stream, specifying the capture and/or playback callback
    functions:
    \code
       pjmedia_aud_stream *stream;

       status = pjmedia_aud_stream_create(&param, &rec_cb, &play_cb, 
                                          user_data, &stream);
    \endcode

@subsection working_with_stream Working with audio streams

 -# To start the audio stream:
    \code
	status = pjmedia_aud_stream_start(stream);
    \endcode\n
    To stop the stream:
    \code
	status = pjmedia_aud_stream_stop(stream);
    \endcode\n
    And to destroy the stream:
    \code
	status = pjmedia_aud_stream_destroy(stream);
    \endcode\n
 -# Info: The following shows how to retrieve the capability value of the
    stream (in this case, the current output volume setting).
    \code
    // Volume setting is an unsigned integer showing the level in percent.
    unsigned vol;
    status = pjmedia_aud_stream_get_cap(stream, 
					PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING,
					&vol);
    \endcode
 -# Info: And following shows how to modify the capability value of the
    stream (in this case, the current output volume setting).
    \code
    // Volume setting is an unsigned integer showing the level in percent.
    unsigned vol = 50;
    status = pjmedia_aud_stream_set_cap(stream, 
					PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING,
					&vol);
    \endcode


*/


/**
 * @}
 */

