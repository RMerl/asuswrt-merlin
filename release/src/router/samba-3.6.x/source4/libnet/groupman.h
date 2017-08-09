/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Rafal Szczesniak 2007
   
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

#include "librpc/gen_ndr/misc.h"


/*
 * IO structures for groupman.c functions
 */

struct libnet_rpc_groupadd {
	struct {
		struct policy_handle domain_handle;
		const char *groupname;
	} in;
	struct {
		struct policy_handle group_handle;
	} out;
};


struct libnet_rpc_groupdel {
	struct {
		struct policy_handle domain_handle;
		const char *groupname;
	} in;
	struct {
		struct policy_handle group_handle;
	} out;
};
