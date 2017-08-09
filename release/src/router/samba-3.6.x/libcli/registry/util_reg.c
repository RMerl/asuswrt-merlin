/*
 * Unix SMB/CIFS implementation.
 * Registry helper routines
 * Copyright (C) Volker Lendecke 2006
 * Copyright (C) Guenther Deschner 2009
 * Copyright (C) Jelmer Vernooij 2003-2007
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
#include "../librpc/gen_ndr/ndr_misc.h"
#include "../libcli/registry/util_reg.h"

/**
 * @file
 * @brief Registry utility functions
 */

static const struct {
	uint32_t id;
	const char *name;
} reg_value_types[] = {
	{ REG_NONE, "REG_NONE" },
	{ REG_SZ, "REG_SZ" },
	{ REG_EXPAND_SZ, "REG_EXPAND_SZ" },
	{ REG_BINARY, "REG_BINARY" },
	{ REG_DWORD, "REG_DWORD" },
	{ REG_DWORD_BIG_ENDIAN, "REG_DWORD_BIG_ENDIAN" },
	{ REG_LINK, "REG_LINK" },
	{ REG_MULTI_SZ, "REG_MULTI_SZ" },
	{ REG_RESOURCE_LIST, "REG_RESOURCE_LIST" },
	{ REG_FULL_RESOURCE_DESCRIPTOR, "REG_FULL_RESOURCE_DESCRIPTOR" },
	{ REG_RESOURCE_REQUIREMENTS_LIST, "REG_RESOURCE_REQUIREMENTS_LIST" },
	{ REG_QWORD, "REG_QWORD" },

	{ 0, NULL }
};

/** Return string description of registry value type */
_PUBLIC_ const char *str_regtype(int type)
{
	unsigned int i;
	for (i = 0; reg_value_types[i].name; i++) {
		if (reg_value_types[i].id == type)
			return reg_value_types[i].name;
	}

	return "Unknown";
}

/** Return registry value type for string description */
_PUBLIC_ int regtype_by_string(const char *str)
{
	unsigned int i;
	for (i = 0; reg_value_types[i].name; i++) {
		if (strequal(reg_value_types[i].name, str))
			return reg_value_types[i].id;
	}

	return -1;
}

/*******************************************************************
 push a string in unix charset into a REG_SZ UCS2 null terminated blob
 ********************************************************************/

bool push_reg_sz(TALLOC_CTX *mem_ctx, DATA_BLOB *blob, const char *s)
{
	union winreg_Data data;
	enum ndr_err_code ndr_err;
	data.string = s;
	ndr_err = ndr_push_union_blob(blob, mem_ctx, &data, REG_SZ,
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
	ndr_err = ndr_push_union_blob(blob, mem_ctx, &data, REG_MULTI_SZ,
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
	ndr_err = ndr_pull_union_blob(blob, mem_ctx, &data, REG_SZ,
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

bool pull_reg_multi_sz(TALLOC_CTX *mem_ctx, 
		       const DATA_BLOB *blob, const char ***a)
{
	union winreg_Data data;
	enum ndr_err_code ndr_err;
	ndr_err = ndr_pull_union_blob(blob, mem_ctx, &data, REG_MULTI_SZ,
			(ndr_pull_flags_fn_t)ndr_pull_winreg_Data);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return false;
	}
	*a = data.string_array;
	return true;
}
