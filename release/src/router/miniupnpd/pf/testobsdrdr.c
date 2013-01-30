/* $Id: testobsdrdr.c,v 1.19 2010/03/07 09:25:20 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2010 Thomas Bernard 
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <syslog.h>

#include "obsdrdr.h"

/*int logpackets = 1;*/
int runtime_flags = 0;
const char * tag = 0;

void
list_rules(void);

void
test_index(void)
{
	char ifname[16/*IFNAMSIZ*/];
	char iaddr[32];
	char desc[64];
	unsigned short iport = 0;
	unsigned short eport = 0;
	int proto = 0;
	ifname[0] = '\0';
	iaddr[0] = '\0';
	if(get_redirect_rule_by_index(0, ifname, &eport, iaddr, sizeof(iaddr),
	                              &iport, &proto, desc, sizeof(desc),
                                  0, 0) < 0)
	{
		printf("get.._by_index : no rule\n");
	}
	else
	{
		printf("%s %u -> %s:%u proto %d\n", ifname, (unsigned int)eport,
		       iaddr, (unsigned int)iport, proto);
		printf("description: \"%s\"\n", desc);
	}
}

int
main(int arc, char * * argv)
{
	char buf[32];
	char desc[64];
	unsigned short iport;
	u_int64_t packets = 0;
	u_int64_t bytes = 0;

	openlog("testobsdrdr", LOG_PERROR, LOG_USER);
	if(init_redirect() < 0)
	{
		fprintf(stderr, "init_redirect() failed\n");
		return 1;
	}
	//add_redirect_rule("ep0", 12123, "192.168.1.23", 1234);
	//add_redirect_rule2("ep0", 12155, "192.168.1.155", 1255, IPPROTO_TCP);
	//add_redirect_rule2("ep0", 12123, "192.168.1.125", 1234,
	//                   IPPROTO_UDP, "test description");
	//add_redirect_rule2("em0", 12123, "127.1.2.3", 1234,
	//                   IPPROTO_TCP, "test description tcp");

	list_rules();

	if(get_redirect_rule("xl1", 4662, IPPROTO_TCP,
	                     buf, 32, &iport, desc, sizeof(desc),
	                     &packets, &bytes) < 0)
		printf("get_redirect_rule() failed\n");
	else
	{
		printf("\n%s:%d '%s' packets=%llu bytes=%llu\n", buf, (int)iport, desc,
		       packets, bytes);
	}
#if 0
	if(delete_redirect_rule("ep0", 12123, IPPROTO_UDP) < 0)
		printf("delete_redirect_rule() failed\n");
	else
		printf("delete_redirect_rule() succeded\n");

	if(delete_redirect_rule("ep0", 12123, IPPROTO_UDP) < 0)
		printf("delete_redirect_rule() failed\n");
	else
		printf("delete_redirect_rule() succeded\n");
#endif
	//test_index();

	//clear_redirect_rules();
	//list_rules();

	return 0;
}


