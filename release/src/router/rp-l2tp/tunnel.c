/***********************************************************************
*
* tunnel.c
*
* Functions for manipulating L2TP tunnel objects.
*
* Copyright (C) 2002 Roaring Penguin Software Inc.
* Copyright (C) 2005-2007 Oleg I. Vdovikin (oleg@cs.msu.su)
*	Persist fixes, route manipulation
*
* This software may be distributed under the terms of the GNU General
* Public License, Version 2, or (at your option) any later version.
*
* LIC: GPL
*
***********************************************************************/

static char const RCSID[] =
"$Id: tunnel.c 3323 2011-09-21 18:45:48Z lly.dev $";

#include "l2tp.h"
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <features.h>
#include <resolv.h>

/* Hash tables of all tunnels */
static hash_table tunnels_by_my_id;
static hash_table tunnels_by_peer_address;

static uint16_t tunnel_make_tid(void);
static l2tp_tunnel *tunnel_new(EventSelector *es);
static void tunnel_free(l2tp_tunnel *tunnel);
static int tunnel_send_SCCRQ(l2tp_tunnel *tunnel);
static void tunnel_handle_SCCRQ(l2tp_dgram *dgram,
					 EventSelector *es,
					 struct sockaddr_in *from);
static void tunnel_handle_SCCRP(l2tp_tunnel *tunnel,
				l2tp_dgram *dgram);
static void tunnel_handle_SCCCN(l2tp_tunnel *tunnel,
				l2tp_dgram *dgram);
static void tunnel_handle_ICRQ(l2tp_tunnel *tunnel,
			       l2tp_dgram *dgram);
static void tunnel_process_received_datagram(l2tp_tunnel *tunnel,
					     l2tp_dgram *dgram);
static void tunnel_schedule_ack(l2tp_tunnel *tunnel);
static void tunnel_do_ack(EventSelector *es, int fd, unsigned int flags,
			  void *data);
static void tunnel_handle_timeout(EventSelector *es, int fd,
				  unsigned int flags, void *data);
static void tunnel_finally_cleanup(EventSelector *es, int fd,
				   unsigned int flags, void *data);

static void tunnel_do_hello(EventSelector *es, int fd,
			    unsigned int flags, void *data);

static void tunnel_dequeue_acked_packets(l2tp_tunnel *tunnel);

static void tunnel_send_StopCCN(l2tp_tunnel *tunnel,
				int result_code,
				int error_code,
				char const *fmt, ...);
static void tunnel_schedule_destruction(l2tp_tunnel *tunnel);
static int tunnel_set_params(l2tp_tunnel *tunnel, l2tp_dgram *dgram);

static int tunnel_outstanding_frames(l2tp_tunnel *tunnel);
static void tunnel_setup_hello(l2tp_tunnel *tunnel);
static void tunnel_tell_sessions_tunnel_open(l2tp_tunnel *tunnel);
static l2tp_tunnel *tunnel_establish(l2tp_peer *peer, EventSelector *es);
static l2tp_tunnel *tunnel_find_bypeer(struct sockaddr_in addr);

static char *state_names[] = {
    "idle", "wait-ctl-reply", "wait-ctl-conn", "established",
    "received-stop-ccn", "sent-stop-ccn"
};

#ifdef RTCONFIG_VPNC
int vpnc = 0;
#endif

#define VENDOR_STR "Roaring Penguin Software Inc."

/* Comparison of serial numbers according to RFC 1982 */
#define SERIAL_GT(a, b) \
(((a) > (b) && (a) - (b) < 32768) || ((a) < (b) && (b) - (a) > 32768))

#define SERIAL_LT(a, b) \
(((a) < (b) && (b) - (a) < 32768) || ((a) > (b) && (a) - (b) > 32768))

/* Route manipulation */
#define sin_addr(s) (((struct sockaddr_in *)(s))->sin_addr)
#define route_msg l2tp_set_errmsg
static int route_add(const struct in_addr inetaddr, struct rtentry *rt);
static int route_del(struct rtentry *rt);

/**********************************************************************
* %FUNCTION: tunnel_set_state
* %ARGUMENTS:
*  tunnel -- the tunnel
*  state -- new state
* %RETURNS:
*  Nothing
***********************************************************************/
static void
tunnel_set_state(l2tp_tunnel *tunnel, int state)
{
    if (state == tunnel->state) return;
    DBG(l2tp_db(DBG_TUNNEL, "tunnel(%s) state %s -> %s\n",
	   l2tp_debug_tunnel_to_str(tunnel),
	   state_names[tunnel->state],
	   state_names[state]));
    tunnel->state = state;
}

/**********************************************************************
* %FUNCTION: tunnel_find_by_my_id
* %ARGUMENTS:
*  id -- tunnel ID
* %RETURNS:
*  A tunnel whose my_id field equals id, or NULL if no such tunnel
***********************************************************************/
l2tp_tunnel *
l2tp_tunnel_find_by_my_id(uint16_t id)
{
    l2tp_tunnel candidate;
    candidate.my_id = id;
    return hash_find(&tunnels_by_my_id, &candidate);
}

/**********************************************************************
* %FUNCTION: tunnel_find_bypeer
* %ARGUMENTS:
*  addr -- peer address
* %RETURNS:
*  A tunnel whose peer_addr field equals addr, or NULL if no such tunnel
***********************************************************************/
static l2tp_tunnel *
tunnel_find_bypeer(struct sockaddr_in addr)
{
    l2tp_tunnel candidate;
    candidate.peer_addr = addr;
    return hash_find(&tunnels_by_peer_address, &candidate);
}

/**********************************************************************
* %FUNCTION: tunnel_flow_control_stats
* %ARGUMENTS:
*  tunnel -- a tunnel
* %RETURNS:
*  A string representing flow-control info (used for debugging)
***********************************************************************/
static char const *
tunnel_flow_control_stats(l2tp_tunnel *tunnel)
{
    static char buf[256];

    snprintf(buf, sizeof(buf), "rws=%d cwnd=%d ssthresh=%d outstanding=%d",
	     (int) tunnel->peer_rws,
	     tunnel->cwnd,
	     tunnel->ssthresh,
	     tunnel_outstanding_frames(tunnel));
    return buf;
}

/**********************************************************************
* %FUNCTION: tunnel_outstanding_frames
* %ARGUMENTS:
*  tunnel -- a tunnel
* %RETURNS:
*  The number of outstanding (transmitted, but not ack'd) frames.
***********************************************************************/
static int
tunnel_outstanding_frames(l2tp_tunnel *tunnel)
{
    int Ns = (int) tunnel->Ns_on_wire;
    int Nr = (int) tunnel->peer_Nr;
    if (Ns >= Nr) return Ns - Nr;
    return 65536 - Nr + Ns;
}

/**********************************************************************
* %FUNCTION: tunnel_make_tid
* %ARGUMENTS:
*  None
* %RETURNS:
*  An unused, random tunnel ID.
***********************************************************************/
static uint16_t tunnel_make_tid(void)
{
    uint16_t id;
    for(;;) {
	L2TP_RANDOM_FILL(id);
	if (!id) continue;
	if (!l2tp_tunnel_find_by_my_id(id)) return id;
    }
}

/**********************************************************************
* %FUNCTION: tunnel_enqueue_dgram
* %ARGUMENTS:
*  tunnel -- the tunnel
*  dgram -- an L2TP datagram to place at the tail of the transmit queue
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Adds datagram to end of transmit queue.
***********************************************************************/
static void
tunnel_enqueue_dgram(l2tp_tunnel *tunnel,
		     l2tp_dgram  *dgram)
{
    dgram->next = NULL;
    if (tunnel->xmit_queue_tail) {
	tunnel->xmit_queue_tail->next = dgram;
	tunnel->xmit_queue_tail = dgram;
    } else {
	tunnel->xmit_queue_head = dgram;
	tunnel->xmit_queue_tail = dgram;
    }
    if (!tunnel->xmit_new_dgrams) {
	tunnel->xmit_new_dgrams = dgram;
    }

    dgram->Ns = tunnel->Ns;
    tunnel->Ns++;
    DBG(l2tp_db(DBG_FLOW, "tunnel_enqueue_dgram(%s, %s) %s\n",
	   l2tp_debug_tunnel_to_str(tunnel),
	   l2tp_debug_message_type_to_str(dgram->msg_type),
	   tunnel_flow_control_stats(tunnel)));

}

/**********************************************************************
* %FUNCTION: tunnel_dequeue_head
* %ARGUMENTS:
*  tunnel -- the tunnel
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Dequeues datagram from head of transmit queue and frees it
***********************************************************************/
static void
tunnel_dequeue_head(l2tp_tunnel *tunnel)
{
    l2tp_dgram *dgram = tunnel->xmit_queue_head;
    if (dgram) {
	tunnel->xmit_queue_head = dgram->next;
	if (tunnel->xmit_new_dgrams == dgram) {
	    tunnel->xmit_new_dgrams = dgram->next;
	}
	if (tunnel->xmit_queue_tail == dgram) {
	    tunnel->xmit_queue_tail = NULL;
	}
	l2tp_dgram_free(dgram);
    }
}

/**********************************************************************
* %FUNCTION: tunnel_xmit_queued_messages
* %ARGUMENTS:
*  tunnel -- the tunnel
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Transmits as many control messages as possible from the queue.
***********************************************************************/
static void
tunnel_xmit_queued_messages(l2tp_tunnel *tunnel)
{
    l2tp_dgram *dgram;
    struct timeval t;

    dgram = tunnel->xmit_new_dgrams;
    if (!dgram) return;

    DBG(l2tp_db(DBG_FLOW, "xmit_queued(%s): %s\n",
	   l2tp_debug_tunnel_to_str(tunnel),
	   tunnel_flow_control_stats(tunnel)));
    while (dgram) {
	/* If window is closed, we can't transmit anything */
	if (tunnel_outstanding_frames(tunnel) >= tunnel->cwnd) {
	    break;
	}

	/* Update Nr */
	dgram->Nr = tunnel->Nr;

	/* Tid might have changed if call was initiated before
	   tunnel establishment was complete */
	dgram->tid = tunnel->assigned_id;

	if (l2tp_dgram_send_to_wire(dgram, &tunnel->peer_addr) < 0) {
	    break;
	}

	/* Cancel any pending ack */
	if (tunnel->ack_handler) {
	    Event_DelHandler(tunnel->es, tunnel->ack_handler);
	    tunnel->ack_handler = NULL;
	}

	tunnel->xmit_new_dgrams = dgram->next;
	tunnel->Ns_on_wire = dgram->Ns + 1;
	DBG(l2tp_db(DBG_FLOW, "loop in xmit_queued(%s): %s\n",
	       l2tp_debug_tunnel_to_str(tunnel),
	       tunnel_flow_control_stats(tunnel)));
	dgram = dgram->next;
    }

    t.tv_sec = tunnel->timeout;
    t.tv_usec = 0;

    /* Set retransmission timer */
    if (tunnel->timeout_handler) {
	Event_ChangeTimeout(tunnel->timeout_handler, t);
    } else {
	tunnel->timeout_handler =
	    Event_AddTimerHandler(tunnel->es, t,
				  tunnel_handle_timeout, tunnel);
    }
}

/**********************************************************************
* %FUNCTION: tunnel_xmit_control_message
* %ARGUMENTS:
*  tunnel -- the tunnel
*  dgram -- the datagram to transmit.  After return from this
*           function, the tunnel "owns" the dgram and the caller should
*           no longer use it.
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Transmits a control message.  If there is no room in the transmit
*  window, queues the message.
***********************************************************************/
void
l2tp_tunnel_xmit_control_message(l2tp_tunnel *tunnel,
				 l2tp_dgram  *dgram)
{
    /* Queue the datagram in case we need to retransmit it */
    tunnel_enqueue_dgram(tunnel, dgram);

    /* Run the queue */
    tunnel_xmit_queued_messages(tunnel);
}

static unsigned int
tunnel_compute_peerhash(void *data)
{
    return (unsigned int) (((l2tp_tunnel *)data)->peer_addr.sin_addr.s_addr);
}

static int
tunnel_compare_peer(void *d1, void *d2)
{
    return (((l2tp_tunnel *)d1)->peer_addr.sin_addr.s_addr) !=
	(((l2tp_tunnel *)d2)->peer_addr.sin_addr.s_addr);
}

/**********************************************************************
* %FUNCTION: tunnel_compute_hash_my_id
* %ARGUMENTS:
*  data -- a void pointer which is really a tunnel
* %RETURNS:
*  My tunnel ID
***********************************************************************/
static unsigned int
tunnel_compute_hash_my_id(void *data)
{
    return (unsigned int) (((l2tp_tunnel *) data)->my_id);
}

/**********************************************************************
* %FUNCTION: tunnel_compare_my_id
* %ARGUMENTS:
*  item1 -- first tunnel
*  item2 -- second tunnel
* %RETURNS:
*  0 if both tunnels have same ID, non-zero otherwise
***********************************************************************/
static int
tunnel_compare_my_id(void *item1, void *item2)
{
    return ((l2tp_tunnel *) item1)->my_id != ((l2tp_tunnel *) item2)->my_id;
}

/**********************************************************************
* %FUNCTION: tunnel_cleanup
* %ARGUMENTS:
*  data -- event selector
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Shuts down all tunnels and waits for them to exit
***********************************************************************/
static void
tunnel_cleanup(void *data)
{
    EventSelector *es = (EventSelector *) data;
    int i;

    /* Stop all tunnels */
    l2tp_tunnel_stop_all("Shutting down");

    while (hash_num_entries(&tunnels_by_my_id)) {
	i = Event_HandleEvent(es);
	if (i < 0) {
	    exit(EXIT_FAILURE);
	}
    }
}

/**********************************************************************
* %FUNCTION: tunnel_init
* %ARGUMENTS:
*  es -- event selector
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Initializes static tunnel data structures.
***********************************************************************/
void
l2tp_tunnel_init(EventSelector *es)
{
    hash_init(&tunnels_by_my_id,
	      offsetof(l2tp_tunnel, hash_by_my_id),
	      tunnel_compute_hash_my_id,
	      tunnel_compare_my_id);
    hash_init(&tunnels_by_peer_address,
	      offsetof(l2tp_tunnel, hash_by_peer),
	      tunnel_compute_peerhash,
	      tunnel_compare_peer);

    l2tp_register_shutdown_handler(tunnel_cleanup, es);
}

/**********************************************************************
* %FUNCTION: tunnel_new
* %ARGUMENTS:
*  es -- event selector
* %RETURNS:
*  A newly-allocated and initialized l2tp_tunnel object
***********************************************************************/
static l2tp_tunnel *
tunnel_new(EventSelector *es)
{
    l2tp_tunnel *tunnel = malloc(sizeof(l2tp_tunnel));
    if (!tunnel) return NULL;

    memset(tunnel, 0, sizeof(l2tp_tunnel));
    l2tp_session_hash_init(&tunnel->sessions_by_my_id);
    tunnel->rws = 8;
    tunnel->peer_rws = 1;
    tunnel->es = es;
    tunnel->timeout = 1;
    tunnel->my_id = tunnel_make_tid();
    tunnel->ssthresh = 1;
    tunnel->cwnd = 1;
    tunnel->private = NULL;

    hash_insert(&tunnels_by_my_id, tunnel);
    DBG(l2tp_db(DBG_TUNNEL, "tunnel_new() -> %s\n", l2tp_debug_tunnel_to_str(tunnel)));
    return tunnel;
}

/**********************************************************************
* %FUNCTION: tunnel_free
* %ARGUMENTS:
*  tunnel -- tunnel to free
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Frees all memory used by tunnel
***********************************************************************/
static void
tunnel_free(l2tp_tunnel *tunnel)
{
    void *cursor;
    l2tp_session *ses;
    EventSelector *es = tunnel->es;

    DBG(l2tp_db(DBG_TUNNEL, "tunnel_free(%s)\n", l2tp_debug_tunnel_to_str(tunnel)));

    hash_remove(&tunnels_by_my_id, tunnel);
    hash_remove(&tunnels_by_peer_address, tunnel);

    for (ses = hash_start(&tunnel->sessions_by_my_id, &cursor);
	 ses ;
	 ses = hash_next(&tunnel->sessions_by_my_id, &cursor)) {
	l2tp_session_free(ses, "Tunnel closing down", 1);
    }
    if (tunnel->hello_handler) Event_DelHandler(es, tunnel->hello_handler);
    if (tunnel->timeout_handler) Event_DelHandler(es, tunnel->timeout_handler);
    if (tunnel->ack_handler) Event_DelHandler(es, tunnel->ack_handler);

    while(tunnel->xmit_queue_head) {
	tunnel_dequeue_head(tunnel);
    }

    DBG(l2tp_db(DBG_TUNNEL, "tunnel_close(%s) ops: %p\n",
	l2tp_debug_tunnel_to_str(tunnel), tunnel->call_ops));
    if (tunnel->call_ops && tunnel->call_ops->tunnel_close) {
	tunnel->call_ops->tunnel_close(tunnel);
    }

    route_del(&tunnel->rt);
    memset(tunnel, 0, sizeof(l2tp_tunnel));
    free(tunnel);
}

/**********************************************************************
* %FUNCTION: tunnel_establish
* %ARGUMENTS:
*  peer -- peer with which to establish tunnel
*  es -- event selector for event loop
* %RETURNS:
*  A newly-allocated tunnel, or NULL on error.
* %DESCRIPTION:
*  Begins tunnel establishment to peer.
***********************************************************************/
static l2tp_tunnel *
tunnel_establish(l2tp_peer *peer, EventSelector *es)
{
    l2tp_tunnel *tunnel;
    struct sockaddr_in peer_addr = peer->addr;
    struct hostent *he;

    /* check peer_addr and resolv it based on the peername if needed */
    if (peer_addr.sin_addr.s_addr == INADDR_ANY) {
#if !defined(__UCLIBC__) \
 || (__UCLIBC_MAJOR__ == 0 \
 && (__UCLIBC_MINOR__ < 9 || (__UCLIBC_MINOR__ == 9 && __UCLIBC_SUBLEVEL__ < 31)))
	/* force ns refresh from resolv.conf with uClibc pre-0.9.31 */
	res_init();
#endif
	he = gethostbyname(peer->peername);
	if (!he) {
            l2tp_set_errmsg("tunnel_establish: gethostbyname failed for '%s'", peer->peername);
	    if (peer->persist && (peer->maxfail == 0 || peer->fail++ < peer->maxfail)) 
	    {
		struct timeval t;

		t.tv_sec = peer->holdoff;
		t.tv_usec = 0;
		Event_AddTimerHandler(es, t, l2tp_tunnel_reestablish, peer);
	    }
	    return NULL;
	}
	memcpy(&peer_addr.sin_addr, he->h_addr, sizeof(peer_addr.sin_addr));
    }
    
    tunnel = tunnel_new(es);
    if (!tunnel) return NULL;

    tunnel->peer = peer;
    tunnel->peer_addr = peer_addr;
    tunnel->call_ops = tunnel->peer->lac_ops;

    DBG(l2tp_db(DBG_TUNNEL, "tunnel_establish(%s) -> %s (%s)\n",
	    l2tp_debug_tunnel_to_str(tunnel),
	    inet_ntoa(peer_addr.sin_addr), peer->peername));

    memset(&tunnel->rt, 0, sizeof(tunnel->rt));
    route_add(tunnel->peer_addr.sin_addr, &tunnel->rt);

    hash_insert(&tunnels_by_peer_address, tunnel);
    tunnel_send_SCCRQ(tunnel);
    tunnel_set_state(tunnel, TUNNEL_WAIT_CTL_REPLY);
    return tunnel;
}

/**********************************************************************
* %FUNCTION: tunnel_send_SCCRQ
* %ARGUMENTS:
*  tunnel -- the tunnel we wish to establish
* %RETURNS:
*  0 if we handed the datagram off to control transport, -1 otherwise.
* %DESCRIPTION:
*  Sends SCCRQ to establish a tunnel.
***********************************************************************/
static int
tunnel_send_SCCRQ(l2tp_tunnel *tunnel)
{
    uint16_t u16;
    uint32_t u32;
    unsigned char tie_breaker[8];
    unsigned char challenge[16];
    int old_hide;
    char *hostname;

    l2tp_dgram *dgram = l2tp_dgram_new_control(MESSAGE_SCCRQ, 0, 0);
    if (!dgram) return -1;

    /* Add the AVP's */
    /* HACK!  Cisco equipment cannot handle hidden AVP's in SCCRQ.
       So we temporarily disable AVP hiding */
    old_hide = tunnel->peer->hide_avps;
    tunnel->peer->hide_avps = 0;

    /* Protocol version */
    u16 = htons(0x0100);
    l2tp_dgram_add_avp(dgram, tunnel, MANDATORY,
		  sizeof(u16), VENDOR_IETF, AVP_PROTOCOL_VERSION, &u16);

    /* Framing capabilities -- hard-coded as sync and async */
    u32 = htonl(3);
    l2tp_dgram_add_avp(dgram, tunnel, MANDATORY,
		  sizeof(u32), VENDOR_IETF, AVP_FRAMING_CAPABILITIES, &u32);

    hostname = tunnel->peer->hostname[0] ? tunnel->peer->hostname : Hostname;

    /* Host name */
    l2tp_dgram_add_avp(dgram, tunnel, MANDATORY,
		  strlen(hostname), VENDOR_IETF, AVP_HOST_NAME,
		  hostname);

    /* Assigned ID */
    u16 = htons(tunnel->my_id);
    l2tp_dgram_add_avp(dgram, tunnel, MANDATORY,
		  sizeof(u16), VENDOR_IETF, AVP_ASSIGNED_TUNNEL_ID, &u16);

    /* Receive window size */
    u16 = htons(tunnel->rws);
    l2tp_dgram_add_avp(dgram, tunnel, MANDATORY,
		  sizeof(u16), VENDOR_IETF, AVP_RECEIVE_WINDOW_SIZE, &u16);

    /* Tie-breaker */
    l2tp_random_fill(tie_breaker, sizeof(tie_breaker));
    l2tp_dgram_add_avp(dgram, tunnel, NOT_MANDATORY,
		  sizeof(tie_breaker), VENDOR_IETF, AVP_TIE_BREAKER,
		  tie_breaker);

    /* Vendor name */
    l2tp_dgram_add_avp(dgram, tunnel, NOT_MANDATORY,
		  strlen(VENDOR_STR), VENDOR_IETF,
		  AVP_VENDOR_NAME, VENDOR_STR);

    /* Challenge */
    if (tunnel->peer->secret_len) {
	l2tp_random_fill(challenge, sizeof(challenge));
	l2tp_dgram_add_avp(dgram, tunnel, MANDATORY,
			   sizeof(challenge), VENDOR_IETF,
			   AVP_CHALLENGE, challenge);

	/* Compute and save expected response */
	l2tp_auth_gen_response(MESSAGE_SCCRP, tunnel->peer->secret,
			       challenge, sizeof(challenge), tunnel->expected_response);
    }

    tunnel->peer->hide_avps = old_hide;
    /* And ship it out */
    l2tp_tunnel_xmit_control_message(tunnel, dgram);
    return 1;
}

/**********************************************************************
* %FUNCTION: tunnel_handle_received_control_datagram
* %ARGUMENTS:
*  dgram -- received datagram
*  es -- event selector
*  from -- address of sender
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Handles a received control datagram
***********************************************************************/
void
l2tp_tunnel_handle_received_control_datagram(l2tp_dgram *dgram,
					     EventSelector *es,
					     struct sockaddr_in *from)
{
    l2tp_tunnel *tunnel;

    /* If it's SCCRQ, then handle it */
    if (dgram->msg_type == MESSAGE_SCCRQ) {
	tunnel_handle_SCCRQ(dgram, es, from);
	return;
    }

    /* Find the tunnel to which it refers */
    if (dgram->tid == 0) {
	l2tp_set_errmsg("Invalid control message - tunnel ID = 0");
	return;
    }
    tunnel = l2tp_tunnel_find_by_my_id(dgram->tid);

    if (!tunnel) {
	/* TODO: Send error message back? */
	DBG(l2tp_db(DBG_TUNNEL, "Invalid control message - unknown tunnel ID %d\n",
		   (int) dgram->tid));
	return;
    }

    /* Verify that source address is the tunnel's peer */
    if (tunnel->peer && tunnel->peer->validate_peer_ip) {
	if (from->sin_addr.s_addr != tunnel->peer_addr.sin_addr.s_addr) {
	    l2tp_set_errmsg("Invalid control message for tunnel %s - not sent from peer",
			    l2tp_debug_tunnel_to_str(tunnel));
	    return;
	}
    }

    /* Set port for replies */
    tunnel->peer_addr.sin_port = from->sin_port;

    /* Schedule an ACK for 100ms from now, but do not ack ZLB's */
    if (dgram->msg_type != MESSAGE_ZLB) {
	tunnel_schedule_ack(tunnel);
    }

    /* If it's an old datagram, ignore it */
    if (dgram->Ns != tunnel->Nr) {
	if (SERIAL_LT(dgram->Ns, tunnel->Nr)) {
	    /* Old packet: Drop it */
	    /* Throw away ack'd packets in our xmit queue */
	    tunnel_dequeue_acked_packets(tunnel);
	    return;
	}
	/* Out-of-order packet or intermediate dropped packets.
	   TODO: Look into handling this better */
	return;
    }

    /* Do not increment if we got ZLB */
    if (dgram->msg_type != MESSAGE_ZLB) {
	tunnel->Nr++;
    }

    /* Update peer_Nr */
    if (SERIAL_GT(dgram->Nr, tunnel->peer_Nr)) {
	tunnel->peer_Nr = dgram->Nr;
    }

    /* Reschedule HELLO handler for 60 seconds in future */
    if (tunnel->state != TUNNEL_RECEIVED_STOP_CCN &&
	tunnel->state != TUNNEL_SENT_STOP_CCN &&
	tunnel->hello_handler != NULL) {
	struct timeval t;
	t.tv_sec = 60;
	t.tv_usec = 0;
	Event_ChangeTimeout(tunnel->hello_handler, t);
    }

    /* Reset retransmission stuff */
    tunnel->retransmissions = 0;
    tunnel->timeout = 1;

    /* Throw away ack'd packets in our xmit queue */
    tunnel_dequeue_acked_packets(tunnel);

    /* Let the specific tunnel handle it */
    tunnel_process_received_datagram(tunnel, dgram);

    /* Run the xmit queue -- window may have opened */
    tunnel_xmit_queued_messages(tunnel);

    /* Destroy tunnel if required and if xmit queue empty */
    if (!tunnel->xmit_queue_head) {
	if (tunnel->timeout_handler) {
	    Event_DelHandler(tunnel->es, tunnel->timeout_handler);
	    tunnel->timeout_handler = NULL;
	}
	if (tunnel->state == TUNNEL_RECEIVED_STOP_CCN) {
	    tunnel_schedule_destruction(tunnel);
	} else if (tunnel->state == TUNNEL_SENT_STOP_CCN) {
	    /* Our stop-CCN has been ack'd, destroy NOW */
	    tunnel_free(tunnel);
	}
    }
}

/**********************************************************************
* %FUNCTION: tunnel_handle_SCCRQ
* %ARGUMENTS:
*  dgram -- the received datagram
*  es -- event selector
*  from -- address of sender
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Handles an incoming SCCRQ
***********************************************************************/
static void
tunnel_handle_SCCRQ(l2tp_dgram *dgram,
		    EventSelector *es,
		    struct sockaddr_in *from)
{
    l2tp_tunnel *tunnel = NULL;

    uint16_t u16;
    uint32_t u32;
    unsigned char challenge[16];
    char *hostname;

    /* TODO: Check if this is a retransmitted SCCRQ */
    /* Allocate a tunnel */
    tunnel = tunnel_new(es);
    if (!tunnel) {
	l2tp_set_errmsg("Unable to allocate new tunnel");
	return;
    }

    tunnel->peer_addr = *from;

    hash_insert(&tunnels_by_peer_address, tunnel);

    /* We've received our first control datagram (SCCRQ) */
    tunnel->Nr = 1;

    if (tunnel_set_params(tunnel, dgram) < 0) return;

    /* Hunky-dory.  Send SCCRP */
    dgram = l2tp_dgram_new_control(MESSAGE_SCCRP, tunnel->assigned_id, 0);
    if (!dgram) {
	/* Doh! Out of resources.  Not much chance of StopCCN working... */
	tunnel_send_StopCCN(tunnel,
			    RESULT_GENERAL_ERROR, ERROR_OUT_OF_RESOURCES,
			    "Out of memory");
	return;
    }

    /* Protocol version */
    u16 = htons(0x0100);
    l2tp_dgram_add_avp(dgram, tunnel, MANDATORY,
		  sizeof(u16), VENDOR_IETF, AVP_PROTOCOL_VERSION, &u16);

    /* Framing capabilities -- hard-coded as sync and async */
    u32 = htonl(3);
    l2tp_dgram_add_avp(dgram, tunnel, MANDATORY,
		  sizeof(u32), VENDOR_IETF, AVP_FRAMING_CAPABILITIES, &u32);

    hostname = tunnel->peer->hostname[0] ? tunnel->peer->hostname : Hostname;

    /* Host name */
    l2tp_dgram_add_avp(dgram, tunnel, MANDATORY,
		  strlen(hostname), VENDOR_IETF, AVP_HOST_NAME,
		  hostname);

    /* Assigned ID */
    u16 = htons(tunnel->my_id);
    l2tp_dgram_add_avp(dgram, tunnel, MANDATORY,
		  sizeof(u16), VENDOR_IETF, AVP_ASSIGNED_TUNNEL_ID, &u16);

    /* Receive window size */
    u16 = htons(tunnel->rws);
    l2tp_dgram_add_avp(dgram, tunnel, MANDATORY,
		  sizeof(u16), VENDOR_IETF, AVP_RECEIVE_WINDOW_SIZE, &u16);

    /* Vendor name */
    l2tp_dgram_add_avp(dgram, tunnel, NOT_MANDATORY,
		  strlen(VENDOR_STR), VENDOR_IETF,
		  AVP_VENDOR_NAME, VENDOR_STR);

    if (tunnel->peer->secret_len) {
	/* Response */
	l2tp_dgram_add_avp(dgram, tunnel, MANDATORY,
			   MD5LEN, VENDOR_IETF, AVP_CHALLENGE_RESPONSE,
			   tunnel->response);

	/* Challenge */
	l2tp_random_fill(challenge, sizeof(challenge));
	l2tp_dgram_add_avp(dgram, tunnel, MANDATORY,
			   sizeof(challenge), VENDOR_IETF,
			   AVP_CHALLENGE, challenge);

	/* Compute and save expected response */
	l2tp_auth_gen_response(MESSAGE_SCCCN, tunnel->peer->secret,
			       challenge, sizeof(challenge), tunnel->expected_response);
    }

    tunnel_set_state(tunnel, TUNNEL_WAIT_CTL_CONN);

    /* And ship it out */
    l2tp_tunnel_xmit_control_message(tunnel, dgram);
}

/**********************************************************************
* %FUNCTION: tunnel_schedule_ack
* %ARGUMENTS:
*  tunnel -- the tunnel
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Schedules an ack for 100ms from now.  The hope is we'll be able to
*  piggy-back the ack on a packet in the queue; if not, we'll send a ZLB.
***********************************************************************/
static void
tunnel_schedule_ack(l2tp_tunnel *tunnel)
{
    struct timeval t;

    DBG(l2tp_db(DBG_FLOW, "tunnel_schedule_ack(%s)\n",
	   l2tp_debug_tunnel_to_str(tunnel)));
    /* Already scheduled?  Do nothing */
    if (tunnel->ack_handler) return;

    t.tv_sec = 0;
    t.tv_usec = 100000;
    tunnel->ack_handler = Event_AddTimerHandler(tunnel->es,
						t, tunnel_do_ack, tunnel);
}

/**********************************************************************
* %FUNCTION: tunnel_do_ack
* %ARGUMENTS:
*  es -- event selector
*  fd, flags -- not used
*  data -- pointer to tunnel which needs ack
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  If there is a frame on our queue, update it's Nr and run queue; if not,
*  send a ZLB immediately.
***********************************************************************/
static void
tunnel_do_ack(EventSelector *es,
	      int fd,
	      unsigned int flags,
	      void *data)
{
    l2tp_tunnel *tunnel = (l2tp_tunnel *) data;

    /* Ack handler has fired */
    tunnel->ack_handler = NULL;

    DBG(l2tp_db(DBG_FLOW, "tunnel_do_ack(%s)\n",
	   l2tp_debug_tunnel_to_str(tunnel)));

    /* If nothing is on the queue, add a ZLB */
    /* Or, if we cannot transmit because of flow-control, send ZLB */
    if (!tunnel->xmit_new_dgrams ||
	tunnel_outstanding_frames(tunnel) >= tunnel->cwnd) {
	tunnel_send_ZLB(tunnel);
	return;
    }

    /* Run the queue */
    tunnel_xmit_queued_messages(tunnel);
}

/**********************************************************************
* %FUNCTION: tunnel_dequeue_acked_packets
* %ARGUMENTS:
*  tunnel -- the tunnel
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Discards all acknowledged packets from our xmit queue.
***********************************************************************/
static void
tunnel_dequeue_acked_packets(l2tp_tunnel *tunnel)
{
    l2tp_dgram *dgram = tunnel->xmit_queue_head;
    DBG(l2tp_db(DBG_FLOW, "tunnel_dequeue_acked_packets(%s) %s\n",
       l2tp_debug_tunnel_to_str(tunnel), tunnel_flow_control_stats(tunnel)));
    while (dgram) {
	if (SERIAL_LT(dgram->Ns, tunnel->peer_Nr)) {
	    tunnel_dequeue_head(tunnel);
	    if (tunnel->cwnd < tunnel->ssthresh) {
		/* Slow start: increment CWND for each packet dequeued */
		tunnel->cwnd++;
		if (tunnel->cwnd > tunnel->peer_rws) {
		    tunnel->cwnd = tunnel->peer_rws;
		}
	    } else {
		/* Congestion avoidance: increment CWND for each CWND
		   packets dequeued */
		tunnel->cwnd_counter++;
		if (tunnel->cwnd_counter >= tunnel->cwnd) {
		    tunnel->cwnd_counter = 0;
		    tunnel->cwnd++;
		    if (tunnel->cwnd > tunnel->peer_rws) {
			tunnel->cwnd = tunnel->peer_rws;
		    }
		}
	    }
	} else {
	    break;
	}
	dgram = tunnel->xmit_queue_head;
    }
}

/**********************************************************************
* %FUNCTION: tunnel_handle_timeout
* %ARGUMENTS:
*  es -- event selector
*  fd, flags -- not used
*  data -- pointer to tunnel which needs ack
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Called when retransmission timer fires.
***********************************************************************/
static void
tunnel_handle_timeout(EventSelector *es,
		      int fd,
		      unsigned int flags,
		      void *data)
{
    l2tp_tunnel *tunnel = (l2tp_tunnel *) data;

    /* Timeout handler has fired */
    tunnel->timeout_handler = NULL;
    DBG(l2tp_db(DBG_FLOW, "tunnel_handle_timeout(%s)\n",
	   l2tp_debug_tunnel_to_str(tunnel)));

    /* Reset xmit_new_dgrams */
    tunnel->xmit_new_dgrams = tunnel->xmit_queue_head;

    /* Set Ns on wire to Ns of first frame in queue */
    if (tunnel->xmit_queue_head) {
	tunnel->Ns_on_wire = tunnel->xmit_queue_head->Ns;
    }

    /* Go back to slow-start phase */
    tunnel->ssthresh = tunnel->cwnd / 2;
    if (!tunnel->ssthresh) tunnel->ssthresh = 1;
    tunnel->cwnd = 1;
    tunnel->cwnd_counter = 0;

    tunnel->retransmissions++;
    if (tunnel->retransmissions >= MAX_RETRANSMISSIONS) {
	l2tp_set_errmsg("Too many retransmissions on tunnel (%s); closing down",
		   l2tp_debug_tunnel_to_str(tunnel));
		   
	if (tunnel->state < TUNNEL_ESTABLISHED && tunnel->peer && tunnel->peer->persist && 
	    (tunnel->peer->maxfail == 0 || tunnel->peer->fail++ < tunnel->peer->maxfail)) 
	{
	    struct timeval t;

	    t.tv_sec = tunnel->peer->holdoff;
	    t.tv_usec = 0;
	    Event_AddTimerHandler(tunnel->es, t, l2tp_tunnel_reestablish, tunnel->peer);
	}
	
	/* Close tunnel... */
	tunnel_free(tunnel);
	return;
    }

    /* Double timeout, capping at 8 seconds */
    if (tunnel->timeout < 8) {
	tunnel->timeout *= 2;
    }

    /* Run the queue */
    tunnel_xmit_queued_messages(tunnel);
}

/**********************************************************************
* %FUNCTION: tunnel_send_StopCCN
* %ARGUMENTS:
*  tunnel -- the tunnel
*  result_code -- what to put in result-code AVP
*  error_code -- what to put in error-code field
*  fmt -- format string for error message
*  ... -- args for formatting error message
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Sends a StopCCN control message.
***********************************************************************/
static void
tunnel_send_StopCCN(l2tp_tunnel *tunnel,
		    int result_code,
		    int error_code,
		    char const *fmt, ...)
{
    char buf[256];
    va_list ap;
    uint16_t u16;
    uint16_t len;
    l2tp_dgram *dgram;

    /* Build the buffer for the result-code AVP */
    buf[0] = result_code / 256;
    buf[1] = result_code & 255;
    buf[2] = error_code / 256;
    buf[3] = error_code & 255;

    va_start(ap, fmt);
    vsnprintf(buf+4, 256-4, fmt, ap);
    buf[255] = 0;
    va_end(ap);

    DBG(l2tp_db(DBG_TUNNEL, "tunnel_send_StopCCN(%s, %d, %d, %s)\n",
	   l2tp_debug_tunnel_to_str(tunnel), result_code, error_code, buf+4));

    len = 4 + strlen(buf+4);
    /* Build the datagram */
    dgram = l2tp_dgram_new_control(MESSAGE_StopCCN, tunnel->assigned_id, 0);
    if (!dgram) return;

    /* Add assigned tunnel ID */
    u16 = htons(tunnel->my_id);
    l2tp_dgram_add_avp(dgram, tunnel, MANDATORY,
		  sizeof(u16), VENDOR_IETF, AVP_ASSIGNED_TUNNEL_ID, &u16);

    /* Add result code */
    l2tp_dgram_add_avp(dgram, tunnel, MANDATORY,
		  len, VENDOR_IETF, AVP_RESULT_CODE, buf);

    /* TODO: Clean up */
    tunnel_set_state(tunnel, TUNNEL_SENT_STOP_CCN);

    /* Ship it out */
    l2tp_tunnel_xmit_control_message(tunnel, dgram);
}

/**********************************************************************
* %FUNCTION: tunnel_handle_StopCCN
* %ARGUMENTS:
*  tunnel -- the tunnel
*  dgram -- received datagram
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Processes a received StopCCN frame (shuts tunnel down)
***********************************************************************/
static void
tunnel_handle_StopCCN(l2tp_tunnel *tunnel,
		       l2tp_dgram *dgram)
{
    unsigned char *val;
    int mandatory, hidden;
    uint16_t len, vendor, type;
    int err;
    l2tp_session *ses;
    void *cursor;

    /* Shut down all the sessions */
    for (ses = hash_start(&tunnel->sessions_by_my_id, &cursor);
	 ses ;
	 ses = hash_next(&tunnel->sessions_by_my_id, &cursor)) {
	hash_remove(&tunnel->sessions_by_my_id, ses);
	l2tp_session_free(ses, "Tunnel closing down", 1);
    }

    tunnel_set_state(tunnel, TUNNEL_RECEIVED_STOP_CCN);

    /* If there are any queued datagrams waiting for transmission,
       nuke them and adjust tunnel's Ns to whatever peer has ack'd */
    /* TODO: Is this correct? */
    if (tunnel->xmit_queue_head) {
	tunnel->Ns = tunnel->peer_Nr;
	while(tunnel->xmit_queue_head) {
	    tunnel_dequeue_head(tunnel);
	}
    }

    /* Parse the AVP's */
    while(1) {
	val = l2tp_dgram_pull_avp(dgram, tunnel, &mandatory, &hidden,
			     &len, &vendor, &type, &err);
	if (!val) {
	    break;
	}

	/* Only care about assigned tunnel ID.  TODO: Fix this! */
	if (vendor != VENDOR_IETF || type != AVP_ASSIGNED_TUNNEL_ID) {
	    continue;
	}

	if (len == 2) {
	    tunnel->assigned_id = ((unsigned int) val[0]) * 256 +
		(unsigned int) val[1];
	}
    }

    /* No point in delaying ack; there will never be a datagram for
       piggy-back.  So cancel ack timer and shoot out a ZLB now */
    if (tunnel->ack_handler) {
	Event_DelHandler(tunnel->es, tunnel->ack_handler);
	tunnel->ack_handler = NULL;
    }
    tunnel_send_ZLB(tunnel);
    /* We'll be scheduled for destruction after this function returns */
}

/**********************************************************************
* %FUNCTION: tunnel_process_received_datagram
* %ARGUMENTS:
*  tunnel -- the tunnel
*  dgram -- the received datagram
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Handles a received control message for this tunnel
***********************************************************************/
static void
tunnel_process_received_datagram(l2tp_tunnel *tunnel,
				 l2tp_dgram *dgram)
{
    l2tp_session *ses = NULL;

    /* If message is associated with existing session, find session */

    DBG(l2tp_db(DBG_TUNNEL, "tunnel_process_received_datagram(%s, %s)\n",
	   l2tp_debug_tunnel_to_str(tunnel),
	   l2tp_debug_message_type_to_str(dgram->msg_type)));
    switch (dgram->msg_type) {
    case MESSAGE_OCRP:
    case MESSAGE_OCCN:
    case MESSAGE_ICRP:
    case MESSAGE_ICCN:
    case MESSAGE_CDN:
	ses = l2tp_tunnel_find_session(tunnel, dgram->sid);
	if (!ses) {
	    l2tp_set_errmsg("Session-related message for unknown session %d",
		       (int) dgram->sid);
	    return;
	}
    }

    switch (dgram->msg_type) {
    case MESSAGE_StopCCN:
	tunnel_handle_StopCCN(tunnel, dgram);
	return;
    case MESSAGE_SCCRP:
	tunnel_handle_SCCRP(tunnel, dgram);
	return;
    case MESSAGE_SCCCN:
	tunnel_handle_SCCCN(tunnel, dgram);
	return;
    case MESSAGE_ICRQ:
	tunnel_handle_ICRQ(tunnel, dgram);
	return;
    case MESSAGE_CDN:
	l2tp_session_handle_CDN(ses, dgram);
	return;
    case MESSAGE_ICRP:
	l2tp_session_handle_ICRP(ses, dgram);
	return;
    case MESSAGE_ICCN:
	l2tp_session_handle_ICCN(ses, dgram);
	return;
    case MESSAGE_HELLO:
	tunnel_setup_hello(tunnel);
	return;
    }
}

/**********************************************************************
* %FUNCTION: tunnel_finally_cleanup
* %ARGUMENTS:
*  es -- event selector
*  fd, flags -- ignored
*  data -- the tunnel
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Deallocates all tunnel state
***********************************************************************/
static void
tunnel_finally_cleanup(EventSelector *es,
		       int fd,
		       unsigned int flags,
		       void *data)
{
    l2tp_tunnel *tunnel = (l2tp_tunnel *) data;

    /* Hello handler has fired */
    tunnel->hello_handler = NULL;
    tunnel_free(tunnel);
}

/**********************************************************************
* %FUNCTION: tunnel_schedule_destruction
* %ARGUMENTS:
*  tunnel -- tunnel which is to be destroyed
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Schedules tunnel for destruction 31 seconds hence.
***********************************************************************/
static void
tunnel_schedule_destruction(l2tp_tunnel *tunnel)
{
    struct timeval t;
    void *cursor;
    l2tp_session *ses;

    t.tv_sec = 31;
    t.tv_usec = 0;

    DBG(l2tp_db(DBG_TUNNEL, "tunnel_schedule_destruction(%s)\n",
	   l2tp_debug_tunnel_to_str(tunnel)));
    /* Shut down the tunnel in 31 seconds - (ab)use the hello handler */
    if (tunnel->hello_handler) {
	Event_DelHandler(tunnel->es, tunnel->hello_handler);
    }
    tunnel->hello_handler =
	Event_AddTimerHandler(tunnel->es, t,
			      tunnel_finally_cleanup, tunnel);

    /* Kill the sessions now */
    for (ses = hash_start(&tunnel->sessions_by_my_id, &cursor);
	 ses ;
	 ses = hash_next(&tunnel->sessions_by_my_id, &cursor)) {
	l2tp_session_free(ses, "Tunnel closing down", 1);
    }

    /* Clear hash table */
    l2tp_session_hash_init(&tunnel->sessions_by_my_id);
}

/**********************************************************************
* %FUNCTION: tunnel_send_ZLB
* %ARGUMENTS:
*  tunnel -- the tunnel
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Sends a ZLB ack packet.
***********************************************************************/
void
tunnel_send_ZLB(l2tp_tunnel *tunnel)
{
    l2tp_dgram *dgram =
	l2tp_dgram_new_control(MESSAGE_ZLB, tunnel->assigned_id, 0);
    if (!dgram) {
	l2tp_set_errmsg("tunnel_send_ZLB: Out of memory");
	return;
    }
    dgram->Nr = tunnel->Nr;
    dgram->Ns = tunnel->Ns_on_wire;
    l2tp_dgram_send_to_wire(dgram, &tunnel->peer_addr);
    l2tp_dgram_free(dgram);
}

/**********************************************************************
* %FUNCTION: tunnel_handle_SCCRP
* %ARGUMENTS:
*  tunnel -- the tunnel
*  dgram -- the incoming datagram
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Handles an incoming SCCRP
***********************************************************************/
static void
tunnel_handle_SCCRP(l2tp_tunnel *tunnel,
		    l2tp_dgram *dgram)
{

    /* TODO: If we don't get challenge response at all, barf */
    /* Are we expecing SCCRP? */
    if (tunnel->state != TUNNEL_WAIT_CTL_REPLY) {
	tunnel_send_StopCCN(tunnel, RESULT_FSM_ERROR, 0, "Not expecting SCCRP");
	return;
    }

    /* Extract tunnel params */
    if (tunnel_set_params(tunnel, dgram) < 0) return;

    DBG(l2tp_db(DBG_TUNNEL, "tunnel_establish(%s) ops: %p\n",
	l2tp_debug_tunnel_to_str(tunnel), tunnel->call_ops));
    if (tunnel->call_ops && tunnel->call_ops->tunnel_establish &&
	tunnel->call_ops->tunnel_establish(tunnel) < 0) {
	tunnel_send_StopCCN(tunnel, RESULT_GENERAL_ERROR, ERROR_VENDOR_SPECIFIC,
			    "%s", l2tp_get_errmsg());
	return;
    }

    tunnel_set_state(tunnel, TUNNEL_ESTABLISHED);
    tunnel_setup_hello(tunnel);

    /* Hunky-dory.  Send SCCCN */
    dgram = l2tp_dgram_new_control(MESSAGE_SCCCN, tunnel->assigned_id, 0);
    if (!dgram) {
	/* Doh! Out of resources.  Not much chance of StopCCN working... */
	tunnel_send_StopCCN(tunnel,
			    RESULT_GENERAL_ERROR, ERROR_OUT_OF_RESOURCES,
			    "Out of memory");
	return;
    }

    /* Add response */
    if (tunnel->peer->secret_len) {
	l2tp_dgram_add_avp(dgram, tunnel, MANDATORY,
			   MD5LEN, VENDOR_IETF, AVP_CHALLENGE_RESPONSE,
			   tunnel->response);
    }

    /* Ship it out */
    l2tp_tunnel_xmit_control_message(tunnel, dgram);

    /* Tell sessions tunnel has been established */
    tunnel_tell_sessions_tunnel_open(tunnel);
}

/**********************************************************************
* %FUNCTION: tunnel_set_params
* %ARGUMENTS:
*  tunnel -- the tunnel
*  dgram -- incoming SCCRQ or SCCRP datagram
* %RETURNS:
*  0 if OK, non-zero on error
* %DESCRIPTION:
*  Sets up initial tunnel parameters (assigned ID, etc.)
***********************************************************************/
static int
tunnel_set_params(l2tp_tunnel *tunnel,
		  l2tp_dgram *dgram)
{
    unsigned char *val;
    int mandatory, hidden;
    uint16_t len, vendor, type;
    int err = 0;
    int found_response = 0;

    uint16_t u16;

    /* Get host name */
    val = l2tp_dgram_search_avp(dgram, tunnel, &mandatory, &hidden, &len,
                           VENDOR_IETF, AVP_HOST_NAME);
    if (!val) {
	l2tp_set_errmsg("No host name AVP in SCCRQ");
	tunnel_free(tunnel);
	return -1;
    }

    if (len >= MAX_HOSTNAME) len = MAX_HOSTNAME-1;
    memcpy(tunnel->peer_hostname, val, len);
    tunnel->peer_hostname[len+1] = 0;
    DBG(l2tp_db(DBG_TUNNEL, "%s: Peer host name is '%s'\n",
                           l2tp_debug_tunnel_to_str(tunnel), tunnel->peer_hostname));

    /* Find peer */
    if (tunnel->peer == NULL || tunnel->peer->addr.sin_addr.s_addr != INADDR_ANY)
	tunnel->peer = l2tp_peer_find(&tunnel->peer_addr, tunnel->peer_hostname);

    /* Get assigned tunnel ID */
    val = l2tp_dgram_search_avp(dgram, tunnel, &mandatory, &hidden, &len,
			   VENDOR_IETF, AVP_ASSIGNED_TUNNEL_ID);
    if (!val) {
	l2tp_set_errmsg("No assigned tunnel ID AVP in SCCRQ");
	tunnel_free(tunnel);
	return -1;
    }

    if (!l2tp_dgram_validate_avp(VENDOR_IETF, AVP_ASSIGNED_TUNNEL_ID,
			    len, mandatory)) {
	tunnel_free(tunnel);
	return -1;
    }

    /* Set tid */
    tunnel->assigned_id = ((unsigned int) val[0]) * 256 + (unsigned int) val[1];
    if (!tunnel->assigned_id) {
	l2tp_set_errmsg("Invalid assigned-tunnel-ID of zero");
	tunnel_free(tunnel);
	return -1;
    }

    /* Validate peer */
    if (!tunnel->peer) {
	l2tp_set_errmsg("Peer %s is not authorized to create a tunnel",
		   inet_ntoa(tunnel->peer_addr.sin_addr));
	tunnel_send_StopCCN(tunnel, RESULT_NOAUTH, ERROR_OK, "%s", l2tp_get_errmsg());
	return -1;
    }

    /* Setup LNS call ops if LAC wasn't set before */
    if (!tunnel->call_ops)
	tunnel->call_ops = tunnel->peer->lns_ops;

    /* Pull out and examine AVP's */
    while(1) {
	val = l2tp_dgram_pull_avp(dgram, tunnel, &mandatory, &hidden,
			     &len, &vendor, &type, &err);
	if (!val) {
	    if (err) {
		tunnel_send_StopCCN(tunnel, RESULT_GENERAL_ERROR,
				    ERROR_BAD_VALUE, "%s", l2tp_get_errmsg());
		return -1;
	    }
	    break;
	}

	/* Unknown vendor?  Ignore, unless mandatory */
	if (vendor != VENDOR_IETF) {
	    if (!mandatory) continue;
	    tunnel_send_StopCCN(tunnel, RESULT_GENERAL_ERROR,
				ERROR_UNKNOWN_AVP_WITH_M_BIT,
				"Unknown mandatory attribute (vendor=%d, type=%d) in %s",
				(int) vendor, (int) type,
				l2tp_debug_avp_type_to_str(dgram->msg_type));
	    return -1;
	}

	/* Validate AVP, ignore invalid AVP's without M bit set */
	if (!l2tp_dgram_validate_avp(vendor, type, len, mandatory)) {
	    if (!mandatory) continue;
	    tunnel_send_StopCCN(tunnel, RESULT_GENERAL_ERROR,
				ERROR_BAD_LENGTH, "%s", l2tp_get_errmsg());
	    return -1;
	}

	switch(type) {
	case AVP_PROTOCOL_VERSION:
	    u16 = ((uint16_t) val[0]) * 256 + val[1];
	    if (u16 != 0x100) {
		tunnel_send_StopCCN(tunnel, RESULT_UNSUPPORTED_VERSION,
				    0x100, "Unsupported protocol version");
		return -1;
	    }
	    break;

	case AVP_HOST_NAME:
	    /* Already been handled */
	    break;

	case AVP_FRAMING_CAPABILITIES:
	    /* TODO: Do we care about framing capabilities? */
	    break;

	case AVP_ASSIGNED_TUNNEL_ID:
	    /* Already been handled */
	    break;

	case AVP_BEARER_CAPABILITIES:
	    /* TODO: Do we care? */
	    break;

	case AVP_RECEIVE_WINDOW_SIZE:
	    u16 = ((uint16_t) val[0]) * 256 + val[1];
	    /* Silently correct bogus rwin */
	    if (u16 < 1) u16 = 1;
	    tunnel->peer_rws = u16;
	    tunnel->ssthresh = u16;
	    break;

	case AVP_CHALLENGE:
	    if (tunnel->peer->secret_len) {
		l2tp_auth_gen_response(
		    ((dgram->msg_type == MESSAGE_SCCRQ) ? MESSAGE_SCCRP : MESSAGE_SCCCN),
		    tunnel->peer->secret,
		    val, len, tunnel->response);
	    }
	    break;

	case AVP_CHALLENGE_RESPONSE:
	    /* Length has been validated by l2tp_dgram_validate_avp */
	    if (tunnel->peer->secret_len) {
		if (memcmp(val, tunnel->expected_response, MD5LEN)) {
		    tunnel_send_StopCCN(tunnel, RESULT_NOAUTH, ERROR_BAD_VALUE,
					"Incorrect challenge response");
		    return -1;
		}
	    }
	    found_response = 1;
	    break;

	case AVP_TIE_BREAKER:
	    /* TODO: Handle tie-breaker */
	    break;

	case AVP_FIRMWARE_REVISION:
	    /* TODO: Do we care? */
	    break;

	case AVP_VENDOR_NAME:
	    /* TODO: Do we care? */
	    break;

	default:
	    /* TODO: Maybe print an error? */
	    break;
	}
    }

    if (tunnel->peer->secret_len &&
	!found_response &&
	dgram->msg_type != MESSAGE_SCCRQ) {
	tunnel_send_StopCCN(tunnel, RESULT_NOAUTH, 0,
			    "Missing challenge-response");
	return -1;
    }
    return 0;
}

/**********************************************************************
* %FUNCTION: tunnel_setup_hello
* %ARGUMENTS:
*  tunnel -- the tunnel
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Sets up timer for sending HELLO messages
***********************************************************************/
static void
tunnel_setup_hello(l2tp_tunnel *tunnel)
{
    struct timeval t;

    t.tv_sec = 60;
    t.tv_usec = 0;

    if (tunnel->hello_handler) {
	Event_DelHandler(tunnel->es, tunnel->hello_handler);
    }
    tunnel->hello_handler = Event_AddTimerHandler(tunnel->es, t,
						  tunnel_do_hello, tunnel);
}

/**********************************************************************
* %FUNCTION: tunnel_do_hello
* %ARGUMENTS:
*  es -- event selector
*  fd, flags -- ignored
*  data -- the tunnel
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Deallocates all tunnel state
***********************************************************************/
static void
tunnel_do_hello(EventSelector *es,
		int fd,
		unsigned int flags,
		void *data)
{
    l2tp_tunnel *tunnel = (l2tp_tunnel *) data;
    l2tp_dgram *dgram;

    /* Hello handler has fired */
    tunnel->hello_handler = NULL;

    /* Reschedule HELLO timer */
    tunnel_setup_hello(tunnel);

    /* Send a HELLO message */
    dgram = l2tp_dgram_new_control(MESSAGE_HELLO, tunnel->assigned_id, 0);
    if (dgram) l2tp_tunnel_xmit_control_message(tunnel, dgram);
}

/**********************************************************************
* %FUNCTION: tunnel_handle_SCCCN
* %ARGUMENTS:
*  tunnel -- the tunnel
*  dgram -- the incoming datagram
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Handles an incoming SCCCN
***********************************************************************/
static void
tunnel_handle_SCCCN(l2tp_tunnel *tunnel,
		    l2tp_dgram *dgram)
{
    unsigned char *val;
    uint16_t len;
    int hidden, mandatory;

    /* Are we expecing SCCCN? */
    if (tunnel->state != TUNNEL_WAIT_CTL_CONN) {
	tunnel_send_StopCCN(tunnel, RESULT_FSM_ERROR, 0, "Not expecting SCCCN");
	return;
    }

    /* Check challenge response */
    if (tunnel->peer->secret_len) {
	val = l2tp_dgram_search_avp(dgram, tunnel, &mandatory, &hidden, &len,
				    VENDOR_IETF, AVP_CHALLENGE_RESPONSE);
	if (!val) {
	    tunnel_send_StopCCN(tunnel, RESULT_NOAUTH, 0,
				"Missing challenge-response");
	    return;
	}

	if (!l2tp_dgram_validate_avp(VENDOR_IETF, AVP_CHALLENGE_RESPONSE,
				     len, mandatory)) {
	    tunnel_send_StopCCN(tunnel, RESULT_GENERAL_ERROR, ERROR_BAD_LENGTH,
				"Invalid challenge-response");
	    return;
	}
	if (memcmp(val, tunnel->expected_response, MD5LEN)) {
	    tunnel_send_StopCCN(tunnel, RESULT_NOAUTH, ERROR_BAD_VALUE,
				"Incorrect challenge response");
	    return;
	}
    }

    DBG(l2tp_db(DBG_TUNNEL, "tunnel_establish(%s) ops: %p\n",
	l2tp_debug_tunnel_to_str(tunnel), tunnel->call_ops));
    if (tunnel->call_ops && tunnel->call_ops->tunnel_establish &&
	tunnel->call_ops->tunnel_establish(tunnel) < 0) {
	tunnel_send_StopCCN(tunnel, RESULT_GENERAL_ERROR, ERROR_VENDOR_SPECIFIC,
			    "%s", l2tp_get_errmsg());
	return;
    }

    tunnel_set_state(tunnel, TUNNEL_ESTABLISHED);
    tunnel_setup_hello(tunnel);

    /* Tell sessions tunnel has been established */
    tunnel_tell_sessions_tunnel_open(tunnel);
}

/**********************************************************************
* %FUNCTION: tunnel_find_for_peer
* %ARGUMENTS:
*  peer -- an L2TP peer
*  es -- an event selector
* %RETURNS:
*  An existing tunnel to peer (if one exists) or a new one (if one does not),
*  or NULL if no tunnel could be established.  If the existing tunnel
*  is in the state RECEIVED_STOP_CCN or SENT_STOP_CCN, make a new one.
***********************************************************************/
l2tp_tunnel *
l2tp_tunnel_find_for_peer(l2tp_peer *peer,
		     EventSelector *es)
{
    l2tp_tunnel *tunnel;
    void *cursor;
    
    if (peer->addr.sin_addr.s_addr == INADDR_ANY)
    {
	for (tunnel = hash_start(&tunnels_by_my_id, &cursor);
	    tunnel && tunnel->peer != peer; 
	    tunnel = hash_next(&tunnels_by_my_id, &cursor));
    } else {
	tunnel = tunnel_find_bypeer(peer->addr);
    }

    if (tunnel) {
	if (tunnel->state == TUNNEL_WAIT_CTL_REPLY ||
	    tunnel->state == TUNNEL_WAIT_CTL_CONN ||
	    tunnel->state == TUNNEL_ESTABLISHED) {
	    return tunnel;
	}
    }

    /* No tunnel, or tunnel in wrong state */
    return tunnel_establish(peer, es);
}

/**********************************************************************
* %FUNCTION: tunnel_find_session
* %ARGUMENTS:
*  sid -- session ID
* %RETURNS:
*  The session with specified ID, or NULL if no such session
***********************************************************************/
l2tp_session *
l2tp_tunnel_find_session(l2tp_tunnel *tunnel,
		    uint16_t sid)
{
    l2tp_session candidate;
    candidate.my_id = sid;
    return hash_find(&tunnel->sessions_by_my_id, &candidate);
}

/**********************************************************************
* %FUNCTION: tunnel_tell_sessions_tunnel_open
* %ARGUMENTS:
*  tunnel -- the tunnel
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Informs all waiting sessions that tunnel has moved into ESTABLISHED state.
***********************************************************************/
static void
tunnel_tell_sessions_tunnel_open(l2tp_tunnel *tunnel)
{
    l2tp_session *ses;
    void *cursor;

    for (ses = hash_start(&tunnel->sessions_by_my_id, &cursor);
	 ses ;
	 ses = hash_next(&tunnel->sessions_by_my_id, &cursor)) {
	l2tp_session_notify_tunnel_open(ses);
    }
}

/**********************************************************************
* %FUNCTION: tunnel_add_session
* %ARGUMENTS:
*  ses -- session to add
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Adds session to tunnel's hash table.  If tunnel is up, calls
*  l2tp_session_notify_tunnel_open
***********************************************************************/
void
l2tp_tunnel_add_session(l2tp_session *ses)
{
    l2tp_tunnel *tunnel = ses->tunnel;

    hash_insert(&tunnel->sessions_by_my_id, ses);
    if (tunnel->state == TUNNEL_ESTABLISHED) {
	l2tp_session_notify_tunnel_open(ses);
    }
}

void
l2tp_tunnel_reestablish(EventSelector *es,
		      int fd,
		      unsigned int flags,
		      void *data)
{
    l2tp_peer *peer = (l2tp_peer*) data;
    l2tp_session *ses;

    ses = l2tp_session_call_lns(peer, "foobar", es, NULL);
    if (!ses) {
        DBG(l2tp_db(DBG_TUNNEL, "l2tp_tunnel_reestablish() failed\n"));
        return;
    }
}

/**********************************************************************
* %FUNCTION: tunnel_delete_session
* %ARGUMENTS:
*  ses -- session to delete
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Deletes session from tunnel's hash table and frees it.
***********************************************************************/
void
l2tp_tunnel_delete_session(l2tp_session *ses, char const *reason, int may_reestablish)
{
    l2tp_tunnel *tunnel = ses->tunnel;

    if (may_reestablish && ses->state < SESSION_ESTABLISHED &&
	tunnel->peer && tunnel->peer->persist &&
	(tunnel->peer->maxfail == 0 || tunnel->peer->fail++ < tunnel->peer->maxfail))
    {
	struct timeval t;

	t.tv_sec = tunnel->peer->holdoff;
	t.tv_usec = 0;
	Event_AddTimerHandler(tunnel->es, t, l2tp_tunnel_reestablish, tunnel->peer);
    }

    hash_remove(&tunnel->sessions_by_my_id, ses);
    l2tp_session_free(ses, reason, may_reestablish);

    /* Tear down tunnel if so indicated */
    if (!hash_num_entries(&tunnel->sessions_by_my_id)) {
	if (!tunnel->peer->retain_tunnel) {
	    tunnel_send_StopCCN(tunnel,
				RESULT_GENERAL_REQUEST, 0,
				"Last session has closed");
	}
    }
}

/**********************************************************************
* %FUNCTION: tunnel_handle_ICRQ
* %ARGUMENTS:
*  tunnel -- the tunnel
*  dgram -- the datagram
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Handles ICRQ (Incoming Call ReQuest)
***********************************************************************/
static void
tunnel_handle_ICRQ(l2tp_tunnel *tunnel,
		   l2tp_dgram *dgram)
{
    uint16_t u16;
    unsigned char *val;
    uint16_t len;
    int mandatory, hidden;

    /* Get assigned session ID */
    val = l2tp_dgram_search_avp(dgram, tunnel, &mandatory, &hidden, &len,
			   VENDOR_IETF, AVP_ASSIGNED_SESSION_ID);
    if (!val) {
	l2tp_set_errmsg("No assigned tunnel ID AVP in ICRQ");
	return;
    }
    if (!l2tp_dgram_validate_avp(VENDOR_IETF, AVP_ASSIGNED_SESSION_ID,
			    len, mandatory)) {
	/* TODO: send CDN */
	return;
    }

    /* Set assigned session ID */
    u16 = ((uint16_t) val[0]) * 256 + (uint16_t) val[1];

    if (!u16) {
	/* TODO: send CDN */
	return;
    }

    /* Tunnel in wrong state? */
    if (tunnel->state != TUNNEL_ESTABLISHED) {
	/* TODO: Send CDN */
	return;
    }

    /* Set up new incoming call */
    /* TODO: Include calling number */
    l2tp_session_lns_handle_incoming_call(tunnel, u16, dgram, "");
}

/**********************************************************************
* %FUNCTION: l2tp_tunnel_stop_all
* %ARGUMENTS:
*  reason -- reason for stopping tunnels
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Stops all tunnels
***********************************************************************/
void
l2tp_tunnel_stop_all(char const *reason)
{
    l2tp_tunnel *tunnel;
    void *cursor;

    /* Send StopCCN on all tunnels except those which are scheduled for
       destruction */
    for (tunnel = hash_start(&tunnels_by_my_id, &cursor);
	 tunnel;
	 tunnel = hash_next(&tunnels_by_my_id, &cursor)) {
	l2tp_tunnel_stop_tunnel(tunnel, reason);
    }

}

/**********************************************************************
* %FUNCTION: l2tp_tunnel_stop_tunnel
* %ARGUMENTS:
*  tunnel -- tunnel to stop
*  reason -- reason for stopping tunnel
* %RETURNS:
*  Nothing
* %DESCRIPTION:
*  Stops a tunnels (sends StopCCN)
***********************************************************************/
void
l2tp_tunnel_stop_tunnel(l2tp_tunnel *tunnel,
			char const *reason)
{
    /* Do not send StopCCN if we've received one already */
    if (tunnel->state != TUNNEL_RECEIVED_STOP_CCN &&
	tunnel->state != TUNNEL_SENT_STOP_CCN) {
	tunnel_send_StopCCN(tunnel, RESULT_SHUTTING_DOWN, 0, reason);
    }
}

/**********************************************************************
* %FUNCTION: l2tp_num_tunnels
* %ARGUMENTS:
*  None
* %RETURNS:
*  The number of L2TP tunnels
***********************************************************************/
int
l2tp_num_tunnels(void)
{
    return hash_num_entries(&tunnels_by_my_id);
}

/**********************************************************************
* %FUNCTION: l2tp_first_tunnel
* %ARGUMENTS:
*  cursor -- cursor for keeping track of where we are in interation
* %RETURNS:
*  First L2TP tunnel
***********************************************************************/
l2tp_tunnel *
l2tp_first_tunnel(void **cursor)
{
    return hash_start(&tunnels_by_my_id, cursor);
}


/**********************************************************************
* %FUNCTION: l2tp_next_tunnel
* %ARGUMENTS:
*  cursor -- cursor for keeping track of where we are in interation
* %RETURNS:
*  Next L2TP tunnel
***********************************************************************/
l2tp_tunnel *
l2tp_next_tunnel(void **cursor)
{
    return hash_next(&tunnels_by_my_id, cursor);
}

/**********************************************************************
* %FUNCTION: l2tp_tunnel_state_name
* %ARGUMENTS:
*  tunnel -- the tunnel
* %RETURNS:
*  The name of the tunnel's state
***********************************************************************/
char const *
l2tp_tunnel_state_name(l2tp_tunnel *tunnel)
{
    return state_names[tunnel->state];
}

/**********************************************************************
* %FUNCTION: l2tp_tunnel_first_session
* %ARGUMENTS:
*  tunnel -- the tunnel
*  cursor -- cursor for hash table iteration
* %RETURNS:
*  First session in tunnel
***********************************************************************/
l2tp_session *
l2tp_tunnel_first_session(l2tp_tunnel *tunnel, void **cursor)
{
    return hash_start(&tunnel->sessions_by_my_id, cursor);
}

/**********************************************************************
* %FUNCTION: l2tp_tunnel_next_session
* %ARGUMENTS:
*  tunnel -- the tunnel
*  cursor -- cursor for hash table iteration
* %RETURNS:
*  Next session in tunnel
***********************************************************************/
l2tp_session *
l2tp_tunnel_next_session(l2tp_tunnel *tunnel, void **cursor)
{
    return hash_next(&tunnel->sessions_by_my_id, cursor);
}

/* Route manipulation */
static int
route_ctrl(int ctrl, struct rtentry *rt)
{
	int s;

	/* Open a raw socket to the kernel */
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0 || ioctl(s, ctrl, rt) < 0)
		route_msg("%s: %s", __FUNCTION__, strerror(errno));
	else errno = 0;

	close(s);
	return errno;
}

static int
route_del(struct rtentry *rt)
{
	if (rt->rt_dev) {
		route_ctrl(SIOCDELRT, rt);
		free(rt->rt_dev), rt->rt_dev = NULL;
	}

	return 0;
}

static int
route_add(const struct in_addr inetaddr, struct rtentry *rt)
{
	char buf[256], dev[64], rdev[64];
	u_int32_t dest, mask, gateway, flags, bestmask = 0;
	int metric;

	FILE *f = fopen("/proc/net/route", "r");
	if (f == NULL) {
		route_msg("%s: /proc/net/route: %s", strerror(errno), __FUNCTION__);
		return -1;
	}

	rt->rt_gateway.sa_family = 0;

	while (fgets(buf, sizeof(buf), f))
	{
		if (sscanf(buf, "%63s %x %x %x %*s %*s %d %x", dev, &dest,
			&gateway, &flags, &metric, &mask) != 6)
			continue;
		if ((flags & RTF_UP) == (RTF_UP) && (inetaddr.s_addr & mask) == dest &&
#ifdef RTCONFIG_VPNC
		    (dest || strncmp(dev, "ppp", 3) || vpnc) /* avoid default via pppX to avoid on-demand loops*/)
#else
		    (dest || strncmp(dev, "ppp", 3)) /* avoid default via pppX to avoid on-demand loops*/)		
#endif
		{
			if ((mask | bestmask) == bestmask && rt->rt_gateway.sa_family)
				continue;
			bestmask = mask;

			sin_addr(&rt->rt_gateway).s_addr = gateway;
			rt->rt_gateway.sa_family = AF_INET;
			rt->rt_flags = flags;
			rt->rt_metric = metric;
			strncpy(rdev, dev, sizeof(rdev));

			if (mask == INADDR_BROADCAST)
				break;
		}
	}

	fclose(f);

	/* check for no route */
	if (rt->rt_gateway.sa_family != AF_INET) 
	{
		/* route_msg("%s: no route to host", __FUNCTION__); */
		return -1;
	}

	/* check for existing route to this host,
	 * add if missing based on the existing routes */
	if (rt->rt_flags & RTF_HOST)
	{
		/* route_msg("%s: not adding existing route", __FUNCTION__); */
		return -1;
	}

	sin_addr(&rt->rt_dst) = inetaddr;
	rt->rt_dst.sa_family = AF_INET;

	sin_addr(&rt->rt_genmask).s_addr = INADDR_BROADCAST;
	rt->rt_genmask.sa_family = AF_INET;

	rt->rt_flags &= RTF_GATEWAY;
	rt->rt_flags |= RTF_UP | RTF_HOST;

	rt->rt_metric++;
	rt->rt_dev = strdup(rdev);

	if (!rt->rt_dev)
	{
		/* route_msg("%s: no memory", __FUNCTION__); */
		return -1;
	}

	if (!route_ctrl(SIOCADDRT, rt))
		return 0;

	free(rt->rt_dev), rt->rt_dev = NULL;

	return -1;
}
