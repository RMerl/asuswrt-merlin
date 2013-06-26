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
#include "se_access_check_utils.h"

/* Globals */

BOOL failed;
SEC_DESC *sd;

struct ace_entry acl_denyall[] = {
	{ SEC_ACE_TYPE_ACCESS_DENIED, SEC_ACE_FLAG_CONTAINER_INHERIT,
	  GENERIC_ALL_ACCESS, "S-1-1-0" },
	{ 0, 0, 0, NULL}
};

/* Check that access is always allowed for a NULL security descriptor */

BOOL denyall_check(struct passwd *pw, int ngroups, gid_t *groups)
{
	uint32 acc_granted, status;
	BOOL result;

	result = se_access_check(sd, pw->pw_uid, pw->pw_gid,
				 ngroups, groups, 
				 SEC_RIGHTS_MAXIMUM_ALLOWED,
				 &acc_granted, &status);
	
	if (result || acc_granted != 0) {
		printf("FAIL: denyall se_access_check %d/%d\n",
		       pw->pw_uid, pw->pw_gid);
		failed = True;
	}

	return True;
}

/* Main function */

int main(int argc, char **argv)
{
	/* Initialisation */

	generate_wellknown_sids();

	/* Create security descriptor */

	sd = build_sec_desc(acl_denyall, NULL, NULL_SID, NULL_SID);

	if (!sd) {
		printf("FAIL: could not build security descriptor\n");
		return 1;
	}

	/* Run test */

	visit_pwdb(denyall_check);

	/* Return */

        if (!failed) {
		printf("PASS\n");
		return 0;
	} 

	return 1;
}
