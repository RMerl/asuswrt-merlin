/* $Id: listener_tcp.c 3553 2011-05-05 06:14:19Z nanang $ */
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

#if PJ_HAS_TCP

struct accept_op
{
    pj_ioqueue_op_key_t	op_key;
    pj_sock_t		sock;
    pj_sockaddr		src_addr;
    int			src_addr_len;
};

struct tcp_listener
{
    pj_turn_listener	     base;
    pj_ioqueue_key_t	    *key;
    unsigned		     accept_cnt;
    struct accept_op	    *accept_op;	/* Array of accept_op's	*/
};


static void lis_on_accept_complete(pj_ioqueue_key_t *key, 
				   pj_ioqueue_op_key_t *op_key, 
				   pj_sock_t sock, 
				   pj_status_t status);
static pj_status_t lis_destroy(pj_turn_listener *listener);
static void transport_create(pj_sock_t sock, pj_turn_listener *lis,
			     pj_sockaddr_t *src_addr, int src_addr_len);

static void show_err(const char *sender, const char *title, 
		     pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));
    PJ_LOG(4,(sender, "%s: %s", title, errmsg));
}


/*
 * Create a new listener on the specified port.
 */
PJ_DEF(pj_status_t) pj_turn_listener_create_tcp(pj_turn_srv *srv,
					        int af,
					        const pj_str_t *bound_addr,
					        unsigned port,
						unsigned concurrency_cnt,
						unsigned flags,
						pj_turn_listener **p_listener)
{
    pj_pool_t *pool;
    struct tcp_listener *tcp_lis;
    pj_ioqueue_callback ioqueue_cb;
    unsigned i;
    pj_status_t status;

    /* Create structure */
    pool = pj_pool_create(srv->core.pf, "tcpl%p", 1000, 1000, NULL);
    tcp_lis = PJ_POOL_ZALLOC_T(pool, struct tcp_listener);
    tcp_lis->base.pool = pool;
    tcp_lis->base.obj_name = pool->obj_name;
    tcp_lis->base.server = srv;
    tcp_lis->base.tp_type = PJ_TURN_TP_TCP;
    tcp_lis->base.sock = PJ_INVALID_SOCKET;
    //tcp_lis->base.sendto = &tcp_sendto;
    tcp_lis->base.destroy = &lis_destroy;
    tcp_lis->accept_cnt = concurrency_cnt;
    tcp_lis->base.flags = flags;

    /* Create socket */
    status = pj_sock_socket(af, pj_SOCK_STREAM(), 0, &tcp_lis->base.sock);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Init bind address */
    status = pj_sockaddr_init(af, &tcp_lis->base.addr, bound_addr, 
			      (pj_uint16_t)port);
    if (status != PJ_SUCCESS) 
	goto on_error;
    
    /* Create info */
    pj_ansi_strcpy(tcp_lis->base.info, "TCP:");
    pj_sockaddr_print(&tcp_lis->base.addr, tcp_lis->base.info+4, 
		      sizeof(tcp_lis->base.info)-4, 3);

    /* Bind socket */
    status = pj_sock_bind(tcp_lis->base.sock, &tcp_lis->base.addr, 
			  pj_sockaddr_get_len(&tcp_lis->base.addr));
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Listen() */
    status = pj_sock_listen(tcp_lis->base.sock, 5);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Register to ioqueue */
    pj_bzero(&ioqueue_cb, sizeof(ioqueue_cb));
    ioqueue_cb.on_accept_complete = &lis_on_accept_complete;
    status = pj_ioqueue_register_sock(pool, srv->core.ioqueue, tcp_lis->base.sock,
				      tcp_lis, &ioqueue_cb, &tcp_lis->key);

    /* Create op keys */
    tcp_lis->accept_op = (struct accept_op*)pj_pool_calloc(pool, concurrency_cnt,
						    sizeof(struct accept_op));

    /* Create each accept_op and kick off read operation */
    for (i=0; i<concurrency_cnt; ++i) {
	lis_on_accept_complete(tcp_lis->key, &tcp_lis->accept_op[i].op_key, 
			       PJ_INVALID_SOCKET, PJ_EPENDING);
    }

    /* Done */
    PJ_LOG(4,(tcp_lis->base.obj_name, "Listener %s created", 
	   tcp_lis->base.info));

    *p_listener = &tcp_lis->base;
    return PJ_SUCCESS;


on_error:
    lis_destroy(&tcp_lis->base);
    return status;
}


/*
 * Destroy listener.
 */
static pj_status_t lis_destroy(pj_turn_listener *listener)
{
    struct tcp_listener *tcp_lis = (struct tcp_listener *)listener;
    unsigned i;

    if (tcp_lis->key) {
	pj_ioqueue_unregister(tcp_lis->key);
	tcp_lis->key = NULL;
	tcp_lis->base.sock = PJ_INVALID_SOCKET;
    } else if (tcp_lis->base.sock != PJ_INVALID_SOCKET) {
	pj_sock_close(tcp_lis->base.sock);
	tcp_lis->base.sock = PJ_INVALID_SOCKET;
    }

    for (i=0; i<tcp_lis->accept_cnt; ++i) {
	/* Nothing to do */
    }

    if (tcp_lis->base.pool) {
	pj_pool_t *pool = tcp_lis->base.pool;

	PJ_LOG(4,(tcp_lis->base.obj_name, "Listener %s destroyed", 
		  tcp_lis->base.info));

	tcp_lis->base.pool = NULL;
	pj_pool_release(pool);
    }
    return PJ_SUCCESS;
}


/*
 * Callback on new TCP connection.
 */
static void lis_on_accept_complete(pj_ioqueue_key_t *key, 
				   pj_ioqueue_op_key_t *op_key, 
				   pj_sock_t sock, 
				   pj_status_t status)
{
    struct tcp_listener *tcp_lis;
    struct accept_op *accept_op = (struct accept_op*) op_key;

    tcp_lis = (struct tcp_listener*) pj_ioqueue_get_user_data(key);

    PJ_UNUSED_ARG(sock);

    do {
	/* Report new connection. */
	if (status == PJ_SUCCESS) {
	    char addr[PJ_INET6_ADDRSTRLEN+8];
	    PJ_LOG(5,(tcp_lis->base.obj_name, "Incoming TCP from %s",
		      pj_sockaddr_print(&accept_op->src_addr, addr,
					sizeof(addr), 3)));
	    transport_create(accept_op->sock, &tcp_lis->base,
			     &accept_op->src_addr, accept_op->src_addr_len);
	} else if (status != PJ_EPENDING) {
	    show_err(tcp_lis->base.obj_name, "accept()", status);
	}

	/* Prepare next accept() */
	accept_op->src_addr_len = sizeof(accept_op->src_addr);
	status = pj_ioqueue_accept(key, op_key, &accept_op->sock,
				   NULL,
				   &accept_op->src_addr,
				   &accept_op->src_addr_len);

    } while (status != PJ_EPENDING && status != PJ_ECANCELLED &&
	     status != PJ_STATUS_FROM_OS(PJ_BLOCKING_ERROR_VAL));
}


/****************************************************************************/
/*
 * Transport
 */
enum
{
    TIMER_NONE,
    TIMER_DESTROY
};

/* The delay in seconds to be applied before TCP transport is destroyed when 
 * no allocation is referencing it. This also means the initial time to wait
 * after the initial TCP connection establishment to receive a valid STUN
 * message in the transport.
 */
#define SHUTDOWN_DELAY  10

struct recv_op
{
    pj_ioqueue_op_key_t	op_key;
    pj_turn_pkt		pkt;
};

struct tcp_transport
{
    pj_turn_transport	 base;
    pj_pool_t		*pool;
    pj_timer_entry	 timer;

    pj_turn_allocation	*alloc;
    int			 ref_cnt;

    pj_sock_t		 sock;
    pj_ioqueue_key_t	*key;
    struct recv_op	 recv_op;
    pj_ioqueue_op_key_t	 send_op;
};


static void tcp_on_read_complete(pj_ioqueue_key_t *key, 
				 pj_ioqueue_op_key_t *op_key, 
				 pj_ssize_t bytes_read);

static pj_status_t tcp_sendto(pj_turn_transport *tp,
			      const void *packet,
			      pj_size_t size,
			      unsigned flag,
			      const pj_sockaddr_t *addr,
			      int addr_len);
static void tcp_destroy(struct tcp_transport *tcp);
static void tcp_add_ref(pj_turn_transport *tp,
			pj_turn_allocation *alloc);
static void tcp_dec_ref(pj_turn_transport *tp,
			pj_turn_allocation *alloc);
static void timer_callback(pj_timer_heap_t *timer_heap,
			   pj_timer_entry *entry);

static void transport_create(pj_sock_t sock, pj_turn_listener *lis,
			     pj_sockaddr_t *src_addr, int src_addr_len)
{
    pj_pool_t *pool;
    struct tcp_transport *tcp;
    pj_ioqueue_callback cb;
    pj_status_t status;

    pool = pj_pool_create(lis->server->core.pf, "tcp%p", 1000, 1000, NULL);

    tcp = PJ_POOL_ZALLOC_T(pool, struct tcp_transport);
    tcp->base.obj_name = pool->obj_name;
    tcp->base.listener = lis;
    tcp->base.info = lis->info;
    tcp->base.sendto = &tcp_sendto;
    tcp->base.add_ref = &tcp_add_ref;
    tcp->base.dec_ref = &tcp_dec_ref;
    tcp->pool = pool;
    tcp->sock = sock;

    pj_timer_entry_init(&tcp->timer, TIMER_NONE, tcp, &timer_callback);

    /* Register to ioqueue */
    pj_bzero(&cb, sizeof(cb));
    cb.on_read_complete = &tcp_on_read_complete;
    status = pj_ioqueue_register_sock(pool, lis->server->core.ioqueue, sock,
				      tcp, &cb, &tcp->key);
    if (status != PJ_SUCCESS) {
	tcp_destroy(tcp);
	return;
    }

    /* Init pkt */
    tcp->recv_op.pkt.pool = pj_pool_create(lis->server->core.pf, "tcpkt%p", 
					   1000, 1000, NULL);
    tcp->recv_op.pkt.transport = &tcp->base;
    tcp->recv_op.pkt.src.tp_type = PJ_TURN_TP_TCP;
    tcp->recv_op.pkt.src_addr_len = src_addr_len;
    pj_memcpy(&tcp->recv_op.pkt.src.clt_addr, src_addr, src_addr_len);

    tcp_on_read_complete(tcp->key, &tcp->recv_op.op_key, -PJ_EPENDING);
    /* Should not access transport from now, it may have been destroyed */
}


static void tcp_destroy(struct tcp_transport *tcp)
{
    if (tcp->key) {
	pj_ioqueue_unregister(tcp->key);
	tcp->key = NULL;
	tcp->sock = 0;
    } else if (tcp->sock) {
	pj_sock_close(tcp->sock);
	tcp->sock = 0;
    }

    if (tcp->pool) {
	pj_pool_release(tcp->pool);
    }
}


static void timer_callback(pj_timer_heap_t *timer_heap,
			   pj_timer_entry *entry)
{
    struct tcp_transport *tcp = (struct tcp_transport*) entry->user_data;

    PJ_UNUSED_ARG(timer_heap);

    tcp_destroy(tcp);
}


static void tcp_on_read_complete(pj_ioqueue_key_t *key, 
				 pj_ioqueue_op_key_t *op_key, 
				 pj_ssize_t bytes_read)
{
    struct tcp_transport *tcp;
    struct recv_op *recv_op = (struct recv_op*) op_key;
    pj_status_t status;

    tcp = (struct tcp_transport*) pj_ioqueue_get_user_data(key);

    do {
	/* Report to server or allocation, if we have allocation */
	if (bytes_read > 0) {

	    recv_op->pkt.len = bytes_read;
	    pj_gettimeofday(&recv_op->pkt.rx_time);

	    tcp_add_ref(&tcp->base, NULL);

	    if (tcp->alloc) {
		pj_turn_allocation_on_rx_client_pkt(tcp->alloc, &recv_op->pkt);
	    } else {
		pj_turn_srv_on_rx_pkt(tcp->base.listener->server, &recv_op->pkt);
	    }

	    pj_assert(tcp->ref_cnt > 0);
	    tcp_dec_ref(&tcp->base, NULL);

	} else if (bytes_read != -PJ_EPENDING) {
	    /* TCP connection closed/error. Notify client and then destroy 
	     * ourselves.
	     * Note: the -PJ_EPENDING is the value passed during init.
	     */
	    ++tcp->ref_cnt;

	    if (tcp->alloc) {
		if (bytes_read != 0) {
		    show_err(tcp->base.obj_name, "TCP socket error", 
			     -bytes_read);
		} else {
		    PJ_LOG(5,(tcp->base.obj_name, "TCP socket closed"));
		}
		pj_turn_allocation_on_transport_closed(tcp->alloc, &tcp->base);
		tcp->alloc = NULL;
	    }

	    pj_assert(tcp->ref_cnt > 0);
	    if (--tcp->ref_cnt == 0) {
		tcp_destroy(tcp);
		return;
	    }
	}

	/* Reset pool */
	pj_pool_reset(recv_op->pkt.pool);

	/* If packet is full discard it */
	if (recv_op->pkt.len == sizeof(recv_op->pkt.pkt)) {
	    PJ_LOG(4,(tcp->base.obj_name, "Buffer discarded"));
	    recv_op->pkt.len = 0;
	}

	/* Read next packet */
	bytes_read = sizeof(recv_op->pkt.pkt) - recv_op->pkt.len;
	status = pj_ioqueue_recv(tcp->key, op_key,
				 recv_op->pkt.pkt + recv_op->pkt.len, 
				 &bytes_read, 0);

	if (status != PJ_EPENDING && status != PJ_SUCCESS)
	    bytes_read = -status;

    } while (status != PJ_EPENDING && status != PJ_ECANCELLED &&
	     status != PJ_STATUS_FROM_OS(PJ_BLOCKING_ERROR_VAL));

}


static pj_status_t tcp_sendto(pj_turn_transport *tp,
			      const void *packet,
			      pj_size_t size,
			      unsigned flag,
			      const pj_sockaddr_t *addr,
			      int addr_len)
{
    struct tcp_transport *tcp = (struct tcp_transport*) tp;
    pj_ssize_t length = size;

    PJ_UNUSED_ARG(addr);
    PJ_UNUSED_ARG(addr_len);

    return pj_ioqueue_send(tcp->key, &tcp->send_op, packet, &length, flag);
}


static void tcp_add_ref(pj_turn_transport *tp,
			pj_turn_allocation *alloc)
{
    struct tcp_transport *tcp = (struct tcp_transport*) tp;

    ++tcp->ref_cnt;

    if (tcp->alloc == NULL && alloc) {
	tcp->alloc = alloc;
    }

    /* Cancel shutdown timer if it's running */
    if (tcp->timer.id != TIMER_NONE) {
	pj_timer_heap_cancel(tcp->base.listener->server->core.timer_heap,
			     &tcp->timer);
	tcp->timer.id = TIMER_NONE;
    }
}


static void tcp_dec_ref(pj_turn_transport *tp,
			pj_turn_allocation *alloc)
{
    struct tcp_transport *tcp = (struct tcp_transport*) tp;

    --tcp->ref_cnt;

    if (alloc && alloc == tcp->alloc) {
	tcp->alloc = NULL;
    }

    if (tcp->ref_cnt == 0 && tcp->timer.id == TIMER_NONE) {
	pj_time_val delay = { SHUTDOWN_DELAY, 0 };
	tcp->timer.id = TIMER_DESTROY;
	pj_timer_heap_schedule(tcp->base.listener->server->core.timer_heap,
			       &tcp->timer, &delay);
    }
}

#else	/* PJ_HAS_TCP */

/* To avoid empty translation unit warning */
int listener_tcp_dummy = 0;

#endif	/* PJ_HAS_TCP */

