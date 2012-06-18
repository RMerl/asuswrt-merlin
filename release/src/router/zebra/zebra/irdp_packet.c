/*
 *
 * Copyright (C) 2000  Robert Olsson.
 * Swedish University of Agricultural Sciences
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

/* 
 * This work includes work with the following copywrite:
 *
 * Copyright (C) 1997, 2000 Kunihiro Ishiguro
 *
 */

/* 
 * Thanks to Jens Låås at Swedish University of Agricultural Sciences
 * for reviewing and tests.
 */


#include <zebra.h>


#ifdef HAVE_IRDP

#include "if.h"
#include "vty.h"
#include "sockunion.h"
#include "prefix.h"
#include "command.h"
#include "memory.h"
#include "stream.h"
#include "ioctl.h"
#include "connected.h"
#include "log.h"
#include "zclient.h"
#include "thread.h"
#include "zebra/interface.h"
#include "zebra/rtadv.h"
#include "zebra/rib.h"
#include "zebra/zserv.h"
#include "zebra/redistribute.h"
#include "zebra/irdp.h"
#include <netinet/ip_icmp.h>
#include "if.h"
#include "sockunion.h"
#include "log.h"



/* GLOBAL VARS */

int irdp_sock;
char b1[16], b2[16], b3[16], b4[16];  /* For inet_2a */


extern struct thread_master *master;
extern struct thread *t_irdp_raw;
extern struct interface *get_iflist_ifp(int idx);
int in_cksum (void *ptr, int nbytes);
void process_solicit (struct interface *ifp);

void parse_irdp_packet(char *p, 
		  int len, 
		  struct interface *ifp)
{
  struct ip *ip = (struct ip *)p ;
  struct icmphdr *icmp;
  struct in_addr src;
  int ip_hlen, ip_len;
  struct zebra_if *zi;
  struct irdp_interface *irdp;

  zi = ifp->info;
  if(!zi) return;

  irdp = &zi->irdp;
  if(!irdp) return;

  ip_hlen = ip->ip_hl*4; 
  ip_len = ntohs(ip->ip_len);
  len = len - ip_hlen;
  src = ip->ip_src;

  if(ip_len < ICMP_MINLEN) {
    zlog_err ("IRDP: RX ICMP packet too short from %s\n",
	      inet_ntoa (src));
    return;
  }

  /* Check so we don't checksum packets longer than oure RX_BUF - (ethlen +
   len of IP-header) 14+20 */

  if(ip_len > IRDP_RX_BUF-34) {
    zlog_err ("IRDP: RX ICMP packet too long from %s\n",
	      inet_ntoa (src));
    return;
  }


  if (in_cksum (ip, ip_len)) {
    zlog_warn ("IRDP: RX ICMP packet from %s. Bad checksum, silently ignored",
	       inet_ntoa (src));
    return;
  }

  icmp = (struct icmphdr *) (p+ip_hlen);


  /* Handle just only IRDP */

  if( icmp->type == ICMP_ROUTERADVERT);
  else if( icmp->type == ICMP_ROUTERSOLICIT);
  else return;

 
  if (icmp->code != 0) {
    zlog_warn ("IRDP: RX packet type %d from %s. Bad ICMP type code, silently ignored",
	       icmp->type,
	       inet_ntoa (src));
    return;
  }

  if(ip->ip_dst.s_addr == INADDR_BROADCAST && 
     irdp->flags & IF_BROADCAST);

  else if ( ntohl(ip->ip_dst.s_addr) == INADDR_ALLRTRS_GROUP && 
	    ! (irdp->flags &  IF_BROADCAST));

  else { /* ERROR */

    zlog_warn ("IRDP: RX illegal from %s to %s while %s operates in %s\n",
	       inet_ntoa (src), 
	       ntohl(ip->ip_dst.s_addr) == INADDR_ALLRTRS_GROUP? 
	       "multicast" : inet_ntoa(ip->ip_dst),
	       ifp->name,
	       irdp->flags &  IF_BROADCAST? 
	       "broadcast" : "multicast");

    zlog_warn ("IRDP: Please correct settings\n");
    return;
  }

  switch (icmp->type) 
    {
    case ICMP_ROUTERADVERT:
      break;

    case ICMP_ROUTERSOLICIT:

      if(irdp->flags & IF_DEBUG_MESSAGES) 
	zlog_warn ("IRDP: RX Solicit on %s from %s\n",
		   ifp->name,
		   inet_ntoa (src));

      process_solicit(ifp);
      break;

    default:
      zlog_warn ("IRDP: RX type %d from %s. Bad ICMP type, silently ignored",
		 icmp->type,
		 inet_ntoa (src));
    }
}

int irdp_recvmsg (int sock, 
	      u_char *buf, 
	      int size, 
	      int *ifindex)
{
  struct msghdr msg;
  struct iovec iov;
  struct cmsghdr *ptr;
  char adata[1024];
  int ret;

  msg.msg_name = (void *)0;
  msg.msg_namelen = 0;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = (void *) adata;
  msg.msg_controllen = sizeof adata;

  iov.iov_base = buf;
  iov.iov_len = size;

  ret = recvmsg (sock, &msg, 0);
  if (ret < 0) {
    zlog_warn("IRDP: recvmsg: read error %s", strerror(errno));
    return ret;
  }

  if (msg.msg_flags & MSG_TRUNC) {
    zlog_warn("IRDP: recvmsg: truncated message");
    return ret;
  }
  if (msg.msg_flags & MSG_CTRUNC) {
    zlog_warn("IRDP: recvmsg: truncated control message");
    return ret;
  }

  for (ptr = CMSG_FIRSTHDR(&msg); ptr ; ptr = CMSG_NXTHDR(&msg, ptr))
    if (ptr->cmsg_level == SOL_IP && ptr->cmsg_type == IP_PKTINFO) 
      {
        struct in_pktinfo *pktinfo;
           pktinfo = (struct in_pktinfo *) CMSG_DATA (ptr);
        *ifindex = pktinfo->ipi_ifindex;
      }
  return ret;
}

int irdp_read_raw(struct thread *r)
{
  struct interface *ifp;
  struct zebra_if *zi;
  struct irdp_interface *irdp;
  char buf[IRDP_RX_BUF];
  int ret, ifindex;
  
  int irdp_sock = THREAD_FD (r);
  t_irdp_raw = thread_add_read (master, irdp_read_raw, NULL, irdp_sock);
  
  ret = irdp_recvmsg (irdp_sock, buf, IRDP_RX_BUF,  &ifindex);
 
  if (ret < 0) zlog_warn ("IRDP: RX Error length = %d", ret);

  ifp = get_iflist_ifp(ifindex);
  if(! ifp ) return ret;

  zi= ifp->info;
  if(! zi ) return ret;

  irdp = &zi->irdp;
  if(! irdp ) return ret;

  if(! (irdp->flags & IF_ACTIVE)) {

    if(irdp->flags & IF_DEBUG_MISC) 
      zlog_warn("IRDP: RX ICMP for disabled interface %s\n",
		ifp->name);
    return 0;
  }

  if(irdp->flags & IF_DEBUG_PACKET) {
    int i;
    zlog_warn("IRDP: RX (idx %d) ", ifindex);
    for(i=0; i < ret; i++) zlog_warn( "IRDP: RX %x ", buf[i]&0xFF);
  }

  parse_irdp_packet(buf, ret, ifp);
  return ret;
}

void send_packet(struct interface *ifp, 
	    struct stream *s,
	    u_int32_t dst,
	    struct prefix *p,
	    u_int32_t ttl)
{
  static struct sockaddr_in sockdst = {AF_INET};
  struct ip *ip;
  struct icmphdr *icmp;
  struct msghdr *msg;
  struct cmsghdr *cmsg;
  struct iovec iovector;
  char msgbuf[256];
  char buf[256];
  struct in_pktinfo *pktinfo;
  u_long src;
  int on;
 
  if (! (ifp->flags & IFF_UP)) return;

  if(!p) src = ntohl(p->u.prefix4.s_addr);
  else src = 0; /* Is filled in */
  
  ip = (struct ip *) buf;
  ip->ip_hl = sizeof(struct ip) >> 2;
  ip->ip_v = IPVERSION;
  ip->ip_tos = 0xC0;
  ip->ip_off = 0L;
  ip->ip_p = 1;       /* IP_ICMP */
  ip->ip_ttl = ttl;
  ip->ip_src.s_addr = src;
  ip->ip_dst.s_addr = dst;
  icmp = (struct icmphdr *) (buf + 20);

  /* Merge IP header with icmp packet */

  stream_get(icmp, s, s->putp);

  /* icmp->checksum is already calculated */
  ip->ip_len  = sizeof(struct ip) + s->putp;
  stream_free(s); 

  on = 1;
  if (setsockopt(irdp_sock, IPPROTO_IP, IP_HDRINCL,
		 (char *) &on, sizeof(on)) < 0)
    zlog_warn("sendto %s", strerror (errno));


  if(dst == INADDR_BROADCAST ) {
    on = 1;
    if (setsockopt(irdp_sock, SOL_SOCKET, SO_BROADCAST,
		   (char *) &on, sizeof(on)) < 0)
      zlog_warn("sendto %s", strerror (errno));
  }

  if(dst !=  INADDR_BROADCAST) {
      on = 0; 
      if( setsockopt(irdp_sock,IPPROTO_IP, IP_MULTICAST_LOOP, 
		     (char *)&on,sizeof(on)) < 0)
	zlog_warn("sendto %s", strerror (errno));
  }

  bzero(&sockdst,sizeof(sockdst));
  sockdst.sin_family=AF_INET;
  sockdst.sin_addr.s_addr = dst;

  cmsg = (struct cmsghdr *) (msgbuf + sizeof(struct msghdr));
  cmsg->cmsg_len = sizeof(struct cmsghdr) + sizeof(struct in_pktinfo);
  cmsg->cmsg_level = SOL_IP;
  cmsg->cmsg_type = IP_PKTINFO;
  pktinfo = (struct in_pktinfo *) CMSG_DATA(cmsg);
  pktinfo->ipi_ifindex = ifp->ifindex;
  pktinfo->ipi_spec_dst.s_addr = src;
  pktinfo->ipi_addr.s_addr = src;
 
  iovector.iov_base = (void *) buf;
  iovector.iov_len = ip->ip_len; 
  msg = (struct msghdr *) msgbuf;
  msg->msg_name = &sockdst;
  msg->msg_namelen = sizeof(sockdst);
  msg->msg_iov = &iovector;
  msg->msg_iovlen = 1;
  msg->msg_control = cmsg;
  msg->msg_controllen = cmsg->cmsg_len;
 
  if (sendmsg(irdp_sock, msg, 0) < 0) {
    zlog_warn("sendto %s", strerror (errno));
  }
  /*   printf("TX on %s idx %d\n", ifp->name, ifp->ifindex); */
}


#endif /* HAVE_IRDP */



