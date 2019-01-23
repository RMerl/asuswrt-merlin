/*
   Unix SMB/CIFS implementation.
   Samba utility functions

   Copyright (C) Stefan (metze) Metzmacher 	2002-2004
   Copyright (C) Andrew Tridgell 		1992-2004
   Copyright (C) Jeremy Allison  		1999

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

#ifndef _DOM_SID_H_
#define _DOM_SID_H_

#include "librpc/gen_ndr/security.h"

/* Some well-known SIDs */
extern const struct dom_sid global_sid_World_Domain;
extern const struct dom_sid global_sid_World;
extern const struct dom_sid global_sid_Creator_Owner_Domain;
extern const struct dom_sid global_sid_NT_Authority;
extern const struct dom_sid global_sid_Enterprise_DCs;
extern const struct dom_sid global_sid_System;
extern const struct dom_sid global_sid_NULL;
extern const struct dom_sid global_sid_Authenticated_Users;
extern const struct dom_sid global_sid_Network;
extern const struct dom_sid global_sid_Creator_Owner;
extern const struct dom_sid global_sid_Creator_Group;
extern const struct dom_sid global_sid_Anonymous;
extern const struct dom_sid global_sid_Builtin;
extern const struct dom_sid global_sid_Builtin_Administrators;
extern const struct dom_sid global_sid_Builtin_Users;
extern const struct dom_sid global_sid_Builtin_Guests;
extern const struct dom_sid global_sid_Builtin_Power_Users;
extern const struct dom_sid global_sid_Builtin_Account_Operators;
extern const struct dom_sid global_sid_Builtin_Server_Operators;
extern const struct dom_sid global_sid_Builtin_Print_Operators;
extern const struct dom_sid global_sid_Builtin_Backup_Operators;
extern const struct dom_sid global_sid_Builtin_Replicator;
extern const struct dom_sid global_sid_Builtin_PreWin2kAccess;
extern const struct dom_sid global_sid_Unix_Users;
extern const struct dom_sid global_sid_Unix_Groups;

int dom_sid_compare_auth(const struct dom_sid *sid1,
			 const struct dom_sid *sid2);
int dom_sid_compare(const struct dom_sid *sid1, const struct dom_sid *sid2);
int dom_sid_compare_domain(const struct dom_sid *sid1,
			   const struct dom_sid *sid2);
bool dom_sid_equal(const struct dom_sid *sid1, const struct dom_sid *sid2);
bool sid_append_rid(struct dom_sid *sid, uint32_t rid);
bool string_to_sid(struct dom_sid *sidout, const char *sidstr);
bool dom_sid_parse_endp(const char *sidstr,struct dom_sid *sidout,
			const char **endp);
bool dom_sid_parse(const char *sidstr, struct dom_sid *ret);
struct dom_sid *dom_sid_parse_talloc(TALLOC_CTX *mem_ctx, const char *sidstr);
struct dom_sid *dom_sid_parse_length(TALLOC_CTX *mem_ctx, const DATA_BLOB *sid);
struct dom_sid *dom_sid_dup(TALLOC_CTX *mem_ctx, const struct dom_sid *dom_sid);
struct dom_sid *dom_sid_add_rid(TALLOC_CTX *mem_ctx,
				const struct dom_sid *domain_sid,
				uint32_t rid);
NTSTATUS dom_sid_split_rid(TALLOC_CTX *mem_ctx, const struct dom_sid *sid,
			   struct dom_sid **domain, uint32_t *rid);
bool dom_sid_in_domain(const struct dom_sid *domain_sid,
		       const struct dom_sid *sid);

#define DOM_SID_STR_BUFLEN (15*11+25)
int dom_sid_string_buf(const struct dom_sid *sid, char *buf, int buflen);
char *dom_sid_string(TALLOC_CTX *mem_ctx, const struct dom_sid *sid);


const char *sid_type_lookup(uint32_t sid_type);
const struct security_token *get_system_token(void);
bool sid_compose(struct dom_sid *dst, const struct dom_sid *domain_sid, uint32_t rid);
bool sid_split_rid(struct dom_sid *sid, uint32_t *rid);
bool sid_peek_rid(const struct dom_sid *sid, uint32_t *rid);
bool sid_peek_check_rid(const struct dom_sid *exp_dom_sid, const struct dom_sid *sid, uint32_t *rid);
void sid_copy(struct dom_sid *dst, const struct dom_sid *src);
bool sid_blob_parse(DATA_BLOB in, struct dom_sid *sid);
bool sid_parse(const char *inbuf, size_t len, struct dom_sid *sid);
int sid_compare_domain(const struct dom_sid *sid1, const struct dom_sid *sid2);
bool sid_equal(const struct dom_sid *sid1, const struct dom_sid *sid2);
NTSTATUS add_sid_to_array(TALLOC_CTX *mem_ctx, const struct dom_sid *sid,
			  struct dom_sid **sids, uint32_t *num);
NTSTATUS add_sid_to_array_unique(TALLOC_CTX *mem_ctx, const struct dom_sid *sid,
				 struct dom_sid **sids, uint32_t *num_sids);
void del_sid_from_array(const struct dom_sid *sid, struct dom_sid **sids,
			uint32_t *num);
bool add_rid_to_array_unique(TALLOC_CTX *mem_ctx,
			     uint32_t rid, uint32_t **pp_rids, size_t *p_num);
bool is_null_sid(const struct dom_sid *sid);

#endif /*_DOM_SID_H_*/

