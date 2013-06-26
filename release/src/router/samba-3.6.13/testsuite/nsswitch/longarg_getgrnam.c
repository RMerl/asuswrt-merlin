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
#include <stdlib.h>
#include <grp.h>
#include <sys/types.h>

#include "longarg_utils.h"

int main(void)
{
	struct group *grp;
	char *domain = getenv("TEST_WORKGROUP");
	char long_name[65535];
	int failed = 0;

	sprintf(long_name, "%s/%s", domain, LONG_STRING);
		
	grp = getgrnam(long_name);
	printf("%s\n", !grp ? "PASS" : "FAIL");

	return grp == NULL;
}
