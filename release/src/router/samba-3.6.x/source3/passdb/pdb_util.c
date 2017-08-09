/*
 *  Unix SMB/CIFS implementation.
 *  Authentication utility functions
 *  Copyright (C) Andrew Tridgell 1992-1998
 *  Copyright (C) Andrew Bartlett 2001
 *  Copyright (C) Jeremy Allison 2000-2001
 *  Copyright (C) Rafal Szczesniak 2002
 *  Copyright (C) Volker Lendecke 2006
 *  Copyright (C) Michael Adam 2007
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

#include "includes.h"
#include "../libcli/security/security.h"
#include "passdb.h"
#include "lib/winbind_util.h"

/**
 * Add sid as a member of builtin_sid.
 *
 * @param[in] builtin_sid	An existing builtin group.
 * @param[in] dom_sid		sid to add as a member of builtin_sid.
 * @return Normal NTSTATUS return
 */
static NTSTATUS add_sid_to_builtin(const struct dom_sid *builtin_sid,
				   const struct dom_sid *dom_sid)
{
	NTSTATUS status = NT_STATUS_OK;

	if (!dom_sid || !builtin_sid) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = pdb_add_aliasmem(builtin_sid, dom_sid);

	if (NT_STATUS_EQUAL(status, NT_STATUS_MEMBER_IN_ALIAS)) {
		DEBUG(5, ("add_sid_to_builtin %s is already a member of %s\n",
			  sid_string_dbg(dom_sid),
			  sid_string_dbg(builtin_sid)));
		return NT_STATUS_OK;
	}

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(4, ("add_sid_to_builtin %s could not be added to %s: "
			  "%s\n", sid_string_dbg(dom_sid),
			  sid_string_dbg(builtin_sid), nt_errstr(status)));
	}
	return status;
}

/**
 * Create the requested BUILTIN if it doesn't already exist.  This requires
 * winbindd to be running.
 *
 * @param[in] rid BUILTIN rid to create
 * @return Normal NTSTATUS return.
 */
static NTSTATUS create_builtin(uint32 rid)
{
	NTSTATUS status = NT_STATUS_OK;
	struct dom_sid sid;
	gid_t gid;

	if (!sid_compose(&sid, &global_sid_Builtin, rid)) {
		return NT_STATUS_NO_SUCH_ALIAS;
	}

	if (!sid_to_gid(&sid, &gid)) {
		if (!lp_winbind_nested_groups() || !winbind_ping()) {
			return NT_STATUS_PROTOCOL_UNREACHABLE;
		}
		status = pdb_create_builtin_alias(rid);
	}
	return status;
}

/*******************************************************************
*******************************************************************/

NTSTATUS create_builtin_users(const struct dom_sid *dom_sid)
{
	NTSTATUS status;
	struct dom_sid dom_users;

	status = create_builtin(BUILTIN_RID_USERS);
	if ( !NT_STATUS_IS_OK(status) ) {
		DEBUG(5,("create_builtin_users: Failed to create Users\n"));
		return status;
	}

	/* add domain users */
	if ((IS_DC || (lp_server_role() == ROLE_DOMAIN_MEMBER))
		&& sid_compose(&dom_users, dom_sid, DOMAIN_RID_USERS))
	{
		status = add_sid_to_builtin(&global_sid_Builtin_Users,
					    &dom_users);
	}

	return status;
}

/*******************************************************************
*******************************************************************/

NTSTATUS create_builtin_administrators(const struct dom_sid *dom_sid)
{
	NTSTATUS status;
	struct dom_sid dom_admins, root_sid;
	fstring root_name;
	enum lsa_SidType type;
	TALLOC_CTX *ctx;
	bool ret;

	status = create_builtin(BUILTIN_RID_ADMINISTRATORS);
	if ( !NT_STATUS_IS_OK(status) ) {
		DEBUG(5,("create_builtin_administrators: Failed to create Administrators\n"));
		return status;
	}

	/* add domain admins */
	if ((IS_DC || (lp_server_role() == ROLE_DOMAIN_MEMBER))
		&& sid_compose(&dom_admins, dom_sid, DOMAIN_RID_ADMINS))
	{
		status = add_sid_to_builtin(&global_sid_Builtin_Administrators,
					    &dom_admins);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	}

	/* add root */
	if ( (ctx = talloc_init("create_builtin_administrators")) == NULL ) {
		return NT_STATUS_NO_MEMORY;
	}
	fstr_sprintf( root_name, "%s\\root", get_global_sam_name() );
	ret = lookup_name(ctx, root_name, LOOKUP_NAME_DOMAIN, NULL, NULL,
			  &root_sid, &type);
	TALLOC_FREE( ctx );

	if ( ret ) {
		status = add_sid_to_builtin(&global_sid_Builtin_Administrators,
					    &root_sid);
	}

	return status;
}
