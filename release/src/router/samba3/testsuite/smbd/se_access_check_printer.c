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

struct ace_entry acl_printer[] = {

	/* Everyone is allowed to print */

	{ SEC_ACE_TYPE_ACCESS_ALLOWED, SEC_ACE_FLAG_CONTAINER_INHERIT,
	  PRINTER_ACE_PRINT, "S-1-1-0" },

	/* Except for user0 who uses too much paper */

	{ SEC_ACE_TYPE_ACCESS_DENIED, SEC_ACE_FLAG_CONTAINER_INHERIT,
	  PRINTER_ACE_FULL_CONTROL, "user0" },

	/* Users 1 and 2 can manage documents */

	{ SEC_ACE_TYPE_ACCESS_ALLOWED, SEC_ACE_FLAG_CONTAINER_INHERIT,
	  PRINTER_ACE_MANAGE_DOCUMENTS, "user1" },

	{ SEC_ACE_TYPE_ACCESS_ALLOWED, SEC_ACE_FLAG_CONTAINER_INHERIT,
	  PRINTER_ACE_MANAGE_DOCUMENTS, "user2" },

	/* Domain Admins can also manage documents */

	{ SEC_ACE_TYPE_ACCESS_ALLOWED, SEC_ACE_FLAG_CONTAINER_INHERIT,
	  PRINTER_ACE_MANAGE_DOCUMENTS, "Domain Admins" },

	/* User 3 is da man */

	{ SEC_ACE_TYPE_ACCESS_ALLOWED, SEC_ACE_FLAG_CONTAINER_INHERIT,
	  PRINTER_ACE_FULL_CONTROL, "user3" },

	{ 0, 0, 0, NULL}
};

BOOL test_user(char *username, uint32 acc_desired, uint32 *acc_granted)
{
	struct passwd *pw;
	uint32 status;

	if (!(pw = getpwnam(username))) {
		printf("FAIL: could not lookup user info for %s\n",
		       username);
		exit(1);
	}

	return se_access_check(sd, pw->pw_uid, pw->pw_gid, 0, NULL,
			       acc_desired, acc_granted, &status);
}

static char *pace_str(uint32 ace_flags)
{
	if ((ace_flags & PRINTER_ACE_FULL_CONTROL) == 
	    PRINTER_ACE_FULL_CONTROL) return "full control";

	if ((ace_flags & PRINTER_ACE_MANAGE_DOCUMENTS) ==
	    PRINTER_ACE_MANAGE_DOCUMENTS) return "manage documents";

	if ((ace_flags & PRINTER_ACE_PRINT) == PRINTER_ACE_PRINT)
		return "print";

	return "UNKNOWN";
}

uint32 perms[] = {
	PRINTER_ACE_PRINT,
	PRINTER_ACE_FULL_CONTROL,
	PRINTER_ACE_MANAGE_DOCUMENTS,
	0
};

void runtest(void)
{
	uint32 acc_granted;
	BOOL result;
	int i, j;

	for (i = 0; perms[i]; i++) {

		/* Test 10 users */
		
		for (j = 0; j < 10; j++) {
			fstring name;

			/* Test user against ACL */

			snprintf(name, sizeof(fstring), "%s/user%d", 
				 getenv("TEST_WORKGROUP"), j);
			
			result = test_user(name, perms[i], &acc_granted);

			printf("%s: %s %s 0x%08x\n", name, 
			       pace_str(perms[i]), 
			       result ? "TRUE " : "FALSE", acc_granted);

			/* Check results */

			switch (perms[i]) {

			case PRINTER_ACE_PRINT: {
				if (!result || acc_granted !=
				    PRINTER_ACE_PRINT) {
					printf("FAIL: user %s can't print\n",
					       name);
					failed = True;
				}
				break;
			}

			case PRINTER_ACE_FULL_CONTROL: {
				if (j == 3) {
					if (!result || acc_granted !=
					    PRINTER_ACE_FULL_CONTROL) {
						printf("FAIL: user %s doesn't "
						       "have full control\n",
						       name);
						failed = True;
					}
				} else {
					if (result || acc_granted != 0) {
						printf("FAIL: user %s has full "
						       "control\n", name);
						failed = True;
					}
				}
				break;
			}
			case PRINTER_ACE_MANAGE_DOCUMENTS: {
				if (j == 1 || j == 2) {
					if (!result || acc_granted !=
					    PRINTER_ACE_MANAGE_DOCUMENTS) {
						printf("FAIL: user %s can't "
						       "manage documents\n",
						       name);
						failed = True;
					}
				} else {
					if (result || acc_granted != 0) {
						printf("FAIL: user %s can "
						       "manage documents\n",
						       name);
						failed = True;
					}
				}
				break;
			}

			default:
				printf("FAIL: internal error\n");
				exit(1);
			}
		}
	}
}

/* Main function */

int main(int argc, char **argv)
{
	/* Initialisation */

	generate_wellknown_sids();

	/* Create security descriptor */

	sd = build_sec_desc(acl_printer, NULL, NULL_SID, NULL_SID);

	if (!sd) {
		printf("FAIL: could not build security descriptor\n");
		return 1;
	}

	/* Run test */

	runtest();

	/* Return */

        if (!failed) {
		printf("PASS\n");
		return 0;
	} 

	return 1;
}
