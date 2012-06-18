/*
 * Unix SMB/CIFS implementation.
 * Registry helper routines
 * Copyright (C) Michael Adam 2008
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

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_REGISTRY

WERROR registry_init_common(void)
{
	WERROR werr;

	werr = regdb_init();
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0, ("Failed to initialize the registry: %s\n",
			  win_errstr(werr)));
		goto done;
	}

	werr = reghook_cache_init();
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0, ("Failed to initialize the reghook cache: %s\n",
			  win_errstr(werr)));
		goto done;
	}

	/* setup the necessary keys and values */

	werr = init_registry_data();
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0, ("Failed to initialize data in registry!\n"));
	}

done:
	return werr;
}

WERROR registry_init_basic(void)
{
	WERROR werr;

	DEBUG(10, ("registry_init_basic called\n"));

	werr = registry_init_common();
	regdb_close();
	return werr;
}
