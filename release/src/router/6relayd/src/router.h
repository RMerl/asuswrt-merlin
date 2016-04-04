/**
 * Copyright (C) 2012-2013 Steven Barth <steven@midlink.org>
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
#include <stdint.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>

struct icmpv6_opt {
	uint8_t type;
	uint8_t len;
	uint8_t data[6];
};


#define icmpv6_for_each_option(opt, start, end)\
	for (opt = (struct icmpv6_opt*)(start);\
	(void*)(opt + 1) <= (void*)(end) && opt->len > 0 &&\
	(void*)(opt + opt->len) <= (void*)(end); opt += opt->len)


#define MaxRtrAdvInterval 600
#define MinRtrAdvInterval (MaxRtrAdvInterval / 3)
#define MaxValidTime 7200
#define MaxPreferredTime  (3 * MaxRtrAdvInterval)

#define ND_RA_FLAG_PROXY	0x4
#define ND_RA_PREF_HIGH	(1 << 3)
#define ND_RA_PREF_LOW		(3 << 3)
