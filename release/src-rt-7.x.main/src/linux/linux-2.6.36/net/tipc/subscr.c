/*
 * net/tipc/subscr.c: TIPC network topology service
 *
 * Copyright (c) 2000-2006, Ericsson AB
 * Copyright (c) 2005-2007, Wind River Systems
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "core.h"
#include "dbg.h"
#include "name_table.h"
#include "port.h"
#include "ref.h"
#include "subscr.h"

/**
 * struct subscriber - TIPC network topology subscriber
 * @port_ref: object reference to server port connecting to subscriber
 * @lock: pointer to spinlock controlling access to subscriber's server port
 * @subscriber_list: adjacent subscribers in top. server's list of subscribers
 * @subscription_list: list of subscription objects for this subscriber
 */

struct subscriber {
	u32 port_ref;
	spinlock_t *lock;
	struct list_head subscriber_list;
	struct list_head subscription_list;
};

/**
 * struct top_srv - TIPC network topology subscription service
 * @user_ref: TIPC userid of subscription service
 * @setup_port: reference to TIPC port that handles subscription requests
 * @subscription_count: number of active subscriptions (not subscribers!)
 * @subscriber_list: list of ports subscribing to service
 * @lock: spinlock govering access to subscriber list
 */

struct top_srv {
	u32 user_ref;
	u32 setup_port;
	atomic_t subscription_count;
	struct list_head subscriber_list;
	spinlock_t lock;
};

static struct top_srv topsrv = { 0 };

/**
 * subscr_send_event - send a message containing a tipc_event to the subscriber
 *
 * Note: Must not hold subscriber's server port lock, since tipc_send() will
 *       try to take the lock if the message is rejected and returned!
 */

static void subscr_send_event(struct subscription *sub,
			      u32 found_lower,
			      u32 found_upper,
			      u32 event,
			      u32 port_ref,
			      u32 node)
{
	struct iovec msg_sect;

	msg_sect.iov_base = (void *)&sub->evt;
	msg_sect.iov_len = sizeof(struct tipc_event);

	sub->evt.event = htonl(event);
	sub->evt.found_lower = htonl(found_lower);
	sub->evt.found_upper = htonl(found_upper);
	sub->evt.port.ref = htonl(port_ref);
	sub->evt.port.node = htonl(node);
	tipc_send(sub->server_ref, 1, &msg_sect);
}

/**
 * tipc_subscr_overlap - test for subscription overlap with the given values
 *
 * Returns 1 if there is overlap, otherwise 0.
 */

int tipc_subscr_overlap(struct subscription *sub,
			u32 found_lower,
			u32 found_upper)

{
	if (found_lower < sub->seq.lower)
		found_lower = sub->seq.lower;
	if (found_upper > sub->seq.upper)
		found_upper = sub->seq.upper;
	if (found_lower > found_upper)
		return 0;
	return 1;
}

/**
 * tipc_subscr_report_overlap - issue event if there is subscription overlap
 *
 * Protected by nameseq.lock in name_table.c
 */

void tipc_subscr_report_overlap(struct subscription *sub,
				u32 found_lower,
				u32 found_upper,
				u32 event,
				u32 port_ref,
				u32 node,
				int must)
{
	if (!tipc_subscr_overlap(sub, found_lower, found_upper))
		return;
	if (!must && !(sub->filter & TIPC_SUB_PORTS))
		return;

	sub->event_cb(sub, found_lower, found_upper, event, port_ref, node);
}

/**
 * subscr_timeout - subscription timeout has occurred
 */

static void subscr_timeout(struct subscription *sub)
{
	struct port *server_port;

	/* Validate server port reference (in case subscriber is terminating) */

	server_port = tipc_port_lock(sub->server_ref);
	if (server_port == NULL)
		return;

	/* Validate timeout (in case subscription is being cancelled) */

	if (sub->timeout == TIPC_WAIT_FOREVER) {
		tipc_port_unlock(server_port);
		return;
	}

	/* Unlink subscription from name table */

	tipc_nametbl_unsubscribe(sub);

	/* Unlink subscription from subscriber */

	list_del(&sub->subscription_list);

	/* Release subscriber's server port */

	tipc_port_unlock(server_port);

	/* Notify subscriber of timeout */

	subscr_send_event(sub, sub->evt.s.seq.lower, sub->evt.s.seq.upper,
			  TIPC_SUBSCR_TIMEOUT, 0, 0);

	/* Now destroy subscription */

	k_term_timer(&sub->timer);
	kfree(sub);
	atomic_dec(&topsrv.subscription_count);
}

/**
 * subscr_del - delete a subscription within a subscription list
 *
 * Called with subscriber port locked.
 */

static void subscr_del(struct subscription *sub)
{
	tipc_nametbl_unsubscribe(sub);
	list_del(&sub->subscription_list);
	kfree(sub);
	atomic_dec(&topsrv.subscription_count);
}

/**
 * subscr_terminate - terminate communication with a subscriber
 *
 * Called with subscriber port locked.  Routine must temporarily release lock
 * to enable subscription timeout routine(s) to finish without deadlocking;
 * the lock is then reclaimed to allow caller to release it upon return.
 * (This should work even in the unlikely event some other thread creates
 * a new object reference in the interim that uses this lock; this routine will
 * simply wait for it to be released, then claim it.)
 */

static void subscr_terminate(struct subscriber *subscriber)
{
	u32 port_ref;
	struct subscription *sub;
	struct subscription *sub_temp;

	/* Invalidate subscriber reference */

	port_ref = subscriber->port_ref;
	subscriber->port_ref = 0;
	spin_unlock_bh(subscriber->lock);

	/* Sever connection to subscriber */

	tipc_shutdown(port_ref);
	tipc_deleteport(port_ref);

	/* Destroy any existing subscriptions for subscriber */

	list_for_each_entry_safe(sub, sub_temp, &subscriber->subscription_list,
				 subscription_list) {
		if (sub->timeout != TIPC_WAIT_FOREVER) {
			k_cancel_timer(&sub->timer);
			k_term_timer(&sub->timer);
		}
		dbg("Term: Removing sub %u,%u,%u from subscriber %x list\n",
		    sub->seq.type, sub->seq.lower, sub->seq.upper, subscriber);
		subscr_del(sub);
	}

	/* Remove subscriber from topology server's subscriber list */

	spin_lock_bh(&topsrv.lock);
	list_del(&subscriber->subscriber_list);
	spin_unlock_bh(&topsrv.lock);

	/* Reclaim subscriber lock */

	spin_lock_bh(subscriber->lock);

	/* Now destroy subscriber */

	kfree(subscriber);
}

/**
 * subscr_cancel - handle subscription cancellation request
 *
 * Called with subscriber port locked.  Routine must temporarily release lock
 * to enable the subscription timeout routine to finish without deadlocking;
 * the lock is then reclaimed to allow caller to release it upon return.
 *
 * Note that fields of 's' use subscriber's endianness!
 */

static void subscr_cancel(struct tipc_subscr *s,
			  struct subscriber *subscriber)
{
	struct subscription *sub;
	struct subscription *sub_temp;
	__u32 type, lower, upper, timeout, filter;
	int found = 0;

	/* Find first matching subscription, exit if not found */

	type = ntohl(s->seq.type);
	lower = ntohl(s->seq.lower);
	upper = ntohl(s->seq.upper);
	timeout = ntohl(s->timeout);
	filter = ntohl(s->filter) & ~TIPC_SUB_CANCEL;

	list_for_each_entry_safe(sub, sub_temp, &subscriber->subscription_list,
				 subscription_list) {
			if ((type == sub->seq.type) &&
			    (lower == sub->seq.lower) &&
			    (upper == sub->seq.upper) &&
			    (timeout == sub->timeout) &&
                            (filter == sub->filter) &&
                             !memcmp(s->usr_handle,sub->evt.s.usr_handle,
				     sizeof(s->usr_handle)) ){
				found = 1;
				break;
			}
	}
	if (!found)
		return;

	/* Cancel subscription timer (if used), then delete subscription */

	if (sub->timeout != TIPC_WAIT_FOREVER) {
		sub->timeout = TIPC_WAIT_FOREVER;
		spin_unlock_bh(subscriber->lock);
		k_cancel_timer(&sub->timer);
		k_term_timer(&sub->timer);
		spin_lock_bh(subscriber->lock);
	}
	dbg("Cancel: removing sub %u,%u,%u from subscriber %p list\n",
	    sub->seq.type, sub->seq.lower, sub->seq.upper, subscriber);
	subscr_del(sub);
}

/**
 * subscr_subscribe - create subscription for subscriber
 *
 * Called with subscriber port locked.
 */

static struct subscription *subscr_subscribe(struct tipc_subscr *s,
					     struct subscriber *subscriber)
{
	struct subscription *sub;

	/* Detect & process a subscription cancellation request */

	if (ntohl(s->filter) & TIPC_SUB_CANCEL) {
		subscr_cancel(s, subscriber);
		return NULL;
	}

	/* Refuse subscription if global limit exceeded */

	if (atomic_read(&topsrv.subscription_count) >= tipc_max_subscriptions) {
		warn("Subscription rejected, subscription limit reached (%u)\n",
		     tipc_max_subscriptions);
		subscr_terminate(subscriber);
		return NULL;
	}

	/* Allocate subscription object */

	sub = kmalloc(sizeof(*sub), GFP_ATOMIC);
	if (!sub) {
		warn("Subscription rejected, no memory\n");
		subscr_terminate(subscriber);
		return NULL;
	}

	/* Initialize subscription object */

	sub->seq.type = ntohl(s->seq.type);
	sub->seq.lower = ntohl(s->seq.lower);
	sub->seq.upper = ntohl(s->seq.upper);
	sub->timeout = ntohl(s->timeout);
	sub->filter = ntohl(s->filter);
	if ((sub->filter && (sub->filter != TIPC_SUB_PORTS)) ||
	    (sub->seq.lower > sub->seq.upper)) {
		warn("Subscription rejected, illegal request\n");
		kfree(sub);
		subscr_terminate(subscriber);
		return NULL;
	}
	sub->event_cb = subscr_send_event;
	INIT_LIST_HEAD(&sub->nameseq_list);
	list_add(&sub->subscription_list, &subscriber->subscription_list);
	sub->server_ref = subscriber->port_ref;
	memcpy(&sub->evt.s, s, sizeof(struct tipc_subscr));
	atomic_inc(&topsrv.subscription_count);
	if (sub->timeout != TIPC_WAIT_FOREVER) {
		k_init_timer(&sub->timer,
			     (Handler)subscr_timeout, (unsigned long)sub);
		k_start_timer(&sub->timer, sub->timeout);
	}

	return sub;
}

/**
 * subscr_conn_shutdown_event - handle termination request from subscriber
 *
 * Called with subscriber's server port unlocked.
 */

static void subscr_conn_shutdown_event(void *usr_handle,
				       u32 port_ref,
				       struct sk_buff **buf,
				       unsigned char const *data,
				       unsigned int size,
				       int reason)
{
	struct subscriber *subscriber = usr_handle;
	spinlock_t *subscriber_lock;

	if (tipc_port_lock(port_ref) == NULL)
		return;

	subscriber_lock = subscriber->lock;
	subscr_terminate(subscriber);
	spin_unlock_bh(subscriber_lock);
}

/**
 * subscr_conn_msg_event - handle new subscription request from subscriber
 *
 * Called with subscriber's server port unlocked.
 */

static void subscr_conn_msg_event(void *usr_handle,
				  u32 port_ref,
				  struct sk_buff **buf,
				  const unchar *data,
				  u32 size)
{
	struct subscriber *subscriber = usr_handle;
	spinlock_t *subscriber_lock;
	struct subscription *sub;

	/*
	 * Lock subscriber's server port (& make a local copy of lock pointer,
	 * in case subscriber is deleted while processing subscription request)
	 */

	if (tipc_port_lock(port_ref) == NULL)
		return;

	subscriber_lock = subscriber->lock;

	if (size != sizeof(struct tipc_subscr)) {
		subscr_terminate(subscriber);
		spin_unlock_bh(subscriber_lock);
	} else {
		sub = subscr_subscribe((struct tipc_subscr *)data, subscriber);
		spin_unlock_bh(subscriber_lock);
		if (sub != NULL) {

			/*
			 * We must release the server port lock before adding a
			 * subscription to the name table since TIPC needs to be
			 * able to (re)acquire the port lock if an event message
			 * issued by the subscription process is rejected and
			 * returned.  The subscription cannot be deleted while
			 * it is being added to the name table because:
			 * a) the single-threading of the native API port code
			 *    ensures the subscription cannot be cancelled and
			 *    the subscriber connection cannot be broken, and
			 * b) the name table lock ensures the subscription
			 *    timeout code cannot delete the subscription,
			 * so the subscription object is still protected.
			 */

			tipc_nametbl_subscribe(sub);
		}
	}
}

/**
 * subscr_named_msg_event - handle request to establish a new subscriber
 */

static void subscr_named_msg_event(void *usr_handle,
				   u32 port_ref,
				   struct sk_buff **buf,
				   const unchar *data,
				   u32 size,
				   u32 importance,
				   struct tipc_portid const *orig,
				   struct tipc_name_seq const *dest)
{
	static struct iovec msg_sect = {NULL, 0};

	struct subscriber *subscriber;
	u32 server_port_ref;

	/* Create subscriber object */

	subscriber = kzalloc(sizeof(struct subscriber), GFP_ATOMIC);
	if (subscriber == NULL) {
		warn("Subscriber rejected, no memory\n");
		return;
	}
	INIT_LIST_HEAD(&subscriber->subscription_list);
	INIT_LIST_HEAD(&subscriber->subscriber_list);

	/* Create server port & establish connection to subscriber */

	tipc_createport(topsrv.user_ref,
			subscriber,
			importance,
			NULL,
			NULL,
			subscr_conn_shutdown_event,
			NULL,
			NULL,
			subscr_conn_msg_event,
			NULL,
			&subscriber->port_ref);
	if (subscriber->port_ref == 0) {
		warn("Subscriber rejected, unable to create port\n");
		kfree(subscriber);
		return;
	}
	tipc_connect2port(subscriber->port_ref, orig);

	/* Lock server port (& save lock address for future use) */

	subscriber->lock = tipc_port_lock(subscriber->port_ref)->publ.lock;

	/* Add subscriber to topology server's subscriber list */

	spin_lock_bh(&topsrv.lock);
	list_add(&subscriber->subscriber_list, &topsrv.subscriber_list);
	spin_unlock_bh(&topsrv.lock);

	/* Unlock server port */

	server_port_ref = subscriber->port_ref;
	spin_unlock_bh(subscriber->lock);

	/* Send an ACK- to complete connection handshaking */

	tipc_send(server_port_ref, 1, &msg_sect);

	/* Handle optional subscription request */

	if (size != 0) {
		subscr_conn_msg_event(subscriber, server_port_ref,
				      buf, data, size);
	}
}

int tipc_subscr_start(void)
{
	struct tipc_name_seq seq = {TIPC_TOP_SRV, TIPC_TOP_SRV, TIPC_TOP_SRV};
	int res = -1;

	memset(&topsrv, 0, sizeof (topsrv));
	spin_lock_init(&topsrv.lock);
	INIT_LIST_HEAD(&topsrv.subscriber_list);

	spin_lock_bh(&topsrv.lock);
	res = tipc_attach(&topsrv.user_ref, NULL, NULL);
	if (res) {
		spin_unlock_bh(&topsrv.lock);
		return res;
	}

	res = tipc_createport(topsrv.user_ref,
			      NULL,
			      TIPC_CRITICAL_IMPORTANCE,
			      NULL,
			      NULL,
			      NULL,
			      NULL,
			      subscr_named_msg_event,
			      NULL,
			      NULL,
			      &topsrv.setup_port);
	if (res)
		goto failed;

	res = tipc_nametbl_publish_rsv(topsrv.setup_port, TIPC_NODE_SCOPE, &seq);
	if (res)
		goto failed;

	spin_unlock_bh(&topsrv.lock);
	return 0;

failed:
	err("Failed to create subscription service\n");
	tipc_detach(topsrv.user_ref);
	topsrv.user_ref = 0;
	spin_unlock_bh(&topsrv.lock);
	return res;
}

void tipc_subscr_stop(void)
{
	struct subscriber *subscriber;
	struct subscriber *subscriber_temp;
	spinlock_t *subscriber_lock;

	if (topsrv.user_ref) {
		tipc_deleteport(topsrv.setup_port);
		list_for_each_entry_safe(subscriber, subscriber_temp,
					 &topsrv.subscriber_list,
					 subscriber_list) {
			subscriber_lock = subscriber->lock;
			spin_lock_bh(subscriber_lock);
			subscr_terminate(subscriber);
			spin_unlock_bh(subscriber_lock);
		}
		tipc_detach(topsrv.user_ref);
		topsrv.user_ref = 0;
	}
}


int tipc_ispublished(struct tipc_name const *name)
{
	u32 domain = 0;

	return(tipc_nametbl_translate(name->type, name->instance,&domain) != 0);
}
