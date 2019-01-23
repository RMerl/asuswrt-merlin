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
 * TCP/IP parameters registry backend.
 *
 * This replaces the former dynamic tcpip parameters overlay.
 */

#include "includes.h"
#include "registry.h"
#include "reg_objects.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_REGISTRY

extern struct registry_ops regdb_ops;

static int tcpip_params_fetch_values(const char *key, struct regval_ctr *regvals)
{
	regval_ctr_addvalue_sz(regvals, "Hostname", myhostname());

	regval_ctr_addvalue_sz(regvals, "Domain", get_mydnsdomname(talloc_tos()));

	return regval_ctr_numvals(regvals);
}

static int tcpip_params_fetch_subkeys(const char *key,
				      struct regsubkey_ctr *subkey_ctr)
{
	return regdb_ops.fetch_subkeys(key, subkey_ctr);
}

struct registry_ops tcpip_params_reg_ops = {
	.fetch_values = tcpip_params_fetch_values,
	.fetch_subkeys = tcpip_params_fetch_subkeys,
};
