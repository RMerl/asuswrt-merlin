/* 
 *  Unix SMB/CIFS implementation.
 *  Virtual Windows Registry Layer
 *  Copyright (C) Gerald Carter                     2002-2005
 *  Copyright (C) Michael Adam                      2008
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

/* Initialize the registry with all available backends. */

#include "includes.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_REGISTRY

extern struct registry_ops printing_ops;
extern struct registry_ops eventlog_ops;
extern struct registry_ops shares_reg_ops;
extern struct registry_ops smbconf_reg_ops;
extern struct registry_ops netlogon_params_reg_ops;
extern struct registry_ops prod_options_reg_ops;
extern struct registry_ops tcpip_params_reg_ops;
extern struct registry_ops hkpt_params_reg_ops;
extern struct registry_ops current_version_reg_ops;
extern struct registry_ops perflib_reg_ops;
extern struct registry_ops regdb_ops;		/* these are the default */

/* array of registry_hook's which are read into a tree for easy access */
/* #define REG_TDB_ONLY		1 */

struct registry_hook reg_hooks[] = {
#ifndef REG_TDB_ONLY 
  { KEY_PRINTING,    		&printing_ops },
  { KEY_PRINTING_2K, 		&printing_ops },
  { KEY_PRINTING_PORTS, 	&printing_ops },
  { KEY_SHARES,      		&shares_reg_ops },
  { KEY_SMBCONF,      		&smbconf_reg_ops },
  { KEY_NETLOGON_PARAMS,	&netlogon_params_reg_ops },
  { KEY_PROD_OPTIONS,		&prod_options_reg_ops },
  { KEY_TCPIP_PARAMS,		&tcpip_params_reg_ops },
  { KEY_HKPT,			&hkpt_params_reg_ops },
  { KEY_CURRENT_VERSION,	&current_version_reg_ops },
  { KEY_PERFLIB,		&perflib_reg_ops },
#endif
  { NULL, NULL }
};

/***********************************************************************
 Open the registry database and initialize the registry_hook cache
 with all available backens.
 ***********************************************************************/

WERROR registry_init_full(void)
{
	int i;
	WERROR werr;

	werr = registry_init_common();
	if (!W_ERROR_IS_OK(werr)) {
		goto fail;
	}

	/* build the cache tree of registry hooks */

	for ( i=0; reg_hooks[i].keyname; i++ ) {
		werr = reghook_cache_add(reg_hooks[i].keyname, reg_hooks[i].ops);
		if (!W_ERROR_IS_OK(werr)) {
			goto fail;
		}
	}

	if ( DEBUGLEVEL >= 20 )
		reghook_dump_cache(20);

	/* add any keys for other services */

	svcctl_init_keys();
	eventlog_init_keys();
	perfcount_init_keys();

fail:
	/* close and let each smbd open up as necessary */
	regdb_close();
	return werr;
}
