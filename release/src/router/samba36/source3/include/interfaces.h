/*
   Unix SMB/CIFS implementation.
   Machine customisation and include handling
   Copyright (C) Jeremy Allison <jra@samba.org> 2007

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
/*
   This structure is used by lib/interfaces.c to return the list of network
   interfaces on the machine
*/

#ifndef _INTERFACES_H
#define _INTERFACES_H

#include "../replace/replace.h"
#include "../replace/system/network.h"

struct iface_struct {
	char name[16];
	int flags;
	struct sockaddr_storage ip;
	struct sockaddr_storage netmask;
	struct sockaddr_storage bcast;
};

bool make_netmask(struct sockaddr_storage *pss_out,
		  const struct sockaddr_storage *pss_in,
		  unsigned long masklen);
void make_bcast(struct sockaddr_storage *pss_out,
		const struct sockaddr_storage *pss_in,
		const struct sockaddr_storage *nmask);
void make_net(struct sockaddr_storage *pss_out,
	      const struct sockaddr_storage *pss_in,
	      const struct sockaddr_storage *nmask);
int get_interfaces(TALLOC_CTX *mem_ctx, struct iface_struct **pifaces);

#endif
