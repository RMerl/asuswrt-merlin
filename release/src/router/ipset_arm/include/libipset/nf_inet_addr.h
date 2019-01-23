/* Copyright 2007-2010 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef LIBIPSET_NF_INET_ADDR_H
#define LIBIPSET_NF_INET_ADDR_H

#include <stdint.h>				/* uint32_t */
#include <netinet/in.h>				/* struct in[6]_addr */

/* The structure to hold IP addresses, same as in linux/netfilter.h */
union nf_inet_addr {
	uint32_t	all[4];
	uint32_t	ip;
	uint32_t	ip6[4];
	struct in_addr	in;
	struct in6_addr in6;
};

#endif /* LIBIPSET_NF_INET_ADDR_H */
