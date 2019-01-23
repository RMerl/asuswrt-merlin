/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Rafal Szczesniak  2007
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


struct libnet_CreateGroup {
	struct {
		const char *group_name;
		const char *domain_name;
	} in;
	struct {
		const char *error_string;
	} out;
};

enum libnet_GroupInfo_level {
	GROUP_INFO_BY_NAME=0,
	GROUP_INFO_BY_SID
};

struct libnet_GroupInfo {
	struct {
		const char *domain_name;
		enum libnet_GroupInfo_level level;
		union {
			const char *group_name;
			const struct dom_sid *group_sid;
		} data;
	} in;
	struct {
		const char *group_name;
		struct dom_sid *group_sid;
		uint32_t num_members;
		const char *description;

		const char *error_string;
	} out;
};


struct libnet_GroupList {
	struct {
		const char *domain_name;
		int page_size;
		uint32_t resume_index;
	} in;
	struct {
		int count;
		uint32_t resume_index;
		
		struct grouplist {
			const char *sid;
			const char *groupname;
		} *groups;

		const char *error_string;
	} out;
};
