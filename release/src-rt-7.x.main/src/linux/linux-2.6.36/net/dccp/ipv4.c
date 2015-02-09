/*
 *  net/dccp/ipv4.c
 *
 *  An implementation of the DCCP protocol
 *  Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/dccp.h>
#include <linux/icmp.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/random.h>

#include <net/icmp.h>
#include <net/inet_common.h>
#include <net/inet_hashtables.h>
#include <net/inet_sock.h>
#include <net/protocol.h>
#include <net/sock.h>
#include <net/timewait_sock.h>
#include <net/tcp_states.h>
#include <net/xfrm.h>

#include "ackvec.h"
#include "ccid.h"
#include "dccp.h"
#include "feat.h"

/*
 * The per-net dccp.v4_ctl_sk socket is used for responding to
 * the Out-of-the-blue (OOTB) packets. A control sock will be created
 * for this socket at the initialization time.
 */

int dccp_v4_connect(struct sock *sk, struct sockaddr *uaddr, int addr_len)
{
	struct inet_sock *inet = inet_sk(sk);
	struct dccp_sock *dp = dccp_sk(sk);
	const struct sockaddr_in *usin = (struct sockaddr_in *)uaddr;
	struct rtable *rt;
	__be32 daddr, nexthop;
	int tmp;
	int err;

	dp->dccps_role = DCCP_ROLE_CLIENT;

	if (addr_len < sizeof(struct sockaddr_in))
		return -EINVAL;

	if (usin->sin_family != AF_INET)
		return -EAFNOSUPPORT;

	nexthop = daddr = usin->sin_addr.s_addr;
	if (inet->opt != NULL && inet->opt->srr) {
		if (daddr == 0)
			return -EINVAL;
		nexthop = inet->opt->faddr;
	}

	tmp = ip_route_connect(&rt, nexthop, inet->inet_saddr,
			       RT_CONN_FLAGS(sk), sk->sk_bound_dev_if,
			       IPPROTO_DCCP,
			       inet->inet_sport, usin->sin_port, sk, 1);
	if (tmp < 0)
		return tmp;

	if (rt->rt_flags & (RTCF_MULTICAST | RTCF_BROADCAST)) {
		ip_rt_put(rt);
		return -ENETUNREACH;
	}

	if (inet->opt == NULL || !inet->opt->srr)
		daddr = rt->rt_dst;

	if (inet->inet_saddr == 0)
		inet->inet_saddr = rt->rt_src;
	inet->inet_rcv_saddr = inet->inet_saddr;

	inet->inet_dport = usin->sin_port;
	inet->inet_daddr = daddr;

	inet_csk(sk)->icsk_ext_hdr_len = 0;
	if (inet->opt != NULL)
		inet_csk(sk)->icsk_ext_hdr_len = inet->opt->optlen;
	/*
	 * Socket identity is still unknown (sport may be zero).
	 * However we set state to DCCP_REQUESTING and not releasing socket
	 * lock select source port, enter ourselves into the hash tables and
	 * complete initialization after this.
	 */
	dccp_set_state(sk, DCCP_REQUESTING);
	err = inet_hash_connect(&dccp_death_row, sk);
	if (err != 0)
		goto failure;

	err = ip_route_newports(&rt, IPPROTO_DCCP, inet->inet_sport,
				inet->inet_dport, sk);
	if (err != 0)
		goto failure;

	/* OK, now commit destination to socket.  */
	sk_setup_caps(sk, &rt->dst);

	dp->dccps_iss = secure_dccp_sequence_number(inet->inet_saddr,
						    inet->inet_daddr,
						    inet->inet_sport,
						    inet->inet_dport);
	inet->inet_id = dp->dccps_iss ^ jiffies;

	err = dccp_connect(sk);
	rt = NULL;
	if (err != 0)
		goto failure;
out:
	return err;
failure:
	/*
	 * This unhashes the socket and releases the local port, if necessary.
	 */
	dccp_set_state(sk, DCCP_CLOSED);
	ip_rt_put(rt);
	sk->sk_route_caps = 0;
	inet->inet_dport = 0;
	goto out;
}

EXPORT_SYMBOL_GPL(dccp_v4_connect);

/*
 * This routine does path mtu discovery as defined in RFC1191.
 */
static inline void dccp_do_pmtu_discovery(struct sock *sk,
					  const struct iphdr *iph,
					  u32 mtu)
{
	struct dst_entry *dst;
	const struct inet_sock *inet = inet_sk(sk);
	const struct dccp_sock *dp = dccp_sk(sk);

	/* We are not interested in DCCP_LISTEN and request_socks (RESPONSEs
	 * send out by Linux are always < 576bytes so they should go through
	 * unfragmented).
	 */
	if (sk->sk_state == DCCP_LISTEN)
		return;

	/* We don't check in the destentry if pmtu discovery is forbidden
	 * on this route. We just assume that no packet_to_big packets
	 * are send back when pmtu discovery is not active.
	 * There is a small race when the user changes this flag in the
	 * route, but I think that's acceptable.
	 */
	if ((dst = __sk_dst_check(sk, 0)) == NULL)
		return;

	dst->ops->update_pmtu(dst, mtu);

	/* Something is about to be wrong... Remember soft error
	 * for the case, if this connection will not able to recover.
	 */
	if (mtu < dst_mtu(dst) && ip_dont_fragment(sk, dst))
		sk->sk_err_soft = EMSGSIZE;

	mtu = dst_mtu(dst);

	if (inet->pmtudisc != IP_PMTUDISC_DONT &&
	    inet_csk(sk)->icsk_pmtu_cookie > mtu) {
		dccp_sync_mss(sk, mtu);

		/*
		 * From RFC 4340, sec. 14.1:
		 *
		 *	DCCP-Sync packets are the best choice for upward
		 *	probing, since DCCP-Sync probes do not risk application
		 *	data loss.
		 */
		dccp_send_sync(sk, dp->dccps_gsr, DCCP_PKT_SYNC);
	} /* else let the usual retransmit timer handle it */
}

/*
 * This routine is called by the ICMP module when it gets some sort of error
 * condition. If err < 0 then the socket should be closed and the error
 * returned to the user. If err > 0 it's just the icmp type << 8 | icmp code.
 * After adjustment header points to the first 8 bytes of the tcp header. We
 * need to find the appropriate port.
 *
 * The locking strategy used here is very "optimistic". When someone else
 * accesses the socket the ICMP is just dropped and for some paths there is no
 * check at all. A more general error queue to queue errors for later handling
 * is probably better.
 */
static void dccp_v4_err(struct sk_buff *skb, u32 info)
{
	const struct iphdr *iph = (struct iphdr *)skb->data;
	const u8 offset = iph->ihl << 2;
	const struct dccp_hdr *dh = (struct dccp_hdr *)(skb->data + offset);
	struct dccp_sock *dp;
	struct inet_sock *inet;
	const int type = icmp_hdr(skb)->type;
	const int code = icmp_hdr(skb)->code;
	struct sock *sk;
	__u64 seq;
	int err;
	struct net *net = dev_net(skb->dev);

	if (skb->len < offset + sizeof(*dh) ||
	    skb->len < offset + __dccp_basic_hdr_len(dh)) {
		ICMP_INC_STATS_BH(net, ICMP_MIB_INERRORS);
		return;
	}

	sk = inet_lookup(net, &dccp_hashinfo,
			iph->daddr, dh->dccph_dport,
			iph->saddr, dh->dccph_sport, inet_iif(skb));
	if (sk == NULL) {
		ICMP_INC_STATS_BH(net, ICMP_MIB_INERRORS);
		return;
	}

	if (sk->sk_state == DCCP_TIME_WAIT) {
		inet_twsk_put(inet_twsk(sk));
		return;
	}

	bh_lock_sock(sk);
	/* If too many ICMPs get dropped on busy
	 * servers this needs to be solved differently.
	 */
	if (sock_owned_by_user(sk))
		NET_INC_STATS_BH(net, LINUX_MIB_LOCKDROPPEDICMPS);

	if (sk->sk_state == DCCP_CLOSED)
		goto out;

	dp = dccp_sk(sk);
	seq = dccp_hdr_seq(dh);
	if ((1 << sk->sk_state) & ~(DCCPF_REQUESTING | DCCPF_LISTEN) &&
	    !between48(seq, dp->dccps_awl, dp->dccps_awh)) {
		NET_INC_STATS_BH(net, LINUX_MIB_OUTOFWINDOWICMPS);
		goto out;
	}

	switch (type) {
	case ICMP_SOURCE_QUENCH:
		/* Just silently ignore these. */
		goto out;
	case ICMP_PARAMETERPROB:
		err = EPROTO;
		break;
	case ICMP_DEST_UNREACH:
		if (code > NR_ICMP_UNREACH)
			goto out;

		if (code == ICMP_FRAG_NEEDED) { /* PMTU discovery (RFC1191) */
			if (!sock_owned_by_user(sk))
				dccp_do_pmtu_discovery(sk, iph, info);
			goto out;
		}

		err = icmp_err_convert[code].errno;
		break;
	case ICMP_TIME_EXCEEDED:
		err = EHOSTUNREACH;
		break;
	default:
		goto out;
	}

	switch (sk->sk_state) {
		struct request_sock *req , **prev;
	case DCCP_LISTEN:
		if (sock_owned_by_user(sk))
			goto out;
		req = inet_csk_search_req(sk, &prev, dh->dccph_dport,
					  iph->daddr, iph->saddr);
		if (!req)
			goto out;

		/*
		 * ICMPs are not backlogged, hence we cannot get an established
		 * socket here.
		 */
		WARN_ON(req->sk);

		if (seq != dccp_rsk(req)->dreq_iss) {
			NET_INC_STATS_BH(net, LINUX_MIB_OUTOFWINDOWICMPS);
			goto out;
		}
		/*
		 * Still in RESPOND, just remove it silently.
		 * There is no good way to pass the error to the newly
		 * created socket, and POSIX does not want network
		 * errors returned from accept().
		 */
		inet_csk_reqsk_queue_drop(sk, req, prev);
		goto out;

	case DCCP_REQUESTING:
	case DCCP_RESPOND:
		if (!sock_owned_by_user(sk)) {
			DCCP_INC_STATS_BH(DCCP_MIB_ATTEMPTFAILS);
			sk->sk_err = err;

			sk->sk_error_report(sk);

			dccp_done(sk);
		} else
			sk->sk_err_soft = err;
		goto out;
	}

	/* If we've already connected we will keep trying
	 * until we time out, or the user gives up.
	 *
	 * rfc1122 4.2.3.9 allows to consider as hard errors
	 * only PROTO_UNREACH and PORT_UNREACH (well, FRAG_FAILED too,
	 * but it is obsoleted by pmtu discovery).
	 *
	 * Note, that in modern internet, where routing is unreliable
	 * and in each dark corner broken firewalls sit, sending random
	 * errors ordered by their masters even this two messages finally lose
	 * their original sense (even Linux sends invalid PORT_UNREACHs)
	 *
	 * Now we are in compliance with RFCs.
	 *							--ANK (980905)
	 */

	inet = inet_sk(sk);
	if (!sock_owned_by_user(sk) && inet->recverr) {
		sk->sk_err = err;
		sk->sk_error_report(sk);
	} else /* Only an error on timeout */
		sk->sk_err_soft = err;
out:
	bh_unlock_sock(sk);
	sock_put(sk);
}

static inline __sum16 dccp_v4_csum_finish(struct sk_buff *skb,
				      __be32 src, __be32 dst)
{
	return csum_tcpudp_magic(src, dst, skb->len, IPPROTO_DCCP, skb->csum);
}

void dccp_v4_send_check(struct sock *sk, struct sk_buff *skb)
{
	const struct inet_sock *inet = inet_sk(sk);
	struct dccp_hdr *dh = dccp_hdr(skb);

	dccp_csum_outgoing(skb);
	dh->dccph_checksum = dccp_v4_csum_finish(skb,
						 inet->inet_saddr,
						 inet->inet_daddr);
}

EXPORT_SYMBOL_GPL(dccp_v4_send_check);

static inline u64 dccp_v4_init_sequence(const struct sk_buff *skb)
{
	return secure_dccp_sequence_number(ip_hdr(skb)->daddr,
					   ip_hdr(skb)->saddr,
					   dccp_hdr(skb)->dccph_dport,
					   dccp_hdr(skb)->dccph_sport);
}

/*
 * The three way handshake has completed - we got a valid ACK or DATAACK -
 * now create the new socket.
 *
 * This is the equivalent of TCP's tcp_v4_syn_recv_sock
 */
struct sock *dccp_v4_request_recv_sock(struct sock *sk, struct sk_buff *skb,
				       struct request_sock *req,
				       struct dst_entry *dst)
{
	struct inet_request_sock *ireq;
	struct inet_sock *newinet;
	struct sock *newsk;

	if (sk_acceptq_is_full(sk))
		goto exit_overflow;

	if (dst == NULL && (dst = inet_csk_route_req(sk, req)) == NULL)
		goto exit;

	newsk = dccp_create_openreq_child(sk, req, skb);
	if (newsk == NULL)
		goto exit;

	sk_setup_caps(newsk, dst);

	newinet		   = inet_sk(newsk);
	ireq		   = inet_rsk(req);
	newinet->inet_daddr	= ireq->rmt_addr;
	newinet->inet_rcv_saddr = ireq->loc_addr;
	newinet->inet_saddr	= ireq->loc_addr;
	newinet->opt	   = ireq->opt;
	ireq->opt	   = NULL;
	newinet->mc_index  = inet_iif(skb);
	newinet->mc_ttl	   = ip_hdr(skb)->ttl;
	newinet->inet_id   = jiffies;

	dccp_sync_mss(newsk, dst_mtu(dst));

	__inet_hash_nolisten(newsk, NULL);
	__inet_inherit_port(sk, newsk);

	return newsk;

exit_overflow:
	NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_LISTENOVERFLOWS);
exit:
	NET_INC_STATS_BH(sock_net(sk), LINUX_MIB_LISTENDROPS);
	dst_release(dst);
	return NULL;
}

EXPORT_SYMBOL_GPL(dccp_v4_request_recv_sock);

static struct sock *dccp_v4_hnd_req(struct sock *sk, struct sk_buff *skb)
{
	const struct dccp_hdr *dh = dccp_hdr(skb);
	const struct iphdr *iph = ip_hdr(skb);
	struct sock *nsk;
	struct request_sock **prev;
	/* Find possible connection requests. */
	struct request_sock *req = inet_csk_search_req(sk, &prev,
						       dh->dccph_sport,
						       iph->saddr, iph->daddr);
	if (req != NULL)
		return dccp_check_req(sk, skb, req, prev);

	nsk = inet_lookup_established(sock_net(sk), &dccp_hashinfo,
				      iph->saddr, dh->dccph_sport,
				      iph->daddr, dh->dccph_dport,
				      inet_iif(skb));
	if (nsk != NULL) {
		if (nsk->sk_state != DCCP_TIME_WAIT) {
			bh_lock_sock(nsk);
			return nsk;
		}
		inet_twsk_put(inet_twsk(nsk));
		return NULL;
	}

	return sk;
}

static struct dst_entry* dccp_v4_route_skb(struct net *net, struct sock *sk,
					   struct sk_buff *skb)
{
	struct rtable *rt;
	struct flowi fl = { .oif = skb_rtable(skb)->rt_iif,
			    .nl_u = { .ip4_u =
				      { .daddr = ip_hdr(skb)->saddr,
					.saddr = ip_hdr(skb)->daddr,
					.tos = RT_CONN_FLAGS(sk) } },
			    .proto = sk->sk_protocol,
			    .uli_u = { .ports =
				       { .sport = dccp_hdr(skb)->dccph_dport,
					 .dport = dccp_hdr(skb)->dccph_sport }
				     }
			  };

	security_skb_classify_flow(skb, &fl);
	if (ip_route_output_flow(net, &rt, &fl, sk, 0)) {
		IP_INC_STATS_BH(net, IPSTATS_MIB_OUTNOROUTES);
		return NULL;
	}

	return &rt->dst;
}

static int dccp_v4_send_response(struct sock *sk, struct request_sock *req,
				 struct request_values *rv_unused)
{
	int err = -1;
	struct sk_buff *skb;
	struct dst_entry *dst;

	dst = inet_csk_route_req(sk, req);
	if (dst == NULL)
		goto out;

	skb = dccp_make_response(sk, dst, req);
	if (skb != NULL) {
		const struct inet_request_sock *ireq = inet_rsk(req);
		struct dccp_hdr *dh = dccp_hdr(skb);

		dh->dccph_checksum = dccp_v4_csum_finish(skb, ireq->loc_addr,
							      ireq->rmt_addr);
		err = ip_build_and_send_pkt(skb, sk, ireq->loc_addr,
					    ireq->rmt_addr,
					    ireq->opt);
		err = net_xmit_eval(err);
	}

out:
	dst_release(dst);
	return err;
}

static void dccp_v4_ctl_send_reset(struct sock *sk, struct sk_buff *rxskb)
{
	int err;
	const struct iphdr *rxiph;
	struct sk_buff *skb;
	struct dst_entry *dst;
	struct net *net = dev_net(skb_dst(rxskb)->dev);
	struct sock *ctl_sk = net->dccp.v4_ctl_sk;

	/* Never send a reset in response to a reset. */
	if (dccp_hdr(rxskb)->dccph_type == DCCP_PKT_RESET)
		return;

	if (skb_rtable(rxskb)->rt_type != RTN_LOCAL)
		return;

	dst = dccp_v4_route_skb(net, ctl_sk, rxskb);
	if (dst == NULL)
		return;

	skb = dccp_ctl_make_reset(ctl_sk, rxskb);
	if (skb == NULL)
		goto out;

	rxiph = ip_hdr(rxskb);
	dccp_hdr(skb)->dccph_checksum = dccp_v4_csum_finish(skb, rxiph->saddr,
								 rxiph->daddr);
	skb_dst_set(skb, dst_clone(dst));

	bh_lock_sock(ctl_sk);
	err = ip_build_and_send_pkt(skb, ctl_sk,
				    rxiph->daddr, rxiph->saddr, NULL);
	bh_unlock_sock(ctl_sk);

	if (net_xmit_eval(err) == 0) {
		DCCP_INC_STATS_BH(DCCP_MIB_OUTSEGS);
		DCCP_INC_STATS_BH(DCCP_MIB_OUTRSTS);
	}
out:
	 dst_release(dst);
}

static void dccp_v4_reqsk_destructor(struct request_sock *req)
{
	dccp_feat_list_purge(&dccp_rsk(req)->dreq_featneg);
	kfree(inet_rsk(req)->opt);
}

static struct request_sock_ops dccp_request_sock_ops __read_mostly = {
	.family		= PF_INET,
	.obj_size	= sizeof(struct dccp_request_sock),
	.rtx_syn_ack	= dccp_v4_send_response,
	.send_ack	= dccp_reqsk_send_ack,
	.destructor	= dccp_v4_reqsk_destructor,
	.send_reset	= dccp_v4_ctl_send_reset,
};

int dccp_v4_conn_request(struct sock *sk, struct sk_buff *skb)
{
	struct inet_request_sock *ireq;
	struct request_sock *req;
	struct dccp_request_sock *dreq;
	const __be32 service = dccp_hdr_request(skb)->dccph_req_service;
	struct dccp_skb_cb *dcb = DCCP_SKB_CB(skb);

	/* Never answer to DCCP_PKT_REQUESTs send to broadcast or multicast */
	if (skb_rtable(skb)->rt_flags & (RTCF_BROADCAST | RTCF_MULTICAST))
		return 0;	/* discard, don't send a reset here */

	if (dccp_bad_service_code(sk, service)) {
		dcb->dccpd_reset_code = DCCP_RESET_CODE_BAD_SERVICE_CODE;
		goto drop;
	}
	/*
	 * TW buckets are converted to open requests without
	 * limitations, they conserve resources and peer is
	 * evidently real one.
	 */
	dcb->dccpd_reset_code = DCCP_RESET_CODE_TOO_BUSY;
	if (inet_csk_reqsk_queue_is_full(sk))
		goto drop;

	/*
	 * Accept backlog is full. If we have already queued enough
	 * of warm entries in syn queue, drop request. It is better than
	 * clogging syn queue with openreqs with exponentially increasing
	 * timeout.
	 */
	if (sk_acceptq_is_full(sk) && inet_csk_reqsk_queue_young(sk) > 1)
		goto drop;

	req = inet_reqsk_alloc(&dccp_request_sock_ops);
	if (req == NULL)
		goto drop;

	if (dccp_reqsk_init(req, dccp_sk(sk), skb))
		goto drop_and_free;

	dreq = dccp_rsk(req);
	if (dccp_parse_options(sk, dreq, skb))
		goto drop_and_free;

	if (security_inet_conn_request(sk, skb, req))
		goto drop_and_free;

	ireq = inet_rsk(req);
	ireq->loc_addr = ip_hdr(skb)->daddr;
	ireq->rmt_addr = ip_hdr(skb)->saddr;

	/*
	 * Step 3: Process LISTEN state
	 *
	 * Set S.ISR, S.GSR, S.SWL, S.SWH from packet or Init Cookie
	 *
	 * In fact we defer setting S.GSR, S.SWL, S.SWH to
	 * dccp_create_openreq_child.
	 */
	dreq->dreq_isr	   = dcb->dccpd_seq;
	dreq->dreq_iss	   = dccp_v4_init_sequence(skb);
	dreq->dreq_service = service;

	if (dccp_v4_send_response(sk, req, NULL))
		goto drop_and_free;

	inet_csk_reqsk_queue_hash_add(sk, req, DCCP_TIMEOUT_INIT);
	return 0;

drop_and_free:
	reqsk_free(req);
drop:
	DCCP_INC_STATS_BH(DCCP_MIB_ATTEMPTFAILS);
	return -1;
}

EXPORT_SYMBOL_GPL(dccp_v4_conn_request);

int dccp_v4_do_rcv(struct sock *sk, struct sk_buff *skb)
{
	struct dccp_hdr *dh = dccp_hdr(skb);

	if (sk->sk_state == DCCP_OPEN) { /* Fast path */
		if (dccp_rcv_established(sk, skb, dh, skb->len))
			goto reset;
		return 0;
	}

	/*
	 *  Step 3: Process LISTEN state
	 *	 If P.type == Request or P contains a valid Init Cookie option,
	 *	      (* Must scan the packet's options to check for Init
	 *		 Cookies.  Only Init Cookies are processed here,
	 *		 however; other options are processed in Step 8.  This
	 *		 scan need only be performed if the endpoint uses Init
	 *		 Cookies *)
	 *	      (* Generate a new socket and switch to that socket *)
	 *	      Set S := new socket for this port pair
	 *	      S.state = RESPOND
	 *	      Choose S.ISS (initial seqno) or set from Init Cookies
	 *	      Initialize S.GAR := S.ISS
	 *	      Set S.ISR, S.GSR, S.SWL, S.SWH from packet or Init Cookies
	 *	      Continue with S.state == RESPOND
	 *	      (* A Response packet will be generated in Step 11 *)
	 *	 Otherwise,
	 *	      Generate Reset(No Connection) unless P.type == Reset
	 *	      Drop packet and return
	 *
	 * NOTE: the check for the packet types is done in
	 *	 dccp_rcv_state_process
	 */
	if (sk->sk_state == DCCP_LISTEN) {
		struct sock *nsk = dccp_v4_hnd_req(sk, skb);

		if (nsk == NULL)
			goto discard;

		if (nsk != sk) {
			if (dccp_child_process(sk, nsk, skb))
				goto reset;
			return 0;
		}
	}

	if (dccp_rcv_state_process(sk, skb, dh, skb->len))
		goto reset;
	return 0;

reset:
	dccp_v4_ctl_send_reset(sk, skb);
discard:
	kfree_skb(skb);
	return 0;
}

EXPORT_SYMBOL_GPL(dccp_v4_do_rcv);

/**
 *	dccp_invalid_packet  -  check for malformed packets
 *	Implements RFC 4340, 8.5:  Step 1: Check header basics
 *	Packets that fail these checks are ignored and do not receive Resets.
 */
int dccp_invalid_packet(struct sk_buff *skb)
{
	const struct dccp_hdr *dh;
	unsigned int cscov;

	if (skb->pkt_type != PACKET_HOST)
		return 1;

	/* If the packet is shorter than 12 bytes, drop packet and return */
	if (!pskb_may_pull(skb, sizeof(struct dccp_hdr))) {
		DCCP_WARN("pskb_may_pull failed\n");
		return 1;
	}

	dh = dccp_hdr(skb);

	/* If P.type is not understood, drop packet and return */
	if (dh->dccph_type >= DCCP_PKT_INVALID) {
		DCCP_WARN("invalid packet type\n");
		return 1;
	}

	/*
	 * If P.Data Offset is too small for packet type, drop packet and return
	 */
	if (dh->dccph_doff < dccp_hdr_len(skb) / sizeof(u32)) {
		DCCP_WARN("P.Data Offset(%u) too small\n", dh->dccph_doff);
		return 1;
	}
	/*
	 * If P.Data Offset is too too large for packet, drop packet and return
	 */
	if (!pskb_may_pull(skb, dh->dccph_doff * sizeof(u32))) {
		DCCP_WARN("P.Data Offset(%u) too large\n", dh->dccph_doff);
		return 1;
	}

	/*
	 * If P.type is not Data, Ack, or DataAck and P.X == 0 (the packet
	 * has short sequence numbers), drop packet and return
	 */
	if ((dh->dccph_type < DCCP_PKT_DATA    ||
	    dh->dccph_type > DCCP_PKT_DATAACK) && dh->dccph_x == 0)  {
		DCCP_WARN("P.type (%s) not Data || [Data]Ack, while P.X == 0\n",
			  dccp_packet_name(dh->dccph_type));
		return 1;
	}

	/*
	 * If P.CsCov is too large for the packet size, drop packet and return.
	 * This must come _before_ checksumming (not as RFC 4340 suggests).
	 */
	cscov = dccp_csum_coverage(skb);
	if (cscov > skb->len) {
		DCCP_WARN("P.CsCov %u exceeds packet length %d\n",
			  dh->dccph_cscov, skb->len);
		return 1;
	}

	/* If header checksum is incorrect, drop packet and return.
	 * (This step is completed in the AF-dependent functions.) */
	skb->csum = skb_checksum(skb, 0, cscov, 0);

	return 0;
}

EXPORT_SYMBOL_GPL(dccp_invalid_packet);

/* this is called when real data arrives */
static int dccp_v4_rcv(struct sk_buff *skb)
{
	const struct dccp_hdr *dh;
	const struct iphdr *iph;
	struct sock *sk;
	int min_cov;

	/* Step 1: Check header basics */

	if (dccp_invalid_packet(skb))
		goto discard_it;

	iph = ip_hdr(skb);
	/* Step 1: If header checksum is incorrect, drop packet and return */
	if (dccp_v4_csum_finish(skb, iph->saddr, iph->daddr)) {
		DCCP_WARN("dropped packet with invalid checksum\n");
		goto discard_it;
	}

	dh = dccp_hdr(skb);

	DCCP_SKB_CB(skb)->dccpd_seq  = dccp_hdr_seq(dh);
	DCCP_SKB_CB(skb)->dccpd_type = dh->dccph_type;

	dccp_pr_debug("%8.8s src=%pI4@%-5d dst=%pI4@%-5d seq=%llu",
		      dccp_packet_name(dh->dccph_type),
		      &iph->saddr, ntohs(dh->dccph_sport),
		      &iph->daddr, ntohs(dh->dccph_dport),
		      (unsigned long long) DCCP_SKB_CB(skb)->dccpd_seq);

	if (dccp_packet_without_ack(skb)) {
		DCCP_SKB_CB(skb)->dccpd_ack_seq = DCCP_PKT_WITHOUT_ACK_SEQ;
		dccp_pr_debug_cat("\n");
	} else {
		DCCP_SKB_CB(skb)->dccpd_ack_seq = dccp_hdr_ack_seq(skb);
		dccp_pr_debug_cat(", ack=%llu\n", (unsigned long long)
				  DCCP_SKB_CB(skb)->dccpd_ack_seq);
	}

	/* Step 2:
	 *	Look up flow ID in table and get corresponding socket */
	sk = __inet_lookup_skb(&dccp_hashinfo, skb,
			       dh->dccph_sport, dh->dccph_dport);
	/*
	 * Step 2:
	 *	If no socket ...
	 */
	if (sk == NULL) {
		dccp_pr_debug("failed to look up flow ID in table and "
			      "get corresponding socket\n");
		goto no_dccp_socket;
	}

	/*
	 * Step 2:
	 *	... or S.state == TIMEWAIT,
	 *		Generate Reset(No Connection) unless P.type == Reset
	 *		Drop packet and return
	 */
	if (sk->sk_state == DCCP_TIME_WAIT) {
		dccp_pr_debug("sk->sk_state == DCCP_TIME_WAIT: do_time_wait\n");
		inet_twsk_put(inet_twsk(sk));
		goto no_dccp_socket;
	}

	/*
	 * RFC 4340, sec. 9.2.1: Minimum Checksum Coverage
	 *	o if MinCsCov = 0, only packets with CsCov = 0 are accepted
	 *	o if MinCsCov > 0, also accept packets with CsCov >= MinCsCov
	 */
	min_cov = dccp_sk(sk)->dccps_pcrlen;
	if (dh->dccph_cscov && (min_cov == 0 || dh->dccph_cscov < min_cov))  {
		dccp_pr_debug("Packet CsCov %d does not satisfy MinCsCov %d\n",
			      dh->dccph_cscov, min_cov);
		goto discard_and_relse;
	}

	if (!xfrm4_policy_check(sk, XFRM_POLICY_IN, skb))
		goto discard_and_relse;
	nf_reset(skb);

	return sk_receive_skb(sk, skb, 1);

no_dccp_socket:
	if (!xfrm4_policy_check(NULL, XFRM_POLICY_IN, skb))
		goto discard_it;
	/*
	 * Step 2:
	 *	If no socket ...
	 *		Generate Reset(No Connection) unless P.type == Reset
	 *		Drop packet and return
	 */
	if (dh->dccph_type != DCCP_PKT_RESET) {
		DCCP_SKB_CB(skb)->dccpd_reset_code =
					DCCP_RESET_CODE_NO_CONNECTION;
		dccp_v4_ctl_send_reset(sk, skb);
	}

discard_it:
	kfree_skb(skb);
	return 0;

discard_and_relse:
	sock_put(sk);
	goto discard_it;
}

static const struct inet_connection_sock_af_ops dccp_ipv4_af_ops = {
	.queue_xmit	   = ip_queue_xmit,
	.send_check	   = dccp_v4_send_check,
	.rebuild_header	   = inet_sk_rebuild_header,
	.conn_request	   = dccp_v4_conn_request,
	.syn_recv_sock	   = dccp_v4_request_recv_sock,
	.net_header_len	   = sizeof(struct iphdr),
	.setsockopt	   = ip_setsockopt,
	.getsockopt	   = ip_getsockopt,
	.addr2sockaddr	   = inet_csk_addr2sockaddr,
	.sockaddr_len	   = sizeof(struct sockaddr_in),
	.bind_conflict	   = inet_csk_bind_conflict,
#ifdef CONFIG_COMPAT
	.compat_setsockopt = compat_ip_setsockopt,
	.compat_getsockopt = compat_ip_getsockopt,
#endif
};

static int dccp_v4_init_sock(struct sock *sk)
{
	static __u8 dccp_v4_ctl_sock_initialized;
	int err = dccp_init_sock(sk, dccp_v4_ctl_sock_initialized);

	if (err == 0) {
		if (unlikely(!dccp_v4_ctl_sock_initialized))
			dccp_v4_ctl_sock_initialized = 1;
		inet_csk(sk)->icsk_af_ops = &dccp_ipv4_af_ops;
	}

	return err;
}

static struct timewait_sock_ops dccp_timewait_sock_ops = {
	.twsk_obj_size	= sizeof(struct inet_timewait_sock),
};

static struct proto dccp_v4_prot = {
	.name			= "DCCP",
	.owner			= THIS_MODULE,
	.close			= dccp_close,
	.connect		= dccp_v4_connect,
	.disconnect		= dccp_disconnect,
	.ioctl			= dccp_ioctl,
	.init			= dccp_v4_init_sock,
	.setsockopt		= dccp_setsockopt,
	.getsockopt		= dccp_getsockopt,
	.sendmsg		= dccp_sendmsg,
	.recvmsg		= dccp_recvmsg,
	.backlog_rcv		= dccp_v4_do_rcv,
	.hash			= inet_hash,
	.unhash			= inet_unhash,
	.accept			= inet_csk_accept,
	.get_port		= inet_csk_get_port,
	.shutdown		= dccp_shutdown,
	.destroy		= dccp_destroy_sock,
	.orphan_count		= &dccp_orphan_count,
	.max_header		= MAX_DCCP_HEADER,
	.obj_size		= sizeof(struct dccp_sock),
	.slab_flags		= SLAB_DESTROY_BY_RCU,
	.rsk_prot		= &dccp_request_sock_ops,
	.twsk_prot		= &dccp_timewait_sock_ops,
	.h.hashinfo		= &dccp_hashinfo,
#ifdef CONFIG_COMPAT
	.compat_setsockopt	= compat_dccp_setsockopt,
	.compat_getsockopt	= compat_dccp_getsockopt,
#endif
};

static const struct net_protocol dccp_v4_protocol = {
	.handler	= dccp_v4_rcv,
	.err_handler	= dccp_v4_err,
	.no_policy	= 1,
	.netns_ok	= 1,
};

static const struct proto_ops inet_dccp_ops = {
	.family		   = PF_INET,
	.owner		   = THIS_MODULE,
	.release	   = inet_release,
	.bind		   = inet_bind,
	.connect	   = inet_stream_connect,
	.socketpair	   = sock_no_socketpair,
	.accept		   = inet_accept,
	.getname	   = inet_getname,
	.poll		   = dccp_poll,
	.ioctl		   = inet_ioctl,
	.listen		   = inet_dccp_listen,
	.shutdown	   = inet_shutdown,
	.setsockopt	   = sock_common_setsockopt,
	.getsockopt	   = sock_common_getsockopt,
	.sendmsg	   = inet_sendmsg,
	.recvmsg	   = sock_common_recvmsg,
	.mmap		   = sock_no_mmap,
	.sendpage	   = sock_no_sendpage,
#ifdef CONFIG_COMPAT
	.compat_setsockopt = compat_sock_common_setsockopt,
	.compat_getsockopt = compat_sock_common_getsockopt,
#endif
};

static struct inet_protosw dccp_v4_protosw = {
	.type		= SOCK_DCCP,
	.protocol	= IPPROTO_DCCP,
	.prot		= &dccp_v4_prot,
	.ops		= &inet_dccp_ops,
	.no_check	= 0,
	.flags		= INET_PROTOSW_ICSK,
};

static int __net_init dccp_v4_init_net(struct net *net)
{
	if (dccp_hashinfo.bhash == NULL)
		return -ESOCKTNOSUPPORT;

	return inet_ctl_sock_create(&net->dccp.v4_ctl_sk, PF_INET,
				    SOCK_DCCP, IPPROTO_DCCP, net);
}

static void __net_exit dccp_v4_exit_net(struct net *net)
{
	inet_ctl_sock_destroy(net->dccp.v4_ctl_sk);
}

static struct pernet_operations dccp_v4_ops = {
	.init	= dccp_v4_init_net,
	.exit	= dccp_v4_exit_net,
};

static int __init dccp_v4_init(void)
{
	int err = proto_register(&dccp_v4_prot, 1);

	if (err != 0)
		goto out;

	err = inet_add_protocol(&dccp_v4_protocol, IPPROTO_DCCP);
	if (err != 0)
		goto out_proto_unregister;

	inet_register_protosw(&dccp_v4_protosw);

	err = register_pernet_subsys(&dccp_v4_ops);
	if (err)
		goto out_destroy_ctl_sock;
out:
	return err;
out_destroy_ctl_sock:
	inet_unregister_protosw(&dccp_v4_protosw);
	inet_del_protocol(&dccp_v4_protocol, IPPROTO_DCCP);
out_proto_unregister:
	proto_unregister(&dccp_v4_prot);
	goto out;
}

static void __exit dccp_v4_exit(void)
{
	unregister_pernet_subsys(&dccp_v4_ops);
	inet_unregister_protosw(&dccp_v4_protosw);
	inet_del_protocol(&dccp_v4_protocol, IPPROTO_DCCP);
	proto_unregister(&dccp_v4_prot);
}

module_init(dccp_v4_init);
module_exit(dccp_v4_exit);

/*
 * __stringify doesn't likes enums, so use SOCK_DCCP (6) and IPPROTO_DCCP (33)
 * values directly, Also cover the case where the protocol is not specified,
 * i.e. net-pf-PF_INET-proto-0-type-SOCK_DCCP
 */
MODULE_ALIAS_NET_PF_PROTO_TYPE(PF_INET, 33, 6);
MODULE_ALIAS_NET_PF_PROTO_TYPE(PF_INET, 0, 6);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Arnaldo Carvalho de Melo <acme@mandriva.com>");
MODULE_DESCRIPTION("DCCP - Datagram Congestion Controlled Protocol");
