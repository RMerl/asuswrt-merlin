/* $Id: passthrough.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_CODECS_PASSTHROUGH_H__
#define __PJMEDIA_CODECS_PASSTHROUGH_H__

/**
 * @file pjmedia-codec/passthrough.h
 * @brief Passthrough codecs.
 */

#include <pjmedia-codec/types.h>

/**
 * @defgroup PJMED_PASSTHROUGH_CODEC Passthrough Codecs
 * @ingroup PJMEDIA_CODEC_CODECS
 * @brief Implementation of passthrough codecs
 * @{
 *
 * This section describes functions to initialize and register passthrough 
 * codecs factory to the codec manager. After the codec factory has been 
 * registered, application can use @ref PJMEDIA_CODEC API to manipulate 
 * the codec.
 *
 * Passthrough codecs are codecs wrapper that does NOT perform encoding 
 * or decoding, it just PACK and PARSE encoded audio data from/into RTP 
 * payload. This will accomodate pjmedia ports which work with encoded
 * audio data, e.g: encoded audio files, sound device with capability
 * of playing/recording encoded audio data.
 *
 * This codec factory contains various codecs, i.e: G.729, iLBC,
 * AMR, and G.711.
 *
 *
 * \section pjmedia_codec_passthrough_g729 Passthrough G.729
 *
 * G.729 supports 16-bit PCM audio signal with sampling rate 8000Hz, 
 * frame length 10ms, and resulting in bitrate 8000bps.
 *
 * \subsection codec_setting Codec Settings
 *
 * General codec settings for this codec such as VAD and PLC can be 
 * manipulated through the <tt>setting</tt> field in #pjmedia_codec_param. 
 * Please see the documentation of #pjmedia_codec_param for more info.
 *
 * Note that G.729 VAD status should be signalled in SDP, see more
 * description below.
 *
 * \subsubsection annexb Annex B
 *
 * The capability of VAD/DTX is specified in Annex B.
 *
 * By default, Annex B is enabled. This default setting of Annex B can 
 * be modified using #pjmedia_codec_mgr_set_default_param().
 *
 * In #pjmedia_codec_param, Annex B is configured via VAD setting and
 * format parameter "annexb" in the SDP "a=fmtp" attribute in
 * decoding fmtp field. Valid values are "yes" and "no",
 * the implementation default is "yes". When this parameter is omitted
 * in the SDP, the value will be "yes" (RFC 4856 Section 2.1.9).
 *
 * Here is an example of modifying default setting of Annex B to
 * be disabled using #pjmedia_codec_mgr_set_default_param():
 \code
    pjmedia_codec_param param;

    pjmedia_codec_mgr_get_default_param(.., &param);
    ...
    // Set VAD
    param.setting.vad = 0;
    // Set SDP format parameter
    param.setting.dec_fmtp.cnt = 1;
    param.setting.dec_fmtp.param[0].name = pj_str("annexb");
    param.setting.dec_fmtp.param[0].val  = pj_str("no");
    ...
    pjmedia_codec_mgr_set_default_param(.., &param);
 \endcode
 *
 * \note
 * The difference of Annex B status in SDP offer/answer may be considered as 
 * incompatible codec in SDP negotiation.
 *
 * 
 * \section pjmedia_codec_passthrough_ilbc Passthrough iLBC
 *
 * The iLBC codec is developed by Global IP Solutions (GIPS), formerly 
 * Global IP Sound. The iLBC offers low bitrate and graceful audio quality 
 * degradation on frame losses.
 *
 * The iLBC codec supports 16-bit PCM audio signal with sampling rate of 
 * 8000Hz operating at two modes: 20ms and 30ms frame length modes, resulting
 * in bitrates of 15.2kbps for 20ms mode and 13.33kbps for 30ms mode.
 *
 * \subsection codec_setting Codec Settings
 *
 * General codec settings for this codec such as VAD and PLC can be 
 * manipulated through the <tt>setting</tt> field in #pjmedia_codec_param. 
 * Please see the documentation of #pjmedia_codec_param for more info.
 *
 * \subsubsection mode Mode
 *
 * The default mode should be set upon initialization, see
 * #pjmedia_codec_passthrough_init2(). After the codec is initialized, the
 * default mode can be modified using #pjmedia_codec_mgr_set_default_param().
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
 *
 *
 * \section pjmedia_codec_passthrough_amr Passthrough AMR
 *
 * IPP AMR supports 16-bit PCM audio signal with sampling rate 8000Hz,
 * 20ms frame length and producing various bitrates that ranges from 4.75kbps
 * to 12.2kbps.
 *
 * \subsection codec_setting Codec Settings
 *
 * General codec settings for this codec such as VAD and PLC can be 
 * manipulated through the <tt>setting</tt> field in #pjmedia_codec_param. 
 * Please see the documentation of #pjmedia_codec_param for more info.
 *
 * \subsubsection bitrate Bitrate
 *
 * By default, encoding bitrate is 7400bps. This default setting can be 
 * modified using #pjmedia_codec_mgr_set_default_param() by specifying 
 * prefered AMR bitrate in field <tt>info::avg_bps</tt> of 
 * #pjmedia_codec_param. Valid bitrates could be seen in 
 * #pjmedia_codec_amrnb_bitrates.
 *
 * \subsubsection payload_format Payload Format
 *
 * There are two AMR payload format types, bandwidth-efficient and
 * octet-aligned. Default setting is using octet-aligned. This default payload
 * format can be modified using #pjmedia_codec_mgr_set_default_param().
 *
 * In #pjmedia_codec_param, payload format can be set by specifying SDP 
 * format parameters "octet-align" in the SDP "a=fmtp" attribute for 
 * decoding direction. Valid values are "0" (for bandwidth efficient mode)
 * and "1" (for octet-aligned mode).
 *
 * \subsubsection mode_set Mode-Set
 * 
 * Mode-set is used for restricting AMR modes in decoding direction.
 *
 * By default, no mode-set restriction applied. This default setting can be 
 * be modified using #pjmedia_codec_mgr_set_default_param().
 *
 * In #pjmedia_codec_param, mode-set could be specified via format parameters
 * "mode-set" in the SDP "a=fmtp" attribute for decoding direction. Valid 
 * value is a comma separated list of modes from the set 0 - 7, e.g: 
 * "4,5,6,7". When this parameter is omitted, no mode-set restrictions applied.
 *
 * Here is an example of modifying AMR default codec param:
 \code
    pjmedia_codec_param param;

    pjmedia_codec_mgr_get_default_param(.., &param);
    ...
    // set default encoding bitrate to the highest 12.2kbps
    param.info.avg_bps = 12200;

    // restrict decoding bitrate to 10.2kbps and 12.2kbps only
    param.setting.dec_fmtp.param[0].name = pj_str("mode-set");
    param.setting.dec_fmtp.param[0].val  = pj_str("6,7");

    // also set to use bandwidth-efficient payload format
    param.setting.dec_fmtp.param[1].name = pj_str("octet-align");
    param.setting.dec_fmtp.param[1].val  = pj_str("0");

    param.setting.dec_fmtp.cnt = 2;
    ...
    pjmedia_codec_mgr_set_default_param(.., &param);
 \endcode
 * 
 *
 * \section pjmedia_codec_passthrough_g711 Passthrough G.711
 *
 * The G.711 is an ultra low complexity codecs and in trade-off it results
 * in high bitrate, i.e: 64kbps for 16-bit PCM with sampling rate 8000Hz.
 *
 * The factory contains two main compression algorithms, PCMU/u-Law and 
 * PCMA/A-Law.
 *
 * \subsection codec_setting Codec Settings
 *
 * General codec settings for this codec such as VAD and PLC can be 
 * manipulated through the <tt>setting</tt> field in #pjmedia_codec_param. 
 * Please see the documentation of #pjmedia_codec_param for more info.
 */

PJ_BEGIN_DECL


/** 
 * Codec passthrough configuration settings.
 */
typedef struct pjmedia_codec_passthrough_setting
{
    unsigned		 fmt_cnt;	/**< Number of encoding formats
					     to be enabled.		*/
    pjmedia_format	*fmts;		/**< Encoding formats to be 
					     enabled.			*/
    unsigned		 ilbc_mode;	/**< iLBC default mode.		*/
} pjmedia_codec_passthrough_setting;


/**
 * Initialize and register passthrough codecs factory to pjmedia endpoint,
 * all supported encoding formats will be enabled.
 *
 * @param endpt	    The pjmedia endpoint.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_passthrough_init( pjmedia_endpt *endpt );


/**
 * Initialize and register passthrough codecs factory to pjmedia endpoint
 * with only specified encoding formats enabled.
 *
 * @param endpt	    The pjmedia endpoint.
 * @param setting   The settings.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_passthrough_init2(
		       pjmedia_endpt *endpt,
		       const pjmedia_codec_passthrough_setting *setting);


/**
 * Unregister passthrough codecs factory from pjmedia endpoint.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_passthrough_deinit(void);


PJ_END_DECL


/**
 * @}
 */

#endif	/* __PJMEDIA_CODECS_PASSTHROUGH_H__ */

