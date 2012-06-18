/*
 * MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2009 Jardel Weyrich
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution
 */

#include <stdio.h>
#include <syslog.h>
#include <netinet/in.h>
#include "ipfwrdr.h"

// test program for ipfwrdr.c

int main(int argc, char * * argv) {
	openlog("testipfwrdrd", LOG_CONS | LOG_PERROR, LOG_USER);
	init_redirect();
	delete_redirect_rule("lo", 2222, IPPROTO_TCP);
	add_redirect_rule2("lo", 2222, "10.1.1.16", 4444, IPPROTO_TCP, "miniupnpd");
	get_redirect_rule("lo", 2222, IPPROTO_TCP, NULL, 0, NULL, NULL, 0, NULL, NULL);
	shutdown_redirect();
	return 0;
}

