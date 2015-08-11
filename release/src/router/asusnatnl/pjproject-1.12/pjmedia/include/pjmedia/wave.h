/* $Id: wave.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_WAVE_H__
#define __PJMEDIA_WAVE_H__


/**
 * @file wave.h
 * @brief WAVE file manipulation.
 */
#include <pjmedia/types.h>

/**
 * @defgroup PJMEDIA_FILE_FORMAT File Formats
 * @brief Supported file formats
 */


/**
 * @defgroup PJMEDIA_WAVE WAVE Header
 * @ingroup PJMEDIA_FILE_FORMAT
 * @brief Representation of RIFF/WAVE file format
 * @{
 *
 * This the the low level representation of RIFF/WAVE file format. For
 * higher abstraction, please see \ref PJMEDIA_FILE_PLAY and 
 * \ref PJMEDIA_FILE_REC.
 */


PJ_BEGIN_DECL

/**
 * Standard RIFF tag to identify RIFF file format in the WAVE header.
 */
#define PJMEDIA_RIFF_TAG	('F'<<24|'F'<<16|'I'<<8|'R')

/**
 * Standard WAVE tag to identify WAVE header.
 */
#define PJMEDIA_WAVE_TAG	('E'<<24|'V'<<16|'A'<<8|'W')

/**
 * Standard FMT tag to identify format chunks.
 */
#define PJMEDIA_FMT_TAG		(' '<<24|'t'<<16|'m'<<8|'f')

/**
 * Standard DATA tag to identify data chunks.
 */
#define PJMEDIA_DATA_TAG	('a'<<24|'t'<<16|'a'<<8|'d')

/**
 * Standard FACT tag to identify fact chunks.
 */
#define PJMEDIA_FACT_TAG	('t'<<24|'c'<<16|'a'<<8|'f')


/**
 * Enumeration of format compression tag.
 */
typedef enum {
    PJMEDIA_WAVE_FMT_TAG_PCM	= 1,
    PJMEDIA_WAVE_FMT_TAG_ALAW	= 6,
    PJMEDIA_WAVE_FMT_TAG_ULAW	= 7
} pjmedia_wave_fmt_tag;


/**
 * This file describes the simpler/canonical version of a WAVE file.
 * It does not support the full RIFF format specification.
 */
#pragma pack(2)
struct pjmedia_wave_hdr
{
    /** This structure describes RIFF WAVE file header */
    struct {
	pj_uint32_t riff;		/**< "RIFF" ASCII tag.		*/
	pj_uint32_t file_len;		/**< File length minus 8 bytes	*/
	pj_uint32_t wave;		/**< "WAVE" ASCII tag.		*/
    } riff_hdr;

    /** This structure describes format chunks/header  */
    struct {
	pj_uint32_t fmt;		/**< "fmt " ASCII tag.		*/
	pj_uint32_t len;		/**< 16 for PCM.		*/
	pj_uint16_t fmt_tag;		/**< 1 for PCM			*/
	pj_uint16_t nchan;		/**< Number of channels.	*/
	pj_uint32_t sample_rate;	/**< Sampling rate.		*/
	pj_uint32_t bytes_per_sec;	/**< Average bytes per second.	*/
	pj_uint16_t block_align;	/**< nchannels * bits / 8	*/
	pj_uint16_t bits_per_sample;	/**< Bits per sample.		*/
    } fmt_hdr;

    /** The data header preceeds the actual data in the file. */
    struct {
	pj_uint32_t data;		/**< "data" ASCII tag.		*/
	pj_uint32_t len;		/**< Data length.		*/
    } data_hdr;
};
#pragma pack()

/**
 * @see pjmedia_wave_hdr
 */
typedef struct pjmedia_wave_hdr pjmedia_wave_hdr;

/**
 * This structure describes generic RIFF subchunk header.
 */
typedef struct pjmedia_wave_subchunk
{
    pj_uint32_t	    id;			/**< Subchunk ASCII tag.	    */
    pj_uint32_t	    len;		/**< Length following this field    */
} pjmedia_wave_subchunk;


/**
 * Normalize subchunk header from little endian (the representation of
 * RIFF file) into host's endian.
 */
#if defined(PJ_IS_BIG_ENDIAN) && PJ_IS_BIG_ENDIAN!=0
#   define PJMEDIA_WAVE_NORMALIZE_SUBCHUNK(ch)  \
	    do { \
		(ch)->id = pj_swap32((ch)->id); \
		(ch)->len = pj_swap32((ch)->len); \
	    } while (0)
#else
#   define PJMEDIA_WAVE_NORMALIZE_SUBCHUNK(ch)
#endif


/**
 * On big-endian hosts, this function swaps the byte order of the values
 * in the WAVE header fields. On little-endian hosts, this function does 
 * nothing.
 *
 * Application SHOULD call this function after reading the WAVE header
 * chunks from a file.
 *
 * @param hdr	    The WAVE header.
 */
PJ_DECL(void) pjmedia_wave_hdr_file_to_host( pjmedia_wave_hdr *hdr );


/**
 * On big-endian hosts, this function swaps the byte order of the values
 * in the WAVE header fields. On little-endian hosts, this function does 
 * nothing.
 *
 * Application SHOULD call this function before writing the WAVE header
 * to a file.
 *
 * @param hdr	    The WAVE header.
 */
PJ_DECL(void) pjmedia_wave_hdr_host_to_file( pjmedia_wave_hdr *hdr );


PJ_END_DECL

/**
 * @}
 */


#endif	/* __PJMEDIA_WAVE_H__ */

