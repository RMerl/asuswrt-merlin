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
