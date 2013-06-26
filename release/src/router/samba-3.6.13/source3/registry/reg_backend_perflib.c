/*
 *  Unix SMB/CIFS implementation.
 *  Virtual Windows Registry Layer
 *  Copyright (C) Gerald Carter     2002-2005
 *  Copyright (C) Michael Adam      2008
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

/*
 * perflib registry backend.
 *
 * This is a virtual overlay, dynamically presenting perflib values.
 */

#include "includes.h"
#include "registry.h"
#include "reg_util_internal.h"
#include "reg_perfcount.h"
#include "reg_objects.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_REGISTRY

extern struct registry_ops regdb_ops;

#define KEY_PERFLIB_NORM	"HKLM\\SOFTWARE\\MICROSOFT\\WINDOWS NT\\CURRENTVERSION\\PERFLIB"
#define KEY_PERFLIB_009_NORM	"HKLM\\SOFTWARE\\MICROSOFT\\WINDOWS NT\\CURRENTVERSION\\PERFLIB\\009"

static int perflib_params(struct regval_ctr *regvals)
{
	int base_index = -1;
	int last_counter = -1;
	int last_help = -1;
	int version = 0x00010001;
	
	base_index = reg_perfcount_get_base_index();
	regval_ctr_addvalue(regvals, "Base Index", REG_DWORD, (uint8_t *)&base_index, sizeof(base_index));
	last_counter = reg_perfcount_get_last_counter(base_index);
	regval_ctr_addvalue(regvals, "Last Counter", REG_DWORD, (uint8_t *)&last_counter, sizeof(last_counter));
	last_help = reg_perfcount_get_last_help(last_counter);
	regval_ctr_addvalue(regvals, "Last Help", REG_DWORD, (uint8_t *)&last_help, sizeof(last_help));
	regval_ctr_addvalue(regvals, "Version", REG_DWORD, (uint8_t *)&version, sizeof(version));

	return regval_ctr_numvals( regvals );
}

static int perflib_009_params(struct regval_ctr *regvals)
{
	int base_index;
	int buffer_size;
	char *buffer = NULL;

	base_index = reg_perfcount_get_base_index();
	buffer_size = reg_perfcount_get_counter_names(base_index, &buffer);
	regval_ctr_addvalue(regvals, "Counter", REG_MULTI_SZ, (uint8_t *)buffer, buffer_size);
	if(buffer_size > 0)
		SAFE_FREE(buffer);
	buffer_size = reg_perfcount_get_counter_help(base_index, &buffer);
	regval_ctr_addvalue(regvals, "Help", REG_MULTI_SZ, (uint8_t *)buffer, buffer_size);
	if(buffer_size > 0)
		SAFE_FREE(buffer);
	
	return regval_ctr_numvals( regvals );
}

static int perflib_fetch_values(const char *key, struct regval_ctr *regvals)
{
	char *path = NULL;
	TALLOC_CTX *ctx = talloc_tos();

	path = talloc_strdup(ctx, key);
	if (path == NULL) {
		return -1;
	}
	path = normalize_reg_path(ctx, path);
	if (path == NULL) {
		return -1;
	}

	if (strncmp(path, KEY_PERFLIB_NORM, strlen(path)) == 0) {
		return perflib_params(regvals);
	} else if (strncmp(path, KEY_PERFLIB_009_NORM, strlen(path)) == 0) {
		return perflib_009_params(regvals);
	} else {
		return 0;
	}
}

static int perflib_fetch_subkeys(const char *key,
				 struct regsubkey_ctr *subkey_ctr)
{
	return regdb_ops.fetch_subkeys(key, subkey_ctr);
}

struct registry_ops perflib_reg_ops = {
	.fetch_values = perflib_fetch_values,
	.fetch_subkeys = perflib_fetch_subkeys,
};
