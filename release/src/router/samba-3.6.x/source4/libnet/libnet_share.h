/* 
   Unix SMB/CIFS implementation.
   
   Copyright (C) Grégory LEOCADIE <gleocadie@idealx.com> 
   
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

#include "librpc/gen_ndr/srvsvc.h"

enum libnet_ListShares_level {
	LIBNET_LIST_SHARES_GENERIC,
	LIBNET_LIST_SHARES_SRVSVC
};

struct libnet_ListShares {
	struct {
		const char *server_name;
		uint32_t *resume_handle;
		uint32_t level;	
	} in;
	struct {
		const char *error_string;
		union srvsvc_NetShareCtr ctr;
		uint32_t *resume_handle;
	} out;
};

enum libnet_AddShare_level {
	LIBNET_ADD_SHARE_GENERIC,
	LIBNET_ADD_SHARE_SRVSVC
};

struct libnet_AddShare {
	enum libnet_AddShare_level level;
	struct {
		const char * server_name;
		struct srvsvc_NetShareInfo2 share;	
	} in;
	struct {
		const char* error_string;
	} out;
};

enum libnet_DelShare_level {
	LIBNET_DEL_SHARE_GENERIC,
	LIBNET_DEL_SHARE_SRVSVC
};

struct libnet_DelShare {
	enum libnet_DelShare_level level;
	struct {
		const char *server_name;
		const char *share_name;
	} in;
	struct {
		const char *error_string;
	} out;
};
