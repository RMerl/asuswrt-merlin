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
	init_sec_ctx();
	
	/* Become a non-root user */

	setuid(1);
	setgid(1);

	/* Try to push a security context.  This should fail with a
	   smb_assert() error. */

	push_sec_ctx();
	set_sec_ctx(2, 2, 0, NULL);
	printf("FAIL\n");

	return 0;
}
