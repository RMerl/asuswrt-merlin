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
#include "privs.h"

#include "pimd.h"
#include "pim_mroute.h"
#include "pim_str.h"
#include "pim_time.h"
#include "pim_iface.h"
#include "pim_macro.h"

/* GLOBAL VARS */
extern struct zebra_privs_t pimd_privs;

static void mroute_read_on(void);

static int pim_mroute_set(int fd, int enable)
{
  int err;
  int opt = enable ? MRT_INIT : MRT_DONE;
  socklen_t opt_len = sizeof(opt);

  err = setsockopt(fd, IPPROTO_IP, opt, &opt, opt_len);
  if (err) {
    int e = errno;
    zlog_warn("%s %s: failure: setsockopt(fd=%d,IPPROTO_IP,%s=%d): errno=%d: %s",
	      __FILE__, __PRETTY_FUNCTION__,
	      fd, enable ? "MRT_INIT" : "MRT_DONE", opt, e, safe_strerror(e));
    errno = e;
    return -1;
  }

#if 0
  zlog_info("%s %s: setsockopt(fd=%d,IPPROTO_IP,MRT_INIT,opt=%d): ok",
	    __FILE__, __PRETTY_FUNCTION__,
	    fd, opt);
#endif

  return 0;
}

int pim_mroute_msg(int fd, const char *buf, int buf_size)
{
  struct interface     *ifp;
  const struct ip      *ip_hdr;
  const struct igmpmsg *msg;
  const char *upcall;
  char src_str[100];
  char grp_str[100];

  ip_hdr = (const struct ip *) buf;

  /* kernel upcall must have protocol=0 */
  if (ip_hdr->ip_p) {
    /* this is not a kernel upcall */
#ifdef PIM_UNEXPECTED_KERNEL_UPCALL
    zlog_warn("%s: not a kernel upcall proto=%d msg_size=%d",
	      __PRETTY_FUNCTION__, ip_hdr->ip_p, buf_size);
#endif
    return 0;
  }

  msg = (const struct igmpmsg *) buf;

  switch (msg->im_msgtype) {
  case IGMPMSG_NOCACHE:  upcall = "NOCACHE";  break;
  case IGMPMSG_WRONGVIF: upcall = "WRONGVIF"; break;
  case IGMPMSG_WHOLEPKT: upcall = "WHOLEPKT"; break;
  default: upcall = "<unknown_upcall?>";
  }
  ifp = pim_if_find_by_vif_index(msg->im_vif);
  pim_inet4_dump("<src?>", msg->im_src, src_str, sizeof(src_str));
  pim_inet4_dump("<grp?>", msg->im_dst, grp_str, sizeof(grp_str));
    
  if (msg->im_msgtype == IGMPMSG_WRONGVIF) {
    struct pim_ifchannel *ch;
    struct pim_interface *pim_ifp;

    /*
      Send Assert(S,G) on iif as response to WRONGVIF kernel upcall.
      
      RFC 4601 4.8.2.  PIM-SSM-Only Routers
      
      iif is the incoming interface of the packet.
      if (iif is in inherited_olist(S,G)) {
      send Assert(S,G) on iif
      }
    */

    if (PIM_DEBUG_PIM_TRACE) {
      zlog_debug("%s: WRONGVIF from fd=%d for (S,G)=(%s,%s) on %s vifi=%d",
		 __PRETTY_FUNCTION__,
		 fd,
		 src_str,
		 grp_str,
		 ifp ? ifp->name : "<ifname?>",
		 msg->im_vif);
    }

    if (!ifp) {
      zlog_warn("%s: WRONGVIF (S,G)=(%s,%s) could not find input interface for input_vif_index=%d",
		__PRETTY_FUNCTION__,
		src_str, grp_str, msg->im_vif);
      return -1;
    }

    pim_ifp = ifp->info;
    if (!pim_ifp) {
      zlog_warn("%s: WRONGVIF (S,G)=(%s,%s) multicast not enabled on interface %s",
		__PRETTY_FUNCTION__,
		src_str, grp_str, ifp->name);
      return -2;
    }

    ch = pim_ifchannel_find(ifp, msg->im_src, msg->im_dst);
    if (!ch) {
      zlog_warn("%s: WRONGVIF (S,G)=(%s,%s) could not find channel on interface %s",
		__PRETTY_FUNCTION__,
		src_str, grp_str, ifp->name);
      return -3;
    }

    /*
      RFC 4601: 4.6.1.  (S,G) Assert Message State Machine

      Transitions from NoInfo State

      An (S,G) data packet arrives on interface I, AND
      CouldAssert(S,G,I)==TRUE An (S,G) data packet arrived on an
      downstream interface that is in our (S,G) outgoing interface
      list.  We optimistically assume that we will be the assert
      winner for this (S,G), and so we transition to the "I am Assert
      Winner" state and perform Actions A1 (below), which will
      initiate the assert negotiation for (S,G).
    */

    if (ch->ifassert_state != PIM_IFASSERT_NOINFO) {
      zlog_warn("%s: WRONGVIF (S,G)=(%s,%s) channel is not on Assert NoInfo state for interface %s",
		__PRETTY_FUNCTION__,
		src_str, grp_str, ifp->name);
      return -4;
    }

    if (!PIM_IF_FLAG_TEST_COULD_ASSERT(ch->flags)) {
      zlog_warn("%s: WRONGVIF (S,G)=(%s,%s) interface %s is not downstream for channel",
		__PRETTY_FUNCTION__,
		src_str, grp_str, ifp->name);
      return -5;
    }

    if (assert_action_a1(ch)) {
      zlog_warn("%s: WRONGVIF (S,G)=(%s,%s) assert_action_a1 failure on interface %s",
		__PRETTY_FUNCTION__,
		src_str, grp_str, ifp->name);
      return -6;
    }

    return 0;
  } /* IGMPMSG_WRONGVIF */

  zlog_warn("%s: kernel upcall %s type=%d ip_p=%d from fd=%d for (S,G)=(%s,%s) on %s vifi=%d",
	    __PRETTY_FUNCTION__,
	    upcall,
	    msg->im_msgtype,
	    ip_hdr->ip_p,
	    fd,
	    src_str,
	    grp_str,
	    ifp ? ifp->name : "<ifname?>",
	    msg->im_vif);

  return 0;
}

static int mroute_read_msg(int fd)
{
  const int msg_min_size = MAX(sizeof(struct ip), sizeof(struct igmpmsg));
  char buf[1000];
  int rd;

  if (((int) sizeof(buf)) < msg_min_size) {
    zlog_err("%s: fd=%d: buf size=%zu lower than msg_min=%d",
	     __PRETTY_FUNCTION__, fd, sizeof(buf), msg_min_size);
    return -1;
  }

  rd = read(fd, buf, sizeof(buf));
  if (rd < 0) {
    zlog_warn("%s: failure reading fd=%d: errno=%d: %s",
	      __PRETTY_FUNCTION__, fd, errno, safe_strerror(errno));
    return -2;
  }

  if (rd < msg_min_size) {
    zlog_warn("%s: short message reading fd=%d: read=%d msg_min=%d",
	      __PRETTY_FUNCTION__, fd, rd, msg_min_size);
    return -3;
  }

  return pim_mroute_msg(fd, buf, rd);
}

static int mroute_read(struct thread *t)
{
  int fd;
  int result;

  zassert(t);
  zassert(!THREAD_ARG(t));

  fd = THREAD_FD(t);
  zassert(fd == qpim_mroute_socket_fd);

  result = mroute_read_msg(fd);

  /* Keep reading */
  qpim_mroute_socket_reader = 0;
  mroute_read_on();

  return result;
}

static void mroute_read_on()
{
  zassert(!qpim_mroute_socket_reader);
  zassert(PIM_MROUTE_IS_ENABLED);

  THREAD_READ_ON(master, qpim_mroute_socket_reader,
		 mroute_read, 0, qpim_mroute_socket_fd);
}

static void mroute_read_off()
{
  THREAD_OFF(qpim_mroute_socket_reader);
}

int pim_mroute_socket_enable()
{
  int fd;

  if (PIM_MROUTE_IS_ENABLED)
    return -1;

  if ( pimd_privs.change (ZPRIVS_RAISE) )
    zlog_err ("pim_mroute_socket_enable: could not raise privs, %s",
              safe_strerror (errno) );

  fd = socket(AF_INET, SOCK_RAW, IPPROTO_IGMP);

  if ( pimd_privs.change (ZPRIVS_LOWER) )
    zlog_err ("pim_mroute_socket_enable: could not lower privs, %s",
	      safe_strerror (errno) );

  if (fd < 0) {
    zlog_warn("Could not create mroute socket: errno=%d: %s",
	      errno, safe_strerror(errno));
    return -2;
  }

  if (pim_mroute_set(fd, 1)) {
    zlog_warn("Could not enable mroute on socket fd=%d: errno=%d: %s",
	      fd, errno, safe_strerror(errno));
    close(fd);
    return -3;
  }

  qpim_mroute_socket_fd       = fd;
  qpim_mroute_socket_creation = pim_time_monotonic_sec();
  mroute_read_on();

  zassert(PIM_MROUTE_IS_ENABLED);

  return 0;
}

int pim_mroute_socket_disable()
{
  if (PIM_MROUTE_IS_DISABLED)
    return -1;

  if (pim_mroute_set(qpim_mroute_socket_fd, 0)) {
    zlog_warn("Could not disable mroute on socket fd=%d: errno=%d: %s",
	      qpim_mroute_socket_fd, errno, safe_strerror(errno));
    return -2;
  }

  if (close(qpim_mroute_socket_fd)) {
    zlog_warn("Failure closing mroute socket: fd=%d errno=%d: %s",
	      qpim_mroute_socket_fd, errno, safe_strerror(errno));
    return -3;
  }

  mroute_read_off();
  qpim_mroute_socket_fd = -1;

  zassert(PIM_MROUTE_IS_DISABLED);

  return 0;
}

/*
  For each network interface (e.g., physical or a virtual tunnel) that
  would be used for multicast forwarding, a corresponding multicast
  interface must be added to the kernel.
 */
int pim_mroute_add_vif(int vif_index, struct in_addr ifaddr)
{
  struct vifctl vc;
  int err;

  if (PIM_MROUTE_IS_DISABLED) {
    zlog_warn("%s: global multicast is disabled",
	      __PRETTY_FUNCTION__);
    return -1;
  }

  memset(&vc, 0, sizeof(vc));
  vc.vifc_vifi = vif_index;
  vc.vifc_flags = 0;
  vc.vifc_threshold = PIM_MROUTE_MIN_TTL;
  vc.vifc_rate_limit = 0;
  memcpy(&vc.vifc_lcl_addr, &ifaddr, sizeof(vc.vifc_lcl_addr));

#ifdef PIM_DVMRP_TUNNEL  
  if (vc.vifc_flags & VIFF_TUNNEL) {
    memcpy(&vc.vifc_rmt_addr, &vif_remote_addr, sizeof(vc.vifc_rmt_addr));
  }
#endif

  err = setsockopt(qpim_mroute_socket_fd, IPPROTO_IP, MRT_ADD_VIF, (void*) &vc, sizeof(vc)); 
  if (err) {
    char ifaddr_str[100];
    int e = errno;

    pim_inet4_dump("<ifaddr?>", ifaddr, ifaddr_str, sizeof(ifaddr_str));

    zlog_warn("%s %s: failure: setsockopt(fd=%d,IPPROTO_IP,MRT_ADD_VIF,vif_index=%d,ifaddr=%s): errno=%d: %s",
	      __FILE__, __PRETTY_FUNCTION__,
	      qpim_mroute_socket_fd, vif_index, ifaddr_str,
	      e, safe_strerror(e));
    errno = e;
    return -2;
  }

  return 0;
}

int pim_mroute_del_vif(int vif_index)
{
  struct vifctl vc;
  int err;

  if (PIM_MROUTE_IS_DISABLED) {
    zlog_warn("%s: global multicast is disabled",
	      __PRETTY_FUNCTION__);
    return -1;
  }

  memset(&vc, 0, sizeof(vc));
  vc.vifc_vifi = vif_index;

  err = setsockopt(qpim_mroute_socket_fd, IPPROTO_IP, MRT_DEL_VIF, (void*) &vc, sizeof(vc)); 
  if (err) {
    int e = errno;
    zlog_warn("%s %s: failure: setsockopt(fd=%d,IPPROTO_IP,MRT_DEL_VIF,vif_index=%d): errno=%d: %s",
	      __FILE__, __PRETTY_FUNCTION__,
	      qpim_mroute_socket_fd, vif_index,
	      e, safe_strerror(e));
    errno = e;
    return -2;
  }

  return 0;
}

int pim_mroute_add(struct mfcctl *mc)
{
  int err;

  qpim_mroute_add_last = pim_time_monotonic_sec();
  ++qpim_mroute_add_events;

  if (PIM_MROUTE_IS_DISABLED) {
    zlog_warn("%s: global multicast is disabled",
	      __PRETTY_FUNCTION__);
    return -1;
  }

  err = setsockopt(qpim_mroute_socket_fd, IPPROTO_IP, MRT_ADD_MFC,
		   mc, sizeof(*mc));
  if (err) {
    int e = errno;
    zlog_warn("%s %s: failure: setsockopt(fd=%d,IPPROTO_IP,MRT_ADD_MFC): errno=%d: %s",
	      __FILE__, __PRETTY_FUNCTION__,
	      qpim_mroute_socket_fd,
	      e, safe_strerror(e));
    errno = e;
    return -2;
  }

  return 0;
}

int pim_mroute_del(struct mfcctl *mc)
{
  int err;

  qpim_mroute_del_last = pim_time_monotonic_sec();
  ++qpim_mroute_del_events;

  if (PIM_MROUTE_IS_DISABLED) {
    zlog_warn("%s: global multicast is disabled",
	      __PRETTY_FUNCTION__);
    return -1;
  }

  err = setsockopt(qpim_mroute_socket_fd, IPPROTO_IP, MRT_DEL_MFC, mc, sizeof(*mc));
  if (err) {
    int e = errno;
    zlog_warn("%s %s: failure: setsockopt(fd=%d,IPPROTO_IP,MRT_DEL_MFC): errno=%d: %s",
	      __FILE__, __PRETTY_FUNCTION__,
	      qpim_mroute_socket_fd,
	      e, safe_strerror(e));
    errno = e;
    return -2;
  }

  return 0;
}
