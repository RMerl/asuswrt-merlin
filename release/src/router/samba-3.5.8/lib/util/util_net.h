/* 
   Unix SMB/CIFS implementation.
   Utility functions for Samba
   Copyright (C) Andrew Tridgell 1992-1999
   Copyright (C) Jelmer Vernooij 2005
    
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _SAMBA_UTIL_NET_H_
#define _SAMBA_UTIL_NET_H_

#include "system/network.h"

/* The following definitions come from lib/util/util_net.c  */

void zero_sockaddr(struct sockaddr_storage *pss);

bool interpret_string_addr_internal(struct addrinfo **ppres,
				    const char *str, int flags);

bool interpret_string_addr(struct sockaddr_storage *pss,
			   const char *str,
			   int flags);

/*******************************************************************
 Map a text hostname or IP address (IPv4 or IPv6) into a
 struct sockaddr_storage. Version that prefers IPv4.
******************************************************************/

bool interpret_string_addr_prefer_ipv4(struct sockaddr_storage *pss,
				       const char *str,
				       int flags);

#endif /* _SAMBA_UTIL_NET_H_ */
