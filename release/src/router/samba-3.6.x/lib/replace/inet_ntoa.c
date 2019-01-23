/*
 * Unix SMB/CIFS implementation.
 * replacement routines for broken systems
 * Copyright (C) Andrew Tridgell 2003
 * Copyright (C) Michael Adam 2008
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

/**
 * NOTE: this is not thread safe, but it can't be, either
 * since it returns a pointer to static memory.
 */
char *rep_inet_ntoa(struct in_addr ip)
{
	uint8_t *p = (uint8_t *)&ip.s_addr;
	static char buf[18];
	slprintf(buf, 17, "%d.%d.%d.%d",
		 (int)p[0], (int)p[1], (int)p[2], (int)p[3]);
	return buf;
}
