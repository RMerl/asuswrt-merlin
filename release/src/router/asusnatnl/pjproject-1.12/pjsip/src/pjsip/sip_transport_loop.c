/* $Id: sip_transport_loop.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjsip/sip_transport_loop.h>
#include <pjsip/sip_endpoint.h>
#include <pjsip/sip_errno.h>
#include <pj/pool.h>
#include <pj/os.h>
#include <pj/string.h>
#include <pj/lock.h>
#include <pj/assert.h>
#include <pj/compat/socket.h>


#define ADDR_LOOP	"128.0.0.1"
#define ADDR_LOOP_DGRAM	"129.0.0.1"


/** This structure describes incoming packet. */
struct recv_list
{
    PJ_DECL_LIST_MEMBER(struct recv_list);
    pjsip_rx_data  rdata;
};

/** This structure is used to keep delayed send failure. */
struct send_list
{
    PJ_DECL_LIST_MEMBER(struct send_list);
    pj_time_val    sent_time;
    pj_ssize_t	   sent;
    pjsip_tx_data *tdata;
    void	  *token;
    void	 (*callback)(pjsip_transport*, void*, pj_ssize_t);
};

/** This structure describes the loop transport. */
struct loop_transport
{
    pjsip_transport	     base;
    pj_pool_t		    *pool;
    pj_thread_t		    *thread;
    pj_bool_t		     thread_quit_flag;
    pj_bool_t		     discard;
    int			     fail_mode;
    unsigned		     recv_delay;
    unsigned		     send_delay;
    struct recv_list	     recv_list;
    struct send_list	     send_list;
};


/* Helper function to create "incoming" packet */
struct recv_list *create_incoming_packet( struct loop_transport *loop,
					  pjsip_tx_data *tdata )
{
    pj_pool_t *pool;
    struct recv_list *pkt;

    pool = pjsip_endpt_create_pool(loop->base.endpt, "rdata", 
				   PJSIP_POOL_RDATA_LEN, 
				   PJSIP_POOL_RDATA_INC+5);
    if (!pool)
	return NULL;

    pkt = PJ_POOL_ZALLOC_T(pool, struct recv_list);

    /* Initialize rdata. */
    pkt->rdata.tp_info.pool = pool;
    pkt->rdata.tp_info.transport = &loop->base;
    
    /* Copy the packet. */
    pj_memcpy(pkt->rdata.pkt_info.packet, tdata->buf.start,
	      tdata->buf.cur - tdata->buf.start);
    pkt->rdata.pkt_info.len = tdata->buf.cur - tdata->buf.start;

    /* the source address */
    pkt->rdata.pkt_info.src_addr.addr.sa_family = pj_AF_INET();

    /* "Source address" info. */
    pkt->rdata.pkt_info.src_addr_len = sizeof(pj_sockaddr_in);
    if (loop->base.key.type == PJSIP_TRANSPORT_LOOP) {
	pj_ansi_strcpy(pkt->rdata.pkt_info.src_name, ADDR_LOOP);
    } else {
	pj_ansi_strcpy(pkt->rdata.pkt_info.src_name, ADDR_LOOP_DGRAM);
    }
    pkt->rdata.pkt_info.src_port = loop->base.local_name.port;

    /* When do we need to "deliver" this packet. */
    pj_gettimeofday(&pkt->rdata.pkt_info.timestamp);
    pkt->rdata.pkt_info.timestamp.msec += loop->recv_delay;
    pj_time_val_normalize(&pkt->rdata.pkt_info.timestamp);

    /* Done. */

    return pkt;
}


/* Helper function to add pending notification callback. */
static pj_status_t add_notification( struct loop_transport *loop,
				     pjsip_tx_data *tdata,
				     pj_ssize_t sent,
				     void *token,
				     void (*callback)(pjsip_transport*, 
						      void*, pj_ssize_t))
{
    struct send_list *sent_status;

    pjsip_tx_data_add_ref(tdata);
    pj_lock_acquire(tdata->lock);
    sent_status = PJ_POOL_ALLOC_T(tdata->pool, struct send_list);
    pj_lock_release(tdata->lock);

    sent_status->sent = sent;
    sent_status->tdata = tdata;
    sent_status->token = token;
    sent_status->callback = callback;

    pj_gettimeofday(&sent_status->sent_time);
    sent_status->sent_time.msec += loop->send_delay;
    pj_time_val_normalize(&sent_status->sent_time);

    pj_lock_acquire(loop->base.lock);
    pj_list_push_back(&loop->send_list, sent_status);
    pj_lock_release(loop->base.lock);

    return PJ_SUCCESS;
}

/* Handler for sending outgoing message; called by transport manager. */
static pj_status_t loop_send_msg( pjsip_transport *tp, 
				  pjsip_tx_data *tdata,
				  const pj_sockaddr_t *rem_addr,
				  int addr_len,
				  void *token,
				  pjsip_transport_callback cb)
{
    struct loop_transport *loop = (struct loop_transport*)tp;
    struct recv_list *recv_pkt;
    
    PJ_ASSERT_RETURN(tp && (tp->key.type == PJSIP_TRANSPORT_LOOP ||
	             tp->key.type == PJSIP_TRANSPORT_LOOP_DGRAM), PJ_EINVAL);

    PJ_UNUSED_ARG(rem_addr);
    PJ_UNUSED_ARG(addr_len);


    /* Need to send failure? */
    if (loop->fail_mode) {
	if (loop->send_delay == 0) {
	    return PJ_STATUS_FROM_OS(OSERR_ECONNRESET);
	} else {
	    add_notification(loop, tdata, -PJ_STATUS_FROM_OS(OSERR_ECONNRESET),
			     token, cb);

	    return PJ_EPENDING;
	}
    }

    /* Discard any packets? */
    if (loop->discard)
	return PJ_SUCCESS;

    /* Create rdata for the "incoming" packet. */
    recv_pkt = create_incoming_packet(loop, tdata);
    if (!recv_pkt)
	return PJ_ENOMEM;

    /* If delay is not configured, deliver this packet now! */
    if (loop->recv_delay == 0) {
	pj_ssize_t size_eaten;

	size_eaten = pjsip_tpmgr_receive_packet( loop->base.tpmgr, 
						 &recv_pkt->rdata);
	pj_assert(size_eaten == recv_pkt->rdata.pkt_info.len);

	pjsip_endpt_release_pool(loop->base.endpt, 
				 recv_pkt->rdata.tp_info.pool);

    } else {
	/* Otherwise if delay is configured, add the "packet" to the 
	 * receive list to be processed by worker thread.
	 */
	pj_lock_acquire(loop->base.lock);
	pj_list_push_back(&loop->recv_list, recv_pkt);
	pj_lock_release(loop->base.lock);
    }

    if (loop->send_delay != 0) {
	add_notification(loop, tdata, tdata->buf.cur - tdata->buf.start,
			 token, cb);
	return PJ_EPENDING;
    } else {
	return PJ_SUCCESS;
    }
}

/* Handler to destroy the transport; called by transport manager */
static pj_status_t loop_destroy(pjsip_transport *tp)
{
    struct loop_transport *loop = (struct loop_transport*)tp;
    
    PJ_ASSERT_RETURN(tp && (tp->key.type == PJSIP_TRANSPORT_LOOP ||
	             tp->key.type == PJSIP_TRANSPORT_LOOP_DGRAM), PJ_EINVAL);
    
    loop->thread_quit_flag = 1;
    /* Unlock transport mutex before joining thread. */
    pj_lock_release(tp->lock);
    pj_thread_join(loop->thread);
    pj_thread_destroy(loop->thread);

    /* Clear pending send notifications. */
    while (!pj_list_empty(&loop->send_list)) {
	struct send_list *node = loop->send_list.next;
	/* Notify callback. */
	if (node->callback) {
	    (*node->callback)(&loop->base, node->token, -PJSIP_ESHUTDOWN);
	}
	pj_list_erase(node);
	pjsip_tx_data_dec_ref(node->tdata);
    }

    /* Clear "incoming" packets in the queue. */
    while (!pj_list_empty(&loop->recv_list)) {
	struct recv_list *node = loop->recv_list.next;
	pj_list_erase(node);
	pjsip_endpt_release_pool(loop->base.endpt,
				 node->rdata.tp_info.pool);
    }

    /* Self destruct.. heheh.. */
    pj_lock_destroy(loop->base.lock);
    pj_atomic_destroy(loop->base.ref_cnt);
    pjsip_endpt_release_pool(loop->base.endpt, loop->base.pool);

    return PJ_SUCCESS;
}

/* Worker thread for loop transport. */
static int loop_transport_worker_thread(void *arg)
{
    struct loop_transport *loop = (struct loop_transport*) arg;
    struct recv_list r;
    struct send_list s;

    pj_list_init(&r);
    pj_list_init(&s);

    while (!loop->thread_quit_flag) {
	pj_time_val now;

	pj_thread_sleep(1);
	pj_gettimeofday(&now);

	pj_lock_acquire(loop->base.lock);

	/* Move expired send notification to local list. */
	while (!pj_list_empty(&loop->send_list)) {
	    struct send_list *node = loop->send_list.next;

	    /* Break when next node time is greater than now. */
	    if (PJ_TIME_VAL_GTE(node->sent_time, now))
		break;

	    /* Delete this from the list. */
	    pj_list_erase(node);

	    /* Add to local list. */
	    pj_list_push_back(&s, node);
	}

	/* Move expired "incoming" packet to local list. */
	while (!pj_list_empty(&loop->recv_list)) {
	    struct recv_list *node = loop->recv_list.next;

	    /* Break when next node time is greater than now. */
	    if (PJ_TIME_VAL_GTE(node->rdata.pkt_info.timestamp, now))
		break;

	    /* Delete this from the list. */
	    pj_list_erase(node);

	    /* Add to local list. */
	    pj_list_push_back(&r, node);

	}

	pj_lock_release(loop->base.lock);

	/* Process send notification and incoming packet notification
	 * without holding down the loop's mutex.
	 */
	while (!pj_list_empty(&s)) {
	    struct send_list *node = s.next;

	    pj_list_erase(node);

	    /* Notify callback. */
	    if (node->callback) {
		(*node->callback)(&loop->base, node->token, node->sent);
	    }

	    /* Decrement tdata reference counter. */
	    pjsip_tx_data_dec_ref(node->tdata);
	}

	/* Process "incoming" packet. */
	while (!pj_list_empty(&r)) {
	    struct recv_list *node = r.next;
	    pj_ssize_t size_eaten;

	    pj_list_erase(node);

	    /* Notify transport manager about the "incoming packet" */
	    size_eaten = pjsip_tpmgr_receive_packet(loop->base.tpmgr,
						    &node->rdata);

	    /* Must "eat" all the packets. */
	    pj_assert(size_eaten == node->rdata.pkt_info.len);

	    /* Done. */
	    pjsip_endpt_release_pool(loop->base.endpt,
				     node->rdata.tp_info.pool);
	}
    }

    return 0;
}


/* Start loop transport. */
PJ_DEF(pj_status_t) pjsip_loop_start( pjsip_endpoint *endpt,
				      pjsip_transport **transport)
{
    pj_pool_t *pool;
    struct loop_transport *loop;
    pj_status_t status;

    /* Create pool. */
    pool = pjsip_endpt_create_pool(endpt, "loop", 4000, 4000);
    if (!pool)
	return PJ_ENOMEM;

    /* Create the loop structure. */
    loop = PJ_POOL_ZALLOC_T(pool, struct loop_transport);
    
    /* Initialize transport properties. */
    pj_ansi_snprintf(loop->base.obj_name, sizeof(loop->base.obj_name), 
		     "loop%p", loop);
    loop->base.pool = pool;
    status = pj_atomic_create(pool, 0, &loop->base.ref_cnt);
    if (status != PJ_SUCCESS)
	goto on_error;
    status = pj_lock_create_recursive_mutex(pool, "loop", &loop->base.lock);
    if (status != PJ_SUCCESS)
	goto on_error;
    loop->base.key.type = PJSIP_TRANSPORT_LOOP_DGRAM;
    //loop->base.key.rem_addr.sa_family = pj_AF_INET();
    loop->base.type_name = "LOOP-DGRAM";
    loop->base.info = "LOOP-DGRAM";
    loop->base.flag = PJSIP_TRANSPORT_DATAGRAM;
    loop->base.local_name.host = pj_str(ADDR_LOOP_DGRAM);
    loop->base.local_name.port = 
	pjsip_transport_get_default_port_for_type((pjsip_transport_type_e)
						  loop->base.key.type);
    loop->base.addr_len = sizeof(pj_sockaddr_in);
    loop->base.dir = PJSIP_TP_DIR_NONE;
    loop->base.endpt = endpt;
    loop->base.tpmgr = pjsip_endpt_get_tpmgr(endpt);
    loop->base.send_msg = &loop_send_msg;
    loop->base.destroy = &loop_destroy;

    pj_list_init(&loop->recv_list);
    pj_list_init(&loop->send_list);

    /* Create worker thread. */
    status = pj_thread_create(pool, "loop", 
			      &loop_transport_worker_thread, loop, 0, 
			      PJ_THREAD_SUSPENDED, &loop->thread);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Register to transport manager. */
    status = pjsip_transport_register( loop->base.tpmgr, &loop->base);
    if (status != PJ_SUCCESS)
	goto on_error;

    /* Start the thread. */
    status = pj_thread_resume(loop->thread);
    if (status != PJ_SUCCESS)
	goto on_error;

    /*
     * Done.
     */

    if (transport)
	*transport = &loop->base;

    return PJ_SUCCESS;

on_error:
    if (loop->base.lock)
	pj_lock_destroy(loop->base.lock);
    if (loop->thread)
	pj_thread_destroy(loop->thread);
    if (loop->base.ref_cnt)
	pj_atomic_destroy(loop->base.ref_cnt);
    pjsip_endpt_release_pool(endpt, loop->pool);
    return status;
}


PJ_DEF(pj_status_t) pjsip_loop_set_discard( pjsip_transport *tp,
					    pj_bool_t discard,
					    pj_bool_t *prev_value )
{
    struct loop_transport *loop = (struct loop_transport*)tp;

    PJ_ASSERT_RETURN(tp && (tp->key.type == PJSIP_TRANSPORT_LOOP ||
	             tp->key.type == PJSIP_TRANSPORT_LOOP_DGRAM), PJ_EINVAL);

    if (prev_value)
	*prev_value = loop->discard;
    loop->discard = discard;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_loop_set_failure( pjsip_transport *tp,
					    int fail_flag,
					    int *prev_value )
{
    struct loop_transport *loop = (struct loop_transport*)tp;

    PJ_ASSERT_RETURN(tp && (tp->key.type == PJSIP_TRANSPORT_LOOP ||
	             tp->key.type == PJSIP_TRANSPORT_LOOP_DGRAM), PJ_EINVAL);

    if (prev_value)
	*prev_value = loop->fail_mode;
    loop->fail_mode = fail_flag;

    return PJ_SUCCESS;
}


PJ_DEF(pj_status_t) pjsip_loop_set_recv_delay( pjsip_transport *tp,
					       unsigned delay,
					       unsigned *prev_value)
{
    struct loop_transport *loop = (struct loop_transport*)tp;

    PJ_ASSERT_RETURN(tp && (tp->key.type == PJSIP_TRANSPORT_LOOP ||
	             tp->key.type == PJSIP_TRANSPORT_LOOP_DGRAM), PJ_EINVAL);

    if (prev_value)
	*prev_value = loop->recv_delay;
    loop->recv_delay = delay;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsip_loop_set_send_callback_delay( pjsip_transport *tp,
							unsigned delay,
							unsigned *prev_value)
{
    struct loop_transport *loop = (struct loop_transport*)tp;

    PJ_ASSERT_RETURN(tp && (tp->key.type == PJSIP_TRANSPORT_LOOP ||
	             tp->key.type == PJSIP_TRANSPORT_LOOP_DGRAM), PJ_EINVAL);

    if (prev_value)
	*prev_value = loop->send_delay;
    loop->send_delay = delay;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjsip_loop_set_delay( pjsip_transport *tp, unsigned delay )
{
    struct loop_transport *loop = (struct loop_transport*)tp;

    PJ_ASSERT_RETURN(tp && (tp->key.type == PJSIP_TRANSPORT_LOOP ||
	             tp->key.type == PJSIP_TRANSPORT_LOOP_DGRAM), PJ_EINVAL);

    loop->recv_delay = delay;
    loop->send_delay = delay;

    return PJ_SUCCESS;
}

