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

#define PRIVPREFIX              "PRIV_"

typedef struct {
	size_t count;
	DOM_SID *list;
} SID_LIST;

typedef struct {
	TALLOC_CTX *mem_ctx;
	SE_PRIV privilege;
	SID_LIST sids;
} PRIV_SID_LIST;


static bool get_privileges( const DOM_SID *sid, SE_PRIV *mask )
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
		DEBUG(3, ("get_privileges: No privileges assigned to SID "
			  "[%s]\n", sid_string_dbg(sid)));
		return False;
	}

	SMB_ASSERT( data.dsize == sizeof( SE_PRIV ) );

	se_priv_copy( mask, (SE_PRIV*)data.dptr );
	TALLOC_FREE(data.dptr);

	return True;
}

/***************************************************************************
 Store the privilege mask (set) for a given SID
****************************************************************************/

static bool set_privileges( const DOM_SID *sid, SE_PRIV *mask )
{
	struct db_context *db = get_account_pol_db();
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

	/* no packing.  static size structure, just write it out */

	data.dptr  = (uint8 *)mask;
	data.dsize = sizeof(SE_PRIV);

	return NT_STATUS_IS_OK(dbwrap_store_bystring(db, keystr, data,
						     TDB_REPLACE));
}

/*********************************************************************
 get a list of all privileges for all sids in the list
*********************************************************************/

bool get_privileges_for_sids(SE_PRIV *privileges, DOM_SID *slist, int scount)
{
	SE_PRIV mask;
	int i;
	bool found = False;

	se_priv_copy( privileges, &se_priv_none );

	for ( i=0; i<scount; i++ ) {
		/* don't add unless we actually have a privilege assigned */

		if ( !get_privileges( &slist[i], &mask ) )
			continue;

		DEBUG(5,("get_privileges_for_sids: sid = %s\nPrivilege "
			 "set:\n", sid_string_dbg(&slist[i])));
		dump_se_priv( DBGC_ALL, 5, &mask );

		se_priv_add( privileges, &mask );
		found = True;
	}

	return found;
}


/*********************************************************************
 traversal functions for privilege_enumerate_accounts
*********************************************************************/

static int priv_traverse_fn(struct db_record *rec, void *state)
{
	PRIV_SID_LIST *priv = (PRIV_SID_LIST *)state;
	int  prefixlen = strlen(PRIVPREFIX);
	DOM_SID sid;
	fstring sid_string;

	/* easy check first */

	if (rec->value.dsize != sizeof(SE_PRIV) )
		return 0;

	/* check we have a PRIV_+SID entry */

	if ( strncmp((char *)rec->key.dptr, PRIVPREFIX, prefixlen) != 0)
		return 0;

	/* check to see if we are looking for a particular privilege */

	if ( !se_priv_equal(&priv->privilege, &se_priv_none) ) {
		SE_PRIV mask;

		se_priv_copy( &mask, (SE_PRIV*)rec->value.dptr );

		/* if the SID does not have the specified privilege
		   then just return */

		if ( !is_privilege_assigned( &mask, &priv->privilege) )
			return 0;
	}

	fstrcpy( sid_string, (char *)&(rec->key.dptr[strlen(PRIVPREFIX)]) );

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

NTSTATUS privilege_enumerate_accounts(DOM_SID **sids, int *num_sids)
{
	struct db_context *db = get_account_pol_db();
	PRIV_SID_LIST priv;

	if (db == NULL) {
		return NT_STATUS_ACCESS_DENIED;
	}

	ZERO_STRUCT(priv);

	se_priv_copy( &priv.privilege, &se_priv_none );

	db->traverse_read(db, priv_traverse_fn, &priv);

	/* give the memory away; caller will free */

	*sids      = priv.sids.list;
	*num_sids  = priv.sids.count;

	return NT_STATUS_OK;
}

/*********************************************************************
 Retrieve list of SIDs granted a particular privilege
*********************************************************************/

NTSTATUS privilege_enum_sids(const SE_PRIV *mask, TALLOC_CTX *mem_ctx,
			     DOM_SID **sids, int *num_sids)
{
	struct db_context *db = get_account_pol_db();
	PRIV_SID_LIST priv;

	if (db == NULL) {
		return NT_STATUS_ACCESS_DENIED;
	}

	ZERO_STRUCT(priv);

	se_priv_copy(&priv.privilege, mask);
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

bool grant_privilege(const DOM_SID *sid, const SE_PRIV *priv_mask)
{
	SE_PRIV old_mask, new_mask;

	ZERO_STRUCT( old_mask );
	ZERO_STRUCT( new_mask );

	if ( get_privileges( sid, &old_mask ) )
		se_priv_copy( &new_mask, &old_mask );
	else
		se_priv_copy( &new_mask, &se_priv_none );

	se_priv_add( &new_mask, priv_mask );

	DEBUG(10,("grant_privilege: %s\n", sid_string_dbg(sid)));

	DEBUGADD( 10, ("original privilege mask:\n"));
	dump_se_priv( DBGC_ALL, 10, &old_mask );

	DEBUGADD( 10, ("new privilege mask:\n"));
	dump_se_priv( DBGC_ALL, 10, &new_mask );

	return set_privileges( sid, &new_mask );
}

/*********************************************************************
 Add a privilege based on its name
*********************************************************************/

bool grant_privilege_by_name(DOM_SID *sid, const char *name)
{
	SE_PRIV mask;

	if (! se_priv_from_name(name, &mask)) {
        	DEBUG(3, ("grant_privilege_by_name: "
			  "No Such Privilege Found (%s)\n", name));
		return False;
	}

	return grant_privilege( sid, &mask );
}

/***************************************************************************
 Remove privilege from sid
****************************************************************************/

bool revoke_privilege(const DOM_SID *sid, const SE_PRIV *priv_mask)
{
	SE_PRIV mask;

	/* if the user has no privileges, then we can't revoke any */

	if ( !get_privileges( sid, &mask ) )
		return True;

	DEBUG(10,("revoke_privilege: %s\n", sid_string_dbg(sid)));

	DEBUGADD( 10, ("original privilege mask:\n"));
	dump_se_priv( DBGC_ALL, 10, &mask );

	se_priv_remove( &mask, priv_mask );

	DEBUGADD( 10, ("new privilege mask:\n"));
	dump_se_priv( DBGC_ALL, 10, &mask );

	return set_privileges( sid, &mask );
}

/*********************************************************************
 Revoke all privileges
*********************************************************************/

bool revoke_all_privileges( DOM_SID *sid )
{
	return revoke_privilege( sid, &se_priv_all );
}

/*********************************************************************
 Add a privilege based on its name
*********************************************************************/

bool revoke_privilege_by_name(DOM_SID *sid, const char *name)
{
	SE_PRIV mask;

	if (! se_priv_from_name(name, &mask)) {
        	DEBUG(3, ("revoke_privilege_by_name: "
			  "No Such Privilege Found (%s)\n", name));
        	return False;
	}

	return revoke_privilege(sid, &mask);

}

/***************************************************************************
 Retrieve the SIDs assigned to a given privilege
****************************************************************************/

NTSTATUS privilege_create_account(const DOM_SID *sid )
{
	return ( grant_privilege(sid, &se_priv_none) ? NT_STATUS_OK : NT_STATUS_UNSUCCESSFUL);
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

/****************************************************************************
 initialise a privilege list and set the talloc context
 ****************************************************************************/

NTSTATUS privilege_set_init(PRIVILEGE_SET *priv_set)
{
	TALLOC_CTX *mem_ctx;

	ZERO_STRUCTP( priv_set );

	mem_ctx = talloc_init("privilege set");
	if ( !mem_ctx ) {
		DEBUG(0,("privilege_set_init: failed to initialize talloc ctx!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	priv_set->mem_ctx = mem_ctx;

	return NT_STATUS_OK;
}

/****************************************************************************
  initialise a privilege list and with someone else's talloc context
****************************************************************************/

NTSTATUS privilege_set_init_by_ctx(TALLOC_CTX *mem_ctx, PRIVILEGE_SET *priv_set)
{
	ZERO_STRUCTP( priv_set );

	priv_set->mem_ctx = mem_ctx;
	priv_set->ext_ctx = True;

	return NT_STATUS_OK;
}

/****************************************************************************
 Free all memory used by a PRIVILEGE_SET
****************************************************************************/

void privilege_set_free(PRIVILEGE_SET *priv_set)
{
	if ( !priv_set )
		return;

	if ( !( priv_set->ext_ctx ) )
		talloc_destroy( priv_set->mem_ctx );

	ZERO_STRUCTP( priv_set );
}

/****************************************************************************
 duplicate alloc luid_attr
 ****************************************************************************/

NTSTATUS dup_luid_attr(TALLOC_CTX *mem_ctx, LUID_ATTR **new_la, LUID_ATTR *old_la, int count)
{
	int i;

	if ( !old_la )
		return NT_STATUS_OK;

	if (count) {
		*new_la = TALLOC_ARRAY(mem_ctx, LUID_ATTR, count);
		if ( !*new_la ) {
			DEBUG(0,("dup_luid_attr: failed to alloc new LUID_ATTR array [%d]\n", count));
			return NT_STATUS_NO_MEMORY;
		}
	} else {
		*new_la = NULL;
	}

	for (i=0; i<count; i++) {
		(*new_la)[i].luid.high = old_la[i].luid.high;
		(*new_la)[i].luid.low = old_la[i].luid.low;
		(*new_la)[i].attr = old_la[i].attr;
	}

	return NT_STATUS_OK;
}

/*******************************************************************
*******************************************************************/

bool is_privileged_sid( const DOM_SID *sid )
{
	SE_PRIV mask;

	return get_privileges( sid, &mask );
}

/*******************************************************************
*******************************************************************/

bool grant_all_privileges( const DOM_SID *sid )
{
	SE_PRIV mask;

	if (!se_priv_put_all_privileges(&mask)) {
		return False;
	}

	return grant_privilege( sid, &mask );
}
