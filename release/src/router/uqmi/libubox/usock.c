/*
 * usock - socket helper functions
 *
 * Copyright (C) 2010 Steven Barth <steven@midlink.org>
 * Copyright (C) 2011-2012 Felix Fietkau <nbd@openwrt.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include "usock.h"

static void usock_set_flags(int sock, unsigned int type)
{
	if (!(type & USOCK_NOCLOEXEC))
		fcntl(sock, F_SETFD, fcntl(sock, F_GETFD) | FD_CLOEXEC);

	if (type & USOCK_NONBLOCK)
		fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);
}

static int usock_connect(struct sockaddr *sa, int sa_len, int family, int socktype, bool server)
{
	int sock;

	sock = socket(family, socktype, 0);
	if (sock < 0)
		return -1;

	if (server) {
		const int one = 1;
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

		if (!bind(sock, sa, sa_len) &&
		    (socktype != SOCK_STREAM || !listen(sock, SOMAXCONN)))
			return sock;
	} else {
		if (!connect(sock, sa, sa_len) || errno == EINPROGRESS)
			return sock;
	}

	close(sock);
	return -1;
}

static int usock_unix(const char *host, int socktype, bool server)
{
	struct sockaddr_un sun = {.sun_family = AF_UNIX};

	if (strlen(host) >= sizeof(sun.sun_path)) {
		errno = EINVAL;
		return -1;
	}
	strcpy(sun.sun_path, host);

	return usock_connect((struct sockaddr*)&sun, sizeof(sun), AF_UNIX, socktype, server);
}

static int usock_inet(int type, const char *host, const char *service, int socktype, bool server)
{
	struct addrinfo *result, *rp;
	struct addrinfo hints = {
		.ai_family = (type & USOCK_IPV6ONLY) ? AF_INET6 :
			(type & USOCK_IPV4ONLY) ? AF_INET : AF_UNSPEC,
		.ai_socktype = socktype,
		.ai_flags = AI_ADDRCONFIG
			| ((type & USOCK_SERVER) ? AI_PASSIVE : 0)
			| ((type & USOCK_NUMERIC) ? AI_NUMERICHOST : 0),
	};
	int sock = -1;

	if (getaddrinfo(host, service, &hints, &result))
		return -1;

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sock = usock_connect(rp->ai_addr, rp->ai_addrlen, rp->ai_family, socktype, server);
		if (sock >= 0)
			break;
	}

	freeaddrinfo(result);
	return sock;
}

int usock(int type, const char *host, const char *service) {
	int socktype = ((type & 0xff) == USOCK_TCP) ? SOCK_STREAM : SOCK_DGRAM;
	bool server = !!(type & USOCK_SERVER);
	int sock;

	if (type & USOCK_UNIX)
		sock = usock_unix(host, socktype, server);
	else
		sock = usock_inet(type, host, service, socktype, server);

	if (sock < 0)
		return -1;

	usock_set_flags(sock, type);
	return sock;
}
