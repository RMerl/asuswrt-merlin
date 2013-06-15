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
#include "se_access_check_utils.h"

/* Globals */

BOOL failed;
SEC_DESC *sd;

struct ace_entry acl_allowsome[] = {
	{ SEC_ACE_TYPE_ACCESS_ALLOWED, SEC_ACE_FLAG_CONTAINER_INHERIT,
	  GENERIC_ALL_ACCESS, "user0" },
	{ SEC_ACE_TYPE_ACCESS_ALLOWED, SEC_ACE_FLAG_CONTAINER_INHERIT,
	  GENERIC_ALL_ACCESS, "user2" },
	{ 0, 0, 0, NULL}
};

BOOL allowsome_check(struct passwd *pw, int ngroups, gid_t *groups)
{
	uint32 acc_granted, status;
	fstring name;
	BOOL result;
	int len1, len2;

	/* Check only user0 and user2 allowed access */

	result = se_access_check(sd, pw->pw_uid, pw->pw_gid,
				 ngroups, groups, 
				 SEC_RIGHTS_MAXIMUM_ALLOWED,
				 &acc_granted, &status);
	
	len1 = (int)strlen(pw->pw_name) - strlen("user0");
	len2 = (int)strlen(pw->pw_name) - strlen("user2");

	if ((strncmp("user0", &pw->pw_name[MAX(len1, 0)], 
		     strlen("user0")) == 0) ||
	    (strncmp("user2", &pw->pw_name[MAX(len2, 0)], 
		     strlen("user2")) == 0)) {
		if (!result || acc_granted != GENERIC_ALL_ACCESS) {
			printf("FAIL: access not granted for %s\n",
			       pw->pw_name);
		}
	} else {
		if (result || acc_granted != 0) {
			printf("FAIL: access granted for %s\n", pw->pw_name);
		}
	}

	printf("result %s for user %s\n", result ? "allowed" : "denied",
	       pw->pw_name);

	return True;
}

/* Main function */

int main(int argc, char **argv)
{
	/* Initialisation */

	generate_wellknown_sids();

	/* Create security descriptor */

	sd = build_sec_desc(acl_allowsome, NULL, NULL_SID, NULL_SID);

	if (!sd) {
		printf("FAIL: could not build security descriptor\n");
		return 1;
	}

	/* Run test */

	visit_pwdb(allowsome_check);

	/* Return */

        if (!failed) {
		printf("PASS\n");
		return 0;
	} 

	return 1;
}
