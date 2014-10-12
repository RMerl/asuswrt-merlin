/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Security context tests
   Copyright (C) Tim Potter 2000
   
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
#include "sec_ctx_utils.h"

#define NUM_TESTS 10000

int main (int argc, char **argv)
{
	int seed, level = 0, num_tests = 0;

	init_sec_ctx();

	if (argc == 1) {
		seed = time(NULL);
	} else {
		seed = atoi(argv[1]);
	}

	printf("seed = %d\n", seed);

	while(num_tests < NUM_TESTS) {
		switch (random() % 2) {

			/* Push a random context */

		case 0:
			if (level < MAX_SEC_CTX_DEPTH) {
				int ngroups;
				gid_t *groups;

				if (!push_sec_ctx()) {
					printf("FAIL: push random ctx\n");
					return 1;
				}

				get_random_grouplist(&ngroups, &groups);

				set_sec_ctx(random() % 32767, 
					    random() % 32767, 
					    ngroups, groups);

				if (!verify_current_groups(ngroups, 
							   groups)) {
					printf("FAIL: groups did not stick\n");
					return 1;
				}

				printf("pushed (%d, %d) eff=(%d, %d)\n", 
				       getuid(), getgid(), geteuid(), 
				       getegid());

				level++;
				num_tests++;

				free(groups);
			}
			break;

			/* Pop a random context */

		case 1:
			if (level > 0) {
				if (!pop_sec_ctx()) {
					printf("FAIL: pop random ctx\n");
					return 1;
				}

				printf("popped (%d, %d) eff=(%d, %d)\n", 
				       getuid(), getgid(), geteuid(), 
				       getegid());

				level--;
				num_tests++;			
			}
			break;
		}
	}

	/* Everything's cool */

	printf("PASS\n");
	return 0;
}
