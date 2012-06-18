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
#include <stdlib.h>
#include <pwd.h>
#include <sys/types.h>

#include "longarg_utils.h"

int main(void)
{
	struct passwd *pwd;
	char *domain = getenv("TEST_WORKGROUP");
	char long_name[65535];
	int failed = 0;

	sprintf(long_name, "%s/%s", domain, LONG_STRING);
		
	pwd = getpwnam(long_name);
	printf("%s\n", !pwd ? "PASS" : "FAIL");

	return pwd == NULL;
}
