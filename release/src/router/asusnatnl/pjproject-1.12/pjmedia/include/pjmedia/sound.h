/* $Id: sound.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_SOUND_H__
#define __PJMEDIA_SOUND_H__


/**
 * @file sound.h
 * @brief Legacy sound device API
 */
#include <pjmedia-audiodev/audiodev.h>
#include <pjmedia/types.h>


PJ_BEGIN_DECL

/**
 * @defgroup PJMED_SND Portable Sound Hardware Abstraction
 * @ingroup PJMED_SND_PORT
 * @brief PJMEDIA abstraction for sound device hardware
 * @{
 *
 * <strong>Warning: this sound device API has been deprecated
 * and replaced by PJMEDIA Audio Device API. Please see
 * http://trac.pjsip.org/repos/wiki/Audio_Dev_API for more
 * information.</strong>
 *
 * This section describes lower level abstraction for sound device
 * hardware. Application normally uses the higher layer @ref
 * PJMED_SND_PORT abstraction since it works seamlessly with 
 * @ref PJMEDIA_PORT.
 *
 * The sound hardware abstraction basically runs <b>asychronously</b>,
 * and application must register callbacks to be called to receive/
 * supply audio frames from/to the sound hardware.
 *
 * A full duplex sound stream (created with #pjmedia_snd_open()) 
 * requires application to supply two callbacks:
 *  - <b><tt>rec_cb</tt></b> callback to be called when it has finished
 *    capturing one media frame, and 
 *  - <b><tt>play_cb</tt></b> callback to be called when it needs media 
 *    frame to be played to the sound playback hardware.
 *
 * Half duplex sound stream (created with #pjmedia_snd_open_rec() or
 * #pjmedia_snd_open_player()) will only need one of the callback to
 * be specified.
 *
 * After sound stream is created, application need to call
 * #pjmedia_snd_stream_start() to start capturing/playing back media
 * frames from/to the sound device.
 */

/** Opaque declaration for pjmedia_snd_stream. */
typedef struct pjmedia_snd_stream pjmedia_snd_stream;

/**
 * Device information structure returned by #pjmedia_snd_get_dev_info.
 */
typedef struct pjmedia_snd_dev_info
{
    char	name[64];	        /**< Device name.		    */
    unsigned	input_count;	        /**< Max number of input channels.  */
    unsigned	output_count;	        /**< Max number of output channels. */
    unsigned	default_samples_per_sec;/**< Default sampling rate.	    */
} pjmedia_snd_dev_info;

/** 
 * Stream information, can be retrieved from a live stream by calling
 * #pjmedia_snd_stream_get_info().
 */
typedef struct pjmedia_snd_stream_info
{
    pjmedia_dir	dir;		    /**< Stream direction.		    */
    int		play_id;	    /**< Playback dev id, or -1 for rec only*/
    int		rec_id;		    /**< Capture dev id, or -1 for play only*/
    unsigned	clock_rate;	    /**< Actual clock rate.		    */
    unsigned	channel_count;	    /**< Number of channels.		    */
    unsigned	samples_per_frame;  /**< Samples per frame.		    */
    unsigned	bits_per_sample;    /**< Bits per sample.		    */
    unsigned	rec_latency;	    /**< Record latency, in samples.	    */
    unsigned	play_latency;	    /**< Playback latency, in samples.	    */
} pjmedia_snd_stream_info;

/** 
 * This callback is called by player stream when it needs additional data
 * to be played by the device. Application must fill in the whole of output 
 * buffer with sound samples.
 *
 * @param user_data	User data associated with the stream.
 * @param timestamp	Timestamp, in samples.
 * @param output	Buffer to be filled out by application.
 * @param size		The size requested in bytes, which will be equal to
 *			the size of one whole packet.
 *
 * @return		Non-zero to stop the stream.
 */
typedef pj_status_t (*pjmedia_snd_play_cb)(/* in */   void *user_data,
				      /* in */   pj_uint32_t timestamp,
				      /* out */  void *output,
				      /* out */  unsigned size);

/**
 * This callback is called by recorder stream when it has captured the whole
 * packet worth of audio samples.
 *
 * @param user_data	User data associated with the stream.
 * @param timestamp	Timestamp, in samples.
 * @param output	Buffer containing the captured audio samples.
 * @param size		The size of the data in the buffer, in bytes.
 *
 * @return		Non-zero to stop the stream.
 */
typedef pj_status_t (*pjmedia_snd_rec_cb)(/* in */   void *user_data,
				     /* in */   pj_uint32_t timestamp,
				     /* in */   void *input,
				     /* in*/    unsigned size);

/**
 * Init the sound library.
 *
 * @param factory	The sound factory.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pjmedia_snd_init(pj_pool_factory *factory);


/**
 * Get the number of devices detected by the library.
 *
 * @return		Number of devices.
 */
PJ_DECL(int) pjmedia_snd_get_dev_count();


/**
 * Get device info.
 *
 * @param index		The index of the device, which should be in the range
 *			from zero to #pjmedia_snd_get_dev_count - 1.
 */
PJ_DECL(const pjmedia_snd_dev_info*) pjmedia_snd_get_dev_info(unsigned index);


/**
 * Set sound device latency, this function must be called before sound device
 * opened, or otherwise default latency setting will be used, @see
 * PJMEDIA_SND_DEFAULT_REC_LATENCY & PJMEDIA_SND_DEFAULT_PLAY_LATENCY.
 *
 * Choosing latency value is not straightforward, it should accomodate both 
 * minimum latency and stability. Lower latency tends to cause sound device 
 * less reliable (producing audio dropouts) on CPU load disturbance. Moreover,
 * the best latency setting may vary based on many aspects, e.g: sound card, 
 * CPU, OS, kernel, etc.
 *
 * @param input_latency	    The latency of input device, in ms, set to 0
 *			    for default PJMEDIA_SND_DEFAULT_REC_LATENCY.
 * @param output_latency    The latency of output device, in ms, set to 0
 *			    for default PJMEDIA_SND_DEFAULT_PLAY_LATENCY.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_snd_set_latency(unsigned input_latency, 
					     unsigned output_latency);


/**
 * Create sound stream for both capturing audio and audio playback,  from the 
 * same device. This is the recommended way to create simultaneous recorder 
 * and player streams (instead of creating separate capture and playback
 * streams), because it works on backends that does not allow
 * a device to be opened more than once.
 *
 * @param rec_id	    Device index for recorder/capture stream, or
 *			    -1 to use the first capable device.
 * @param play_id	    Device index for playback stream, or -1 to use 
 *			    the first capable device.
 * @param clock_rate	    Sound device's clock rate to set.
 * @param channel_count	    Set number of channels, 1 for mono, or 2 for
 *			    stereo. The channel count determines the format
 *			    of the frame.
 * @param samples_per_frame Number of samples per frame.
 * @param bits_per_sample   Set the number of bits per sample. The normal 
 *			    value for this parameter is 16 bits per sample.
 * @param rec_cb	    Callback to handle captured audio samples.
 * @param play_cb	    Callback to be called when the sound player needs
 *			    more audio samples to play.
 * @param user_data	    User data to be associated with the stream.
 * @param p_snd_strm	    Pointer to receive the stream instance.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_snd_open(int rec_id,
				      int play_id,
				      unsigned clock_rate,
				      unsigned channel_count,
				      unsigned samples_per_frame,
				      unsigned bits_per_sample,
				      pjmedia_snd_rec_cb rec_cb,
				      pjmedia_snd_play_cb play_cb,
				      void *user_data,
				      pjmedia_snd_stream **p_snd_strm);


/**
 * Create a unidirectional audio stream for capturing audio samples from
 * the sound device.
 *
 * @param index		    Device index, or -1 to let the library choose the 
 *			    first available device.
 * @param clock_rate	    Sound device's clock rate to set.
 * @param channel_count	    Set number of channels, 1 for mono, or 2 for
 *			    stereo. The channel count determines the format
 *			    of the frame.
 * @param samples_per_frame Number of samples per frame.
 * @param bits_per_sample   Set the number of bits per sample. The normal 
 *			    value for this parameter is 16 bits per sample.
 * @param rec_cb	    Callback to handle captured audio samples.
 * @param user_data	    User data to be associated with the stream.
 * @param p_snd_strm	    Pointer to receive the stream instance.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_snd_open_rec( int index,
					   unsigned clock_rate,
					   unsigned channel_count,
					   unsigned samples_per_frame,
					   unsigned bits_per_sample,
					   pjmedia_snd_rec_cb rec_cb,
					   void *user_data,
					   pjmedia_snd_stream **p_snd_strm);

/**
 * Create a unidirectional audio stream for playing audio samples to the
 * sound device.
 *
 * @param index		    Device index, or -1 to let the library choose the 
 *			    first available device.
 * @param clock_rate	    Sound device's clock rate to set.
 * @param channel_count	    Set number of channels, 1 for mono, or 2 for
 *			    stereo. The channel count determines the format
 *			    of the frame.
 * @param samples_per_frame Number of samples per frame.
 * @param bits_per_sample   Set the number of bits per sample. The normal 
 *			    value for this parameter is 16 bits per sample.
 * @param play_cb	    Callback to be called when the sound player needs
 *			    more audio samples to play.
 * @param user_data	    User data to be associated with the stream.
 * @param p_snd_strm	    Pointer to receive the stream instance.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_snd_open_player( int index,
					 unsigned clock_rate,
					 unsigned channel_count,
					 unsigned samples_per_frame,
					 unsigned bits_per_sample,
					 pjmedia_snd_play_cb play_cb,
					 void *user_data,
					 pjmedia_snd_stream **p_snd_strm );


/**
 * Get information about live stream.
 *
 * @param strm		The stream to be queried.
 * @param pi		Pointer to stream information to be filled up with
 *			information about the stream.
 *
 * @return		PJ_SUCCESS on success or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_snd_stream_get_info(pjmedia_snd_stream *strm,
						 pjmedia_snd_stream_info *pi);


/**
 * Start the stream.
 *
 * @param stream	The recorder or player stream.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pjmedia_snd_stream_start(pjmedia_snd_stream *stream);

/**
 * Stop the stream.
 *
 * @param stream	The recorder or player stream.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pjmedia_snd_stream_stop(pjmedia_snd_stream *stream);

/**
 * Destroy the stream.
 *
 * @param stream	The recorder of player stream.
 *
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pjmedia_snd_stream_close(pjmedia_snd_stream *stream);

/**
 * Deinitialize sound library.
 *
 * @param inst_id  The instance id of pjsua.
 * @return		Zero on success.
 */
PJ_DECL(pj_status_t) pjmedia_snd_deinit();



/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJMEDIA_SOUND_H__ */
