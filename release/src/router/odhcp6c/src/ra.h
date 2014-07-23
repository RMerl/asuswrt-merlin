/**
 * Copyright (C) 2012-2014 Steven Barth <steven@midlink.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#pragma once

#define ALL_IPV6_NODES {{{0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}}}

#define ALL_IPV6_ROUTERS {{{0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02}}}

struct icmpv6_opt {
	uint8_t type;
	uint8_t len;
	uint8_t data[6];
};

#define ND_OPT_ROUTE_INFORMATION 24


#define icmpv6_for_each_option(opt, start, end)\
	for (opt = (struct icmpv6_opt*)(start);\
	(void*)(opt + 1) <= (void*)(end) && opt->len > 0 &&\
	(void*)(opt + opt->len) <= (void*)(end); opt += opt->len)


int ra_init(const char *ifname, const struct in6_addr *ifid);
bool ra_link_up(void);
bool ra_process(void);
