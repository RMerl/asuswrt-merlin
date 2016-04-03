/* Copyright 2007-2010 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <libipset/utils.h>			/* STRNEQ */
#include <libipset/icmpv6.h>			/* prototypes */

struct icmpv6_names {
	const char *name;
	uint8_t type, code;
};

static const struct icmpv6_names icmpv6_typecodes[] = {
	{ "no-route", 1, 0 },
	{ "communication-prohibited", 1, 1 },
	{ "address-unreachable", 1, 3 },
	{ "port-unreachable", 1, 4 },
	{ "packet-too-big", 2, 0 },
	{ "ttl-zero-during-transit", 3, 0 },
	{ "ttl-zero-during-reassembly", 3, 1 },
	{ "bad-header", 4, 0 },
	{ "unknown-header-type", 4, 1 },
	{ "unknown-option", 4, 2 },
	{ "echo-request", 128, 0 },
	{ "ping", 128, 0 },
	{ "echo-reply", 129, 0 },
	{ "pong", 129, 0 },
	{ "router-solicitation", 133, 0 },
	{ "router-advertisement", 134, 0 },
	{ "neighbour-solicitation", 135, 0 },
	{ "neigbour-solicitation", 135, 0 },
	{ "neighbour-advertisement", 136, 0 },
	{ "neigbour-advertisement", 136, 0 },
	{ "redirect", 137, 0 },
};

const char *id_to_icmpv6(uint8_t id)
{
	return id < ARRAY_SIZE(icmpv6_typecodes) ?
	       icmpv6_typecodes[id].name : NULL;
}

const char *icmpv6_to_name(uint8_t type, uint8_t code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(icmpv6_typecodes); i++)
		if (icmpv6_typecodes[i].type == type &&
		    icmpv6_typecodes[i].code == code)
			return icmpv6_typecodes[i].name;

	return NULL;
}

int name_to_icmpv6(const char *str, uint16_t *typecode)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(icmpv6_typecodes); i++)
		if (STRNCASEQ(icmpv6_typecodes[i].name, str, strlen(str))) {
			*typecode = (icmpv6_typecodes[i].type << 8) |
				    icmpv6_typecodes[i].code;
			return 0;
		}

	return -1;
}
