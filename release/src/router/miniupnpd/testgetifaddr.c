/* $Id: testgetifaddr.c,v 1.5 2012/06/22 16:11:58 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2011 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */
#include <stdio.h>
#include <syslog.h>
#include "getifaddr.h"

#if defined(__sun)
/* solaris 10 does not define LOG_PERROR */
#define LOG_PERROR 0
#endif

int main(int argc, char * * argv) {
	char addr[64];
	if(argc < 2) {
		fprintf(stderr, "Usage:\t%s interface_name\n", argv[0]);
		return 1;
	}

	openlog("testgetifaddr", LOG_CONS|LOG_PERROR, LOG_USER);
	if(getifaddr(argv[1], addr, sizeof(addr)) < 0) {
		fprintf(stderr, "Cannot get address for interface %s.\n", argv[1]);
		return 1;
	}
	printf("Interface %s has IP address %s.\n", argv[1], addr);
	return 0;
}
