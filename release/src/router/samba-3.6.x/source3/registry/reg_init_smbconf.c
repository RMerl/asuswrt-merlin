/*
 * Unix SMB/CIFS implementation.
 * Registry helper routines
 * Copyright (C) Michael Adam 2007
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
#include "registry.h"
#include "reg_cachehook.h"
#include "reg_backend_db.h"
#include "reg_init_basic.h"
#include "reg_init_smbconf.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_REGISTRY

extern struct registry_ops smbconf_reg_ops;

/*
 * init the smbconf portion of the registry.
 * for use in places where not the whole registry is needed,
 * e.g. utils/net_conf.c and loadparm.c
 */
WERROR registry_init_smbconf(const char *keyname)
{
	WERROR werr;

	DEBUG(10, ("registry_init_smbconf called\n"));

	if (keyname == NULL) {
		DEBUG(10, ("registry_init_smbconf: defaulting to key '%s'\n",
			   KEY_SMBCONF));
		keyname = KEY_SMBCONF;
	}

	werr = registry_init_common();
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	werr = init_registry_key(keyname);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(1, ("Failed to initialize registry key '%s': %s\n",
			  keyname, win_errstr(werr)));
		goto done;
	}

	werr = reghook_cache_add(keyname, &smbconf_reg_ops);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(1, ("Failed to add smbconf reghooks to reghook cache: "
			  "%s\n", win_errstr(werr)));
		goto done;
	}

done:
	regdb_close();
	return werr;
}
