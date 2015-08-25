/* $Id: listener_udp.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include "turn.h"
#include <pj/compat/socket.h>

struct read_op
{
    pj_ioqueue_op_key_t	op_key;
    pj_turn_pkt		pkt;
};

struct udp_listener
{
    pj_turn_listener	     base;

    pj_ioqueue_key_t	    *key;
    unsigned		     read_cnt;
    struct read_op	    **read_op;	/* Array of read_op's	*/

    pj_turn_transport	     tp;	/* Transport instance */
};


static pj_status_t udp_destroy(pj_turn_listener *udp);
static void on_read_complete(pj_ioqueue_key_t *key, 
			     pj_ioqueue_op_key_t *op_key, 
			     pj_ssize_t bytes_read);

static pj_status_t udp_sendto(pj_turn_transport *tp,
			      const void *packet,
			      pj_size_t size,
			      unsigned flag,
			      const pj_sockaddr_t *addr,
			      int addr_len);
static void udp_add_ref(pj_turn_transport *tp,
			pj_turn_allocation *alloc);
static void udp_dec_ref(pj_turn_transport *tp,
			pj_turn_allocation *alloc);


/*
 * Create a new listener on the specified port.
 */
PJ_DEF(pj_status_t) pj_turn_listener_create_udp( pj_turn_srv *srv,
					        int af,
					        const pj_str_t *bound_addr,
					        unsigned port,
						unsigned concurrency_cnt,
						unsigned flags,
						pj_turn_listener **p_listener)
{
    pj_pool_t *pool;
    struct udp_listener *udp;
    pj_ioqueue_callback ioqueue_cb;
    unsigned i;
    pj_status_t status;

    /* Create structure */
    pool = pj_pool_create(srv->core.pf, "udp%p", 1000, 1000, NULL);
    udp = PJ_POOL_ZALLOC_T(pool, struct udp_listener);
    udp->base.pool = pool;
    udp->base.obj_name = pool->obj_name;
    udp->base.server = srv;
    udp->base.tp_type = PJ_TURN_TP_UDP;
    udp->base.sock = PJ_INVALID_SOCKET;
    udp->base.destroy = &udp_destroy;
    udp->read_cnt = concurrency_cnt;
    udp->base.flags = flags;

    udp->tp.obj_name = udp->base.obj_name;
    udp->tp.info = udp->base.info;
    udp->tp.listener = &udp->base;
    udp->tp.sendto = &udp_sendto;
    udp->tp.add_ref = &udp_add_ref;
    udp->tp.dec_ref = &udp_dec_ref;

    /* Create socket */
    status = pj_sock_socket(af, pj_SOCK_DGRAM(), 0, &udp->base.sock);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Init bind address */
    status = pj_sockaddr_init(af, &udp->base.addr, bound_addr, 
			      (pj_uint16_t)port);
    if (status != PJ_SUCCESS) 
	goto on_error;
    
    /* Create info */
    pj_ansi_strcpy(udp->base.info, "UDP:");
    pj_sockaddr_print(&udp->base.addr, udp->base.info+4, 
		      sizeof(udp->base.info)-4, 3);

    /* Bind socket */
    status = pj_sock_bind(udp->base.sock, &udp->base.addr, 
			  pj_sockaddr_get_len(&udp->base.addr));
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Register to ioqueue */
    pj_bzero(&ioqueue_cb, sizeof(ioqueue_cb));
    ioqueue_cb.on_read_complete = on_read_complete;
    status = pj_ioqueue_register_sock(pool, srv->core.ioqueue, udp->base.sock,
				      udp, &ioqueue_cb, &udp->key);

    /* Create op keys */
    udp->read_op = (struct read_op**)pj_pool_calloc(pool, concurrency_cnt, 
						    sizeof(struct read_op*));

    /* Create each read_op and kick off read operation */
    for (i=0; i<concurrency_cnt; ++i) {
	pj_pool_t *rpool = pj_pool_create(srv->core.pf, "rop%p", 
					  1000, 1000, NULL);

	udp->read_op[i] = PJ_POOL_ZALLOC_T(pool, struct read_op);
	udp->read_op[i]->pkt.pool = rpool;

	on_read_complete(udp->key, &udp->read_op[i]->op_key, 0);
    }

    /* Done */
    PJ_LOG(4,(udp->base.obj_name, "Listener %s created", udp->base.info));

    *p_listener = &udp->base;
    return PJ_SUCCESS;


on_error:
    udp_destroy(&udp->base);
    return status;
}


/*
 * Destroy listener.
 */
static pj_status_t udp_destroy(pj_turn_listener *listener)
{
    struct udp_listener *udp = (struct udp_listener *)listener;
    unsigned i;

    if (udp->key) {
	pj_ioqueue_unregister(udp->key);
	udp->key = NULL;
	udp->base.sock = PJ_INVALID_SOCKET;
    } else if (udp->base.sock != PJ_INVALID_SOCKET) {
	pj_sock_close(udp->base.sock);
	udp->base.sock = PJ_INVALID_SOCKET;
    }

    for (i=0; i<udp->read_cnt; ++i) {
	if (udp->read_op[i]->pkt.pool) {
	    pj_pool_t *rpool = udp->read_op[i]->pkt.pool;
	    udp->read_op[i]->pkt.pool = NULL;
	    pj_pool_release(rpool);
	}
    }

    if (udp->base.pool) {
	pj_pool_t *pool = udp->base.pool;

	PJ_LOG(4,(udp->base.obj_name, "Listener %s destroyed", 
		  udp->base.info));

	udp->base.pool = NULL;
	pj_pool_release(pool);
    }
    return PJ_SUCCESS;
}

/*
 * Callback to send packet.
 */
static pj_status_t udp_sendto(pj_turn_transport *tp,
			      const void *packet,
			      pj_size_t size,
			      unsigned flag,
			      const pj_sockaddr_t *addr,
			      int addr_len)
{
    pj_ssize_t len = size;
    return pj_sock_sendto(tp->listener->sock, packet, &len, flag, addr, addr_len);
}


static void udp_add_ref(pj_turn_transport *tp,
			pj_turn_allocation *alloc)
{
    /* Do nothing */
    PJ_UNUSED_ARG(tp);
    PJ_UNUSED_ARG(alloc);
}

static void udp_dec_ref(pj_turn_transport *tp,
			pj_turn_allocation *alloc)
{
    /* Do nothing */
    PJ_UNUSED_ARG(tp);
    PJ_UNUSED_ARG(alloc);
}


/*
 * Callback on received packet.
 */
static void on_read_complete(pj_ioqueue_key_t *key, 
			     pj_ioqueue_op_key_t *op_key, 
			     pj_ssize_t bytes_read)
{
    struct udp_listener *udp;
    struct read_op *read_op = (struct read_op*) op_key;
    pj_status_t status;

    udp = (struct udp_listener*) pj_ioqueue_get_user_data(key);

    do {
	pj_pool_t *rpool;

	/* Report to server */
	if (bytes_read > 0) {
	    read_op->pkt.len = bytes_read;
	    pj_gettimeofday(&read_op->pkt.rx_time);

	    pj_turn_srv_on_rx_pkt(udp->base.server, &read_op->pkt);
	}

	/* Reset pool */
	rpool = read_op->pkt.pool;
	pj_pool_reset(rpool);
	read_op->pkt.pool = rpool;
	read_op->pkt.transport = &udp->tp;
	read_op->pkt.src.tp_type = udp->base.tp_type;

	/* Read next packet */
	bytes_read = sizeof(read_op->pkt.pkt);
	read_op->pkt.src_addr_len = sizeof(read_op->pkt.src.clt_addr);
	pj_bzero(&read_op->pkt.src.clt_addr, sizeof(read_op->pkt.src.clt_addr));

	status = pj_ioqueue_recvfrom(udp->key, op_key,
				     read_op->pkt.pkt, &bytes_read, 0,
				     &read_op->pkt.src.clt_addr, 
				     &read_op->pkt.src_addr_len);

	if (status != PJ_EPENDING && status != PJ_SUCCESS)
	    bytes_read = -status;

    } while (status != PJ_EPENDING && status != PJ_ECANCELLED &&
	     status != PJ_STATUS_FROM_OS(PJ_BLOCKING_ERROR_VAL));
}

