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
	int i;

	init_sec_ctx();

	/* Check for underflow */

	if (!push_sec_ctx()) {
		printf("FAIL: push_sec_ctx\n");
		return 1;
	}

	set_sec_ctx(1, 1, 0, NULL);

	if (!pop_sec_ctx()) {
		printf("FAIL: pop_sec_ctx\n");
		return 1;
	}

	if (pop_sec_ctx()) {
		printf("FAIL: underflow push_sec_ctx\n");
		return 1;
	}

	/* Check for overflow */

	for (i = 0; i < MAX_SEC_CTX_DEPTH + 1; i++) {
		BOOL result;

		result = push_sec_ctx();
		set_sec_ctx(i, i, 0, NULL);

		if ((i < MAX_SEC_CTX_DEPTH) && !result) {
			printf("FAIL: push_sec_ctx(%d)\n", i);
			return 1;
		}

		if ((i == MAX_SEC_CTX_DEPTH + 1) && result) {
			printf("FAIL: overflow push_sec_ctx(%d)\n", i);
			return 1;
		}
	}

	/* Everything's cool */

	printf("PASS\n");
	return 0;
}
