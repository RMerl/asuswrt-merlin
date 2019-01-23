/*
 * Unix SMB/CIFS implementation.
 * replacement routines for broken systems
 * Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2006
 * Copyright (C) Michael Adam <obnox@samba.org> 2008
 *
 *  ** NOTE! The following LGPL license applies to the replace
 *  ** library. This does NOT imply that all of Samba is released
 *  ** under the LGPL
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "replace.h"
#include "system/network.h"

int rep_socketpair(int d, int type, int protocol, int sv[2])
{
	if (d != AF_UNIX) {
		errno = EAFNOSUPPORT;
		return -1;
	}

	if (protocol != 0) {
		errno = EPROTONOSUPPORT;
		return -1;
	}

	if (type != SOCK_STREAM) {
		errno = EOPNOTSUPP;
		return -1;
	}

	return pipe(sv);
}
