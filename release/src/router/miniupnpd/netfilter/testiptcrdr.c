/* $Id: testiptcrdr.c,v 1.14 2007/06/11 13:25:25 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006 Thomas Bernard 
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <syslog.h>

#include "iptcrdr.h"

int
main(int argc, char ** argv)
{
	unsigned short eport, iport;
	const char * iaddr;
	printf("Usage %s <ext_port> <internal_ip> <internal_port>\n", argv[0]);
	
	if(argc<4)
		return -1;
	openlog("testuptcrdr", LOG_PERROR|LOG_CONS, LOG_LOCAL0);
	eport = (unsigned short)atoi(argv[1]);
	iaddr = argv[2];
	iport = (unsigned short)atoi(argv[3]);
	printf("trying to redirect port %hu to %s:%hu\n", eport, iaddr, iport);
	if(addnatrule(IPPROTO_TCP, eport, iaddr, iport) < 0)
		return -1;
	if(add_filter_rule(IPPROTO_TCP, iaddr, iport) < 0)
		return -1;
	/* test */
	{
		unsigned short p1, p2;
		char addr[16];
		int proto2;
		char desc[256];
		u_int64_t packets, bytes;
		desc[0] = '\0';
		if(get_redirect_rule_by_index(0, "", &p1, addr, sizeof(addr),
                                      &p2, &proto2, desc, sizeof(desc),
									  &packets, &bytes) < 0)
		{
			printf("redirected port %hu to %s:%hu proto %d   packets=%llu bytes=%llu\n",
			       p1, addr, p2, proto2, packets, bytes);
		}
		else
		{
			printf("rule not found\n");
		}
	}
	printf("trying to list nat rules :\n");
	list_redirect_rule(argv[1]);
	printf("deleting\n");
	delete_redirect_and_filter_rules(eport, IPPROTO_TCP);
	return 0;
}

