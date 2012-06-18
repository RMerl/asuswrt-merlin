/* 
   Unix SMB/CIFS implementation.

   routines for marshalling/unmarshalling DCOM string arrays

   Copyright (C) Jelmer Vernooij 2004
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
//#define NDR_CHECK_DEBUG
#include "includes.h"
#include "librpc/gen_ndr/ndr_dcom.h"
#include "librpc/gen_ndr/ndr_wmi.h"
#include "librpc/ndr/ndr_wmi.h"

// Just for debugging
int NDR_CHECK_depth = 0;
int NDR_CHECK_shift = 0x18;

enum ndr_err_code ndr_push_BSTR(struct ndr_push *ndr, int ndr_flags, const struct BSTR *r)
{
	uint32_t len;
	uint32_t flags;
	enum ndr_err_code status;
	len = strlen(r->data);
        if (ndr_flags & NDR_SCALARS) {
                NDR_CHECK(ndr_push_align(ndr, 4));
                NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, 0x72657355));
                NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, len));
                NDR_CHECK(ndr_push_uint32(ndr, NDR_SCALARS, 2*len));
		flags = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_STR_NOTERM | LIBNDR_FLAG_STR_SIZE4);
		status = ndr_push_string(ndr, NDR_SCALARS, r->data);
		ndr->flags = flags;
		return status;
        }
        return NDR_ERR_SUCCESS;
}

enum ndr_err_code ndr_pull_BSTR(struct ndr_pull *ndr, int ndr_flags, struct BSTR *r)
{
        return NDR_ERR_BAD_SWITCH;
}

void ndr_print_BSTR(struct ndr_print *ndr, const char *name, const struct BSTR *r)
{
	ndr->print(ndr, "%-25s: BSTR(\"%s\")", name, r->data);
}
