/* $Id: conf_switch.c 4117 2012-05-03 04:05:10Z nanang $ */
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
#include <pjmedia/conference.h>
#include <pjmedia/alaw_ulaw.h>
#include <pjmedia/errno.h>
#include <pjmedia/port.h>
#include <pjmedia/silencedet.h>
#include <pjmedia/sound_port.h>
#include <pjmedia/stream.h>
#include <pj/array.h>
#include <pj/assert.h>
#include <pj/log.h>
#include <pj/pool.h>
#include <pj/string.h>

#if defined(PJMEDIA_CONF_USE_SWITCH_BOARD) && PJMEDIA_CONF_USE_SWITCH_BOARD!=0

/* CONF_DEBUG enables detailed operation of the conference bridge.
 * Beware that it prints large amounts of logs (several lines per frame).
 */
//#define CONF_DEBUG
#ifdef CONF_DEBUG
#   include <stdio.h>
#   define TRACE_(x)   PJ_LOG(5,x)
#else
#   define TRACE_(x)
#endif

#define THIS_FILE	    "conf_switch.c"

#define SIGNATURE	    PJMEDIA_CONF_SWITCH_SIGNATURE
#define SIGNATURE_PORT	    PJMEDIA_PORT_SIGNATURE('S', 'W', 'T', 'P')
#define NORMAL_LEVEL	    128
#define SLOT_TYPE	    unsigned
#define INVALID_SLOT	    ((SLOT_TYPE)-1)
#define BUFFER_SIZE	    PJMEDIA_MAX_MTU
#define MAX_LEVEL	    (32767)
#define MIN_LEVEL	    (-32768)

/*
 * DON'T GET CONFUSED WITH TX/RX!!
 *
 * TX and RX directions are always viewed from the conference bridge's point
 * of view, and NOT from the port's point of view. So TX means the bridge
 * is transmitting to the port, RX means the bridge is receiving from the
 * port.
 */


/**
 * This is a port connected to conference bridge.
 */
struct conf_port
{
    SLOT_TYPE		 slot;		/**< Array of listeners.	    */
    pj_str_t		 name;		/**< Port name.			    */
    pjmedia_port	*port;		/**< get_frame() and put_frame()    */
    pjmedia_port_op	 rx_setting;	/**< Can we receive from this port  */
    pjmedia_port_op	 tx_setting;	/**< Can we transmit to this port   */
    unsigned		 listener_cnt;	/**< Number of listeners.	    */
    SLOT_TYPE		*listener_slots;/**< Array of listeners.	    */
    unsigned		 transmitter_cnt;/**<Number of transmitters.	    */

    /* Shortcut for port info. */
    pjmedia_port_info	*info;

    /* Calculated signal levels: */
    unsigned		 tx_level;	/**< Last tx level to this port.    */
    unsigned		 rx_level;	/**< Last rx level from this port.  */

    /* The normalized signal level adjustment.
     * A value of 128 (NORMAL_LEVEL) means there's no adjustment.
     */
    unsigned		 tx_adj_level;	/**< Adjustment for TX.		    */
    unsigned		 rx_adj_level;	/**< Adjustment for RX.		    */

    pj_timestamp	 ts_clock;
    pj_timestamp	 ts_rx;
    pj_timestamp	 ts_tx;

    /* Tx buffer is a temporary buffer to be used when there's mismatch 
     * between port's ptime with conference's ptime. This buffer is used as 
     * the source to buffer the samples until there are enough samples to 
     * fulfill a complete frame to be transmitted to the port.
     */
    pj_uint8_t		 tx_buf[BUFFER_SIZE]; /**< Tx buffer.		    */
};


/*
 * Conference bridge.
 */
struct pjmedia_conf
{
    unsigned		  options;	/**< Bitmask options.		    */
    unsigned		  max_ports;	/**< Maximum ports.		    */
    unsigned		  port_cnt;	/**< Current number of ports.	    */
    unsigned		  connect_cnt;	/**< Total number of connections    */
    pjmedia_port	 *master_port;	/**< Port zero's port.		    */
    char		  master_name_buf[80]; /**< Port0 name buffer.	    */
    pj_mutex_t		 *mutex;	/**< Conference mutex.		    */
    struct conf_port	**ports;	/**< Array of ports.		    */
    pj_uint8_t		  buf[BUFFER_SIZE];	/**< Common buffer.	    */
};


/* Prototypes */
static pj_status_t put_frame(pjmedia_port *this_port, 
			     const pjmedia_frame *frame);
static pj_status_t get_frame(pjmedia_port *this_port, 
			     pjmedia_frame *frame);
static pj_status_t destroy_port(pjmedia_port *this_port);


/*
 * Create port.
 */
static pj_status_t create_conf_port( pj_pool_t *pool,
				     pjmedia_conf *conf,
				     pjmedia_port *port,
				     const pj_str_t *name,
				     struct conf_port **p_conf_port)
{
    struct conf_port *conf_port;
    pjmedia_frame *f;

    PJ_ASSERT_RETURN(pool && conf && port && name && p_conf_port, PJ_EINVAL);

    /* Create port. */
    conf_port = PJ_POOL_ZALLOC_T(pool, struct conf_port);

    /* Set name */
    pj_strdup_with_null(pool, &conf_port->name, name);

    /* Default has tx and rx enabled. */
    conf_port->rx_setting = PJMEDIA_PORT_ENABLE;
    conf_port->tx_setting = PJMEDIA_PORT_ENABLE;

    /* Default level adjustment is 128 (which means no adjustment) */
    conf_port->tx_adj_level = NORMAL_LEVEL;
    conf_port->rx_adj_level = NORMAL_LEVEL;

    /* Create transmit flag array */
    conf_port->listener_slots = (SLOT_TYPE*)
				pj_pool_zalloc(pool, 
					  conf->max_ports * sizeof(SLOT_TYPE));
    PJ_ASSERT_RETURN(conf_port->listener_slots, PJ_ENOMEM);

    /* Save some port's infos, for convenience. */
    conf_port->port = port;
    conf_port->info = &port->info;

    /* Init pjmedia_frame structure in the TX buffer. */
    f = (pjmedia_frame*)conf_port->tx_buf;
    f->buf = conf_port->tx_buf + sizeof(pjmedia_frame);

    /* Done */
    *p_conf_port = conf_port;
    return PJ_SUCCESS;
}

/*
 * Create port zero for the sound device.
 */
static pj_status_t create_sound_port( pj_pool_t *pool,
				      pjmedia_conf *conf )
{
    struct conf_port *conf_port;
    pj_str_t name = { "Master/sound", 12 };
    pj_status_t status;

    status = create_conf_port(pool, conf, conf->master_port, &name, &conf_port);
    if (status != PJ_SUCCESS)
	return status;

     /* Add the port to the bridge */
    conf_port->slot = 0;
    conf->ports[0] = conf_port;
    conf->port_cnt++;

    PJ_LOG(5,(THIS_FILE, "Sound device successfully created for port 0"));
    return PJ_SUCCESS;
}

/*
 * Create conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_conf_create( pj_pool_t *pool,
					 unsigned max_ports,
					 unsigned clock_rate,
					 unsigned channel_count,
					 unsigned samples_per_frame,
					 unsigned bits_per_sample,
					 unsigned options,
					 pjmedia_conf **p_conf )
{
    pjmedia_conf *conf;
    const pj_str_t name = { "Conf", 4 };
    pj_status_t status;

    /* Can only accept 16bits per sample, for now.. */
    PJ_ASSERT_RETURN(bits_per_sample == 16, PJ_EINVAL);

    PJ_LOG(5,(THIS_FILE, "Creating conference bridge with %d ports",
	      max_ports));

    /* Create and init conf structure. */
    conf = PJ_POOL_ZALLOC_T(pool, pjmedia_conf);
    PJ_ASSERT_RETURN(conf, PJ_ENOMEM);

    conf->ports = (struct conf_port**) 
		  pj_pool_zalloc(pool, max_ports*sizeof(void*));
    PJ_ASSERT_RETURN(conf->ports, PJ_ENOMEM);

    conf->options = options;
    conf->max_ports = max_ports;
    
    /* Create and initialize the master port interface. */
    conf->master_port = PJ_POOL_ZALLOC_T(pool, pjmedia_port);
    PJ_ASSERT_RETURN(conf->master_port, PJ_ENOMEM);
    
    pjmedia_port_info_init(&conf->master_port->info, &name, SIGNATURE,
			   clock_rate, channel_count, bits_per_sample,
			   samples_per_frame);

    conf->master_port->port_data.pdata = conf;
    conf->master_port->port_data.ldata = 0;

    conf->master_port->get_frame = &get_frame;
    conf->master_port->put_frame = &put_frame;
    conf->master_port->on_destroy = &destroy_port;


    /* Create port zero for sound device. */
    status = create_sound_port(pool, conf);
    if (status != PJ_SUCCESS)
	return status;

    /* Create mutex. */
    status = pj_mutex_create_recursive(pool, "conf", &conf->mutex);
    if (status != PJ_SUCCESS)
	return status;

    /* Done */

    *p_conf = conf;

    return PJ_SUCCESS;
}


/*
 * Pause sound device.
 */
static pj_status_t pause_sound( pjmedia_conf *conf )
{
    /* Do nothing. */
    PJ_UNUSED_ARG(conf);
    return PJ_SUCCESS;
}

/*
 * Resume sound device.
 */
static pj_status_t resume_sound( pjmedia_conf *conf )
{
    /* Do nothing. */
    PJ_UNUSED_ARG(conf);
    return PJ_SUCCESS;
}


/**
 * Destroy conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_conf_destroy( pjmedia_conf *conf )
{
    PJ_ASSERT_RETURN(conf != NULL, PJ_EINVAL);

    /* Destroy mutex */
    pj_mutex_destroy(conf->mutex);

    return PJ_SUCCESS;
}


/*
 * Destroy the master port (will destroy the conference)
 */
static pj_status_t destroy_port(pjmedia_port *this_port)
{
    pjmedia_conf *conf = (pjmedia_conf*) this_port->port_data.pdata;
    return pjmedia_conf_destroy(conf);
}

/*
 * Get port zero interface.
 */
PJ_DEF(pjmedia_port*) pjmedia_conf_get_master_port(pjmedia_conf *conf)
{
    /* Sanity check. */
    PJ_ASSERT_RETURN(conf != NULL, NULL);

    /* Can only return port interface when PJMEDIA_CONF_NO_DEVICE was
     * present in the option.
     */
    PJ_ASSERT_RETURN((conf->options & PJMEDIA_CONF_NO_DEVICE) != 0, NULL);
    
    return conf->master_port;
}


/*
 * Set master port name.
 */
PJ_DEF(pj_status_t) pjmedia_conf_set_port0_name(pjmedia_conf *conf,
						const pj_str_t *name)
{
    unsigned len;

    /* Sanity check. */
    PJ_ASSERT_RETURN(conf != NULL && name != NULL, PJ_EINVAL);

    len = name->slen;
    if (len > sizeof(conf->master_name_buf))
	len = sizeof(conf->master_name_buf);
    
    if (len > 0) pj_memcpy(conf->master_name_buf, name->ptr, len);

    conf->ports[0]->name.ptr = conf->master_name_buf;
    conf->ports[0]->name.slen = len;

    conf->master_port->info.name = conf->ports[0]->name;

    return PJ_SUCCESS;
}

/*
 * Add stream port to the conference bridge.
 */
PJ_DEF(pj_status_t) pjmedia_conf_add_port( pjmedia_conf *conf,
					   pj_pool_t *pool,
					   pjmedia_port *strm_port,
					   const pj_str_t *port_name,
					   unsigned *p_port )
{
    struct conf_port *conf_port;
    unsigned index;
    pj_status_t status;

    PJ_ASSERT_RETURN(conf && pool && strm_port, PJ_EINVAL);
    /*
    PJ_ASSERT_RETURN(conf->clock_rate == strm_port->info.clock_rate, 
		     PJMEDIA_ENCCLOCKRATE);
    PJ_ASSERT_RETURN(conf->channel_count == strm_port->info.channel_count, 
		     PJMEDIA_ENCCHANNEL);
    PJ_ASSERT_RETURN(conf->bits_per_sample == strm_port->info.bits_per_sample,
		     PJMEDIA_ENCBITS);
    */

    /* Port's samples per frame should be equal to or multiplication of 
     * conference's samples per frame.
     */
    /*
    Not sure if this is needed!
    PJ_ASSERT_RETURN((conf->samples_per_frame %
		     strm_port->info.samples_per_frame==0) ||
		     (strm_port->info.samples_per_frame %
		     conf->samples_per_frame==0),
		     PJMEDIA_ENCSAMPLESPFRAME);
    */

    /* If port_name is not specified, use the port's name */
    if (!port_name)
	port_name = &strm_port->info.name;

    pj_mutex_lock(conf->mutex);

    if (conf->port_cnt >= conf->max_ports) {
	pj_assert(!"Too many ports");
	pj_mutex_unlock(conf->mutex);
	return PJ_ETOOMANY;
    }

    /* Find empty port in the conference bridge. */
    for (index=0; index < conf->max_ports; ++index) {
	if (conf->ports[index] == NULL)
	    break;
    }

    pj_assert(index != conf->max_ports);

    /* Create conf port structure. */
    status = create_conf_port(pool, conf, strm_port, port_name, &conf_port);
    if (status != PJ_SUCCESS) {
	pj_mutex_unlock(conf->mutex);
	return status;
    }

    /* Put the port. */
    conf_port->slot = index;
    conf->ports[index] = conf_port;
    conf->port_cnt++;

    /* Done. */
    if (p_port) {
	*p_port = index;
    }

    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}


/*
 * Add passive port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_add_passive_port( pjmedia_conf *conf,
						   pj_pool_t *pool,
						   const pj_str_t *name,
						   unsigned clock_rate,
						   unsigned channel_count,
						   unsigned samples_per_frame,
						   unsigned bits_per_sample,
						   unsigned options,
						   unsigned *p_slot,
						   pjmedia_port **p_port )
{
    PJ_UNUSED_ARG(conf);
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(name);
    PJ_UNUSED_ARG(clock_rate);
    PJ_UNUSED_ARG(channel_count);
    PJ_UNUSED_ARG(samples_per_frame);
    PJ_UNUSED_ARG(bits_per_sample);
    PJ_UNUSED_ARG(options);
    PJ_UNUSED_ARG(p_slot);
    PJ_UNUSED_ARG(p_port);

    return PJ_ENOTSUP;
}



/*
 * Change TX and RX settings for the port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_configure_port( pjmedia_conf *conf,
						  unsigned slot,
						  pjmedia_port_op tx,
						  pjmedia_port_op rx)
{
    struct conf_port *conf_port;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && slot<conf->max_ports, PJ_EINVAL);

    pj_mutex_lock(conf->mutex);

    /* Port must be valid. */
    conf_port = conf->ports[slot];
    if (conf_port == NULL) {
	pj_mutex_unlock(conf->mutex);
	return PJ_EINVAL;
    }

    if (tx != PJMEDIA_PORT_NO_CHANGE)
	conf_port->tx_setting = tx;

    if (rx != PJMEDIA_PORT_NO_CHANGE)
	conf_port->rx_setting = rx;

    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}


/*
 * Connect port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_connect_port( pjmedia_conf *conf,
					       unsigned src_slot,
					       unsigned sink_slot,
					       int level )
{
    struct conf_port *src_port, *dst_port;
    pj_bool_t start_sound = PJ_FALSE;
    unsigned i;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && src_slot<conf->max_ports && 
		     sink_slot<conf->max_ports, PJ_EINVAL);

    /* For now, level MUST be zero. */
    PJ_ASSERT_RETURN(level == 0, PJ_EINVAL);

    pj_mutex_lock(conf->mutex);

    /* Ports must be valid. */
    src_port = conf->ports[src_slot];
    dst_port = conf->ports[sink_slot];
    if (!src_port || !dst_port) {
	pj_mutex_unlock(conf->mutex);
	return PJ_EINVAL;
    }

    /* Format must match. */
    if (src_port->info->format.id != dst_port->info->format.id ||
	src_port->info->format.bitrate != dst_port->info->format.bitrate) 
    {
	pj_mutex_unlock(conf->mutex);
	return PJMEDIA_ENOTCOMPATIBLE;
    }

    /* Clock rate must match. */
    if (src_port->info->clock_rate != dst_port->info->clock_rate) {
	pj_mutex_unlock(conf->mutex);
	return PJMEDIA_ENCCLOCKRATE;
    }

    /* Channel count must match. */
    if (src_port->info->channel_count != dst_port->info->channel_count) {
	pj_mutex_unlock(conf->mutex);
	return PJMEDIA_ENCCHANNEL;
    }

    /* Source and sink ptime must be equal or a multiplication factor. */
    if ((src_port->info->samples_per_frame % 
	 dst_port->info->samples_per_frame != 0) &&
        (dst_port->info->samples_per_frame % 
         src_port->info->samples_per_frame != 0))
    {
	pj_mutex_unlock(conf->mutex);
	return PJMEDIA_ENCSAMPLESPFRAME;
    }
    
    /* If sink is currently listening to other ports, it needs to be released
     * first before the new connection made.
     */ 
    if (dst_port->transmitter_cnt > 0) {
	unsigned j;
	pj_bool_t transmitter_found = PJ_FALSE;

	pj_assert(dst_port->transmitter_cnt == 1);
	for (j=0; j<conf->max_ports && !transmitter_found; ++j) {
	    if (conf->ports[j]) {
		unsigned k;

		for (k=0; k < conf->ports[j]->listener_cnt; ++k) {
		    if (conf->ports[j]->listener_slots[k] == sink_slot) {
			PJ_LOG(2,(THIS_FILE, "Connection [%d->%d] is "
				  "disconnected for new connection [%d->%d]",
				  j, sink_slot, src_slot, sink_slot));
			pjmedia_conf_disconnect_port(conf, j, sink_slot);
			transmitter_found = PJ_TRUE;
			break;
		    }
		}
	    }
	}
	pj_assert(dst_port->transmitter_cnt == 0);
    }

    /* Check if connection has been made */
    for (i=0; i<src_port->listener_cnt; ++i) {
	if (src_port->listener_slots[i] == sink_slot)
	    break;
    }

    if (i == src_port->listener_cnt) {
	src_port->listener_slots[src_port->listener_cnt] = sink_slot;
	++conf->connect_cnt;
	++src_port->listener_cnt;
	++dst_port->transmitter_cnt;

	if (conf->connect_cnt == 1)
	    start_sound = 1;

	PJ_LOG(4,(THIS_FILE,"Port %d (%.*s) transmitting to port %d (%.*s)",
		  src_slot,
		  (int)src_port->name.slen,
		  src_port->name.ptr,
		  sink_slot,
		  (int)dst_port->name.slen,
		  dst_port->name.ptr));
    }

    pj_mutex_unlock(conf->mutex);

    /* Sound device must be started without mutex, otherwise the
     * sound thread will deadlock (?)
     */
    if (start_sound)
	resume_sound(conf);

    return PJ_SUCCESS;
}


/*
 * Disconnect port
 */
PJ_DEF(pj_status_t) pjmedia_conf_disconnect_port( pjmedia_conf *conf,
						  unsigned src_slot,
						  unsigned sink_slot )
{
    struct conf_port *src_port, *dst_port;
    unsigned i;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && src_slot<conf->max_ports && 
		     sink_slot<conf->max_ports, PJ_EINVAL);

    pj_mutex_lock(conf->mutex);

    /* Ports must be valid. */
    src_port = conf->ports[src_slot];
    dst_port = conf->ports[sink_slot];
    if (!src_port || !dst_port) {
	pj_mutex_unlock(conf->mutex);
	return PJ_EINVAL;
    }

    /* Check if connection has been made */
    for (i=0; i<src_port->listener_cnt; ++i) {
	if (src_port->listener_slots[i] == sink_slot)
	    break;
    }

    if (i != src_port->listener_cnt) {
	pjmedia_frame_ext *f;

	pj_assert(src_port->listener_cnt > 0 && 
		  src_port->listener_cnt < conf->max_ports);
	pj_assert(dst_port->transmitter_cnt > 0 && 
		  dst_port->transmitter_cnt < conf->max_ports);
	pj_array_erase(src_port->listener_slots, sizeof(SLOT_TYPE), 
		       src_port->listener_cnt, i);
	--conf->connect_cnt;
	--src_port->listener_cnt;
	--dst_port->transmitter_cnt;
	
	/* Cleanup listener TX buffer. */
	f = (pjmedia_frame_ext*)dst_port->tx_buf;
	f->base.type = PJMEDIA_FRAME_TYPE_NONE;
	f->base.size = 0;
	f->samples_cnt = 0;
	f->subframe_cnt = 0;

	PJ_LOG(4,(THIS_FILE,
		  "Port %d (%.*s) stop transmitting to port %d (%.*s)",
		  src_slot,
		  (int)src_port->name.slen,
		  src_port->name.ptr,
		  sink_slot,
		  (int)dst_port->name.slen,
		  dst_port->name.ptr));
    }

    pj_mutex_unlock(conf->mutex);

    if (conf->connect_cnt == 0) {
	pause_sound(conf);
    }

    return PJ_SUCCESS;
}

/*
 * Get number of ports currently registered to the conference bridge.
 */
PJ_DEF(unsigned) pjmedia_conf_get_port_count(pjmedia_conf *conf)
{
    return conf->port_cnt;
}

/*
 * Get total number of ports connections currently set up in the bridge.
 */
PJ_DEF(unsigned) pjmedia_conf_get_connect_count(pjmedia_conf *conf)
{
    return conf->connect_cnt;
}


/*
 * Remove the specified port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_remove_port( pjmedia_conf *conf,
					      unsigned port )
{
    struct conf_port *conf_port;
    unsigned i;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && port < conf->max_ports, PJ_EINVAL);

    /* Suspend the sound devices.
     * Don't want to remove port while port is being accessed by sound
     * device's threads!
     */

    pj_mutex_lock(conf->mutex);

    /* Port must be valid. */
    conf_port = conf->ports[port];
    if (conf_port == NULL) {
	pj_mutex_unlock(conf->mutex);
	return PJ_EINVAL;
    }

    conf_port->tx_setting = PJMEDIA_PORT_DISABLE;
    conf_port->rx_setting = PJMEDIA_PORT_DISABLE;

    /* Remove this port from transmit array of other ports. */
    for (i=0; i<conf->max_ports; ++i) {
	unsigned j;
	struct conf_port *src_port;

	src_port = conf->ports[i];

	if (!src_port)
	    continue;

	if (src_port->listener_cnt == 0)
	    continue;

	for (j=0; j<src_port->listener_cnt; ++j) {
	    if (src_port->listener_slots[j] == port) {
		pj_array_erase(src_port->listener_slots, sizeof(SLOT_TYPE),
			       src_port->listener_cnt, j);
		pj_assert(conf->connect_cnt > 0);
		--conf->connect_cnt;
		--src_port->listener_cnt;
		break;
	    }
	}
    }

    /* Update transmitter_cnt of ports we're transmitting to */
    while (conf_port->listener_cnt) {
	unsigned dst_slot;
	struct conf_port *dst_port;
	pjmedia_frame_ext *f;

	dst_slot = conf_port->listener_slots[conf_port->listener_cnt-1];
	dst_port = conf->ports[dst_slot];
	--dst_port->transmitter_cnt;
	--conf_port->listener_cnt;
	pj_assert(conf->connect_cnt > 0);
	--conf->connect_cnt;

	/* Cleanup & reinit listener TX buffer. */
	f = (pjmedia_frame_ext*)dst_port->tx_buf;
	f->base.type = PJMEDIA_FRAME_TYPE_NONE;
	f->base.size = 0;
	f->samples_cnt = 0;
	f->subframe_cnt = 0;
    }

    /* Remove the port. */
    conf->ports[port] = NULL;
    --conf->port_cnt;

    pj_mutex_unlock(conf->mutex);


    /* Stop sound if there's no connection. */
    if (conf->connect_cnt == 0) {
	pause_sound(conf);
    }

    return PJ_SUCCESS;
}


/*
 * Enum ports.
 */
PJ_DEF(pj_status_t) pjmedia_conf_enum_ports( pjmedia_conf *conf,
					     unsigned ports[],
					     unsigned *p_count )
{
    unsigned i, count=0;

    PJ_ASSERT_RETURN(conf && p_count && ports, PJ_EINVAL);

    /* Lock mutex */
    pj_mutex_lock(conf->mutex);

    for (i=0; i<conf->max_ports && count<*p_count; ++i) {
	if (!conf->ports[i])
	    continue;

	ports[count++] = i;
    }

    /* Unlock mutex */
    pj_mutex_unlock(conf->mutex);

    *p_count = count;
    return PJ_SUCCESS;
}

/*
 * Get port info
 */
PJ_DEF(pj_status_t) pjmedia_conf_get_port_info( pjmedia_conf *conf,
						unsigned slot,
						pjmedia_conf_port_info *info)
{
    struct conf_port *conf_port;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && slot<conf->max_ports, PJ_EINVAL);

    /* Lock mutex */
    pj_mutex_lock(conf->mutex);

    /* Port must be valid. */
    conf_port = conf->ports[slot];
    if (conf_port == NULL) {
	pj_mutex_unlock(conf->mutex);
	return PJ_EINVAL;
    }

    pj_bzero(info, sizeof(pjmedia_conf_port_info));

    info->slot = slot;
    info->name = conf_port->name;
    info->tx_setting = conf_port->tx_setting;
    info->rx_setting = conf_port->rx_setting;
    info->listener_cnt = conf_port->listener_cnt;
    info->listener_slots = conf_port->listener_slots;
    info->transmitter_cnt = conf_port->transmitter_cnt;
    info->clock_rate = conf_port->info->clock_rate;
    info->channel_count = conf_port->info->channel_count;
    info->samples_per_frame = conf_port->info->samples_per_frame;
    info->bits_per_sample = conf_port->info->bits_per_sample;
    info->format = conf_port->port->info.format;
    info->tx_adj_level = conf_port->tx_adj_level - NORMAL_LEVEL;
    info->rx_adj_level = conf_port->rx_adj_level - NORMAL_LEVEL;

    /* Unlock mutex */
    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjmedia_conf_get_ports_info(pjmedia_conf *conf,
						unsigned *size,
						pjmedia_conf_port_info info[])
{
    unsigned i, count=0;

    PJ_ASSERT_RETURN(conf && size && info, PJ_EINVAL);

    /* Lock mutex */
    pj_mutex_lock(conf->mutex);

    for (i=0; i<conf->max_ports && count<*size; ++i) {
	if (!conf->ports[i])
	    continue;

	pjmedia_conf_get_port_info(conf, i, &info[count]);
	++count;
    }

    /* Unlock mutex */
    pj_mutex_unlock(conf->mutex);

    *size = count;
    return PJ_SUCCESS;
}


/*
 * Get signal level.
 */
PJ_DEF(pj_status_t) pjmedia_conf_get_signal_level( pjmedia_conf *conf,
						   unsigned slot,
						   unsigned *tx_level,
						   unsigned *rx_level)
{
    struct conf_port *conf_port;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && slot<conf->max_ports, PJ_EINVAL);

    /* Lock mutex */
    pj_mutex_lock(conf->mutex);

    /* Port must be valid. */
    conf_port = conf->ports[slot];
    if (conf_port == NULL) {
	pj_mutex_unlock(conf->mutex);
	return PJ_EINVAL;
    }

    if (tx_level != NULL) {
	*tx_level = conf_port->tx_level;
    }

    if (rx_level != NULL) 
	*rx_level = conf_port->rx_level;

    /* Unlock mutex */
    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}


/*
 * Adjust RX level of individual port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_adjust_rx_level( pjmedia_conf *conf,
						  unsigned slot,
						  int adj_level )
{
    struct conf_port *conf_port;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && slot<conf->max_ports, PJ_EINVAL);

    /* Value must be from -128 to +127 */
    /* Disabled, you can put more than +127, at your own risk: 
     PJ_ASSERT_RETURN(adj_level >= -128 && adj_level <= 127, PJ_EINVAL);
     */
    PJ_ASSERT_RETURN(adj_level >= -128, PJ_EINVAL);

    /* Lock mutex */
    pj_mutex_lock(conf->mutex);

    /* Port must be valid. */
    conf_port = conf->ports[slot];
    if (conf_port == NULL) {
	pj_mutex_unlock(conf->mutex);
	return PJ_EINVAL;
    }

    /* Level adjustment is applicable only for ports that work with raw PCM. */
    PJ_ASSERT_RETURN(conf_port->info->format.id == PJMEDIA_FORMAT_L16,
		     PJ_EIGNORED);

    /* Set normalized adjustment level. */
    conf_port->rx_adj_level = adj_level + NORMAL_LEVEL;

    /* Unlock mutex */
    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}


/*
 * Adjust TX level of individual port.
 */
PJ_DEF(pj_status_t) pjmedia_conf_adjust_tx_level( pjmedia_conf *conf,
						  unsigned slot,
						  int adj_level )
{
    struct conf_port *conf_port;

    /* Check arguments */
    PJ_ASSERT_RETURN(conf && slot<conf->max_ports, PJ_EINVAL);

    /* Value must be from -128 to +127 */
    /* Disabled, you can put more than +127,, at your own risk:
     PJ_ASSERT_RETURN(adj_level >= -128 && adj_level <= 127, PJ_EINVAL);
     */
    PJ_ASSERT_RETURN(adj_level >= -128, PJ_EINVAL);

    /* Lock mutex */
    pj_mutex_lock(conf->mutex);

    /* Port must be valid. */
    conf_port = conf->ports[slot];
    if (conf_port == NULL) {
	pj_mutex_unlock(conf->mutex);
	return PJ_EINVAL;
    }

    /* Level adjustment is applicable only for ports that work with raw PCM. */
    PJ_ASSERT_RETURN(conf_port->info->format.id == PJMEDIA_FORMAT_L16,
		     PJ_EIGNORED);

    /* Set normalized adjustment level. */
    conf_port->tx_adj_level = adj_level + NORMAL_LEVEL;

    /* Unlock mutex */
    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}

/* Deliver frm_src to a listener port, eventually call  port's put_frame() 
 * when samples count in the frm_dst are equal to port's samples_per_frame.
 */
static pj_status_t write_frame(struct conf_port *cport_dst,
			       const pjmedia_frame *frm_src)
{
    pjmedia_frame *frm_dst = (pjmedia_frame*)cport_dst->tx_buf;
    
    PJ_TODO(MAKE_SURE_DEST_FRAME_HAS_ENOUGH_SPACE);

    frm_dst->type = frm_src->type;
    frm_dst->timestamp = cport_dst->ts_tx;

    if (frm_src->type == PJMEDIA_FRAME_TYPE_EXTENDED) {

	pjmedia_frame_ext *f_src = (pjmedia_frame_ext*)frm_src;
	pjmedia_frame_ext *f_dst = (pjmedia_frame_ext*)frm_dst;
	unsigned i;

	for (i = 0; i < f_src->subframe_cnt; ++i) {
	    pjmedia_frame_ext_subframe *sf;
	    
	    /* Copy frame to listener's TX buffer. */
	    sf = pjmedia_frame_ext_get_subframe(f_src, i);
	    pjmedia_frame_ext_append_subframe(f_dst, sf->data, sf->bitlen, 
					      f_src->samples_cnt / 
					      f_src->subframe_cnt);

	    /* Check if it's time to deliver the TX buffer to listener, 
	     * i.e: samples count in TX buffer equal to listener's
	     * samples per frame.
	     */
	    if (f_dst->samples_cnt >= cport_dst->info->samples_per_frame)
	    {
		if (cport_dst->slot) {
		    pjmedia_port_put_frame(cport_dst->port, 
					   (pjmedia_frame*)f_dst);

		    /* Reset TX buffer. */
		    f_dst->subframe_cnt = 0;
		    f_dst->samples_cnt = 0;
		}

		/* Update TX timestamp. */
		pj_add_timestamp32(&cport_dst->ts_tx, 
				   cport_dst->info->samples_per_frame);
	    }
	}

    } else if (frm_src->type == PJMEDIA_FRAME_TYPE_AUDIO) {

	pj_int16_t *f_start, *f_end;

	f_start = (pj_int16_t*)frm_src->buf;
	f_end   = f_start + (frm_src->size >> 1);

	while (f_start < f_end) {
	    unsigned nsamples_to_copy, nsamples_req;

	    /* Copy frame to listener's TX buffer.
	     * Note that if the destination is port 0, just copy the whole
	     * available samples.
	     */
	    nsamples_to_copy = f_end - f_start;
	    nsamples_req = cport_dst->info->samples_per_frame - 
			  (frm_dst->size>>1);
	    if (cport_dst->slot && nsamples_to_copy > nsamples_req)
		nsamples_to_copy = nsamples_req;

	    /* Adjust TX level. */
	    if (cport_dst->tx_adj_level != NORMAL_LEVEL) {
		pj_int16_t *p, *p_end;

		p = f_start;
		p_end = p + nsamples_to_copy;
		while (p < p_end) {
		    pj_int32_t itemp = *p;

		    /* Adjust the level */
		    itemp = (itemp * cport_dst->tx_adj_level) >> 7;

		    /* Clip the signal if it's too loud */
		    if (itemp > MAX_LEVEL) itemp = MAX_LEVEL;
		    else if (itemp < MIN_LEVEL) itemp = MIN_LEVEL;

		    /* Put back in the buffer. */
		    *p = (pj_int16_t)itemp;
		    ++p;
		}
	    }

	    pjmedia_copy_samples((pj_int16_t*)frm_dst->buf + (frm_dst->size>>1),
				 f_start, 
				 nsamples_to_copy);
	    frm_dst->size += nsamples_to_copy << 1;
	    f_start += nsamples_to_copy;

	    /* Check if it's time to deliver the TX buffer to listener, 
	     * i.e: samples count in TX buffer equal to listener's
	     * samples per frame. Note that for destination port 0 this
	     * function will just populate all samples in the TX buffer.
	     */
	    if (cport_dst->slot == 0) {
		/* Update TX timestamp. */
		pj_add_timestamp32(&cport_dst->ts_tx, nsamples_to_copy);
	    } else if ((frm_dst->size >> 1) == 
		       cport_dst->info->samples_per_frame)
	    {
		    pjmedia_port_put_frame(cport_dst->port, frm_dst);

		    /* Reset TX buffer. */
		    frm_dst->size = 0;

		/* Update TX timestamp. */
		pj_add_timestamp32(&cport_dst->ts_tx, 
				   cport_dst->info->samples_per_frame);
	    }
	}

    } else if (frm_src->type == PJMEDIA_FRAME_TYPE_NONE) {

	/* Check port format. */
	if (cport_dst->port &&
	    cport_dst->port->info.format.id == PJMEDIA_FORMAT_L16)
	{
	    /* When there is already some samples in listener's TX buffer, 
	     * pad the buffer with "zero samples".
	     */
	    if (frm_dst->size != 0) {
		pjmedia_zero_samples((pj_int16_t*)frm_dst->buf,
				     cport_dst->info->samples_per_frame - 
				     (frm_dst->size>>1));

		frm_dst->type = PJMEDIA_FRAME_TYPE_AUDIO;
		frm_dst->size = cport_dst->info->samples_per_frame << 1;
		if (cport_dst->slot) {
		    pjmedia_port_put_frame(cport_dst->port, frm_dst);

		    /* Reset TX buffer. */
		    frm_dst->size = 0;
		}

		/* Update TX timestamp. */
		pj_add_timestamp32(&cport_dst->ts_tx, 
				   cport_dst->info->samples_per_frame);
	    }
	} else {
	    pjmedia_frame_ext *f_dst = (pjmedia_frame_ext*)frm_dst;

	    if (f_dst->samples_cnt != 0) {
		frm_dst->type = PJMEDIA_FRAME_TYPE_EXTENDED;
		pjmedia_frame_ext_append_subframe(f_dst, NULL, 0, (pj_uint16_t)
		    (cport_dst->info->samples_per_frame - f_dst->samples_cnt));
		if (cport_dst->slot) {
		    pjmedia_port_put_frame(cport_dst->port, frm_dst);

		    /* Reset TX buffer. */
		    f_dst->subframe_cnt = 0;
		    f_dst->samples_cnt = 0;
		}

		/* Update TX timestamp. */
		pj_add_timestamp32(&cport_dst->ts_tx, 
				   cport_dst->info->samples_per_frame);
	    }
	}

	/* Synchronize clock. */
	while (pj_cmp_timestamp(&cport_dst->ts_clock, 
				&cport_dst->ts_tx) > 0)
	{
	    frm_dst->type = PJMEDIA_FRAME_TYPE_NONE;
	    frm_dst->timestamp = cport_dst->ts_tx;
	    if (cport_dst->slot)
		pjmedia_port_put_frame(cport_dst->port, frm_dst);

	    /* Update TX timestamp. */
	    pj_add_timestamp32(&cport_dst->ts_tx, cport_dst->info->samples_per_frame);
	}
    }

    return PJ_SUCCESS;
}

/*
 * Player callback.
 */
static pj_status_t get_frame(pjmedia_port *this_port, 
			     pjmedia_frame *frame)
{
    pjmedia_conf *conf = (pjmedia_conf*) this_port->port_data.pdata;
    unsigned ci, i;
    
    /* Lock mutex */
    pj_mutex_lock(conf->mutex);

    /* Call get_frame() from all ports (except port 0) that has 
     * receiver and distribute the frame (put the frame to the destination 
     * port's buffer to accommodate different ptime, and ultimately call 
     * put_frame() of that port) to ports that are receiving from this port.
     */
    for (i=1, ci=1; i<conf->max_ports && ci<conf->port_cnt; ++i) {
	struct conf_port *cport = conf->ports[i];

	/* Skip empty port. */
	if (!cport)
	    continue;

	/* Var "ci" is to count how many ports have been visited so far. */
	++ci;

	/* Update clock of the port. */
	pj_add_timestamp32(&cport->ts_clock, 
			   conf->master_port->info.samples_per_frame);

	/* Skip if we're not allowed to receive from this port or 
	 * the port doesn't have listeners.
	 */
	if (cport->rx_setting == PJMEDIA_PORT_DISABLE || 
	    cport->listener_cnt == 0)
	{
	    cport->rx_level = 0;
	    pj_add_timestamp32(&cport->ts_rx, 
			       conf->master_port->info.samples_per_frame);
	    continue;
	}

	/* Get frame from each port, put it to the listener TX buffer,
	 * and eventually call put_frame() of the listener. This loop 
	 * will also make sure the ptime between conf & port synchronized.
	 */
	while (pj_cmp_timestamp(&cport->ts_clock, &cport->ts_rx) > 0) {
	    pjmedia_frame *f = (pjmedia_frame*)conf->buf;
	    pj_status_t status;
	    unsigned j;
	    pj_int32_t level = 0;

	    pj_add_timestamp32(&cport->ts_rx, cport->info->samples_per_frame);
	    
	    f->buf = &conf->buf[sizeof(pjmedia_frame)];
	    f->size = cport->info->samples_per_frame<<1;

	    /* Get frame from port. */
	    status = pjmedia_port_get_frame(cport->port, f);
	    if (status != PJ_SUCCESS)
		continue;

	    /* Calculate & adjust RX level. */
	    if (f->type == PJMEDIA_FRAME_TYPE_AUDIO) {
		if (cport->rx_adj_level != NORMAL_LEVEL) {
		    pj_int16_t *p = (pj_int16_t*)f->buf;
		    pj_int16_t *end;

		    end = p + (f->size >> 1);
		    while (p < end) {
			pj_int32_t itemp = *p;

			/* Adjust the level */
			itemp = (itemp * cport->rx_adj_level) >> 7;

			/* Clip the signal if it's too loud */
			if (itemp > MAX_LEVEL) itemp = MAX_LEVEL;
			else if (itemp < MIN_LEVEL) itemp = MIN_LEVEL;

			level += PJ_ABS(itemp);

			/* Put back in the buffer. */
			*p = (pj_int16_t)itemp;
			++p;
		    }
		    level /= (f->size >> 1);
		} else {
		    level = pjmedia_calc_avg_signal((const pj_int16_t*)f->buf,
						    f->size >> 1);
		}
	    } else if (f->type == PJMEDIA_FRAME_TYPE_EXTENDED) {
		/* For extended frame, level is unknown, so we just set 
		 * it to NORMAL_LEVEL. 
		 */
		level = NORMAL_LEVEL;
	    }

	    cport->rx_level = pjmedia_linear2ulaw(level) ^ 0xff;

	    /* Put the frame to all listeners. */
	    for (j=0; j < cport->listener_cnt; ++j) 
	    {
		struct conf_port *listener;

		listener = conf->ports[cport->listener_slots[j]];

		/* Skip if this listener doesn't want to receive audio */
		if (listener->tx_setting == PJMEDIA_PORT_DISABLE) {
		    pj_add_timestamp32(&listener->ts_tx, 
				       listener->info->samples_per_frame);
		    listener->tx_level = 0;
		    continue;
		}
    	    
		status = write_frame(listener, f);
		if (status != PJ_SUCCESS) {
		    listener->tx_level = 0;
		    continue;
		}

		/* Set listener TX level based on transmitter RX level & 
		 * listener TX level.
		 */
		listener->tx_level = (cport->rx_level * listener->tx_adj_level)
				     >> 8;
	    }
	}
    }

    /* Keep alive. Update TX timestamp and send frame type NONE to all 
     * underflow ports at their own clock.
     */
    for (i=1, ci=1; i<conf->max_ports && ci<conf->port_cnt; ++i) {
	struct conf_port *cport = conf->ports[i];

	/* Skip empty port. */
	if (!cport)
	    continue;

	/* Var "ci" is to count how many ports have been visited so far. */
	++ci;

	if (cport->tx_setting==PJMEDIA_PORT_MUTE || cport->transmitter_cnt==0)
	{
	    pjmedia_frame_ext *f;
	    
	    /* Clear left-over samples in tx_buffer, if any, so that it won't
	     * be transmitted next time we have audio signal.
	     */
	    f = (pjmedia_frame_ext*)cport->tx_buf;
	    f->base.type = PJMEDIA_FRAME_TYPE_NONE;
	    f->base.size = 0;
	    f->samples_cnt = 0;
	    f->subframe_cnt = 0;
	    
	    cport->tx_level = 0;

	    while (pj_cmp_timestamp(&cport->ts_clock, &cport->ts_tx) > 0)
	    {
		if (cport->tx_setting == PJMEDIA_PORT_ENABLE) {
		    pjmedia_frame tmp_f;

		    tmp_f.timestamp = cport->ts_tx;
		    tmp_f.type = PJMEDIA_FRAME_TYPE_NONE;
		    tmp_f.buf = NULL;
		    tmp_f.size = 0;

		    pjmedia_port_put_frame(cport->port, &tmp_f);
		    pj_add_timestamp32(&cport->ts_tx, cport->info->samples_per_frame);
		}
	    }
	}
    }

    /* Return sound playback frame. */
    do {
	struct conf_port *this_cport = conf->ports[this_port->port_data.ldata];
	pjmedia_frame *f_src = (pjmedia_frame*) this_cport->tx_buf;

	frame->type = f_src->type;

	if (f_src->type == PJMEDIA_FRAME_TYPE_EXTENDED) {
	    pjmedia_frame_ext *f_src_ = (pjmedia_frame_ext*)f_src;
	    pjmedia_frame_ext *f_dst = (pjmedia_frame_ext*)frame;
	    pjmedia_frame_ext_subframe *sf;
	    unsigned samples_per_subframe;
	    
	    if (f_src_->samples_cnt < this_cport->info->samples_per_frame) {
		f_dst->base.type = PJMEDIA_FRAME_TYPE_NONE;
		f_dst->samples_cnt = 0;
		f_dst->subframe_cnt = 0;
		break;
	    }

	    f_dst->samples_cnt = 0;
	    f_dst->subframe_cnt = 0;
	    i = 0;
	    samples_per_subframe = f_src_->samples_cnt / f_src_->subframe_cnt;


	    while (f_dst->samples_cnt < this_cport->info->samples_per_frame) {
		sf = pjmedia_frame_ext_get_subframe(f_src_, i++);
		pj_assert(sf);
		pjmedia_frame_ext_append_subframe(f_dst, sf->data, sf->bitlen,
						  samples_per_subframe);
	    }

	    /* Shift left TX buffer. */
	    pjmedia_frame_ext_pop_subframes(f_src_, i);

	} else if (f_src->type == PJMEDIA_FRAME_TYPE_AUDIO) {
	    if ((f_src->size>>1) < this_cport->info->samples_per_frame) {
		frame->type = PJMEDIA_FRAME_TYPE_NONE;
		frame->size = 0;
		break;
	    }

	    pjmedia_copy_samples((pj_int16_t*)frame->buf, 
				 (pj_int16_t*)f_src->buf, 
				 this_cport->info->samples_per_frame);
	    frame->size = this_cport->info->samples_per_frame << 1;

	    /* Shift left TX buffer. */
	    f_src->size -= frame->size;
	    if (f_src->size)
		pjmedia_move_samples((pj_int16_t*)f_src->buf,
				     (pj_int16_t*)f_src->buf + 
				     this_cport->info->samples_per_frame,
				     f_src->size >> 1);
	} else { /* PJMEDIA_FRAME_TYPE_NONE */
	    pjmedia_frame_ext *f_src_ = (pjmedia_frame_ext*)f_src;

	    /* Reset source/TX buffer */
	    f_src_->base.size = 0;
	    f_src_->samples_cnt = 0;
	    f_src_->subframe_cnt = 0;
	}
    } while (0);

    /* Unlock mutex */
    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}

/*
 * Recorder callback.
 */
static pj_status_t put_frame(pjmedia_port *this_port, 
			     const pjmedia_frame *f)
{
    pjmedia_conf *conf = (pjmedia_conf*) this_port->port_data.pdata;
    struct conf_port *cport;
    unsigned j;
    pj_int32_t level = 0;

    /* Lock mutex */
    pj_mutex_lock(conf->mutex);

    /* Get conf port of this port */
    cport = conf->ports[this_port->port_data.ldata];
    if (cport == NULL) {
	/* Unlock mutex */
	pj_mutex_unlock(conf->mutex);
	return PJ_SUCCESS;
    }

    pj_add_timestamp32(&cport->ts_rx, cport->info->samples_per_frame);
    
    /* Skip if this port is muted/disabled. */
    if (cport->rx_setting == PJMEDIA_PORT_DISABLE) {
	cport->rx_level = 0;
	/* Unlock mutex */
	pj_mutex_unlock(conf->mutex);
	return PJ_SUCCESS;
    }

    /* Skip if no port is listening to the microphone */
    if (cport->listener_cnt == 0) {
	cport->rx_level = 0;
	/* Unlock mutex */
	pj_mutex_unlock(conf->mutex);
	return PJ_SUCCESS;
    }

    /* Calculate & adjust RX level. */
    if (f->type == PJMEDIA_FRAME_TYPE_AUDIO) {
	if (cport->rx_adj_level != NORMAL_LEVEL) {
	    pj_int16_t *p = (pj_int16_t*)f->buf;
	    pj_int16_t *end;

	    end = p + (f->size >> 1);
	    while (p < end) {
		pj_int32_t itemp = *p;

		/* Adjust the level */
		itemp = (itemp * cport->rx_adj_level) >> 7;

		/* Clip the signal if it's too loud */
		if (itemp > MAX_LEVEL) itemp = MAX_LEVEL;
		else if (itemp < MIN_LEVEL) itemp = MIN_LEVEL;

		level += PJ_ABS(itemp);

		/* Put back in the buffer. */
		*p = (pj_int16_t)itemp;
		++p;
	    }
	    level /= (f->size >> 1);
	} else {
	    level = pjmedia_calc_avg_signal((const pj_int16_t*)f->buf,
					    f->size >> 1);
	}
    } else if (f->type == PJMEDIA_FRAME_TYPE_EXTENDED) {
	/* For extended frame, level is unknown, so we just set 
	 * it to NORMAL_LEVEL. 
	 */
	level = NORMAL_LEVEL;
    }

    cport->rx_level = pjmedia_linear2ulaw(level) ^ 0xff;

    /* Put the frame to all listeners. */
    for (j=0; j < cport->listener_cnt; ++j) 
    {
	struct conf_port *listener;
	pj_status_t status;

	listener = conf->ports[cport->listener_slots[j]];

	/* Skip if this listener doesn't want to receive audio */
	if (listener->tx_setting == PJMEDIA_PORT_DISABLE) {
	    pj_add_timestamp32(&listener->ts_tx, 
			       listener->info->samples_per_frame);
	    listener->tx_level = 0;
	    continue;
	}

	/* Skip loopback for now. */
	if (listener == cport) {
	    pj_add_timestamp32(&listener->ts_tx, 
			       listener->info->samples_per_frame);
	    listener->tx_level = 0;
	    continue;
	}
	    
	status = write_frame(listener, f);
	if (status != PJ_SUCCESS) {
	    listener->tx_level = 0;
	    continue;
	}

	/* Set listener TX level based on transmitter RX level & listener
	 * TX level.
	 */
	listener->tx_level = (cport->rx_level * listener->tx_adj_level) >> 8;
    }

    /* Unlock mutex */
    pj_mutex_unlock(conf->mutex);

    return PJ_SUCCESS;
}

#endif
