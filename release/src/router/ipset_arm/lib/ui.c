/* Copyright 2007-2010 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdio.h>		/* printf */
#include <libipset/icmp.h>	/* id_to_icmp */
#include <libipset/icmpv6.h>	/* id_to_icmpv6 */
#include <libipset/ui.h>	/* prototypes */

/**
 * ipset_port_usage - prints the usage for the port parameter
 *
 * Print the usage for the port parameter to stdout.
 */
void
ipset_port_usage(void)
{
	int i;
	const char *name;

	printf("      [PROTO:]PORT is a valid pattern of the following:\n"
	       "           PORTNAME         TCP port name from /etc/services\n"
	       "           PORTNUMBER       TCP port number identifier\n"
	       "           tcp|sctp|udp|udplite:PORTNAME|PORTNUMBER\n"
	       "           icmp:CODENAME    supported ICMP codename\n"
	       "           icmp:TYPE/CODE   ICMP type/code value\n"
	       "           icmpv6:CODENAME  supported ICMPv6 codename\n"
	       "           icmpv6:TYPE/CODE ICMPv6 type/code value\n"
	       "           PROTO:0          all other protocols\n\n");

	printf("           Supported ICMP codenames:\n");
	i = 0;
	while ((name = id_to_icmp(i++)) != NULL)
		printf("               %s\n", name);
	printf("           Supported ICMPv6 codenames:\n");
	i = 0;
	while ((name = id_to_icmpv6(i++)) != NULL)
		printf("               %s\n", name);
}
