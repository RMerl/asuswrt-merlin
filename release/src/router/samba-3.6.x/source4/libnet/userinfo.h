/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Rafal Szczesniak 2005
   
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


#include "librpc/gen_ndr/samr.h"

/*
 * IO structures for userinfo.c functions
 */

struct libnet_rpc_userinfo {
	struct {
		struct policy_handle domain_handle;
		const char *username;
		const char *sid;
		uint16_t level;
	} in;
	struct {
		union samr_UserInfo info;
	} out;
};


/*
 * Monitor messages sent from userinfo.c functions
 */

struct msg_rpc_open_user {
	uint32_t rid, access_mask;
};

struct msg_rpc_query_user {
	uint16_t level;
};

struct msg_rpc_close_user {
	uint32_t rid;
};
