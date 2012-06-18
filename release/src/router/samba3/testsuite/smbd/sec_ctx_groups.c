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
	int ngroups, initial_ngroups, check_ngroups, final_ngroups;
	gid_t *groups, *initial_groups, *check_groups, *final_groups;
	int i;

	init_sec_ctx();

	/* Save current groups */

	initial_ngroups = sys_getgroups(0, NULL);
	initial_groups = malloc(sizeof(gid_t) * initial_ngroups);
	sys_getgroups(initial_ngroups, initial_groups);

	printf("Initial groups are: ");
	for (i = 0; i < initial_ngroups; i++) {
		printf("%d, ", initial_groups[i]);
	}
	printf("\n");

	/* Push a context plus groups */

	get_random_grouplist(&ngroups, &groups);

	printf("Random groups are: ");
	for (i = 0; i < ngroups; i++) {
		printf("%d, ", groups[i]);
	}
	printf("\n");

	if (!push_sec_ctx()) {
		printf("FAIL: push_sec_ctx\n");
		return 1;
	}

	set_sec_ctx(1, 2, ngroups, groups);

	/* Check grouplist stuck */

	check_ngroups = sys_getgroups(0, NULL);
	check_groups = malloc(sizeof(gid_t) * check_ngroups);
	sys_getgroups(check_ngroups, check_groups);

	printf("Actual groups are: ");
	for (i = 0; i < check_ngroups; i++) {
		printf("%d, ", check_groups[i]);
	}
	printf("\n");

	if (ngroups != check_ngroups) {
		printf("FAIL: number of groups differs\n");
		return 1;
	}

	for (i = 0; i < ngroups; i++) {
		if (groups[i] != check_groups[i]) {
			printf("FAIL: group %d differs\n", i);
			return 1;
		}
	}

	safe_free(groups);
	safe_free(check_groups);

	/* Pop and check initial groups are back */

	if (!pop_sec_ctx()) {
		printf("FAIL: pop_sec_ctx\n");
		return 1;
	}

	final_ngroups = sys_getgroups(0, NULL);
	final_groups = malloc(sizeof(gid_t) * final_ngroups);
	sys_getgroups(final_ngroups, final_groups);
	
	printf("Final groups are: ");
	for (i = 0; i < final_ngroups; i++) {
		printf("%d, ", final_groups[i]);
	}
	printf("\n");

	if (initial_ngroups != final_ngroups) {
		printf("FAIL: final number of groups differ\n");
		return 1;
	}

	for (i = 0; i < initial_ngroups; i++) {
		if (initial_groups[i] != final_groups[i]) {
			printf("FAIL: final group %d differs\n", i);
			return 1;
		}
	}

	printf("Final groups are: ");
	for (i = 0; i < final_ngroups; i++) {
		printf("%d, ", final_groups[i]);
	}
	printf("\n");

	safe_free(initial_groups);
	safe_free(final_groups);

	/* Everything's cool */

	printf("PASS\n");
	return 0;
}
