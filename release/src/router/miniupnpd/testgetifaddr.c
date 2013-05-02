/* $Id: testgetifaddr.c,v 1.6 2013/03/23 10:46:55 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2013 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */
#include <stdio.h>
#include <syslog.h>
#include <netinet/in.h>
#include "getifaddr.h"

#if defined(__sun)
/* solaris 10 does not define LOG_PERROR */
#define LOG_PERROR 0
#endif

int main(int argc, char * * argv) {
	char str_addr[64];
	struct in_addr addr;
	struct in_addr mask;
	if(argc < 2) {
		fprintf(stderr, "Usage:\t%s interface_name\n", argv[0]);
		return 1;
	}

	openlog("testgetifaddr", LOG_CONS|LOG_PERROR, LOG_USER);
	if(getifaddr(argv[1], str_addr, sizeof(str_addr), &addr, &mask) < 0) {
		fprintf(stderr, "Cannot get address for interface %s.\n", argv[1]);
		return 1;
	}
	printf("Interface %s has IP address %s.\n", argv[1], str_addr);
	return 0;
}
