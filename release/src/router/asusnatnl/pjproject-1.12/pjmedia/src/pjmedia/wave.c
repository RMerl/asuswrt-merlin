/* $Id: wave.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia/wave.h>

/*
 * Change the endianness of WAVE header fields.
 */
static void wave_hdr_swap_bytes( pjmedia_wave_hdr *hdr )
{
#if defined(PJ_IS_BIG_ENDIAN) && PJ_IS_BIG_ENDIAN!=0
    hdr->riff_hdr.riff		    = pj_swap32(hdr->riff_hdr.riff);
    hdr->riff_hdr.file_len	    = pj_swap32(hdr->riff_hdr.file_len);
    hdr->riff_hdr.wave		    = pj_swap32(hdr->riff_hdr.wave);
    
    hdr->fmt_hdr.fmt		    = pj_swap32(hdr->fmt_hdr.fmt);
    hdr->fmt_hdr.len		    = pj_swap32(hdr->fmt_hdr.len);
    hdr->fmt_hdr.fmt_tag	    = pj_swap16(hdr->fmt_hdr.fmt_tag);
    hdr->fmt_hdr.nchan		    = pj_swap16(hdr->fmt_hdr.nchan);
    hdr->fmt_hdr.sample_rate	    = pj_swap32(hdr->fmt_hdr.sample_rate);
    hdr->fmt_hdr.bytes_per_sec	    = pj_swap32(hdr->fmt_hdr.bytes_per_sec);
    hdr->fmt_hdr.block_align	    = pj_swap16(hdr->fmt_hdr.block_align);
    hdr->fmt_hdr.bits_per_sample    = pj_swap16(hdr->fmt_hdr.bits_per_sample);
    
    hdr->data_hdr.data		    = pj_swap32(hdr->data_hdr.data);
    hdr->data_hdr.len		    = pj_swap32(hdr->data_hdr.len);
#else
    PJ_UNUSED_ARG(hdr);
#endif
}


PJ_DEF(void) pjmedia_wave_hdr_file_to_host( pjmedia_wave_hdr *hdr )
{
    wave_hdr_swap_bytes(hdr);
}

PJ_DEF(void) pjmedia_wave_hdr_host_to_file( pjmedia_wave_hdr *hdr )
{
    wave_hdr_swap_bytes(hdr);
}

