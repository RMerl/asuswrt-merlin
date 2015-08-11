/* $Id: mem_port.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_MEM_PORT_H__
#define __PJMEDIA_MEM_PORT_H__

/**
 * @file mem_port.h
 * @brief Memory based media playback/capture port
 */
#include <pjmedia/port.h>

PJ_BEGIN_DECL


/**
 * @defgroup PJMEDIA_MEM_PLAYER Memory/Buffer-based Playback Port
 * @ingroup PJMEDIA_PORT
 * @brief Media playback from a fixed size memory buffer
 * @{
 *
 * A memory/buffer based playback port is used to play media from a fixed
 * size buffer. This is useful over @ref PJMEDIA_FILE_PLAY for 
 * situation where filesystems are not available in the target system.
 */


/**
 * Memory player options.
 */
enum pjmedia_mem_player_option
{
    /**
     * Tell the memory player to return NULL frame when the whole
     * buffer has been played instead of rewinding the buffer back
     * to start position.
     */
    PJMEDIA_MEM_NO_LOOP = 1
};


/**
 * Create the buffer based playback to play the media from the specified
 * buffer.
 *
 * @param pool		    Pool to allocate memory for the port structure.
 * @param buffer	    The buffer to play the media from, which should
 *			    be available throughout the life time of the port.
 *			    The player plays the media directly from this
 *			    buffer (i.e. no copying is done).
 * @param size		    The size of the buffer, in bytes.
 * @param clock_rate	    Sampling rate.
 * @param channel_count	    Number of channels.
 * @param samples_per_frame Number of samples per frame.
 * @param bits_per_sample   Number of bits per sample.
 * @param options	    Option flags, see #pjmedia_mem_player_option
 * @param p_port	    Pointer to receive the port instance.
 *
 * @return		    PJ_SUCCESS on success, or the appropriate
 *			    error code.
 */
PJ_DECL(pj_status_t) pjmedia_mem_player_create(pj_pool_t *pool,
					       const void *buffer,
					       pj_size_t size,
					       unsigned clock_rate,
					       unsigned channel_count,
					       unsigned samples_per_frame,
					       unsigned bits_per_sample,
					       unsigned options,
					       pjmedia_port **p_port );


/**
 * Register a callback to be called when the buffer reading has reached the
 * end of buffer. If the player is set to play repeatedly, then the callback
 * will be called multiple times. Note that only one callback can be 
 * registered for each player port.
 *
 * @param port		The memory player port.
 * @param user_data	User data to be specified in the callback
 * @param cb		Callback to be called. If the callback returns non-
 *			PJ_SUCCESS, the playback will stop. Note that if
 *			application destroys the player port in the callback,
 *			it must return non-PJ_SUCCESS here.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_mem_player_set_eof_cb( pjmedia_port *port,
			       void *user_data,
			       pj_status_t (*cb)(pjmedia_port *port,
						 void *usr_data));


/**
 * @}
 */

/**
 * @defgroup PJMEDIA_MEM_CAPTURE Memory/Buffer-based Capture Port
 * @ingroup PJMEDIA_PORT
 * @brief Media capture to fixed size memory buffer
 * @{
 *
 * A memory based capture is used to save media streams to a fixed size
 * buffer. This is useful over @ref PJMEDIA_FILE_REC for 
 * situation where filesystems are not available in the target system.
 */

/**
 * Create media port to capture/record media into a fixed size buffer.
 *
 * @param pool		    Pool to allocate memory for the port structure.
 * @param buffer	    The buffer to record the media to, which should
 *			    be available throughout the life time of the port.
 * @param size		    The maximum size of the buffer, in bytes.
 * @param clock_rate	    Sampling rate.
 * @param channel_count	    Number of channels.
 * @param samples_per_frame Number of samples per frame.
 * @param bits_per_sample   Number of bits per sample.
 * @param options	    Option flags.
 * @param p_port	    Pointer to receive the port instance.
 *
 * @return		    PJ_SUCCESS on success, or the appropriate
 *			    error code.
 */
PJ_DECL(pj_status_t) pjmedia_mem_capture_create(pj_pool_t *pool,
						void *buffer,
						pj_size_t size,
						unsigned clock_rate,
						unsigned channel_count,
						unsigned samples_per_frame,
						unsigned bits_per_sample,
						unsigned options,
						pjmedia_port **p_port);


/**
 * Register a callback to be called when no space left in the buffer.
 * Note that when a callback is registered, this callback will also be
 * called when application destroys the port and the callback has not 
 * been called before.
 *
 * @param port		The memory recorder port.
 * @param user_data	User data to be specified in the callback
 * @param cb		Callback to be called. If the callback returns non-
 *			PJ_SUCCESS, the recording will stop. In other cases
 *                      recording will be restarted and the rest of the frame
 *                      will be stored starting from the beginning of the 
 *			buffer. Note that if application destroys the capture
 *			port in the callback, it must return non-PJ_SUCCESS 
 *			here.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t)
pjmedia_mem_capture_set_eof_cb(pjmedia_port *port,
                               void *user_data,
                               pj_status_t (*cb)(pjmedia_port *port,
						 void *usr_data));

/**
 * Return the current size of the recorded data in the buffer.
 *
 * @param port		The memory recorder port.
 * @return		The size of buffer data..
 */
PJ_DECL(pj_size_t)
pjmedia_mem_capture_get_size(pjmedia_port *port);


/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJMEDIA_MEM_PORT_H__ */
