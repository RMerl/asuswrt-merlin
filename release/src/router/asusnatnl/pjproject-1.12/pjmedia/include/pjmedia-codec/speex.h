/* $Id: speex.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_CODEC_SPEEX_H__
#define __PJMEDIA_CODEC_SPEEX_H__

/**
 * @file speex.h
 * @brief Speex codec header.
 */

#include <pjmedia-codec/types.h>

/**
 * @defgroup PJMED_SPEEX Speex Codec Family
 * @ingroup PJMEDIA_CODEC_CODECS
 * @brief Implementation of Speex codecs (narrow/wide/ultrawide-band).
 * @{
 *
 * This section describes functions to initialize and register speex codec
 * factory to the codec manager. After the codec factory has been registered,
 * application can use @ref PJMEDIA_CODEC API to manipulate the codec.
 *
 * The Speex codec uses multiple bit rates, and supports ultra-wideband 
 * (32 kHz sampling rate), wideband (16 kHz sampling rate) and narrowband 
 * (telephone quality, 8 kHz sampling rate)
 *
 * By default, the speex codec factory registers three Speex codecs:
 * "speex/8000" narrowband codec, "speex/16000" wideband codec, and 
 * "speex/32000" ultra-wideband codec. This behavior can be changed by
 * specifying #pjmedia_speex_options flags during initialization.
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
 * \subsubsection quality_vs_complexity Quality vs Complexity
 *
 * The Speex codec quality versus computational complexity and bandwidth
 * requirement can be adjusted by modifying the quality and complexity
 * setting, by calling #pjmedia_codec_speex_set_param(). The RFC 5574
 * Section 5 shows the relationship between quality setting and the
 * resulting bitrate.
 *
 * The default setting of quality is specified in 
 * #PJMEDIA_CODEC_SPEEX_DEFAULT_QUALITY. And the default setting of
 * complexity is specified in #PJMEDIA_CODEC_SPEEX_DEFAULT_COMPLEXITY.
 */

PJ_BEGIN_DECL


/**
 * Bitmask options to be passed during Speex codec factory initialization.
 */
enum pjmedia_speex_options
{
    PJMEDIA_SPEEX_NO_NB	    = 1,    /**< Disable narrowband mode.	*/
    PJMEDIA_SPEEX_NO_WB	    = 2,    /**< Disable wideband mode.		*/
    PJMEDIA_SPEEX_NO_UWB    = 4,    /**< Disable ultra-wideband mode.	*/
};


/**
 * Initialize and register Speex codec factory to pjmedia endpoint.
 *
 * @param endpt		The pjmedia endpoint.
 * @param options	Bitmask of pjmedia_speex_options (default=0).
 * @param quality	Specify encoding quality, or use -1 for default 
 *			(@see PJMEDIA_CODEC_SPEEX_DEFAULT_QUALITY).
 * @param complexity	Specify encoding complexity , or use -1 for default 
 *			(@see PJMEDIA_CODEC_SPEEX_DEFAULT_COMPLEXITY).
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_speex_init( pjmedia_endpt *endpt,
					       unsigned options,
					       int quality,
					       int complexity );


/**
 * Initialize Speex codec factory using default settings and register to 
 * pjmedia endpoint.
 *
 * @param endpt		The pjmedia endpoint.
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_speex_init_default(pjmedia_endpt *endpt);


/**
 * Change the settings of Speex codec.
 *
 * @param clock_rate	Clock rate of Speex mode to be set.
 * @param quality	Specify encoding quality, or use -1 for default 
 *			(@see PJMEDIA_CODEC_SPEEX_DEFAULT_QUALITY).
 * @param complexity	Specify encoding complexity , or use -1 for default 
 *			(@see PJMEDIA_CODEC_SPEEX_DEFAULT_COMPLEXITY).
 *
 * @return		PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_speex_set_param(unsigned clock_rate,
						   int quality,
						   int complexity);


/**
 * Unregister Speex codec factory from pjmedia endpoint and deinitialize
 * the Speex codec library.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_speex_deinit(void);


PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJMEDIA_CODEC_SPEEX_H__ */

