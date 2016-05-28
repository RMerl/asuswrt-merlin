/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Security context tests
   Copyright (C) Tim Potter 2000
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"
#include "sec_ctx_utils.h"

int main(int argc, char **argv)
{
	extern struct current_user current_user;
	uid_t initial_uid = current_user.uid;
	gid_t initial_gid = current_user.gid;
	int ngroups;
	gid_t *groups;

	init_sec_ctx();

	/* Check initial id */

	if (initial_uid != 0 || initial_gid != 0) {
		printf("FAIL: current_user not initialised to root\n");
		return 1;
	}

	/* Push a context and check current user is updated */

	if (!push_sec_ctx()) {
		printf("FAIL: push_sec_ctx\n");
		return 1;
	}

	set_sec_ctx(1, 2, 0, NULL);

	if (current_user.uid != 1 || current_user.gid != 2) {
		printf("FAIL: current_user id not updated after push\n");
		return 1;
	}

	if (current_user.ngroups != 0 || current_user.groups) {
		printf("FAIL: current_user groups not updated after push\n");
		return 1;
	}

	/* Push another */

	get_random_grouplist(&ngroups, &groups);

	if (!push_sec_ctx()) {
		printf("FAIL: push_sec_ctx\n");
		return 1;
	}

	set_sec_ctx(2, 3, ngroups, groups);

	if (current_user.uid != 2 || current_user.gid != 3) {
		printf("FAIL: current_user id not updated after second "
		       "push\n");
		return 1;
	}

	if (current_user.ngroups != ngroups || 
	    (memcmp(current_user.groups, groups, 
		    sizeof(gid_t) * ngroups) != 0)) {
		printf("FAIL: current_user groups not updated\n");
		return 1;
	}

	/* Pop them both off */

	if (!pop_sec_ctx()) {
		printf("FAIL: pop_sec_ctx\n");
		return 1;
	}

	if (current_user.uid != 1 || current_user.gid != 2) {
		printf("FAIL: current_user not updaded pop\n");
		return 1;
	}

	if (!pop_sec_ctx()) {
		printf("FAIL: pop_sec_ctx\n");
		return 1;
	}

	/* Check initial state was returned */

	if (current_user.uid != initial_uid || 
	    current_user.gid != initial_gid) {
		printf("FAIL: current_user not updaded pop\n");
		return 1;
	}

	/* Everything's cool */

	printf("PASS\n");
	return 0;
}
