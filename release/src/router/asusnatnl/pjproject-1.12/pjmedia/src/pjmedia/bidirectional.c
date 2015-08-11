/* $Id: bidirectional.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia/bidirectional.h>
#include <pj/pool.h>


#define THIS_FILE   "bidirectional.c"
#define SIGNATURE   PJMEDIA_PORT_SIGNATURE('B', 'D', 'I', 'R')

struct bidir_port
{
    pjmedia_port     base;
    pjmedia_port    *get_port;
    pjmedia_port    *put_port;
};


static pj_status_t put_frame(pjmedia_port *this_port, 
			     const pjmedia_frame *frame)
{
    struct bidir_port *p = (struct bidir_port*)this_port;
    return pjmedia_port_put_frame(p->put_port, frame);
}


static pj_status_t get_frame(pjmedia_port *this_port, 
			     pjmedia_frame *frame)
{
    struct bidir_port *p = (struct bidir_port*)this_port;
    return pjmedia_port_get_frame(p->get_port, frame);
}


PJ_DEF(pj_status_t) pjmedia_bidirectional_port_create( pj_pool_t *pool,
						       pjmedia_port *get_port,
						       pjmedia_port *put_port,
						       pjmedia_port **p_port )
{
    struct bidir_port *port;

    port = PJ_POOL_ZALLOC_T(pool, struct bidir_port);

    pjmedia_port_info_init(&port->base.info, &get_port->info.name, SIGNATURE,
			   get_port->info.clock_rate,
			   get_port->info.channel_count,
			   get_port->info.bits_per_sample,
			   get_port->info.samples_per_frame);

    port->get_port = get_port;
    port->put_port = put_port;

    port->base.get_frame = &get_frame;
    port->base.put_frame = &put_frame;

    *p_port = &port->base;

    return PJ_SUCCESS;
}

