/* $Id: sound_port.h 4065 2012-04-21 02:17:07Z bennylp $ */
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
#ifndef __PJMEDIA_SOUND_PORT_H__
#define __PJMEDIA_SOUND_PORT_H__

/**
 * @file sound_port.h
 * @brief Media port connection abstraction to sound device.
 */
#include <pjmedia-audiodev/audiodev.h>
#include <pjmedia/port.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJMED_SND_PORT Sound Device Port
 * @ingroup PJMEDIA_PORT_CLOCK
 * @brief Media Port Connection Abstraction to the Sound Device
 @{

 As explained in @ref PJMED_SND, the sound hardware abstraction provides
 some callbacks for its user:
 - it calls <b><tt>rec_cb</tt></b> callback when it has finished capturing
   one media frame, and 
 - it calls <b><tt>play_cb</tt></b> when it needs media frame to be 
   played to the sound playback hardware.

 The @ref PJMED_SND_PORT (the object being explained here) add a
 thin wrapper to the hardware abstraction:
 - it will call downstream port's <tt>put_frame()</tt>
   when <b><tt>rec_cb()</tt></b> is called (i.e. when the sound hardware 
   has finished capturing frame), and 
 - it will call downstream port's <tt>get_frame()</tt> when 
   <b><tt>play_cb()</tt></b> is called (i.e. every time the 
   sound hardware needs more frames to be played to the playback hardware).

 This simple abstraction enables media to flow automatically (and
 in timely manner) from the downstream media port to the sound device.
 In other words, the sound device port supplies media clock to
 the ports. The media clock concept is explained in @ref PJMEDIA_PORT_CLOCK
 section.

 Application registers downstream port to the sound device port by
 calling #pjmedia_snd_port_connect();
 
 */

/**
 * Sound port options.
 */
enum pjmedia_snd_port_option
{
    /** 
     * Don't start the audio device when creating a sound port.
     */    
    PJMEDIA_SND_PORT_NO_AUTO_START = 1
};

/**
 * This structure specifies the parameters to create the sound port.
 * Use pjmedia_snd_port_param_default() to initialize this structure with
 * default values (mostly zeroes)
 */
typedef struct pjmedia_snd_port_param
{
    /**
     * Base structure.
     */
    pjmedia_aud_param base;
    
    /**
     * Sound port creation options.
     */
    unsigned options;

    /**
     * Echo cancellation options/flags.
     */
    unsigned ec_options;

} pjmedia_snd_port_param;

/**
 * Initialize pjmedia_snd_port_param with default values.
 *
 * @param prm		    The parameter.
 */
PJ_DECL(void) pjmedia_snd_port_param_default(pjmedia_snd_port_param *prm);

/**
 * This opaque type describes sound device port connection.
 * Sound device port is not a media port, but it is used to connect media
 * port to the sound device.
 */
typedef struct pjmedia_snd_port pjmedia_snd_port;


/**
 * Create bidirectional sound port for both capturing and playback of
 * audio samples.
 *
 * @param pool		    Pool to allocate sound port structure.
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
 * @param options	    Options flag.
 * @param p_port	    Pointer to receive the sound device port instance.
 *
 * @return		    PJ_SUCCESS on success, or the appropriate error
 *			    code.
 */
PJ_DECL(pj_status_t) pjmedia_snd_port_create( pj_pool_t *pool,
					      int rec_id,
					      int play_id,
					      unsigned clock_rate,
					      unsigned channel_count,
					      unsigned samples_per_frame,
					      unsigned bits_per_sample,
					      unsigned options,
					      pjmedia_snd_port **p_port);

/**
 * Create unidirectional sound device port for capturing audio streams from 
 * the sound device with the specified parameters.
 *
 * @param pool		    Pool to allocate sound port structure.
 * @param index		    Device index, or -1 to let the library choose the 
 *			    first available device.
 * @param clock_rate	    Sound device's clock rate to set.
 * @param channel_count	    Set number of channels, 1 for mono, or 2 for
 *			    stereo. The channel count determines the format
 *			    of the frame.
 * @param samples_per_frame Number of samples per frame.
 * @param bits_per_sample   Set the number of bits per sample. The normal 
 *			    value for this parameter is 16 bits per sample.
 * @param options	    Options flag.
 * @param p_port	    Pointer to receive the sound device port instance.
 *
 * @return		    PJ_SUCCESS on success, or the appropriate error
 *			    code.
 */
PJ_DECL(pj_status_t) pjmedia_snd_port_create_rec(pj_pool_t *pool,
						 int index,
						 unsigned clock_rate,
						 unsigned channel_count,
						 unsigned samples_per_frame,
						 unsigned bits_per_sample,
						 unsigned options,
						 pjmedia_snd_port **p_port);
					      
/**
 * Create unidirectional sound device port for playing audio streams with the 
 * specified parameters.
 *
 * @param pool		    Pool to allocate sound port structure.
 * @param index		    Device index, or -1 to let the library choose the 
 *			    first available device.
 * @param clock_rate	    Sound device's clock rate to set.
 * @param channel_count	    Set number of channels, 1 for mono, or 2 for
 *			    stereo. The channel count determines the format
 *			    of the frame.
 * @param samples_per_frame Number of samples per frame.
 * @param bits_per_sample   Set the number of bits per sample. The normal 
 *			    value for this parameter is 16 bits per sample.
 * @param options	    Options flag.
 * @param p_port	    Pointer to receive the sound device port instance.
 *
 * @return		    PJ_SUCCESS on success, or the appropriate error
 *			    code.
 */
PJ_DECL(pj_status_t) pjmedia_snd_port_create_player(pj_pool_t *pool,
						    int index,
						    unsigned clock_rate,
						    unsigned channel_count,
						    unsigned samples_per_frame,
						    unsigned bits_per_sample,
						    unsigned options,
						    pjmedia_snd_port **p_port);


/**
 * Create sound device port according to the specified parameters.
 *
 * @param pool		    Pool to allocate sound port structure.
 * @param prm		    Sound port parameter.
 * @param p_port	    Pointer to receive the sound device port instance.
 *
 * @return		    PJ_SUCCESS on success, or the appropriate error
 *			    code.
 */
PJ_DECL(pj_status_t) pjmedia_snd_port_create2(pj_pool_t *pool,
					      const pjmedia_snd_port_param *prm,
					      pjmedia_snd_port **p_port);


/**
 * Destroy sound device port.
 *
 * @param snd_port	    The sound device port.
 *
 * @return		    PJ_SUCCESS on success, or the appropriate error
 *			    code.
 */
PJ_DECL(pj_status_t) pjmedia_snd_port_destroy(pjmedia_snd_port *snd_port);


/**
 * Retrieve the sound stream associated by this sound device port.
 *
 * @param snd_port	    The sound device port.
 *
 * @return		    The sound stream instance.
 */
PJ_DECL(pjmedia_aud_stream*) pjmedia_snd_port_get_snd_stream(
						pjmedia_snd_port *snd_port);


/**
 * Change the echo cancellation settings. The echo cancellation settings 
 * should have been specified when this sound port was created, by setting
 * the appropriate fields in the pjmedia_aud_param, because not all sound
 * device implementation supports changing the EC setting once the device
 * has been opened.
 *
 * The behavior of this function depends on whether device or software AEC
 * is being used. If the device supports AEC, this function will forward
 * the change request to the device and it will be up to the device whether
 * to support the request. If software AEC is being used (the software EC
 * will be used if the device does not support AEC), this function will
 * change the software EC settings.
 *
 * @param snd_port	    The sound device port.
 * @param pool		    Pool to re-create the echo canceller if necessary.
 * @param tail_ms	    Maximum echo tail length to be supported, in
 *			    miliseconds. If zero is specified, the EC would
 *			    be disabled.
 * @param options	    The options to be passed to #pjmedia_echo_create().
 *			    This is only used if software EC is being used.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_snd_port_set_ec( pjmedia_snd_port *snd_port,
					      pj_pool_t *pool,
					      unsigned tail_ms,
					      unsigned options);


/**
 * Get current echo canceller tail length, in miliseconds. The tail length 
 * will be zero if EC is not enabled.
 *
 * @param snd_port	    The sound device port.
 * @param p_length	    Pointer to receive the tail length.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_snd_port_get_ec_tail(pjmedia_snd_port *snd_port,
						  unsigned *p_length);


/**
 * Connect a port to the sound device port. If the sound device port has a
 * sound recorder device, then this will start periodic function call to
 * the port's put_frame() function. If the sound device has a sound player
 * device, then this will start periodic function call to the port's
 * get_frame() function.
 *
 * For this version of PJMEDIA, the media port MUST have the same audio
 * settings as the sound device port, or otherwise the connection will
 * fail. This means the port MUST have the same clock_rate, channel count,
 * samples per frame, and bits per sample as the sound device port.
 *
 * @param snd_port	    The sound device port.
 * @param port		    The media port to be connected.
 *
 * @return		    PJ_SUCCESS on success, or the appropriate error
 *			    code.
 */
PJ_DECL(pj_status_t) pjmedia_snd_port_connect(pjmedia_snd_port *snd_port,
					      pjmedia_port *port);


/**
 * Retrieve the port instance currently attached to the sound port, if any.
 *
 * @param snd_port	    The sound device port.
 *
 * @return		    The port instance currently attached to the 
 *			    sound device port, or NULL if there is no port
 *			    currently attached to the sound device port.
 */
PJ_DECL(pjmedia_port*) pjmedia_snd_port_get_port(pjmedia_snd_port *snd_port);


/**
 * Disconnect currently attached port from the sound device port.
 *
 * @param snd_port	    The sound device port.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_snd_port_disconnect(pjmedia_snd_port *snd_port);


/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJMEDIA_SOUND_PORT_H__ */

