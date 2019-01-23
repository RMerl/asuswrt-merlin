/*
   Unix SMB/CIFS implementation.
   Privileges handling functions
   Copyright (C) Jean François Micouleau	1998-2001
   Copyright (C) Simo Sorce			2002-2003
   Copyright (C) Gerald (Jerry) Carter          2005
   Copyright (C) Michael Adam			2007
   Copyright (C) Andrew Bartlett		2010
   Copyright (C) Andrew Tridgell                2004

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

/*
 * Basic privileges functions (mask-operations and conversion
 * functions between the different formats (se_priv, privset, luid)
 * moved here * from lib/privileges.c to minimize linker deps.
 *
 * generally SID- and LUID-related code is left in lib/privileges.c
 *
 * some extra functions to hide privs array from lib/privileges.c
 */

#include "includes.h"
#include "libcli/security/privileges.h"
#include "libcli/security/privileges_private.h"
#include "librpc/gen_ndr/security.h"

/* The use of strcasecmp here is safe, all the comparison strings are ASCII */
#undef strcasecmp

#define NUM_SHORT_LIST_PRIVS 9

static const struct {
	enum sec_privilege luid;
	uint64_t privilege_mask;
	const char *name;
	const char *description;
} privs[] = {

	{SEC_PRIV_MACHINE_ACCOUNT, SEC_PRIV_MACHINE_ACCOUNT_BIT,   "SeMachineAccountPrivilege",	"Add machines to domain"},
	{SEC_PRIV_TAKE_OWNERSHIP,  SEC_PRIV_TAKE_OWNERSHIP_BIT,    "SeTakeOwnershipPrivilege",    "Take ownership of files or other objects"},
        {SEC_PRIV_BACKUP,          SEC_PRIV_BACKUP_BIT,            "SeBackupPrivilege",           "Back up files and directories"},
        {SEC_PRIV_RESTORE,         SEC_PRIV_RESTORE_BIT,           "SeRestorePrivilege",          "Restore files and directories"},
	{SEC_PRIV_REMOTE_SHUTDOWN, SEC_PRIV_REMOTE_SHUTDOWN_BIT,   "SeRemoteShutdownPrivilege",	"Force shutdown from a remote system"},

	{SEC_PRIV_PRINT_OPERATOR,  SEC_PRIV_PRINT_OPERATOR_BIT,	 "SePrintOperatorPrivilege",	"Manage printers"},
	{SEC_PRIV_ADD_USERS,       SEC_PRIV_ADD_USERS_BIT,	 "SeAddUsersPrivilege",		"Add users and groups to the domain"},
	{SEC_PRIV_DISK_OPERATOR,   SEC_PRIV_DISK_OPERATOR_BIT,	 "SeDiskOperatorPrivilege",	"Manage disk shares"},
	{SEC_PRIV_SECURITY,	   SEC_PRIV_SECURITY_BIT,	 "SeSecurityPrivilege",	"System security"},


	/* The list from here on is not displayed in the code from
	 * source3, and is after index NUM_SHORT_LIST_PRIVS for that
	 * reason */ 

	{SEC_PRIV_SYSTEMTIME,
	 SEC_PRIV_SYSTEMTIME_BIT,
	 "SeSystemtimePrivilege",
	"Set the system clock"},

	{SEC_PRIV_SHUTDOWN,
	 SEC_PRIV_SHUTDOWN_BIT,
	 "SeShutdownPrivilege",
	"Shutdown the system"},

	{SEC_PRIV_DEBUG,
	 SEC_PRIV_DEBUG_BIT,
	 "SeDebugPrivilege",
	"Debug processes"},

	{SEC_PRIV_SYSTEM_ENVIRONMENT,
	 SEC_PRIV_SYSTEM_ENVIRONMENT_BIT,
	 "SeSystemEnvironmentPrivilege",
	"Modify system environment"},

	{SEC_PRIV_SYSTEM_PROFILE,
	 SEC_PRIV_SYSTEM_PROFILE_BIT,
	 "SeSystemProfilePrivilege",
	"Profile the system"},

	{SEC_PRIV_PROFILE_SINGLE_PROCESS,
	 SEC_PRIV_PROFILE_SINGLE_PROCESS_BIT,
	 "SeProfileSingleProcessPrivilege",
	"Profile one process"},

	{SEC_PRIV_INCREASE_BASE_PRIORITY,
	 SEC_PRIV_INCREASE_BASE_PRIORITY_BIT,
	 "SeIncreaseBasePriorityPrivilege",
	 "Increase base priority"},

	{SEC_PRIV_LOAD_DRIVER,
	 SEC_PRIV_LOAD_DRIVER_BIT,
	 "SeLoadDriverPrivilege",
	"Load drivers"},

	{SEC_PRIV_CREATE_PAGEFILE,
	 SEC_PRIV_CREATE_PAGEFILE_BIT,
	 "SeCreatePagefilePrivilege",
	"Create page files"},

	{SEC_PRIV_INCREASE_QUOTA,
	 SEC_PRIV_INCREASE_QUOTA_BIT,
	 "SeIncreaseQuotaPrivilege",
	"Increase quota"},

	{SEC_PRIV_CHANGE_NOTIFY,
	 SEC_PRIV_CHANGE_NOTIFY_BIT,
	 "SeChangeNotifyPrivilege",
	"Register for change notify"},

	{SEC_PRIV_UNDOCK,
	 SEC_PRIV_UNDOCK_BIT,
	 "SeUndockPrivilege",
	"Undock devices"},

	{SEC_PRIV_MANAGE_VOLUME,
	 SEC_PRIV_MANAGE_VOLUME_BIT,
	 "SeManageVolumePrivilege",
	"Manage system volumes"},

	{SEC_PRIV_IMPERSONATE,
	 SEC_PRIV_IMPERSONATE_BIT,
	 "SeImpersonatePrivilege",
	"Impersonate users"},

	{SEC_PRIV_CREATE_GLOBAL,
	 SEC_PRIV_CREATE_GLOBAL_BIT,
	 "SeCreateGlobalPrivilege",
	"Create global"},

	{SEC_PRIV_ENABLE_DELEGATION,
	 SEC_PRIV_ENABLE_DELEGATION_BIT,
	 "SeEnableDelegationPrivilege",
	"Enable Delegation"},
};

/* These are rights, not privileges, and should not be confused.  The
 * names are very similar, and they are quite similar in behaviour,
 * but they are not to be enumerated as a system-wide list or have an
 * LUID value */
static const struct {
	uint32_t right_mask;
	const char *name;
	const char *description;
} rights[] = {
	{LSA_POLICY_MODE_INTERACTIVE,
	 "SeInteractiveLogonRight",
	"Interactive logon"},

	{LSA_POLICY_MODE_NETWORK,
	 "SeNetworkLogonRight",
	"Network logon"},

	{LSA_POLICY_MODE_REMOTE_INTERACTIVE,
	 "SeRemoteInteractiveLogonRight",
	"Remote Interactive logon"}
};

/*
  return a privilege mask given a privilege id
*/
uint64_t sec_privilege_mask(enum sec_privilege privilege)
{
	int i;
	for (i=0;i<ARRAY_SIZE(privs);i++) {
		if (privs[i].luid == privilege) {
			return privs[i].privilege_mask;
		}
	}

	return 0;
}

/***************************************************************************
 put all valid privileges into a mask
****************************************************************************/

void se_priv_put_all_privileges(uint64_t *privilege_mask)
{
	int i;

	*privilege_mask = 0;
	for ( i=0; i<ARRAY_SIZE(privs); i++ ) {
		*privilege_mask |= privs[i].privilege_mask;
	}
}

/*********************************************************************
 Lookup the uint64_t bitmask value for a privilege name
*********************************************************************/

bool se_priv_from_name( const char *name, uint64_t *privilege_mask )
{
	int i;
	for ( i=0; i<ARRAY_SIZE(privs); i++ ) {
		if ( strequal( privs[i].name, name ) ) {
			*privilege_mask = privs[i].privilege_mask;
			return true;
		}
	}

	return false;
}

const char* get_privilege_dispname( const char *name )
{
	int i;

	if (!name) {
		return NULL;
	}

	for ( i=0; i<ARRAY_SIZE(privs); i++ ) {
		if ( strequal( privs[i].name, name ) ) {
			return privs[i].description;
		}
	}

	return NULL;
}

/*******************************************************************
 return the number of elements in the 'short' privlege array (traditional source3 behaviour)
*******************************************************************/

int num_privileges_in_short_list( void )
{
	return NUM_SHORT_LIST_PRIVS;
}

/****************************************************************************
 add a privilege to a privilege array
 ****************************************************************************/

static bool privilege_set_add(PRIVILEGE_SET *priv_set, struct lsa_LUIDAttribute set)
{
	struct lsa_LUIDAttribute *new_set;

	/* we can allocate memory to add the new privilege */

	new_set = talloc_realloc(priv_set->mem_ctx, priv_set->set, struct lsa_LUIDAttribute, priv_set->count + 1);
	if ( !new_set ) {
		DEBUG(0,("privilege_set_add: failed to allocate memory!\n"));
		return false;
	}

	new_set[priv_set->count].luid.high = set.luid.high;
	new_set[priv_set->count].luid.low = set.luid.low;
	new_set[priv_set->count].attribute = set.attribute;

	priv_set->count++;
	priv_set->set = new_set;

	return true;
}

/*******************************************************************
*******************************************************************/

bool se_priv_to_privilege_set( PRIVILEGE_SET *set, uint64_t privilege_mask )
{
	int i;
	struct lsa_LUIDAttribute luid;

	luid.attribute = 0;
	luid.luid.high = 0;

	for ( i=0; i<ARRAY_SIZE(privs); i++ ) {
		if ((privilege_mask & privs[i].privilege_mask) == 0)
			continue;

		luid.luid.high = 0;
		luid.luid.low = privs[i].luid;

		if ( !privilege_set_add( set, luid ) )
			return false;
	}

	return true;
}

/*******************************************************************
*******************************************************************/

bool privilege_set_to_se_priv( uint64_t *privilege_mask, struct lsa_PrivilegeSet *privset )
{
	uint32_t i;

	ZERO_STRUCTP( privilege_mask );

	for ( i=0; i<privset->count; i++ ) {
		uint64_t r;

		/* sanity check for invalid privilege.  we really
		   only care about the low 32 bits */

		if ( privset->set[i].luid.high != 0 )
			return false;

		r = sec_privilege_mask(privset->set[i].luid.low);
		if (r) {
			*privilege_mask |= r;
		}
	}

	return true;
}

/*
  map a privilege id to the wire string constant
*/
const char *sec_privilege_name(enum sec_privilege privilege)
{
	int i;
	for (i=0;i<ARRAY_SIZE(privs);i++) {
		if (privs[i].luid == privilege) {
			return privs[i].name;
		}
	}
	return NULL;
}

/*
  map a privilege id to a privilege display name. Return NULL if not found

  TODO: this should use language mappings
*/
const char *sec_privilege_display_name(enum sec_privilege privilege, uint16_t *language)
{
	int i;
	for (i=0;i<ARRAY_SIZE(privs);i++) {
		if (privs[i].luid == privilege) {
			return privs[i].description;
		}
	}
	return NULL;
}

/*
  map a privilege name to a privilege id. Return SEC_PRIV_INVALID if not found
*/
enum sec_privilege sec_privilege_id(const char *name)
{
	int i;
	for (i=0;i<ARRAY_SIZE(privs);i++) {
		if (strcasecmp(privs[i].name, name) == 0) {
			return privs[i].luid;
		}
	}
	return SEC_PRIV_INVALID;
}

/*
  map a 'right' name to it's bitmap value. Return 0 if not found
*/
uint32_t sec_right_bit(const char *name)
{
	int i;
	for (i=0;i<ARRAY_SIZE(rights);i++) {
		if (strcasecmp(rights[i].name, name) == 0) {
			return rights[i].right_mask;
		}
	}
	return 0;
}

/*
  assist in walking the table of privileges - return the LUID (low 32 bits) by index
*/
enum sec_privilege sec_privilege_from_index(int idx)
{
	if (idx >= 0 && idx<ARRAY_SIZE(privs)) {
		return privs[idx].luid;
	}
	return SEC_PRIV_INVALID;
}

/*
  assist in walking the table of privileges - return the string constant by index
*/
const char *sec_privilege_name_from_index(int idx)
{
	if (idx >= 0 && idx<ARRAY_SIZE(privs)) {
		return privs[idx].name;
	}
	return NULL;
}



/*
  return true if a security_token has a particular privilege bit set
*/
bool security_token_has_privilege(const struct security_token *token, enum sec_privilege privilege)
{
	uint64_t mask;

	if (!token) {
		return false;
	}

	mask = sec_privilege_mask(privilege);
	if (mask == 0) {
		return false;
	}

	if (token->privilege_mask & mask) {
		return true;
	}
	return false;
}

/*
  set a bit in the privilege mask
*/
void security_token_set_privilege(struct security_token *token, enum sec_privilege privilege)
{
	/* Relies on the fact that an invalid privilage will return 0, so won't change this */
	token->privilege_mask |= sec_privilege_mask(privilege);
}

/*
  set a bit in the rights mask
*/
void security_token_set_right_bit(struct security_token *token, uint32_t right_bit)
{
	token->rights_mask |= right_bit;
}

void security_token_debug_privileges(int dbg_class, int dbg_lev, const struct security_token *token)
{
	DEBUGADDC(dbg_class, dbg_lev, (" Privileges (0x%16llX):\n",
				       (unsigned long long) token->privilege_mask));

	if (token->privilege_mask) {
		int idx = 0;
		int i = 0;
		for (idx = 0; idx<ARRAY_SIZE(privs); idx++) {
			if (token->privilege_mask & privs[idx].privilege_mask) {
				DEBUGADDC(dbg_class, dbg_lev,
					  ("  Privilege[%3lu]: %s\n", (unsigned long)i++,
					   privs[idx].name));
			}
		}
	}
	DEBUGADDC(dbg_class, dbg_lev, (" Rights (0x%16lX):\n",
				       (unsigned long) token->rights_mask));

	if (token->rights_mask) {
		int idx = 0;
		int i = 0;
		for (idx = 0; idx<ARRAY_SIZE(rights); idx++) {
			if (token->rights_mask & rights[idx].right_mask) {
				DEBUGADDC(dbg_class, dbg_lev,
					  ("  Right[%3lu]: %s\n", (unsigned long)i++,
					   rights[idx].name));
			}
		}
	}
}
