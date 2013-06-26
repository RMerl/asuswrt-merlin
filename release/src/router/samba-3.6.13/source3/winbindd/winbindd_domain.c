/*
   Unix SMB/CIFS implementation.

   Winbind domain child functions

   Copyright (C) Stefan Metzmacher 2007

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
#include "winbindd.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND

static const struct winbindd_child_dispatch_table domain_dispatch_table[] = {
	{
		.name		= "PING",
		.struct_cmd	= WINBINDD_PING,
		.struct_fn	= winbindd_dual_ping,
	},{
		.name		= "LIST_TRUSTDOM",
		.struct_cmd	= WINBINDD_LIST_TRUSTDOM,
		.struct_fn	= winbindd_dual_list_trusted_domains,
	},{
		.name		= "INIT_CONNECTION",
		.struct_cmd	= WINBINDD_INIT_CONNECTION,
		.struct_fn	= winbindd_dual_init_connection,
	},{
		.name		= "PAM_AUTH",
		.struct_cmd	= WINBINDD_PAM_AUTH,
		.struct_fn	= winbindd_dual_pam_auth,
	},{
		.name		= "AUTH_CRAP",
		.struct_cmd	= WINBINDD_PAM_AUTH_CRAP,
		.struct_fn	= winbindd_dual_pam_auth_crap,
	},{
		.name		= "PAM_LOGOFF",
		.struct_cmd	= WINBINDD_PAM_LOGOFF,
		.struct_fn	= winbindd_dual_pam_logoff,
	},{
		.name		= "CHNG_PSWD_AUTH_CRAP",
		.struct_cmd	= WINBINDD_PAM_CHNG_PSWD_AUTH_CRAP,
		.struct_fn	= winbindd_dual_pam_chng_pswd_auth_crap,
	},{
		.name		= "PAM_CHAUTHTOK",
		.struct_cmd	= WINBINDD_PAM_CHAUTHTOK,
		.struct_fn	= winbindd_dual_pam_chauthtok,
	},{
		.name		= "NDRCMD",
		.struct_cmd	= WINBINDD_DUAL_NDRCMD,
		.struct_fn	= winbindd_dual_ndrcmd,
	},{
		.name		= NULL,
	}
};

void setup_domain_child(struct winbindd_domain *domain)
{
	int i;

        for (i=0; i<lp_winbind_max_domain_connections(); i++) {
                setup_child(domain, &domain->children[i],
			    domain_dispatch_table,
                            "log.wb", domain->name);
		domain->children[i].domain = domain;
	}
}
