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
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ether.h>
#include <arpa/inet.h>
#include <syslog.h>

#include "snooper.h"

int open_socket(int domain, int type, int protocol)
{
	int fd, value;

#if defined(SOCK_CLOEXEC) && defined(SOCK_NONBLOCK)
	fd = socket(domain, type | SOCK_CLOEXEC | SOCK_NONBLOCK, protocol);
	if (fd >= 0)
		return fd;
	else if (fd < 0 && errno != EINVAL)
		goto error;
#endif
	fd = socket(domain, type, protocol);
	if (fd < 0)
		goto error;

#ifdef FD_CLOEXEC
	value = fcntl(fd, F_GETFD, 0);
	if (value < 0 || fcntl(fd, F_SETFD, value | FD_CLOEXEC) < 0) {
		log_error("fcntl::FD_CLOEXEC: %s", strerror(errno));
		close(fd);
		return -1;
	}
#endif

	value = fcntl(fd, F_GETFL, 0);
	if (value < 0 || fcntl(fd, F_SETFL, value | O_NONBLOCK) < 0) {
		log_error("fcntl::O_NONBLOCK: %s", strerror(errno));
		close(fd);
		return -1;
	}

	return fd;

error:
	log_error("socket: %s", strerror(errno));
	return -1;
}

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
