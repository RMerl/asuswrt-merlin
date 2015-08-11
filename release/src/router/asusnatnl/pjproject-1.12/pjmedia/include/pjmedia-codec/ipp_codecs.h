/* $Id: ipp_codecs.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_CODECS_IPP_H__
#define __PJMEDIA_CODECS_IPP_H__

/**
 * @file pjmedia-codec/ipp_codecs.h
 * @brief IPP codecs wrapper.
 */

#include <pjmedia-codec/types.h>

/**
 * @defgroup PJMED_IPP_CODEC IPP Codecs
 * @ingroup PJMEDIA_CODEC_CODECS
 * @brief Implementation of IPP codecs
 * @{
 *
 * This section describes functions to initialize and register IPP codec
 * factory to the codec manager. After the codec factory has been registered,
 * application can use @ref PJMEDIA_CODEC API to manipulate the codec.
 *
 * This codec factory contains various codecs, i.e: G.729, G.723.1, G.726, 
 * G.728, G.722.1, AMR, and AMR-WB.
 *
 *
 * \section pjmedia_codec_ipp_g729 IPP G.729
 *
 * IPP G.729 is compliant with ITU-T G.729 and Annexes A, B, C, C+, D, 
 * E, I specifications. However, currently the pjmedia implementation is
 * using Annexes A and B only.
 *
 * IPP G.729 supports 16-bit PCM audio signal with sampling rate 8000Hz, 
 * frame length 10ms, and resulting in bitrate 8000bps (annexes D and E
 * introduce bitrates 6400bps and 11800bps).
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
 * \section pjmedia_codec_ipp_g7231 IPP G.723.1
 *
 * IPP G.723.1 speech codec is compliant with ITU-T G.723.1 and Annex A
 * specifications.
 *
 * IPP G.723.1 supports 16-bit PCM audio signal with sampling rate 8000Hz, 
 * frame length 30ms, and resulting in bitrates 5300bps and 6300bps.
 *
 * By default, pjmedia implementation uses encoding bitrate of 6300bps.
 * The bitrate is signalled in-band in G.723.1 frames and interoperable.
 *
 * \subsection codec_setting Codec Settings
 *
 * General codec settings for this codec such as VAD and PLC can be 
 * manipulated through the <tt>setting</tt> field in #pjmedia_codec_param. 
 * Please see the documentation of #pjmedia_codec_param for more info.
 *
 *
 * \section pjmedia_codec_ipp_g726 IPP G.726
 *
 * IPP G.726 is compliant with ITU-T G.726 and G.726 Annex A specifications.
 *
 * IPP G.726 supports 16-bit PCM audio signal with sampling rate 8000Hz,
 * 10ms frame length and producing 16kbps, 24kbps, 32kbps, 48kbps bitrates.
 * The bitrate is specified explicitly in its encoding name, i.e: G726-16,
 * G726-24, G726-32, G726-48.
 *
 * \subsection codec_setting Codec Settings
 *
 * General codec settings for this codec such as VAD and PLC can be 
 * manipulated through the <tt>setting</tt> field in #pjmedia_codec_param. 
 * Please see the documentation of #pjmedia_codec_param for more info.
 *
 *
 * \section pjmedia_codec_ipp_g728 IPP G.728
 *
 * IPP G.728 is compliant with ITU-T G.728 with I, G, H Appendixes 
 * specifications for Low-Delay CELP coder.
 * 
 * IPP G.728 supports 16-bit PCM audio signal with sampling rate 8000Hz,
 * 20ms frame length and producing 9.6kbps, 12.8kbps, and 16kbps bitrates.
 *
 * The pjmedia implementation currently uses 16kbps bitrate only.
 *
 * \subsection codec_setting Codec Settings
 *
 * General codec settings for this codec such as VAD and PLC can be 
 * manipulated through the <tt>setting</tt> field in #pjmedia_codec_param. 
 * Please see the documentation of #pjmedia_codec_param for more info.
 *
 *
 * \section pjmedia_codec_ipp_g7221 IPP G.722.1
 *
 * The pjmedia implementation of IPP G.722.1 supports 16-bit PCM audio 
 * signal with sampling rate 16000Hz, 20ms frame length and producing 
 * 16kbps, 24kbps, and 32kbps bitrates.
 *
 * \subsection codec_setting Codec Settings
 *
 * General codec settings for this codec such as VAD and PLC can be 
 * manipulated through the <tt>setting</tt> field in #pjmedia_codec_param. 
 * Please see the documentation of #pjmedia_codec_param for more info.
 *
 * \subsubsection bitrate Bitrate
 *
 * The codec implementation supports only standard bitrates, i.e:
 * 24kbps and 32kbps. Both are enabled by default.
 *
 * \remark
 * There is a flaw in the codec manager as currently it could not 
 * differentiate G.722.1 codecs by bitrates, hence invoking 
 * #pjmedia_codec_mgr_set_default_param() may only affect a G.722.1 codec
 * with the highest priority (or first index found in codec enumeration 
 * when they have same priority) and invoking
 * #pjmedia_codec_mgr_set_codec_priority() will set priority of all G.722.1
 * codecs with sampling rate as specified.
 *
 *
 * \section pjmedia_codec_ipp_amr IPP AMR
 *
 * The IPP AMR is compliant with GSM06.90-94 specifications for GSM Adaptive
 * Multi-Rate codec.
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
 * \section pjmedia_codec_ipp_amrwb IPP AMR-WB
 *
 * The IPP AMR-WB is compliant with 3GPP TS 26.190-192, 194, 201 
 * specifications for Adaptive Multi-Rate WideBand codec.
 *
 * IPP AMR-WB supports 16-bit PCM audio signal with sampling rate 16000Hz,
 * 20ms frame length and producing various bitrates. Valid bitrates could be
 * seen in #pjmedia_codec_amrwb_bitrates. The pjmedia implementation default
 * bitrate is 15850bps.
 *
 * \subsection codec_setting Codec Settings
 *
 * General codec settings for this codec such as VAD and PLC can be 
 * manipulated through the <tt>setting</tt> field in #pjmedia_codec_param. 
 * Please see the documentation of #pjmedia_codec_param for more info.
 *
 * \subsubsection bitrate Bitrate
 *
 * By default, encoding bitrate is 15850bps. This default setting can be 
 * modified using #pjmedia_codec_mgr_set_default_param() by specifying 
 * prefered AMR bitrate in field <tt>info::avg_bps</tt> of 
 * #pjmedia_codec_param.
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
 */

PJ_BEGIN_DECL


/**
 * Initialize and register IPP codecs factory to pjmedia endpoint.
 *
 * @param endpt	    The pjmedia endpoint.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_ipp_init( pjmedia_endpt *endpt );


/**
 * Unregister IPP codecs factory from pjmedia endpoint and deinitialize
 * the IPP codecs library.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_codec_ipp_deinit(void);


PJ_END_DECL


/**
 * @}
 */

#endif	/* __PJMEDIA_CODECS_IPP_H__ */

