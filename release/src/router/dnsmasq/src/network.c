/* dnsmasq is Copyright (c) 2000-2013 Simon Kelley

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 dated June, 1991, or
   (at your option) version 3 dated 29 June, 2007.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
     
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "dnsmasq.h"

#ifdef HAVE_LINUX_NETWORK

int indextoname(int fd, int index, char *name)
{
  struct ifreq ifr;
  
  if (index == 0)
    return 0;

  ifr.ifr_ifindex = index;
  if (ioctl(fd, SIOCGIFNAME, &ifr) == -1)
    return 0;

  strncpy(name, ifr.ifr_name, IF_NAMESIZE);

  return 1;
}


#elif defined(HAVE_SOLARIS_NETWORK)

#include <zone.h>
#include <alloca.h>
#ifndef LIFC_UNDER_IPMP
#  define LIFC_UNDER_IPMP 0
#endif

int indextoname(int fd, int index, char *name)
{
  int64_t lifc_flags;
  struct lifnum lifn;
  int numifs, bufsize, i;
  struct lifconf lifc;
  struct lifreq *lifrp;
  
  if (index == 0)
    return 0;
  
  if (getzoneid() == GLOBAL_ZONEID) 
    {
      if (!if_indextoname(index, name))
	return 0;
      return 1;
    }
  
  lifc_flags = LIFC_NOXMIT | LIFC_TEMPORARY | LIFC_ALLZONES | LIFC_UNDER_IPMP;
  lifn.lifn_family = AF_UNSPEC;
  lifn.lifn_flags = lifc_flags;
  if (ioctl(fd, SIOCGLIFNUM, &lifn) < 0) 
    return 0;
  
  numifs = lifn.lifn_count;
  bufsize = numifs * sizeof(struct lifreq);
  
  lifc.lifc_family = AF_UNSPEC;
  lifc.lifc_flags = lifc_flags;
  lifc.lifc_len = bufsize;
  lifc.lifc_buf = alloca(bufsize);
  
  if (ioctl(fd, SIOCGLIFCONF, &lifc) < 0)  
    return 0;
  
  lifrp = lifc.lifc_req;
  for (i = lifc.lifc_len / sizeof(struct lifreq); i; i--, lifrp++) 
    {
      struct lifreq lifr;
      strncpy(lifr.lifr_name, lifrp->lifr_name, IF_NAMESIZE);
      if (ioctl(fd, SIOCGLIFINDEX, &lifr) < 0) 
	return 0;
      
      if (lifr.lifr_index == index) {
	strncpy(name, lifr.lifr_name, IF_NAMESIZE);
	return 1;
      }
    }
  return 0;
}


#else

int indextoname(int fd, int index, char *name)
{ 
  (void)fd;

  if (index == 0 || !if_indextoname(index, name))
    return 0;

  return 1;
}

#endif

int iface_check(int family, struct all_addr *addr, char *name, int *auth)
{
  struct iname *tmp;
  int ret = 1, match_addr = 0;

  /* Note: have to check all and not bail out early, so that we set the
     "used" flags. */
  
  if (auth)
    *auth = 0;
  
  if (daemon->if_names || daemon->if_addrs)
    {
      ret = 0;

      for (tmp = daemon->if_names; tmp; tmp = tmp->next)
	if (tmp->name && wildcard_match(tmp->name, name))
	  ret = tmp->used = 1;
	        
      if (addr)
	for (tmp = daemon->if_addrs; tmp; tmp = tmp->next)
	  if (tmp->addr.sa.sa_family == family)
	    {
	      if (family == AF_INET &&
		  tmp->addr.in.sin_addr.s_addr == addr->addr.addr4.s_addr)
		ret = match_addr = tmp->used = 1;
#ifdef HAVE_IPV6
	      else if (family == AF_INET6 &&
		       IN6_ARE_ADDR_EQUAL(&tmp->addr.in6.sin6_addr, 
					  &addr->addr.addr6))
		ret = match_addr = tmp->used = 1;
#endif
	    }          
    }
  
  if (!match_addr)
    for (tmp = daemon->if_except; tmp; tmp = tmp->next)
      if (tmp->name && wildcard_match(tmp->name, name))
	ret = 0;
    

  for (tmp = daemon->authinterface; tmp; tmp = tmp->next)
    if (tmp->name)
      {
	if (strcmp(tmp->name, name) == 0)
	  break;
      }
    else if (addr && tmp->addr.sa.sa_family == AF_INET && family == AF_INET &&
	     tmp->addr.in.sin_addr.s_addr == addr->addr.addr4.s_addr)
      break;
#ifdef HAVE_IPV6
    else if (addr && tmp->addr.sa.sa_family == AF_INET6 && family == AF_INET6 &&
	     IN6_ARE_ADDR_EQUAL(&tmp->addr.in6.sin6_addr, &addr->addr.addr6))
      break;
#endif      

  if (tmp && auth) 
    {
      *auth = 1;
      ret = 1;
    }

  return ret; 
}


/* Fix for problem that the kernel sometimes reports the loopback inerface as the
   arrival interface when a packet originates locally, even when sent to address of 
   an interface other than the loopback. Accept packet if it arrived via a loopback 
   interface, even when we're not accepting packets that way, as long as the destination
   address is one we're believing. Interface list must be up-to-date before calling. */
int loopback_exception(int fd, int family, struct all_addr *addr, char *name)    
{
  struct ifreq ifr;
  struct irec *iface;

  strncpy(ifr.ifr_name, name, IF_NAMESIZE);
  if (ioctl(fd, SIOCGIFFLAGS, &ifr) != -1 &&
      ifr.ifr_flags & IFF_LOOPBACK)
    {
      for (iface = daemon->interfaces; iface; iface = iface->next)
	if (iface->addr.sa.sa_family == family)
	  {
	    if (family == AF_INET)
	      {
		if (iface->addr.in.sin_addr.s_addr == addr->addr.addr4.s_addr)
		  return 1;
	      }
#ifdef HAVE_IPV6
	    else if (IN6_ARE_ADDR_EQUAL(&iface->addr.in6.sin6_addr, &addr->addr.addr6))
	      return 1;
#endif
	    
	  }
    }
  return 0;
}

/* If we're configured with something like --interface=eth0:0 then we'll listen correctly
   on the relevant address, but the name of the arrival interface, derived from the
   index won't match the config. Check that we found an interface address for the arrival 
   interface: daemon->interfaces must be up-to-date. */
int label_exception(int index, int family, struct all_addr *addr)
{
  struct irec *iface;

  /* labels only supported on IPv4 addresses. */
  if (family != AF_INET)
    return 0;

  for (iface = daemon->interfaces; iface; iface = iface->next)
    if (iface->index == index && iface->addr.sa.sa_family == AF_INET &&
	iface->addr.in.sin_addr.s_addr == addr->addr.addr4.s_addr)
      return 1;

  return 0;
}

struct iface_param {
  struct addrlist *spare;
  int fd;
};

static int iface_allowed(struct iface_param *param, int if_index, char *label,
			 union mysockaddr *addr, struct in_addr netmask, int dad) 
{
  struct irec *iface;
  int mtu = 0, loopback;
  struct ifreq ifr;
  int tftp_ok = !!option_bool(OPT_TFTP);
  int dhcp_ok = 1;
  int auth_dns = 0;
#ifdef HAVE_DHCP
  struct iname *tmp;
#endif

  if (!indextoname(param->fd, if_index, ifr.ifr_name) ||
      ioctl(param->fd, SIOCGIFFLAGS, &ifr) == -1)
    return 0;
   
  loopback = ifr.ifr_flags & IFF_LOOPBACK;
  
  if (loopback)
    dhcp_ok = 0;
  
  if (ioctl(param->fd, SIOCGIFMTU, &ifr) != -1)
    mtu = ifr.ifr_mtu;
  
  if (!label)
    label = ifr.ifr_name;

  
  /* Update addresses from interface_names. These are a set independent
     of the set we're listening on. */  
#ifdef HAVE_IPV6
  if (addr->sa.sa_family != AF_INET6 || !IN6_IS_ADDR_LINKLOCAL(&addr->in6.sin6_addr))
#endif
    {
      struct interface_name *int_name;
      struct addrlist *al;

      for (int_name = daemon->int_names; int_name; int_name = int_name->next)
	if (strncmp(label, int_name->intr, IF_NAMESIZE) == 0)
	  {
	    if (param->spare)
	      {
		al = param->spare;
		param->spare = al->next;
	      }
	    else
	      al = whine_malloc(sizeof(struct addrlist));
	    
	    if (al)
	      {
		if (addr->sa.sa_family == AF_INET)
		  {
		    al->addr.addr.addr4 = addr->in.sin_addr;
		    al->next = int_name->addr4;
		    int_name->addr4 = al;
		  }
#ifdef HAVE_IPV6
		else
		 {
		    al->addr.addr.addr6 = addr->in6.sin6_addr;
		    al->next = int_name->addr6;
		    int_name->addr6 = al;
		 } 
#endif
	      }
	  }
    }
 
  /* check whether the interface IP has been added already 
     we call this routine multiple times. */
  for (iface = daemon->interfaces; iface; iface = iface->next) 
    if (sockaddr_isequal(&iface->addr, addr))
      {
	iface->dad = dad;
	return 1;
      }

 /* If we are restricting the set of interfaces to use, make
     sure that loopback interfaces are in that set. */
  if (daemon->if_names && loopback)
    {
      struct iname *lo;
      for (lo = daemon->if_names; lo; lo = lo->next)
	if (lo->name && strcmp(lo->name, ifr.ifr_name) == 0)
	  break;
      
      if (!lo && (lo = whine_malloc(sizeof(struct iname)))) 
	{
	  if ((lo->name = whine_malloc(strlen(ifr.ifr_name)+1)))
	    {
	      strcpy(lo->name, ifr.ifr_name);
	      lo->used = 1;
	      lo->next = daemon->if_names;
	      daemon->if_names = lo;
	    }
	  else
	    free(lo);
	}
    }
  
  if (addr->sa.sa_family == AF_INET &&
      !iface_check(AF_INET, (struct all_addr *)&addr->in.sin_addr, label, &auth_dns))
    return 1;

#ifdef HAVE_IPV6
  if (addr->sa.sa_family == AF_INET6 &&
      !iface_check(AF_INET6, (struct all_addr *)&addr->in6.sin6_addr, label, &auth_dns))
    return 1;
#endif
    
#ifdef HAVE_DHCP
  /* No DHCP where we're doing auth DNS. */
  if (auth_dns)
    {
      tftp_ok = 0;
      dhcp_ok = 0;
    }
  else
    for (tmp = daemon->dhcp_except; tmp; tmp = tmp->next)
      if (tmp->name && wildcard_match(tmp->name, ifr.ifr_name))
	{
	  tftp_ok = 0;
	  dhcp_ok = 0;
	}
#endif
 
  
  if (daemon->tftp_interfaces)
    {
      /* dedicated tftp interface list */
      tftp_ok = 0;
      for (tmp = daemon->tftp_interfaces; tmp; tmp = tmp->next)
	if (tmp->name && wildcard_match(tmp->name, ifr.ifr_name))
	  tftp_ok = 1;
    }
  
  /* add to list */
  if ((iface = whine_malloc(sizeof(struct irec))))
    {
      iface->addr = *addr;
      iface->netmask = netmask;
      iface->tftp_ok = tftp_ok;
      iface->dhcp_ok = dhcp_ok;
      iface->dns_auth = auth_dns;
      iface->mtu = mtu;
      iface->dad = dad;
      iface->done = iface->multicast_done = 0;
      iface->index = if_index;
      if ((iface->name = whine_malloc(strlen(ifr.ifr_name)+1)))
	{
	  strcpy(iface->name, ifr.ifr_name);
	  iface->next = daemon->interfaces;
	  daemon->interfaces = iface;
	  return 1;
	}
      free(iface);

    }
  
  errno = ENOMEM; 
  return 0;
}

#ifdef HAVE_IPV6
static int iface_allowed_v6(struct in6_addr *local, int prefix, 
			    int scope, int if_index, int flags, 
			    int preferred, int valid, void *vparam)
{
  union mysockaddr addr;
  struct in_addr netmask; /* dummy */
  netmask.s_addr = 0;

  (void)prefix; /* warning */
  (void)scope; /* warning */
  (void)preferred;
  (void)valid;
  
  memset(&addr, 0, sizeof(addr));
#ifdef HAVE_SOCKADDR_SA_LEN
  addr.in6.sin6_len = sizeof(addr.in6);
#endif
  addr.in6.sin6_family = AF_INET6;
  addr.in6.sin6_addr = *local;
  addr.in6.sin6_port = htons(daemon->port);
  addr.in6.sin6_scope_id = if_index;
  
  return iface_allowed((struct iface_param *)vparam, if_index, NULL, &addr, netmask, !!(flags & IFACE_TENTATIVE));
}
#endif

static int iface_allowed_v4(struct in_addr local, int if_index, char *label,
			    struct in_addr netmask, struct in_addr broadcast, void *vparam)
{
  union mysockaddr addr;

  memset(&addr, 0, sizeof(addr));
#ifdef HAVE_SOCKADDR_SA_LEN
  addr.in.sin_len = sizeof(addr.in);
#endif
  addr.in.sin_family = AF_INET;
  addr.in.sin_addr = broadcast; /* warning */
  addr.in.sin_addr = local;
  addr.in.sin_port = htons(daemon->port);

  return iface_allowed((struct iface_param *)vparam, if_index, label, &addr, netmask, 0);
}
   
int enumerate_interfaces(int reset)
{
  static struct addrlist *spare = NULL;
  static int done = 0, active = 0;
  struct iface_param param;
  int errsave, ret = 1;
  struct addrlist *addr, *tmp;
  struct interface_name *intname;
  
  /* Do this max once per select cycle  - also inhibits netlink socket use
   in TCP child processes. */

  if (reset)
    {
      done = 0;
      return 1;
    }

  if (done || active)
    return 1;

  done = 1;

  /* protect against recusive calls from iface_enumerate(); */
  active = 1;

  if ((param.fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
    return 0;
 
  /* remove addresses stored against interface_names */
  for (intname = daemon->int_names; intname; intname = intname->next)
    {
      for (addr = intname->addr4; addr; addr = tmp)
	{
	  tmp = addr->next;
	  addr->next = spare;
	  spare = addr;
	}
      
      intname->addr4 = NULL;

#ifdef HAVE_IPV6
      for (addr = intname->addr6; addr; addr = tmp)
	{
	  tmp = addr->next;
	  addr->next = spare;
	  spare = addr;
	} 
      
      intname->addr6 = NULL;
#endif
    }
  
  param.spare = spare;
  
#ifdef HAVE_IPV6
  ret = iface_enumerate(AF_INET6, &param, iface_allowed_v6);
#endif

  if (ret)
    ret = iface_enumerate(AF_INET, &param, iface_allowed_v4); 
 
  errsave = errno;
  close(param.fd);
  errno = errsave;

  spare = param.spare;
  active = 0;

  return ret;
}

/* set NONBLOCK bit on fd: See Stevens 16.6 */
int fix_fd(int fd)
{
  int flags;

  if ((flags = fcntl(fd, F_GETFL)) == -1 ||
      fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    return 0;
  
  return 1;
}

static int make_sock(union mysockaddr *addr, int type, int dienow)
{
  int family = addr->sa.sa_family;
  int fd, rc, opt = 1;
  
  if ((fd = socket(family, type, 0)) == -1)
    {
      int port;
      char *s;

      /* No error if the kernel just doesn't support this IP flavour */
      if (errno == EPROTONOSUPPORT ||
	  errno == EAFNOSUPPORT ||
	  errno == EINVAL)
	return -1;
      
    err:
      port = prettyprint_addr(addr, daemon->addrbuff);
      if (!option_bool(OPT_NOWILD) && !option_bool(OPT_CLEVERBIND))
	sprintf(daemon->addrbuff, "port %d", port);
      s = _("failed to create listening socket for %s: %s");
      
      if (fd != -1)
	close (fd);
      
      if (dienow)
	{
	  /* failure to bind addresses given by --listen-address at this point
	     is OK if we're doing bind-dynamic */
	  if (!option_bool(OPT_CLEVERBIND))
	    die(s, daemon->addrbuff, EC_BADNET);
	}
      else
	my_syslog(LOG_WARNING, s, daemon->addrbuff, strerror(errno));
      
      return -1;
    }	
  
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1 || !fix_fd(fd))
    goto err;
  
#ifdef HAVE_IPV6
  if (family == AF_INET6 && setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) == -1)
    goto err;
#endif
  
  if ((rc = bind(fd, (struct sockaddr *)addr, sa_len(addr))) == -1)
    goto err;
  
  if (type == SOCK_STREAM)
    {
      if (listen(fd, 5) == -1)
	goto err;
    }
  else if (!option_bool(OPT_NOWILD))
    {
      if (family == AF_INET)
	{
#if defined(HAVE_LINUX_NETWORK) 
	  if (setsockopt(fd, IPPROTO_IP, IP_PKTINFO, &opt, sizeof(opt)) == -1)
	    goto err;
#elif defined(IP_RECVDSTADDR) && defined(IP_RECVIF)
	  if (setsockopt(fd, IPPROTO_IP, IP_RECVDSTADDR, &opt, sizeof(opt)) == -1 ||
	      setsockopt(fd, IPPROTO_IP, IP_RECVIF, &opt, sizeof(opt)) == -1)
	    goto err;
#endif
	}
#ifdef HAVE_IPV6
      else if (!set_ipv6pktinfo(fd))
	goto err;
#endif
    }
  
  return fd;
}

#ifdef HAVE_IPV6  
int set_ipv6pktinfo(int fd)
{
  int opt = 1;

  /* The API changed around Linux 2.6.14 but the old ABI is still supported:
     handle all combinations of headers and kernel.
     OpenWrt note that this fixes the problem addressed by your very broken patch. */
  daemon->v6pktinfo = IPV6_PKTINFO;
  
#ifdef IPV6_RECVPKTINFO
  if (setsockopt(fd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &opt, sizeof(opt)) != -1)
    return 1;
# ifdef IPV6_2292PKTINFO
  else if (errno == ENOPROTOOPT && setsockopt(fd, IPPROTO_IPV6, IPV6_2292PKTINFO, &opt, sizeof(opt)) != -1)
    {
      daemon->v6pktinfo = IPV6_2292PKTINFO;
      return 1;
    }
# endif 
#else
  if (setsockopt(fd, IPPROTO_IPV6, IPV6_PKTINFO, &opt, sizeof(opt)) != -1)
    return 1;
#endif

  return 0;
}
#endif


/* Find the interface on which a TCP connection arrived, if possible, or zero otherwise. */
int tcp_interface(int fd, int af)
{ 
  int if_index = 0;

#ifdef HAVE_LINUX_NETWORK
  int opt = 1;
  struct cmsghdr *cmptr;
  struct msghdr msg;
  
  /* use mshdr do that the CMSDG_* macros are available */
  msg.msg_control = daemon->packet;
  msg.msg_controllen = daemon->packet_buff_sz;
  
  /* we overwrote the buffer... */
  daemon->srv_save = NULL;
  
  if (af == AF_INET)
    {
      if (setsockopt(fd, IPPROTO_IP, IP_PKTINFO, &opt, sizeof(opt)) != -1 &&
	  getsockopt(fd, IPPROTO_IP, IP_PKTOPTIONS, msg.msg_control, (socklen_t *)&msg.msg_controllen) != -1)
	for (cmptr = CMSG_FIRSTHDR(&msg); cmptr; cmptr = CMSG_NXTHDR(&msg, cmptr))
	  if (cmptr->cmsg_level == IPPROTO_IP && cmptr->cmsg_type == IP_PKTINFO)
            {
              union {
                unsigned char *c;
                struct in_pktinfo *p;
              } p;
	      
	      p.c = CMSG_DATA(cmptr);
	      if_index = p.p->ipi_ifindex;
	    }
    }
#ifdef HAVE_IPV6
  else
    {
      /* Only the RFC-2292 API has the ability to find the interface for TCP connections,
	 it was removed in RFC-3542 !!!! 

	 Fortunately, Linux kept the 2292 ABI when it moved to 3542. The following code always
	 uses the old ABI, and should work with pre- and post-3542 kernel headers */

#ifdef IPV6_2292PKTOPTIONS   
#  define PKTOPTIONS IPV6_2292PKTOPTIONS
#else
#  define PKTOPTIONS IPV6_PKTOPTIONS
#endif

      if (set_ipv6pktinfo(fd) &&
	  getsockopt(fd, IPPROTO_IPV6, PKTOPTIONS, msg.msg_control, (socklen_t *)&msg.msg_controllen) != -1)
	{
          for (cmptr = CMSG_FIRSTHDR(&msg); cmptr; cmptr = CMSG_NXTHDR(&msg, cmptr))
            if (cmptr->cmsg_level == IPPROTO_IPV6 && cmptr->cmsg_type == daemon->v6pktinfo)
              {
                union {
                  unsigned char *c;
                  struct in6_pktinfo *p;
                } p;
                p.c = CMSG_DATA(cmptr);
		
		if_index = p.p->ipi6_ifindex;
              }
	}
    }
#endif /* IPV6 */
#endif /* Linux */
 
  return if_index;
}
      
static struct listener *create_listeners(union mysockaddr *addr, int do_tftp, int dienow)
{
  struct listener *l = NULL;
  int fd = -1, tcpfd = -1, tftpfd = -1;

  if (daemon->port != 0)
    {
      fd = make_sock(addr, SOCK_DGRAM, dienow);
      tcpfd = make_sock(addr, SOCK_STREAM, dienow);
    }
  
#ifdef HAVE_TFTP
  if (do_tftp)
    {
      if (addr->sa.sa_family == AF_INET)
	{
	  /* port must be restored to DNS port for TCP code */
	  short save = addr->in.sin_port;
	  addr->in.sin_port = htons(TFTP_PORT);
	  tftpfd = make_sock(addr, SOCK_DGRAM, dienow);
	  addr->in.sin_port = save;
	}
#  ifdef HAVE_IPV6
      else
	{
	  short save = addr->in6.sin6_port;
	  addr->in6.sin6_port = htons(TFTP_PORT);
	  tftpfd = make_sock(addr, SOCK_DGRAM, dienow);
	  addr->in6.sin6_port = save;
	}  
#  endif
    }
#endif

  if (fd != -1 || tcpfd != -1 || tftpfd != -1)
    {
      l = safe_malloc(sizeof(struct listener));
      l->next = NULL;
      l->family = addr->sa.sa_family;
      l->fd = fd;
      l->tcpfd = tcpfd;
      l->tftpfd = tftpfd;
    }

  return l;
}

void create_wildcard_listeners(void)
{
  union mysockaddr addr;
  struct listener *l, *l6;

  memset(&addr, 0, sizeof(addr));
#ifdef HAVE_SOCKADDR_SA_LEN
  addr.in.sin_len = sizeof(addr.in);
#endif
  addr.in.sin_family = AF_INET;
  addr.in.sin_addr.s_addr = INADDR_ANY;
  addr.in.sin_port = htons(daemon->port);

  l = create_listeners(&addr, !!option_bool(OPT_TFTP), 1);

#ifdef HAVE_IPV6
  memset(&addr, 0, sizeof(addr));
#  ifdef HAVE_SOCKADDR_SA_LEN
  addr.in6.sin6_len = sizeof(addr.in6);
#  endif
  addr.in6.sin6_family = AF_INET6;
  addr.in6.sin6_addr = in6addr_any;
  addr.in6.sin6_port = htons(daemon->port);
 
  l6 = create_listeners(&addr, !!option_bool(OPT_TFTP), 1);
  if (l) 
    l->next = l6;
  else 
    l = l6;
#endif

  daemon->listeners = l;
}

void create_bound_listeners(int dienow)
{
  struct listener *new;
  struct irec *iface;
  struct iname *if_tmp;

  for (iface = daemon->interfaces; iface; iface = iface->next)
    if (!iface->done && !iface->dad && 
	(new = create_listeners(&iface->addr, iface->tftp_ok, dienow)))
      {
	new->iface = iface;
	new->next = daemon->listeners;
	daemon->listeners = new;
	iface->done = 1;
      }

  /* Check for --listen-address options that haven't been used because there's
     no interface with a matching address. These may be valid: eg it's possible
     to listen on 127.0.1.1 even if the loopback interface is 127.0.0.1

     If the address isn't valid the bind() will fail and we'll die() 
     (except in bind-dynamic mode, when we'll complain but keep trying.)

     The resulting listeners have the ->iface field NULL, and this has to be
     handled by the DNS and TFTP code. It disables --localise-queries processing
     (no netmask) and some MTU login the tftp code. */

  for (if_tmp = daemon->if_addrs; if_tmp; if_tmp = if_tmp->next)
    if (!if_tmp->used && 
	(new = create_listeners(&if_tmp->addr, !!option_bool(OPT_TFTP), dienow)))
      {
	new->iface = NULL;
	new->next = daemon->listeners;
	daemon->listeners = new;
      }
}

int is_dad_listeners(void)
{
  struct irec *iface;
  
  if (option_bool(OPT_NOWILD))
    for (iface = daemon->interfaces; iface; iface = iface->next)
      if (iface->dad && !iface->done)
	return 1;
  
  return 0;
}

#ifdef HAVE_DHCP6
void join_multicast(int dienow)      
{
  struct irec *iface, *tmp;

  for (iface = daemon->interfaces; iface; iface = iface->next)
    if (iface->addr.sa.sa_family == AF_INET6 && iface->dhcp_ok && !iface->multicast_done)
      {
	/* There's an irec per address but we only want to join for multicast 
	   once per interface. Weed out duplicates. */
	for (tmp = daemon->interfaces; tmp; tmp = tmp->next)
	  if (tmp->multicast_done && tmp->index == iface->index)
	    break;
	
	iface->multicast_done = 1;
	
	if (!tmp)
	  {
	    struct ipv6_mreq mreq;
	    int err = 0;

	    mreq.ipv6mr_interface = iface->index;
	    
	    inet_pton(AF_INET6, ALL_RELAY_AGENTS_AND_SERVERS, &mreq.ipv6mr_multiaddr);
	    
	    if ((daemon->doing_dhcp6 || daemon->relay6) &&
		setsockopt(daemon->dhcp6fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) == -1)
	      err = 1;
	    
	    inet_pton(AF_INET6, ALL_SERVERS, &mreq.ipv6mr_multiaddr);
	    
	    if (daemon->doing_dhcp6 && 
		setsockopt(daemon->dhcp6fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) == -1)
	      err = 1;
	    
	    inet_pton(AF_INET6, ALL_ROUTERS, &mreq.ipv6mr_multiaddr);
	    
	    if (daemon->doing_ra &&
		setsockopt(daemon->icmp6fd, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) == -1)
	      err = 1;
	    
	    if (err)
	      {
		char *s = _("interface %s failed to join DHCPv6 multicast group: %s");
		if (dienow)
		  die(s, iface->name, EC_BADNET);
		else
		  my_syslog(LOG_ERR, s, iface->name, strerror(errno));
	      }
	  }
      }
}
#endif

/* return a UDP socket bound to a random port, have to cope with straying into
   occupied port nos and reserved ones. */
int random_sock(int family)
{
  int fd;

  if ((fd = socket(family, SOCK_DGRAM, 0)) != -1)
    {
      union mysockaddr addr;
      unsigned int ports_avail = 65536u - (unsigned short)daemon->min_port;
      int tries = ports_avail < 30 ? 3 * ports_avail : 100;

      memset(&addr, 0, sizeof(addr));
      addr.sa.sa_family = family;

      /* don't loop forever if all ports in use. */

      if (fix_fd(fd))
	while(tries--)
	  {
	    unsigned short port = rand16();
	    
	    if (daemon->min_port != 0)
	      port = htons(daemon->min_port + (port % ((unsigned short)ports_avail)));
	    
	    if (family == AF_INET) 
	      {
		addr.in.sin_addr.s_addr = INADDR_ANY;
		addr.in.sin_port = port;
#ifdef HAVE_SOCKADDR_SA_LEN
		addr.in.sin_len = sizeof(struct sockaddr_in);
#endif
	      }
#ifdef HAVE_IPV6
	    else
	      {
		addr.in6.sin6_addr = in6addr_any; 
		addr.in6.sin6_port = port;
#ifdef HAVE_SOCKADDR_SA_LEN
		addr.in6.sin6_len = sizeof(struct sockaddr_in6);
#endif
	      }
#endif
	    
	    if (bind(fd, (struct sockaddr *)&addr, sa_len(&addr)) == 0)
	      return fd;
	    
	    if (errno != EADDRINUSE && errno != EACCES)
	      break;
	  }

      close(fd);
    }

  return -1; 
}
  

int local_bind(int fd, union mysockaddr *addr, char *intname, int is_tcp)
{
  union mysockaddr addr_copy = *addr;

  /* cannot set source _port_ for TCP connections. */
  if (is_tcp)
    {
      if (addr_copy.sa.sa_family == AF_INET)
	addr_copy.in.sin_port = 0;
#ifdef HAVE_IPV6
      else
	addr_copy.in6.sin6_port = 0;
#endif
    }
  
  if (bind(fd, (struct sockaddr *)&addr_copy, sa_len(&addr_copy)) == -1)
    return 0;
    
#if defined(SO_BINDTODEVICE)
  if (intname[0] != 0 &&
      setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, intname, IF_NAMESIZE) == -1)
    return 0;
#endif

  return 1;
}

static struct serverfd *allocate_sfd(union mysockaddr *addr, char *intname)
{
  struct serverfd *sfd;
  int errsave;

  /* when using random ports, servers which would otherwise use
     the INADDR_ANY/port0 socket have sfd set to NULL */
  if (!daemon->osport && intname[0] == 0)
    {
      errno = 0;
      
      if (addr->sa.sa_family == AF_INET &&
	  addr->in.sin_addr.s_addr == INADDR_ANY &&
	  addr->in.sin_port == htons(0)) 
	return NULL;

#ifdef HAVE_IPV6
      if (addr->sa.sa_family == AF_INET6 &&
	  memcmp(&addr->in6.sin6_addr, &in6addr_any, sizeof(in6addr_any)) == 0 &&
	  addr->in6.sin6_port == htons(0)) 
	return NULL;
#endif
    }
      
  /* may have a suitable one already */
  for (sfd = daemon->sfds; sfd; sfd = sfd->next )
    if (sockaddr_isequal(&sfd->source_addr, addr) &&
	strcmp(intname, sfd->interface) == 0)
      return sfd;
  
  /* need to make a new one. */
  errno = ENOMEM; /* in case malloc fails. */
  if (!(sfd = whine_malloc(sizeof(struct serverfd))))
    return NULL;
  
  if ((sfd->fd = socket(addr->sa.sa_family, SOCK_DGRAM, 0)) == -1)
    {
      free(sfd);
      return NULL;
    }
  
  if (!local_bind(sfd->fd, addr, intname, 0) || !fix_fd(sfd->fd))
    { 
      errsave = errno; /* save error from bind. */
      close(sfd->fd);
      free(sfd);
      errno = errsave;
      return NULL;
    }
    
  strcpy(sfd->interface, intname); 
  sfd->source_addr = *addr;
  sfd->next = daemon->sfds;
  daemon->sfds = sfd;
  return sfd; 
}

/* create upstream sockets during startup, before root is dropped which may be needed
   this allows query_port to be a low port and interface binding */
void pre_allocate_sfds(void)
{
  struct server *srv;
  
  if (daemon->query_port != 0)
    {
      union  mysockaddr addr;
      memset(&addr, 0, sizeof(addr));
      addr.in.sin_family = AF_INET;
      addr.in.sin_addr.s_addr = INADDR_ANY;
      addr.in.sin_port = htons(daemon->query_port);
#ifdef HAVE_SOCKADDR_SA_LEN
      addr.in.sin_len = sizeof(struct sockaddr_in);
#endif
      allocate_sfd(&addr, "");
#ifdef HAVE_IPV6
      memset(&addr, 0, sizeof(addr));
      addr.in6.sin6_family = AF_INET6;
      addr.in6.sin6_addr = in6addr_any;
      addr.in6.sin6_port = htons(daemon->query_port);
#ifdef HAVE_SOCKADDR_SA_LEN
      addr.in6.sin6_len = sizeof(struct sockaddr_in6);
#endif
      allocate_sfd(&addr, "");
#endif
    }
  
  for (srv = daemon->servers; srv; srv = srv->next)
    if (!(srv->flags & (SERV_LITERAL_ADDRESS | SERV_NO_ADDR | SERV_USE_RESOLV | SERV_NO_REBIND)) &&
	!allocate_sfd(&srv->source_addr, srv->interface) &&
	errno != 0 &&
	option_bool(OPT_NOWILD))
      {
	prettyprint_addr(&srv->source_addr, daemon->namebuff);
	if (srv->interface[0] != 0)
	  {
	    strcat(daemon->namebuff, " ");
	    strcat(daemon->namebuff, srv->interface);
	  }
	die(_("failed to bind server socket for %s: %s"),
	    daemon->namebuff, EC_BADNET);
      }  
}


void check_servers(void)
{
  struct irec *iface;
  struct server *new, *tmp, *ret = NULL;
  int port = 0;

  /* interface may be new since startup */
  if (!option_bool(OPT_NOWILD))
    enumerate_interfaces(0);
  
  for (new = daemon->servers; new; new = tmp)
    {
      tmp = new->next;
      
      if (!(new->flags & (SERV_LITERAL_ADDRESS | SERV_NO_ADDR | SERV_USE_RESOLV | SERV_NO_REBIND)))
	{
	  port = prettyprint_addr(&new->addr, daemon->namebuff);

	  /* 0.0.0.0 is nothing, the stack treats it like 127.0.0.1 */
	  if (new->addr.sa.sa_family == AF_INET &&
	      new->addr.in.sin_addr.s_addr == 0)
	    {
	      free(new);
	      continue;
	    }

	  for (iface = daemon->interfaces; iface; iface = iface->next)
	    if (sockaddr_isequal(&new->addr, &iface->addr))
	      break;
	  if (iface)
	    {
	      my_syslog(LOG_WARNING, _("ignoring nameserver %s - local interface"), daemon->namebuff);
	      free(new);
	      continue;
	    }
	  
	  /* Do we need a socket set? */
	  if (!new->sfd && 
	      !(new->sfd = allocate_sfd(&new->source_addr, new->interface)) &&
	      errno != 0)
	    {
	      my_syslog(LOG_WARNING, 
			_("ignoring nameserver %s - cannot make/bind socket: %s"),
			daemon->namebuff, strerror(errno));
	      free(new);
	      continue;
	    }
	}
      
      /* reverse order - gets it right. */
      new->next = ret;
      ret = new;
      
      if (!(new->flags & SERV_NO_REBIND))
	{
	  if (new->flags & (SERV_HAS_DOMAIN | SERV_FOR_NODOTS | SERV_USE_RESOLV))
	    {
	      char *s1, *s2;
	      if (!(new->flags & SERV_HAS_DOMAIN))
		s1 = _("unqualified"), s2 = _("names");
	      else if (strlen(new->domain) == 0)
		s1 = _("default"), s2 = "";
	      else
		s1 = _("domain"), s2 = new->domain;
	      
	      if (new->flags & SERV_NO_ADDR)
		my_syslog(LOG_INFO, _("using local addresses only for %s %s"), s1, s2);
	      else if (new->flags & SERV_USE_RESOLV)
		my_syslog(LOG_INFO, _("using standard nameservers for %s %s"), s1, s2);
	      else if (!(new->flags & SERV_LITERAL_ADDRESS))
		my_syslog(LOG_INFO, _("using nameserver %s#%d for %s %s"), daemon->namebuff, port, s1, s2);
	    }
	  else if (new->interface[0] != 0)
	    my_syslog(LOG_INFO, _("using nameserver %s#%d(via %s)"), daemon->namebuff, port, new->interface); 
	  else
	    my_syslog(LOG_INFO, _("using nameserver %s#%d"), daemon->namebuff, port); 
	}
    }
  
  daemon->servers = ret;
}

/* Return zero if no servers found, in that case we keep polling.
   This is a protection against an update-time/write race on resolv.conf */
int reload_servers(char *fname)
{
  FILE *f;
  char *line;
  struct server *old_servers = NULL;
  struct server *new_servers = NULL;
  struct server *serv;
  int gotone = 0;

  /* buff happens to be MAXDNAME long... */
  if (!(f = fopen(fname, "r")))
    {
      my_syslog(LOG_ERR, _("failed to read %s: %s"), fname, strerror(errno));
      return 0;
    }
  
  /* move old servers to free list - we can reuse the memory 
     and not risk malloc if there are the same or fewer new servers. 
     Servers which were specced on the command line go to the new list. */
  for (serv = daemon->servers; serv;)
    {
      struct server *tmp = serv->next;
      if (serv->flags & SERV_FROM_RESOLV)
	{
	  serv->next = old_servers;
	  old_servers = serv; 
	  /* forward table rules reference servers, so have to blow them away */
	  server_gone(serv);
	}
      else
	{
	  serv->next = new_servers;
	  new_servers = serv;
	}
      serv = tmp;
    }
  
  while ((line = fgets(daemon->namebuff, MAXDNAME, f)))
    {
      union mysockaddr addr, source_addr;
      char *token = strtok(line, " \t\n\r");
      
      if (!token)
	continue;
      if (strcmp(token, "nameserver") != 0 && strcmp(token, "server") != 0)
	continue;
      if (!(token = strtok(NULL, " \t\n\r")))
	continue;
      
      memset(&addr, 0, sizeof(addr));
      memset(&source_addr, 0, sizeof(source_addr));
      
      if ((addr.in.sin_addr.s_addr = inet_addr(token)) != (in_addr_t) -1)
	{
#ifdef HAVE_SOCKADDR_SA_LEN
	  source_addr.in.sin_len = addr.in.sin_len = sizeof(source_addr.in);
#endif
	  source_addr.in.sin_family = addr.in.sin_family = AF_INET;
	  addr.in.sin_port = htons(NAMESERVER_PORT);
	  source_addr.in.sin_addr.s_addr = INADDR_ANY;
	  source_addr.in.sin_port = htons(daemon->query_port);
	}
#ifdef HAVE_IPV6
      else 
	{	
	  int scope_index = 0;
	  char *scope_id = strchr(token, '%');
	  
	  if (scope_id)
	    {
	      *(scope_id++) = 0;
	      scope_index = if_nametoindex(scope_id);
	    }
	  
	  if (inet_pton(AF_INET6, token, &addr.in6.sin6_addr) > 0)
	    {
#ifdef HAVE_SOCKADDR_SA_LEN
	      source_addr.in6.sin6_len = addr.in6.sin6_len = sizeof(source_addr.in6);
#endif
	      source_addr.in6.sin6_family = addr.in6.sin6_family = AF_INET6;
	      source_addr.in6.sin6_flowinfo = addr.in6.sin6_flowinfo = 0;
	      addr.in6.sin6_port = htons(NAMESERVER_PORT);
	      addr.in6.sin6_scope_id = scope_index;
	      source_addr.in6.sin6_addr = in6addr_any;
	      source_addr.in6.sin6_port = htons(daemon->query_port);
	      source_addr.in6.sin6_scope_id = 0;
	    }
	  else
	    continue;
	}
#else /* IPV6 */
      else
	continue;
#endif 

      if (old_servers)
	{
	  serv = old_servers;
	  old_servers = old_servers->next;
	}
      else if (!(serv = whine_malloc(sizeof (struct server))))
	continue;
      
      /* this list is reverse ordered: 
	 it gets reversed again in check_servers */
      serv->next = new_servers;
      new_servers = serv;
      serv->addr = addr;
      serv->source_addr = source_addr;
      serv->domain = NULL;
      serv->interface[0] = 0;
      serv->sfd = NULL;
      serv->flags = SERV_FROM_RESOLV;
      serv->queries = serv->failed_queries = 0;
      gotone = 1;
    }
  
  /* Free any memory not used. */
  while (old_servers)
    {
      struct server *tmp = old_servers->next;
      free(old_servers);
      old_servers = tmp;
    }

  daemon->servers = new_servers;
  fclose(f);

  return gotone;
}






