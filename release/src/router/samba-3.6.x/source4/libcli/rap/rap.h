/*
   Unix SMB/CIFS implementation.
   RAP client
   Copyright (C) Volker Lendecke 2004
   Copyright (C) Tim Potter 2005
   Copyright (C) Jelmer Vernooij 2007
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

#define RAP_GOTO(call) do { \
	NTSTATUS _status; \
	_status = call; \
	if (!NT_STATUS_IS_OK(_status)) { \
		result = _status; \
		goto done; \
	} \
} while (0)

#define RAP_RETURN(call) do { \
	NTSTATUS _status; \
	_status = call; \
	if (!NT_STATUS_IS_OK(_status)) { \
		return _status; \
	} \
} while (0)

#define NDR_GOTO(call) do { \
	enum ndr_err_code _ndr_err; \
	_ndr_err = call; \
	if (!NDR_ERR_CODE_IS_SUCCESS(_ndr_err)) { \
		result = ndr_map_error2ntstatus(_ndr_err); \
		goto done; \
	} \
} while (0)

#define NDR_RETURN(call) do { \
	enum ndr_err_code _ndr_err; \
	_ndr_err = call; \
	if (!NDR_ERR_CODE_IS_SUCCESS(_ndr_err)) { \
		return ndr_map_error2ntstatus(_ndr_err); \
	} \
} while (0)

struct rap_call {
	uint16_t callno;
	char *paramdesc;
	const char *datadesc;
	const char *auxdatadesc;

	uint16_t status;
	uint16_t convert;

	uint16_t rcv_paramlen, rcv_datalen;

	struct ndr_push *ndr_push_param;
	struct ndr_push *ndr_push_data;
	struct ndr_pull *ndr_pull_param;
	struct ndr_pull *ndr_pull_data;
};

#define RAPNDR_FLAGS (LIBNDR_FLAG_NOALIGN|LIBNDR_FLAG_STR_ASCII|LIBNDR_FLAG_STR_NULLTERM);

#include "../librpc/gen_ndr/rap.h"
#include "libcli/rap/proto.h"
