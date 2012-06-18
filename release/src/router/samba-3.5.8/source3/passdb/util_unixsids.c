/* 
   Unix SMB/CIFS implementation.
   Translate unix-defined names to SIDs and vice versa
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

bool sid_check_is_unix_users(const DOM_SID *sid)
{
	return sid_equal(sid, &global_sid_Unix_Users);
}

bool sid_check_is_in_unix_users(const DOM_SID *sid)
{
	DOM_SID dom_sid;
	uint32 rid;

	sid_copy(&dom_sid, sid);
	sid_split_rid(&dom_sid, &rid);

	return sid_check_is_unix_users(&dom_sid);
}

bool uid_to_unix_users_sid(uid_t uid, DOM_SID *sid)
{
	sid_copy(sid, &global_sid_Unix_Users);
	return sid_append_rid(sid, (uint32_t)uid);
}

bool gid_to_unix_groups_sid(gid_t gid, DOM_SID *sid)
{
	sid_copy(sid, &global_sid_Unix_Groups);
	return sid_append_rid(sid, (uint32_t)gid);
}

const char *unix_users_domain_name(void)
{
	return "Unix User";
}

bool lookup_unix_user_name(const char *name, DOM_SID *sid)
{
	struct passwd *pwd;

	pwd = Get_Pwnam_alloc(talloc_autofree_context(), name);
	if (pwd == NULL) {
		return False;
	}

	sid_copy(sid, &global_sid_Unix_Users);
	sid_append_rid(sid, (uint32_t)pwd->pw_uid); /* For 64-bit uid's we have enough
					  * space ... */
	TALLOC_FREE(pwd);
	return True;
}

bool sid_check_is_unix_groups(const DOM_SID *sid)
{
	return sid_equal(sid, &global_sid_Unix_Groups);
}

bool sid_check_is_in_unix_groups(const DOM_SID *sid)
{
	DOM_SID dom_sid;
	uint32 rid;

	sid_copy(&dom_sid, sid);
	sid_split_rid(&dom_sid, &rid);

	return sid_check_is_unix_groups(&dom_sid);
}

const char *unix_groups_domain_name(void)
{
	return "Unix Group";
}

bool lookup_unix_group_name(const char *name, DOM_SID *sid)
{
	struct group *grp;

	grp = sys_getgrnam(name);
	if (grp == NULL) {
		return False;
	}

	sid_copy(sid, &global_sid_Unix_Groups);
	sid_append_rid(sid, (uint32_t)grp->gr_gid); /* For 64-bit uid's we have enough
					   * space ... */
	return True;
}
