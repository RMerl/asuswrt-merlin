/* Test out of order operations with {set,get,end}grent */

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

#include <stdio.h>
#include <grp.h>

int main (int argc, char **argv)
{
	struct group *gr;
	int found = 0;
	int num_users, i;

	/* Test getgrent() without setgrent() */

	for (i = 0; i < 100; i++) {
		gr = getgrent();

		/* This is supposed to work */

#if 0
		if (gr != NULL) {
			printf("FAIL: getgrent() with no setgrent()\n");
			return 1;
		}
#endif
	}

	/* Work out how many user till first domain group */

	num_users = 0;
	setgrent();

	while (1) {
		gr = getgrent();
		num_users++;

		if (gr == NULL) break;

		if (strchr(gr->gr_name, '/')) {
			found = 1;
			break;
		}

	}

	if (!found) {
		printf("FAIL: could not find any domain groups\n");
		return 1;
	}

	/* Test stopping getgrent in the middle of a set of users */

	endgrent();

	/* Test setgrent() without any getgrent() calls */

	setgrent();

	for (i = 0; i < (num_users - 1); i++) {
		getgrent();
	}

	endgrent();

	/* Test lots of setgrent() calls */

	for (i = 0; i < 100; i++) {
		setgrent();
	}

	/* Test lots of endgrent() calls */

	for (i = 0; i < 100; i++) {
		endgrent();
	}

	/* Everything's cool */

	printf("PASS\n");
	return 0;
}
