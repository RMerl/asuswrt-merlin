/*
 * Unix SMB/CIFS implementation.
 * Registry helper routines
 * Copyright (C) Volker Lendecke 2006
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "../librpc/gen_ndr/ndr_winreg.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_REGISTRY

extern struct registry_ops smbconf_reg_ops;

const char *reg_type_lookup(enum winreg_Type type)
{
	const char *result;

	switch(type) {
	case REG_NONE:
		result = "REG_NONE";
		break;
	case REG_SZ:
		result = "REG_SZ";
		break;
	case REG_EXPAND_SZ:
		result = "REG_EXPAND_SZ";
		break;
	case REG_BINARY:
		result = "REG_BINARY";
		break;
	case REG_DWORD:
		result = "REG_DWORD";
		break;
	case REG_DWORD_BIG_ENDIAN:
		result = "REG_DWORD_BIG_ENDIAN";
		break;
	case REG_LINK:
		result = "REG_LINK";
		break;
	case REG_MULTI_SZ:
		result = "REG_MULTI_SZ";
		break;
	case REG_RESOURCE_LIST:
		result = "REG_RESOURCE_LIST";
		break;
	case REG_FULL_RESOURCE_DESCRIPTOR:
		result = "REG_FULL_RESOURCE_DESCRIPTOR";
		break;
	case REG_RESOURCE_REQUIREMENTS_LIST:
		result = "REG_RESOURCE_REQUIREMENTS_LIST";
		break;
	case REG_QWORD:
		result = "REG_QWORD";
		break;
	default:
		result = "REG TYPE IS UNKNOWN";
		break;
	}
	return result;
}

/*******************************************************************
 push a string in unix charset into a REG_SZ UCS2 null terminated blob
 ********************************************************************/

bool push_reg_sz(TALLOC_CTX *mem_ctx, DATA_BLOB *blob, const char *s)
{
	union winreg_Data data;
	enum ndr_err_code ndr_err;
	data.string = s;
	ndr_err = ndr_push_union_blob(blob, mem_ctx, NULL, &data, REG_SZ,
			(ndr_push_flags_fn_t)ndr_push_winreg_Data);
	return NDR_ERR_CODE_IS_SUCCESS(ndr_err);
}

/*******************************************************************
 push a string_array in unix charset into a REG_MULTI_SZ UCS2 double-null
 terminated blob
 ********************************************************************/

bool push_reg_multi_sz(TALLOC_CTX *mem_ctx, DATA_BLOB *blob, const char **a)
{
	union winreg_Data data;
	enum ndr_err_code ndr_err;
	data.string_array = a;
	ndr_err = ndr_push_union_blob(blob, mem_ctx, NULL, &data, REG_MULTI_SZ,
			(ndr_push_flags_fn_t)ndr_push_winreg_Data);
	return NDR_ERR_CODE_IS_SUCCESS(ndr_err);
}

/*******************************************************************
 pull a string in unix charset out of a REG_SZ UCS2 null terminated blob
 ********************************************************************/

bool pull_reg_sz(TALLOC_CTX *mem_ctx, const DATA_BLOB *blob, const char **s)
{
	union winreg_Data data;
	enum ndr_err_code ndr_err;
	ndr_err = ndr_pull_union_blob(blob, mem_ctx, NULL, &data, REG_SZ,
			(ndr_pull_flags_fn_t)ndr_pull_winreg_Data);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return false;
	}
	*s = data.string;
	return true;
}

/*******************************************************************
 pull a string_array in unix charset out of a REG_MULTI_SZ UCS2 double-null
 terminated blob
 ********************************************************************/

bool pull_reg_multi_sz(TALLOC_CTX *mem_ctx, const DATA_BLOB *blob, const char ***a)
{
	union winreg_Data data;
	enum ndr_err_code ndr_err;
	ndr_err = ndr_pull_union_blob(blob, mem_ctx, NULL, &data, REG_MULTI_SZ,
			(ndr_pull_flags_fn_t)ndr_pull_winreg_Data);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return false;
	}
	*a = data.string_array;
	return true;
}
