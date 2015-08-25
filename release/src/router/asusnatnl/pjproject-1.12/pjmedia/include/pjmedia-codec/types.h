/* $Id: types.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_CODEC_TYPES_H__
#define __PJMEDIA_CODEC_TYPES_H__

/**
 * @file types.h
 * @brief PJMEDIA-CODEC types and constants
 */

#include <pjmedia-codec/config.h>
#include <pjmedia/codec.h>

/**
 * @defgroup pjmedia_codec_types PJMEDIA-CODEC Types and Constants
 * @ingroup PJMEDIA_CODEC
 * @brief Constants used by PJMEDIA-CODEC
 * @{
 */



/**
 * These are the dynamic payload types that are used by codecs in
 * this library. Also see the header file <pjmedia/codec.h> for list
 * of static payload types.
 */
enum
{
    /* According to IANA specifications, dynamic payload types are to be in
     * the range 96-127 (inclusive). This enum is structured to place the
     * values of the payload types specified below into that range.
     *
     * PJMEDIA_RTP_PT_DYNAMIC is defined in <pjmedia/codec.h>. It is defined
     * to be 96.
     *
     * PJMEDIA_RTP_PT_TELEPHONE_EVENTS is defined in <pjmedia/config.h>.
     * The default value is 96.
     */
#if PJMEDIA_RTP_PT_TELEPHONE_EVENTS
    PJMEDIA_RTP_PT_START = PJMEDIA_RTP_PT_TELEPHONE_EVENTS,
#else
    PJMEDIA_RTP_PT_START = (PJMEDIA_RTP_PT_DYNAMIC-1),
#endif

    PJMEDIA_RTP_PT_SPEEX_NB,			/**< Speex narrowband/8KHz  */
    PJMEDIA_RTP_PT_SPEEX_WB,			/**< Speex wideband/16KHz   */
    PJMEDIA_RTP_PT_SPEEX_UWB,			/**< Speex 32KHz	    */
    PJMEDIA_RTP_PT_L16_8KHZ_MONO,		/**< L16 @ 8KHz, mono	    */
    PJMEDIA_RTP_PT_L16_8KHZ_STEREO,		/**< L16 @ 8KHz, stereo     */
    //PJMEDIA_RTP_PT_L16_11KHZ_MONO,		/**< L16 @ 11KHz, mono	    */
    //PJMEDIA_RTP_PT_L16_11KHZ_STEREO,		/**< L16 @ 11KHz, stereo    */
    PJMEDIA_RTP_PT_L16_16KHZ_MONO,		/**< L16 @ 16KHz, mono	    */
    PJMEDIA_RTP_PT_L16_16KHZ_STEREO,		/**< L16 @ 16KHz, stereo    */
    //PJMEDIA_RTP_PT_L16_22KHZ_MONO,		/**< L16 @ 22KHz, mono	    */
    //PJMEDIA_RTP_PT_L16_22KHZ_STEREO,		/**< L16 @ 22KHz, stereo    */
    //PJMEDIA_RTP_PT_L16_32KHZ_MONO,		/**< L16 @ 32KHz, mono	    */
    //PJMEDIA_RTP_PT_L16_32KHZ_STEREO,		/**< L16 @ 32KHz, stereo    */
    //PJMEDIA_RTP_PT_L16_48KHZ_MONO,		/**< L16 @ 48KHz, mono	    */
    //PJMEDIA_RTP_PT_L16_48KHZ_STEREO,		/**< L16 @ 48KHz, stereo    */
    PJMEDIA_RTP_PT_ILBC,			/**< iLBC (13.3/15.2Kbps)   */
    PJMEDIA_RTP_PT_AMR,				/**< AMR (4.75 - 12.2Kbps)  */
    PJMEDIA_RTP_PT_AMRWB,			/**< AMRWB (6.6 - 23.85Kbps)*/
    PJMEDIA_RTP_PT_AMRWBE,			/**< AMRWBE		    */
    PJMEDIA_RTP_PT_G726_16,			/**< G726 @ 16Kbps	    */
    PJMEDIA_RTP_PT_G726_24,			/**< G726 @ 24Kbps	    */
    PJMEDIA_RTP_PT_G726_32,			/**< G726 @ 32Kbps	    */
    PJMEDIA_RTP_PT_G726_40,			/**< G726 @ 40Kbps	    */
    PJMEDIA_RTP_PT_G722_1_16,			/**< G722.1 (16Kbps)	    */
    PJMEDIA_RTP_PT_G722_1_24,			/**< G722.1 (24Kbps)	    */
    PJMEDIA_RTP_PT_G722_1_32,			/**< G722.1 (32Kbps)	    */
    PJMEDIA_RTP_PT_G7221C_24,			/**< G722.1 Annex C (24Kbps)*/
    PJMEDIA_RTP_PT_G7221C_32,			/**< G722.1 Annex C (32Kbps)*/
    PJMEDIA_RTP_PT_G7221C_48,			/**< G722.1 Annex C (48Kbps)*/
    PJMEDIA_RTP_PT_G7221_RSV1,			/**< G722.1 reserve	    */
    PJMEDIA_RTP_PT_G7221_RSV2,			/**< G722.1 reserve	    */

    /* Caution!
     * Ensure the value of the last pt above is <= 127.
     */
};

/**
 * @}
 */


#endif	/* __PJMEDIA_CODEC_TYPES_H__ */
