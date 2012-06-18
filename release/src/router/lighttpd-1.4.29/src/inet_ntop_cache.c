#include "base.h"
#include "inet_ntop_cache.h"
#include "sys-socket.h"

#include <sys/types.h>

#include <string.h>

const char * inet_ntop_cache_get_ip(server *srv, sock_addr *addr) {
#ifdef HAVE_IPV6
	size_t ndx = 0, i;
	for (i = 0; i < INET_NTOP_CACHE_MAX; i++) {
		if (srv->inet_ntop_cache[i].ts != 0 && srv->inet_ntop_cache[i].family == addr->plain.sa_family) {
			if (srv->inet_ntop_cache[i].family == AF_INET6 &&
			    0 == memcmp(srv->inet_ntop_cache[i].addr.ipv6.s6_addr, addr->ipv6.sin6_addr.s6_addr, 16)) {
				/* IPv6 found in cache */
				break;
			} else if (srv->inet_ntop_cache[i].family == AF_INET &&
				   srv->inet_ntop_cache[i].addr.ipv4.s_addr == addr->ipv4.sin_addr.s_addr) {
				/* IPv4 found in cache */
				break;

			}
		}
	}

	if (i == INET_NTOP_CACHE_MAX) {
		/* not found in cache */

		i = ndx;
		inet_ntop(addr->plain.sa_family,
			  addr->plain.sa_family == AF_INET6 ?
			  (const void *) &(addr->ipv6.sin6_addr) :
			  (const void *) &(addr->ipv4.sin_addr),
			  srv->inet_ntop_cache[i].b2, INET6_ADDRSTRLEN);

		srv->inet_ntop_cache[i].ts = srv->cur_ts;
		srv->inet_ntop_cache[i].family = addr->plain.sa_family;

		if (srv->inet_ntop_cache[i].family == AF_INET) {
			srv->inet_ntop_cache[i].addr.ipv4.s_addr = addr->ipv4.sin_addr.s_addr;
		} else if (srv->inet_ntop_cache[i].family == AF_INET6) {
			memcpy(srv->inet_ntop_cache[i].addr.ipv6.s6_addr, addr->ipv6.sin6_addr.s6_addr, 16);
		}
	}

	return srv->inet_ntop_cache[i].b2;
#else
	UNUSED(srv);
	return inet_ntoa(addr->ipv4.sin_addr);
#endif
}
