/*
   Unix SMB/CIFS implementation.

   routines for marshalling/unmarshalling preg structures

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
#include "librpc/gen_ndr/ndr_preg.h"

_PUBLIC_ enum ndr_err_code ndr_push_preg_file(struct ndr_push *ndr, int ndr_flags, const struct preg_file *r)
{
	uint32_t cntr_entries_0;
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_push_align(ndr, 4));
			NDR_CHECK(ndr_push_preg_header(ndr, NDR_SCALARS, &r->header));
			for (cntr_entries_0 = 0; cntr_entries_0 < r->num_entries; cntr_entries_0++) {
				NDR_CHECK(ndr_push_preg_entry(ndr, NDR_SCALARS, &r->entries[cntr_entries_0]));
			}
			NDR_CHECK(ndr_push_trailer_align(ndr, 4));
		}
		if (ndr_flags & NDR_BUFFERS) {
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}

_PUBLIC_ enum ndr_err_code ndr_pull_preg_file(struct ndr_pull *ndr, int ndr_flags, struct preg_file *r)
{
	{
		uint32_t _flags_save_STRUCT = ndr->flags;
		ndr_set_flags(&ndr->flags, LIBNDR_FLAG_NOALIGN);
		if (ndr_flags & NDR_SCALARS) {
			NDR_CHECK(ndr_pull_align(ndr, 4));
			NDR_CHECK(ndr_pull_preg_header(ndr, NDR_SCALARS, &r->header));
			r->num_entries = 0;
			NDR_PULL_ALLOC_N(ndr, r->entries, r->num_entries);
			while (ndr->offset + 12 <= ndr->data_size) {
				r->entries = talloc_realloc(ndr, r->entries, struct preg_entry, r->num_entries + 1);
				NDR_ERR_HAVE_NO_MEMORY(r->entries);
				NDR_CHECK(ndr_pull_preg_entry(ndr, NDR_SCALARS, &r->entries[r->num_entries]));
				r->num_entries++;
			}
			NDR_CHECK(ndr_pull_trailer_align(ndr, 4));
		}
		if (ndr_flags & NDR_BUFFERS) {
		}
		ndr->flags = _flags_save_STRUCT;
	}
	return NDR_ERR_SUCCESS;
}
