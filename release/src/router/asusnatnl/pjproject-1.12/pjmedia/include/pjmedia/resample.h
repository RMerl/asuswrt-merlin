/* $Id: resample.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJMEDIA_RESAMPLE_H__
#define __PJMEDIA_RESAMPLE_H__



/**
 * @file resample.h
 * @brief Sample rate converter.
 */
#include <pjmedia/types.h>
#include <pjmedia/port.h>

/**
 * @defgroup PJMEDIA_RESAMPLE Resampling Algorithm
 * @ingroup PJMEDIA_FRAME_OP
 * @brief Sample rate conversion algorithm
 * @{
 *
 * This section describes the base resampling functions. In addition to this,
 * application can use the @ref PJMEDIA_RESAMPLE_PORT which provides
 * media port abstraction for the base resampling algorithm.
 */

PJ_BEGIN_DECL

/*
 * This file declares two types of API:
 *
 * Application can use #pjmedia_resample_create() and #pjmedia_resample_run()
 * to convert a frame from source rate to destination rate. The inpuit frame 
 * must have a constant length.
 *
 * Alternatively, application can create a resampling port with
 * #pjmedia_resample_port_create() and connect the port to other ports to
 * change the sampling rate of the samples.
 */


/**
 * Opaque resample session.
 */
typedef struct pjmedia_resample pjmedia_resample;

/**
 * Create a frame based resample session.
 *
 * @param pool			Pool to allocate the structure and buffers.
 * @param high_quality		If true, then high quality conversion will be
 *				used, at the expense of more CPU and memory,
 *				because temporary buffer needs to be created.
 * @param large_filter		If true, large filter size will be used.
 * @param channel_count		Number of channels.
 * @param rate_in		Clock rate of the input samples.
 * @param rate_out		Clock rate of the output samples.
 * @param samples_per_frame	Number of samples per frame in the input.
 * @param p_resample		Pointer to receive the resample session.
 *
 * @return PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_resample_create(pj_pool_t *pool,
					     pj_bool_t high_quality,
					     pj_bool_t large_filter,
					     unsigned channel_count,
					     unsigned rate_in,
					     unsigned rate_out,
					     unsigned samples_per_frame,
					     pjmedia_resample **p_resample);


/**
 * Use the resample session to resample a frame. The frame must have the
 * same size and settings as the resample session, or otherwise the
 * behavior is undefined.
 *
 * @param resample		The resample session.
 * @param input			Buffer containing the input samples.
 * @param output		Buffer to store the output samples.
 */
PJ_DECL(void) pjmedia_resample_run( pjmedia_resample *resample,
				    const pj_int16_t *input,
				    pj_int16_t *output );


/**
 * Get the input frame size of a resample session.
 *
 * @param resample		The resample session.
 *
 * @return			The frame size, in number of samples.
 */
PJ_DECL(unsigned) pjmedia_resample_get_input_size(pjmedia_resample *resample);


/**
 * Destroy the resample.
 *
 * @param resample		The resample session.
 */
PJ_DECL(void) pjmedia_resample_destroy(pjmedia_resample *resample);

/**
 * @}
 */

/**
 * @defgroup PJMEDIA_RESAMPLE_PORT Resample Port
 * @ingroup PJMEDIA_PORT
 * @brief Audio sample rate conversion
 * @{
 *
 * This section describes media port abstraction for @ref PJMEDIA_RESAMPLE.
 */


/**
 * Option flags that can be specified when creating resample port.
 */
enum pjmedia_resample_port_options
{
    /**
     * Do not use high quality resampling algorithm, but use linear
     * algorithm instead.
     */
    PJMEDIA_RESAMPLE_USE_LINEAR = 1,

    /**
     * Use small filter workspace when high quality resampling is
     * used.
     */
    PJMEDIA_RESAMPLE_USE_SMALL_FILTER = 2,

    /**
     * Do not destroy downstream port when resample port is destroyed.
     */
    PJMEDIA_RESAMPLE_DONT_DESTROY_DN = 4
};



/**
 * Create a resample port. This creates a bidirectional resample session,
 * which will resample frames when the port's get_frame() and put_frame()
 * is called.
 *
 * When the resample port's get_frame() is called, this port will get
 * a frame from the downstream port and resample the frame to the target
 * clock rate before returning it to the caller.
 *
 * When the resample port's put_frame() is called, this port will resample
 * the frame to the downstream port's clock rate before giving the frame
 * to the downstream port.
 *
 * @param pool			Pool to allocate the structure and buffers.
 * @param dn_port		The downstream port, which clock rate is to
 *				be converted to the target clock rate.
 * @param clock_rate		Target clock rate.
 * @param options		Flags from #pjmedia_resample_port_options.
 *				When this flag is zero, the default behavior
 *				is to use high quality resampling with
 *				large filter, and to destroy downstream port
 *				when resample port is destroyed.
 * @param p_port		Pointer to receive the resample port instance.
 *
 * @return PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_resample_port_create( pj_pool_t *pool,
						   pjmedia_port *dn_port,
						   unsigned clock_rate,
						   unsigned options,
						   pjmedia_port **p_port );


PJ_END_DECL

/**
 * @}
 */


#endif	/* __PJMEDIA_RESAMPLE_H__ */

