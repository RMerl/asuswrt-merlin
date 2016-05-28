/* Test out of order operations with {set,get,end}pwent */

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

#include <stdio.h>
#include <pwd.h>

int main (int argc, char **argv)
{
	struct passwd *pw;
	int found = 0;
	int num_users, i;

	/* Test getpwent() without setpwent() */

	for (i = 0; i < 100; i++) {
		pw = getpwent();

		/* This is supposed to work */

#if 0
		if (pw != NULL) {
			printf("FAIL: getpwent() with no setpwent()\n");
			return 1;
		}
#endif
	}

	/* Work out how many user till first domain user */

	num_users = 0;
	setpwent();

	while (1) {
		pw = getpwent();
		num_users++;

		if (pw == NULL) break;

		if (strchr(pw->pw_name, '/')) {
			found = 1;
			break;
		}

	}

	if (!found) {
		printf("FAIL: could not find any domain users\n");
		return 1;
	}

	/* Test stopping getpwent in the middle of a set of users */

	endpwent();

	/* Test setpwent() without any getpwent() calls */

	setpwent();

	for (i = 0; i < (num_users - 1); i++) {
		getpwent();
	}

	endpwent();

	/* Test lots of setpwent() calls */
	
	setpwent();

	for (i = 0; i < (num_users - 1); i++) {
		getpwent();
	}

	for (i = 0; i < 100; i++) {
		setpwent();
	}

	/* Test lots of endpwent() calls */

	setpwent();

	for (i = 0; i < (num_users - 1); i++) {
		getpwent();
	}

	for (i = 0; i < 100; i++) {
		endpwent();
	}

	/* Everything's cool */

	printf("PASS\n");
	return 0;
}
