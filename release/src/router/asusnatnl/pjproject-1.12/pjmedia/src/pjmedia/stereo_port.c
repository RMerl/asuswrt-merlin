/* $Id: stereo_port.c 3553 2011-05-05 06:14:19Z nanang $ */
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

#include <pjmedia/stereo.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/string.h>


#define SIGNATURE		PJMEDIA_PORT_SIGNATURE('S','T','R','O')


struct stereo_port
{
    pjmedia_port	 base;
    pjmedia_port	*dn_port;
    unsigned		 options;
    pj_int16_t		*put_buf;
    pj_int16_t		*get_buf;
};



static pj_status_t stereo_put_frame(pjmedia_port *this_port,
				    const pjmedia_frame *frame);
static pj_status_t stereo_get_frame(pjmedia_port *this_port, 
				    pjmedia_frame *frame);
static pj_status_t stereo_destroy(pjmedia_port *this_port);



PJ_DEF(pj_status_t) pjmedia_stereo_port_create( pj_pool_t *pool,
						pjmedia_port *dn_port,
						unsigned channel_count,
						unsigned options,
						pjmedia_port **p_port )
{
    const pj_str_t name = pj_str("stereo");
    struct stereo_port *sport;
    unsigned samples_per_frame;

    /* Validate arguments. */
    PJ_ASSERT_RETURN(pool && dn_port && channel_count && p_port, PJ_EINVAL);

    /* Only supports 16bit samples per frame */
    PJ_ASSERT_RETURN(dn_port->info.bits_per_sample == 16, PJMEDIA_ENCBITS);

    /* Validate channel counts */
    PJ_ASSERT_RETURN(((dn_port->info.channel_count>1 && channel_count==1) ||
		      (dn_port->info.channel_count==1 && channel_count>1)),
		      PJ_EINVAL);

    /* Create and initialize port. */
    sport = PJ_POOL_ZALLOC_T(pool, struct stereo_port);
    PJ_ASSERT_RETURN(sport != NULL, PJ_ENOMEM);

    samples_per_frame = dn_port->info.samples_per_frame * channel_count /
			dn_port->info.channel_count;

    pjmedia_port_info_init(&sport->base.info, &name, SIGNATURE, 
			   dn_port->info.clock_rate,
			   channel_count, 
			   dn_port->info.bits_per_sample, 
			   samples_per_frame);

    sport->dn_port = dn_port;
    sport->options = options;

    /* We always need buffer for put_frame */
    sport->put_buf = (pj_int16_t*)
		     pj_pool_alloc(pool, dn_port->info.bytes_per_frame);

    /* See if we need buffer for get_frame */
    if (dn_port->info.channel_count > channel_count) {
	sport->get_buf = (pj_int16_t*)
			 pj_pool_alloc(pool, dn_port->info.bytes_per_frame);
    }

    /* Media port interface */
    sport->base.get_frame = &stereo_get_frame;
    sport->base.put_frame = &stereo_put_frame;
    sport->base.on_destroy = &stereo_destroy;


    /* Done */
    *p_port = &sport->base;

    return PJ_SUCCESS;
}

static pj_status_t stereo_put_frame(pjmedia_port *this_port,
				    const pjmedia_frame *frame)
{
    struct stereo_port *sport = (struct stereo_port*) this_port;
    pjmedia_frame tmp_frame;

    /* Return if we don't have downstream port. */
    if (sport->dn_port == NULL) {
	return PJ_SUCCESS;
    }

    if (frame->type == PJMEDIA_FRAME_TYPE_AUDIO) {
	tmp_frame.buf = sport->put_buf;
	if (sport->dn_port->info.channel_count == 1) {
	    pjmedia_convert_channel_nto1((pj_int16_t*)tmp_frame.buf, 
					 (const pj_int16_t*)frame->buf,
					 sport->base.info.channel_count, 
					 sport->base.info.samples_per_frame, 
					 (sport->options & PJMEDIA_STEREO_MIX),
					 0);
	} else {
	    pjmedia_convert_channel_1ton((pj_int16_t*)tmp_frame.buf, 
					 (const pj_int16_t*)frame->buf,
					 sport->dn_port->info.channel_count, 
					 sport->base.info.samples_per_frame,
					 sport->options);
	}
	tmp_frame.size = sport->dn_port->info.bytes_per_frame;
    } else {
	tmp_frame.buf = frame->buf;
	tmp_frame.size = frame->size;
    }

    tmp_frame.type = frame->type;
    tmp_frame.timestamp.u64 = frame->timestamp.u64;

    return pjmedia_port_put_frame( sport->dn_port, &tmp_frame );
}



static pj_status_t stereo_get_frame(pjmedia_port *this_port, 
				    pjmedia_frame *frame)
{
    struct stereo_port *sport = (struct stereo_port*) this_port;
    pjmedia_frame tmp_frame;
    pj_status_t status;

    /* Return silence if we don't have downstream port */
    if (sport->dn_port == NULL) {
	pj_bzero(frame->buf, frame->size);
	return PJ_SUCCESS;
    }

    tmp_frame.buf = sport->get_buf? sport->get_buf : frame->buf;
    tmp_frame.size = sport->dn_port->info.bytes_per_frame;
    tmp_frame.timestamp.u64 = frame->timestamp.u64;
    tmp_frame.type = PJMEDIA_FRAME_TYPE_AUDIO;

    status = pjmedia_port_get_frame( sport->dn_port, &tmp_frame);
    if (status != PJ_SUCCESS)
	return status;

    if (tmp_frame.type != PJMEDIA_FRAME_TYPE_AUDIO) {
	frame->type = tmp_frame.type;
	frame->timestamp = tmp_frame.timestamp;
	frame->size = tmp_frame.size;
	if (tmp_frame.size && tmp_frame.buf == sport->get_buf)
	    pj_memcpy(frame->buf, tmp_frame.buf, tmp_frame.size);
	return PJ_SUCCESS;
    }

    if (sport->base.info.channel_count == 1) {
	pjmedia_convert_channel_nto1((pj_int16_t*)frame->buf, 
				     (const pj_int16_t*)tmp_frame.buf,
				     sport->dn_port->info.channel_count, 
				     sport->dn_port->info.samples_per_frame, 
				     (sport->options & PJMEDIA_STEREO_MIX), 0);
    } else {
	pjmedia_convert_channel_1ton((pj_int16_t*)frame->buf, 
				     (const pj_int16_t*)tmp_frame.buf,
				     sport->base.info.channel_count, 
				     sport->dn_port->info.samples_per_frame,
				     sport->options);
    }

    frame->size = sport->base.info.bytes_per_frame;
    frame->type = PJMEDIA_FRAME_TYPE_AUDIO;

    return PJ_SUCCESS;
}


static pj_status_t stereo_destroy(pjmedia_port *this_port)
{
    struct stereo_port *sport = (struct stereo_port*) this_port;

    if ((sport->options & PJMEDIA_STEREO_DONT_DESTROY_DN)==0) {
	pjmedia_port_destroy(sport->dn_port);
	sport->dn_port = NULL;
    }

    return PJ_SUCCESS;
}

