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
 * HKPT parameters registry backend.
 *
 * This replaces the former dynamic hkpt parameters overlay.
 */

#include "includes.h"
#include "registry.h"
#include "reg_perfcount.h"
#include "reg_objects.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_REGISTRY

extern struct registry_ops regdb_ops;

static int hkpt_params_fetch_values(const char *key, struct regval_ctr *regvals)
{
	uint32 base_index;
	uint32 buffer_size;
	char *buffer = NULL;

	/* This is ALMOST the same as perflib_009_params, but HKPT has
	   a "Counters" entry instead of a "Counter" key. <Grrrr> */

	base_index = reg_perfcount_get_base_index();
	buffer_size = reg_perfcount_get_counter_names(base_index, &buffer);
	regval_ctr_addvalue(regvals, "Counters", REG_MULTI_SZ, (uint8 *)buffer,
			    buffer_size);

	if(buffer_size > 0) {
		SAFE_FREE(buffer);
	}

	buffer_size = reg_perfcount_get_counter_help(base_index, &buffer);
	regval_ctr_addvalue(regvals, "Help", REG_MULTI_SZ, (uint8 *)buffer, buffer_size);
	if(buffer_size > 0) {
		SAFE_FREE(buffer);
	}

	return regval_ctr_numvals( regvals );
}

static int hkpt_params_fetch_subkeys(const char *key,
					 struct regsubkey_ctr *subkey_ctr)
{
	return regdb_ops.fetch_subkeys(key, subkey_ctr);
}

struct registry_ops hkpt_params_reg_ops = {
	.fetch_values = hkpt_params_fetch_values,
	.fetch_subkeys = hkpt_params_fetch_subkeys,
};
