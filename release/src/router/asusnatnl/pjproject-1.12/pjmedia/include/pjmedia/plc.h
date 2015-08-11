/* $Id: plc.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_PLC_H__
#define __PJMEDIA_PLC_H__


/**
 * @file plc.h
 * @brief Packet Lost Concealment (PLC) API.
 */
#include <pjmedia/types.h>

/**
 * @defgroup PJMED_PLC Packet Lost Concealment (PLC)
 * @ingroup PJMEDIA_FRAME_OP
 * @brief Packet lost compensation algorithm
 * @{
 *
 * This section describes PJMEDIA's implementation of Packet Lost
 * Concealment algorithm. This algorithm is used to implement PLC for
 * codecs that do not have built-in support for one (e.g. G.711 or GSM 
 * codecs).
 *
 * The PLC algorithm (either built-in or external) is embedded in
 * PJMEDIA codec instance, and application can conceal lost frames
 * by calling <b><tt>recover()</tt></b> member of the codec's member
 * operation (#pjmedia_codec_op).
 *
 * See also @ref plc_codec for more info.
 */


PJ_BEGIN_DECL


/**
 * Opaque declaration for PLC.
 */
typedef struct pjmedia_plc pjmedia_plc;



/**
 * Create PLC session. This function will select the PLC algorithm to
 * use based on the arguments.
 *
 * @param pool		    Pool to allocate memory for the PLC.
 * @param clock_rate	    Media sampling rate.
 * @param samples_per_frame Number of samples per frame.
 * @param options	    Must be zero for now.
 * @param p_plc		    Pointer to receive the PLC instance.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_plc_create( pj_pool_t *pool,
					 unsigned clock_rate,
					 unsigned samples_per_frame,
					 unsigned options,
					 pjmedia_plc **p_plc);


/**
 * Save a good frame to PLC.
 *
 * @param plc		    The PLC session.
 * @param frame		    The good frame to be stored to PLC. This frame
 *			    must have the same length as the configured
 *			    samples per frame.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_plc_save( pjmedia_plc *plc,
				       pj_int16_t *frame );


/**
 * Generate a replacement for lost frame.
 *
 * @param plc		    The PLC session.
 * @param frame		    Buffer to receive the generated frame. This buffer
 *			    must be able to store the frame.
 *
 * @return		    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_plc_generate( pjmedia_plc *plc,
					   pj_int16_t *frame );




PJ_END_DECL

/**
 * @}
 */

#endif	/* __PJMEDIA_PLC_H__ */

