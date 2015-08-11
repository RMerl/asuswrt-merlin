/* $Id: audiodev.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_AUDIODEV_AUDIODEV_H__
#define __PJMEDIA_AUDIODEV_AUDIODEV_H__

/**
 * @file audiodev.h
 * @brief Audio device API.
 */
#include <pjmedia-audiodev/config.h>
#include <pjmedia-audiodev/errno.h>
#include <pjmedia/types.h>
#include <pj/pool.h>


PJ_BEGIN_DECL

/**
 * @defgroup s2_audio_device_reference Audio Device API Reference
 * @ingroup audio_device_api
 * @brief API Reference
 * @{
 */

/**
 * Type for device index.
 */
typedef pj_int32_t pjmedia_aud_dev_index;

/**
 * Device index constants.
 */
enum
{
    /** 
     * Constant to denote default capture device 
     */
    PJMEDIA_AUD_DEFAULT_CAPTURE_DEV = -1,

    /** 
     * Constant to denote default playback device 
     */
    PJMEDIA_AUD_DEFAULT_PLAYBACK_DEV = -2,

    /**
     * Constant to denote invalid device index.
     */
    PJMEDIA_AUD_INVALID_DEV = -3
};


/**
 * This enumeration identifies various audio device capabilities. These audio
 * capabilities indicates what features are supported by the underlying
 * audio device implementation.
 *
 * Applications get these capabilities in the #pjmedia_aud_dev_info structure.
 *
 * Application can also set the specific features/capabilities when opening
 * the audio stream by setting the \a flags member of #pjmedia_aud_param
 * structure.
 *
 * Once audio stream is running, application can also retrieve or set some
 * specific audio capability, by using #pjmedia_aud_stream_get_cap() and
 * #pjmedia_aud_stream_set_cap() and specifying the desired capability. The
 * value of the capability is specified as pointer, and application needs to
 * supply the pointer with the correct value, according to the documentation
 * of each of the capability.
 */
typedef enum pjmedia_aud_dev_cap
{
    /** 
     * Support for audio formats other than PCM. The value of this capability
     * is represented by #pjmedia_format structure.
     */
    PJMEDIA_AUD_DEV_CAP_EXT_FORMAT = 1,

    /** 
     * Support for audio input latency control or query. The value of this 
     * capability is an unsigned integer containing milliseconds value of
     * the latency.
     */
    PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY = 2,

    /** 
     * Support for audio output latency control or query. The value of this 
     * capability is an unsigned integer containing milliseconds value of
     * the latency.
     */
    PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY = 4,

    /** 
     * Support for setting/retrieving the audio input device volume level.
     * The value of this capability is an unsigned integer representing 
     * the input audio volume setting in percent.
     */
    PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING = 8,

    /** 
     * Support for setting/retrieving the audio output device volume level.
     * The value of this capability is an unsigned integer representing 
     * the output audio volume setting in percent.
     */
    PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING = 16,

    /** 
     * Support for monitoring the current audio input signal volume. 
     * The value of this capability is an unsigned integer representing 
     * the audio volume in percent.
     */
    PJMEDIA_AUD_DEV_CAP_INPUT_SIGNAL_METER = 32,

    /** 
     * Support for monitoring the current audio output signal volume. 
     * The value of this capability is an unsigned integer representing 
     * the audio volume in percent.
     */
    PJMEDIA_AUD_DEV_CAP_OUTPUT_SIGNAL_METER = 64,

    /** 
     * Support for audio input routing. The value of this capability is an 
     * integer containing #pjmedia_aud_dev_route enumeration.
     */
    PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE = 128,

    /** 
     * Support for audio output routing (e.g. loudspeaker vs earpiece). The
     * value of this capability is an integer containing #pjmedia_aud_dev_route
     * enumeration.
     */
    PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE = 256,

    /** 
     * The audio device has echo cancellation feature. The value of this
     * capability is a pj_bool_t containing boolean PJ_TRUE or PJ_FALSE.
     */
    PJMEDIA_AUD_DEV_CAP_EC = 512,

    /** 
     * The audio device supports setting echo cancellation fail length. The
     * value of this capability is an unsigned integer representing the
     * echo tail in milliseconds.
     */
    PJMEDIA_AUD_DEV_CAP_EC_TAIL = 1024,

    /** 
     * The audio device has voice activity detection feature. The value
     * of this capability is a pj_bool_t containing boolean PJ_TRUE or 
     * PJ_FALSE.
     */
    PJMEDIA_AUD_DEV_CAP_VAD = 2048,

    /** 
     * The audio device has comfort noise generation feature. The value
     * of this capability is a pj_bool_t containing boolean PJ_TRUE or 
     * PJ_FALSE.
     */
    PJMEDIA_AUD_DEV_CAP_CNG = 4096,

    /** 
     * The audio device has packet loss concealment feature. The value
     * of this capability is a pj_bool_t containing boolean PJ_TRUE or 
     * PJ_FALSE.
     */
    PJMEDIA_AUD_DEV_CAP_PLC = 8192,
    
    /**
     * End of capability
     */
    PJMEDIA_AUD_DEV_CAP_MAX = 16384

} pjmedia_aud_dev_cap;


/**
 * This enumeration describes audio routing setting.
 */
typedef enum pjmedia_aud_dev_route
{
    /** Default route. */
    PJMEDIA_AUD_DEV_ROUTE_DEFAULT = 0,

    /** Route to loudspeaker */
    PJMEDIA_AUD_DEV_ROUTE_LOUDSPEAKER = 1,

    /** Route to earpiece */
    PJMEDIA_AUD_DEV_ROUTE_EARPIECE = 2,

    /** Route to paired Bluetooth device */
    PJMEDIA_AUD_DEV_ROUTE_BLUETOOTH = 4

} pjmedia_aud_dev_route;


/**
 * Device information structure returned by #pjmedia_aud_dev_get_info().
 */
typedef struct pjmedia_aud_dev_info
{
    /** 
     * The device name 
     */
    char name[64];

    /** 
     * Maximum number of input channels supported by this device. If the
     * value is zero, the device does not support input operation (i.e.
     * it is a playback only device). 
     */
    unsigned input_count;

    /** 
     * Maximum number of output channels supported by this device. If the
     * value is zero, the device does not support output operation (i.e. 
     * it is an input only device).
     */
    unsigned output_count;

    /** 
     * Default sampling rate.
     */
    unsigned default_samples_per_sec;

    /** 
     * The underlying driver name 
     */
    char driver[32];

    /** 
     * Device capabilities, as bitmask combination of #pjmedia_aud_dev_cap.
     */
    unsigned caps;

    /** 
     * Supported audio device routes, as bitmask combination of 
     * #pjmedia_aud_dev_route. The value may be zero if the device
     * does not support audio routing.
     */
    unsigned routes;

    /** 
     * Number of audio formats supported by this device. The value may be
     * zero if the device does not support non-PCM format.
     */
    unsigned ext_fmt_cnt;

    /** 
     * Array of supported extended audio formats 
     */
    pjmedia_format ext_fmt[8];


} pjmedia_aud_dev_info;


/** 
 * This callback is called by player stream when it needs additional data
 * to be played by the device. Application must fill in the whole of output 
 * buffer with audio samples.
 *
 * The frame argument contains the following values:
 *  - timestamp	    Playback timestamp, in samples.
 *  - buf	    Buffer to be filled out by application.
 *  - size	    The size requested in bytes, which will be equal to
 *		    the size of one whole packet.
 *
 * @param user_data User data associated with the stream.
 * @param frame	    Audio frame, which buffer is to be filled in by
 *		    the application.
 *
 * @return	    Returning non-PJ_SUCCESS will cause the audio stream
 *		    to stop
 */
typedef pj_status_t (*pjmedia_aud_play_cb)(void *user_data,
					   pjmedia_frame *frame);

/**
 * This callback is called by recorder stream when it has captured the whole
 * packet worth of audio samples.
 *
 * @param user_data User data associated with the stream.
 * @param frame	    Captured frame.
 *
 * @return	    Returning non-PJ_SUCCESS will cause the audio stream
 *		    to stop
 */
typedef pj_status_t (*pjmedia_aud_rec_cb)(void *user_data,
					  pjmedia_frame *frame);

/**
 * This structure specifies the parameters to open the audio stream.
 */
typedef struct pjmedia_aud_param
{
    /**
     * The audio direction. This setting is mandatory.
     */
    pjmedia_dir dir;

    /**
     * The audio recorder device ID. This setting is mandatory if the audio
     * direction includes input/capture direction.
     */
    pjmedia_aud_dev_index rec_id;

    /**
     * The audio playback device ID. This setting is mandatory if the audio
     * direction includes output/playback direction.
     */
    pjmedia_aud_dev_index play_id;

    /** 
     * Clock rate/sampling rate. This setting is mandatory. 
     */
    unsigned clock_rate;

    /** 
     * Number of channels. This setting is mandatory. 
     */
    unsigned channel_count;

    /** 
     * Number of samples per frame. This setting is mandatory. 
     */
    unsigned samples_per_frame;

    /** 
     * Number of bits per sample. This setting is mandatory. 
     */
    unsigned bits_per_sample;

    /** 
     * This flags specifies which of the optional settings are valid in this
     * structure. The flags is bitmask combination of pjmedia_aud_dev_cap.
     */
    unsigned flags;

    /** 
     * Set the audio format. This setting is optional, and will only be used
     * if PJMEDIA_AUD_DEV_CAP_EXT_FORMAT is set in the flags.
     */
    pjmedia_format ext_fmt;

    /**
     * Input latency, in milliseconds. This setting is optional, and will 
     * only be used if PJMEDIA_AUD_DEV_CAP_INPUT_LATENCY is set in the flags.
     */
    unsigned input_latency_ms;

    /**
     * Input latency, in milliseconds. This setting is optional, and will 
     * only be used if PJMEDIA_AUD_DEV_CAP_OUTPUT_LATENCY is set in the flags.
     */
    unsigned output_latency_ms;

    /**
     * Input volume setting, in percent. This setting is optional, and will 
     * only be used if PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING is set in 
     * the flags.
     */
    unsigned input_vol;

    /**
     * Output volume setting, in percent. This setting is optional, and will 
     * only be used if PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING is set in 
     * the flags.
     */
    unsigned output_vol;

    /** 
     * Set the audio input route. This setting is optional, and will only be
     * used if PJMEDIA_AUD_DEV_CAP_INPUT_ROUTE is set in the flags.
     */
    pjmedia_aud_dev_route input_route;

    /** 
     * Set the audio output route. This setting is optional, and will only be
     * used if PJMEDIA_AUD_DEV_CAP_OUTPUT_ROUTE is set in the flags.
     */
    pjmedia_aud_dev_route output_route;

    /**
     * Enable/disable echo canceller, if the device supports it. This setting
     * is optional, and will only be used if PJMEDIA_AUD_DEV_CAP_EC is set in
     * the flags.
     */
    pj_bool_t ec_enabled;

    /**
     * Set echo canceller tail length in milliseconds, if the device supports
     * it. This setting is optional, and will only be used if
     * PJMEDIA_AUD_DEV_CAP_EC_TAIL is set in the flags.
     */
    unsigned ec_tail_ms;

    /** 
     * Enable/disable PLC. This setting is optional, and will only be used
     * if PJMEDIA_AUD_DEV_CAP_PLC is set in the flags.
     */
    pj_bool_t plc_enabled;

    /** 
     * Enable/disable CNG. This setting is optional, and will only be used
     * if PJMEDIA_AUD_DEV_CAP_CNG is set in the flags.
     */
    pj_bool_t cng_enabled;

} pjmedia_aud_param;


/** Forward declaration for pjmedia_aud_stream */
typedef struct pjmedia_aud_stream pjmedia_aud_stream;

/** Forward declaration for audio device factory */
typedef struct pjmedia_aud_dev_factory pjmedia_aud_dev_factory;

/* typedef for factory creation function */
typedef pjmedia_aud_dev_factory*
(*pjmedia_aud_dev_factory_create_func_ptr)(pj_pool_factory*);


/**
 * Get string info for the specified capability.
 *
 * @param cap		The capability ID.
 * @param p_desc	Optional pointer which will be filled with longer 
 *			description about the capability.
 *
 * @return		Capability name.
 */
PJ_DECL(const char*) pjmedia_aud_dev_cap_name(pjmedia_aud_dev_cap cap,
					      const char **p_desc);


/**
 * Set a capability field value in #pjmedia_aud_param structure. This will
 * also set the flags field for the specified capability in the structure.
 *
 * @param param		The structure.
 * @param cap		The audio capability which value is to be set.
 * @param pval		Pointer to value. Please see the type of value to
 *			be supplied in the pjmedia_aud_dev_cap documentation.
 *
 * @return		PJ_SUCCESS on successful operation or the appropriate
 *			error code.
 */
PJ_DECL(pj_status_t) pjmedia_aud_param_set_cap(pjmedia_aud_param *param,
					       pjmedia_aud_dev_cap cap,
					       const void *pval);


/**
 * Get a capability field value from #pjmedia_aud_param structure. This
 * function will return PJMEDIA_EAUD_INVCAP error if the flag for that
 * capability is not set in the flags field in the structure.
 *
 * @param param		The structure.
 * @param cap		The audio capability which value is to be retrieved.
 * @param pval		Pointer to value. Please see the type of value to
 *			be supplied in the pjmedia_aud_dev_cap documentation.
 *
 * @return		PJ_SUCCESS on successful operation or the appropriate
 *			error code.
 */
PJ_DECL(pj_status_t) pjmedia_aud_param_get_cap(const pjmedia_aud_param *param,
					       pjmedia_aud_dev_cap cap,
					       void *pval);

/**
 * Initialize the audio subsystem. This will register all supported audio 
 * device factories to the audio subsystem. This function may be called
 * more than once, but each call to this function must have the
 * corresponding #pjmedia_aud_subsys_shutdown() call.
 *
 * @param pf		The pool factory.
 *
 * @return		PJ_SUCCESS on successful operation or the appropriate
 *			error code.
 */
PJ_DECL(pj_status_t) pjmedia_aud_subsys_init(pj_pool_factory *pf);


/**
 * Get the pool factory registered to the audio subsystem.
 *
 * @return		The pool factory.
 */
PJ_DECL(pj_pool_factory*) pjmedia_aud_subsys_get_pool_factory(void);


/**
 * Shutdown the audio subsystem. This will destroy all audio device factories
 * registered in the audio subsystem. Note that currently opened audio streams
 * may or may not be closed, depending on the implementation of the audio
 * device factories.
 *
 * @return		PJ_SUCCESS on successful operation or the appropriate
 *			error code.
 */
PJ_DECL(pj_status_t) pjmedia_aud_subsys_shutdown(void);


/**
 * Register a supported audio device factory to the audio subsystem. This
 * function can only be called after calling #pjmedia_aud_subsys_init().
 *
 * @param adf		The audio device factory.
 *
 * @return		PJ_SUCCESS on successful operation or the appropriate
 *			error code.
 */
PJ_DECL(pj_status_t)
pjmedia_aud_register_factory( pjmedia_aud_dev_factory_create_func_ptr adf);


/**
 * Unregister an audio device factory from the audio subsystem. This
 * function can only be called after calling #pjmedia_aud_subsys_init().
 * Devices from this factory will be unlisted. If a device from this factory
 * is currently in use, then the behavior is undefined.
 *
 * @param adf		The audio device factory.
 *
 * @return		PJ_SUCCESS on successful operation or the appropriate
 *			error code.
 */
PJ_DECL(pj_status_t)
pjmedia_aud_unregister_factory(pjmedia_aud_dev_factory_create_func_ptr adf);


/**
 * Refresh the list of sound devices installed in the system. This function
 * will only refresh the list of audio device so all active audio streams will
 * be unaffected. After refreshing the device list, application MUST make sure
 * to update all index references to audio devices (i.e. all variables of type
 * pjmedia_aud_dev_index) before calling any function that accepts audio device
 * index as its parameter.
 *
 * @return		PJ_SUCCESS on successful operation or the appropriate
 *			error code.
 */
PJ_DECL(pj_status_t) pjmedia_aud_dev_refresh(void);


/**
 * Get the number of sound devices installed in the system.
 *
 * @return		The number of sound devices installed in the system.
 */
PJ_DECL(unsigned) pjmedia_aud_dev_count(void);


/**
 * Get device information.
 *
 * @param id		The audio device ID.
 * @param info		The device information which will be filled in by this
 *			function once it returns successfully.
 *
 * @return		PJ_SUCCESS on successful operation or the appropriate
 *			error code.
 */
PJ_DECL(pj_status_t) pjmedia_aud_dev_get_info(pjmedia_aud_dev_index id,
					      pjmedia_aud_dev_info *info);


/**
 * Lookup device index based on the driver and device name.
 *
 * @param drv_name	The driver name.
 * @param dev_name	The device name.
 * @param id		Pointer to store the returned device ID.
 *
 * @return		PJ_SUCCESS if the device can be found.
 */
PJ_DECL(pj_status_t) pjmedia_aud_dev_lookup(const char *drv_name,
					    const char *dev_name,
					    pjmedia_aud_dev_index *id);


/**
 * Initialize the audio device parameters with default values for the
 * specified device.
 *
 * @param id		The audio device ID.
 * @param param		The audio device parameters which will be initialized
 *			by this function once it returns successfully.
 *
 * @return		PJ_SUCCESS on successful operation or the appropriate
 *			error code.
 */
PJ_DECL(pj_status_t) pjmedia_aud_dev_default_param(pjmedia_aud_dev_index id,
						   pjmedia_aud_param *param);


/**
 * Open audio stream object using the specified parameters.
 *
 * @param param		Sound device parameters to be used for the stream.
 * @param rec_cb	Callback to be called on every input frame captured.
 * @param play_cb	Callback to be called everytime the sound device needs
 *			audio frames to be played back.
 * @param user_data	Arbitrary user data, which will be given back in the
 *			callbacks.
 * @param p_strm	Pointer to receive the audio stream.
 *
 * @return		PJ_SUCCESS on successful operation or the appropriate
 *			error code.
 */
PJ_DECL(pj_status_t) pjmedia_aud_stream_create(const pjmedia_aud_param *param,
					       pjmedia_aud_rec_cb rec_cb,
					       pjmedia_aud_play_cb play_cb,
					       void *user_data,
					       pjmedia_aud_stream **p_strm);

/**
 * Get the running parameters for the specified audio stream.
 *
 * @param strm		The audio stream.
 * @param param		Audio stream parameters to be filled in by this 
 *			function once it returns successfully.
 *
 * @return		PJ_SUCCESS on successful operation or the appropriate
 *			error code.
 */
PJ_DECL(pj_status_t) pjmedia_aud_stream_get_param(pjmedia_aud_stream *strm,
						  pjmedia_aud_param *param);

/**
 * Get the value of a specific capability of the audio stream.
 *
 * @param strm		The audio stream.
 * @param cap		The audio capability which value is to be retrieved.
 * @param value		Pointer to value to be filled in by this function 
 *			once it returns successfully.  Please see the type 
 *			of value to be supplied in the pjmedia_aud_dev_cap
 *			documentation.
 *
 * @return		PJ_SUCCESS on successful operation or the appropriate
 *			error code.
 */
PJ_DECL(pj_status_t) pjmedia_aud_stream_get_cap(pjmedia_aud_stream *strm,
						pjmedia_aud_dev_cap cap,
						void *value);

/**
 * Set the value of a specific capability of the audio stream.
 *
 * @param strm		The audio stream.
 * @param cap		The audio capability which value is to be set.
 * @param value		Pointer to value. Please see the type of value to
 *			be supplied in the pjmedia_aud_dev_cap documentation.
 *
 * @return		PJ_SUCCESS on successful operation or the appropriate
 *			error code.
 */
PJ_DECL(pj_status_t) pjmedia_aud_stream_set_cap(pjmedia_aud_stream *strm,
						pjmedia_aud_dev_cap cap,
						const void *value);

/**
 * Start the stream.
 *
 * @param strm		The audio stream.
 *
 * @return		PJ_SUCCESS on successful operation or the appropriate
 *			error code.
 */
PJ_DECL(pj_status_t) pjmedia_aud_stream_start(pjmedia_aud_stream *strm);

/**
 * Stop the stream.
 *
 * @param strm		The audio stream.
 *
 * @return		PJ_SUCCESS on successful operation or the appropriate
 *			error code.
 */
PJ_DECL(pj_status_t) pjmedia_aud_stream_stop(pjmedia_aud_stream *strm);

/**
 * Destroy the stream.
 *
 * @param strm		The audio stream.
 *
 * @return		PJ_SUCCESS on successful operation or the appropriate
 *			error code.
 */
PJ_DECL(pj_status_t) pjmedia_aud_stream_destroy(pjmedia_aud_stream *strm);


/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJMEDIA_AUDIODEV_AUDIODEV_H__ */

