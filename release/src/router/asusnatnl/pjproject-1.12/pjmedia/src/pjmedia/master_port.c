/* $Id: master_port.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia/master_port.h>
#include <pjmedia/clock.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/lock.h>
#include <pj/pool.h>
#include <pj/string.h>


struct pjmedia_master_port
{
    unsigned	     options;
    pjmedia_clock   *clock;
    pjmedia_port    *u_port;
    pjmedia_port    *d_port;
    unsigned	     buff_size;
    void	    *buff;
    pj_lock_t	    *lock;
};


static void clock_callback(const pj_timestamp *ts, void *user_data);


/*
 * Create a master port.
 *
 */
PJ_DEF(pj_status_t) pjmedia_master_port_create( pj_pool_t *pool,
						pjmedia_port *u_port,
						pjmedia_port *d_port,
						unsigned options,
						pjmedia_master_port **p_m)
{
    pjmedia_master_port *m;
    unsigned clock_rate;
    unsigned channel_count;
    unsigned samples_per_frame;
    unsigned bytes_per_frame;
    pj_status_t status;

    /* Sanity check */
    PJ_ASSERT_RETURN(pool && u_port && d_port && p_m, PJ_EINVAL);


    /* Both ports MUST have equal clock rate */
    PJ_ASSERT_RETURN(u_port->info.clock_rate == d_port->info.clock_rate,
		     PJMEDIA_ENCCLOCKRATE);

    /* Both ports MUST have equal samples per frame */
    PJ_ASSERT_RETURN(u_port->info.samples_per_frame==
		     d_port->info.samples_per_frame,
		     PJMEDIA_ENCSAMPLESPFRAME);

    /* Both ports MUST have equal channel count */
    PJ_ASSERT_RETURN(u_port->info.channel_count == d_port->info.channel_count,
		     PJMEDIA_ENCCHANNEL);


    /* Get clock_rate and samples_per_frame from one of the port. */
    clock_rate = u_port->info.clock_rate;
    samples_per_frame = u_port->info.samples_per_frame;
    channel_count = u_port->info.channel_count;


    /* Get the bytes_per_frame value, to determine the size of the
     * buffer. We take the larger size of the two ports.
     */
    bytes_per_frame = u_port->info.bytes_per_frame;
    if (d_port->info.bytes_per_frame > bytes_per_frame)
	bytes_per_frame = d_port->info.bytes_per_frame;


    /* Create the master port instance */
    m = PJ_POOL_ZALLOC_T(pool, pjmedia_master_port);
    m->options = options;
    m->u_port = u_port;
    m->d_port = d_port;

    
    /* Create buffer */
    m->buff_size = bytes_per_frame;
    m->buff = pj_pool_alloc(pool, m->buff_size);
    if (!m->buff)
	return PJ_ENOMEM;

    /* Create lock object */
    status = pj_lock_create_simple_mutex(pool, "mport", &m->lock);
    if (status != PJ_SUCCESS)
	return status;

    /* Create media clock */
    status = pjmedia_clock_create(pool, clock_rate, channel_count, 
				  samples_per_frame, options, &clock_callback,
				  m, &m->clock);
    if (status != PJ_SUCCESS) {
	pj_lock_destroy(m->lock);
	return status;
    }

    /* Done */
    *p_m = m;

    return PJ_SUCCESS;
}


/*
 * Start the media flow.
 */
PJ_DEF(pj_status_t) pjmedia_master_port_start(pjmedia_master_port *m)
{
    PJ_ASSERT_RETURN(m && m->clock, PJ_EINVAL);
    PJ_ASSERT_RETURN(m->u_port && m->d_port, PJ_EINVALIDOP);

    return pjmedia_clock_start(m->clock);
}


/*
 * Stop the media flow.
 */
PJ_DEF(pj_status_t) pjmedia_master_port_stop(pjmedia_master_port *m)
{
    PJ_ASSERT_RETURN(m && m->clock, PJ_EINVAL);
    
    return pjmedia_clock_stop(m->clock);
}


/* Poll the master port clock */
PJ_DEF(pj_bool_t) pjmedia_master_port_wait( pjmedia_master_port *m,
					    pj_bool_t wait,
					    pj_timestamp *ts)
{
    PJ_ASSERT_RETURN(m && m->clock, PJ_FALSE);

    return pjmedia_clock_wait(m->clock, wait, ts);
}

/*
 * Callback to be called for each clock ticks.
 */
static void clock_callback(const pj_timestamp *ts, void *user_data)
{
    pjmedia_master_port *m = (pjmedia_master_port*) user_data;
    pjmedia_frame frame;
    pj_status_t status;

    
    /* Lock access to ports. */
    pj_lock_acquire(m->lock);

    /* Get frame from upstream port and pass it to downstream port */
    pj_bzero(&frame, sizeof(frame));
    frame.buf = m->buff;
    frame.size = m->buff_size;
    frame.timestamp.u64 = ts->u64;

    status = pjmedia_port_get_frame(m->u_port, &frame);
    if (status != PJ_SUCCESS)
	frame.type = PJMEDIA_FRAME_TYPE_NONE;

    status = pjmedia_port_put_frame(m->d_port, &frame);

    /* Get frame from downstream port and pass it to upstream port */
    pj_bzero(&frame, sizeof(frame));
    frame.buf = m->buff;
    frame.size = m->buff_size;
    frame.timestamp.u64 = ts->u64;

    status = pjmedia_port_get_frame(m->d_port, &frame);
    if (status != PJ_SUCCESS)
	frame.type = PJMEDIA_FRAME_TYPE_NONE;

    status = pjmedia_port_put_frame(m->u_port, &frame);

    /* Release lock */
    pj_lock_release(m->lock);
}


/*
 * Change the upstream port.
 */
PJ_DEF(pj_status_t) pjmedia_master_port_set_uport(pjmedia_master_port *m,
						     pjmedia_port *port)
{
    PJ_ASSERT_RETURN(m && port, PJ_EINVAL);

    /* If we have downstream port, make sure they have matching samples per
     * frame.
     */
    if (m->d_port) {
	PJ_ASSERT_RETURN(
	    port->info.clock_rate/port->info.samples_per_frame==
	    m->d_port->info.clock_rate/m->d_port->info.samples_per_frame,
	    PJMEDIA_ENCSAMPLESPFRAME
	);
    }

    pj_lock_acquire(m->lock);

    m->u_port = port;

    pj_lock_release(m->lock);

    return PJ_SUCCESS;
}


/*
 * Get the upstream port.
 */
PJ_DEF(pjmedia_port*) pjmedia_master_port_get_uport(pjmedia_master_port*m)
{
    PJ_ASSERT_RETURN(m, NULL);
    return m->u_port;
}


/*
 * Change the downstream port.
 */
PJ_DEF(pj_status_t) pjmedia_master_port_set_dport(pjmedia_master_port *m,
						  pjmedia_port *port)
{
    PJ_ASSERT_RETURN(m && port, PJ_EINVAL);

    /* If we have upstream port, make sure they have matching samples per
     * frame.
     */
    if (m->u_port) {
	PJ_ASSERT_RETURN(
	    port->info.clock_rate/port->info.samples_per_frame==
	    m->u_port->info.clock_rate/m->u_port->info.samples_per_frame,
	    PJMEDIA_ENCSAMPLESPFRAME
	);
    }

    pj_lock_acquire(m->lock);

    m->d_port = port;

    pj_lock_release(m->lock);

    return PJ_SUCCESS;
}


/*
 * Get the downstream port.
 */
PJ_DEF(pjmedia_port*) pjmedia_master_port_get_dport(pjmedia_master_port*m)
{
    PJ_ASSERT_RETURN(m, NULL);
    return m->d_port;
}


/*
 * Destroy the master port, and optionally destroy the u_port and 
 * d_port ports.
 */
PJ_DEF(pj_status_t) pjmedia_master_port_destroy(pjmedia_master_port *m,
						pj_bool_t destroy_ports)
{
    PJ_ASSERT_RETURN(m, PJ_EINVAL);

    if (m->clock) {
	pjmedia_clock_destroy(m->clock);
	m->clock = NULL;
    }

    if (m->u_port && destroy_ports) {
	pjmedia_port_destroy(m->u_port);
	m->u_port = NULL;
    }

    if (m->d_port && destroy_ports) {
	pjmedia_port_destroy(m->d_port);
	m->d_port = NULL;
    }

    if (m->lock) {
	pj_lock_destroy(m->lock);
	m->lock = NULL;
    }

    return PJ_SUCCESS;
}


