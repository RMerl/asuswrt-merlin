/* $Id: ilbc.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_CODEC_ILBC_H__
#define __PJMEDIA_CODEC_ILBC_H__

/**
 * @file pjmedia-codec/ilbc.h
 * @brief iLBC codec.
 */

#include <pjmedia-codec/types.h>

/**
 * @defgroup PJMED_ILBC iLBC Codec
 * @ingroup PJMEDIA_CODEC_CODECS
 * @brief Implementation of iLBC Codec
 * @{
 *
 * This section describes functions to initialize and register iLBC codec
 * factory to the codec manager. After the codec factory has been registered,
 * application can use @ref PJMEDIA_CODEC API to manipulate the codec.
 *
 * The iLBC codec is developed by Global IP Solutions (GIPS), formerly 
 * Global IP Sound. The iLBC offers low bitrate and graceful audio quality 
 * degradation on frame losses.
 *
 * The iLBC codec supports 16-bit PCM audio signal with sampling rate of 
 * 8000Hz operating at two modes: 20ms and 30ms frame length modes, resulting
 * in bitrates of 15.2kbps for 20ms mode and 13.33kbps for 30ms mode.
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
 * \subsubsection mode Mode
 *
 * The default mode should be set upon initialization, see
 * #pjmedia_codec_ilbc_init(). After the codec is initialized, the default
 * mode can be modified using #pjmedia_codec_mgr_set_default_param().
 *
 * In #pjmedia_codec_param, iLBC mode can be set by specifying SDP
 * format parameter "mode" in the SDP "a=fmtp" attribute for decoding
 * direction. Valid values are "20" and "30" (for 20ms and 30ms mode 
 * respectively).
 *
 * Here is an example to set up #pjmedia_codec_param to use mode 20ms:
 *  \code
    pjmedia_codec_param param;
    ...
    // setting iLBC mode in SDP
    param.setting.dec_fmtp.cnt = 1;
    param.setting.dec_fmtp.param[0].name = pj_str("mode");
    param.setting.dec_fmtp.param[0].val  = pj_str("20");
    ...
 \endcode
 */

PJ_BEGIN_DECL


/**
 * Initialize and register iLBC codec factory to pjmedia endpoint.
 *
 * @param endpt	    The pjmedia endpoint.
 * @param mode	    Default decoder mode to be used. Valid values are
 *		    20 and 30 ms. Note that encoder mode follows the
 *		    setting advertised in the remote's SDP.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_ilbc_init( pjmedia_endpt *endpt,
					      int mode );



/**
 * Unregister iLBC codec factory from pjmedia endpoint and deinitialize
 * the iLBC codec library.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_ilbc_deinit(void);


PJ_END_DECL


/**
 * @}
 */

#endif	/* __PJMEDIA_CODEC_ILBC_H__ */

