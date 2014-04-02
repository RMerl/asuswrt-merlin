/* $Id: testiptcrdr_peer.c,v 1.1 2013/12/13 13:10:48 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2012 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <syslog.h>

#include "iptcrdr.h"
#include "../commonrdr.h"
#include "iptcrdr.c"

#ifndef PRIu64
#define PRIu64 "llu"
#endif

int
main(int argc, char ** argv)
{
	unsigned short eport, iport, rport;
	const char * eaddr, *iaddr, *rhost;
	printf("Usage %s <ext_ip> <ext_port> <internal_ip> <internal_port> <peer_ip> <peer_port>\n", argv[0]);

	if(argc<6)
		return -1;
	openlog("testiptcrdr_peer", LOG_PERROR|LOG_CONS, LOG_LOCAL0);
	eaddr = argv[1];
	eport = (unsigned short)atoi(argv[2]);
	iaddr = argv[3];
	iport = (unsigned short)atoi(argv[4]);
	rhost = argv[5];
	rport = (unsigned short)atoi(argv[6]);
#if 1
	printf("trying to redirect port %hu to %s:%hu\n", eport, iaddr, iport);
	if(addpeernatrule(IPPROTO_TCP, eaddr, eport, iaddr, iport, rhost, rport) < 0)
		return -1;
#endif
	/* test */
	{
		unsigned short p1, p2;
		char addr[16];
		int proto2;
		char desc[256];
		char rhost[256];
		unsigned int timestamp;
		u_int64_t packets, bytes;

		desc[0] = '\0';
		if(get_redirect_rule_by_index(0, "", &p1,
									  addr, sizeof(addr), &p2,
									  &proto2, desc, sizeof(desc),
									  rhost, sizeof(rhost),
									  &timestamp,
									  &packets, &bytes) < 0)
		{
			printf("rule not found\n");
		}
		else
		{
			printf("redirected port %hu to %s:%hu proto %d   packets=%" PRIu64 " bytes=%" PRIu64 "\n",
			       p1, addr, p2, proto2, packets, bytes);
		}
	}
	printf("trying to list nat rules :\n");
	list_redirect_rule(argv[1]);
	printf("deleting\n");
	delete_redirect_and_filter_rules(eport, IPPROTO_TCP);
	return 0;
}

