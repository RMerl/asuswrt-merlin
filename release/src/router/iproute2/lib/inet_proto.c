/*
 * inet_proto.c
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#include "utils.h"

char *inet_proto_n2a(int proto, char *buf, int len)
{
	static char ncache[16];
	static int icache = -1;
	struct protoent *pe;

	if (proto == icache)
		return ncache;

	pe = getprotobynumber(proto);
	if (pe) {
		icache = proto;
		strncpy(ncache, pe->p_name, 16);
		strncpy(buf, pe->p_name, len);
		return buf;
	}
	snprintf(buf, len, "ipproto-%d", proto);
	return buf;
}

int inet_proto_a2n(char *buf)
{
	static char ncache[16];
	static int icache = -1;
	struct protoent *pe;

	if (icache>=0 && strcmp(ncache, buf) == 0)
		return icache;

	if (buf[0] >= '0' && buf[0] <= '9') {
		__u8 ret;
		if (get_u8(&ret, buf, 10))
			return -1;
		return ret;
	}

	pe = getprotobyname(buf);
	if (pe) {
		icache = pe->p_proto;
		strncpy(ncache, pe->p_name, 16);
		return pe->p_proto;
	}
	return -1;
}


