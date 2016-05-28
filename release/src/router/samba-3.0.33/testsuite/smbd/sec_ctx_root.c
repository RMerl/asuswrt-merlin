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

int main (int argc, char **argv)
{
	int ngroups, actual_ngroups;
	gid_t *groups, *actual_groups;
	extern struct current_user current_user;

	init_sec_ctx();

	/* Initialise a security context */

	get_random_grouplist(&ngroups, &groups);
	set_sec_ctx(1, 1, ngroups, groups);

	/* Become root and check */

	set_root_sec_ctx();

	actual_ngroups = getgroups(0, NULL);
	actual_groups = (gid_t *)malloc(actual_ngroups * sizeof(gid_t));

	getgroups(actual_ngroups, actual_groups);

	if (geteuid() != 0 || getegid() != 0 || actual_ngroups != 0) {
		printf("FAIL: root id not set\n");
		return 1;
	}

	if (current_user.uid != 0 || current_user.gid != 0 ||
	    current_user.ngroups != 0 || current_user.groups) {
		printf("FAIL: current_user not set correctly\n");
		return 1;
	}

	printf("PASS\n");

	return 0;
}
