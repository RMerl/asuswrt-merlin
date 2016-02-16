/* $Id: testportinuse.c,v 1.3 2014/03/28 12:13:17 nanard Exp $ */
/* MiniUPnP project
 * (c) 2014 Thomas Bernard
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <syslog.h>

#include "macros.h"
#include "config.h"
#include "portinuse.h"

#ifdef USE_NETFILTER
const char * miniupnpd_nat_chain = "MINIUPNPD";
const char * miniupnpd_nat_postrouting_chain = "MINIUPNPD-POSTROUTING";
const char * miniupnpd_forward_chain = "MINIUPNPD";
#endif /* USE_NETFILTER */

int main(int argc, char * * argv)
{
#ifndef CHECK_PORTINUSE
	UNUSED(argc); UNUSED(argv);
	printf("CHECK_PORTINUSE is not defined.\n");
#else /* CHECK_PORTINUSE */
	int r;
	const char * if_name;
	unsigned eport;
	int proto;
	const char * iaddr;
	unsigned iport;

	if(argc <= 5) {
		fprintf(stderr, "usage:   %s if_name eport (tcp|udp) iaddr iport\n",
		        argv[0]);
		return 1;
	}
	openlog("testportinuse",  LOG_CONS|LOG_PERROR, LOG_USER);
	if_name = argv[1];
	eport = (unsigned)atoi(argv[2]);
	proto = (0==strcmp(argv[3], "tcp"))?IPPROTO_TCP:IPPROTO_UDP;
	iaddr = argv[4];
	iport = (unsigned)atoi(argv[5]);
	
	r = port_in_use(if_name, eport, proto, iaddr, iport);
	printf("port_in_use(%s, %u, %d, %s, %u) returned %d\n",
	       if_name, eport, proto, iaddr, iport, r);
	closelog();
#endif /* CHECK_PORTINUSE */
	return 0;
}
