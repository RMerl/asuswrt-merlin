/*
 * Kernel routing table readup by getmsg(2)
 * Copyright (C) 1999 Michael Handler
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include <zebra.h>

#include "prefix.h"
#include "log.h"
#include "if.h"

#include "zebra/rib.h"

#include <sys/stream.h>
#include <sys/tihdr.h>

/* Solaris defines these in both <netinet/in.h> and <inet/in.h>, sigh */
#ifdef SUNOS_5
#include <sys/tiuser.h>
#ifndef T_CURRENT
#define T_CURRENT       MI_T_CURRENT
#endif /* T_CURRENT */
#ifndef IRE_CACHE
#define IRE_CACHE               0x0020  /* Cached Route entry */
#endif /* IRE_CACHE */
#ifndef IRE_HOST_REDIRECT
#define IRE_HOST_REDIRECT       0x0200  /* Host route entry from redirects */
#endif /* IRE_HOST_REDIRECT */
#ifndef IRE_CACHETABLE
#define IRE_CACHETABLE          (IRE_CACHE | IRE_BROADCAST | IRE_LOCAL | \
                                 IRE_LOOPBACK)
#endif /* IRE_CACHETABLE */
#undef IPOPT_EOL
#undef IPOPT_NOP
#undef IPOPT_LSRR
#undef IPOPT_RR
#undef IPOPT_SSRR
#endif /* SUNOS_5 */

#include <inet/common.h>
#include <inet/ip.h>
#include <inet/mib2.h>

/* device to read IP routing table from */
#ifndef _PATH_GETMSG_ROUTE
#define _PATH_GETMSG_ROUTE	"/dev/ip"
#endif /* _PATH_GETMSG_ROUTE */

#define RT_BUFSIZ		8192

void handle_route_entry (mib2_ipRouteEntry_t *routeEntry)
{
	struct prefix_ipv4	prefix;
 	struct in_addr		tmpaddr, gateway;
	u_char			zebra_flags = 0;

	if (routeEntry->ipRouteInfo.re_ire_type & IRE_CACHETABLE)
		return;

	if (routeEntry->ipRouteInfo.re_ire_type & IRE_HOST_REDIRECT)
		zebra_flags |= ZEBRA_FLAG_SELFROUTE;

	prefix.family = AF_INET;

	tmpaddr.s_addr = routeEntry->ipRouteDest;
	prefix.prefix = tmpaddr;

	tmpaddr.s_addr = routeEntry->ipRouteMask;
	prefix.prefixlen = ip_masklen (tmpaddr);

	gateway.s_addr = routeEntry->ipRouteNextHop;

	rib_add_ipv4 (ZEBRA_ROUTE_KERNEL, zebra_flags, &prefix,
		      &gateway, 0, 0, 0, 0);
}

void route_read ()
{
	char 			storage[RT_BUFSIZ];

	struct T_optmgmt_req	*TLIreq = (struct T_optmgmt_req *) storage;
	struct T_optmgmt_ack	*TLIack = (struct T_optmgmt_ack *) storage;
	struct T_error_ack	*TLIerr = (struct T_error_ack *) storage;

	struct opthdr		*MIB2hdr;

	mib2_ipRouteEntry_t	*routeEntry, *lastRouteEntry;

	struct strbuf		msgdata;
	int			flags, dev, retval, process;

	if ((dev = open (_PATH_GETMSG_ROUTE, O_RDWR)) == -1) {
		zlog_warn ("can't open %s: %s", _PATH_GETMSG_ROUTE,
			strerror (errno));
		return;
	}

	TLIreq->PRIM_type = T_OPTMGMT_REQ;
	TLIreq->OPT_offset = sizeof (struct T_optmgmt_req);
	TLIreq->OPT_length = sizeof (struct opthdr);
	TLIreq->MGMT_flags = T_CURRENT;

	MIB2hdr = (struct opthdr *) &TLIreq[1];

	MIB2hdr->level = MIB2_IP;
	MIB2hdr->name = 0;
	MIB2hdr->len = 0;
	
	msgdata.buf = storage;
	msgdata.len = sizeof (struct T_optmgmt_req) + sizeof (struct opthdr);

	flags = 0;

	if (putmsg (dev, &msgdata, NULL, flags) == -1) {
		zlog_warn ("putmsg failed: %s", strerror (errno));
		goto exit;
	}

	MIB2hdr = (struct opthdr *) &TLIack[1];
	msgdata.maxlen = sizeof (storage);

	while (1) {
		flags = 0;
		retval = getmsg (dev, &msgdata, NULL, &flags);

		if (retval == -1) {
			zlog_warn ("getmsg(ctl) failed: %s", strerror (errno));
			goto exit;
		}

		/* This is normal loop termination */
		if (retval == 0 &&
			msgdata.len >= sizeof (struct T_optmgmt_ack) &&
			TLIack->PRIM_type == T_OPTMGMT_ACK &&
			TLIack->MGMT_flags == T_SUCCESS &&
			MIB2hdr->len == 0)
			break;

		if (msgdata.len >= sizeof (struct T_error_ack) &&
			TLIerr->PRIM_type == T_ERROR_ACK) {
			zlog_warn ("getmsg(ctl) returned T_ERROR_ACK: %s",
				strerror ((TLIerr->TLI_error == TSYSERR)
				? TLIerr->UNIX_error : EPROTO));
			break;
		}

		/* should dump more debugging info to the log statement,
		   like what GateD does in this instance, but not
		   critical yet. */
		if (retval != MOREDATA ||
			msgdata.len < sizeof (struct T_optmgmt_ack) ||
			TLIack->PRIM_type != T_OPTMGMT_ACK ||
			TLIack->MGMT_flags != T_SUCCESS) {
			errno = ENOMSG;
			zlog_warn ("getmsg(ctl) returned bizarreness");
			break;
		}

		/* MIB2_IP_21 is the the pseudo-MIB2 ipRouteTable
		   entry, see <inet/mib2.h>. "This isn't the MIB data
		   you're looking for." */
		process = (MIB2hdr->level == MIB2_IP &&
			MIB2hdr->name == MIB2_IP_21) ? 1 : 0;

		/* getmsg writes the data buffer out completely, not
		   to the closest smaller multiple. Unless reassembling
		   data structures across buffer boundaries is your idea
		   of a good time, set maxlen to the closest smaller
		   multiple of the size of the datastructure you're
		   retrieving. */
		msgdata.maxlen = sizeof (storage) - (sizeof (storage)
			% sizeof (mib2_ipRouteEntry_t));

		msgdata.len = 0;
		flags = 0;

		do {
			retval = getmsg (dev, NULL, &msgdata, &flags);

			if (retval == -1) {
				zlog_warn ("getmsg(data) failed: %s",
					strerror (errno));
				goto exit;
			}

			if (!(retval == 0 || retval == MOREDATA)) {
				zlog_warn ("getmsg(data) returned %d", retval);
				goto exit;
			}

			if (process) {
				if (msgdata.len %
					sizeof (mib2_ipRouteEntry_t) != 0) {
					zlog_warn ("getmsg(data) returned "
"msgdata.len = %d (%% sizeof (mib2_ipRouteEntry_t) != 0)", msgdata.len);
					goto exit;
				}

				routeEntry = (mib2_ipRouteEntry_t *)
					msgdata.buf;
				lastRouteEntry = (mib2_ipRouteEntry_t *)
					(msgdata.buf + msgdata.len);
				do {
					handle_route_entry (routeEntry);
				} while (++routeEntry < lastRouteEntry);
			}
		} while (retval == MOREDATA);
	}

exit:
	close (dev);
}
