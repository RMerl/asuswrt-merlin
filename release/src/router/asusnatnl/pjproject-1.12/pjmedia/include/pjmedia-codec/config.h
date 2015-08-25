/* $Id: config.h 3817 2011-10-14 06:41:51Z bennylp $ */
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
#ifndef __PJMEDIA_CODEC_CONFIG_H__
#define __PJMEDIA_CODEC_CONFIG_H__

/**
 * @file config.h
 * @brief PJMEDIA-CODEC compile time settings
 */

/**
 * @defgroup pjmedia_codec_config PJMEDIA-CODEC Compile Time Settings
 * @ingroup PJMEDIA_CODEC
 * @brief Various compile time settings such as to enable/disable codecs
 * @{
 */

#include <pjmedia/types.h>


/*
 * Include config_auto.h if autoconf is used (PJ_AUTOCONF is set)
 */
#if defined(PJ_AUTOCONF)
#   include <pjmedia-codec/config_auto.h>
#endif

/**
 * Unless specified otherwise, L16 codec is included by default.
 */
#ifndef PJMEDIA_HAS_L16_CODEC
#   define PJMEDIA_HAS_L16_CODEC    1
#endif


/**
 * Unless specified otherwise, GSM codec is included by default.
 */
#ifndef PJMEDIA_HAS_GSM_CODEC
#   define PJMEDIA_HAS_GSM_CODEC    1
#endif


/**
 * Unless specified otherwise, Speex codec is included by default.
 */
#ifndef PJMEDIA_HAS_SPEEX_CODEC
#   define PJMEDIA_HAS_SPEEX_CODEC    1
#endif

/**
 * Speex codec default complexity setting.
 */
#ifndef PJMEDIA_CODEC_SPEEX_DEFAULT_COMPLEXITY
#   define PJMEDIA_CODEC_SPEEX_DEFAULT_COMPLEXITY   2
#endif

/**
 * Speex codec default quality setting. Please note that pjsua-lib may override
 * this setting via its codec quality setting (i.e PJSUA_DEFAULT_CODEC_QUALITY).
 */
#ifndef PJMEDIA_CODEC_SPEEX_DEFAULT_QUALITY
#   define PJMEDIA_CODEC_SPEEX_DEFAULT_QUALITY	    8
#endif


/**
 * Unless specified otherwise, iLBC codec is included by default.
 */
#ifndef PJMEDIA_HAS_ILBC_CODEC
#   define PJMEDIA_HAS_ILBC_CODEC    1
#endif


/**
 * Unless specified otherwise, G.722 codec is included by default.
 */
#ifndef PJMEDIA_HAS_G722_CODEC
#   define PJMEDIA_HAS_G722_CODEC    1
#endif


/**
 * Default G.722 codec encoder and decoder level adjustment. The G.722
 * specifies that it uses 14 bit PCM for input and output, while PJMEDIA
 * normally uses 16 bit PCM, so the conversion is done by applying
 * level adjustment. If the value is non-zero, then PCM input samples to
 * the encoder will be shifted right by this value, and similarly PCM
 * output samples from the decoder will be shifted left by this value.
 *
 * This can be changed at run-time after initialization by calling
 * #pjmedia_codec_g722_set_pcm_shift().
 *
 * Default: 2.
 */
#ifndef PJMEDIA_G722_DEFAULT_PCM_SHIFT
#   define PJMEDIA_G722_DEFAULT_PCM_SHIFT	    2
#endif


/**
 * Specifies whether G.722 PCM shifting should be stopped when clipping
 * detected in the decoder. Enabling this feature can be useful when
 * talking to G.722 implementation that uses 16 bit PCM for G.722 input/
 * output (for any reason it seems to work) and the PCM shifting causes
 * audio clipping.
 *
 * See also #PJMEDIA_G722_DEFAULT_PCM_SHIFT.
 *
 * Default: enabled.
 */
#ifndef PJMEDIA_G722_STOP_PCM_SHIFT_ON_CLIPPING
#   define PJMEDIA_G722_STOP_PCM_SHIFT_ON_CLIPPING  1
#endif


/**
 * Enable the features provided by Intel IPP libraries, for example
 * codecs such as G.729, G.723.1, G.726, G.728, G.722.1, and AMR.
 *
 * By default this is disabled. Please follow the instructions in
 * http://trac.pjsip.org/repos/wiki/Intel_IPP_Codecs on how to setup
 * Intel IPP with PJMEDIA.
 */
#ifndef PJMEDIA_HAS_INTEL_IPP
#   define PJMEDIA_HAS_INTEL_IPP		0
#endif


/**
 * Visual Studio only: when this option is set, the Intel IPP libraries
 * will be automatically linked to application using pragma(comment)
 * constructs. This is convenient, however it will only link with
 * the stub libraries and the Intel IPP DLL's will be required when
 * distributing the application.
 *
 * If application wants to link with the different types of the Intel IPP
 * libraries (for example, the static libraries), it must set this option
 * to zero and specify the Intel IPP libraries in the application's input
 * library specification manually.
 *
 * Default 1.
 */
#ifndef PJMEDIA_AUTO_LINK_IPP_LIBS
#   define PJMEDIA_AUTO_LINK_IPP_LIBS		1
#endif


/**
 * Enable Intel IPP AMR codec. This also needs to be enabled when AMR WB
 * codec is enabled. This option is only used when PJMEDIA_HAS_INTEL_IPP 
 * is enabled.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_INTEL_IPP_CODEC_AMR
#   define PJMEDIA_HAS_INTEL_IPP_CODEC_AMR	1
#endif


/**
 * Enable Intel IPP AMR wideband codec. The PJMEDIA_HAS_INTEL_IPP_CODEC_AMR
 * option must also be enabled to use this codec. This option is only used 
 * when PJMEDIA_HAS_INTEL_IPP is enabled.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_INTEL_IPP_CODEC_AMRWB
#   define PJMEDIA_HAS_INTEL_IPP_CODEC_AMRWB	1
#endif


/**
 * Enable Intel IPP G.729 codec. This option is only used when
 * PJMEDIA_HAS_INTEL_IPP is enabled.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_INTEL_IPP_CODEC_G729
#   define PJMEDIA_HAS_INTEL_IPP_CODEC_G729	1
#endif


/**
 * Enable Intel IPP G.723.1 codec. This option is only used when
 * PJMEDIA_HAS_INTEL_IPP is enabled.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_INTEL_IPP_CODEC_G723_1
#   define PJMEDIA_HAS_INTEL_IPP_CODEC_G723_1	1
#endif


/**
 * Enable Intel IPP G.726 codec. This option is only used when
 * PJMEDIA_HAS_INTEL_IPP is enabled.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_INTEL_IPP_CODEC_G726
#   define PJMEDIA_HAS_INTEL_IPP_CODEC_G726	1
#endif


/**
 * Enable Intel IPP G.728 codec. This option is only used when
 * PJMEDIA_HAS_INTEL_IPP is enabled.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_INTEL_IPP_CODEC_G728
#   define PJMEDIA_HAS_INTEL_IPP_CODEC_G728	1
#endif


/**
 * Enable Intel IPP G.722.1 codec. This option is only used when
 * PJMEDIA_HAS_INTEL_IPP is enabled.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_INTEL_IPP_CODEC_G722_1
#   define PJMEDIA_HAS_INTEL_IPP_CODEC_G722_1	1
#endif

/**
 * Enable Passthrough codecs.
 *
 * Default: 0
 */
#ifndef PJMEDIA_HAS_PASSTHROUGH_CODECS
#   define PJMEDIA_HAS_PASSTHROUGH_CODECS	0
#endif

/**
 * Enable AMR passthrough codec.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_PASSTHROUGH_CODEC_AMR
#   define PJMEDIA_HAS_PASSTHROUGH_CODEC_AMR	1
#endif

/**
 * Enable G.729 passthrough codec.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_PASSTHROUGH_CODEC_G729
#   define PJMEDIA_HAS_PASSTHROUGH_CODEC_G729	1
#endif

/**
 * Enable iLBC passthrough codec.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_PASSTHROUGH_CODEC_ILBC
#   define PJMEDIA_HAS_PASSTHROUGH_CODEC_ILBC	1
#endif

/**
 * Enable PCMU passthrough codec.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMU
#   define PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMU	1
#endif

/**
 * Enable PCMA passthrough codec.
 *
 * Default: 1
 */
#ifndef PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMA
#   define PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMA	1
#endif

/* If passthrough and PCMU/PCMA are enabled, disable the software
 * G.711 codec
 */
#if PJMEDIA_HAS_PASSTHROUGH_CODECS && \
    (PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMU || PJMEDIA_HAS_PASSTHROUGH_CODEC_PCMA)
#   undef PJMEDIA_HAS_G711_CODEC
#   define PJMEDIA_HAS_G711_CODEC		0
#endif


/**
 * G.722.1 codec is disabled by default.
 */
#ifndef PJMEDIA_HAS_G7221_CODEC
#   define PJMEDIA_HAS_G7221_CODEC		0
#endif

/**
 * Enable OpenCORE AMR-NB codec.
 * See https://trac.pjsip.org/repos/ticket/1388 for some info.
 *
 * Default: 0
 */
#ifndef PJMEDIA_HAS_OPENCORE_AMRNB_CODEC
#   define PJMEDIA_HAS_OPENCORE_AMRNB_CODEC	0
#endif

/**
 * Link with libopencore-amrXX via pragma comment on Visual Studio.
 * This option only makes sense if PJMEDIA_HAS_OPENCORE_AMRNB_CODEC
 * is enabled.
 *
 * Default: 1
 */
#ifndef PJMEDIA_AUTO_LINK_OPENCORE_AMR_LIBS
#  define PJMEDIA_AUTO_LINK_OPENCORE_AMR_LIBS	1
#endif

/**
 * Link with libopencore-amrXX.a that has been produced with gcc.
 * This option only makes sense if PJMEDIA_HAS_OPENCORE_AMRNB_CODEC
 * and PJMEDIA_AUTO_LINK_OPENCORE_AMR_LIBS are enabled.
 *
 * Default: 1
 */
#ifndef PJMEDIA_OPENCORE_AMR_BUILT_WITH_GCC
#   define PJMEDIA_OPENCORE_AMR_BUILT_WITH_GCC	1
#endif


/**
 * Default G.722.1 codec encoder and decoder level adjustment. 
 * If the value is non-zero, then PCM input samples to the encoder will 
 * be shifted right by this value, and similarly PCM output samples from
 * the decoder will be shifted left by this value.
 *
 * This can be changed at run-time after initialization by calling
 * #pjmedia_codec_g7221_set_pcm_shift().
 */
#ifndef PJMEDIA_G7221_DEFAULT_PCM_SHIFT
#   define PJMEDIA_G7221_DEFAULT_PCM_SHIFT	1
#endif


/**
 * Enabling both G.722.1 codec implementations, internal PJMEDIA and IPP,
 * may cause problem in SDP, i.e: payload types duplications. So, let's 
 * just trap such case here at compile time.
 *
 * Application can control which implementation to be used by manipulating
 * PJMEDIA_HAS_G7221_CODEC and PJMEDIA_HAS_INTEL_IPP_CODEC_G722_1 in
 * config_site.h.
 */
#if (PJMEDIA_HAS_G7221_CODEC != 0) && (PJMEDIA_HAS_INTEL_IPP != 0) && \
    (PJMEDIA_HAS_INTEL_IPP_CODEC_G722_1 != 0)
#   error Only one G.722.1 implementation can be enabled at the same time. \
	  Please use PJMEDIA_HAS_G7221_CODEC and \
	  PJMEDIA_HAS_INTEL_IPP_CODEC_G722_1 in your config_site.h \
	  to control which implementation to be used.
#endif

/**
 * @}
 */

#endif	/* __PJMEDIA_CODEC_CONFIG_H__ */
