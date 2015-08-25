/* $Id: g722_enc.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/pool.h>

#include "g722_enc.h"

#if defined(PJMEDIA_HAS_G722_CODEC) && (PJMEDIA_HAS_G722_CODEC != 0)

#define SATURATE(v, max, min) \
    if (v>max) v = max; \
    else if (v<min) v = min

/* QMF tap coefficients */
const int g722_qmf_coeff[24] = {
     3,	    -11,    -11,    53,	    12,	    -156,
    32,	    362,    -210,   -805,   951,    3876,
    3876,   951,    -805,   -210,   362,    32,
    -156,   12,	    53,	    -11,    -11,    3
};


static int block1l (int xl, int sl, int detl)
{
    int il ;

    int i, el, sil, mil, wd, wd1, hdu ;

    static const int q6[32] = {
	0, 35, 72, 110, 150, 190, 233, 276, 323,
	370, 422, 473, 530, 587, 650, 714, 786,
	858, 940, 1023, 1121, 1219, 1339, 1458,
	1612, 1765, 1980, 2195, 2557, 2919, 0, 0
    };

    static const int iln[32] = {
	0, 63, 62, 31, 30, 29, 28, 27, 26, 25,
	24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14,
	13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 0 
    };

    static const int ilp[32] = {
	0, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52,
	51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41,
	40, 39, 38, 37, 36, 35, 34, 33, 32, 0 
    };

    /* SUBTRA */

    el = xl - sl ;
    SATURATE(el, 32767, -32768);

    /* QUANTL */

    sil = el >> 15 ;
    if (sil == 0 )  wd = el ;
    else wd = (32767 - el) & 32767 ;

    mil = 1 ;

    for (i = 1; i < 30; i++) {
	hdu = (q6[i] << 3) * detl;
	wd1 = (hdu >> 15) ;
	if (wd >= wd1)  mil = (i + 1) ;
	else break ;
    }

    if (sil == -1 ) il = iln[mil] ;
    else il = ilp[mil] ;

    return (il) ;
}

static int block2l (int il, int detl)
{
    int dlt;
    int ril, wd2 ;
    static const int qm4[16] = {
	0,	-20456,	-12896,	-8968,
	-6288,	-4240,	-2584,	-1200,
	20456,	12896,	8968,	6288,
	4240,	2584,	1200,	0
    };

    /* INVQAL */
    ril = il >> 2 ;
    wd2 = qm4[ril] ;
    dlt = (detl * wd2) >> 15 ;

    return (dlt) ;
}

static int block3l (g722_enc_t *enc, int il)
{
    int detl;
    int ril, il4, wd, wd1, wd2, wd3, nbpl, depl ;
    static int const wl[8] = {
	-60, -30, 58, 172, 334, 538, 1198, 3042
    } ;
    static int const rl42[16] = {
	0, 7, 6, 5, 4, 3, 2, 1, 7, 6, 5, 4, 3, 2, 1, 0
    };
    static const int ilb[32] = {
	2048, 2093, 2139, 2186, 2233, 2282, 2332,
	2383, 2435, 2489, 2543, 2599, 2656, 2714,
	2774, 2834, 2896, 2960, 3025, 3091, 3158,
	3228, 3298, 3371, 3444, 3520, 3597, 3676,
	3756, 3838, 3922, 4008
    };

    /* LOGSCL */

    ril = il >> 2 ;
    il4 = rl42[ril] ;
    
    wd = (enc->nbl * 32512) >> 15 ;
    nbpl = wd + wl[il4] ;

    if (nbpl <     0) nbpl = 0 ;
    if (nbpl > 18432) nbpl = 18432 ;

    /* SCALEL */

    wd1 =  (nbpl >> 6) & 31 ;
    wd2 = nbpl >> 11 ;
    if ((8 - wd2) < 0)    wd3 = ilb[wd1] << (wd2 - 8) ;
    else   wd3 = ilb[wd1] >> (8 - wd2) ;
    depl = wd3 << 2 ;

    /* DELAYA */
    enc->nbl = nbpl ;

    /* DELAYL */
    detl = depl ;

#ifdef DEBUG_VERBOSE
	printf ("BLOCK3L il=%4d, ril=%4d, il4=%4d, nbl=%4d, wd=%4d, nbpl=%4d\n",
		 il, ril, il4, enc->nbl, wd, nbpl) ;
	printf ("wd1=%4d, wd2=%4d, wd3=%4d, depl=%4d, detl=%4d\n",
		wd1, wd2, wd3, depl, detl) ;
#endif

    return (detl) ;
}

static int block4l (g722_enc_t *enc, int dl)
{
    int sl = enc->slow;
    int i ;
    int wd, wd1, wd2, wd3, wd4, wd5 /*, wd6 */;

    enc->dlt[0] = dl;
    
    /* RECONS */

    enc->rlt[0] = sl + enc->dlt[0] ;
    SATURATE(enc->rlt[0], 32767, -32768);

    /* PARREC */

    enc->plt[0] = enc->dlt[0] + enc->szl ;
    SATURATE(enc->plt[0], 32767, -32768);

    /* UPPOL2 */

    enc->sgl[0] = enc->plt[0] >> 15 ;
    enc->sgl[1] = enc->plt[1] >> 15 ;
    enc->sgl[2] = enc->plt[2] >> 15 ;

    wd1 = enc->al[1] << 2;
    SATURATE(wd1, 32767, -32768);

    if ( enc->sgl[0] == enc->sgl[1] )  wd2 = - wd1 ;
    else  wd2 = wd1 ;
    if ( wd2 > 32767 ) wd2 = 32767;

    wd2 = wd2 >> 7 ;

    if ( enc->sgl[0] == enc->sgl[2] )  wd3 = 128 ;
    else  wd3 = - 128 ;

    wd4 = wd2 + wd3 ;
    wd5 = (enc->al[2] * 32512) >> 15 ;

    enc->apl[2] = wd4 + wd5 ;
    SATURATE(enc->apl[2], 12288, -12288);

    /* UPPOL1 */

    enc->sgl[0] = enc->plt[0] >> 15 ;
    enc->sgl[1] = enc->plt[1] >> 15 ;

    if ( enc->sgl[0] == enc->sgl[1] )  wd1 = 192 ;
    else  wd1 = - 192 ;

    wd2 = (enc->al[1] * 32640) >> 15 ;

    enc->apl[1] = wd1 + wd2 ;
    SATURATE(enc->apl[1], 32767, -32768);

    wd3 = (15360 - enc->apl[2]) ;
    SATURATE(wd3, 32767, -32768);

    if ( enc->apl[1] >  wd3)  enc->apl[1] =  wd3 ;
    if ( enc->apl[1] < -wd3)  enc->apl[1] = -wd3 ;

    /* UPZERO */

    if ( enc->dlt[0] == 0 )  wd1 = 0 ;
    else wd1 = 128 ;

    enc->sgl[0] = enc->dlt[0] >> 15 ;

    for ( i = 1; i < 7; i++ ) {
	enc->sgl[i] = enc->dlt[i] >> 15 ;
	if ( enc->sgl[i] == enc->sgl[0] )  wd2 = wd1 ;
	else wd2 = - wd1 ;
	wd3 = (enc->bl[i] * 32640) >> 15 ;
	enc->bpl[i] = wd2 + wd3 ;
	SATURATE(enc->bpl[i], 32767, -32768);
    }

    /* DELAYA */

    for ( i = 6; i > 0; i-- ) {
	enc->dlt[i] = enc->dlt[i-1] ;
	enc->bl[i]  = enc->bpl[i] ;
    }

    for ( i = 2; i > 0; i-- ) {
	enc->rlt[i] = enc->rlt[i-1] ;
	enc->plt[i] = enc->plt[i-1] ;
	enc->al[i] = enc->apl[i] ;
    }

    /* FILTEP */

    wd1 = enc->rlt[1] + enc->rlt[1];
    SATURATE(wd1, 32767, -32768);
    wd1 = ( enc->al[1] * wd1 ) >> 15 ;

    wd2 = enc->rlt[2] + enc->rlt[2];
    SATURATE(wd2, 32767, -32768);
    wd2 = ( enc->al[2] * wd2 ) >> 15 ;

    enc->spl = wd1 + wd2 ;
    SATURATE(enc->spl, 32767, -32768);

    /* FILTEZ */

    enc->szl = 0 ;
    for (i=6; i>0; i--) {
	wd = enc->dlt[i] + enc->dlt[i];
	SATURATE(wd, 32767, -32768);
	enc->szl += (enc->bl[i] * wd) >> 15 ;
	SATURATE(enc->szl, 32767, -32768);
    }

    /* PREDIC */

    sl = enc->spl + enc->szl ;
    SATURATE(sl, 32767, -32768);

    return (sl) ;
}

static int block1h (int xh, int sh, int deth)
{
    int ih ;

    int eh, sih, mih, wd, wd1, hdu ;

    static const int ihn[3] = { 0, 1, 0 } ;
    static const int ihp[3] = { 0, 3, 2 } ;

    /* SUBTRA */

    eh = xh - sh ;
    SATURATE(eh, 32767, -32768);

    /* QUANTH */

    sih = eh >> 15 ;
    if (sih == 0 )  wd = eh ;
    else wd = (32767 - eh) & 32767 ;

    hdu = (564 << 3) * deth;
    wd1 = (hdu >> 15) ;
    if (wd >= wd1)  mih = 2 ;
    else mih = 1 ;

    if (sih == -1 ) ih = ihn[mih] ;
    else ih = ihp[mih] ;

    return (ih) ;
}

static int block2h (int ih, int deth)
{
    int dh ;
    int wd2 ;
    static const int qm2[4] = {-7408, -1616, 7408, 1616};
    
    /* INVQAH */

    wd2 = qm2[ih] ;
    dh = (deth * wd2) >> 15 ;

    return (dh) ;
}

static int block3h (g722_enc_t *enc, int ih)
{
    int deth ;
    int ih2, wd, wd1, wd2, wd3, nbph, deph ;
    static const int wh[3] = {0, -214, 798} ;
    static const int rh2[4] = {2, 1, 2, 1} ;
    static const int ilb[32] = {
	2048, 2093, 2139, 2186, 2233, 2282, 2332,
	2383, 2435, 2489, 2543, 2599, 2656, 2714,
	2774, 2834, 2896, 2960, 3025, 3091, 3158,
	3228, 3298, 3371, 3444, 3520, 3597, 3676,
	3756, 3838, 3922, 4008
    };

    /* LOGSCH */

    ih2 = rh2[ih] ;
    wd = (enc->nbh * 32512) >> 15 ;
    nbph = wd + wh[ih2] ;

    if (nbph <     0) nbph = 0 ;
    if (nbph > 22528) nbph = 22528 ;

    /* SCALEH */

    wd1 =  (nbph >> 6) & 31 ;
    wd2 = nbph >> 11 ;
    if ((10-wd2) < 0) wd3 = ilb[wd1] << (wd2-10) ;
    else wd3 = ilb[wd1] >> (10-wd2) ;
    deph = wd3 << 2 ;

    /* DELAYA */
    enc->nbh = nbph ;
    /* DELAYH */
    deth = deph ;

    return (deth) ;
}

static int block4h (g722_enc_t *enc, int d)
{
    int sh = enc->shigh;
    int i ;
    int wd, wd1, wd2, wd3, wd4, wd5 /*, wd6 */;

    enc->dh[0] = d;

    /* RECONS */

    enc->rh[0] = sh + enc->dh[0] ;
    SATURATE(enc->rh[0], 32767, -32768);

    /* PARREC */

    enc->ph[0] = enc->dh[0] + enc->szh ;
    SATURATE(enc->ph[0], 32767, -32768);

    /* UPPOL2 */

    enc->sgh[0] = enc->ph[0] >> 15 ;
    enc->sgh[1] = enc->ph[1] >> 15 ;
    enc->sgh[2] = enc->ph[2] >> 15 ;

    wd1 = enc->ah[1] << 2;
    SATURATE(wd1, 32767, -32768);

    if ( enc->sgh[0] == enc->sgh[1] )  wd2 = - wd1 ;
    else  wd2 = wd1 ;
    if ( wd2 > 32767 ) wd2 = 32767;

    wd2 = wd2 >> 7 ;

    if ( enc->sgh[0] == enc->sgh[2] )  wd3 = 128 ;
    else  wd3 = - 128 ;

    wd4 = wd2 + wd3 ;
    wd5 = (enc->ah[2] * 32512) >> 15 ;

    enc->aph[2] = wd4 + wd5 ;
    SATURATE(enc->aph[2], 12288, -12288);

    /* UPPOL1 */

    enc->sgh[0] = enc->ph[0] >> 15 ;
    enc->sgh[1] = enc->ph[1] >> 15 ;

    if ( enc->sgh[0] == enc->sgh[1] )  wd1 = 192 ;
    else wd1 = - 192 ;

    wd2 = (enc->ah[1] * 32640) >> 15 ;

    enc->aph[1] = wd1 + wd2 ;
    SATURATE(enc->aph[1], 32767, -32768);

    wd3 = (15360 - enc->aph[2]) ;
    SATURATE(wd3, 32767, -32768);

    if ( enc->aph[1] >  wd3)  enc->aph[1] =  wd3 ;
    else if ( enc->aph[1] < -wd3)  enc->aph[1] = -wd3 ;

    /* UPZERO */

    if ( enc->dh[0] == 0 )  wd1 = 0 ;
    else wd1 = 128 ;

    enc->sgh[0] = enc->dh[0] >> 15 ;

    for ( i = 1; i < 7; i++ ) {
	enc->sgh[i] = enc->dh[i] >> 15 ;
	if ( enc->sgh[i] == enc->sgh[0] )  wd2 = wd1 ;
	else wd2 = - wd1 ;
	wd3 = (enc->bh[i] * 32640) >> 15 ;
	enc->bph[i] = wd2 + wd3 ;
	SATURATE(enc->bph[i], 32767, -32768);
    }

    /* DELAYA */
    for ( i = 6; i > 0; i-- ) {
	enc->dh[i] = enc->dh[i-1] ;
	enc->bh[i]  = enc->bph[i] ;
    }

    for ( i = 2; i > 0; i-- ) {
	enc->rh[i] = enc->rh[i-1] ;
	enc->ph[i] = enc->ph[i-1] ;
	enc->ah[i] = enc->aph[i] ;
    }

    /* FILTEP */

    wd1 = enc->rh[1] + enc->rh[1];
    SATURATE(wd1, 32767, -32768);
    wd1 = ( enc->ah[1] * wd1 ) >> 15 ;

    wd2 = enc->rh[2] + enc->rh[2];
    SATURATE(wd2, 32767, -32768);
    wd2 = ( enc->ah[2] * wd2 ) >> 15 ;

    enc->sph = wd1 + wd2 ;
    SATURATE(enc->sph, 32767, -32768);

    /* FILTEZ */

    enc->szh = 0 ;
    for (i=6; i>0; i--) {
	wd = enc->dh[i] + enc->dh[i];
	SATURATE(wd, 32767, -32768);
	enc->szh += (enc->bh[i] * wd) >> 15 ;
	SATURATE(enc->szh, 32767, -32768);
    }

    /* PREDIC */

    sh = enc->sph + enc->szh ;
    SATURATE(sh, 32767, -32768);

    return (sh) ;
}

/* PROCESS PCM THROUGH THE QMF FILTER */
static void tx_qmf(g722_enc_t *enc, int pcm1, int pcm2, int *lo, int *hi)
{
    int sumodd, sumeven;
    int i;

    pj_memmove(&enc->x[2], enc->x, 22 * sizeof(enc->x[0]));
    enc->x[1] = pcm1; 
    enc->x[0] = pcm2;

    sumodd = 0;
    for (i=1; i<24; i+=2) sumodd += enc->x[i] * g722_qmf_coeff[i];

    sumeven = 0;
    for (i=0; i<24; i+=2) sumeven += enc->x[i] * g722_qmf_coeff[i];

    *lo  = (sumeven + sumodd) >> 13  ;
    *hi = (sumeven - sumodd) >> 13  ;

    SATURATE(*lo, 16383, -16384);
    SATURATE(*hi, 16383, -16383);
}


PJ_DEF(pj_status_t) g722_enc_init(g722_enc_t *enc)
{
    PJ_ASSERT_RETURN(enc, PJ_EINVAL);
    
    pj_bzero(enc, sizeof(g722_enc_t));

    enc->detlow = 32;
    enc->dethigh = 8;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) g722_enc_encode( g722_enc_t *enc, 
				     pj_int16_t in[], 
				     pj_size_t nsamples,
				     void *out,
				     pj_size_t *out_size)
{
    unsigned i;
    int xlow, ilow, dlowt;
    int xhigh, ihigh, dhigh;
    pj_uint8_t *out_ = (pj_uint8_t*) out;

    PJ_ASSERT_RETURN(enc && in && nsamples && out && out_size, PJ_EINVAL);
    PJ_ASSERT_RETURN(nsamples % 2 == 0, PJ_EINVAL);
    PJ_ASSERT_RETURN(*out_size >= (nsamples >> 1), PJ_ETOOSMALL);
    
    for(i = 0; i < nsamples; i += 2) {
	tx_qmf(enc, in[i], in[i+1], &xlow, &xhigh);

	/* low band encoder */
	ilow = block1l (xlow, enc->slow, enc->detlow) ;
	dlowt = block2l (ilow, enc->detlow) ;
	enc->detlow = block3l (enc, ilow) ;
	enc->slow = block4l (enc, dlowt) ;

	/* high band encoder */
	ihigh = block1h (xhigh, enc->shigh, enc->dethigh) ;
	dhigh = block2h (ihigh, enc->dethigh) ;
	enc->dethigh = block3h (enc, ihigh) ;
	enc->shigh = block4h (enc, dhigh) ;

	/* bits mix low & high adpcm */
	out_[i/2] = (pj_uint8_t)((ihigh << 6) | ilow);
    }

    *out_size = nsamples >> 1;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) g722_enc_deinit(g722_enc_t *enc)
{
    pj_bzero(enc, sizeof(g722_enc_t));

    return PJ_SUCCESS;
}

#endif
