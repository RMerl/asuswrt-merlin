/*
 * traceroute.c - TCP/IPv6 traceroute tool
 * $Id: trace-udp.c 624 2008-05-01 14:26:09Z remi $
 */

/*************************************************************************
 *  Copyright © 2005-2006 Rémi Denis-Courmont.                           *
 *  This program is free software: you can redistribute and/or modify    *
 *  it under the terms of the GNU General Public License as published by *
 *  the Free Software Foundation, versions 2 or 3 of the license.        *
 *                                                                       *
 *  This program is distributed in the hope that it will be useful,      *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 *  GNU General Public License for more details.                         *
 *                                                                       *
 *  You should have received a copy of the GNU General Public License    *
 *  along with this program. If not, see <http://www.gnu.org/licenses/>. *
 *************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#undef _GNU_SOURCE
#define _BSD_SOURCE 1

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/socket.h> // SOCK_DGRAM
#include <netinet/in.h>
#include <netinet/udp.h>

#include "traceroute.h"


/* UDP probes (traditional traceroute) */
static ssize_t
send_udp_probe (int fd, unsigned ttl, unsigned n, size_t plen, uint16_t port)
{
	if (plen < sizeof (struct udphdr))
		plen = sizeof (struct udphdr);

	struct
	{
		struct udphdr uh;
		uint8_t payload[plen - sizeof (struct udphdr)];
	} packet;
	memset (&packet, 0, plen);

	(void)n;
	packet.uh.source = sport;
	packet.uh.dest = htons (ntohs (port) + ttl);
	/* For UDP-Lite we have full checksum coverage, if only because the
	 * IPV6_CHECKSUM setsockopt only supports full coverage. Hence
	 * we can set coverage to the length of the packet, even though zero
	 * would be more idiosyncrasic. */
	packet.uh.len = htons (plen);
	/*if (plen > sizeof (struct udphdr))
		packet.payload[0] = (uint8_t)ttl;*/

	return send_payload (fd, &packet, plen, ttl);
}


static ssize_t
parse_udp_error (const void *data, size_t len, int *ttl, unsigned *n,
                 uint16_t port)
{
	const struct udphdr *puh = (const struct udphdr *)data;
	uint16_t rport;

	if ((len < 4) || (puh->source != sport ))
		return -1;

	rport = ntohs (puh->dest);
	port = ntohs (port);
	if ((rport < port) || (rport > port + 255))
		return -1;

	*ttl = rport - port;
	*n = (unsigned)(-1);
	return 0;
}


const tracetype udp_type =
	{ SOCK_DGRAM, IPPROTO_UDP, 6,
	  send_udp_probe, NULL, parse_udp_error };
const tracetype udplite_type =
	{ SOCK_DGRAM, IPPROTO_UDPLITE, 6,
	  send_udp_probe, NULL, parse_udp_error };
