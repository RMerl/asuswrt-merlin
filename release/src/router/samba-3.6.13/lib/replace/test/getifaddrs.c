/*
 * Unix SMB/CIFS implementation.
 *
 * libreplace getifaddrs test
 *
 * Copyright (C) Michael Adam <obnox@samba.org> 2008
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef AUTOCONF_TEST
#include "replace.h"
#include "system/network.h"
#endif

#ifdef HAVE_INET_NTOP
#define rep_inet_ntop inet_ntop
#endif

static const char *format_sockaddr(struct sockaddr *addr,
				   char *addrstring,
				   socklen_t addrlen)
{
	const char *result = NULL;

	if (addr->sa_family == AF_INET) {
		result = rep_inet_ntop(AF_INET,
				       &((struct sockaddr_in *)addr)->sin_addr,
				       addrstring,
				       addrlen);
#ifdef HAVE_STRUCT_SOCKADDR_IN6
	} else if (addr->sa_family == AF_INET6) {
		result = rep_inet_ntop(AF_INET6,
				       &((struct sockaddr_in6 *)addr)->sin6_addr,
				       addrstring,
				       addrlen);
#endif
	}
	return result;
}

int getifaddrs_test(void)
{
	struct ifaddrs *ifs = NULL;
	struct ifaddrs *ifs_head = NULL;
	int ret;

	ret = getifaddrs(&ifs);
	ifs_head = ifs;
	if (ret != 0) {
		fprintf(stderr, "getifaddrs() failed: %s\n", strerror(errno));
		return 1;
	}

	while (ifs) {
		printf("%-10s ", ifs->ifa_name);
		if (ifs->ifa_addr != NULL) {
			char addrstring[INET6_ADDRSTRLEN];
			const char *result;

			result = format_sockaddr(ifs->ifa_addr,
						 addrstring,
						 sizeof(addrstring));
			if (result != NULL) {
				printf("IP=%s ", addrstring);
			}

			if (ifs->ifa_netmask != NULL) {
				result = format_sockaddr(ifs->ifa_netmask,
							 addrstring,
							 sizeof(addrstring));
				if (result != NULL) {
					printf("NETMASK=%s", addrstring);
				}
			} else {
				printf("AF=%d ", ifs->ifa_addr->sa_family);
			}
		} else {
			printf("<no address>");
		}

		printf("\n");
		ifs = ifs->ifa_next;
	}

	freeifaddrs(ifs_head);

	return 0;
}
