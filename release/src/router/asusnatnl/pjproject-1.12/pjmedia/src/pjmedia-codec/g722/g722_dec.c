/* $Id: g722_dec.c 3553 2011-05-05 06:14:19Z nanang $ */
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

#include "g722_dec.h"

#if defined(PJMEDIA_HAS_G722_CODEC) && (PJMEDIA_HAS_G722_CODEC != 0)

#define MODE 1

#define SATURATE(v, max, min) \
    if (v>max) v = max; \
    else if (v<min) v = min

extern const int g722_qmf_coeff[24];

static const int qm4[16] = 
{
	0, -20456, -12896, -8968,
    -6288,  -4240,  -2584, -1200,
    20456,  12896,   8968,  6288,
     4240,   2584,   1200,     0
};
static const int ilb[32] = {
    2048, 2093, 2139, 2186, 2233, 2282, 2332,
    2383, 2435, 2489, 2543, 2599, 2656, 2714, 
    2774, 2834, 2896, 2960, 3025, 3091, 3158, 
    3228, 3298, 3371, 3444, 3520, 3597, 3676,
    3756, 3838, 3922, 4008
};


static int block2l (int il, int detl)
{
    int dlt ;
    int ril, wd2 ;

    /* INVQAL */
    ril = il >> 2 ;
    wd2 = qm4[ril] ;
    dlt = (detl * wd2) >> 15 ;

    return (dlt) ;
}


static int block3l (g722_dec_t *dec, int il)
{
    int detl ;
    int ril, il4, wd, wd1, wd2, wd3, nbpl, depl ;
    static const int  wl[8] = {
	-60, -30, 58, 172, 334, 538, 1198, 3042
    };
    static const int rl42[16] = {
	0, 7, 6, 5, 4, 3, 2, 1, 7, 6, 5, 4, 3, 2, 1, 0
    };

    /* LOGSCL */
    ril = il >> 2 ;
    il4 = rl42[ril] ;
    wd = (dec->nbl * 32512) >> 15 ;
    nbpl = wd + wl[il4] ;

    if (nbpl <     0) nbpl = 0 ;
    if (nbpl > 18432) nbpl = 18432 ;

    /* SCALEL */
    wd1 =  (nbpl >> 6) & 31 ;
    wd2 = nbpl >> 11 ;
    if ((8 - wd2) < 0)   wd3 = ilb[wd1] << (wd2 - 8) ;
    else wd3 = ilb[wd1] >> (8 - wd2) ;
    depl = wd3 << 2 ;

    /* DELAYA */
    dec->nbl = nbpl ;
    /* DELAYL */
    detl = depl ;

    return (detl) ;
}


static int block4l (g722_dec_t *dec, int dl)
{
    int sl = dec->slow ;
    int i ;
    int wd, wd1, wd2, wd3, wd4, wd5/*, wd6 */;

    dec->dlt[0] = dl;

    /* RECONS */ 
    dec->rlt[0] = sl + dec->dlt[0] ;
    SATURATE(dec->rlt[0], 32767, -32768);

    /* PARREC */
    dec->plt[0] = dec->dlt[0] + dec->szl ;
    SATURATE(dec->plt[0], 32767, -32768);

    /* UPPOL2 */
    dec->sgl[0] = dec->plt[0] >> 15 ;
    dec->sgl[1] = dec->plt[1] >> 15 ;
    dec->sgl[2] = dec->plt[2] >> 15 ;

    wd1 = dec->al[1] << 2;
    SATURATE(wd1, 32767, -32768);

    if ( dec->sgl[0] == dec->sgl[1] )  wd2 = - wd1 ;
    else  wd2 = wd1 ;
    if (wd2 > 32767) wd2 = 32767;
    wd2 = wd2 >> 7 ;

    if ( dec->sgl[0] == dec->sgl[2] )  wd3 = 128 ; 
    else  wd3 = - 128 ;

    wd4 = wd2 + wd3 ;
    wd5 = (dec->al[2] * 32512) >> 15 ;

    dec->apl[2] = wd4 + wd5 ;
    SATURATE(dec->apl[2], 12288, -12288);
    
    /* UPPOL1 */
    dec->sgl[0] = dec->plt[0] >> 15 ;
    dec->sgl[1] = dec->plt[1] >> 15 ;

    if ( dec->sgl[0] == dec->sgl[1] )  wd1 = 192 ;
    else  wd1 = - 192 ;

    wd2 = (dec->al[1] * 32640) >> 15 ;

    dec->apl[1] = wd1 + wd2 ;
    SATURATE(dec->apl[1], 32767, -32768);

    wd3 = (15360 - dec->apl[2]) ;
    SATURATE(wd3, 32767, -32768);
    if ( dec->apl[1] >  wd3)  dec->apl[1] =  wd3 ;
    if ( dec->apl[1] < -wd3)  dec->apl[1] = -wd3 ;

    /* UPZERO */
    if ( dec->dlt[0] == 0 )  wd1 = 0 ;
    else  wd1 = 128 ;

    dec->sgl[0] = dec->dlt[0] >> 15 ;

    for ( i = 1; i < 7; i++ ) {
	dec->sgl[i] = dec->dlt[i] >> 15 ;
	if ( dec->sgl[i] == dec->sgl[0] )  wd2 = wd1 ;
	else  wd2 = - wd1 ;
	wd3 = (dec->bl[i] * 32640) >> 15 ;
	dec->bpl[i] = wd2 + wd3 ;
	SATURATE(dec->bpl[i], 32767, -32768);
    }

    /* DELAYA */
    for ( i = 6; i > 0; i-- ) {
	dec->dlt[i] = dec->dlt[i-1] ;
	dec->bl[i]  = dec->bpl[i] ;
    }

    for ( i = 2; i > 0; i-- ) {
	dec->rlt[i] = dec->rlt[i-1] ;
	dec->plt[i] = dec->plt[i-1] ;
	dec->al[i]  = dec->apl[i] ;
    }

    /* FILTEP */
    wd1 = dec->rlt[1] << 1;
    SATURATE(wd1, 32767, -32768);
    wd1 = ( dec->al[1] * wd1 ) >> 15 ;

    wd2 = dec->rlt[2] << 1;
    SATURATE(wd2, 32767, -32768);
    wd2 = ( dec->al[2] * wd2 ) >> 15 ;

    dec->spl = wd1 + wd2 ;
    SATURATE(dec->spl, 32767, -32768);

    /* FILTEZ */
    dec->szl = 0 ;
    for (i=6; i>0; i--) {
	wd = dec->dlt[i] << 1;
	SATURATE(wd, 32767, -32768);
	dec->szl += (dec->bl[i] * wd) >> 15 ;
	SATURATE(dec->szl, 32767, -32768);
    }

    /* PREDIC */
    sl = dec->spl + dec->szl ;
    SATURATE(sl, 32767, -32768);

    return (sl) ;
}

static int block5l (int ilr, int sl, int detl, int mode)
{
    int yl ;
    int ril, dl, wd2 = 0;
    static const int qm5[32] = {
	  -280,   -280, -23352, -17560,
	-14120, -11664,  -9752,  -8184,
	 -6864,  -5712,  -4696,  -3784,
	 -2960,  -2208,  -1520,   -880,
	 23352,  17560,  14120,  11664,
	  9752,   8184,   6864,   5712,
	  4696,   3784,   2960,   2208,
	  1520,    880,    280,   -280
    };
    static const int qm6[64] = {
	-136,	-136,	-136,	-136,
	-24808,	-21904,	-19008,	-16704,
	-14984,	-13512,	-12280,	-11192,
	-10232,	-9360,	-8576,	-7856,
	-7192,	-6576,	-6000,	-5456,
	-4944,	-4464,	-4008,	-3576,
	-3168,	-2776,	-2400,	-2032,
	-1688,	-1360,	-1040,	-728,
	24808,	21904,	19008,	16704,
	14984,	13512,	12280,	11192,
	10232,	9360,	8576,	7856,
	7192,	6576,	6000,	5456,
	4944,	4464,	4008,	3576,
	3168,	2776,	2400,	2032,
	1688,	1360,	1040,	728,
	432,	136,	-432,	-136
    };
    
    /* INVQBL */
    if (mode == 1) {
	ril = ilr ;
	wd2 = qm6[ril] ;
    }

    if (mode == 2) {
	ril = ilr >> 1 ;
	wd2 = qm5[ril] ;
    }

    if (mode == 3) {
	ril = ilr >> 2 ;
	wd2 = qm4[ril] ;
    }

    dl = (detl * wd2 ) >> 15 ;

    /* RECONS */
    yl = sl + dl ;
    SATURATE(yl, 32767, -32768);

    return (yl) ;
}

static int block6l (int yl)
{
    int rl ;

    rl = yl ;
    SATURATE(rl, 16383, -16384);

    return (rl) ;
}

static int block2h (int ih, int deth)
{
    int dh ;
    int wd2 ;
    static const int qm2[4] = {-7408, -1616, 7408, 1616} ;
    
    /* INVQAH */
    wd2 = qm2[ih] ;
    dh = (deth * wd2) >> 15 ;

    return (dh) ;
}

static int block3h (g722_dec_t *dec, int ih)
{
    int deth ;
    int ih2, wd, wd1, wd2, wd3, nbph, deph ;
    static const int wh[3] = {0, -214, 798} ;
    static const int rh2[4] = {2, 1, 2, 1} ;
    
    /* LOGSCH */
    ih2 = rh2[ih] ;
    wd = (dec->nbh * 32512) >> 15 ;
    nbph = wd + wh[ih2] ;

    if (nbph <     0) nbph = 0 ;
    if (nbph > 22528) nbph = 22528 ;


    /* SCALEH */
    wd1 =  (nbph >> 6) & 31 ;
    wd2 = nbph >> 11 ;
    if ((10 - wd2) < 0) wd3 = ilb[wd1] << (wd2 - 10) ;
    else wd3 = ilb[wd1] >> (10 - wd2) ;
    deph = wd3 << 2 ;

    /* DELAYA */
    dec->nbh = nbph ;
    
    /* DELAYH */
    deth = deph ;

    return (deth) ;
}

static int block4h (g722_dec_t *dec, int d)
{
    int sh = dec->shigh;
    int i ;
    int wd, wd1, wd2, wd3, wd4, wd5/*, wd6 */;

    dec->dh[0] = d;

    /* RECONS */ 
    dec->rh[0] = sh + dec->dh[0] ;
    SATURATE(dec->rh[0], 32767, -32768);

    /* PARREC */
    dec->ph[0] = dec->dh[0] + dec->szh ;
    SATURATE(dec->ph[0], 32767, -32768);

    /* UPPOL2 */
    dec->sgh[0] = dec->ph[0] >> 15 ;
    dec->sgh[1] = dec->ph[1] >> 15 ;
    dec->sgh[2] = dec->ph[2] >> 15 ;

    wd1 = dec->ah[1] << 2;
    SATURATE(wd1, 32767, -32768);

    if ( dec->sgh[0] == dec->sgh[1] )  wd2 = - wd1 ;
    else  wd2 = wd1 ;
    if (wd2 > 32767) wd2 = 32767;

    wd2 = wd2 >> 7 ;

    if ( dec->sgh[0] == dec->sgh[2] )  wd3 = 128 ; 
    else  wd3 = - 128 ;

    wd4 = wd2 + wd3 ;
    wd5 = (dec->ah[2] * 32512) >> 15 ;

    dec->aph[2] = wd4 + wd5 ;
    SATURATE(dec->aph[2], 12288, -12288);
    
    /* UPPOL1 */
    dec->sgh[0] = dec->ph[0] >> 15 ;
    dec->sgh[1] = dec->ph[1] >> 15 ;

    if ( dec->sgh[0] == dec->sgh[1] )  wd1 = 192 ;
    else  wd1 = - 192 ;

    wd2 = (dec->ah[1] * 32640) >> 15 ;

    dec->aph[1] = wd1 + wd2 ;
    SATURATE(dec->aph[1], 32767, -32768);
    //dec->aph[2]?
    //if (aph[2] > 32767) aph[2] = 32767;
    //if (aph[2] < -32768) aph[2] = -32768;

    wd3 = (15360 - dec->aph[2]) ;
    SATURATE(wd3, 32767, -32768);
    if ( dec->aph[1] >  wd3)  dec->aph[1] =  wd3 ;
    if ( dec->aph[1] < -wd3)  dec->aph[1] = -wd3 ;

    /* UPZERO */
    if ( dec->dh[0] == 0 )  wd1 = 0 ;
    if ( dec->dh[0] != 0 )  wd1 = 128 ;

    dec->sgh[0] = dec->dh[0] >> 15 ;

    for ( i = 1; i < 7; i++ ) {
	dec->sgh[i] = dec->dh[i] >> 15 ;
	if ( dec->sgh[i] == dec->sgh[0] )  wd2 = wd1 ;
	else wd2 = - wd1 ;
	wd3 = (dec->bh[i] * 32640) >> 15 ;
	dec->bph[i] = wd2 + wd3 ;
    }
 
    /* DELAYA */
    for ( i = 6; i > 0; i-- ) {
	dec->dh[i] = dec->dh[i-1] ;
	dec->bh[i] = dec->bph[i] ;
    }

    for ( i = 2; i > 0; i-- ) {
	dec->rh[i] = dec->rh[i-1] ;
	dec->ph[i] = dec->ph[i-1] ;
	dec->ah[i] = dec->aph[i] ;
    }

    /* FILTEP */
    wd1 = dec->rh[1] << 1 ;
    SATURATE(wd1, 32767, -32768);
    wd1 = ( dec->ah[1] * wd1 ) >> 15 ;

    wd2 = dec->rh[2] << 1;
    SATURATE(wd2, 32767, -32768);
    wd2 = ( dec->ah[2] * wd2 ) >> 15 ;

    dec->sph = wd1 + wd2 ;
    SATURATE(dec->sph, 32767, -32768);

    /* FILTEZ */
    dec->szh = 0 ;
    for (i=6; i>0; i--) {
	wd = dec->dh[i] << 1;
	SATURATE(wd, 32767, -32768);
	dec->szh += (dec->bh[i] * wd) >> 15 ;
	SATURATE(dec->szh, 32767, -32768);
    }

    /* PREDIC */
    sh = dec->sph + dec->szh ;
    SATURATE(sh, 32767, -32768);

    return (sh) ;
}

static int block5h (int dh, int sh)
{
    int rh ;

    rh = dh + sh;
    SATURATE(rh, 16383, -16384);

    return (rh) ;
}

void rx_qmf(g722_dec_t *dec, int rl, int rh, int *xout1, int *xout2)
{
    int i;

    pj_memmove(&dec->xd[1], dec->xd, 11*sizeof(dec->xd[0]));
    pj_memmove(&dec->xs[1], dec->xs, 11*sizeof(dec->xs[0]));

    /* RECA */
    dec->xd[0] = rl - rh ;
    if (dec->xd[0] > 16383) dec->xd[0] = 16383;
    else if (dec->xd[0] < -16384) dec->xd[0] = -16384;
    
    /* RECB */
    dec->xs[0] = rl + rh ;
    if (dec->xs[0] > 16383) dec->xs[0] = 16383;
    else if (dec->xs[0] < -16384) dec->xs[0] = -16384;
    
    /* ACCUMC */
    *xout1 = 0;
    for (i=0; i<12; ++i) *xout1 += dec->xd[i] * g722_qmf_coeff[2*i];
    *xout1 = *xout1 >> 12 ;
    if (*xout1 >  16383)  *xout1 =  16383 ;
    else if (*xout1 < -16384)  *xout1 = -16384 ;
    
    /* ACCUMD */
    *xout2 = 0;
    for (i=0; i<12; ++i) *xout2 += dec->xs[i] * g722_qmf_coeff[2*i+1];
    *xout2  = *xout2  >> 12 ;
    if (*xout2  >  16383)  *xout2  =  16383 ;
    else if (*xout2  < -16384)  *xout2  = -16384 ;
}


PJ_DEF(pj_status_t) g722_dec_init(g722_dec_t *dec)
{
    PJ_ASSERT_RETURN(dec, PJ_EINVAL);

    pj_bzero(dec, sizeof(g722_dec_t));

    dec->detlow = 32;
    dec->dethigh = 8;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) g722_dec_decode( g722_dec_t *dec, 
				    void *in, 
				    pj_size_t in_size,
				    pj_int16_t out[],
				    pj_size_t *nsamples)
{
    unsigned i;
    int ilowr, ylow, rlow, dlowt;
    int ihigh, rhigh, dhigh;
    int pcm1, pcm2;
    pj_uint8_t *in_ = (pj_uint8_t*) in;

    PJ_ASSERT_RETURN(dec && in && in_size && out && nsamples, PJ_EINVAL);
    PJ_ASSERT_RETURN(*nsamples >= (in_size << 1), PJ_ETOOSMALL);

    for(i = 0; i < in_size; ++i) {
	ilowr = in_[i] & 63;
	ihigh = (in_[i] >> 6) & 3;

	/* low band decoder */
	ylow = block5l (ilowr, dec->slow, dec->detlow, MODE) ;	
	rlow = block6l (ylow) ;
	dlowt = block2l (ilowr, dec->detlow) ;
	dec->detlow = block3l (dec, ilowr) ;
	dec->slow = block4l (dec, dlowt) ;
	/* rlow <= output low band pcm */

	/* high band decoder */
	dhigh = block2h (ihigh, dec->dethigh) ;
	rhigh = block5h (dhigh, dec->shigh) ;
	dec->dethigh = block3h (dec, ihigh) ;
	dec->shigh = block4h (dec, dhigh) ;
	/* rhigh <= output high band pcm */

	rx_qmf(dec, rlow, rhigh, &pcm1, &pcm2);
	out[i*2]   = (pj_int16_t)pcm1;
	out[i*2+1] = (pj_int16_t)pcm2;
    }

    *nsamples = in_size << 1;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) g722_dec_deinit(g722_dec_t *dec)
{
    pj_bzero(dec, sizeof(g722_dec_t));

    return PJ_SUCCESS;
}

#endif
