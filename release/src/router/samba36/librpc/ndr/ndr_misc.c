/* 
   Unix SMB/CIFS implementation.

   UUID/GUID/policy_handle functions

   Copyright (C) Andrew Tridgell                   2003.
   Copyright (C) Stefan (metze) Metzmacher         2004.
   
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

#include "includes.h"
#include "system/network.h"
#include "librpc/ndr/libndr.h"

_PUBLIC_ void ndr_print_GUID(struct ndr_print *ndr, const char *name, const struct GUID *guid)
{
	ndr->print(ndr, "%-25s: %s", name, GUID_string(ndr, guid));
}

bool ndr_syntax_id_equal(const struct ndr_syntax_id *i1,
			 const struct ndr_syntax_id *i2)
{
	return GUID_equal(&i1->uuid, &i2->uuid)
		&& (i1->if_version == i2->if_version);
}

_PUBLIC_ char *ndr_syntax_id_to_string(TALLOC_CTX *mem_ctx, const struct ndr_syntax_id *id)
{
	return talloc_asprintf(mem_ctx,
			       "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x/0x%08x",
			       id->uuid.time_low, id->uuid.time_mid,
			       id->uuid.time_hi_and_version,
			       id->uuid.clock_seq[0],
			       id->uuid.clock_seq[1],
			       id->uuid.node[0], id->uuid.node[1],
			       id->uuid.node[2], id->uuid.node[3],
			       id->uuid.node[4], id->uuid.node[5],
			       (unsigned)id->if_version);
}

_PUBLIC_ bool ndr_syntax_id_from_string(const char *s, struct ndr_syntax_id *id)
{
	int ret;
	size_t i;
	uint32_t time_low;
	uint32_t time_mid, time_hi_and_version;
	uint32_t clock_seq[2];
	uint32_t node[6];
	uint32_t if_version;

	ret = sscanf(s,
		     "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x/0x%08x",
		     &time_low, &time_mid, &time_hi_and_version,
		     &clock_seq[0], &clock_seq[1],
		     &node[0], &node[1], &node[2], &node[3], &node[4], &node[5],
		     &if_version);
	if (ret != 12) {
		return false;
	}

	id->uuid.time_low = time_low;
	id->uuid.time_mid = time_mid;
	id->uuid.time_hi_and_version = time_hi_and_version;
	id->uuid.clock_seq[0] = clock_seq[0];
	id->uuid.clock_seq[1] = clock_seq[1];
	for (i=0; i<6; i++) {
		id->uuid.node[i] = node[i];
	}
	id->if_version = if_version;

	return true;
}
