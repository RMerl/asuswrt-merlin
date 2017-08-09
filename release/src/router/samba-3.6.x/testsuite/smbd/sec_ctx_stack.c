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

int main (int argc, char **argv)
{
	BOOL result;
	int i;

	init_sec_ctx();

	/* Push a whole bunch of security contexts */

	for (i = 0; i < MAX_SEC_CTX_DEPTH; i++) {

		result = push_sec_ctx();
		set_sec_ctx(i + 1, i + 2, 0, NULL);

		if (!result) {
			printf("FAIL: push_sec_ctx(%d)\n", i);
			return 1;
		}

		printf("pushed context (%d, %d) eff=(%d, %d)\n", 
		       getuid(), getgid(), geteuid(), getegid());

		if ((geteuid() != i + 1) || (getegid() != i + 2)) {
			printf("FAIL: incorrect context pushed\n");
			return 1;
		}
	}

	/* Pop them all off */

	for (i = MAX_SEC_CTX_DEPTH; i > 0; i--) {

		result = pop_sec_ctx();

		if (!result) {
			printf("FAIL: pop_sec_ctx(%d)\n", i);
			return 1;
		}

		printf("popped context (%d, %d) eff=(%d, %d)\n", 
		       getuid(), getgid(), geteuid(), getegid());

		printf("i = %d\n",i);

		if (i > 1) {
			if ((geteuid() != i - 1) || (getegid() != i)) {
				printf("FAIL: incorrect context popped\n");
				return 1;
			}
		} else {
			if ((geteuid() != 0) || (getegid() != 0)) {
				printf("FAIL: incorrect context popped\n");
				return 1;
			}
		}
	}

	/* Everything's cool */

	printf("PASS\n");
	return 0;
}
