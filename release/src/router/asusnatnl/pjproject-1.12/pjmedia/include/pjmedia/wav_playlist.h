/* $Id: wav_playlist.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_WAV_PLAYLIST_H__
#define __PJMEDIA_WAV_PLAYLIST_H__

/**
 * @file wav_playlist.h
 * @brief WAV file playlist.
 */
#include <pjmedia/wav_port.h>



PJ_BEGIN_DECL


/**
 * @defgroup PJMEDIA_WAV_PLAYLIST WAV File Play List
 * @ingroup PJMEDIA_PORT
 * @brief Audio playback of multiple WAV files
 * @{
 *
 * The WAV play list port enables application to play back multiple
 * WAV files in a playlist.
 */

/**
 * Create a WAV playlist from the array of WAV file names. The WAV
 * files must have the same clock rate, number of channels, and bits
 * per sample, or otherwise this function will return error.
 *
 * @param pool		Pool to create memory buffers for this port.
 * @param port_label	Optional label to set as the port name.
 * @param file_list	Array of WAV file names.
 * @param file_count	Number of files in the array.
 * @param ptime		The duration (in miliseconds) of each frame read
 *			from this port. If the value is zero, the default
 *			duration (20ms) will be used.
 * @param options	Optional options. Application may specify 
 *			PJMEDIA_FILE_NO_LOOP to prevent play back loop.
 * @param buff_size	Buffer size to be allocated. If the value is zero or
 *			negative, the port will use default buffer size (which
 *			is about 4KB).
 * @param p_port	Pointer to receive the file port instance.
 *
 * @return		PJ_SUCCESS on success, or the appropriate error code.
 */
PJ_DECL(pj_status_t) pjmedia_wav_playlist_create(pj_pool_t *pool,
						 const pj_str_t *port_label,
						 const pj_str_t file_list[],
						 int file_count,
						 unsigned ptime,
						 unsigned options,
						 pj_ssize_t buff_size,
						 pjmedia_port **p_port);


/**
 * Register a callback to be called when the file reading has reached the
 * end of file of the last file. If the file is set to play repeatedly, 
 * then the callback will be called multiple times. Note that only one 
 * callback can be registered for each file port.
 *
 * @param port		The WAV play list port.
 * @param user_data	User data to be specified in the callback
 * @param cb		Callback to be called. If the callback returns non-
 *			PJ_SUCCESS, the playback will stop. Note that if
 *			application destroys the file port in the callback,
 *			it must return non-PJ_SUCCESS here.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t)
pjmedia_wav_playlist_set_eof_cb(pjmedia_port *port,
			        void *user_data,
			        pj_status_t (*cb)(pjmedia_port *port,
						  void *usr_data));


/**
 * @}
 */


PJ_END_DECL


#endif	/* __PJMEDIA_WAV_PLAYLIST_H__ */
