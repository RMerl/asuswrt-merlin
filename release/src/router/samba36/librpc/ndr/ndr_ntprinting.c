/*
   Unix SMB/CIFS implementation.

   routines for marshalling/unmarshalling special ntprinting structures

   Copyright (C) Guenther Deschner 2010

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
#include "../librpc/gen_ndr/ndr_ntprinting.h"

_PUBLIC_ uint32_t ndr_ntprinting_string_flags(uint32_t string_flags)
{
	uint32_t flags = LIBNDR_FLAG_STR_NULLTERM;

	if (string_flags & LIBNDR_FLAG_STR_ASCII) {
		flags |= LIBNDR_FLAG_STR_ASCII;
	} else {
		flags |= LIBNDR_FLAG_STR_UTF8;
	}

	return flags;
}

_PUBLIC_ enum ndr_err_code ndr_pull_ntprinting_printer(struct ndr_pull *ndr, int ndr_flags, struct ntprinting_printer *r)
{
	uint32_t _ptr_devmode;
	TALLOC_CTX *_mem_save_devmode_0;
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_pull_align(ndr, 5));
			NDR_CHECK(ndr_pull_ntprinting_printer_info(ndr, NDR_SCALARS, &r->info));
			NDR_CHECK(ndr_pull_generic_ptr(ndr, &_ptr_devmode));
			if (_ptr_devmode) {
				NDR_PULL_ALLOC(ndr, r->devmode);
			} else {
				r->devmode = NULL;
			}
		}
		if (ndr_flags & NDR_BUFFERS) {
			if (r->devmode) {
				_mem_save_devmode_0 = NDR_PULL_GET_MEM_CTX(ndr);
				NDR_PULL_SET_MEM_CTX(ndr, r->devmode, 0);
				r->devmode->string_flags = r->info.string_flags;
				NDR_CHECK(ndr_pull_ntprinting_devicemode(ndr, NDR_SCALARS|NDR_BUFFERS, r->devmode));
				NDR_PULL_SET_MEM_CTX(ndr, _mem_save_devmode_0, 0);
			}
		}
		if (ndr_flags & NDR_SCALARS) {
			r->count = 0;
			NDR_PULL_ALLOC_N(ndr, r->printer_data, r->count);
			while (ndr->offset + 4 <= ndr->data_size) {
				uint32_t ptr = 0;
				ptr = IVAL(ndr->data, ndr->offset);
				if (ptr == 0) {
					ndr->offset = ndr->offset + 4;
					break;
				}
				r->printer_data = talloc_realloc(ndr, r->printer_data, struct ntprinting_printer_data, r->count + 1);
				NDR_ERR_HAVE_NO_MEMORY(r->printer_data);
				r->printer_data[r->count].string_flags = r->info.string_flags;
				NDR_CHECK(ndr_pull_ntprinting_printer_data(ndr, NDR_SCALARS, &r->printer_data[r->count]));
				r->count++;
			}
			NDR_CHECK(ndr_pull_trailer_align(ndr, 5));
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}
