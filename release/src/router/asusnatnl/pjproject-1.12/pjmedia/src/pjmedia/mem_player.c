/* $Id: mem_player.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia/mem_port.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/pool.h>


#define THIS_FILE	    "mem_player.c"

#define SIGNATURE	    PJMEDIA_PORT_SIGNATURE('M', 'P', 'l', 'y')
#define BYTES_PER_SAMPLE    2

struct mem_player
{
    pjmedia_port     base;

    unsigned	     options;
    pj_timestamp     timestamp;

    char	    *buffer;
    pj_size_t	     buf_size;
    char	    *read_pos;

    pj_bool_t	     eof;
    void	    *user_data;
    pj_status_t    (*cb)(pjmedia_port *port,
			 void *user_data);

};


static pj_status_t mem_put_frame(pjmedia_port *this_port, 
				  const pjmedia_frame *frame);
static pj_status_t mem_get_frame(pjmedia_port *this_port, 
				  pjmedia_frame *frame);
static pj_status_t mem_on_destroy(pjmedia_port *this_port);


PJ_DEF(pj_status_t) pjmedia_mem_player_create( pj_pool_t *pool,
					       const void *buffer,
					       pj_size_t size,
					       unsigned clock_rate,
					       unsigned channel_count,
					       unsigned samples_per_frame,
					       unsigned bits_per_sample,
					       unsigned options,
					       pjmedia_port **p_port )
{
    struct mem_player *port;
    pj_str_t name = pj_str("memplayer");

    /* Sanity check */
    PJ_ASSERT_RETURN(pool && buffer && size && clock_rate && channel_count &&
		     samples_per_frame && bits_per_sample && p_port,
		     PJ_EINVAL);

    /* Can only support 16bit PCM */
    PJ_ASSERT_RETURN(bits_per_sample == 16, PJ_EINVAL);


    port = PJ_POOL_ZALLOC_T(pool, struct mem_player);
    PJ_ASSERT_RETURN(port != NULL, PJ_ENOMEM);

    /* Create the port */
    pjmedia_port_info_init(&port->base.info, &name, SIGNATURE, clock_rate,
			   channel_count, bits_per_sample, samples_per_frame);

    port->base.put_frame = &mem_put_frame;
    port->base.get_frame = &mem_get_frame;
    port->base.on_destroy = &mem_on_destroy;


    /* Save the buffer */
    port->buffer = port->read_pos = (char*)buffer;
    port->buf_size = size;

    /* Options */
    port->options = options;

    *p_port = &port->base;

    return PJ_SUCCESS;
}



/*
 * Register a callback to be called when the file reading has reached the
 * end of buffer.
 */
PJ_DEF(pj_status_t) pjmedia_mem_player_set_eof_cb( pjmedia_port *port,
			       void *user_data,
			       pj_status_t (*cb)(pjmedia_port *port,
						 void *usr_data))
{
    struct mem_player *player;

    PJ_ASSERT_RETURN(port->info.signature == SIGNATURE,
		     PJ_EINVALIDOP);

    player = (struct mem_player*) port;
    player->user_data = user_data;
    player->cb = cb;

    return PJ_SUCCESS;
}


static pj_status_t mem_put_frame( pjmedia_port *this_port, 
				  const pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(this_port);
    PJ_UNUSED_ARG(frame);

    return PJ_SUCCESS;
}


static pj_status_t mem_get_frame( pjmedia_port *this_port, 
				  pjmedia_frame *frame)
{
    struct mem_player *player;
    char *endpos;
    pj_size_t size_needed, size_written;

    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE,
		     PJ_EINVALIDOP);

    player = (struct mem_player*) this_port;

    if (player->eof) {
	pj_status_t status = PJ_SUCCESS;

	/* Call callback, if any */
	if (player->cb)
	    status = (*player->cb)(this_port, player->user_data);

	/* If callback returns non PJ_SUCCESS or 'no loop' is specified
	 * return immediately (and don't try to access player port since
	 * it might have been destroyed by the callback).
	 */
	if ((status != PJ_SUCCESS) || (player->options & PJMEDIA_MEM_NO_LOOP)) {
	    frame->type = PJMEDIA_FRAME_TYPE_NONE;
	    return PJ_EEOF;
	}
	
	player->eof = PJ_FALSE;
    }

    size_needed = this_port->info.bytes_per_frame;
    size_written = 0;
    endpos = player->buffer + player->buf_size;

    while (size_written < size_needed) {
	char *dst = ((char*)frame->buf) + size_written;
	pj_size_t max;
	
	max = size_needed - size_written;
	if (endpos - player->read_pos < (int)max)
	    max = endpos - player->read_pos;

	pj_memcpy(dst, player->read_pos, max);
	size_written += max;
	player->read_pos += max;

	pj_assert(player->read_pos <= endpos);

	if (player->read_pos == endpos) {
	    /* Set EOF flag */
	    player->eof = PJ_TRUE;
	    /* Reset read pointer */
	    player->read_pos = player->buffer;

	    /* Pad with zeroes then return for no looped play */
	    if (player->options & PJMEDIA_MEM_NO_LOOP) {
		pj_size_t null_len;

    		null_len = size_needed - size_written;
		pj_bzero(dst + max, null_len);
		break;
	    }
	}
    }

    frame->size = this_port->info.bytes_per_frame;
    frame->timestamp.u64 = player->timestamp.u64;
    frame->type = PJMEDIA_FRAME_TYPE_AUDIO;

    player->timestamp.u64 += this_port->info.samples_per_frame;

    return PJ_SUCCESS;
}


static pj_status_t mem_on_destroy(pjmedia_port *this_port)
{
    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE,
		     PJ_EINVALIDOP);

    /* Destroy signature */
    this_port->info.signature = 0;

    return PJ_SUCCESS;
}


