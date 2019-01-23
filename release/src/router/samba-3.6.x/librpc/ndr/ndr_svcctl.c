/*
   Unix SMB/CIFS implementation.

   routines for marshalling/unmarshalling special svcctl types

   Copyright (C) Guenther Deschner 2009

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
#include "librpc/gen_ndr/ndr_svcctl.h"

_PUBLIC_ enum ndr_err_code ndr_push_ENUM_SERVICE_STATUSW_array(struct ndr_push *ndr, uint32_t count, struct ENUM_SERVICE_STATUSW *r)
{
	uint32_t cntr_array_1;

	for (cntr_array_1 = 0; cntr_array_1 < count; cntr_array_1++) {
		NDR_CHECK(ndr_push_ENUM_SERVICE_STATUSW(ndr, NDR_SCALARS, &r[cntr_array_1]));
	}
	for (cntr_array_1 = 0; cntr_array_1 < count; cntr_array_1++) {
		NDR_CHECK(ndr_push_ENUM_SERVICE_STATUSW(ndr, NDR_BUFFERS, &r[cntr_array_1]));
	}

	return NDR_ERR_SUCCESS;

}

_PUBLIC_ enum ndr_err_code ndr_pull_ENUM_SERVICE_STATUSW_array(struct ndr_pull *ndr, uint32_t count, struct ENUM_SERVICE_STATUSW *r)
{
	uint32_t cntr_array_1;

	for (cntr_array_1 = 0; cntr_array_1 < count; cntr_array_1++) {
		NDR_CHECK(ndr_pull_ENUM_SERVICE_STATUSW(ndr, NDR_SCALARS, &r[cntr_array_1]));
	}
	for (cntr_array_1 = 0; cntr_array_1 < count; cntr_array_1++) {
		NDR_CHECK(ndr_pull_ENUM_SERVICE_STATUSW(ndr, NDR_BUFFERS, &r[cntr_array_1]));
	}

	return NDR_ERR_SUCCESS;
}
