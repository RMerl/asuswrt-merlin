/*
 * traceroute.h - TCP/IPv6 traceroute tool common header
 * $Id: traceroute.h 483 2007-08-08 15:09:36Z remi $
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

#ifndef NDISC6_TRACEROUTE_H
# define NDISC6_TRACEROUTE_H

typedef ssize_t (*trace_send_t) (int fd, unsigned ttl, unsigned n,
                                 size_t plen, uint16_t port);

typedef ssize_t (*trace_parser_t) (const void *restrict data, size_t len,
                                   int *restrict ttl,
                                   unsigned *restrict n, uint16_t port);

typedef struct tracetype
{
	int gai_socktype;
	int protocol;
	int checksum_offset;
	trace_send_t send_probe;
	trace_parser_t parse_resp, parse_err;
} tracetype;

# ifdef __cplusplus
extern "C" {
# endif

ssize_t send_payload (int fd, const void *payload, size_t length, int hlim);

# ifdef __cplusplus
}
#endif

extern bool ecn;
extern uint16_t sport;

extern const tracetype udp_type, udplite_type, echo_type, syn_type, ack_type;

#ifndef IPPROTO_UDPLITE
# define IPPROTO_UDPLITE 136
#endif

#endif
