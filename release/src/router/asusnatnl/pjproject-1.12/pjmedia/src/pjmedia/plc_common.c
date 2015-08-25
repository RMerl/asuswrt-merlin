/* $Id: plc_common.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia/plc.h>
#include <pjmedia/errno.h>
#include <pjmedia/wsola.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/string.h>


static void* plc_wsola_create(pj_pool_t*, unsigned c, unsigned f);
static void  plc_wsola_save(void*, pj_int16_t*);
static void  plc_wsola_generate(void*, pj_int16_t*);

/**
 * This struct is used internally to represent a PLC backend.
 */
struct plc_alg
{
    void* (*plc_create)(pj_pool_t*, unsigned c, unsigned f);
    void  (*plc_save)(void*, pj_int16_t*);
    void  (*plc_generate)(void*, pj_int16_t*);
};


static struct plc_alg plc_wsola =
{
    &plc_wsola_create,
    &plc_wsola_save,
    &plc_wsola_generate
};


struct pjmedia_plc
{
    void	    *obj;
    struct plc_alg  *op;
};


/*
 * Create PLC session. This function will select the PLC algorithm to
 * use based on the arguments.
 */
PJ_DEF(pj_status_t) pjmedia_plc_create( pj_pool_t *pool,
					unsigned clock_rate,
					unsigned samples_per_frame,
					unsigned options,
					pjmedia_plc **p_plc)
{
    pjmedia_plc *plc;

    PJ_ASSERT_RETURN(pool && clock_rate && samples_per_frame && p_plc,
		     PJ_EINVAL);
    PJ_ASSERT_RETURN(options == 0, PJ_EINVAL);

    PJ_UNUSED_ARG(options);

    plc = PJ_POOL_ZALLOC_T(pool, pjmedia_plc);

    plc->op = &plc_wsola;
    plc->obj = plc->op->plc_create(pool, clock_rate, samples_per_frame);

    *p_plc = plc;

    return PJ_SUCCESS;
}


/*
 * Save a good frame to PLC.
 */
PJ_DEF(pj_status_t) pjmedia_plc_save( pjmedia_plc *plc,
				      pj_int16_t *frame )
{
    PJ_ASSERT_RETURN(plc && frame, PJ_EINVAL);
 
    plc->op->plc_save(plc->obj, frame);
    return PJ_SUCCESS;
}


/*
 * Generate a replacement for lost frame.
 */
PJ_DEF(pj_status_t) pjmedia_plc_generate( pjmedia_plc *plc,
					  pj_int16_t *frame )
{
    PJ_ASSERT_RETURN(plc && frame, PJ_EINVAL);
    
    plc->op->plc_generate(plc->obj, frame);
    return PJ_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////////
/*
 * Packet loss concealment based on WSOLA
 */
struct wsola_plc
{
    pjmedia_wsola   *wsola;
    pj_bool_t	     prev_lost;
};


static void* plc_wsola_create(pj_pool_t *pool, unsigned clock_rate, 
			      unsigned samples_per_frame)
{
    struct wsola_plc *o;
    unsigned flag;
    pj_status_t status;

    PJ_UNUSED_ARG(clock_rate);

    o = PJ_POOL_ZALLOC_T(pool, struct wsola_plc);
    o->prev_lost = PJ_FALSE;

    flag = PJMEDIA_WSOLA_NO_DISCARD;
    if (PJMEDIA_WSOLA_PLC_NO_FADING)
	flag |= PJMEDIA_WSOLA_NO_FADING;

    status = pjmedia_wsola_create(pool, clock_rate, samples_per_frame, 1,
				  flag, &o->wsola);
    if (status != PJ_SUCCESS)
	return NULL;

    return o;
}

static void plc_wsola_save(void *plc, pj_int16_t *frame)
{
    struct wsola_plc *o = (struct wsola_plc*) plc;

    pjmedia_wsola_save(o->wsola, frame, o->prev_lost);
    o->prev_lost = PJ_FALSE;
}

static void plc_wsola_generate(void *plc, pj_int16_t *frame)
{
    struct wsola_plc *o = (struct wsola_plc*) plc;
    
    pjmedia_wsola_generate(o->wsola, frame);
    o->prev_lost = PJ_TRUE;
}


