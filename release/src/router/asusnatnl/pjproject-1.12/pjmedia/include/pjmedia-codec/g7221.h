/* $Id: g7221.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_CODECS_G7221_H__
#define __PJMEDIA_CODECS_G7221_H__

/**
 * @file pjmedia-codec/g7221.h
 * @brief G722.1 codec.
 */

#include <pjmedia-codec/types.h>

/**
 * @defgroup PJMED_G7221_CODEC G.722.1 Codec (Siren7/Siren14)
 * @ingroup PJMEDIA_CODEC_CODECS
 * @brief Implementation of G.722.1 codec
 * @{
 *
 * <b>G.722.1 licensed from Polycom?/b><br />
 * <b>G.722.1 Annex C licensed from Polycom?/b>
 *
 * This section describes functions to initialize and register G.722.1 codec
 * factory to the codec manager. After the codec factory has been registered,
 * application can use @ref PJMEDIA_CODEC API to manipulate the codec.
 *
 * PJMEDIA G722.1 codec implementation is based on ITU-T Recommendation 
 * G.722.1 (05/2005) C fixed point implementation including its Annex C.
 *
 * G.722.1 is a low complexity codec that supports 7kHz and 14kHz audio 
 * bandwidth working at bitrates ranging from 16kbps to 48kbps. It may be
 * used with speech or music inputs.
 *
 *
 * \section codec_setting Codec Settings
 *
 * \subsection general_setting General Settings
 *
 * General codec settings for this codec such as VAD and PLC can be 
 * manipulated through the <tt>setting</tt> field in #pjmedia_codec_param. 
 * Please see the documentation of #pjmedia_codec_param for more info.
 *
 * \subsection specific_setting Codec Specific Settings
 *
 * The following settings are applicable for this codec.
 *
 * \subsubsection bitrate Bitrate
 *
 * The codec implementation supports standard and non-standard bitrates.
 * Use #pjmedia_codec_g7221_set_mode() to enable or disable the bitrates.
 *
 * By default, only standard bitrates are enabled upon initialization:
 * - for 7kHz audio bandwidth (16kHz sampling rate): 24kbps and 32kbps,
 * - for 14kHz audio bandwidth (32kHz sampling rate): 24kbps, 32kbps, and
 *   48kbps.
 *
 * The usage of non-standard bitrates must follow these requirements:
 * - for 7kHz audio bandwidth (16kHz sampling rate): 16000 to 32000 bps, 
 *   multiplication of 400
 * - for 14kHz audio bandwidth (32kHz sampling rate): 24000 to 48000 bps,
 *   multiplication of 400
 *
 * \note
 * Currently only up to two non-standard modes can be enabled.
 *
 * \remark
 * There is a flaw in the codec manager as currently it could not
 * differentiate G.722.1 codecs by bitrates, hence invoking 
 * #pjmedia_codec_mgr_set_default_param() may only affect a G.722.1 codec
 * with the highest priority (or first index found in codec enumeration 
 * when they have same priority) and invoking
 * #pjmedia_codec_mgr_set_codec_priority() will set priority of all G.722.1
 * codecs with sampling rate as specified.
 */

PJ_BEGIN_DECL

/**
 * Initialize and register G.722.1 codec factory to pjmedia endpoint.
 *
 * @param endpt	    The pjmedia endpoint.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_g7221_init( pjmedia_endpt *endpt );


/**
 * Enable and disable G.722.1 mode. By default, the standard modes are 
 * enabled upon initialization, i.e.:
 * - sampling rate 16kHz, bitrate 24kbps and 32kbps.
 * - sampling rate 32kHz, bitrate 24kbps, 32kbps, and 48kbps.
 * This function can also be used for enabling non-standard modes.
 * Note that currently only up to two non-standard modes can be enabled
 * at one time.
 *
 * @param sample_rate	PCM sampling rate, in Hz, valid values are only 
 *			16000 and 32000.
 * @param bitrate	G722.1 bitrate, in bps, the valid values are
 *			standard and non-standard bitrates as described 
 *			above.
 * @param enabled	PJ_TRUE for enabling specified mode.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_g7221_set_mode(unsigned sample_rate, 
						  unsigned bitrate, 
						  pj_bool_t enabled);

/**
 * Set the G.722.1 codec encoder and decoder level adjustment. 
 * If the value is non-zero, then PCM input samples to the encoder will 
 * be shifted right by this value, and similarly PCM output samples from
 * the decoder will be shifted left by this value.
 *
 * \note
 * This function is also applicable for G722.1 implementation with IPP
 * back-end.
 *
 * Default value is PJMEDIA_G7221_DEFAULT_PCM_SHIFT.
 *
 * @param val		The value
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_g7221_set_pcm_shift(int val);



/**
 * Unregister G.722.1 codecs factory from pjmedia endpoint.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_g7221_deinit(void);


PJ_END_DECL


/**
 * @}
 */

#endif	/* __PJMEDIA_CODECS_G7221_H__ */

