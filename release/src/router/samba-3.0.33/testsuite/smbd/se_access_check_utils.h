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

#ifndef _SE_ACCESS_CHECK_UTILS_H
#define _SE_ACCESS_CHECK_UTILS_H

#include "includes.h"

/* Structure to build ACE lists from */

struct ace_entry {
	uint8 type, flags;
	uint32 mask;
	char *sid;
};

#define NULL_SID  "S-1-0-0"
#define WORLD_SID "S-1-1-0"

/* Function prototypes */

SEC_ACL *build_acl(struct ace_entry *ace_list);
SEC_DESC *build_sec_desc(struct ace_entry *dacl, struct ace_entry *sacl, 
			 char *owner_sid, char *group_sid);

void visit_pwdb(BOOL (*fn)(struct passwd *pw, int ngroups, gid_t *groups));

#endif
