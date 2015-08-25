/* $Id: errno.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_ERRNO_H__
#define __PJMEDIA_ERRNO_H__

/**
 * @file errno.h Error Codes
 * @brief PJMEDIA specific error codes.
 */

#include <pjmedia/types.h>
#include <pj/errno.h>

/**
 * @defgroup PJMEDIA_ERRNO Error Codes
 * @ingroup PJMEDIA_BASE
 * @brief PJMEDIA specific error codes.
 * @{
 */


PJ_BEGIN_DECL


/**
 * Start of error code relative to PJ_ERRNO_START_USER.
 */
#define PJMEDIA_ERRNO_START       (PJ_ERRNO_START_USER + PJ_ERRNO_SPACE_SIZE)
#define PJMEDIA_ERRNO_END         (PJMEDIA_ERRNO_START + PJ_ERRNO_SPACE_SIZE - 1)


/**
 * Mapping from PortAudio error codes to pjmedia error space.
 */
#define PJMEDIA_PORTAUDIO_ERRNO_START (PJMEDIA_ERRNO_END-10000)
#define PJMEDIA_PORTAUDIO_ERRNO_END   (PJMEDIA_PORTAUDIO_ERRNO_START + 10000 -1)
/**
 * Convert PortAudio error code to PJMEDIA error code.
 * PortAudio error code range: 0 >= err >= -10000
 */
#define PJMEDIA_ERRNO_FROM_PORTAUDIO(err)   ((int)PJMEDIA_PORTAUDIO_ERRNO_START-err)


#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)

 /**
 * Mapping from LibSRTP error codes to pjmedia error space.
 */
#define PJMEDIA_LIBSRTP_ERRNO_START (PJMEDIA_ERRNO_END-10200)
#define PJMEDIA_LIBSRTP_ERRNO_END   (PJMEDIA_LIBSRTP_ERRNO_START + 200 - 1)
/**
 * Convert LibSRTP error code to PJMEDIA error code.
 * LibSRTP error code range: 0 <= err < 200
 */
#define PJMEDIA_ERRNO_FROM_LIBSRTP(err)   (PJMEDIA_LIBSRTP_ERRNO_START+err)

#endif

/************************************************************
 * GENERIC/GENERAL PJMEDIA ERRORS
 ***********************************************************/
/**
 * @hideinitializer
 * General/unknown PJMEDIA error.
 */
#define PJMEDIA_ERROR		    (PJMEDIA_ERRNO_START+1)	/* 220001 */


/************************************************************
 * SDP ERRORS
 ***********************************************************/
/**
 * @hideinitializer
 * Generic invalid SDP descriptor.
 */
#define PJMEDIA_SDP_EINSDP	    (PJMEDIA_ERRNO_START+20)    /* 220020 */
/**
 * @hideinitializer
 * Invalid SDP version.
 */
#define PJMEDIA_SDP_EINVER	    (PJMEDIA_ERRNO_START+21)    /* 220021 */
/**
 * @hideinitializer
 * Invalid SDP origin (o=) line.
 */
#define PJMEDIA_SDP_EINORIGIN	    (PJMEDIA_ERRNO_START+22)    /* 220022 */
/**
 * @hideinitializer
 * Invalid SDP time (t=) line.
 */
#define PJMEDIA_SDP_EINTIME	    (PJMEDIA_ERRNO_START+23)    /* 220023 */
/**
 * @hideinitializer
 * Empty SDP subject/name (s=) line.
 */
#define PJMEDIA_SDP_EINNAME	    (PJMEDIA_ERRNO_START+24)    /* 220024 */
/**
 * @hideinitializer
 * Invalid SDP connection info (c=) line.
 */
#define PJMEDIA_SDP_EINCONN	    (PJMEDIA_ERRNO_START+25)    /* 220025 */
/**
 * @hideinitializer
 * Missing SDP connection info line.
 */
#define PJMEDIA_SDP_EMISSINGCONN    (PJMEDIA_ERRNO_START+26)    /* 220026 */
/**
 * @hideinitializer
 * Invalid attribute (a=) line.
 */
#define PJMEDIA_SDP_EINATTR	    (PJMEDIA_ERRNO_START+27)    /* 220027 */
/**
 * @hideinitializer
 * Invalid rtpmap attribute.
 */
#define PJMEDIA_SDP_EINRTPMAP	    (PJMEDIA_ERRNO_START+28)    /* 220028 */
/**
 * @hideinitializer
 * rtpmap attribute is too long.
 */
#define PJMEDIA_SDP_ERTPMAPTOOLONG  (PJMEDIA_ERRNO_START+29)    /* 220029 */
/**
 * @hideinitializer
 * rtpmap is missing for dynamic payload type.
 */
#define PJMEDIA_SDP_EMISSINGRTPMAP  (PJMEDIA_ERRNO_START+30)    /* 220030 */
/**
 * @hideinitializer
 * Invalid SDP media (m=) line.
 */
#define PJMEDIA_SDP_EINMEDIA	    (PJMEDIA_ERRNO_START+31)    /* 220031 */
/**
 * @hideinitializer
 * No payload format in the media stream.
 */
#define PJMEDIA_SDP_ENOFMT	    (PJMEDIA_ERRNO_START+32)    /* 220032 */
/**
 * @hideinitializer
 * Invalid payload type in media.
 */
#define PJMEDIA_SDP_EINPT	    (PJMEDIA_ERRNO_START+33)    /* 220033 */
/**
 * @hideinitializer
 * Invalid SDP "fmtp" attribute.
 */
#define PJMEDIA_SDP_EINFMTP	    (PJMEDIA_ERRNO_START+34)    /* 220034 */
/**
 * @hideinitializer
 * Invalid SDP "rtcp" attribute.
 */
#define PJMEDIA_SDP_EINRTCP	    (PJMEDIA_ERRNO_START+35)    /* 220035 */
/**
 * @hideinitializer
 * Invalid SDP media transport protocol.
 */
#define PJMEDIA_SDP_EINPROTO	    (PJMEDIA_ERRNO_START+36)    /* 220036 */


/************************************************************
 * SDP NEGOTIATOR ERRORS
 ***********************************************************/
/**
 * @hideinitializer
 * Invalid state to perform the specified operation.
 */
#define PJMEDIA_SDPNEG_EINSTATE	    (PJMEDIA_ERRNO_START+40)    /* 220040 */
/**
 * @hideinitializer
 * No initial local SDP.
 */
#define PJMEDIA_SDPNEG_ENOINITIAL   (PJMEDIA_ERRNO_START+41)    /* 220041 */
/**
 * @hideinitializer
 * No currently active SDP.
 */
#define PJMEDIA_SDPNEG_ENOACTIVE    (PJMEDIA_ERRNO_START+42)    /* 220042 */
/**
 * @hideinitializer
 * No current offer or answer.
 */
#define PJMEDIA_SDPNEG_ENONEG	    (PJMEDIA_ERRNO_START+43)    /* 220043 */
/**
 * @hideinitializer
 * Media count mismatch in offer and answer.
 */
#define PJMEDIA_SDPNEG_EMISMEDIA    (PJMEDIA_ERRNO_START+44)    /* 220044 */
/**
 * @hideinitializer
 * Media type is different in the remote answer.
 */
#define PJMEDIA_SDPNEG_EINVANSMEDIA (PJMEDIA_ERRNO_START+45)    /* 220045 */
/**
 * @hideinitializer
 * Transport type is different in the remote answer.
 */
#define PJMEDIA_SDPNEG_EINVANSTP    (PJMEDIA_ERRNO_START+46)    /* 220046 */
/**
 * @hideinitializer
 * No common media payload is provided in the answer.
 */
#define PJMEDIA_SDPNEG_EANSNOMEDIA  (PJMEDIA_ERRNO_START+47)    /* 220047 */
/**
 * @hideinitializer
 * No media is active after negotiation.
 */
#define PJMEDIA_SDPNEG_ENOMEDIA	    (PJMEDIA_ERRNO_START+48)    /* 220048 */
/**
 * @hideinitializer
 * No suitable codec for remote offer.
 */
#define PJMEDIA_SDPNEG_NOANSCODEC   (PJMEDIA_ERRNO_START+49)    /* 220049 */
/**
 * @hideinitializer
 * No suitable telephone-event for remote offer.
 */
#define PJMEDIA_SDPNEG_NOANSTELEVENT (PJMEDIA_ERRNO_START+50)   /* 220050 */
/**
 * @hideinitializer
 * No suitable answer for unknown remote offer.
 */
#define PJMEDIA_SDPNEG_NOANSUNKNOWN (PJMEDIA_ERRNO_START+51)    /* 220051 */


/************************************************************
 * SDP COMPARISON STATUS
 ***********************************************************/
/**
 * @hideinitializer
 * SDP media stream not equal.
 */
#define PJMEDIA_SDP_EMEDIANOTEQUAL  (PJMEDIA_ERRNO_START+60)    /* 220060 */
/**
 * @hideinitializer
 * Port number in SDP media descriptor not equal.
 */
#define PJMEDIA_SDP_EPORTNOTEQUAL   (PJMEDIA_ERRNO_START+61)    /* 220061 */
/**
 * @hideinitializer
 * Transport in SDP media descriptor not equal.
 */
#define PJMEDIA_SDP_ETPORTNOTEQUAL  (PJMEDIA_ERRNO_START+62)    /* 220062 */
/**
 * @hideinitializer
 * Media format in SDP media descriptor not equal.
 */
#define PJMEDIA_SDP_EFORMATNOTEQUAL (PJMEDIA_ERRNO_START+63)    /* 220063 */
/**
 * @hideinitializer
 * SDP connection description not equal.
 */
#define PJMEDIA_SDP_ECONNNOTEQUAL   (PJMEDIA_ERRNO_START+64)    /* 220064 */
/**
 * @hideinitializer
 * SDP attributes not equal.
 */
#define PJMEDIA_SDP_EATTRNOTEQUAL   (PJMEDIA_ERRNO_START+65)    /* 220065 */
/**
 * @hideinitializer
 * SDP media direction not equal.
 */
#define PJMEDIA_SDP_EDIRNOTEQUAL    (PJMEDIA_ERRNO_START+66)    /* 220066 */
/**
 * @hideinitializer
 * SDP fmtp attribute not equal.
 */
#define PJMEDIA_SDP_EFMTPNOTEQUAL   (PJMEDIA_ERRNO_START+67)    /* 220067 */
/**
 * @hideinitializer
 * SDP ftpmap attribute not equal.
 */
#define PJMEDIA_SDP_ERTPMAPNOTEQUAL (PJMEDIA_ERRNO_START+68)    /* 220068 */
/**
 * @hideinitializer
 * SDP session descriptor not equal.
 */
#define PJMEDIA_SDP_ESESSNOTEQUAL   (PJMEDIA_ERRNO_START+69)    /* 220069 */
/**
 * @hideinitializer
 * SDP origin not equal.
 */
#define PJMEDIA_SDP_EORIGINNOTEQUAL (PJMEDIA_ERRNO_START+70)    /* 220070 */
/**
 * @hideinitializer
 * SDP name/subject not equal.
 */
#define PJMEDIA_SDP_ENAMENOTEQUAL   (PJMEDIA_ERRNO_START+71)    /* 220071 */
/**
 * @hideinitializer
 * SDP time not equal.
 */
#define PJMEDIA_SDP_ETIMENOTEQUAL   (PJMEDIA_ERRNO_START+72)    /* 220072 */


/************************************************************
 * CODEC
 ***********************************************************/
/**
 * @hideinitializer
 * Unsupported codec.
 */
#define PJMEDIA_CODEC_EUNSUP	    (PJMEDIA_ERRNO_START+80)    /* 220080 */
/**
 * @hideinitializer
 * Codec internal creation error.
 */
#define PJMEDIA_CODEC_EFAILED	    (PJMEDIA_ERRNO_START+81)    /* 220081 */
/**
 * @hideinitializer
 * Codec frame is too short.
 */
#define PJMEDIA_CODEC_EFRMTOOSHORT  (PJMEDIA_ERRNO_START+82)    /* 220082 */
/**
 * @hideinitializer
 * PCM buffer is too short.
 */
#define PJMEDIA_CODEC_EPCMTOOSHORT  (PJMEDIA_ERRNO_START+83)    /* 220083 */
/**
 * @hideinitializer
 * Invalid codec frame length.
 */
#define PJMEDIA_CODEC_EFRMINLEN	    (PJMEDIA_ERRNO_START+84)    /* 220084 */
/**
 * @hideinitializer
 * Invalid PCM frame length.
 */
#define PJMEDIA_CODEC_EPCMFRMINLEN  (PJMEDIA_ERRNO_START+85)    /* 220085 */
/**
 * @hideinitializer
 * Invalid mode.
 */
#define PJMEDIA_CODEC_EINMODE	    (PJMEDIA_ERRNO_START+86)    /* 220086 */


/************************************************************
 * MEDIA
 ***********************************************************/
/**
 * @hideinitializer
 * Invalid remote IP address (in SDP).
 */
#define PJMEDIA_EINVALIDIP	    (PJMEDIA_ERRNO_START+100)    /* 220100 */
/**
 * @hideinitializer
 * Asymetric codec is not supported.
 */
#define PJMEDIA_EASYMCODEC	    (PJMEDIA_ERRNO_START+101)    /* 220101 */
/**
 * @hideinitializer
 * Invalid payload type.
 */
#define PJMEDIA_EINVALIDPT	    (PJMEDIA_ERRNO_START+102)    /* 220102 */
/**
 * @hideinitializer
 * Missing rtpmap.
 */
#define PJMEDIA_EMISSINGRTPMAP	    (PJMEDIA_ERRNO_START+103)    /* 220103 */
/**
 * @hideinitializer
 * Invalid media type.
 */
#define PJMEDIA_EINVALIMEDIATYPE    (PJMEDIA_ERRNO_START+104)    /* 220104 */
/**
 * @hideinitializer
 * Remote does not support DTMF.
 */
#define PJMEDIA_EREMOTENODTMF	    (PJMEDIA_ERRNO_START+105)    /* 220105 */
/**
 * @hideinitializer
 * Invalid DTMF digit.
 */
#define PJMEDIA_RTP_EINDTMF	    (PJMEDIA_ERRNO_START+106)    /* 220106 */
/**
 * @hideinitializer
 * Remote does not support RFC 2833
 */
#define PJMEDIA_RTP_EREMNORFC2833   (PJMEDIA_ERRNO_START+107)    /* 220107 */



/************************************************************
 * RTP SESSION ERRORS
 ***********************************************************/
/**
 * @hideinitializer
 * General invalid RTP packet error.
 */
#define PJMEDIA_RTP_EINPKT	    (PJMEDIA_ERRNO_START+120)    /* 220120 */
/**
 * @hideinitializer
 * Invalid RTP packet packing.
 */
#define PJMEDIA_RTP_EINPACK	    (PJMEDIA_ERRNO_START+121)    /* 220121 */
/**
 * @hideinitializer
 * Invalid RTP packet version.
 */
#define PJMEDIA_RTP_EINVER	    (PJMEDIA_ERRNO_START+122)    /* 220122 */
/**
 * @hideinitializer
 * RTP SSRC id mismatch.
 */
#define PJMEDIA_RTP_EINSSRC	    (PJMEDIA_ERRNO_START+123)    /* 220123 */
/**
 * @hideinitializer
 * RTP payload type mismatch.
 */
#define PJMEDIA_RTP_EINPT	    (PJMEDIA_ERRNO_START+124)    /* 220124 */
/**
 * @hideinitializer
 * Invalid RTP packet length.
 */
#define PJMEDIA_RTP_EINLEN	    (PJMEDIA_ERRNO_START+125)    /* 220125 */
/**
 * @hideinitializer
 * RTP session restarted.
 */
#define PJMEDIA_RTP_ESESSRESTART    (PJMEDIA_ERRNO_START+130)    /* 220130 */
/**
 * @hideinitializer
 * RTP session in probation
 */
#define PJMEDIA_RTP_ESESSPROBATION  (PJMEDIA_ERRNO_START+131)    /* 220131 */
/**
 * @hideinitializer
 * Bad RTP sequence number
 */
#define PJMEDIA_RTP_EBADSEQ	    (PJMEDIA_ERRNO_START+132)    /* 220132 */
/**
 * @hideinitializer
 * RTP media port destination is not configured
 */
#define PJMEDIA_RTP_EBADDEST	    (PJMEDIA_ERRNO_START+133)    /* 220133 */
/**
 * @hideinitializer
 * RTP is not configured.
 */
#define PJMEDIA_RTP_ENOCONFIG	    (PJMEDIA_ERRNO_START+134)    /* 220134 */


/************************************************************
 * PORT ERRORS
 ***********************************************************/
/**
 * @hideinitializer
 * Generic incompatible port error.
 */
#define PJMEDIA_ENOTCOMPATIBLE	    (PJMEDIA_ERRNO_START+160)    /* 220160 */
/**
 * @hideinitializer
 * Incompatible clock rate
 */
#define PJMEDIA_ENCCLOCKRATE	    (PJMEDIA_ERRNO_START+161)    /* 220161 */
/**
 * @hideinitializer
 * Incompatible samples per frame
 */
#define PJMEDIA_ENCSAMPLESPFRAME    (PJMEDIA_ERRNO_START+162)    /* 220162 */
/**
 * @hideinitializer
 * Incompatible media type
 */
#define PJMEDIA_ENCTYPE		    (PJMEDIA_ERRNO_START+163)    /* 220163 */
/**
 * @hideinitializer
 * Incompatible bits per sample
 */
#define PJMEDIA_ENCBITS		    (PJMEDIA_ERRNO_START+164)    /* 220164 */
/**
 * @hideinitializer
 * Incompatible bytes per frame
 */
#define PJMEDIA_ENCBYTES	    (PJMEDIA_ERRNO_START+165)    /* 220165 */
/**
 * @hideinitializer
 * Incompatible number of channels
 */
#define PJMEDIA_ENCCHANNEL	    (PJMEDIA_ERRNO_START+166)    /* 220166 */


/************************************************************
 * FILE ERRORS
 ***********************************************************/
/**
 * @hideinitializer
 * Not a valid WAVE file.
 */
#define PJMEDIA_ENOTVALIDWAVE	    (PJMEDIA_ERRNO_START+180)    /* 220180 */
/**
 * @hideinitializer
 * Unsupported WAVE file.
 */
#define PJMEDIA_EWAVEUNSUPP	    (PJMEDIA_ERRNO_START+181)    /* 220181 */
/**
 * @hideinitializer
 * Wave file too short.
 */
#define PJMEDIA_EWAVETOOSHORT	    (PJMEDIA_ERRNO_START+182)    /* 220182 */
/**
 * @hideinitializer
 * Sound frame is too large for file buffer.
 */
#define PJMEDIA_EFRMFILETOOBIG	    (PJMEDIA_ERRNO_START+183)    /* 220183 */


/************************************************************
 * SOUND DEVICE ERRORS
 ***********************************************************/
/**
 * @hideinitializer
 * No suitable audio capture device.
 */
#define PJMEDIA_ENOSNDREC	    (PJMEDIA_ERRNO_START+200)    /* 220200 */
/**
 * @hideinitializer
 * No suitable audio playback device.
 */
#define PJMEDIA_ENOSNDPLAY	    (PJMEDIA_ERRNO_START+201)    /* 220201 */
/**
 * @hideinitializer
 * Invalid sound device ID.
 */
#define PJMEDIA_ESNDINDEVID	    (PJMEDIA_ERRNO_START+202)    /* 220202 */
/**
 * @hideinitializer
 * Invalid sample format for sound device.
 */
#define PJMEDIA_ESNDINSAMPLEFMT	    (PJMEDIA_ERRNO_START+203)    /* 220203 */


#if defined(PJMEDIA_HAS_SRTP) && (PJMEDIA_HAS_SRTP != 0)
/************************************************************
 * SRTP TRANSPORT ERRORS
 ***********************************************************/
/**
 * @hideinitializer
 * SRTP crypto-suite name not match the offerer tag.
 */
#define PJMEDIA_SRTP_ECRYPTONOTMATCH (PJMEDIA_ERRNO_START+220)   /* 220220 */
/**
 * @hideinitializer
 * Invalid SRTP key length for specific crypto.
 */
#define PJMEDIA_SRTP_EINKEYLEN	    (PJMEDIA_ERRNO_START+221)    /* 220221 */
/**
 * @hideinitializer
 * Unsupported SRTP crypto-suite.
 */
#define PJMEDIA_SRTP_ENOTSUPCRYPTO  (PJMEDIA_ERRNO_START+222)    /* 220222 */
/**
 * @hideinitializer
 * SRTP SDP contains ambigue answer.
 */
#define PJMEDIA_SRTP_ESDPAMBIGUEANS (PJMEDIA_ERRNO_START+223)    /* 220223 */
/**
 * @hideinitializer
 * Duplicated crypto tag.
 */
#define PJMEDIA_SRTP_ESDPDUPCRYPTOTAG (PJMEDIA_ERRNO_START+224)  /* 220224 */
/**
 * @hideinitializer
 * Invalid crypto attribute.
 */
#define PJMEDIA_SRTP_ESDPINCRYPTO   (PJMEDIA_ERRNO_START+225)    /* 220225 */
/**
 * @hideinitializer
 * Invalid crypto tag.
 */
#define PJMEDIA_SRTP_ESDPINCRYPTOTAG (PJMEDIA_ERRNO_START+226)   /* 220226 */
/**
 * @hideinitializer
 * Invalid SDP media transport for SRTP.
 */
#define PJMEDIA_SRTP_ESDPINTRANSPORT (PJMEDIA_ERRNO_START+227)   /* 220227 */
/**
 * @hideinitializer
 * SRTP crypto attribute required in SDP.
 */
#define PJMEDIA_SRTP_ESDPREQCRYPTO  (PJMEDIA_ERRNO_START+228)    /* 220228 */
/**
 * @hideinitializer
 * Secure transport required in SDP media descriptor.
 */
#define PJMEDIA_SRTP_ESDPREQSECTP   (PJMEDIA_ERRNO_START+229)    /* 220229 */

#endif /* PJMEDIA_HAS_SRTP */


/**
 * Get error message for the specified error code. Note that this
 * function is only able to decode PJMEDIA specific error code.
 * Application should use pj_strerror(), which should be able to
 * decode all error codes belonging to all subsystems (e.g. pjlib,
 * pjmedia, pjsip, etc).
 *
 * @param status    The error code.
 * @param buffer    The buffer where to put the error message.
 * @param bufsize   Size of the buffer.
 *
 * @return	    The error message as NULL terminated string,
 *                  wrapped with pj_str_t.
 */
PJ_DECL(pj_str_t) pjmedia_strerror( pj_status_t status, char *buffer,
				    pj_size_t bufsize);


PJ_END_DECL

/**
 * @}
 */


#endif	/* __PJMEDIA_ERRNO_H__ */

