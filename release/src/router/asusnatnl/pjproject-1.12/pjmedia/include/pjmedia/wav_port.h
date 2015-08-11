/* $Id: wav_port.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_WAV_PORT_H__
#define __PJMEDIA_WAV_PORT_H__

/**
 * @file wav_port.h
 * @brief WAV file player and writer.
 */
#include <pjmedia/port.h>



PJ_BEGIN_DECL


/**
 * @defgroup PJMEDIA_FILE_PLAY WAV File Player
 * @ingroup PJMEDIA_PORT
 * @brief Audio playback from WAV file
 * @{
 */

/**
 * WAV file player options.
 */
enum pjmedia_file_player_option
{
    /**
     * Tell the file player to return NULL frame when the whole
     * file has been played.
     */
    PJMEDIA_FILE_NO_LOOP = 1
};


/**
 * Create a media port to play streams from a WAV file. WAV player port
 * supports for reading WAV file with uncompressed 16 bit PCM format or 
 * compressed G.711 A-law/U-law format.
 *
 * @param pool		Pool to create memory buffers for this port.
 * @param filename	File name to open.
 * @param ptime		The duration (in miliseconds) of each frame read
 *			from this port. If the value is zero, the default
 *			duration (20ms) will be used.
 * @param flags		Port creation flags.
 * @param buff_size	Buffer size to be allocated. If the value is zero or
 *			negative, the port will use default buffer size (which
 *			is about 4KB).
 * @param p_port	Pointer to receive the file port instance.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_wav_player_port_create( pj_pool_t *pool,
						     const char *filename,
						     unsigned ptime,
						     unsigned flags,
						     pj_ssize_t buff_size,
						     pjmedia_port **p_port );


/**
 * Get the data length, in bytes.
 *
 * @param port		The file player port.
 *
 * @return		The length of the data, in bytes. Upon error it will
 *			return negative value.
 */
PJ_DECL(pj_ssize_t) pjmedia_wav_player_get_len(pjmedia_port *port);


/**
 * Set the file play position of WAV player.
 *
 * @param port		The file player port.
 * @param offset	Playback position in bytes, relative to the start of
 *			the payload.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_wav_player_port_set_pos( pjmedia_port *port,
						      pj_uint32_t offset );


/**
 * Get the file play position of WAV player.
 *
 * @param port		The file player port.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_ssize_t) pjmedia_wav_player_port_get_pos( pjmedia_port *port );


/**
 * Register a callback to be called when the file reading has reached the
 * end of file. If the file is set to play repeatedly, then the callback
 * will be called multiple times. Note that only one callback can be 
 * registered for each file port.
 *
 * @param port		The file player port.
 * @param user_data	User data to be specified in the callback
 * @param cb		Callback to be called. If the callback returns non-
 *			PJ_SUCCESS, the playback will stop. Note that if
 *			application destroys the file port in the callback,
 *			it must return non-PJ_SUCCESS here.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_wav_player_set_eof_cb( pjmedia_port *port,
			       void *user_data,
			       pj_status_t (*cb)(pjmedia_port *port,
						 void *usr_data));

/**
 * @}
 */


/**
 * @defgroup PJMEDIA_FILE_REC File Writer (Recorder)
 * @ingroup PJMEDIA_PORT
 * @brief Audio capture/recording to WAV file
 * @{
 */


/**
 * WAV file writer options.
 */
enum pjmedia_file_writer_option
{
    /**
     * Tell the file writer to save the audio in PCM format.
     */
    PJMEDIA_FILE_WRITE_PCM = 0,

    /**
     * Tell the file writer to save the audio in G711 Alaw format.
     */
    PJMEDIA_FILE_WRITE_ALAW = 1,

    /**
     * Tell the file writer to save the audio in G711 Alaw format.
     */
    PJMEDIA_FILE_WRITE_ULAW = 2,
};


/**
 * Create a media port to record streams to a WAV file. Note that the port
 * must be closed properly (with #pjmedia_port_destroy()) so that the WAV
 * header can be filled with correct values (such as the file length).
 * WAV writer port supports for writing audio in uncompressed 16 bit PCM format
 * or compressed G.711 U-law/A-law format, this needs to be specified in 
 * \a flags param.
 *
 * @param pool		    Pool to create memory buffers for this port.
 * @param filename	    File name.
 * @param clock_rate	    The sampling rate.
 * @param channel_count	    Number of channels.
 * @param samples_per_frame Number of samples per frame.
 * @param bits_per_sample   Number of bits per sample (eg 16).
 * @param flags		    Port creation flags, see
 *			    #pjmedia_file_writer_option.
 * @param buff_size	    Buffer size to be allocated. If the value is 
 *			    zero or negative, the port will use default buffer
 *			    size (which is about 4KB).
 * @param p_port	    Pointer to receive the file port instance.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_wav_writer_port_create(pj_pool_t *pool,
						    const char *filename,
						    unsigned clock_rate,
						    unsigned channel_count,
						    unsigned samples_per_frame,
						    unsigned bits_per_sample,
						    unsigned flags,
						    pj_ssize_t buff_size,
						    pjmedia_port **p_port );


/**
 * Get current writing position. Note that this does not necessarily match
 * the size written to the file, since the WAV writer employs some internal
 * buffering. Also the value reported here only indicates the payload size
 * (it does not include the size of the WAV header),
 *
 * @param port		The file writer port.
 *
 * @return		Positive value to indicate the position (in bytes), 
 *			or negative value containing the error code.
 */
PJ_DECL(pj_ssize_t) pjmedia_wav_writer_port_get_pos( pjmedia_port *port );


/**
 * Register the callback to be called when the file writing has reached
 * certain size. Application can use this callback, for example, to limit
 * the size of the output file.
 *
 * @param port		The file writer port.
 * @param pos		The file position on which the callback will be called.
 * @param user_data	User data to be specified in the callback, and will be
 *			given on the callback.
 * @param cb		Callback to be called. If the callback returns non-
 *			PJ_SUCCESS, the writing will stop. Note that if 
 *			application destroys the port in the callback, it must
 *			return non-PJ_SUCCESS here.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_wav_writer_port_set_cb( pjmedia_port *port,
				pj_size_t pos,
				void *user_data,
				pj_status_t (*cb)(pjmedia_port *port,
						  void *usr_data));


/**
 * @}
 */


PJ_END_DECL


#endif	/* __PJMEDIA_WAV_PORT_H__ */
