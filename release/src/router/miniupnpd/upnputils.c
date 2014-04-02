/* $Id: upnputils.c,v 1.8 2014/02/05 17:00:26 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2014 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef AF_LINK
#include <net/if_dl.h>
#endif

#include "upnputils.h"
#include "upnpglobalvars.h"
#ifdef ENABLE_IPV6
#include "getroute.h"
#endif

int
sockaddr_to_string(const struct sockaddr * addr, char * str, size_t size)
{
	char buffer[64];
	unsigned short port = 0;
	int n = -1;

	switch(addr->sa_family)
	{
	case AF_INET6:
		inet_ntop(addr->sa_family,
		          &((struct sockaddr_in6 *)addr)->sin6_addr,
		          buffer, sizeof(buffer));
		port = ntohs(((struct sockaddr_in6 *)addr)->sin6_port);
		n = snprintf(str, size, "[%s]:%hu", buffer, port);
		break;
	case AF_INET:
		inet_ntop(addr->sa_family,
		          &((struct sockaddr_in *)addr)->sin_addr,
		          buffer, sizeof(buffer));
		port = ntohs(((struct sockaddr_in *)addr)->sin_port);
		n = snprintf(str, size, "%s:%hu", buffer, port);
		break;
#ifdef AF_LINK
#if defined(__sun)
		/* solaris does not seem to have link_ntoa */
		/* #define link_ntoa _link_ntoa	*/
#define link_ntoa(x) "dummy-link_ntoa"
#endif
	case AF_LINK:
		{
			struct sockaddr_dl * sdl = (struct sockaddr_dl *)addr;
			n = snprintf(str, size, "index=%hu type=%d %s",
			             sdl->sdl_index, sdl->sdl_type,
			             link_ntoa(sdl));
		}
		break;
#endif
	default:
		n = snprintf(str, size, "unknown address family %d", addr->sa_family);
#if 0
		n = snprintf(str, size, "unknown address family %d "
		             "%02x %02x %02x %02x %02x %02x %02x %02x",
		             addr->sa_family,
		             addr->sa_data[0], addr->sa_data[1], (unsigned)addr->sa_data[2], addr->sa_data[3],
		             addr->sa_data[4], addr->sa_data[5], (unsigned)addr->sa_data[6], addr->sa_data[7]);
#endif
	}
	return n;
}


int
set_non_blocking(int fd)
{
	int flags = fcntl(fd, F_GETFL);
	if(flags < 0)
		return 0;
	if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
		return 0;
	return 1;
}

struct lan_addr_s *
get_lan_for_peer(const struct sockaddr * peer)
{
	struct lan_addr_s * lan_addr = NULL;
	char dbg_str[64];

#ifdef ENABLE_IPV6
	if(peer->sa_family == AF_INET6)
	{
		struct sockaddr_in6 * peer6 = (struct sockaddr_in6 *)peer;
		if(IN6_IS_ADDR_V4MAPPED(&peer6->sin6_addr))
		{
			struct in_addr peer_addr;
			memcpy(&peer_addr, &peer6->sin6_addr.s6_addr[12], 4);
			for(lan_addr = lan_addrs.lh_first;
			    lan_addr != NULL;
			    lan_addr = lan_addr->list.le_next)
			{
				if( (peer_addr.s_addr & lan_addr->mask.s_addr)
				   == (lan_addr->addr.s_addr & lan_addr->mask.s_addr))
					break;
			}
		}
		else
		{
			int index = -1;
			if(peer6->sin6_scope_id > 0)
				index = (int)peer6->sin6_scope_id;
			else
			{
				if(get_src_for_route_to(peer, NULL, NULL, &index) < 0)
					return NULL;
			}
			syslog(LOG_DEBUG, "%s looking for LAN interface index=%d",
			       "get_lan_for_peer()", index);
			for(lan_addr = lan_addrs.lh_first;
			    lan_addr != NULL;
			    lan_addr = lan_addr->list.le_next)
			{
				syslog(LOG_DEBUG,
				       "ifname=%s index=%u str=%s addr=%08x mask=%08x",
				       lan_addr->ifname, lan_addr->index,
				       lan_addr->str,
				       ntohl(lan_addr->addr.s_addr),
				       ntohl(lan_addr->mask.s_addr));
				if(index == (int)lan_addr->index)
					break;
			}
		}
	}
	else if(peer->sa_family == AF_INET)
	{
#endif
		for(lan_addr = lan_addrs.lh_first;
		    lan_addr != NULL;
		    lan_addr = lan_addr->list.le_next)
		{
			if( (((const struct sockaddr_in *)peer)->sin_addr.s_addr & lan_addr->mask.s_addr)
			   == (lan_addr->addr.s_addr & lan_addr->mask.s_addr))
				break;
		}
#ifdef ENABLE_IPV6
	}
#endif

	sockaddr_to_string(peer, dbg_str, sizeof(dbg_str));
	if(lan_addr) {
		syslog(LOG_DEBUG, "%s: %s found in LAN %s %s",
		       "get_lan_for_peer()", dbg_str,
		       lan_addr->ifname, lan_addr->str);
	} else {
		syslog(LOG_DEBUG, "%s: %s not found !", "get_lan_for_peer()",
		       dbg_str);
	}
	return lan_addr;
}

