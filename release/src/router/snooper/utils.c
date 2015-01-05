/*
 * Ethernet Switch IGMP Snooper
 * Copyright (C) 2014 ASUSTeK Inc.
 * All Rights Reserved.

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ether.h>
#include <arpa/inet.h>
#include <syslog.h>

#include "snooper.h"

void ether_mtoe(in_addr_t addr, unsigned char *ea)
{
	struct in_addr in = { .s_addr = addr };

	ETHER_MAP_IP_MULTICAST(&in, ea);
}

unsigned int ether_hash(unsigned char *ea)
{
	register unsigned int hash = 0;
	int i;

	for (i = 0; i < ETHER_ADDR_LEN; i++)
		hash = ea[i] + (hash << 6) + (hash << 16) - hash;

	return hash;
}

int logger(int level, char *fmt, ...)
{
	va_list ap;
#ifdef DEBUG
	struct timeval tv;
#endif

	va_start(ap, fmt);
#ifdef DEBUG
	time_to_timeval(now(), &tv);
	printf(FMT_TIME " ", ARG_TIMEVAL(&tv));
	vprintf(fmt, ap);
	printf("\n");
#endif
	vsyslog(level, fmt, ap);
	va_end(ap);

	return 0;
}
