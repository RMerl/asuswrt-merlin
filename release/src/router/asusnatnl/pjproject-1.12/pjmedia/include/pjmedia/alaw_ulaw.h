/* $Id: alaw_ulaw.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_ALAW_ULAW_H__
#define __PJMEDIA_ALAW_ULAW_H__

#include <pjmedia/types.h>

PJ_BEGIN_DECL

#if defined(PJMEDIA_HAS_ALAW_ULAW_TABLE) && PJMEDIA_HAS_ALAW_ULAW_TABLE!=0

extern const pj_uint8_t pjmedia_linear2ulaw_tab[16384];
extern const pj_uint8_t pjmedia_linear2alaw_tab[16384];
extern const pj_int16_t pjmedia_ulaw2linear_tab[256];
extern const pj_int16_t pjmedia_alaw2linear_tab[256];


/**
 * Convert 16-bit linear PCM value to 8-bit A-Law.
 *
 * @param pcm_val   16-bit linear PCM value.
 * @return	    8-bit A-Law value.
 */
#define pjmedia_linear2alaw(pcm_val)	\
	    pjmedia_linear2alaw_tab[(((pj_int16_t)pcm_val) >> 2) & 0x3fff]

/**
 * Convert 8-bit A-Law value to 16-bit linear PCM value.
 *
 * @param chara_val 8-bit A-Law value.
 * @return	    16-bit linear PCM value.
 */
#define pjmedia_alaw2linear(chara_val)	\
	    pjmedia_alaw2linear_tab[chara_val]

/**
 * Convert 16-bit linear PCM value to 8-bit U-Law.
 *
 * @param pcm_val   16-bit linear PCM value.
 * @return	    U-bit A-Law value.
 */
#define pjmedia_linear2ulaw(pcm_val)	\
	    pjmedia_linear2ulaw_tab[(((pj_int16_t)pcm_val) >> 2) & 0x3fff]

/**
 * Convert 8-bit U-Law value to 16-bit linear PCM value.
 *
 * @param u_val	    8-bit U-Law value.
 * @return	    16-bit linear PCM value.
 */
#define pjmedia_ulaw2linear(u_val)	\
	    pjmedia_ulaw2linear_tab[u_val]

/**
 * Convert 8-bit A-Law value to 8-bit U-Law value.
 *
 * @param aval	    8-bit A-Law value.
 * @return	    8-bit U-Law value.
 */
#define pjmedia_alaw2ulaw(aval)		\
	    pjmedia_linear2ulaw(pjmedia_alaw2linear(aval))

/**
 * Convert 8-bit U-Law value to 8-bit A-Law value.
 *
 * @param uval	    8-bit U-Law value.
 * @return	    8-bit A-Law value.
 */
#define pjmedia_ulaw2alaw(uval)		\
	    pjmedia_linear2alaw(pjmedia_ulaw2linear(uval))


#else

/**
 * Convert 16-bit linear PCM value to 8-bit A-Law.
 *
 * @param pcm_val   16-bit linear PCM value.
 * @return	    8-bit A-Law value.
 */
PJ_DECL(pj_uint8_t) pjmedia_linear2alaw(int pcm_val);

/**
 * Convert 8-bit A-Law value to 16-bit linear PCM value.
 *
 * @param chara_val 8-bit A-Law value.
 * @return	    16-bit linear PCM value.
 */
PJ_DECL(int) pjmedia_alaw2linear(unsigned chara_val);

/**
 * Convert 16-bit linear PCM value to 8-bit U-Law.
 *
 * @param pcm_val   16-bit linear PCM value.
 * @return	    U-bit A-Law value.
 */
PJ_DECL(unsigned char) pjmedia_linear2ulaw(int pcm_val);

/**
 * Convert 8-bit U-Law value to 16-bit linear PCM value.
 *
 * @param u_val	    8-bit U-Law value.
 * @return	    16-bit linear PCM value.
 */
PJ_DECL(int) pjmedia_ulaw2linear(unsigned char u_val);

/**
 * Convert 8-bit A-Law value to 8-bit U-Law value.
 *
 * @param aval	    8-bit A-Law value.
 * @return	    8-bit U-Law value.
 */
PJ_DECL(unsigned char) pjmedia_alaw2ulaw(unsigned char aval);

/**
 * Convert 8-bit U-Law value to 8-bit A-Law value.
 *
 * @param uval	    8-bit U-Law value.
 * @return	    8-bit A-Law value.
 */
PJ_DECL(unsigned char) pjmedia_ulaw2alaw(unsigned char uval);

#endif

/**
 * Encode 16-bit linear PCM data to 8-bit U-Law data.
 *
 * @param dst	    Destination buffer for 8-bit U-Law data.
 * @param src	    Source, 16-bit linear PCM data.
 * @param count	    Number of samples.
 */
PJ_INLINE(void) pjmedia_ulaw_encode(pj_uint8_t *dst, const pj_int16_t *src, 
				    pj_size_t count)
{
    const pj_int16_t *end = src + count;
    
    while (src < end) {
	*dst++ = pjmedia_linear2ulaw(*src++);
    }
}

/**
 * Encode 16-bit linear PCM data to 8-bit A-Law data.
 *
 * @param dst	    Destination buffer for 8-bit A-Law data.
 * @param src	    Source, 16-bit linear PCM data.
 * @param count	    Number of samples.
 */
PJ_INLINE(void) pjmedia_alaw_encode(pj_uint8_t *dst, const pj_int16_t *src, 
				    pj_size_t count)
{
    const pj_int16_t *end = src + count;
    
    while (src < end) {
	*dst++ = pjmedia_linear2alaw(*src++);
    }
}

/**
 * Decode 8-bit U-Law data to 16-bit linear PCM data.
 *
 * @param dst	    Destination buffer for 16-bit PCM data.
 * @param src	    Source, 8-bit U-Law data.
 * @param len	    Encoded frame/source length in bytes.
 */
PJ_INLINE(void) pjmedia_ulaw_decode(pj_int16_t *dst, const pj_uint8_t *src, 
				    pj_size_t len)
{
    const pj_uint8_t *end = src + len;
    
    while (src < end) {
	*dst++ = pjmedia_ulaw2linear(*src++);
    }
}

/**
 * Decode 8-bit A-Law data to 16-bit linear PCM data.
 *
 * @param dst	    Destination buffer for 16-bit PCM data.
 * @param src	    Source, 8-bit A-Law data.
 * @param len	    Encoded frame/source length in bytes.
 */
PJ_INLINE(void) pjmedia_alaw_decode(pj_int16_t *dst, const pj_uint8_t *src, 
				    pj_size_t len)
{
    const pj_uint8_t *end = src + len;
    
    while (src < end) {
	*dst++ = pjmedia_alaw2linear(*src++);
    }
}

PJ_END_DECL

#endif	/* __PJMEDIA_ALAW_ULAW_H__ */

