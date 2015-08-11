/*	$Id: ifaddrs.c 241182 2011-02-17 21:50:03Z $	*/
/* 	from USAGI: ifaddrs.c,v 1.20.2.1 2002/12/08 08:22:23 yoshfuji Exp */

/* 
 * Copyright (C)2000 YOSHIFUJI Hideaki
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>
#include <time.h>
#include <malloc.h>
#include <errno.h>
#include <unistd.h>

#define __set_errno(x)	errno = (x)

#include <sys/socket.h>
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>	/* the L2 protocols */
#include <sys/uio.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <ifaddrs.h>
#include <netinet/in.h>

#ifdef _USAGI_LIBINET6
#include "libc-compat.h"
#endif

/* ====================================================================== */
struct nlmsg_list
{
  struct nlmsg_list *nlm_next;
  struct nlmsghdr *nlh;
  int size;
  time_t seq;
};

struct rtmaddr_ifamap
{
  void *address;
  void *local;
#ifdef IFA_NETMASK
  void *netmask;
#endif
  void *broadcast;
#ifdef HAVE_IFADDRS_IFA_ANYCAST
  void *anycast;
#endif
  int address_len;
  int local_len;
#ifdef IFA_NETMASK
  int netmask_len;
#endif
  int broadcast_len;
#ifdef HAVE_IFADDRS_IFA_ANYCAST
  int anycast_len;
#endif
};

/* ====================================================================== */
static size_t
ifa_sa_len (sa_family_t family, int len)
{
  size_t size;
  switch (family)
    {
    case AF_INET:
      size = sizeof (struct sockaddr_in);
      break;
    case AF_INET6:
      size = sizeof (struct sockaddr_in6);
      break;
    case AF_PACKET:
      size = (size_t) (((struct sockaddr_ll *) NULL)->sll_addr) + len;
      if (size < sizeof (struct sockaddr_ll))
	size = sizeof (struct sockaddr_ll);
      break;
    default:
      size = (size_t) (((struct sockaddr *) NULL)->sa_data) + len;
      if (size < sizeof (struct sockaddr))
	size = sizeof (struct sockaddr);
    }
  return size;
}

static void
ifa_make_sockaddr (sa_family_t family,
		   struct sockaddr *sa,
		   void *p, size_t len, uint32_t scope, uint32_t scopeid)
{
  if (sa == NULL)
    return;
  switch (family)
    {
    case AF_INET:
      memcpy (&((struct sockaddr_in *) sa)->sin_addr, (char *) p, len);
      break;
    case AF_INET6:
      memcpy (&((struct sockaddr_in6 *) sa)->sin6_addr, (char *) p, len);
      if (IN6_IS_ADDR_LINKLOCAL (p) || IN6_IS_ADDR_MC_LINKLOCAL (p))
	{
	  ((struct sockaddr_in6 *) sa)->sin6_scope_id = scopeid;
	}
      break;
    case AF_PACKET:
      memcpy (((struct sockaddr_ll *) sa)->sll_addr, (char *) p, len);
      ((struct sockaddr_ll *) sa)->sll_halen = len;
      break;
    default:
      memcpy (sa->sa_data, p, len);
 break;
    }
  sa->sa_family = family;
#ifdef HAVE_SOCKADDR_SA_LEN
  sa->sa_len = ifa_sa_len (family, len);
#endif
}

static struct sockaddr *
ifa_make_sockaddr_mask (sa_family_t family,
			struct sockaddr *sa, uint32_t prefixlen)
{
  int i;
  char *p = NULL, c;
  uint32_t max_prefixlen = 0;

  if (sa == NULL)
    return NULL;
  switch (family)
    {
    case AF_INET:
      memset (&((struct sockaddr_in *) sa)->sin_addr, 0,
	      sizeof (((struct sockaddr_in *) sa)->sin_addr));
      p = (char *) &((struct sockaddr_in *) sa)->sin_addr;
      max_prefixlen = 32;
      break;
    case AF_INET6:
      memset (&((struct sockaddr_in6 *) sa)->sin6_addr, 0,
	      sizeof (((struct sockaddr_in6 *) sa)->sin6_addr));
      p = (char *) &((struct sockaddr_in6 *) sa)->sin6_addr;
      max_prefixlen = 128;
      break;
    default:
      return NULL;
    }
  sa->sa_family = family;
#ifdef HAVE_SOCKADDR_SA_LEN
  sa->sa_len = ifa_sa_len (family, len);
#endif
  if (p)
    {
      if (prefixlen > max_prefixlen)
	prefixlen = max_prefixlen;
      for (i = 0; i < (prefixlen / 8); i++)
	*p++ = 0xff;
      c = 0xff;
      c <<= (8 - (prefixlen % 8));
      *p = c;
    }
  return sa;
}

/* ====================================================================== */
static int
nl_sendreq (int sd, int request, int flags, int *seq)
{
  char reqbuf[NLMSG_ALIGN (sizeof (struct nlmsghdr)) +
	      NLMSG_ALIGN (sizeof (struct rtgenmsg))];
  struct sockaddr_nl nladdr;
  struct nlmsghdr *req_hdr;
  struct rtgenmsg *req_msg;
  time_t t = time (NULL);

  if (seq)
    *seq = t;
  memset (&reqbuf, 0, sizeof (reqbuf));
  req_hdr = (struct nlmsghdr *) reqbuf;
  req_msg = (struct rtgenmsg *) NLMSG_DATA (req_hdr);
  req_hdr->nlmsg_len = NLMSG_LENGTH (sizeof (*req_msg));
  req_hdr->nlmsg_type = request;
  req_hdr->nlmsg_flags = flags | NLM_F_REQUEST;
  req_hdr->nlmsg_pid = 0;
  req_hdr->nlmsg_seq = t;
  req_msg->rtgen_family = AF_UNSPEC;
  memset (&nladdr, 0, sizeof (nladdr));
  nladdr.nl_family = AF_NETLINK;
  return (sendto (sd, (void *) req_hdr, req_hdr->nlmsg_len, 0,
		  (struct sockaddr *) &nladdr, sizeof (nladdr)));
}

static int
nl_recvmsg (int sd, int request, int seq,
	    void *buf, size_t buflen, int *flags)
{
  struct msghdr msg;
  struct iovec iov = { buf, buflen };
  struct sockaddr_nl nladdr;
  int read_len;

  for (;;)
    {
      msg.msg_name = (void *) &nladdr;
      msg.msg_namelen = sizeof (nladdr);
      msg.msg_iov = &iov;
      msg.msg_iovlen = 1;
      msg.msg_control = NULL;
      msg.msg_controllen = 0;
      msg.msg_flags = 0;
      read_len = recvmsg (sd, &msg, 0);
      if ((read_len < 0 && errno == EINTR) || (msg.msg_flags & MSG_TRUNC))
	continue;
      if (flags)
	*flags = msg.msg_flags;
      break;
    }
  return read_len;
}

static int
nl_getmsg (int sd, int request, int seq, pid_t pid, struct nlmsghdr **nlhp, int *done)
{
  struct nlmsghdr *nh;
  size_t bufsize = 65536, lastbufsize = 0;
  void *buff = NULL;
  int result = 0, read_size;
  int msg_flags;
  for (;;)
    {
      void *newbuff = realloc (buff, bufsize);
      if (newbuff == NULL || bufsize < lastbufsize)
	{
	  result = -1;
	  break;
	}
      buff = newbuff;
      result = read_size =
	nl_recvmsg (sd, request, seq, buff, bufsize, &msg_flags);
      if (read_size < 0 || (msg_flags & MSG_TRUNC))
	{
	  lastbufsize = bufsize;
	  bufsize *= 2;
	  continue;
	}
      if (read_size == 0)
	break;
      nh = (struct nlmsghdr *) buff;
      for (nh = (struct nlmsghdr *) buff;
	   NLMSG_OK (nh, read_size);
	   nh = (struct nlmsghdr *) NLMSG_NEXT (nh, read_size))
	{
	  if (nh->nlmsg_pid != pid || nh->nlmsg_seq != seq)
	    continue;
	  if (nh->nlmsg_type == NLMSG_DONE)
	    {
	      (*done)++;
	      break;		/* ok */
	    }
	  if (nh->nlmsg_type == NLMSG_ERROR)
	    {
	      struct nlmsgerr *nlerr = (struct nlmsgerr *) NLMSG_DATA (nh);
	      result = -1;
	      if (nh->nlmsg_len < NLMSG_LENGTH (sizeof (struct nlmsgerr)))
		__set_errno (EIO);
	      else
		__set_errno (-nlerr->error);
	      break;
	    }
	}
      break;
    }
  if (result < 0)
    if (buff)
      {
	int saved_errno = errno;
	free (buff);
	__set_errno (saved_errno);
      }
  *nlhp = (struct nlmsghdr *) buff;
  return result;
}

static int
nl_getlist (int sd, int seq, pid_t pid,
	    int request,
	    struct nlmsg_list **nlm_list, struct nlmsg_list **nlm_end)
{
  struct nlmsghdr *nlh = NULL;
  int status;
  int done = 0;

  status = nl_sendreq (sd, request, NLM_F_ROOT | NLM_F_MATCH, &seq);
  if (status < 0)
    return status;
  if (seq == 0)
    seq = (int) time (NULL);
  while (!done)
    {
      status = nl_getmsg (sd, request, seq, pid, &nlh, &done);
      if (status < 0)
	return status;
      if (nlh)
	{
	  struct nlmsg_list *nlm_next =
	    (struct nlmsg_list *) malloc (sizeof (struct nlmsg_list));
	  if (nlm_next == NULL)
	    {
	      int saved_errno = errno;
	      free (nlh);
	      __set_errno (saved_errno);
	      status = -1;
	    }
	  else
	    {
	      nlm_next->nlm_next = NULL;
	      nlm_next->nlh = (struct nlmsghdr *) nlh;
	      nlm_next->size = status;
	      nlm_next->seq = seq;
	      if (*nlm_list == NULL)
		{
		  *nlm_list = nlm_next;
		  *nlm_end = nlm_next;
		}
	      else
		{
		  (*nlm_end)->nlm_next = nlm_next;
		  *nlm_end = nlm_next;
		}
	    }
	}
    }
  return status >= 0 ? seq : status;
}

/* ---------------------------------------------------------------------- */
static void
free_nlmsglist (struct nlmsg_list *nlm0)
{
  struct nlmsg_list *nlm, *nlm_next;
  int saved_errno;
  if (!nlm0)
    return;
  saved_errno = errno;
  nlm = nlm0;
  while (nlm)
    {
      if (nlm->nlh)
	free (nlm->nlh);
      nlm_next = nlm->nlm_next;
      free(nlm);
      nlm = nlm_next;
    }
  __set_errno (saved_errno);
}

static void
free_data (void *data, void *ifdata)
{
  int saved_errno = errno;
  if (data != NULL)
    free (data);
  if (ifdata != NULL)
    free (ifdata);
  __set_errno (saved_errno);
}

/* ---------------------------------------------------------------------- */
static void
nl_close (int sd)
{
  int saved_errno = errno;
  if (sd >= 0)
    close (sd);
  __set_errno (saved_errno);
}

/* ---------------------------------------------------------------------- */
static int
nl_open (pid_t *pid)
{
  struct sockaddr_nl nladdr;
  int sd;

  sd = socket (PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (sd < 0)
    return -1;
  memset (&nladdr, 0, sizeof (nladdr));
  nladdr.nl_family = AF_NETLINK;
  if (bind (sd, (struct sockaddr *) &nladdr, sizeof (nladdr)) < 0)
    {
      nl_close (sd);
      return -1;
    }
  if (pid)
    {
      socklen_t len = sizeof(nladdr);
      if (getsockname (sd, (struct sockaddr *) &nladdr, &len) < 0)
        {
          nl_close (sd);
          return -1;
        }
      *pid = nladdr.nl_pid;
    }
  return sd;
}

/* ====================================================================== */
int
getifaddrs (struct ifaddrs **ifap)
{
  int sd;
  struct nlmsg_list *nlmsg_list, *nlmsg_end, *nlm;
  /* - - - - - - - - - - - - - - - */
  int icnt;
  size_t dlen, xlen, nlen;
  uint32_t max_ifindex = 0;

  pid_t pid;
  int seq;
  int result;
  int build;			/* 0 or 1 */

/* ---------------------------------- */
  /* initialize */
  icnt = dlen = xlen = nlen = 0;
  nlmsg_list = nlmsg_end = NULL;

  if (ifap)
    *ifap = NULL;

/* ---------------------------------- */
  /* open socket and bind */
  sd = nl_open (&pid);
  if (sd < 0)
    return -1;

/* ---------------------------------- */
  /* gather info */
  if ((seq = nl_getlist (sd, 0, pid, RTM_GETLINK, &nlmsg_list, &nlmsg_end)) < 0)
    {
      free_nlmsglist (nlmsg_list);
      nl_close (sd);
      return -1;
    }
  if ((seq = nl_getlist (sd, seq + 1, pid, RTM_GETADDR,
			 &nlmsg_list, &nlmsg_end)) < 0)
    {
      free_nlmsglist (nlmsg_list);
      nl_close (sd);
      return -1;
    }

/* ---------------------------------- */
  /* Estimate size of result buffer and fill it */
  for (build = 0; build <= 1; build++)
    {
      struct ifaddrs *ifl = NULL, *ifa = NULL;
      struct nlmsghdr *nlh, *nlh0;
      void *data = NULL, *xdata = NULL, *ifdata = NULL;
      char *ifname = NULL, **iflist = NULL;
      uint16_t *ifflist = NULL;
      struct rtmaddr_ifamap ifamap;

      if (build)
	{
	  ifa = data = calloc (1,
			       NLMSG_ALIGN (sizeof (struct ifaddrs[icnt]))
			       + dlen + xlen + nlen);
	  ifdata = calloc (1,
			   NLMSG_ALIGN (sizeof (char *[max_ifindex + 1]))
			   +
			   NLMSG_ALIGN (sizeof (uint16_t[max_ifindex + 1])));
	  if (ifap != NULL)
	    *ifap = (ifdata != NULL) ? ifa : NULL;
	  else
	    {
	      free_data (data, ifdata);
	      result = 0;
	      break;
	    }
	  if (data == NULL || ifdata == NULL)
	    {
	      free_data (data, ifdata);
	      result = -1;
	      break;
	    }
	  ifl = NULL;
	  data += NLMSG_ALIGN (sizeof (struct ifaddrs)) * icnt;
	  xdata = data + dlen;
	  ifname = xdata + xlen;
	  iflist = ifdata;
	  ifflist =
	    ((void *) iflist) +
	    NLMSG_ALIGN (sizeof (char *[max_ifindex + 1]));
	}

      for (nlm = nlmsg_list; nlm; nlm = nlm->nlm_next)
	{
	  int nlmlen = nlm->size;
	  if (!(nlh0 = nlm->nlh))
	    continue;
	  for (nlh = nlh0;
	       NLMSG_OK (nlh, nlmlen); nlh = NLMSG_NEXT (nlh, nlmlen))
	    {
	      struct ifinfomsg *ifim = NULL;
	      struct ifaddrmsg *ifam = NULL;
	      struct rtattr *rta;

	      size_t nlm_struct_size = 0;
	      sa_family_t nlm_family = 0;
	      uint32_t nlm_scope = 0, nlm_index = 0;
#ifndef IFA_NETMASK
	      size_t sockaddr_size = 0;
	      uint32_t nlm_prefixlen = 0;
#endif
	      size_t rtasize;

	      memset (&ifamap, 0, sizeof (ifamap));

	      /* check if the message is what we want */
	      if (nlh->nlmsg_pid != pid || nlh->nlmsg_seq != nlm->seq)
		continue;
	      if (nlh->nlmsg_type == NLMSG_DONE)
		{
		  break;	/* ok */
		}
	      switch (nlh->nlmsg_type)
		{
		case RTM_NEWLINK:
		  ifim = (struct ifinfomsg *) NLMSG_DATA (nlh);
		  nlm_struct_size = sizeof (*ifim);
		  nlm_family = ifim->ifi_family;
		  nlm_scope = 0;
		  nlm_index = ifim->ifi_index;
		  nlm_prefixlen = 0;
		  if (build)
		    ifflist[nlm_index] = ifa->ifa_flags = ifim->ifi_flags;
		  break;
		case RTM_NEWADDR:
		  ifam = (struct ifaddrmsg *) NLMSG_DATA (nlh);
		  nlm_struct_size = sizeof (*ifam);
		  nlm_family = ifam->ifa_family;
		  nlm_scope = ifam->ifa_scope;
		  nlm_index = ifam->ifa_index;
		  nlm_prefixlen = ifam->ifa_prefixlen;
		  if (build)
		    ifa->ifa_flags = ifflist[nlm_index];
		  break;
		default:
		  continue;
		}

	      if (!build)
		{
		  if (max_ifindex < nlm_index)
		    max_ifindex = nlm_index;
		}
	      else
		{
		  if (ifl != NULL)
		    ifl->ifa_next = ifa;
		}

	      rtasize =
		NLMSG_PAYLOAD (nlh, nlmlen) - NLMSG_ALIGN (nlm_struct_size);
	      for (rta =
		   (struct rtattr *) (((char *) NLMSG_DATA (nlh)) +
				      NLMSG_ALIGN (nlm_struct_size));
		   RTA_OK (rta, rtasize); rta = RTA_NEXT (rta, rtasize))
		{
		  struct sockaddr **sap = NULL;
		  void *rtadata = RTA_DATA (rta);
		  size_t rtapayload = RTA_PAYLOAD (rta);
		  socklen_t sa_len;

		  switch (nlh->nlmsg_type)
		    {
		    case RTM_NEWLINK:
		      switch (rta->rta_type)
			{
			case IFLA_ADDRESS:
			case IFLA_BROADCAST:
			  if (build)
			    {
			      sap =
				(rta->rta_type ==
				 IFLA_ADDRESS) ? &ifa->ifa_addr : &ifa->
				ifa_broadaddr;
			      *sap = (struct sockaddr *) data;
			    }
			  sa_len = ifa_sa_len (AF_PACKET, rtapayload);
			  if (rta->rta_type == IFLA_ADDRESS)
			    sockaddr_size = NLMSG_ALIGN (sa_len);
			  if (!build)
			    {
			      dlen += NLMSG_ALIGN (sa_len);
			    }
			  else
			    {
			      memset (*sap, 0, sa_len);
			      ifa_make_sockaddr (AF_PACKET, *sap, rtadata,
						 rtapayload, 0, 0);
			      ((struct sockaddr_ll *) *sap)->sll_ifindex =
				nlm_index;
			      ((struct sockaddr_ll *) *sap)->sll_hatype =
				ifim->ifi_type;
			      data += NLMSG_ALIGN (sa_len);
			    }
			  break;
			case IFLA_IFNAME:	/* Name of Interface */
			  if (!build)
			    nlen += NLMSG_ALIGN (rtapayload + 1);
			  else
			    {
			      ifa->ifa_name = ifname;
			      if (iflist[nlm_index] == NULL)
				iflist[nlm_index] = ifa->ifa_name;
			      strncpy (ifa->ifa_name, rtadata, rtapayload);
			      ifa->ifa_name[rtapayload] = '\0';
			      ifname += NLMSG_ALIGN (rtapayload + 1);
			    }
			  break;
			case IFLA_STATS:	/* Statistics of Interface */
			  if (!build)
			    xlen += NLMSG_ALIGN (rtapayload);
			  else
			    {
			      ifa->ifa_data = xdata;
			      memcpy (ifa->ifa_data, rtadata, rtapayload);
			      xdata += NLMSG_ALIGN (rtapayload);
			    }
			  break;
			case IFLA_UNSPEC:
			  break;
			case IFLA_MTU:
			  break;
			case IFLA_LINK:
			  break;
			case IFLA_QDISC:
			  break;
			default:
				;
			}
		      break;
		    case RTM_NEWADDR:
		      if (nlm_family == AF_PACKET)
			break;
		      switch (rta->rta_type)
			{
			case IFA_ADDRESS:
			  ifamap.address = rtadata;
			  ifamap.address_len = rtapayload;
			  break;
			case IFA_LOCAL:
			  ifamap.local = rtadata;
			  ifamap.local_len = rtapayload;
			  break;
			case IFA_BROADCAST:
			  ifamap.broadcast = rtadata;
			  ifamap.broadcast_len = rtapayload;
			  break;
#ifdef HAVE_IFADDRS_IFA_ANYCAST
			case IFA_ANYCAST:
			  ifamap.anycast = rtadata;
			  ifamap.anycast_len = rtapayload;
			  break;
#endif
			case IFA_LABEL:
			  if (!build)
			    nlen += NLMSG_ALIGN (rtapayload + 1);
			  else
			    {
			      ifa->ifa_name = ifname;
			      if (iflist[nlm_index] == NULL)
				iflist[nlm_index] = ifname;
			      strncpy (ifa->ifa_name, rtadata, rtapayload);
			      ifa->ifa_name[rtapayload] = '\0';
			      ifname += NLMSG_ALIGN (rtapayload + 1);
			    }
			  break;
			case IFA_UNSPEC:
			  break;
			case IFA_CACHEINFO:
			  break;
			default:
				;
			}
		    }
		}
	      if (nlh->nlmsg_type == RTM_NEWADDR && nlm_family != AF_PACKET)
		{
		  if (!ifamap.local)
		    {
		      ifamap.local = ifamap.address;
		      ifamap.local_len = ifamap.address_len;
		    }
		  if (!ifamap.address)
		    {
		      ifamap.address = ifamap.local;
		      ifamap.address_len = ifamap.local_len;
		    }
		  if (ifamap.address_len != ifamap.local_len ||
		      (ifamap.address != NULL &&
		       memcmp (ifamap.address, ifamap.local,
			       ifamap.address_len)))
		    {
		      /* p2p; address is peer and local is ours */
		      ifamap.broadcast = ifamap.address;
		      ifamap.broadcast_len = ifamap.address_len;
		      ifamap.address = ifamap.local;
		      ifamap.address_len = ifamap.local_len;
		    }
		  if (ifamap.address)
		    {
#ifndef IFA_NETMASK
		      sockaddr_size =
			NLMSG_ALIGN (ifa_sa_len
				     (nlm_family, ifamap.address_len));
#endif
		      if (!build)
			dlen +=
			  NLMSG_ALIGN (ifa_sa_len
				       (nlm_family, ifamap.address_len));
		      else
			{
			  ifa->ifa_addr = (struct sockaddr *) data;
			  ifa_make_sockaddr (nlm_family, ifa->ifa_addr,
					     ifamap.address,
					     ifamap.address_len, nlm_scope,
					     nlm_index);
			  data +=
			    NLMSG_ALIGN (ifa_sa_len
					 (nlm_family, ifamap.address_len));
			}
		    }
#ifdef IFA_NETMASK
		  if (ifamap.netmask)
		    {
		      if (!build)
			dlen +=
			  NLMSG_ALIGN (ifa_sa_len
				       (nlm_family, ifamap.netmask_len));
		      else
			{
			  ifa->ifa_netmask = (struct sockaddr *) data;
			  ifa_make_sockaddr (nlm_family, ifa->ifa_netmask,
					     ifamap.netmask,
					     ifamap.netmask_len, nlm_scope,
					     nlm_index);
			  data +=
			    NLMSG_ALIGN (ifa_sa_len
					 (nlm_family, ifamap.netmask_len));
			}
		    }
#endif
		  if (ifamap.broadcast)
		    {
		      if (!build)
			dlen +=
			  NLMSG_ALIGN (ifa_sa_len
				       (nlm_family, ifamap.broadcast_len));
		      else
			{
			  ifa->ifa_broadaddr = (struct sockaddr *) data;
			  ifa_make_sockaddr (nlm_family, ifa->ifa_broadaddr,
					     ifamap.broadcast,
					     ifamap.broadcast_len, nlm_scope,
					     nlm_index);
			  data +=
			    NLMSG_ALIGN (ifa_sa_len
					 (nlm_family, ifamap.broadcast_len));
			}
		    }
#ifdef HAVE_IFADDRS_IFA_ANYCAST
		  if (ifamap.anycast)
		    {
		      if (!build)
			dlen +=
			  NLMSG_ALIGN (ifa_sa_len
				       (nlm_family, ifamap.anycast_len));
		      else
			{
			  ifa->ifa_anycast = (struct sockaddr *) data;
			  ifa_make_sockaddr (nlm_family, ifa->ifa_anyaddr,
					     ifamap.anycast,
					     ifamap.anycast_len, nlm_scope,
					     nlm_index);
			  data +=
			    NLMSG_ALIGN (ifa_sa_len
					 (nlm_family, ifamap.anycast_len));
			}
		    }
#endif
		}
	      if (!build)
		{
#ifndef IFA_NETMASK
		  dlen += sockaddr_size;
#endif
		  icnt++;
		}
	      else
		{
		  if (ifa->ifa_name == NULL)
		    ifa->ifa_name = iflist[nlm_index];
#ifndef IFA_NETMASK
		  if (ifa->ifa_addr &&
		      ifa->ifa_addr->sa_family != AF_UNSPEC &&
		      ifa->ifa_addr->sa_family != AF_PACKET)
		    {
		      ifa->ifa_netmask = (struct sockaddr *) data;
		      ifa_make_sockaddr_mask (ifa->ifa_addr->sa_family,
					      ifa->ifa_netmask,
					      nlm_prefixlen);
		    }
		  data += sockaddr_size;
#endif
		  ifl = ifa++;
		}
	    }
	}
      if (!build)
	{
	  if (icnt == 0 && (dlen + nlen + xlen == 0))
	    {
	      if (ifap != NULL)
		*ifap = NULL;
	      break;		/* cannot found any addresses */
	    }
	}
      else
	free_data (NULL, ifdata);
    }

/* ---------------------------------- */
  /* Finalize */
  free_nlmsglist (nlmsg_list);
  nl_close (sd);
  return 0;
}

/* ---------------------------------------------------------------------- */
void
freeifaddrs (struct ifaddrs *ifa)
{
  free (ifa);
}
