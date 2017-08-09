/*
   Unix SMB/CIFS implementation.
   Translate BUILTIN names to SIDs and vice versa
   Copyright (C) Volker Lendecke 2005

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
#include "../libcli/security/security.h"

struct rid_name_map {
	uint32 rid;
	const char *name;
};

static const struct rid_name_map builtin_aliases[] = {
	{ BUILTIN_RID_ADMINISTRATORS,		"Administrators" },
	{ BUILTIN_RID_USERS,		"Users" },
	{ BUILTIN_RID_GUESTS,		"Guests" },
	{ BUILTIN_RID_POWER_USERS,	"Power Users" },
	{ BUILTIN_RID_ACCOUNT_OPERATORS,	"Account Operators" },
	{ BUILTIN_RID_SERVER_OPERATORS,		"Server Operators" },
	{ BUILTIN_RID_PRINT_OPERATORS,		"Print Operators" },
	{ BUILTIN_RID_BACKUP_OPERATORS,		"Backup Operators" },
	{ BUILTIN_RID_REPLICATOR,		"Replicator" },
	{ BUILTIN_RID_RAS_SERVERS,		"RAS Servers" },
	{ BUILTIN_RID_PRE_2K_ACCESS,
		"Pre-Windows 2000 Compatible Access" },
	{ BUILTIN_RID_REMOTE_DESKTOP_USERS,
		"Remote Desktop Users" },
	{ BUILTIN_RID_NETWORK_CONF_OPERATORS,
		"Network Configuration Operators" },
	{ BUILTIN_RID_INCOMING_FOREST_TRUST,
		"Incoming Forest Trust Builders" },
	{ BUILTIN_RID_PERFMON_USERS,
		"Performance Monitor Users" },
	{ BUILTIN_RID_PERFLOG_USERS,
		"Performance Log Users" },
	{ BUILTIN_RID_AUTH_ACCESS,
		"Windows Authorization Access Group" },
	{ BUILTIN_RID_TS_LICENSE_SERVERS,
		"Terminal Server License Servers" },
	{  0, NULL}};

/*******************************************************************
 Look up a rid in the BUILTIN domain
 ********************************************************************/
bool lookup_builtin_rid(TALLOC_CTX *mem_ctx, uint32 rid, const char **name)
{
	const struct rid_name_map *aliases = builtin_aliases;

	while (aliases->name != NULL) {
		if (rid == aliases->rid) {
			*name = talloc_strdup(mem_ctx, aliases->name);
			return True;
		}
		aliases++;
	}

	return False;
}

/*******************************************************************
 Look up a name in the BUILTIN domain
 ********************************************************************/
bool lookup_builtin_name(const char *name, uint32 *rid)
{
	const struct rid_name_map *aliases = builtin_aliases;

	while (aliases->name != NULL) {
		if (strequal(name, aliases->name)) {
			*rid = aliases->rid;
			return True;
		}
		aliases++;
	}

	return False;
}

/*****************************************************************
 Return the name of the BUILTIN domain
*****************************************************************/

const char *builtin_domain_name(void)
{
	return "BUILTIN";
}

/*****************************************************************
 Check if the SID is the builtin SID (S-1-5-32).
*****************************************************************/

bool sid_check_is_builtin(const struct dom_sid *sid)
{
	return dom_sid_equal(sid, &global_sid_Builtin);
}

/*****************************************************************
 Check if the SID is one of the builtin SIDs (S-1-5-32-a).
*****************************************************************/

bool sid_check_is_in_builtin(const struct dom_sid *sid)
{
	struct dom_sid dom_sid;

	sid_copy(&dom_sid, sid);
	sid_split_rid(&dom_sid, NULL);

	return sid_check_is_builtin(&dom_sid);
}
