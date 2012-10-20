/* dnsmasq is Copyright (c) 2000-2012 Simon Kelley

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
  if (index == 0 || !if_indextoname(index, name))
    return 0;

  return 1;
}

#endif

int iface_check(int family, struct all_addr *addr, char *name)
{
  struct iname *tmp;
  int ret = 1;

  /* Note: have to check all and not bail out early, so that we set the
     "used" flags. */
  
  if (daemon->if_names || daemon->if_addrs)
    {
      ret = 0;

      for (tmp = daemon->if_names; tmp; tmp = tmp->next)
	if (tmp->name && (strcmp(tmp->name, name) == 0))
	  ret = tmp->used = 1;
	        
      for (tmp = daemon->if_addrs; tmp; tmp = tmp->next)
	if (tmp->addr.sa.sa_family == family)
	  {
	    if (family == AF_INET &&
		tmp->addr.in.sin_addr.s_addr == addr->addr.addr4.s_addr)
	      ret = tmp->used = 1;
#ifdef HAVE_IPV6
	    else if (family == AF_INET6 &&
		     IN6_ARE_ADDR_EQUAL(&tmp->addr.in6.sin6_addr, 
					&addr->addr.addr6))
	      ret = tmp->used = 1;
#endif
	  }          
    }
  
  for (tmp = daemon->if_except; tmp; tmp = tmp->next)
    if (tmp->name && (strcmp(tmp->name, name) == 0))
      ret = 0;
    
  return ret; 
}
      
static int iface_allowed(struct irec **irecp, int if_index, 
			 union mysockaddr *addr, struct in_addr netmask, int dad) 
{
  struct irec *iface;
  int fd, mtu = 0, loopback;
  struct ifreq ifr;
  int tftp_ok = !!option_bool(OPT_TFTP);
  int dhcp_ok = 1;
#ifdef HAVE_DHCP
  struct iname *tmp;
#endif

  /* check whether the interface IP has been added already 
     we call this routine multiple times. */
  for (iface = *irecp; iface; iface = iface->next) 
    if (sockaddr_isequal(&iface->addr, addr))
      {
	iface->dad = dad;
	return 1;
      }

  if ((fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1 ||
      !indextoname(fd, if_index, ifr.ifr_name) ||
      ioctl(fd, SIOCGIFFLAGS, &ifr) == -1)
    {
      if (fd != -1)
	{
	  int errsave = errno;
	  close(fd);
	  errno = errsave;
	}
      return 0;
    }
   
  loopback = ifr.ifr_flags & IFF_LOOPBACK;
  
  if (loopback)
     dhcp_ok = 0;

  if (ioctl(fd, SIOCGIFMTU, &ifr) != -1)
    mtu = ifr.ifr_mtu;
  
  close(fd);
  
  /* If we are restricting the set of interfaces to use, make
     sure that loopback interfaces are in that set. */
  if (daemon->if_names && loopback)
    {
      struct iname *lo;
      for (lo = daemon->if_names; lo; lo = lo->next)
	if (lo->name && strcmp(lo->name, ifr.ifr_name) == 0)
	  break;
      
      if (!lo && 
	  (lo = whine_malloc(sizeof(struct iname))) &&
	  (lo->name = whine_malloc(strlen(ifr.ifr_name)+1)))
	{
	  strcpy(lo->name, ifr.ifr_name);
	  lo->used = 1;
	  lo->next = daemon->if_names;
	  daemon->if_names = lo;
	}
    }
  
  if (addr->sa.sa_family == AF_INET &&
      !iface_check(AF_INET, (struct all_addr *)&addr->in.sin_addr, ifr.ifr_name))
    return 1;
  
#ifdef HAVE_DHCP
  for (tmp = daemon->dhcp_except; tmp; tmp = tmp->next)
    if (tmp->name && (strcmp(tmp->name, ifr.ifr_name) == 0))
      {
	tftp_ok = 0;
	dhcp_ok = 0;
      }
#endif
  
#ifdef HAVE_IPV6
  if (addr->sa.sa_family == AF_INET6 &&
      !iface_check(AF_INET6, (struct all_addr *)&addr->in6.sin6_addr, ifr.ifr_name))
    return 1;
#endif
  

  /* add to list */
  if ((iface = whine_malloc(sizeof(struct irec))))
    {
      iface->addr = *addr;
      iface->netmask = netmask;
      iface->tftp_ok = tftp_ok;
      iface->dhcp_ok = dhcp_ok;
      iface->mtu = mtu;
      iface->dad = dad;
      iface->done = 0;
      if ((iface->name = whine_malloc(strlen(ifr.ifr_name)+1)))
	{
	  strcpy(iface->name, ifr.ifr_name);
	  iface->next = *irecp;
	  *irecp = iface;
	  return 1;
	}
      free(iface);
    }
  
  errno = ENOMEM; 
  return 0;
}

#ifdef HAVE_IPV6
static int iface_allowed_v6(struct in6_addr *local, int prefix, 
			    int scope, int if_index, int dad, void *vparam)
{
  union mysockaddr addr;
  struct in_addr netmask; /* dummy */
  netmask.s_addr = 0;

  (void)prefix; /* warning */
  (void)scope; /* warning */
  
  memset(&addr, 0, sizeof(addr));
#ifdef HAVE_SOCKADDR_SA_LEN
  addr.in6.sin6_len = sizeof(addr.in6);
#endif
  addr.in6.sin6_family = AF_INET6;
  addr.in6.sin6_addr = *local;
  addr.in6.sin6_port = htons(daemon->port);
  addr.in6.sin6_scope_id = if_index;
  
  return iface_allowed((struct irec **)vparam, if_index, &addr, netmask, dad);
}
#endif

static int iface_allowed_v4(struct in_addr local, int if_index, 
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

  return iface_allowed((struct irec **)vparam, if_index, &addr, netmask, 0);
}
   
int enumerate_interfaces(void)
{
#ifdef HAVE_IPV6
  if (!iface_enumerate(AF_INET6, &daemon->interfaces, iface_allowed_v6))
    return 0; 
#endif

  return iface_enumerate(AF_INET, &daemon->interfaces, iface_allowed_v4); 
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
    enumerate_interfaces();
  
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


/* Use an IPv4 listener socket for ioctling */
struct in_addr get_ifaddr(char *intr)
{
  struct listener *l;
  struct ifreq ifr;
  struct sockaddr_in ret;
  
  ret.sin_addr.s_addr = -1;

  for (l = daemon->listeners; 
       l && (l->family != AF_INET || l->fd == -1);
       l = l->next);
  
  strncpy(ifr.ifr_name, intr, IF_NAMESIZE);
  ifr.ifr_addr.sa_family = AF_INET;
  
  if (l &&  ioctl(l->fd, SIOCGIFADDR, &ifr) != -1)
    memcpy(&ret, &ifr.ifr_addr, sizeof(ret)); 
  
  return ret.sin_addr;
}



