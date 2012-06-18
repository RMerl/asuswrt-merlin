/*
   Unix SMB/CIFS implementation.
   Privileges handling functions
   Copyright (C) Jean François Micouleau	1998-2001
   Copyright (C) Simo Sorce			2002-2003
   Copyright (C) Gerald (Jerry) Carter          2005
   Copyright (C) Michael Adam			2007

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

const SE_PRIV se_priv_all         = SE_ALL_PRIVS;
static const SE_PRIV se_priv_end  = SE_END;

/* Define variables for all privileges so we can use the
   SE_PRIV* in the various se_priv_XXX() functions */

const SE_PRIV se_priv_none       = SE_NONE;
const SE_PRIV se_machine_account = SE_MACHINE_ACCOUNT;
const SE_PRIV se_print_operator  = SE_PRINT_OPERATOR;
const SE_PRIV se_add_users       = SE_ADD_USERS;
const SE_PRIV se_disk_operators  = SE_DISK_OPERATOR;
const SE_PRIV se_remote_shutdown = SE_REMOTE_SHUTDOWN;
const SE_PRIV se_restore         = SE_RESTORE;
const SE_PRIV se_take_ownership  = SE_TAKE_OWNERSHIP;
const SE_PRIV se_security       = SE_SECURITY;

/********************************************************************
 This is a list of privileges reported by a WIndows 2000 SP4 AD DC
 just for reference purposes (and I know the LUID is not guaranteed
 across reboots):

            SeCreateTokenPrivilege  Create a token object ( 0x0, 0x2 )
     SeAssignPrimaryTokenPrivilege  Replace a process level token ( 0x0, 0x3 )
             SeLockMemoryPrivilege  Lock pages in memory ( 0x0, 0x4 )
          SeIncreaseQuotaPrivilege  Increase quotas ( 0x0, 0x5 )
         SeMachineAccountPrivilege  Add workstations to domain ( 0x0, 0x6 )
                    SeTcbPrivilege  Act as part of the operating system ( 0x0, 0x7 )
               SeSecurityPrivilege  Manage auditing and security log ( 0x0, 0x8 )
          SeTakeOwnershipPrivilege  Take ownership of files or other objects ( 0x0, 0x9 )
             SeLoadDriverPrivilege  Load and unload device drivers ( 0x0, 0xa )
          SeSystemProfilePrivilege  Profile system performance ( 0x0, 0xb )
             SeSystemtimePrivilege  Change the system time ( 0x0, 0xc )
   SeProfileSingleProcessPrivilege  Profile single process ( 0x0, 0xd )
   SeIncreaseBasePriorityPrivilege  Increase scheduling priority ( 0x0, 0xe )
         SeCreatePagefilePrivilege  Create a pagefile ( 0x0, 0xf )
        SeCreatePermanentPrivilege  Create permanent shared objects ( 0x0, 0x10 )
                 SeBackupPrivilege  Back up files and directories ( 0x0, 0x11 )
                SeRestorePrivilege  Restore files and directories ( 0x0, 0x12 )
               SeShutdownPrivilege  Shut down the system ( 0x0, 0x13 )
                  SeDebugPrivilege  Debug programs ( 0x0, 0x14 )
                  SeAuditPrivilege  Generate security audits ( 0x0, 0x15 )
      SeSystemEnvironmentPrivilege  Modify firmware environment values ( 0x0, 0x16 )
           SeChangeNotifyPrivilege  Bypass traverse checking ( 0x0, 0x17 )
         SeRemoteShutdownPrivilege  Force shutdown from a remote system ( 0x0, 0x18 )
                 SeUndockPrivilege  Remove computer from docking station ( 0x0, 0x19 )
              SeSyncAgentPrivilege  Synchronize directory service data ( 0x0, 0x1a )
       SeEnableDelegationPrivilege  Enable computer and user accounts to be trusted for delegation ( 0x0, 0x1b )
           SeManageVolumePrivilege  Perform volume maintenance tasks ( 0x0, 0x1c )
            SeImpersonatePrivilege  Impersonate a client after authentication ( 0x0, 0x1d )
           SeCreateGlobalPrivilege  Create global objects ( 0x0, 0x1e )

 ********************************************************************/

/* we have to define the LUID here due to a horrible check by printmig.exe
   that requires the SeBackupPrivilege match what is in Windows.  So match
   those that we implement and start Samba privileges at 0x1001 */

PRIVS privs[] = {
#if 0	/* usrmgr will display these twice if you include them.  We don't
	   use them but we'll keep the bitmasks reserved in privileges.h anyways */

	{SE_NETWORK_LOGON,	"SeNetworkLogonRight",		"Access this computer from network", 	   { 0x0, 0x0 }},
	{SE_INTERACTIVE_LOGON,	"SeInteractiveLogonRight",	"Log on locally", 			   { 0x0, 0x0 }},
	{SE_BATCH_LOGON,	"SeBatchLogonRight",		"Log on as a batch job",		   { 0x0, 0x0 }},
	{SE_SERVICE_LOGON,	"SeServiceLogonRight",		"Log on as a service",			   { 0x0, 0x0 }},
#endif
	{SE_MACHINE_ACCOUNT,	"SeMachineAccountPrivilege",	"Add machines to domain",		   { 0x0, 0x0006 }},
	{SE_SECURITY,		"SeSecurityPrivilege",		"Manage auditing and security log",	   { 0x0, 0x0008 }},
	{SE_TAKE_OWNERSHIP,     "SeTakeOwnershipPrivilege",     "Take ownership of files or other objects",{ 0x0, 0x0009 }},
        {SE_BACKUP,             "SeBackupPrivilege",            "Back up files and directories",	   { 0x0, 0x0011 }},
        {SE_RESTORE,            "SeRestorePrivilege",           "Restore files and directories",	   { 0x0, 0x0012 }},
	{SE_REMOTE_SHUTDOWN,	"SeRemoteShutdownPrivilege",	"Force shutdown from a remote system",	   { 0x0, 0x0018 }},

	{SE_PRINT_OPERATOR,	"SePrintOperatorPrivilege",	"Manage printers",			   { 0x0, 0x1001 }},
	{SE_ADD_USERS,		"SeAddUsersPrivilege",		"Add users and groups to the domain",	   { 0x0, 0x1002 }},
	{SE_DISK_OPERATOR,	"SeDiskOperatorPrivilege",	"Manage disk shares",			   { 0x0, 0x1003 }},


	{SE_END, "", "", { 0x0, 0x0 }}
};

/***************************************************************************
 copy an SE_PRIV structure
****************************************************************************/

bool se_priv_copy( SE_PRIV *dst, const SE_PRIV *src )
{
	if ( !dst || !src )
		return False;

	memcpy( dst, src, sizeof(SE_PRIV) );

	return True;
}

/***************************************************************************
 put all privileges into a mask
****************************************************************************/

bool se_priv_put_all_privileges(SE_PRIV *mask)
{
	int i;
	uint32 num_privs = count_all_privileges();

	if (!se_priv_copy(mask, &se_priv_none)) {
		return False;
	}
	for ( i=0; i<num_privs; i++ ) {
		se_priv_add(mask, &privs[i].se_priv);
	}
	return True;
}

/***************************************************************************
 combine 2 SE_PRIV structures and store the resulting set in mew_mask
****************************************************************************/

void se_priv_add( SE_PRIV *mask, const SE_PRIV *addpriv )
{
	int i;

	for ( i=0; i<SE_PRIV_MASKSIZE; i++ ) {
		mask->mask[i] |= addpriv->mask[i];
	}
}

/***************************************************************************
 remove one SE_PRIV sytucture from another and store the resulting set
 in mew_mask
****************************************************************************/

void se_priv_remove( SE_PRIV *mask, const SE_PRIV *removepriv )
{
	int i;

	for ( i=0; i<SE_PRIV_MASKSIZE; i++ ) {
		mask->mask[i] &= ~removepriv->mask[i];
	}
}

/***************************************************************************
 invert a given SE_PRIV and store the set in new_mask
****************************************************************************/

static void se_priv_invert( SE_PRIV *new_mask, const SE_PRIV *mask )
{
	SE_PRIV allprivs;

	se_priv_copy( &allprivs, &se_priv_all );
	se_priv_remove( &allprivs, mask );
	se_priv_copy( new_mask, &allprivs );
}

/***************************************************************************
 check if 2 SE_PRIV structure are equal
****************************************************************************/

bool se_priv_equal( const SE_PRIV *mask1, const SE_PRIV *mask2 )
{
	return ( memcmp(mask1, mask2, sizeof(SE_PRIV)) == 0 );
}

/***************************************************************************
 check if 2 LUID's are equal.
****************************************************************************/

static bool luid_equal( const LUID *luid1, const LUID *luid2 )
{
	return ( luid1->low == luid2->low && luid1->high == luid2->high);
}

/***************************************************************************
 check if a SE_PRIV has any assigned privileges
****************************************************************************/

static bool se_priv_empty( const SE_PRIV *mask )
{
	SE_PRIV p1;
	int i;

	se_priv_copy( &p1, mask );

	for ( i=0; i<SE_PRIV_MASKSIZE; i++ ) {
		p1.mask[i] &= se_priv_all.mask[i];
	}

	return se_priv_equal( &p1, &se_priv_none );
}

/*********************************************************************
 Lookup the SE_PRIV value for a privilege name
*********************************************************************/

bool se_priv_from_name( const char *name, SE_PRIV *mask )
{
	int i;

	for ( i=0; !se_priv_equal(&privs[i].se_priv, &se_priv_end); i++ ) {
		if ( strequal( privs[i].name, name ) ) {
			se_priv_copy( mask, &privs[i].se_priv );
			return True;
		}
	}

	return False;
}

/***************************************************************************
 dump an SE_PRIV structure to the log files
****************************************************************************/

void dump_se_priv( int dbg_cl, int dbg_lvl, const SE_PRIV *mask )
{
	int i;

	DEBUGADDC( dbg_cl, dbg_lvl,("SE_PRIV "));

	for ( i=0; i<SE_PRIV_MASKSIZE; i++ ) {
		DEBUGADDC( dbg_cl, dbg_lvl,(" 0x%x", mask->mask[i] ));
	}

	DEBUGADDC( dbg_cl, dbg_lvl, ("\n"));
}

/****************************************************************************
 check if the privilege is in the privilege list
****************************************************************************/

bool is_privilege_assigned(const SE_PRIV *privileges,
			   const SE_PRIV *check)
{
	SE_PRIV p1, p2;

	if ( !privileges || !check )
		return False;

	/* everyone has privileges if you aren't checking for any */

	if ( se_priv_empty( check ) ) {
		DEBUG(1,("is_privilege_assigned: no privileges in check_mask!\n"));
		return True;
	}

	se_priv_copy( &p1, check );

	/* invert the SE_PRIV we want to check for and remove that from the
	   original set.  If we are left with the SE_PRIV we are checking
	   for then return True */

	se_priv_invert( &p1, check );
	se_priv_copy( &p2, privileges );
	se_priv_remove( &p2, &p1 );

	return se_priv_equal( &p2, check );
}

/****************************************************************************
 check if the privilege is in the privilege list
****************************************************************************/

static bool is_any_privilege_assigned( SE_PRIV *privileges, const SE_PRIV *check )
{
	SE_PRIV p1, p2;

	if ( !privileges || !check )
		return False;

	/* everyone has privileges if you aren't checking for any */

	if ( se_priv_empty( check ) ) {
		DEBUG(1,("is_any_privilege_assigned: no privileges in check_mask!\n"));
		return True;
	}

	se_priv_copy( &p1, check );

	/* invert the SE_PRIV we want to check for and remove that from the
	   original set.  If we are left with the SE_PRIV we are checking
	   for then return True */

	se_priv_invert( &p1, check );
	se_priv_copy( &p2, privileges );
	se_priv_remove( &p2, &p1 );

	/* see if we have any bits left */

	return !se_priv_empty( &p2 );
}

/*********************************************************************
 Generate the LUID_ATTR structure based on a bitmask
*********************************************************************/

const char* get_privilege_dispname( const char *name )
{
	int i;

	if (!name) {
		return NULL;
	}

	for ( i=0; !se_priv_equal(&privs[i].se_priv, &se_priv_end); i++ ) {

		if ( strequal( privs[i].name, name ) ) {
			return privs[i].description;
		}
	}

	return NULL;
}

/****************************************************************************
 initialise a privilege list and set the talloc context
 ****************************************************************************/

/****************************************************************************
 Does the user have the specified privilege ?  We only deal with one privilege
 at a time here.
*****************************************************************************/

bool user_has_privileges(const NT_USER_TOKEN *token, const SE_PRIV *privilege)
{
	if ( !token )
		return False;

	return is_privilege_assigned( &token->privileges, privilege );
}

/****************************************************************************
 Does the user have any of the specified privileges ?  We only deal with one privilege
 at a time here.
*****************************************************************************/

bool user_has_any_privilege(NT_USER_TOKEN *token, const SE_PRIV *privilege)
{
	if ( !token )
		return False;

	return is_any_privilege_assigned( &token->privileges, privilege );
}

/*******************************************************************
 return the number of elements in the privlege array
*******************************************************************/

int count_all_privileges( void )
{
	/*
	 * The -1 is due to the weird SE_END record...
	 */
	return (sizeof(privs) / sizeof(privs[0])) - 1;
}


/*********************************************************************
 Generate the LUID_ATTR structure based on a bitmask
 The assumption here is that the privilege has already been validated
 so we are guaranteed to find it in the list.
*********************************************************************/

LUID_ATTR get_privilege_luid( SE_PRIV *mask )
{
	LUID_ATTR priv_luid;
	int i;

	ZERO_STRUCT( priv_luid );

	for ( i=0; !se_priv_equal(&privs[i].se_priv, &se_priv_end); i++ ) {

		if ( se_priv_equal( &privs[i].se_priv, mask ) ) {
			priv_luid.luid = privs[i].luid;
			break;
		}
	}

	return priv_luid;
}

/****************************************************************************
 Convert a LUID to a named string
****************************************************************************/

const char *luid_to_privilege_name(const LUID *set)
{
	int i;

	for ( i=0; !se_priv_equal(&privs[i].se_priv, &se_priv_end); i++ ) {
		if (luid_equal(set, &privs[i].luid)) {
			return privs[i].name;
		}
	}

	return NULL;
}


/****************************************************************************
 add a privilege to a privilege array
 ****************************************************************************/

static bool privilege_set_add(PRIVILEGE_SET *priv_set, LUID_ATTR set)
{
	LUID_ATTR *new_set;

	/* we can allocate memory to add the new privilege */

	new_set = TALLOC_REALLOC_ARRAY(priv_set->mem_ctx, priv_set->set, LUID_ATTR, priv_set->count + 1);
	if ( !new_set ) {
		DEBUG(0,("privilege_set_add: failed to allocate memory!\n"));
		return False;
	}

	new_set[priv_set->count].luid.high = set.luid.high;
	new_set[priv_set->count].luid.low = set.luid.low;
	new_set[priv_set->count].attr = set.attr;

	priv_set->count++;
	priv_set->set = new_set;

	return True;
}

/*******************************************************************
*******************************************************************/

bool se_priv_to_privilege_set( PRIVILEGE_SET *set, SE_PRIV *mask )
{
	int i;
	uint32 num_privs = count_all_privileges();
	LUID_ATTR luid;

	luid.attr = 0;
	luid.luid.high = 0;

	for ( i=0; i<num_privs; i++ ) {
		if ( !is_privilege_assigned(mask, &privs[i].se_priv) )
			continue;

		luid.luid = privs[i].luid;

		if ( !privilege_set_add( set, luid ) )
			return False;
	}

	return True;
}

/*******************************************************************
*******************************************************************/

static bool luid_to_se_priv( struct lsa_LUID *luid, SE_PRIV *mask )
{
	int i;
	uint32 num_privs = count_all_privileges();
	LUID local_luid;

	local_luid.low = luid->low;
	local_luid.high = luid->high;

	for ( i=0; i<num_privs; i++ ) {
		if (luid_equal(&local_luid, &privs[i].luid)) {
			se_priv_copy( mask, &privs[i].se_priv );
			return True;
		}
	}

	return False;
}

/*******************************************************************
*******************************************************************/

bool privilege_set_to_se_priv( SE_PRIV *mask, struct lsa_PrivilegeSet *privset )
{
	int i;

	ZERO_STRUCTP( mask );

	for ( i=0; i<privset->count; i++ ) {
		SE_PRIV r;

		if ( luid_to_se_priv( &privset->set[i].luid, &r ) )
			se_priv_add( mask, &r );
	}

	return True;
}

