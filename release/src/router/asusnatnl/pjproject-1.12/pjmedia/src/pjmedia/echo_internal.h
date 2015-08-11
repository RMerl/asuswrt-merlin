/* $Id: echo_internal.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_ECHO_INTERNAL_H__
#define __PJMEDIA_ECHO_INTERNAL_H__

#include <pjmedia/types.h>

PJ_BEGIN_DECL

/*
 * Simple echo suppressor
 */
PJ_DECL(pj_status_t) echo_supp_create(pj_pool_t *pool,
				      unsigned clock_rate,
				      unsigned channel_count,
				      unsigned samples_per_frame,
				      unsigned tail_ms,
				      unsigned options,
				      void **p_state );
PJ_DECL(pj_status_t) echo_supp_destroy(void *state);
PJ_DECL(void) echo_supp_reset(void *state);
PJ_DECL(pj_status_t) echo_supp_cancel_echo(void *state,
					   pj_int16_t *rec_frm,
					   const pj_int16_t *play_frm,
					   unsigned options,
					   void *reserved );

PJ_DECL(pj_status_t) speex_aec_create(pj_pool_t *pool,
				      unsigned clock_rate,
				      unsigned channel_count,
				      unsigned samples_per_frame,
				      unsigned tail_ms,
				      unsigned options,
				      void **p_state );
PJ_DECL(pj_status_t) speex_aec_destroy(void *state );
PJ_DECL(void) speex_aec_reset(void *state );
PJ_DECL(pj_status_t) speex_aec_cancel_echo(void *state,
					   pj_int16_t *rec_frm,
					   const pj_int16_t *play_frm,
					   unsigned options,
					   void *reserved );

PJ_DECL(pj_status_t) ipp_aec_create(pj_pool_t *pool,
				    unsigned clock_rate,
				    unsigned channel_count,
				    unsigned samples_per_frame,
				    unsigned tail_ms,
				    unsigned options,
				    void **p_echo );
PJ_DECL(pj_status_t) ipp_aec_destroy(void *state );
PJ_DECL(void) ipp_aec_reset(void *state );
PJ_DECL(pj_status_t) ipp_aec_cancel_echo(void *state,
					 pj_int16_t *rec_frm,
					 const pj_int16_t *play_frm,
					 unsigned options,
					 void *reserved );


PJ_END_DECL

#endif

