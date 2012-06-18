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

void char_to_sid(DOM_SID *sid, char *sid_str)
{
	/* If it looks like a SID, call string_to_sid() else look it up
	   using wbinfo. */

	if (strncmp(sid_str, "S-", 2) == 0) {
		string_to_sid(sid, sid_str);
	} else {
		struct winbindd_request request;
		struct winbindd_response response;

		/* Send off request */

		ZERO_STRUCT(request);
		ZERO_STRUCT(response);
		
		fstrcpy(request.data.name, sid_str);
		if (winbindd_request(WINBINDD_LOOKUPNAME, &request, 
				     &response) != NSS_STATUS_SUCCESS) {
			printf("FAIL: unable to look up sid for name %s\n",
			       sid_str);
			exit(1);
		}

		string_to_sid(sid, response.data.sid.sid);
		printf("converted char %s to sid %s\n", sid_str, 
		       response.data.sid.sid);
	}	
}

/* Construct an ACL from a list of ace_entry structures */

SEC_ACL *build_acl(struct ace_entry *ace_list)
{
	SEC_ACE *aces = NULL;
	SEC_ACL *result;
	int num_aces = 0;

	if (ace_list == NULL) return NULL;

	/* Create aces */

	while(ace_list->sid) {
		SEC_ACCESS sa;
		DOM_SID sid;

		/* Create memory for new ACE */

		if (!(aces = Realloc(aces, 
				     sizeof(SEC_ACE) * (num_aces + 1)))) {
			return NULL;
		}

		/* Create ace */

		init_sec_access(&sa, ace_list->mask);

		char_to_sid(&sid, ace_list->sid);
		init_sec_ace(&aces[num_aces], &sid, ace_list->type,
			     sa, ace_list->flags);

		num_aces++;
		ace_list++;
	}

	/* Create ACL from list of ACEs */

	result = make_sec_acl(ACL_REVISION, num_aces, aces);
	free(aces);

	return result;
}

/* Make a security descriptor */

SEC_DESC *build_sec_desc(struct ace_entry *dacl, struct ace_entry *sacl, 
			 char *owner_sid, char *group_sid)
{
	DOM_SID the_owner_sid, the_group_sid;
	SEC_ACL *the_dacl, *the_sacl;
	SEC_DESC *result;
	size_t size;

	/* Build up bits of security descriptor */

	char_to_sid(&the_owner_sid, owner_sid);
	char_to_sid(&the_group_sid, group_sid);

	the_dacl = build_acl(dacl);
	the_sacl = build_acl(sacl);

	result =  make_sec_desc(SEC_DESC_REVISION, 
				SEC_DESC_SELF_RELATIVE | SEC_DESC_DACL_PRESENT,
				&the_owner_sid, &the_group_sid, 
				the_sacl, the_dacl, &size);

	free_sec_acl(&the_dacl);
	free_sec_acl(&the_sacl);

	return result;
}

/* Iterate over password database and call a user-specified function */

void visit_pwdb(BOOL (*fn)(struct passwd *pw, int ngroups, gid_t *groups))
{
	struct passwd *pw;
	int ngroups;
	gid_t *groups;

	setpwent();

	while ((pw = getpwent())) {
		BOOL result;

		/* Get grouplist */

		ngroups = getgroups(0, NULL);

		groups = malloc(sizeof(gid_t) * ngroups);
		getgroups(ngroups, groups);

		/* Call function */

		result = fn(pw, ngroups, groups);
		if (!result) break;

		/* Clean up */

		free(groups);
	}

	endpwent();
}
