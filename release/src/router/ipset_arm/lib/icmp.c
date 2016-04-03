/* Copyright 2007-2010 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <libipset/utils.h>			/* STRNEQ */
#include <libipset/icmp.h>			/* prototypes */

struct icmp_names {
	const char *name;
	uint8_t type, code;
};

static const struct icmp_names icmp_typecodes[] = {
	{ "echo-reply", 0, 0 },
	{ "pong", 0, 0 },
	{ "network-unreachable", 3, 0 },
	{ "host-unreachable", 3, 1 },
	{ "protocol-unreachable", 3, 2 },
	{ "port-unreachable", 3, 3 },
	{ "fragmentation-needed", 3, 4 },
	{ "source-route-failed", 3, 5 },
	{ "network-unknown", 3, 6 },
	{ "host-unknown", 3, 7 },
	{ "network-prohibited", 3, 9 },
	{ "host-prohibited", 3, 10 },
	{ "TOS-network-unreachable", 3, 11 },
	{ "TOS-host-unreachable", 3, 12 },
	{ "communication-prohibited", 3, 13 },
	{ "host-precedence-violation", 3, 14 },
	{ "precedence-cutoff", 3, 15 },
	{ "source-quench", 4, 0 },
	{ "network-redirect", 5, 0 },
	{ "host-redirect", 5, 1 },
	{ "TOS-network-redirect", 5, 2 },
	{ "TOS-host-redirect", 5, 3 },
	{ "echo-request", 8, 0 },
	{ "ping", 8, 0 },
	{ "router-advertisement", 9, 0 },
	{ "router-solicitation", 10, 0 },
	{ "ttl-zero-during-transit", 11, 0 },
	{ "ttl-zero-during-reassembly", 11, 1 },
	{ "ip-header-bad", 12, 0 },
	{ "required-option-missing", 12, 1 },
	{ "timestamp-request", 13, 0 },
	{ "timestamp-reply", 14, 0 },
	{ "address-mask-request", 17, 0 },
	{ "address-mask-reply", 18, 0 },
};

const char *id_to_icmp(uint8_t id)
{
	return id < ARRAY_SIZE(icmp_typecodes) ? icmp_typecodes[id].name : NULL;
}

const char *icmp_to_name(uint8_t type, uint8_t code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(icmp_typecodes); i++)
		if (icmp_typecodes[i].type == type &&
		    icmp_typecodes[i].code == code)
			return icmp_typecodes[i].name;

	return NULL;
}

int name_to_icmp(const char *str, uint16_t *typecode)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(icmp_typecodes); i++)
		if (STRNCASEQ(icmp_typecodes[i].name, str, strlen(str))) {
			*typecode = (icmp_typecodes[i].type << 8) |
				    icmp_typecodes[i].code;
			return 0;
		}

	return -1;
}
