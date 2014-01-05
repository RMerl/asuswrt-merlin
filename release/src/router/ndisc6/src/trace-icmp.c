/*
 * trace-icmp.c - ICMP code for IPv6 traceroute tool
 * $Id: trace-icmp.c 624 2008-05-01 14:26:09Z remi $
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

#include <string.h>
#include <stdbool.h>
#include <stdint.h> // uint16_t

#include <unistd.h> // getpid()
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>

#include "traceroute.h"


/* ICMPv6 Echo probes */
static ssize_t
send_echo_probe (int fd, unsigned ttl, unsigned n, size_t plen, uint16_t port)
{
	if (plen < sizeof (struct icmp6_hdr))
		plen = sizeof (struct icmp6_hdr);

	struct
	{
		struct icmp6_hdr ih;
		uint8_t payload[plen - sizeof (struct icmp6_hdr)];
	} packet;
	memset (&packet, 0, plen);

	packet.ih.icmp6_type = ICMP6_ECHO_REQUEST;
	packet.ih.icmp6_id = htons (getpid ());
	packet.ih.icmp6_seq = htons ((ttl << 8) | (n & 0xff));
	(void)port;

	return send_payload (fd, &packet.ih, plen, ttl);
}


static ssize_t
parse_echo_reply (const void *data, size_t len, int *ttl, unsigned *n,
                  uint16_t port)
{
	const struct icmp6_hdr *pih = (const struct icmp6_hdr *)data;

	if ((len < sizeof (*pih))
	 || (pih->icmp6_type != ICMP6_ECHO_REPLY)
	 || (pih->icmp6_id != htons (getpid ())))
		return -1;

	(void)port;

	*ttl = ntohs (pih->icmp6_seq) >> 8;
	*n = ntohs (pih->icmp6_seq) & 0xff;
	return 0;
}


static ssize_t
parse_echo_error (const void *data, size_t len, int *ttl, unsigned *n,
                  uint16_t port)
{
	const struct icmp6_hdr *pih = (const struct icmp6_hdr *)data;

	if ((len < sizeof (*pih))
	 || (pih->icmp6_type != ICMP6_ECHO_REQUEST) || (pih->icmp6_code)
	 || (pih->icmp6_id != htons (getpid ())))
		return -1;

	(void)port;

	*ttl = ntohs (pih->icmp6_seq) >> 8;
	*n = ntohs (pih->icmp6_seq) & 0xff;
	return 0;
}


const tracetype echo_type =
	{ SOCK_DGRAM, IPPROTO_ICMPV6, -1 /* checksum auto-set for ICMPv6 */,
	  send_echo_probe, parse_echo_reply, parse_echo_error };
