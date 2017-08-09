/*
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Andrew Tridgell 		1992-1998
   Copyright (C) Luke Kenneth Caseson Leighton 	1998-1999
   Copyright (C) Jeremy Allison  		1999
   Copyright (C) Stefan (metze) Metzmacher 	2002
   Copyright (C) Simo Sorce 			2002
   Copyright (C) Jim McDonough <jmcd@us.ibm.com> 2005
   Copyright (C) Andrew Bartlett                2010

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
#include "../librpc/gen_ndr/ndr_security.h"
#include "../librpc/gen_ndr/netlogon.h"
#include "../libcli/security/security.h"

/*
 * Some useful sids, more well known sids can be found at
 * http://support.microsoft.com/kb/243330/EN-US/
 */


const struct dom_sid global_sid_World_Domain =               /* Everyone domain */
{ 1, 0, {0,0,0,0,0,1}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
const struct dom_sid global_sid_World =                      /* Everyone */
{ 1, 1, {0,0,0,0,0,1}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
const struct dom_sid global_sid_Creator_Owner_Domain =       /* Creator Owner domain */
{ 1, 0, {0,0,0,0,0,3}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
const struct dom_sid global_sid_NT_Authority =    		/* NT Authority */
{ 1, 0, {0,0,0,0,0,5}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
const struct dom_sid global_sid_System =			/* System */
{ 1, 1, {0,0,0,0,0,5}, {18,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
const struct dom_sid global_sid_NULL =            		/* NULL sid */
{ 1, 1, {0,0,0,0,0,0}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
const struct dom_sid global_sid_Authenticated_Users =	/* All authenticated rids */
{ 1, 1, {0,0,0,0,0,5}, {11,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
#if 0
/* for documentation */
const struct dom_sid global_sid_Restriced =			/* Restriced Code */
{ 1, 1, {0,0,0,0,0,5}, {12,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
#endif
const struct dom_sid global_sid_Network =			/* Network rids */
{ 1, 1, {0,0,0,0,0,5}, {2,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};

const struct dom_sid global_sid_Creator_Owner =		/* Creator Owner */
{ 1, 1, {0,0,0,0,0,3}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
const struct dom_sid global_sid_Creator_Group =		/* Creator Group */
{ 1, 1, {0,0,0,0,0,3}, {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
const struct dom_sid global_sid_Anonymous =			/* Anonymous login */
{ 1, 1, {0,0,0,0,0,5}, {7,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
const struct dom_sid global_sid_Enterprise_DCs =		/* Enterprise DCs */
{ 1, 1, {0,0,0,0,0,5}, {9,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
const struct dom_sid global_sid_Builtin = 			/* Local well-known domain */
{ 1, 1, {0,0,0,0,0,5}, {32,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
const struct dom_sid global_sid_Builtin_Administrators =	/* Builtin administrators */
{ 1, 2, {0,0,0,0,0,5}, {32,544,0,0,0,0,0,0,0,0,0,0,0,0,0}};
const struct dom_sid global_sid_Builtin_Users =		/* Builtin users */
{ 1, 2, {0,0,0,0,0,5}, {32,545,0,0,0,0,0,0,0,0,0,0,0,0,0}};
const struct dom_sid global_sid_Builtin_Guests =		/* Builtin guest users */
{ 1, 2, {0,0,0,0,0,5}, {32,546,0,0,0,0,0,0,0,0,0,0,0,0,0}};
const struct dom_sid global_sid_Builtin_Power_Users =	/* Builtin power users */
{ 1, 2, {0,0,0,0,0,5}, {32,547,0,0,0,0,0,0,0,0,0,0,0,0,0}};
const struct dom_sid global_sid_Builtin_Account_Operators =	/* Builtin account operators */
{ 1, 2, {0,0,0,0,0,5}, {32,548,0,0,0,0,0,0,0,0,0,0,0,0,0}};
const struct dom_sid global_sid_Builtin_Server_Operators =	/* Builtin server operators */
{ 1, 2, {0,0,0,0,0,5}, {32,549,0,0,0,0,0,0,0,0,0,0,0,0,0}};
const struct dom_sid global_sid_Builtin_Print_Operators =	/* Builtin print operators */
{ 1, 2, {0,0,0,0,0,5}, {32,550,0,0,0,0,0,0,0,0,0,0,0,0,0}};
const struct dom_sid global_sid_Builtin_Backup_Operators =	/* Builtin backup operators */
{ 1, 2, {0,0,0,0,0,5}, {32,551,0,0,0,0,0,0,0,0,0,0,0,0,0}};
const struct dom_sid global_sid_Builtin_Replicator =		/* Builtin replicator */
{ 1, 2, {0,0,0,0,0,5}, {32,552,0,0,0,0,0,0,0,0,0,0,0,0,0}};
const struct dom_sid global_sid_Builtin_PreWin2kAccess =	/* Builtin pre win2k access */
{ 1, 2, {0,0,0,0,0,5}, {32,554,0,0,0,0,0,0,0,0,0,0,0,0,0}};

const struct dom_sid global_sid_Unix_Users =			/* Unmapped Unix users */
{ 1, 1, {0,0,0,0,0,22}, {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
const struct dom_sid global_sid_Unix_Groups =			/* Unmapped Unix groups */
{ 1, 1, {0,0,0,0,0,22}, {2,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};

/* Unused, left here for documentary purposes */
#if 0
#define SECURITY_NULL_SID_AUTHORITY    0
#define SECURITY_WORLD_SID_AUTHORITY   1
#define SECURITY_LOCAL_SID_AUTHORITY   2
#define SECURITY_CREATOR_SID_AUTHORITY 3
#define SECURITY_NT_AUTHORITY          5
#endif

static struct dom_sid system_sid_array[1] =
{ { 1, 1, {0,0,0,0,0,5}, {18,0,0,0,0,0,0,0,0,0,0,0,0,0,0}} };
static const struct security_token system_token = {
	.num_sids       = ARRAY_SIZE(system_sid_array),
	.sids           = system_sid_array,
	.privilege_mask = SE_ALL_PRIVS
};

/****************************************************************************
 Lookup string names for SID types.
****************************************************************************/

static const struct {
	enum lsa_SidType sid_type;
	const char *string;
} sid_name_type[] = {
	{SID_NAME_USE_NONE, "None"},
	{SID_NAME_USER, "User"},
	{SID_NAME_DOM_GRP, "Domain Group"},
	{SID_NAME_DOMAIN, "Domain"},
	{SID_NAME_ALIAS, "Local Group"},
	{SID_NAME_WKN_GRP, "Well-known Group"},
	{SID_NAME_DELETED, "Deleted Account"},
	{SID_NAME_INVALID, "Invalid Account"},
	{SID_NAME_UNKNOWN, "UNKNOWN"},
	{SID_NAME_COMPUTER, "Computer"}
};

const char *sid_type_lookup(uint32_t sid_type)
{
	int i;

	/* Look through list */
	for (i=0; i < ARRAY_SIZE(sid_name_type); i++) {
		if (sid_name_type[i].sid_type == sid_type) {
			return sid_name_type[i].string;
		}
	}

	/* Default return */
	return "SID *TYPE* is INVALID";
}

/**************************************************************************
 Create the SYSTEM token.
***************************************************************************/

const struct security_token *get_system_token(void)
{
	return &system_token;
}

bool sid_compose(struct dom_sid *dst, const struct dom_sid *domain_sid, uint32_t rid)
{
	sid_copy(dst, domain_sid);
	return sid_append_rid(dst, rid);
}

/*****************************************************************
 Removes the last rid from the end of a sid
*****************************************************************/

bool sid_split_rid(struct dom_sid *sid, uint32_t *rid)
{
	if (sid->num_auths > 0) {
		sid->num_auths--;
		if (rid != NULL) {
			*rid = sid->sub_auths[sid->num_auths];
		}
		return true;
	}
	return false;
}

/*****************************************************************
 Return the last rid from the end of a sid
*****************************************************************/

bool sid_peek_rid(const struct dom_sid *sid, uint32_t *rid)
{
	if (!sid || !rid)
		return false;

	if (sid->num_auths > 0) {
		*rid = sid->sub_auths[sid->num_auths - 1];
		return true;
	}
	return false;
}

/*****************************************************************
 Return the last rid from the end of a sid
 and check the sid against the exp_dom_sid
*****************************************************************/

bool sid_peek_check_rid(const struct dom_sid *exp_dom_sid, const struct dom_sid *sid, uint32_t *rid)
{
	if (!exp_dom_sid || !sid || !rid)
		return false;

	if (sid->num_auths != (exp_dom_sid->num_auths+1)) {
		return false;
	}

	if (sid_compare_domain(exp_dom_sid, sid)!=0){
		*rid=(-1);
		return false;
	}

	return sid_peek_rid(sid, rid);
}

/*****************************************************************
 Copies a sid
*****************************************************************/

void sid_copy(struct dom_sid *dst, const struct dom_sid *src)
{
	int i;

	ZERO_STRUCTP(dst);

	dst->sid_rev_num = src->sid_rev_num;
	dst->num_auths = src->num_auths;

	memcpy(&dst->id_auth[0], &src->id_auth[0], sizeof(src->id_auth));

	for (i = 0; i < src->num_auths; i++)
		dst->sub_auths[i] = src->sub_auths[i];
}

/*****************************************************************
 Parse a on-the-wire SID (in a DATA_BLOB) to a struct dom_sid.
*****************************************************************/

bool sid_blob_parse(DATA_BLOB in, struct dom_sid *sid)
{
	enum ndr_err_code ndr_err;
	ndr_err = ndr_pull_struct_blob_all(&in, NULL, sid,
					   (ndr_pull_flags_fn_t)ndr_pull_dom_sid);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return false;
	}
	return true;
}

/*****************************************************************
 Parse a on-the-wire SID to a struct dom_sid.
*****************************************************************/

bool sid_parse(const char *inbuf, size_t len, struct dom_sid *sid)
{
	DATA_BLOB in = data_blob_const(inbuf, len);
	return sid_blob_parse(in, sid);
}

/*****************************************************************
 See if 2 SIDs are in the same domain
 this just compares the leading sub-auths
*****************************************************************/

int sid_compare_domain(const struct dom_sid *sid1, const struct dom_sid *sid2)
{
	int n, i;

	n = MIN(sid1->num_auths, sid2->num_auths);

	for (i = n-1; i >= 0; --i)
		if (sid1->sub_auths[i] != sid2->sub_auths[i])
			return sid1->sub_auths[i] - sid2->sub_auths[i];

	return dom_sid_compare_auth(sid1, sid2);
}

/*****************************************************************
 Compare two sids.
*****************************************************************/

bool sid_equal(const struct dom_sid *sid1, const struct dom_sid *sid2)
{
	return dom_sid_compare(sid1, sid2) == 0;
}

/********************************************************************
 Add SID to an array SIDs
********************************************************************/

NTSTATUS add_sid_to_array(TALLOC_CTX *mem_ctx, const struct dom_sid *sid,
			  struct dom_sid **sids, uint32_t *num)
{
	*sids = talloc_realloc(mem_ctx, *sids, struct dom_sid,
			       (*num)+1);
	if (*sids == NULL) {
		*num = 0;
		return NT_STATUS_NO_MEMORY;
	}

	sid_copy(&((*sids)[*num]), sid);
	*num += 1;

	return NT_STATUS_OK;
}


/********************************************************************
 Add SID to an array SIDs ensuring that it is not already there
********************************************************************/

NTSTATUS add_sid_to_array_unique(TALLOC_CTX *mem_ctx, const struct dom_sid *sid,
				 struct dom_sid **sids, uint32_t *num_sids)
{
	uint32_t i;

	for (i=0; i<(*num_sids); i++) {
		if (dom_sid_compare(sid, &(*sids)[i]) == 0)
			return NT_STATUS_OK;
	}

	return add_sid_to_array(mem_ctx, sid, sids, num_sids);
}

/********************************************************************
 Remove SID from an array
********************************************************************/

void del_sid_from_array(const struct dom_sid *sid, struct dom_sid **sids,
			uint32_t *num)
{
	struct dom_sid *sid_list = *sids;
	uint32_t i;

	for ( i=0; i<*num; i++ ) {

		/* if we find the SID, then decrement the count
		   and break out of the loop */

		if ( sid_equal(sid, &sid_list[i]) ) {
			*num -= 1;
			break;
		}
	}

	/* This loop will copy the remainder of the array
	   if i < num of sids ni the array */

	for ( ; i<*num; i++ )
		sid_copy( &sid_list[i], &sid_list[i+1] );

	return;
}

bool add_rid_to_array_unique(TALLOC_CTX *mem_ctx,
			     uint32_t rid, uint32_t **pp_rids, size_t *p_num)
{
	size_t i;

	for (i=0; i<*p_num; i++) {
		if ((*pp_rids)[i] == rid)
			return true;
	}

	*pp_rids = talloc_realloc(mem_ctx, *pp_rids, uint32_t, *p_num+1);

	if (*pp_rids == NULL) {
		*p_num = 0;
		return false;
	}

	(*pp_rids)[*p_num] = rid;
	*p_num += 1;
	return true;
}

bool is_null_sid(const struct dom_sid *sid)
{
	static const struct dom_sid null_sid = {0};
	return sid_equal(sid, &null_sid);
}
