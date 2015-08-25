/* $Id: null_port.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia/null_port.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/string.h>


#define SIGNATURE   PJMEDIA_PORT_SIGNATURE('N', 'U', 'L', 'L')

static pj_status_t null_get_frame(pjmedia_port *this_port, 
				  pjmedia_frame *frame);
static pj_status_t null_put_frame(pjmedia_port *this_port, 
				  const pjmedia_frame *frame);
static pj_status_t null_on_destroy(pjmedia_port *this_port);


PJ_DEF(pj_status_t) pjmedia_null_port_create( pj_pool_t *pool,
					      unsigned sampling_rate,
					      unsigned channel_count,
					      unsigned samples_per_frame,
					      unsigned bits_per_sample,
					      pjmedia_port **p_port )
{
    pjmedia_port *port;
    const pj_str_t name = pj_str("null-port");

    PJ_ASSERT_RETURN(pool && p_port, PJ_EINVAL);

    port = PJ_POOL_ZALLOC_T(pool, pjmedia_port);
    PJ_ASSERT_RETURN(pool != NULL, PJ_ENOMEM);

    pjmedia_port_info_init(&port->info, &name, SIGNATURE, sampling_rate,
			   channel_count, bits_per_sample, samples_per_frame);

    port->get_frame = &null_get_frame;
    port->put_frame = &null_put_frame;
    port->on_destroy = &null_on_destroy;


    *p_port = port;
    
    return PJ_SUCCESS;
}



/*
 * Put frame to file.
 */
static pj_status_t null_put_frame(pjmedia_port *this_port, 
				  const pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(this_port);
    PJ_UNUSED_ARG(frame);
    return PJ_SUCCESS;
}


/*
 * Get frame from file.
 */
static pj_status_t null_get_frame(pjmedia_port *this_port, 
				  pjmedia_frame *frame)
{
    frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame->size = this_port->info.samples_per_frame * 2;
    frame->timestamp.u32.lo += this_port->info.samples_per_frame;
    pjmedia_zero_samples((pj_int16_t*)frame->buf, 
			 this_port->info.samples_per_frame);

    return PJ_SUCCESS;
}


/*
 * Destroy port.
 */
static pj_status_t null_on_destroy(pjmedia_port *this_port)
{
    PJ_UNUSED_ARG(this_port);
    return PJ_SUCCESS;
}
