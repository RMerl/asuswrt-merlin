/* SCTP kernel implementation
 * (C) Copyright IBM Corp. 2001, 2004
 * Copyright (c) 1999-2000 Cisco, Inc.
 * Copyright (c) 1999-2001 Motorola, Inc.
 * Copyright (c) 2001 Intel Corp.
 * Copyright (c) 2001 Nokia, Inc.
 * Copyright (c) 2001 La Monte H.P. Yarroll
 *
 * These are the definitions needed for the sctp_ulpevent type.  The
 * sctp_ulpevent type is used to carry information from the state machine
 * upwards to the ULP.
 *
 * This file is part of the SCTP kernel implementation
 *
 * This SCTP implementation is free software;
 * you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This SCTP implementation is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *                 ************************
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU CC; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Please send any bug reports or fixes you make to the
 * email address(es):
 *    lksctp developers <lksctp-developers@lists.sourceforge.net>
 *
 * Or submit a bug report through the following website:
 *    http://www.sf.net/projects/lksctp
 *
 * Written or modified by:
 *   Jon Grimm             <jgrimm@us.ibm.com>
 *   La Monte H.P. Yarroll <piggy@acm.org>
 *   Karl Knutson          <karl@athena.chicago.il.us>
 *   Sridhar Samudrala     <sri@us.ibm.com>
 *
 * Any bugs reported given to us we will try to fix... any fixes shared will
 * be incorporated into the next SCTP release.
 */

#ifndef __sctp_ulpevent_h__
#define __sctp_ulpevent_h__

/* A structure to carry information to the ULP (e.g. Sockets API) */
/* Warning: This sits inside an skb.cb[] area.  Be very careful of
 * growing this structure as it is at the maximum limit now.
 */
struct sctp_ulpevent {
	struct sctp_association *asoc;
	__u16 stream;
	__u16 ssn;
	__u16 flags;
	__u32 ppid;
	__u32 tsn;
	__u32 cumtsn;
	int msg_flags;
	int iif;
	unsigned int rmem_len;
};

/* Retrieve the skb this event sits inside of. */
static inline struct sk_buff *sctp_event2skb(const struct sctp_ulpevent *ev)
{
	return container_of((void *)ev, struct sk_buff, cb);
}

/* Retrieve & cast the event sitting inside the skb. */
static inline struct sctp_ulpevent *sctp_skb2event(struct sk_buff *skb)
{
	return (struct sctp_ulpevent *)skb->cb;
}

void sctp_ulpevent_free(struct sctp_ulpevent *);
int sctp_ulpevent_is_notification(const struct sctp_ulpevent *);
void sctp_queue_purge_ulpevents(struct sk_buff_head *list);

struct sctp_ulpevent *sctp_ulpevent_make_assoc_change(
	const struct sctp_association *asoc,
	__u16 flags,
	__u16 state,
	__u16 error,
	__u16 outbound,
	__u16 inbound,
	struct sctp_chunk *chunk,
	gfp_t gfp);

struct sctp_ulpevent *sctp_ulpevent_make_peer_addr_change(
	const struct sctp_association *asoc,
	const struct sockaddr_storage *aaddr,
	int flags,
	int state,
	int error,
	gfp_t gfp);

struct sctp_ulpevent *sctp_ulpevent_make_remote_error(
	const struct sctp_association *asoc,
	struct sctp_chunk *chunk,
	__u16 flags,
	gfp_t gfp);
struct sctp_ulpevent *sctp_ulpevent_make_send_failed(
	const struct sctp_association *asoc,
	struct sctp_chunk *chunk,
	__u16 flags,
	__u32 error,
	gfp_t gfp);

struct sctp_ulpevent *sctp_ulpevent_make_shutdown_event(
	const struct sctp_association *asoc,
	__u16 flags,
	gfp_t gfp);

struct sctp_ulpevent *sctp_ulpevent_make_pdapi(
	const struct sctp_association *asoc,
	__u32 indication, gfp_t gfp);

struct sctp_ulpevent *sctp_ulpevent_make_adaptation_indication(
	const struct sctp_association *asoc, gfp_t gfp);

struct sctp_ulpevent *sctp_ulpevent_make_rcvmsg(struct sctp_association *asoc,
	struct sctp_chunk *chunk,
	gfp_t gfp);

struct sctp_ulpevent *sctp_ulpevent_make_authkey(
	const struct sctp_association *asoc, __u16 key_id,
	__u32 indication, gfp_t gfp);

void sctp_ulpevent_read_sndrcvinfo(const struct sctp_ulpevent *event,
	struct msghdr *);
__u16 sctp_ulpevent_get_notification_type(const struct sctp_ulpevent *event);

/* Is this event type enabled? */
static inline int sctp_ulpevent_type_enabled(__u16 sn_type,
					     struct sctp_event_subscribe *mask)
{
	char *amask = (char *) mask;
	return amask[sn_type - SCTP_SN_TYPE_BASE];
}

/* Given an event subscription, is this event enabled? */
static inline int sctp_ulpevent_is_enabled(const struct sctp_ulpevent *event,
					   struct sctp_event_subscribe *mask)
{
	__u16 sn_type;
	int enabled = 1;

	if (sctp_ulpevent_is_notification(event)) {
		sn_type = sctp_ulpevent_get_notification_type(event);
		enabled = sctp_ulpevent_type_enabled(sn_type, mask);
	}
	return enabled;
}

#endif /* __sctp_ulpevent_h__ */







