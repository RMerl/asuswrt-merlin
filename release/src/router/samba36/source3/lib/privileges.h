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

#ifndef _LIB_PRIVILEGES_H_
#define _LIB_PRIVILEGES_H_

#include "../libcli/security/privileges.h"

/* The following definitions come from lib/privileges.c  */

bool get_privileges_for_sids(uint64_t *privileges, struct dom_sid *slist, int scount);
NTSTATUS get_privileges_for_sid_as_set(TALLOC_CTX *mem_ctx, PRIVILEGE_SET **privileges, struct dom_sid *sid);
NTSTATUS privilege_enumerate_accounts(struct dom_sid **sids, int *num_sids);
NTSTATUS privilege_enum_sids(enum sec_privilege privilege, TALLOC_CTX *mem_ctx,
			     struct dom_sid **sids, int *num_sids);
bool grant_privilege_set(const struct dom_sid *sid, struct lsa_PrivilegeSet *set);
bool grant_privilege_by_name( const struct dom_sid *sid, const char *name);
bool revoke_all_privileges( const struct dom_sid *sid );
bool revoke_privilege_set(const struct dom_sid *sid, struct lsa_PrivilegeSet *set);
bool revoke_privilege_by_name(const struct dom_sid *sid, const char *name);
NTSTATUS privilege_create_account(const struct dom_sid *sid );
NTSTATUS privilege_delete_account(const struct dom_sid *sid);
bool is_privileged_sid( const struct dom_sid *sid );
bool grant_all_privileges( const struct dom_sid *sid );

#endif /* _LIB_PRIVILEGES_H_ */
