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


#include "includes.h"
#include "lib/privileges.h"
#include "dbwrap.h"
#include "libcli/security/privileges_private.h"
#include "../libcli/security/security.h"
#include "passdb.h"

#define PRIVPREFIX              "PRIV_"

typedef struct {
	uint32_t count;
	struct dom_sid *list;
} SID_LIST;

typedef struct {
	TALLOC_CTX *mem_ctx;
	uint64_t privilege;
	SID_LIST sids;
} PRIV_SID_LIST;

/*
  interpret an old style SE_PRIV structure
 */
static uint64_t map_old_SE_PRIV(unsigned char *dptr)
{
	uint32_t *old_masks = (uint32_t *)dptr;
	/*
	 * the old privileges code only ever used up to 0x800, except
	 * for a special case of 'SE_ALL_PRIVS' which was 0xFFFFFFFF
	 */
	if (old_masks[0] == 0xFFFFFFFF) {
		/* they set all privileges */
		return SE_ALL_PRIVS;
	}

	/* the old code used the machine byte order, but we don't know
	 * the byte order of the machine that wrote it. However we can
	 * tell what byte order it was by taking advantage of the fact
	 * that it only ever use up to 0x800
	 */
	if (dptr[0] || dptr[1]) {
		/* it was little endian */
		return IVAL(dptr, 0);
	}

	/* it was either zero or big-endian */
	return RIVAL(dptr, 0);
}


static bool get_privileges( const struct dom_sid *sid, uint64_t *mask )
{
	struct db_context *db = get_account_pol_db();
	fstring tmp, keystr;
	TDB_DATA data;

	/* Fail if the admin has not enable privileges */

	if ( !lp_enable_privileges() ) {
		return False;
	}

	if ( db == NULL )
		return False;

	/* PRIV_<SID> (NULL terminated) as the key */

	fstr_sprintf(keystr, "%s%s", PRIVPREFIX, sid_to_fstring(tmp, sid));

	data = dbwrap_fetch_bystring( db, talloc_tos(), keystr );

	if ( !data.dptr ) {
		DEBUG(4, ("get_privileges: No privileges assigned to SID "
			  "[%s]\n", sid_string_dbg(sid)));
		return False;
	}

	if (data.dsize == 4*4) {
		/* it's an old style SE_PRIV structure. */
		*mask = map_old_SE_PRIV(data.dptr);
	} else {
		if (data.dsize != sizeof( uint64_t ) ) {
			DEBUG(3, ("get_privileges: Invalid privileges record assigned to SID "
				  "[%s]\n", sid_string_dbg(sid)));
			return False;
		}

		*mask = BVAL(data.dptr, 0);
	}

	TALLOC_FREE(data.dptr);

	return True;
}

/***************************************************************************
 Store the privilege mask (set) for a given SID
****************************************************************************/

static bool set_privileges( const struct dom_sid *sid, uint64_t mask )
{
	struct db_context *db = get_account_pol_db();
	uint8_t privbuf[8];
	fstring tmp, keystr;
	TDB_DATA data;

	if ( !lp_enable_privileges() )
		return False;

	if ( db == NULL )
		return False;

	if ( !sid || (sid->num_auths == 0) ) {
		DEBUG(0,("set_privileges: Refusing to store empty SID!\n"));
		return False;
	}

	/* PRIV_<SID> (NULL terminated) as the key */

	fstr_sprintf(keystr, "%s%s", PRIVPREFIX, sid_to_fstring(tmp, sid));

	/* This writes the 64 bit bitmask out in little endian format */
	SBVAL(privbuf,0,mask);

	data.dptr  = privbuf;
	data.dsize = sizeof(privbuf);

	return NT_STATUS_IS_OK(dbwrap_store_bystring(db, keystr, data,
						     TDB_REPLACE));
}

/*********************************************************************
 get a list of all privileges for all sids in the list
*********************************************************************/

bool get_privileges_for_sids(uint64_t *privileges, struct dom_sid *slist, int scount)
{
	uint64_t mask;
	int i;
	bool found = False;

	*privileges = 0;

	for ( i=0; i<scount; i++ ) {
		/* don't add unless we actually have a privilege assigned */

		if ( !get_privileges( &slist[i], &mask ) )
			continue;

		DEBUG(5,("get_privileges_for_sids: sid = %s\nPrivilege "
			 "set: 0x%llx\n", sid_string_dbg(&slist[i]),
			 (unsigned long long)mask));

		*privileges |= mask;
		found = True;
	}

	return found;
}

NTSTATUS get_privileges_for_sid_as_set(TALLOC_CTX *mem_ctx, PRIVILEGE_SET **privileges, struct dom_sid *sid)
{
	uint64_t mask;
	if (!get_privileges(sid, &mask)) {
		return NT_STATUS_OBJECT_NAME_NOT_FOUND;
	}

	*privileges = talloc_zero(mem_ctx, PRIVILEGE_SET);
	if (!*privileges) {
		return NT_STATUS_NO_MEMORY;
	}

	if (!se_priv_to_privilege_set(*privileges, mask)) {
		return NT_STATUS_NO_MEMORY;
	}
	return NT_STATUS_OK;
}

/*********************************************************************
 traversal functions for privilege_enumerate_accounts
*********************************************************************/

static int priv_traverse_fn(struct db_record *rec, void *state)
{
	PRIV_SID_LIST *priv = (PRIV_SID_LIST *)state;
	int  prefixlen = strlen(PRIVPREFIX);
	struct dom_sid sid;
	fstring sid_string;

	/* check we have a PRIV_+SID entry */

	if ( strncmp((char *)rec->key.dptr, PRIVPREFIX, prefixlen) != 0)
		return 0;

	/* check to see if we are looking for a particular privilege */

	fstrcpy( sid_string, (char *)&(rec->key.dptr[strlen(PRIVPREFIX)]) );

	if (priv->privilege != 0) {
		uint64_t mask;

		if (rec->value.dsize == 4*4) {
			mask = map_old_SE_PRIV(rec->value.dptr);
		} else {
			if (rec->value.dsize != sizeof( uint64_t ) ) {
				DEBUG(3, ("get_privileges: Invalid privileges record assigned to SID "
					  "[%s]\n", sid_string));
				return 0;
			}
			mask = BVAL(rec->value.dptr, 0);
		}

		/* if the SID does not have the specified privilege
		   then just return */

		if ((mask & priv->privilege) == 0) {
			return 0;
		}
	}

	/* this is a last ditch safety check to preventing returning
	   and invalid SID (i've somehow run into this on development branches) */

	if ( strcmp( "S-0-0", sid_string ) == 0 )
		return 0;

	if ( !string_to_sid(&sid, sid_string) ) {
		DEBUG(0,("travsersal_fn_enum__acct: Could not convert SID [%s]\n",
			sid_string));
		return 0;
	}

	if (!NT_STATUS_IS_OK(add_sid_to_array(priv->mem_ctx, &sid,
					      &priv->sids.list,
					      &priv->sids.count)))
	{
		return 0;
	}

	return 0;
}

/*********************************************************************
 Retreive list of privileged SIDs (for _lsa_enumerate_accounts()
*********************************************************************/

NTSTATUS privilege_enumerate_accounts(struct dom_sid **sids, int *num_sids)
{
	struct db_context *db = get_account_pol_db();
	PRIV_SID_LIST priv;

	if (db == NULL) {
		return NT_STATUS_ACCESS_DENIED;
	}

	ZERO_STRUCT(priv);

	db->traverse_read(db, priv_traverse_fn, &priv);

	/* give the memory away; caller will free */

	*sids      = priv.sids.list;
	*num_sids  = priv.sids.count;

	return NT_STATUS_OK;
}

/*********************************************************************
 Retrieve list of SIDs granted a particular privilege
*********************************************************************/

NTSTATUS privilege_enum_sids(enum sec_privilege privilege, TALLOC_CTX *mem_ctx,
			     struct dom_sid **sids, int *num_sids)
{
	struct db_context *db = get_account_pol_db();
	PRIV_SID_LIST priv;

	if (db == NULL) {
		return NT_STATUS_ACCESS_DENIED;
	}

	ZERO_STRUCT(priv);

	priv.privilege = sec_privilege_mask(privilege);
	priv.mem_ctx = mem_ctx;

	db->traverse_read(db, priv_traverse_fn, &priv);

	/* give the memory away; caller will free */

	*sids      = priv.sids.list;
	*num_sids  = priv.sids.count;

	return NT_STATUS_OK;
}

/***************************************************************************
 Add privilege to sid
****************************************************************************/

static bool grant_privilege_bitmap(const struct dom_sid *sid, const uint64_t priv_mask)
{
	uint64_t old_mask, new_mask;

	ZERO_STRUCT( old_mask );
	ZERO_STRUCT( new_mask );

	if ( get_privileges( sid, &old_mask ) )
		new_mask = old_mask;
	else
		new_mask = 0;

	new_mask |= priv_mask;

	DEBUG(10,("grant_privilege: %s\n", sid_string_dbg(sid)));

	DEBUGADD( 10, ("original privilege mask: 0x%llx\n", (unsigned long long)new_mask));

	DEBUGADD( 10, ("new privilege mask:      0x%llx\n", (unsigned long long)new_mask));

	return set_privileges( sid, new_mask );
}

/*********************************************************************
 Add a privilege based on its name
*********************************************************************/

bool grant_privilege_by_name(const struct dom_sid *sid, const char *name)
{
	uint64_t mask;

	if (! se_priv_from_name(name, &mask)) {
        	DEBUG(3, ("grant_privilege_by_name: "
			  "No Such Privilege Found (%s)\n", name));
		return False;
	}

	return grant_privilege_bitmap( sid, mask );
}

/***************************************************************************
 Grant a privilege set (list of LUID values) from a sid
****************************************************************************/

bool grant_privilege_set(const struct dom_sid *sid, struct lsa_PrivilegeSet *set)
{
	uint64_t privilege_mask;
	if (!privilege_set_to_se_priv(&privilege_mask, set)) {
		return false;
	}
	return grant_privilege_bitmap(sid, privilege_mask);
}

/***************************************************************************
 Remove privilege from sid
****************************************************************************/

static bool revoke_privilege_bitmap(const struct dom_sid *sid, const uint64_t priv_mask)
{
	uint64_t mask;

	/* if the user has no privileges, then we can't revoke any */

	if ( !get_privileges( sid, &mask ) )
		return True;

	DEBUG(10,("revoke_privilege: %s\n", sid_string_dbg(sid)));

	DEBUGADD( 10, ("original privilege mask: 0x%llx\n", (unsigned long long)mask));

	mask &= ~priv_mask;

	DEBUGADD( 10, ("new privilege mask:      0x%llx\n", (unsigned long long)mask));

	return set_privileges( sid, mask );
}

/***************************************************************************
 Remove a privilege set (list of LUID values) from a sid
****************************************************************************/

bool revoke_privilege_set(const struct dom_sid *sid, struct lsa_PrivilegeSet *set)
{
	uint64_t privilege_mask;
	if (!privilege_set_to_se_priv(&privilege_mask, set)) {
		return false;
	}
	return revoke_privilege_bitmap(sid, privilege_mask);
}

/*********************************************************************
 Revoke all privileges
*********************************************************************/

bool revoke_all_privileges( const struct dom_sid *sid )
{
	return revoke_privilege_bitmap( sid, SE_ALL_PRIVS);
}

/*********************************************************************
 Add a privilege based on its name
*********************************************************************/

bool revoke_privilege_by_name(const struct dom_sid *sid, const char *name)
{
	uint64_t mask;

	if (! se_priv_from_name(name, &mask)) {
        	DEBUG(3, ("revoke_privilege_by_name: "
			  "No Such Privilege Found (%s)\n", name));
        	return False;
	}

	return revoke_privilege_bitmap(sid, mask);

}

/***************************************************************************
 Retrieve the SIDs assigned to a given privilege
****************************************************************************/

NTSTATUS privilege_create_account(const struct dom_sid *sid )
{
	return ( grant_privilege_bitmap(sid, 0) ? NT_STATUS_OK : NT_STATUS_UNSUCCESSFUL);
}

/***************************************************************************
 Delete a privileged account
****************************************************************************/

NTSTATUS privilege_delete_account(const struct dom_sid *sid)
{
	struct db_context *db = get_account_pol_db();
	fstring tmp, keystr;

	if (!lp_enable_privileges()) {
		return NT_STATUS_OK;
	}

	if (!db) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (!sid || (sid->num_auths == 0)) {
		return NT_STATUS_INVALID_SID;
	}

	/* PRIV_<SID> (NULL terminated) as the key */

	fstr_sprintf(keystr, "%s%s", PRIVPREFIX, sid_to_fstring(tmp, sid));

	return dbwrap_delete_bystring(db, keystr);
}

/*******************************************************************
*******************************************************************/

bool is_privileged_sid( const struct dom_sid *sid )
{
	uint64_t mask;

	return get_privileges( sid, &mask );
}

/*******************************************************************
*******************************************************************/

bool grant_all_privileges( const struct dom_sid *sid )
{
	uint64_t mask;

	se_priv_put_all_privileges(&mask);

	return grant_privilege_bitmap( sid, mask );
}
