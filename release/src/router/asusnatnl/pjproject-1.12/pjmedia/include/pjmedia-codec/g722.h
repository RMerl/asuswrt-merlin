/* $Id: g722.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_CODEC_G722_H__
#define __PJMEDIA_CODEC_G722_H__

/**
 * @file pjmedia-codec/g722.h
 * @brief G.722 codec.
 */

#include <pjmedia-codec/types.h>

/**
 * @defgroup PJMED_G722 G.722 Codec
 * @ingroup PJMEDIA_CODEC_CODECS
 * @brief Implementation of G.722 Codec
 * @{
 *
 * This section describes functions to initialize and register G.722 codec
 * factory to the codec manager. After the codec factory has been registered,
 * application can use @ref PJMEDIA_CODEC API to manipulate the codec.
 *
 * The G.722 implementation uses 16-bit PCM with sampling rate 16000Hz and 
 * 20ms frame length resulting in 64kbps bitrate.
 *
 * The G.722 codec implementation is provided as part of pjmedia-codec
 * library, and does not depend on external G.722 codec implementation.
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
 * Currently none.
 */

PJ_BEGIN_DECL


/**
 * Initialize and register G.722 codec factory to pjmedia endpoint.
 *
 * @param endpt	    The pjmedia endpoint.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_g722_init(pjmedia_endpt *endpt);


/**
 * Unregister G.722 codec factory from pjmedia endpoint and cleanup
 * resources allocated by the factory.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_g722_deinit(void);


/**
 * Set the G.722 codec encoder and decoder level adjustment.
 * If the value is non-zero, then PCM input samples to the encoder will 
 * be shifted right by this value, and similarly PCM output samples from
 * the decoder will be shifted left by this value.
 *
 * Default value is PJMEDIA_G722_DEFAULT_PCM_SHIFT.
 *
 * @param val		The value
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_g722_set_pcm_shift(unsigned val);


PJ_END_DECL


/**
 * @}
 */

#endif /* __PJMEDIA_CODEC_G722_H__ */

