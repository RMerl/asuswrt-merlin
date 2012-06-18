/*
 * Unix SMB/CIFS implementation.
 *
 * Dummy replacements for socket functions.
 *
 * Copyright (C) Michael Adam <obnox@samba.org> 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "replace.h"
#include "system/network.h"

int rep_connect(int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen)
{
	errno = ENOSYS;
	return -1;
}

struct hostent *rep_gethostbyname(const char *name)
{
	errno = ENOSYS;
	return NULL;
}
