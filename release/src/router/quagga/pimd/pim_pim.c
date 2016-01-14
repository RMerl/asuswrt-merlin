/*
  PIM for Quagga
  Copyright (C) 2008  Everton da Silva Marques

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING; if not, write to the
  Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
  MA 02110-1301 USA
  
  $QuaggaId: $Format:%an, %ai, %h$ $
*/

#include <zebra.h>

#include "log.h"
#include "thread.h"
#include "memory.h"

#include "pimd.h"
#include "pim_pim.h"
#include "pim_time.h"
#include "pim_iface.h"
#include "pim_sock.h"
#include "pim_str.h"
#include "pim_util.h"
#include "pim_tlv.h"
#include "pim_neighbor.h"
#include "pim_hello.h"
#include "pim_join.h"
#include "pim_assert.h"
#include "pim_msg.h"
#include "pim_rand.h"

static int on_pim_hello_send(struct thread *t);
static int pim_hello_send(struct interface *ifp,
			  uint16_t holdtime);

static void sock_close(struct interface *ifp)
{
  struct pim_interface *pim_ifp = ifp->info;

  if (PIM_DEBUG_PIM_TRACE) {
    if (pim_ifp->t_pim_sock_read) {
      zlog_debug("Cancelling READ event for PIM socket fd=%d on interface %s",
		 pim_ifp->pim_sock_fd,
		 ifp->name);
    }
  }
  THREAD_OFF(pim_ifp->t_pim_sock_read);

  if (PIM_DEBUG_PIM_TRACE) {
    if (pim_ifp->t_pim_hello_timer) {
      zlog_debug("Cancelling PIM hello timer for interface %s",
		 ifp->name);
    }
  }
  THREAD_OFF(pim_ifp->t_pim_hello_timer);

  if (PIM_DEBUG_PIM_TRACE) {
    zlog_debug("Deleting PIM socket fd=%d on interface %s",
	       pim_ifp->pim_sock_fd, ifp->name);
  }

  if (close(pim_ifp->pim_sock_fd)) {
    zlog_warn("Failure closing PIM socket fd=%d on interface %s: errno=%d: %s",
	      pim_ifp->pim_sock_fd, ifp->name,
	      errno, safe_strerror(errno));
  }
  
  pim_ifp->pim_sock_fd = -1;
  pim_ifp->pim_sock_creation = 0;

  zassert(pim_ifp->pim_sock_fd < 0);
  zassert(!pim_ifp->t_pim_sock_read);
  zassert(!pim_ifp->t_pim_hello_timer);
  zassert(!pim_ifp->pim_sock_creation);
}

void pim_sock_delete(struct interface *ifp, const char *delete_message)
{
  zlog_info("PIM INTERFACE DOWN: on interface %s: %s",
	    ifp->name, delete_message);

  if (!ifp->info) {
    zlog_err("%s: %s: but PIM not enabled on interface %s (!)",
	     __PRETTY_FUNCTION__, delete_message, ifp->name);
    return;
  }

  /*
    RFC 4601: 4.3.1.  Sending Hello Messages
    
    Before an interface goes down or changes primary IP address, a Hello
    message with a zero HoldTime should be sent immediately (with the
    old IP address if the IP address changed).
  */
  pim_hello_send(ifp, 0 /* zero-sec holdtime */);

  pim_neighbor_delete_all(ifp, delete_message);

  sock_close(ifp);
}

int pim_pim_packet(struct interface *ifp, uint8_t *buf, size_t len)
{
  struct ip *ip_hdr;
  size_t ip_hlen; /* ip header length in bytes */
  char src_str[100];
  char dst_str[100];
  uint8_t *pim_msg;
  int pim_msg_len;
  uint8_t pim_version;
  uint8_t pim_type;
  uint16_t pim_checksum; /* received checksum */
  uint16_t checksum;     /* computed checksum */
  struct pim_neighbor *neigh;

  if (!ifp->info) {
    zlog_warn("%s: PIM not enabled on interface %s",
	      __PRETTY_FUNCTION__, ifp->name);
    return -1;
  }
    
  if (len < sizeof(*ip_hdr)) {
    zlog_warn("PIM packet size=%zu shorter than minimum=%zu",
	      len, sizeof(*ip_hdr));
    return -1;
  }

  ip_hdr = (struct ip *) buf;

  pim_inet4_dump("<src?>", ip_hdr->ip_src, src_str, sizeof(src_str));
  pim_inet4_dump("<dst?>", ip_hdr->ip_dst, dst_str, sizeof(dst_str));

  ip_hlen = ip_hdr->ip_hl << 2; /* ip_hl gives length in 4-byte words */

  if (PIM_DEBUG_PIM_PACKETS) {
    zlog_debug("Recv IP packet from %s to %s on %s: size=%zu ip_header_size=%zu ip_proto=%d",
	       src_str, dst_str, ifp->name, len, ip_hlen, ip_hdr->ip_p);
  }

  if (ip_hdr->ip_p != PIM_IP_PROTO_PIM) {
    zlog_warn("IP packet protocol=%d is not PIM=%d",
	      ip_hdr->ip_p, PIM_IP_PROTO_PIM);
    return -1;
  }

  if (ip_hlen < PIM_IP_HEADER_MIN_LEN) {
    zlog_warn("IP packet header size=%zu shorter than minimum=%d",
	      ip_hlen, PIM_IP_HEADER_MIN_LEN);
    return -1;
  }
  if (ip_hlen > PIM_IP_HEADER_MAX_LEN) {
    zlog_warn("IP packet header size=%zu greater than maximum=%d",
	      ip_hlen, PIM_IP_HEADER_MAX_LEN);
    return -1;
  }

  pim_msg = buf + ip_hlen;
  pim_msg_len = len - ip_hlen;

  if (PIM_DEBUG_PIM_PACKETDUMP_RECV) {
    pim_pkt_dump(__PRETTY_FUNCTION__, pim_msg, pim_msg_len);
  }

  if (pim_msg_len < PIM_PIM_MIN_LEN) {
    zlog_warn("PIM message size=%d shorter than minimum=%d",
	      pim_msg_len, PIM_PIM_MIN_LEN);
    return -1;
  }

  pim_version = PIM_MSG_HDR_GET_VERSION(pim_msg);
  pim_type    = PIM_MSG_HDR_GET_TYPE(pim_msg);

  if (pim_version != PIM_PROTO_VERSION) {
    zlog_warn("Ignoring PIM pkt from %s with unsupported version: %d",
	      ifp->name, pim_version);
    return -1;
  }

  /* save received checksum */
  pim_checksum = PIM_MSG_HDR_GET_CHECKSUM(pim_msg);

  /* for computing checksum */
  *(uint16_t *) PIM_MSG_HDR_OFFSET_CHECKSUM(pim_msg) = 0;

  checksum = in_cksum(pim_msg, pim_msg_len);
  if (checksum != pim_checksum) {
    zlog_warn("Ignoring PIM pkt from %s with invalid checksum: received=%x calculated=%x",
	      ifp->name, pim_checksum, checksum);
    return -1;
  }

  if (PIM_DEBUG_PIM_PACKETS) {
    zlog_debug("Recv PIM packet from %s to %s on %s: ttl=%d pim_version=%d pim_type=%d pim_msg_size=%d checksum=%x",
	       src_str, dst_str, ifp->name, ip_hdr->ip_ttl,
	       pim_version, pim_type, pim_msg_len, checksum);
  }

  if (pim_type == PIM_MSG_TYPE_HELLO) {
    int result = pim_hello_recv(ifp,
                 ip_hdr->ip_src,
                 pim_msg + PIM_MSG_HEADER_LEN,
                 pim_msg_len - PIM_MSG_HEADER_LEN);
    if (!result) {
      pim_if_dr_election(ifp); /* PIM Hello message is received */
    }
    return result;
  }

  neigh = pim_neighbor_find(ifp, ip_hdr->ip_src);
  if (!neigh) {
    zlog_warn("%s %s: non-hello PIM message type=%d from non-neighbor %s on %s",
	      __FILE__, __PRETTY_FUNCTION__,
	      pim_type, src_str, ifp->name);
    return -1;
  }

  switch (pim_type) {
  case PIM_MSG_TYPE_JOIN_PRUNE:
    return pim_joinprune_recv(ifp, neigh,
			      ip_hdr->ip_src,
			      pim_msg + PIM_MSG_HEADER_LEN,
			      pim_msg_len - PIM_MSG_HEADER_LEN);
  case PIM_MSG_TYPE_ASSERT:
    return pim_assert_recv(ifp, neigh,
			   ip_hdr->ip_src,
			   pim_msg + PIM_MSG_HEADER_LEN,
			   pim_msg_len - PIM_MSG_HEADER_LEN);
  default:
    zlog_warn("%s %s: unsupported PIM message type=%d from %s on %s",
	      __FILE__, __PRETTY_FUNCTION__,
	      pim_type, src_str, ifp->name);
  }

  return -1;
}

static void pim_sock_read_on(struct interface *ifp);

static int pim_sock_read(struct thread *t)
{
  struct interface *ifp;
  struct pim_interface *pim_ifp;
  int fd;
  struct sockaddr_in from;
  struct sockaddr_in to;
  socklen_t fromlen = sizeof(from);
  socklen_t tolen = sizeof(to);
  uint8_t buf[PIM_PIM_BUFSIZE_READ];
  int len;
  int ifindex = -1;
  int result = -1; /* defaults to bad */

  zassert(t);

  ifp = THREAD_ARG(t);
  zassert(ifp);

  fd = THREAD_FD(t);

  pim_ifp = ifp->info;
  zassert(pim_ifp);

  zassert(fd == pim_ifp->pim_sock_fd);

  len = pim_socket_recvfromto(fd, buf, sizeof(buf),
			      &from, &fromlen,
			      &to, &tolen,
			      &ifindex);
  if (len < 0) {
    zlog_warn("Failure receiving IP PIM packet on fd=%d: errno=%d: %s",
	      fd, errno, safe_strerror(errno));
    goto done;
  }

  if (PIM_DEBUG_PIM_PACKETS) {
    char from_str[100];
    char to_str[100];
    
    if (!inet_ntop(AF_INET, &from.sin_addr, from_str, sizeof(from_str)))
      sprintf(from_str, "<from?>");
    if (!inet_ntop(AF_INET, &to.sin_addr, to_str, sizeof(to_str)))
      sprintf(to_str, "<to?>");
    
    zlog_debug("Recv IP PIM pkt size=%d from %s to %s on fd=%d on ifindex=%d (sock_ifindex=%d)",
	       len, from_str, to_str, fd, ifindex, ifp->ifindex);
  }

  if (PIM_DEBUG_PIM_PACKETDUMP_RECV) {
    pim_pkt_dump(__PRETTY_FUNCTION__, buf, len);
  }

#ifdef PIM_CHECK_RECV_IFINDEX_SANITY
  /* ifindex sanity check */
  if (ifindex != (int) ifp->ifindex) {
    char from_str[100];
    char to_str[100];
    struct interface *recv_ifp;

    if (!inet_ntop(AF_INET, &from.sin_addr, from_str , sizeof(from_str)))
      sprintf(from_str, "<from?>");
    if (!inet_ntop(AF_INET, &to.sin_addr, to_str , sizeof(to_str)))
      sprintf(to_str, "<to?>");

    recv_ifp = if_lookup_by_index(ifindex);
    if (recv_ifp) {
      zassert(ifindex == (int) recv_ifp->ifindex);
    }

#ifdef PIM_REPORT_RECV_IFINDEX_MISMATCH
    zlog_warn("Interface mismatch: recv PIM pkt from %s to %s on fd=%d: recv_ifindex=%d (%s) sock_ifindex=%d (%s)",
	      from_str, to_str, fd,
	      ifindex, recv_ifp ? recv_ifp->name : "<if-notfound>",
	      ifp->ifindex, ifp->name);
#endif
    goto done;
  }
#endif

  int fail = pim_pim_packet(ifp, buf, len);
  if (fail) {
    zlog_warn("%s: pim_pim_packet() return=%d",
              __PRETTY_FUNCTION__, fail);
    goto done;
  }

  result = 0; /* good */

 done:
  pim_sock_read_on(ifp);

  if (result) {
    ++pim_ifp->pim_ifstat_hello_recvfail;
  }

  return result;
}

static void pim_sock_read_on(struct interface *ifp)
{
  struct pim_interface *pim_ifp;

  zassert(ifp);
  zassert(ifp->info);

  pim_ifp = ifp->info;

  if (PIM_DEBUG_PIM_TRACE) {
    zlog_debug("Scheduling READ event on PIM socket fd=%d",
	       pim_ifp->pim_sock_fd);
  }
  pim_ifp->t_pim_sock_read = 0;
  zassert(!pim_ifp->t_pim_sock_read);
  THREAD_READ_ON(master, pim_ifp->t_pim_sock_read, pim_sock_read, ifp,
		 pim_ifp->pim_sock_fd);
}

static int pim_sock_open(struct in_addr ifaddr, int ifindex)
{
  int fd;

  fd = pim_socket_mcast(IPPROTO_PIM, ifaddr, 0 /* loop=false */);
  if (fd < 0)
    return -1;

  if (pim_socket_join(fd, qpim_all_pim_routers_addr, ifaddr, ifindex)) {
    return -2;
  }

  return fd;
}

void pim_ifstat_reset(struct interface *ifp)
{
  struct pim_interface *pim_ifp;

  zassert(ifp);

  pim_ifp = ifp->info;
  if (!pim_ifp) {
    return;
  }

  pim_ifp->pim_ifstat_start          = pim_time_monotonic_sec();
  pim_ifp->pim_ifstat_hello_sent     = 0;
  pim_ifp->pim_ifstat_hello_sendfail = 0;
  pim_ifp->pim_ifstat_hello_recv     = 0;
  pim_ifp->pim_ifstat_hello_recvfail = 0;
}

void pim_sock_reset(struct interface *ifp)
{
  struct pim_interface *pim_ifp;

  zassert(ifp);
  zassert(ifp->info);

  pim_ifp = ifp->info;

  pim_ifp->primary_address = pim_find_primary_addr(ifp);

  pim_ifp->pim_sock_fd       = -1;
  pim_ifp->pim_sock_creation = 0;
  pim_ifp->t_pim_sock_read   = 0;

  pim_ifp->t_pim_hello_timer          = 0;
  pim_ifp->pim_hello_period           = PIM_DEFAULT_HELLO_PERIOD;
  pim_ifp->pim_default_holdtime       = -1; /* unset: means 3.5 * pim_hello_period */
  pim_ifp->pim_triggered_hello_delay  = PIM_DEFAULT_TRIGGERED_HELLO_DELAY;
  pim_ifp->pim_dr_priority            = PIM_DEFAULT_DR_PRIORITY;
  pim_ifp->pim_propagation_delay_msec = PIM_DEFAULT_PROPAGATION_DELAY_MSEC;
  pim_ifp->pim_override_interval_msec = PIM_DEFAULT_OVERRIDE_INTERVAL_MSEC;
  if (PIM_DEFAULT_CAN_DISABLE_JOIN_SUPPRESSION) {
    PIM_IF_DO_PIM_CAN_DISABLE_JOIN_SUPRESSION(pim_ifp->options);
  }
  else {
    PIM_IF_DONT_PIM_CAN_DISABLE_JOIN_SUPRESSION(pim_ifp->options);
  }

  /* neighbors without lan_delay */
  pim_ifp->pim_number_of_nonlandelay_neighbors = 0;
  pim_ifp->pim_neighbors_highest_propagation_delay_msec = 0;
  pim_ifp->pim_neighbors_highest_override_interval_msec = 0;

  /* DR Election */
  pim_ifp->pim_dr_election_last          = 0; /* timestamp */
  pim_ifp->pim_dr_election_count         = 0;
  pim_ifp->pim_dr_election_changes       = 0;
  pim_ifp->pim_dr_num_nondrpri_neighbors = 0; /* neighbors without dr_pri */
  pim_ifp->pim_dr_addr                   = pim_ifp->primary_address;

  pim_ifstat_reset(ifp);
}

int pim_msg_send(int fd,
		 struct in_addr dst,
		 uint8_t *pim_msg,
		 int pim_msg_size,
		 const char *ifname)
{
  ssize_t            sent;
  struct sockaddr_in to;
  socklen_t          tolen;

  if (PIM_DEBUG_PIM_PACKETS) {
    char dst_str[100];
    pim_inet4_dump("<dst?>", dst, dst_str, sizeof(dst_str));
    zlog_debug("%s: to %s on %s: msg_size=%d checksum=%x",
	       __PRETTY_FUNCTION__,
	       dst_str, ifname, pim_msg_size,
	       *(uint16_t *) PIM_MSG_HDR_OFFSET_CHECKSUM(pim_msg));
  }

#if 0
  memset(&to, 0, sizeof(to));
#endif
  to.sin_family = AF_INET;
  to.sin_addr = dst;
#if 0
  to.sin_port = htons(0);
#endif
  tolen = sizeof(to);

  if (PIM_DEBUG_PIM_PACKETDUMP_SEND) {
    pim_pkt_dump(__PRETTY_FUNCTION__, pim_msg, pim_msg_size);
  }

  sent = sendto(fd, pim_msg, pim_msg_size, MSG_DONTWAIT, &to, tolen);
  if (sent != (ssize_t) pim_msg_size) {
    int e = errno;
    char dst_str[100];
    pim_inet4_dump("<dst?>", dst, dst_str, sizeof(dst_str));
    if (sent < 0) {
      zlog_warn("%s: sendto() failure to %s on %s: fd=%d msg_size=%d: errno=%d: %s",
		__PRETTY_FUNCTION__,
		dst_str, ifname, fd, pim_msg_size,
		e, safe_strerror(e));
    }
    else {
      zlog_warn("%s: sendto() partial to %s on %s: fd=%d msg_size=%d: sent=%zd",
		__PRETTY_FUNCTION__,
		dst_str, ifname, fd,
		pim_msg_size, sent);
    }
    return -1;
  }

  return 0;
}

static int hello_send(struct interface *ifp,
		      uint16_t holdtime)
{
  uint8_t pim_msg[PIM_PIM_BUFSIZE_WRITE];
  struct pim_interface *pim_ifp;
  int pim_tlv_size;
  int pim_msg_size;

  pim_ifp = ifp->info;

  if (PIM_DEBUG_PIM_PACKETS || PIM_DEBUG_PIM_HELLO) {
    char dst_str[100];
    pim_inet4_dump("<dst?>", qpim_all_pim_routers_addr, dst_str, sizeof(dst_str));
    zlog_debug("%s: to %s on %s: holdt=%u prop_d=%u overr_i=%u dis_join_supp=%d dr_prio=%u gen_id=%08x addrs=%d",
	       __PRETTY_FUNCTION__,
	       dst_str, ifp->name,
	       holdtime,
	       pim_ifp->pim_propagation_delay_msec, pim_ifp->pim_override_interval_msec,
	       PIM_IF_TEST_PIM_CAN_DISABLE_JOIN_SUPRESSION(pim_ifp->options),
	       pim_ifp->pim_dr_priority, pim_ifp->pim_generation_id,
	       listcount(ifp->connected));
  }

  pim_tlv_size = pim_hello_build_tlv(ifp->name,
				     pim_msg + PIM_PIM_MIN_LEN,
				     sizeof(pim_msg) - PIM_PIM_MIN_LEN,
				     holdtime,
				     pim_ifp->pim_dr_priority,
				     pim_ifp->pim_generation_id,
				     pim_ifp->pim_propagation_delay_msec,
				     pim_ifp->pim_override_interval_msec,
				     PIM_IF_TEST_PIM_CAN_DISABLE_JOIN_SUPRESSION(pim_ifp->options),
				     ifp->connected);
  if (pim_tlv_size < 0) {
    return -1;
  }

  pim_msg_size = pim_tlv_size + PIM_PIM_MIN_LEN;

  zassert(pim_msg_size >= PIM_PIM_MIN_LEN);
  zassert(pim_msg_size <= PIM_PIM_BUFSIZE_WRITE);

  pim_msg_build_header(pim_msg, pim_msg_size,
		       PIM_MSG_TYPE_HELLO);

  if (pim_msg_send(pim_ifp->pim_sock_fd,
		   qpim_all_pim_routers_addr,
		   pim_msg,
		   pim_msg_size,
		   ifp->name)) {
    zlog_warn("%s: could not send PIM message on interface %s",
	      __PRETTY_FUNCTION__, ifp->name);
    return -2;
  }

  return 0;
}

static int pim_hello_send(struct interface *ifp,
			  uint16_t holdtime)
{
  struct pim_interface *pim_ifp;

  zassert(ifp);
  pim_ifp = ifp->info;
  zassert(pim_ifp);

  if (hello_send(ifp, holdtime)) {
    ++pim_ifp->pim_ifstat_hello_sendfail;

    zlog_warn("Could not send PIM hello on interface %s",
	      ifp->name);
    return -1;
  }

  ++pim_ifp->pim_ifstat_hello_sent;

  return 0;
}

static void hello_resched(struct interface *ifp)
{
  struct pim_interface *pim_ifp;

  zassert(ifp);
  pim_ifp = ifp->info;
  zassert(pim_ifp);

  if (PIM_DEBUG_PIM_TRACE) {
    zlog_debug("Rescheduling %d sec hello on interface %s",
	       pim_ifp->pim_hello_period, ifp->name);
  }
  THREAD_OFF(pim_ifp->t_pim_hello_timer);
  THREAD_TIMER_ON(master, pim_ifp->t_pim_hello_timer,
		  on_pim_hello_send,
		  ifp, pim_ifp->pim_hello_period);
}

/*
  Periodic hello timer
 */
static int on_pim_hello_send(struct thread *t)
{
  struct pim_interface *pim_ifp;
  struct interface *ifp;

  zassert(t);
  ifp = THREAD_ARG(t);
  zassert(ifp);

  pim_ifp = ifp->info;

  /*
   * Schedule next hello
   */
  pim_ifp->t_pim_hello_timer = 0;
  hello_resched(ifp);

  /*
   * Send hello
   */
  return pim_hello_send(ifp, PIM_IF_DEFAULT_HOLDTIME(pim_ifp));
}

/*
  RFC 4601: 4.3.1.  Sending Hello Messages

  Thus, if a router needs to send a Join/Prune or Assert message on an
  interface on which it has not yet sent a Hello message with the
  currently configured IP address, then it MUST immediately send the
  relevant Hello message without waiting for the Hello Timer to
  expire, followed by the Join/Prune or Assert message.
 */
void pim_hello_restart_now(struct interface *ifp)
{
  struct pim_interface *pim_ifp;

  zassert(ifp);
  pim_ifp = ifp->info;
  zassert(pim_ifp);

  /*
   * Reset next hello timer
   */
  hello_resched(ifp);

  /*
   * Immediately send hello
   */
  pim_hello_send(ifp, PIM_IF_DEFAULT_HOLDTIME(pim_ifp));
}

/*
  RFC 4601: 4.3.1.  Sending Hello Messages

  To allow new or rebooting routers to learn of PIM neighbors quickly,
  when a Hello message is received from a new neighbor, or a Hello
  message with a new GenID is received from an existing neighbor, a
  new Hello message should be sent on this interface after a
  randomized delay between 0 and Triggered_Hello_Delay.
 */
void pim_hello_restart_triggered(struct interface *ifp)
{
  struct pim_interface *pim_ifp;
  int triggered_hello_delay_msec;
  int random_msec;

  zassert(ifp);
  pim_ifp = ifp->info;
  zassert(pim_ifp);

  triggered_hello_delay_msec = 1000 * pim_ifp->pim_triggered_hello_delay;

  if (pim_ifp->t_pim_hello_timer) {
    long remain_msec = pim_time_timer_remain_msec(pim_ifp->t_pim_hello_timer);
    if (remain_msec <= triggered_hello_delay_msec) {
      /* Rescheduling hello would increase the delay, then it's faster
	 to just wait for the scheduled periodic hello. */
      return;
    }

    THREAD_OFF(pim_ifp->t_pim_hello_timer);
    pim_ifp->t_pim_hello_timer = 0;
  }
  zassert(!pim_ifp->t_pim_hello_timer);

  random_msec = pim_rand_next(0, triggered_hello_delay_msec);

  if (PIM_DEBUG_PIM_EVENTS) {
    zlog_debug("Scheduling %d msec triggered hello on interface %s",
	       random_msec, ifp->name);
  }

  THREAD_TIMER_MSEC_ON(master, pim_ifp->t_pim_hello_timer,
		       on_pim_hello_send,
		       ifp, random_msec);
}

int pim_sock_add(struct interface *ifp)
{
  struct pim_interface *pim_ifp;
  struct in_addr ifaddr;

  pim_ifp = ifp->info;
  zassert(pim_ifp);

  if (pim_ifp->pim_sock_fd >= 0) {
    zlog_warn("Can't recreate existing PIM socket fd=%d for interface %s",
	      pim_ifp->pim_sock_fd, ifp->name);
    return -1;
  }

  ifaddr = pim_ifp->primary_address;

  pim_ifp->pim_sock_fd = pim_sock_open(ifaddr, ifp->ifindex);
  if (pim_ifp->pim_sock_fd < 0) {
    zlog_warn("Could not open PIM socket on interface %s",
	      ifp->name);
    return -2;
  }

  pim_ifp->t_pim_sock_read   = 0;
  pim_ifp->pim_sock_creation = pim_time_monotonic_sec();

  pim_ifp->pim_generation_id = pim_rand() & (int64_t) 0xFFFFFFFF;

  zlog_info("PIM INTERFACE UP: on interface %s ifindex=%d",
	    ifp->name, ifp->ifindex);

  /*
   * Start receiving PIM messages
   */
  pim_sock_read_on(ifp);

  /*
   * Start sending PIM hello's
   */
  pim_hello_restart_triggered(ifp);

  return 0;
}
