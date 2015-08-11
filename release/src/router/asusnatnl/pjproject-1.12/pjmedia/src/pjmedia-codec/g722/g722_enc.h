/* $Id: g722_enc.h 3553 2011-05-05 06:14:19Z nanang $ */
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
/*
 * Based on implementation found in Carnegie Mellon Speech Group Software
 * depository (ftp://ftp.cs.cmu.edu/project/fgdata/index.html). No copyright
 * was claimed in the original source codes.
 */
#ifndef __PJMEDIA_CODEC_G722_ENC_H__
#define __PJMEDIA_CODEC_G722_ENC_H__

#include <pjmedia-codec/types.h>

/* Encoder state */
typedef struct g722_enc_t {
    /* PCM low band */
    int slow;
    int detlow;
    int spl;
    int szl;
    int rlt  [3];
    int al   [3];
    int apl  [3];
    int plt  [3];
    int dlt  [7];
    int bl   [7];
    int bpl  [7];
    int sgl  [7];
    int nbl;

    /* PCM high band*/
    int shigh;
    int dethigh;
    int sph;
    int szh;
    int rh   [3];
    int ah   [3];
    int aph  [3];
    int ph   [3];
    int dh   [7];
    int bh   [7];
    int bph  [7];
    int sgh  [7];
    int nbh;

    /* QMF signal history */
    int x[24];
} g722_enc_t;


PJ_DECL(pj_status_t) g722_enc_init(g722_enc_t *enc);

PJ_DECL(pj_status_t) g722_enc_encode(g722_enc_t *enc, 
				     pj_int16_t in[], 
				     pj_size_t nsamples,
				     void *out,
				     pj_size_t *out_size);

PJ_DECL(pj_status_t) g722_enc_deinit(g722_enc_t *enc);

#endif /* __PJMEDIA_CODEC_G722_ENC_H__ */

