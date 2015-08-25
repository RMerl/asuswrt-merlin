/* $Id: splitcomb.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjmedia/splitcomb.h>
#include <pjmedia/delaybuf.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>


#define SIGNATURE	    PJMEDIA_PORT_SIGNATURE('S', 'p', 'C', 'b')
#define SIGNATURE_PORT	    PJMEDIA_PORT_SIGNATURE('S', 'p', 'C', 'P')
#define THIS_FILE	    "splitcomb.c"
#define TMP_SAMP_TYPE	    pj_int16_t

/* Maximum number of channels. */
#define MAX_CHANNELS	    16

/* Maximum number of buffers to be accommodated by delaybuf */
#define MAX_BUF_CNT	    PJMEDIA_SOUND_BUFFER_COUNT

/* Maximum number of burst before we pause the media flow */
#define MAX_BURST	    (buf_cnt + 6)

/* Maximum number of NULL frames received before we pause the
 * media flow.
 */
#define MAX_NULL_FRAMES	    (rport->max_burst)


/* Operations */
#define OP_PUT		    (1)
#define OP_GET		    (-1)


/* 
 * Media flow directions:
 *
 *             put_frame() +-----+
 *  UPSTREAM  ------------>|split|<--> DOWNSTREAM
 *            <------------|comb |
 *             get_frame() +-----+
 *
 */
enum sc_dir
{
    /* This is the media direction from the splitcomb to the 
     * downstream port(s), which happens when:
     *  - put_frame() is called to the splitcomb
     *  - get_frame() is called to the reverse channel port.
     */
    DIR_DOWNSTREAM,

    /* This is the media direction from the downstream port to 
     * the splitcomb, which happens when:
     *  - get_frame() is called to the splitcomb
     *  - put_frame() is called to the reverse channel port.
     */
    DIR_UPSTREAM
};



/*
 * This structure describes the splitter/combiner.
 */
struct splitcomb
{
    pjmedia_port      base;

    unsigned	      options;

    /* Array of ports, one for each channel */
    struct {
	pjmedia_port *port;
	pj_bool_t     reversed;
    } port_desc[MAX_CHANNELS];

    /* Temporary buffers needed to extract mono frame from
     * multichannel frame. We could use stack for this, but this
     * way it should be safer for devices with small stack size.
     */
    TMP_SAMP_TYPE    *get_buf;
    TMP_SAMP_TYPE    *put_buf;
};


/*
 * This structure describes reverse port.
 */
struct reverse_port
{
    pjmedia_port     base;
    struct splitcomb*parent;
    unsigned	     ch_num;

    /* Maximum burst before media flow is suspended.
     * With reverse port, it's possible that either end of the 
     * port doesn't actually process the media flow (meaning, it
     * stops calling get_frame()/put_frame()). When this happens,
     * the other end will encounter excessive underflow or overflow,
     * depending on which direction is not actively processed by
     * the stopping end.
     *
     * To avoid excessive underflow/overflow, the media flow will
     * be suspended once underflow/overflow goes over this max_burst
     * limit.
     */
    int		     max_burst;

    /* When the media interface port of the splitcomb or the reverse
     * channel port is registered to conference bridge, the bridge
     * will transmit NULL frames to the media port when the media
     * port is not receiving any audio from other slots (for example,
     * when no other slots are connected to the media port).
     *
     * When this happens, we will generate zero frame to our buffer,
     * to avoid underflow/overflow. But after too many NULL frames
     * are received, we will pause the media flow instead, to save
     * some processing.
     *
     * This value controls how many NULL frames can be received
     * before we suspend media flow for a particular direction.
     */
    unsigned	     max_null_frames;

    /* A reverse port need a temporary buffer to store frames
     * (because of the different phase, see splitcomb.h for details). 
     * Since we can not expect get_frame() and put_frame() to be
     * called evenly one after another, we use delay buffers to
     * accomodate the burst.
     *
     * We maintain state for each direction, hence the array. The
     * array is indexed by direction (sc_dir).
     */
    struct {

	/* The delay buffer where frames will be stored */
	pjmedia_delay_buf   *dbuf;

	/* Flag to indicate that audio flow on this direction
	 * is currently being suspended (perhaps because nothing
	 * is processing the frame on the other end).
	 */
	pj_bool_t	paused;

	/* Operation level. When the level exceeds a maximum value,
	 * the media flow on this direction will be paused.
	 */
	int		level;

	/* Timestamp. */
	pj_timestamp	ts;

	/* Number of NULL frames transmitted to this port so far.
	 * NULL frame indicate that nothing is transmitted, and 
	 * once we get too many of this, we should pause the media
	 * flow to reduce processing.
	 */
	unsigned	null_cnt;

    } buf[2];

    /* Must have temporary put buffer for the delay buf,
     * unfortunately.
     */
    pj_int16_t	      *tmp_up_buf;
};


/*
 * Prototypes.
 */
static pj_status_t put_frame(pjmedia_port *this_port, 
			     const pjmedia_frame *frame);
static pj_status_t get_frame(pjmedia_port *this_port, 
			     pjmedia_frame *frame);
static pj_status_t on_destroy(pjmedia_port *this_port);

static pj_status_t rport_put_frame(pjmedia_port *this_port, 
				   const pjmedia_frame *frame);
static pj_status_t rport_get_frame(pjmedia_port *this_port, 
				   pjmedia_frame *frame);
static pj_status_t rport_on_destroy(pjmedia_port *this_port);


/*
 * Create the splitter/combiner.
 */
PJ_DEF(pj_status_t) pjmedia_splitcomb_create( pj_pool_t *pool,
					      unsigned clock_rate,
					      unsigned channel_count,
					      unsigned samples_per_frame,
					      unsigned bits_per_sample,
					      unsigned options,
					      pjmedia_port **p_splitcomb)
{
    const pj_str_t name = pj_str("splitcomb");
    struct splitcomb *sc;

    /* Sanity check */
    PJ_ASSERT_RETURN(pool && clock_rate && channel_count &&
		     samples_per_frame && bits_per_sample &&
		     p_splitcomb, PJ_EINVAL);

    /* Only supports 16 bits per sample */
    PJ_ASSERT_RETURN(bits_per_sample == 16, PJ_EINVAL);

    *p_splitcomb = NULL;

    /* Create the splitter/combiner structure */
    sc = PJ_POOL_ZALLOC_T(pool, struct splitcomb);
    PJ_ASSERT_RETURN(sc != NULL, PJ_ENOMEM);

    /* Create temporary buffers */
    sc->get_buf = (TMP_SAMP_TYPE*)
		  pj_pool_alloc(pool, samples_per_frame * 
				      sizeof(TMP_SAMP_TYPE) /
				      channel_count);
    PJ_ASSERT_RETURN(sc->get_buf, PJ_ENOMEM);

    sc->put_buf = (TMP_SAMP_TYPE*)
		  pj_pool_alloc(pool, samples_per_frame * 
				      sizeof(TMP_SAMP_TYPE) /
				      channel_count);
    PJ_ASSERT_RETURN(sc->put_buf, PJ_ENOMEM);


    /* Save options */
    sc->options = options;

    /* Initialize port */
    pjmedia_port_info_init(&sc->base.info, &name, SIGNATURE, clock_rate,
			   channel_count, bits_per_sample, samples_per_frame);

    sc->base.put_frame = &put_frame;
    sc->base.get_frame = &get_frame;
    sc->base.on_destroy = &on_destroy;

    /* Init ports array */
    /*
    sc->port_desc = pj_pool_zalloc(pool, channel_count*sizeof(*sc->port_desc));
    */
    pj_bzero(sc->port_desc, sizeof(sc->port_desc));

    /* Done for now */
    *p_splitcomb = &sc->base;

    return PJ_SUCCESS;
}


/*
 * Attach media port with the same phase as the splitter/combiner.
 */
PJ_DEF(pj_status_t) pjmedia_splitcomb_set_channel( pjmedia_port *splitcomb,
						   unsigned ch_num,
						   unsigned options,
						   pjmedia_port *port)
{
    struct splitcomb *sc = (struct splitcomb*) splitcomb;

    /* Sanity check */
    PJ_ASSERT_RETURN(splitcomb && port, PJ_EINVAL);

    /* Make sure this is really a splitcomb port */
    PJ_ASSERT_RETURN(sc->base.info.signature == SIGNATURE, PJ_EINVAL);

    /* Check the channel number */
    PJ_ASSERT_RETURN(ch_num < sc->base.info.channel_count, PJ_EINVAL);

    /* options is unused for now */
    PJ_UNUSED_ARG(options);

    sc->port_desc[ch_num].port = port;
    sc->port_desc[ch_num].reversed = PJ_FALSE;

    return PJ_SUCCESS;
}


/*
 * Create reverse phase port for the specified channel.
 */
PJ_DEF(pj_status_t) pjmedia_splitcomb_create_rev_channel( pj_pool_t *pool,
				      pjmedia_port *splitcomb,
				      unsigned ch_num,
				      unsigned options,
				      pjmedia_port **p_chport)
{
    const pj_str_t name = pj_str("scomb-rev");
    struct splitcomb *sc = (struct splitcomb*) splitcomb;
    struct reverse_port *rport;
    unsigned buf_cnt, ptime;
    pjmedia_port *port;
    pj_status_t status;

    /* Sanity check */
    PJ_ASSERT_RETURN(pool && splitcomb, PJ_EINVAL);

    /* Make sure this is really a splitcomb port */
    PJ_ASSERT_RETURN(sc->base.info.signature == SIGNATURE, PJ_EINVAL);

    /* Check the channel number */
    PJ_ASSERT_RETURN(ch_num < sc->base.info.channel_count, PJ_EINVAL);

    /* options is unused for now */
    PJ_UNUSED_ARG(options);

    /* Create the port */
    rport = PJ_POOL_ZALLOC_T(pool, struct reverse_port);
    rport->parent = sc;
    rport->ch_num = ch_num;

    /* Initialize port info... */
    port = &rport->base;
    pjmedia_port_info_init(&port->info, &name, SIGNATURE_PORT, 
			   splitcomb->info.clock_rate, 1, 
			   splitcomb->info.bits_per_sample, 
			   splitcomb->info.samples_per_frame / 
				   splitcomb->info.channel_count);

    /* ... and the callbacks */
    port->put_frame = &rport_put_frame;
    port->get_frame = &rport_get_frame;
    port->on_destroy = &rport_on_destroy;

    /* Buffer settings */
    buf_cnt = options & 0xFF;
    if (buf_cnt == 0)
	buf_cnt = MAX_BUF_CNT;

    ptime = port->info.samples_per_frame * 1000 / port->info.clock_rate /
	    port->info.channel_count;

    rport->max_burst = MAX_BURST;
    rport->max_null_frames = MAX_NULL_FRAMES;

    /* Create downstream/put buffers */
    status = pjmedia_delay_buf_create(pool, "scombdb-dn",
				      port->info.clock_rate,
				      port->info.samples_per_frame,
				      port->info.channel_count,
				      buf_cnt * ptime, 0,
				      &rport->buf[DIR_DOWNSTREAM].dbuf);
    if (status != PJ_SUCCESS) {
	return status;
    }

    /* Create upstream/get buffers */
    status = pjmedia_delay_buf_create(pool, "scombdb-up",
				      port->info.clock_rate,
				      port->info.samples_per_frame,
				      port->info.channel_count,
				      buf_cnt * ptime, 0,
				      &rport->buf[DIR_UPSTREAM].dbuf);
    if (status != PJ_SUCCESS) {
	pjmedia_delay_buf_destroy(rport->buf[DIR_DOWNSTREAM].dbuf);
	return status;
    }

    /* And temporary upstream/get buffer */
    rport->tmp_up_buf = (pj_int16_t*)
	                pj_pool_alloc(pool, port->info.bytes_per_frame);

    /* Save port in the splitcomb */
    sc->port_desc[ch_num].port = &rport->base;
    sc->port_desc[ch_num].reversed = PJ_TRUE;


    /* Done */
    *p_chport = port;
    return status;
}


/* 
 * Extract one mono frame from a multichannel frame. 
 */
static void extract_mono_frame( const pj_int16_t *in,
			        pj_int16_t *out,
				unsigned ch,
				unsigned ch_cnt,
				unsigned samples_count)
{
    unsigned i;

    in += ch;
    for (i=0; i<samples_count; ++i) {
	*out++ = *in;
	in += ch_cnt;
    }
}


/* 
 * Put one mono frame into a multichannel frame 
 */
static void store_mono_frame( const pj_int16_t *in,
			      pj_int16_t *out,
			      unsigned ch,
			      unsigned ch_cnt,
			      unsigned samples_count)
{
    unsigned i;

    out += ch;
    for (i=0; i<samples_count; ++i) {
	*out = *in++;
	out += ch_cnt;
    }
}

/* Update operation on the specified direction  */
static void op_update(struct reverse_port *rport, int dir, int op)
{
    char *dir_name[2] = {"downstream", "upstream"};

    rport->buf[dir].level += op;

    if (op == OP_PUT) {
	rport->buf[dir].ts.u64 += rport->base.info.samples_per_frame;
    }

    if (rport->buf[dir].paused) {
	if (rport->buf[dir].level < -rport->max_burst) {
	    /* Prevent the level from overflowing and resets back to zero */
	    rport->buf[dir].level = -rport->max_burst;
	} else if (rport->buf[dir].level > rport->max_burst) {
	    /* Prevent the level from overflowing and resets back to zero */
	    rport->buf[dir].level = rport->max_burst;
	} else {
	    /* Level has fallen below max level, we can resume
	     * media flow.
	     */
	    PJ_LOG(5,(rport->base.info.name.ptr, 
		      "Resuming media flow on %s direction (level=%d)", 
		      dir_name[dir], rport->buf[dir].level));
	    rport->buf[dir].level = 0;
	    rport->buf[dir].paused = PJ_FALSE;

	    //This will cause disruption in audio, and it seems to be
	    //working fine without this anyway, so we disable it for now.
	    //pjmedia_delay_buf_learn(rport->buf[dir].dbuf);

	}
    } else {
	if (rport->buf[dir].level >= rport->max_burst ||
	    rport->buf[dir].level <= -rport->max_burst) 
	{
	    /* Level has reached maximum level, the other side of
	     * rport is not sending/retrieving frames. Pause the
	     * rport on this direction.
	     */
	    PJ_LOG(5,(rport->base.info.name.ptr, 
		      "Pausing media flow on %s direction (level=%d)", 
		      dir_name[dir], rport->buf[dir].level));
	    rport->buf[dir].paused = PJ_TRUE;
	}
    }
}


/*
 * "Write" a multichannel frame downstream. This would split 
 * the multichannel frame into individual mono channel, and write 
 * it to the appropriate port.
 */
static pj_status_t put_frame(pjmedia_port *this_port, 
			     const pjmedia_frame *frame)
{
    struct splitcomb *sc = (struct splitcomb*) this_port;
    unsigned ch;

    /* Handle null frame */
    if (frame->type == PJMEDIA_FRAME_TYPE_NONE) {
	for (ch=0; ch < this_port->info.channel_count; ++ch) {
	    pjmedia_port *port = sc->port_desc[ch].port;

	    if (!port) continue;

	    if (!sc->port_desc[ch].reversed) {
		pjmedia_port_put_frame(port, frame);
	    } else {
		struct reverse_port *rport = (struct reverse_port*)port;

		/* Update the number of NULL frames received. Once we have too
		 * many of this, we'll stop calling op_update() to let the
		 * media be suspended.
		 */

		if (++rport->buf[DIR_DOWNSTREAM].null_cnt > 
			rport->max_null_frames) 
		{
		    /* Prevent the counter from overflowing and resetting
		     * back to zero
		     */
		    rport->buf[DIR_DOWNSTREAM].null_cnt = 
			rport->max_null_frames + 1;
		    continue;
		}

		/* Write zero port to delaybuf so that it doesn't underflow. 
		 * If we don't do this, get_frame() on this direction will
		 * cause delaybuf to generate missing frame and the last
		 * frame transmitted to delaybuf will be replayed multiple
		 * times, which doesn't sound good.
		 */

		/* Update rport state. */
		op_update(rport, DIR_DOWNSTREAM, OP_PUT);

		/* Discard frame if rport is paused on this direction */
		if (rport->buf[DIR_DOWNSTREAM].paused)
		    continue;

		/* Generate zero frame. */
		pjmedia_zero_samples(sc->put_buf, 
				     port->info.samples_per_frame);

		/* Put frame to delay buffer */
		pjmedia_delay_buf_put(rport->buf[DIR_DOWNSTREAM].dbuf,
				      sc->put_buf);

	    }
	}
	return PJ_SUCCESS;
    }

    /* Not sure how we would handle partial frame, so better reject
     * it for now.
     */
    PJ_ASSERT_RETURN(frame->size == this_port->info.bytes_per_frame,
		     PJ_EINVAL);

    /* 
     * Write mono frame into each channels 
     */
    for (ch=0; ch < this_port->info.channel_count; ++ch) {
	pjmedia_port *port = sc->port_desc[ch].port;

	if (!port)
	    continue;

	/* Extract the mono frame to temporary buffer */
	extract_mono_frame((const pj_int16_t*)frame->buf, sc->put_buf, ch, 
			   this_port->info.channel_count, 
			   frame->size * 8 / 
			     this_port->info.bits_per_sample /
			     this_port->info.channel_count);

	if (!sc->port_desc[ch].reversed) {
	    /* Write to normal port */
	    pjmedia_frame mono_frame;

	    mono_frame.buf = sc->put_buf;
	    mono_frame.size = frame->size / this_port->info.channel_count;
	    mono_frame.type = frame->type;
	    mono_frame.timestamp.u64 = frame->timestamp.u64;

	    /* Write */
	    pjmedia_port_put_frame(port, &mono_frame);

	} else {
	    /* Write to reversed phase port */
	    struct reverse_port *rport = (struct reverse_port*)port;

	    /* Reset NULL frame counter */
	    rport->buf[DIR_DOWNSTREAM].null_cnt = 0;

	    /* Update rport state. */
	    op_update(rport, DIR_DOWNSTREAM, OP_PUT);

	    if (!rport->buf[DIR_DOWNSTREAM].paused) {
		pjmedia_delay_buf_put(rport->buf[DIR_DOWNSTREAM].dbuf, 
				      sc->put_buf);
	    }
	}
    }

    return PJ_SUCCESS;
}


/*
 * Get a multichannel frame upstream.
 * This will get mono channel frame from each port and put the
 * mono frame into the multichannel frame.
 */
static pj_status_t get_frame(pjmedia_port *this_port, 
			     pjmedia_frame *frame)
{
    struct splitcomb *sc = (struct splitcomb*) this_port;
    unsigned ch;
    pj_bool_t has_frame = PJ_FALSE;

    /* Read frame from each port */
    for (ch=0; ch < this_port->info.channel_count; ++ch) {
	pjmedia_port *port = sc->port_desc[ch].port;
	pjmedia_frame mono_frame;
	pj_status_t status;

	if (!port) {
	    pjmedia_zero_samples(sc->get_buf, 
				 this_port->info.samples_per_frame /
				    this_port->info.channel_count);

	} else if (sc->port_desc[ch].reversed == PJ_FALSE) {
	    /* Read from normal port */
	    mono_frame.buf = sc->get_buf;
	    mono_frame.size = port->info.bytes_per_frame;
	    mono_frame.timestamp.u64 = frame->timestamp.u64;

	    status = pjmedia_port_get_frame(port, &mono_frame);
	    if (status != PJ_SUCCESS || 
		mono_frame.type != PJMEDIA_FRAME_TYPE_AUDIO)
	    {
		pjmedia_zero_samples(sc->get_buf, 
				     port->info.samples_per_frame);
	    }

	    frame->timestamp.u64 = mono_frame.timestamp.u64;

	} else {
	    /* Read from temporary buffer for reverse port */
	    struct reverse_port *rport = (struct reverse_port*)port;

	    /* Update rport state. */
	    op_update(rport, DIR_UPSTREAM, OP_GET);

	    if (!rport->buf[DIR_UPSTREAM].paused) {
		pjmedia_delay_buf_get(rport->buf[DIR_UPSTREAM].dbuf, 
				      sc->get_buf);

	    } else {
		pjmedia_zero_samples(sc->get_buf, 
				     port->info.samples_per_frame);
	    }

	    frame->timestamp.u64 = rport->buf[DIR_UPSTREAM].ts.u64;
	}

	/* Combine the mono frame into multichannel frame */
	store_mono_frame(sc->get_buf, 
			 (pj_int16_t*)frame->buf, ch,
			 this_port->info.channel_count,
			 this_port->info.samples_per_frame /
			 this_port->info.channel_count);

	has_frame = PJ_TRUE;
    }

    /* Return NO_FRAME is we don't get any frames from downstream ports */
    if (has_frame) {
	frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
	frame->size = this_port->info.bytes_per_frame;
    } else
	frame->type = PJMEDIA_FRAME_TYPE_NONE;

    return PJ_SUCCESS;
}


static pj_status_t on_destroy(pjmedia_port *this_port)
{
    /* Nothing to do for the splitcomb
     * Reverse ports must be destroyed separately.
     */
    PJ_UNUSED_ARG(this_port);

    return PJ_SUCCESS;
}


/*
 * Put a frame in the reverse port (upstream direction). This frame
 * will be picked up by get_frame() above.
 */
static pj_status_t rport_put_frame(pjmedia_port *this_port, 
				   const pjmedia_frame *frame)
{
    struct reverse_port *rport = (struct reverse_port*) this_port;

    pj_assert(frame->size <= rport->base.info.bytes_per_frame);

    /* Handle NULL frame */
    if (frame->type != PJMEDIA_FRAME_TYPE_AUDIO) {
	/* Update the number of NULL frames received. Once we have too
	 * many of this, we'll stop calling op_update() to let the
	 * media be suspended.
	 */
	if (++rport->buf[DIR_UPSTREAM].null_cnt > rport->max_null_frames) {
	    /* Prevent the counter from overflowing and resetting back 
	     * to zero
	     */
	    rport->buf[DIR_UPSTREAM].null_cnt = rport->max_null_frames + 1;
	    return PJ_SUCCESS;
	}

	/* Write zero port to delaybuf so that it doesn't underflow. 
	 * If we don't do this, get_frame() on this direction will
	 * cause delaybuf to generate missing frame and the last
	 * frame transmitted to delaybuf will be replayed multiple
	 * times, which doesn't sound good.
	 */

	/* Update rport state. */
	op_update(rport, DIR_UPSTREAM, OP_PUT);

	/* Discard frame if rport is paused on this direction */
	if (rport->buf[DIR_UPSTREAM].paused)
	    return PJ_SUCCESS;

	/* Generate zero frame. */
	pjmedia_zero_samples(rport->tmp_up_buf, 
			     this_port->info.samples_per_frame);

	/* Put frame to delay buffer */
	return pjmedia_delay_buf_put(rport->buf[DIR_UPSTREAM].dbuf, 
				     rport->tmp_up_buf);
    }

    /* Not sure how to handle partial frame, so better reject for now */
    PJ_ASSERT_RETURN(frame->size == this_port->info.bytes_per_frame,
		     PJ_EINVAL);

    /* Reset NULL frame counter */
    rport->buf[DIR_UPSTREAM].null_cnt = 0;

    /* Update rport state. */
    op_update(rport, DIR_UPSTREAM, OP_PUT);

    /* Discard frame if rport is paused on this direction */
    if (rport->buf[DIR_UPSTREAM].paused)
	return PJ_SUCCESS;

    /* Unfortunately must copy to temporary buffer since delay buf
     * modifies the frame content.
     */
    pjmedia_copy_samples(rport->tmp_up_buf, (const pj_int16_t*)frame->buf,
		         this_port->info.samples_per_frame);

    /* Put frame to delay buffer */
    return pjmedia_delay_buf_put(rport->buf[DIR_UPSTREAM].dbuf, 
				 rport->tmp_up_buf);
}


/* Get a mono frame from a reversed phase channel (downstream direction).
 * The frame is put by put_frame() call to the splitcomb.
 */
static pj_status_t rport_get_frame(pjmedia_port *this_port, 
				   pjmedia_frame *frame)
{
    struct reverse_port *rport = (struct reverse_port*) this_port;

    /* Update state */
    op_update(rport, DIR_DOWNSTREAM, OP_GET);

    /* Return no frame if media flow on this direction is being
     * paused.
     */
    if (rport->buf[DIR_DOWNSTREAM].paused) {
	frame->type = PJMEDIA_FRAME_TYPE_NONE;
	return PJ_SUCCESS;
    }

    /* Get frame from delay buffer */
    frame->size = this_port->info.bytes_per_frame;
    frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
    frame->timestamp.u64 = rport->buf[DIR_DOWNSTREAM].ts.u64;

    return pjmedia_delay_buf_get(rport->buf[DIR_DOWNSTREAM].dbuf, 
				 (short*)frame->buf);
}


static pj_status_t rport_on_destroy(pjmedia_port *this_port)
{
    struct reverse_port *rport = (struct reverse_port*) this_port;

    pjmedia_delay_buf_destroy(rport->buf[DIR_DOWNSTREAM].dbuf);
    pjmedia_delay_buf_destroy(rport->buf[DIR_UPSTREAM].dbuf);

    return PJ_SUCCESS;
}

