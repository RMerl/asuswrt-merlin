/* 
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1997,
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1997,
 *  Copyright (C) Paul Ashton                       1997.
 *  Copyright (C) Gerald (Jerry) Carter             2005
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

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_PARSE

/*******************************************************************
 Reads or writes an NTTIME structure.
********************************************************************/

bool smb_io_time(const char *desc, NTTIME *nttime, prs_struct *ps, int depth)
{
	uint32 low, high;
	if (nttime == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_time");
	depth++;

	if(!prs_align(ps))
		return False;
	
	if (MARSHALLING(ps)) {
		low = *nttime & 0xFFFFFFFF;
		high = *nttime >> 32;
	}
	
	if(!prs_uint32("low ", ps, depth, &low)) /* low part */
		return False;
	if(!prs_uint32("high", ps, depth, &high)) /* high part */
		return False;

	if (UNMARSHALLING(ps)) {
		*nttime = (((uint64_t)high << 32) + low);
	}

	return True;
}

/*******************************************************************
 Reads or writes a struct GUID
********************************************************************/

bool smb_io_uuid(const char *desc, struct GUID *uuid, 
		 prs_struct *ps, int depth)
{
	if (uuid == NULL)
		return False;

	prs_debug(ps, depth, desc, "smb_io_uuid");
	depth++;

	if(!prs_uint32 ("data   ", ps, depth, &uuid->time_low))
		return False;
	if(!prs_uint16 ("data   ", ps, depth, &uuid->time_mid))
		return False;
	if(!prs_uint16 ("data   ", ps, depth, &uuid->time_hi_and_version))
		return False;

	if(!prs_uint8s (False, "data   ", ps, depth, uuid->clock_seq, sizeof(uuid->clock_seq)))
		return False;
	if(!prs_uint8s (False, "data   ", ps, depth, uuid->node, sizeof(uuid->node)))
		return False;

	return True;
}
