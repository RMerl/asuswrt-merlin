/* 
 *  Unix SMB/CIFS implementation.
 *  UUID server routines
 *  Copyright (C) Theodore Ts'o               1996, 1997,
 *  Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2002, 2003
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"

void smb_uuid_pack(const struct GUID uu, UUID_FLAT *ptr)
{
	SIVAL(ptr->info, 0, uu.time_low);
	SSVAL(ptr->info, 4, uu.time_mid);
	SSVAL(ptr->info, 6, uu.time_hi_and_version);
	memcpy(ptr->info+8, uu.clock_seq, 2);
	memcpy(ptr->info+10, uu.node, 6);
}

void smb_uuid_unpack(const UUID_FLAT in, struct GUID *uu)
{
	uu->time_low = IVAL(in.info, 0);
	uu->time_mid = SVAL(in.info, 4);
	uu->time_hi_and_version = SVAL(in.info, 6);
	memcpy(uu->clock_seq, in.info+8, 2);
	memcpy(uu->node, in.info+10, 6);
}

/*****************************************************************
 Return the binary string representation of a GUID.
 Caller must free.
*****************************************************************/

char *guid_binstring(TALLOC_CTX *mem_ctx, const struct GUID *guid)
{
	UUID_FLAT guid_flat;

	smb_uuid_pack(*guid, &guid_flat);

	return binary_string_rfc2254(mem_ctx, guid_flat.info, UUID_FLAT_SIZE);
}
