/* $Id: mp3_port.h 1177 2007-04-09 07:06:08Z bennylp $ */
/* 
 * Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
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

/*
 * Contributed by:
 *  Toni < buldozer at aufbix dot org >
 */

#ifndef __PJMEDIA_MP3_PORT_H__
#define __PJMEDIA_MP3_PORT_H__


/**
 * @file mp3_port.h
 * @brief MP3 writer
 */
#include <pjmedia/port.h>

/**
 * @defgroup PJMEDIA_MP3_FILE_REC MP3 Audio File Writer (Recorder)
 * @ingroup PJMEDIA_PORT
 * @brief MP3 Audio File Writer (Recorder)
 * @{
 *
 * This section describes MP3 file writer. Currently it only works on Windows
 * using BladeEncDLL of the LAME MP3 encoder. <b>Note that the LAME_ENC.DLL 
 * file must exist in the PATH so that the encoder can work properly.</b>
 *
 * The MP3 file writer is created with #pjmedia_mp3_writer_port_create() which
 * among other things specifies the desired file name and audio properties.
 * It then takes PCM input when #pjmedia_port_put_frame() is called and encode
 * the PCM input into MP3 streams before writing it to the .mp3 file.
 */


PJ_BEGIN_DECL


/**
 * This structure contains encoding options that can be specified during
 * MP3 writer port creation. Application should always zero the structure
 * before setting some value to make sure that default options will be used.
 */
typedef struct pjmedia_mp3_encoder_option
{
    /** Specify whether variable bit rate should be used. Variable bitrate
     *  would normally produce better quality at the expense of probably
     *  larger file.
     */
    pj_bool_t	vbr;

    /** Target bitrate, in bps. If VBR is enabled, this settings specifies 
     *  the  average bit-rate requested, and will make the encoder ignore 
     *  the quality setting. For CBR, this specifies the actual bitrate,
     *  and if this option is zero, it will be set to the sampling rate
     *  multiplied by number of channels.
     */
    unsigned	bit_rate;

    /** Encoding quality, 0-9, with 0 is the highest quality. For VBR, the 
     *  quality setting will only take effect when bit_rate setting is zero.
     */
    unsigned	quality;

} pjmedia_mp3_encoder_option;


/**
 * Create a media port to record PCM media to a MP3 file. After the port
 * is created, application can call #pjmedia_port_put_frame() to feed the
 * port with PCM frames. The port then will encode the PCM frame into MP3
 * stream, and store it to MP3 file specified in the argument.
 *
 * When application has finished with writing MP3 file, it must destroy the
 * media port with #pjmedia_port_destroy() so that the MP3 file can be
 * closed properly.
 *
 * @param pool		    Pool to create memory buffers for this port.
 * @param filename	    File name.
 * @param clock_rate	    The sampling rate.
 * @param channel_count	    Number of channels.
 * @param samples_per_frame Number of samples per frame.
 * @param bits_per_sample   Number of bits per sample (eg 16).
 * @param option	    Optional option to set encoding parameters.
 * @param p_port	    Pointer to receive the file port instance.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) 
pjmedia_mp3_writer_port_create(pj_pool_t *pool,
			       const char *filename,
			       unsigned clock_rate,
			       unsigned channel_count,
			       unsigned samples_per_frame,
			       unsigned bits_per_sample,
			       const pjmedia_mp3_encoder_option *option,
			       pjmedia_port **p_port );

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
pjmedia_mp3_writer_port_set_cb( pjmedia_port *port,
				pj_size_t pos,
				void *user_data,
				pj_status_t (*cb)(pjmedia_port *port,
							void *usr_data));


/**
 * @}
 */


PJ_END_DECL

#endif	/* __PJMEDIA_MP3_PORT_H__ */

