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

/* Keep linker happy */

void exit_server(char *reason) {}

/* Generate random list of groups */

void get_random_grouplist(int *ngroups, gid_t **groups)
{
	int i;

	*ngroups = random() % groups_max();
	*groups = malloc(*ngroups * sizeof(gid_t));

	if (!groups) {
		printf("FAIL: malloc random grouplist\n");
		return;
	}

	for (i = 0; i < *ngroups; i++) {
		(*groups)[i] = random() % 32767;
	}
}

/* Check a list of groups with current groups */

BOOL verify_current_groups(int ngroups, gid_t *groups)
{
	int actual_ngroups;
	gid_t *actual_groups;

	actual_ngroups = getgroups(0, NULL);
	actual_groups = (gid_t *)malloc(actual_ngroups * sizeof(gid_t));

	getgroups(actual_ngroups, actual_groups);

	if (actual_ngroups != ngroups) {
		return False;
	}

	return memcmp(actual_groups, groups, actual_ngroups *
		      sizeof(gid_t)) == 0;
}
