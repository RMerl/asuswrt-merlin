/* 
   Unix SMB/CIFS implementation.
   system call wrapper interface.
   Copyright (C) Andrew Tridgell 2002
   Copyright (C) Andrew Barteltt 2002

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
   This file may assume linkage with smbd - for things like become_root()
   etc. 
*/

#include "includes.h"
#include "system/passwd.h"
#include "nsswitch/winbind_client.h"

#ifndef HAVE_GETGROUPLIST

#ifdef HAVE_GETGRSET
static int getgrouplist_getgrset(const char *user, gid_t gid, gid_t *groups,
				 int *grpcnt)
{
	char *grplist;
	char *grp;
	gid_t temp_gid;
	int num_gids = 1;
	int ret = 0;
	long l;

	grplist = getgrset(user);

	DEBUG(10, ("getgrset returned %s\n", grplist));

	if (grplist == NULL) {
		return -1;
	}

	if (*grpcnt > 0) {
		groups[0] = gid;
	}

	while ((grp = strsep(&grplist, ",")) != NULL) {
		l = strtol(grp, NULL, 10);
		temp_gid = (gid_t) l;
		if (temp_gid == gid) {
			continue;
		}

		if (num_gids + 1 > *grpcnt) {
			num_gids++;
			continue;
		}
		groups[num_gids++] = temp_gid;
	}
	free(grplist);

	if (num_gids > *grpcnt) {
		ret = -1;
	}
	*grpcnt = num_gids;

	DEBUG(10, ("Found %d groups for user %s\n", *grpcnt, user));

	return ret;
}

#else /* HAVE_GETGRSET */

/*
  This is a *much* faster way of getting the list of groups for a user
  without changing the current supplementary group list. The old
  method used getgrent() which could take 20 minutes on a really big
  network with hundeds of thousands of groups and users. The new method
  takes a couple of seconds.

  NOTE!! this function only works if it is called as root!
  */

static int getgrouplist_internals(const char *user, gid_t gid, gid_t *groups,
				  int *grpcnt)
{
	gid_t *gids_saved;
	int ret, ngrp_saved, num_gids;

	if (non_root_mode()) {
		*grpcnt = 0;
		return 0;
	}

	/* work out how many groups we need to save */
	ngrp_saved = getgroups(0, NULL);
	if (ngrp_saved == -1) {
		/* this shouldn't happen */
		return -1;
	}

	gids_saved = SMB_MALLOC_ARRAY(gid_t, ngrp_saved+1);
	if (!gids_saved) {
		errno = ENOMEM;
		return -1;
	}

	ngrp_saved = getgroups(ngrp_saved, gids_saved);
	if (ngrp_saved == -1) {
		SAFE_FREE(gids_saved);
		/* very strange! */
		return -1;
	}

	if (initgroups(user, gid) == -1) {
		DEBUG(0, ("getgrouplist_internals: initgroups() failed!\n"));
		SAFE_FREE(gids_saved);
		return -1;
	}

	/* this must be done to cope with systems that put the current egid in the
	   return from getgroups() */
	save_re_gid();
	set_effective_gid(gid);
	setgid(gid);

	num_gids = getgroups(0, NULL);
	if (num_gids == -1) {
		SAFE_FREE(gids_saved);
		/* very strange! */
		return -1;
	}

	if (num_gids + 1 > *grpcnt) {
		*grpcnt = num_gids + 1;
		ret = -1;
	} else {
		ret = getgroups(*grpcnt - 1, &groups[1]);
		if (ret < 0) {
			SAFE_FREE(gids_saved);
			/* very strange! */
			return -1;
		}
		groups[0] = gid;
		*grpcnt = ret + 1;
	}

	restore_re_gid();

	if (sys_setgroups(gid, ngrp_saved, gids_saved) != 0) {
		/* yikes! */
		DEBUG(0,("ERROR: getgrouplist: failed to reset group list!\n"));
		smb_panic("getgrouplist: failed to reset group list!");
	}

	free(gids_saved);
	return ret;
}
#endif /* HAVE_GETGRSET */
#endif /* HAVE_GETGROUPLIST */

static int sys_getgrouplist(const char *user, gid_t gid, gid_t *groups, int *grpcnt)
{
	int retval;
	bool winbind_env;

	DEBUG(10,("sys_getgrouplist: user [%s]\n", user));

	/* This is only ever called for Unix users, remote memberships are
	 * always determined by the info3 coming back from auth3 or the
	 * PAC. */
	winbind_env = winbind_env_set();
	(void)winbind_off();

#ifdef HAVE_GETGROUPLIST
	retval = getgrouplist(user, gid, groups, grpcnt);
#else
#ifdef HAVE_GETGRSET
	retval = getgrouplist_getgrset(user, gid, groups, grpcnt);
#else
	become_root();
	retval = getgrouplist_internals(user, gid, groups, grpcnt);
	unbecome_root();
#endif /* HAVE_GETGRSET */
#endif /* HAVE_GETGROUPLIST */

	/* allow winbindd lookups, but only if they were not already disabled */
	if (!winbind_env) {
		(void)winbind_on();
	}

	return retval;
}

bool getgroups_unix_user(TALLOC_CTX *mem_ctx, const char *user,
			 gid_t primary_gid,
			 gid_t **ret_groups, uint32_t *p_ngroups)
{
	uint32_t ngrp;
	int max_grp;
	gid_t *temp_groups;
	gid_t *groups;
	int i;

	max_grp = MIN(128, groups_max());
	temp_groups = SMB_MALLOC_ARRAY(gid_t, max_grp);
	if (! temp_groups) {
		return False;
	}

	if (sys_getgrouplist(user, primary_gid, temp_groups, &max_grp) == -1) {
		temp_groups = SMB_REALLOC_ARRAY(temp_groups, gid_t, max_grp);
		if (!temp_groups) {
			return False;
		}
		
		if (sys_getgrouplist(user, primary_gid,
				     temp_groups, &max_grp) == -1) {
			DEBUG(0, ("get_user_groups: failed to get the unix "
				  "group list\n"));
			SAFE_FREE(temp_groups);
			return False;
		}
	}
	
	ngrp = 0;
	groups = NULL;

	/* Add in primary group first */
	if (!add_gid_to_array_unique(mem_ctx, primary_gid, &groups, &ngrp)) {
		SAFE_FREE(temp_groups);
		return False;
	}

	for (i=0; i<max_grp; i++) {
		if (!add_gid_to_array_unique(mem_ctx, temp_groups[i],
					&groups, &ngrp)) {
			SAFE_FREE(temp_groups);
			return False;
		}
	}

	*p_ngroups = ngrp;
	*ret_groups = groups;
	SAFE_FREE(temp_groups);
	return True;
}
