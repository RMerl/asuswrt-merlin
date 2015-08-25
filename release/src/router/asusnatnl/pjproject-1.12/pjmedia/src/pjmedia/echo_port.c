/* $Id: echo_port.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia/echo_port.h>
#include <pjmedia/echo.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>


#define THIS_FILE   "ec_port.c"
#define SIGNATURE   PJMEDIA_PORT_SIGNATURE('E', 'C', 'H', 'O')
#define BUF_COUNT   32

struct ec
{
    pjmedia_port	 base;
    pjmedia_port	*dn_port;
    pjmedia_echo_state	*ec;
};


static pj_status_t ec_put_frame(pjmedia_port *this_port, 
				const pjmedia_frame *frame);
static pj_status_t ec_get_frame(pjmedia_port *this_port, 
				pjmedia_frame *frame);
static pj_status_t ec_on_destroy(pjmedia_port *this_port);


PJ_DEF(pj_status_t) pjmedia_echo_port_create(pj_pool_t *pool,
					     pjmedia_port *dn_port,
					     unsigned tail_ms,
					     unsigned latency_ms,
					     unsigned options,
					     pjmedia_port **p_port )
{
    const pj_str_t AEC = { "EC", 2 };
    struct ec *ec;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && dn_port && p_port, PJ_EINVAL);
    PJ_ASSERT_RETURN(dn_port->info.bits_per_sample==16 && tail_ms, 
		     PJ_EINVAL);

    /* Create the port and the AEC itself */
    ec = PJ_POOL_ZALLOC_T(pool, struct ec);
    
    pjmedia_port_info_init(&ec->base.info, &AEC, SIGNATURE,
			   dn_port->info.clock_rate, 
			   dn_port->info.channel_count, 
			   dn_port->info.bits_per_sample,
			   dn_port->info.samples_per_frame);

    status = pjmedia_echo_create2(pool, dn_port->info.clock_rate, 
				  dn_port->info.channel_count,
				  dn_port->info.samples_per_frame,
				  tail_ms, latency_ms, options, &ec->ec);
    if (status != PJ_SUCCESS)
	return status;

    /* More init */
    ec->dn_port = dn_port;
    ec->base.get_frame = &ec_get_frame;
    ec->base.put_frame = &ec_put_frame;
    ec->base.on_destroy = &ec_on_destroy;

    /* Done */
    *p_port = &ec->base;

    return PJ_SUCCESS;
}


static pj_status_t ec_put_frame( pjmedia_port *this_port, 
				 const pjmedia_frame *frame)
{
    struct ec *ec = (struct ec*)this_port;

    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE, PJ_EINVAL);

    if (frame->type == PJMEDIA_FRAME_TYPE_NONE ) {
	return pjmedia_port_put_frame(ec->dn_port, frame);
    }

    PJ_ASSERT_RETURN(frame->size == this_port->info.samples_per_frame * 2,
		     PJ_EINVAL);

    pjmedia_echo_capture(ec->ec, (pj_int16_t*)frame->buf, 0);

    return pjmedia_port_put_frame(ec->dn_port, frame);
}


static pj_status_t ec_get_frame( pjmedia_port *this_port, 
				 pjmedia_frame *frame)
{
    struct ec *ec = (struct ec*)this_port;
    pj_status_t status;

    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE, PJ_EINVAL);

    status = pjmedia_port_get_frame(ec->dn_port, frame);
    if (status!=PJ_SUCCESS || frame->type!=PJMEDIA_FRAME_TYPE_AUDIO) {
	pjmedia_zero_samples((pj_int16_t*)frame->buf, 
			     this_port->info.samples_per_frame);
    }

    pjmedia_echo_playback(ec->ec, (pj_int16_t*)frame->buf);

    return status;
}


static pj_status_t ec_on_destroy(pjmedia_port *this_port)
{
    struct ec *ec = (struct ec*)this_port;

    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE, PJ_EINVAL);

    pjmedia_echo_destroy(ec->ec);

    return PJ_SUCCESS;
}


