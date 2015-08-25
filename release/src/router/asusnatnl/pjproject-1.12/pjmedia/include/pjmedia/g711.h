/* $Id: g711.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_G711_H__
#define __PJMEDIA_G711_H__

/**
 * @file g711.h
 * @brief G711 Codec
 */

#include <pjmedia-codec/types.h>

/**
 * @defgroup PJMED_G711 G.711 Codec
 * @ingroup PJMEDIA_CODEC_CODECS
 * @brief Standard G.711/PCMA and PCMU codec.
 * @{
 *
 * This section describes functions to initialize and register G.711 codec
 * factory to the codec manager. After the codec factory has been registered,
 * application can use @ref PJMEDIA_CODEC API to manipulate the codec.
 *
 * The G.711 is an ultra low complexity codecs and in trade-off it results
 * in high bitrate, i.e: 64kbps for 16-bit PCM with sampling rate 8000Hz.
 *
 * The factory contains two main compression algorithms, PCMU/u-Law and 
 * PCMA/A-Law.
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
 * Initialize and register G711 codec factory to pjmedia endpoint.
 * This will register PCMU and PCMA codec, in that order.
 *
 * @param endpt		The pjmedia endpoint.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_g711_init(pjmedia_endpt *endpt);



/**
 * Unregister G711 codec factory from pjmedia endpoint.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_g711_deinit(void);


PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJMEDIA_G711_H__ */

