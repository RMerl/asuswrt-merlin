/* $Id: upnputils.c,v 1.5 2012/05/24 16:51:09 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2012 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#include "config.h"

#include <stdio.h>
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

