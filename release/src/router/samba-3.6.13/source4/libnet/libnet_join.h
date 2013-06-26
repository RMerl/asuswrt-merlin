/* 
   Unix SMB/CIFS implementation.
   
   Copyright (C) Stefan Metzmacher	2004
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
   Copyright (C) Brad Henry 2005
   
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

#ifndef __LIBNET_JOIN_H__
#define __LIBNET_JOIN_H__

#include "librpc/gen_ndr/netlogon.h"

enum libnet_Join_level {
	LIBNET_JOIN_AUTOMATIC,
	LIBNET_JOIN_SPECIFIED,
};

enum libnet_JoinDomain_level {
	LIBNET_JOINDOMAIN_AUTOMATIC,
	LIBNET_JOINDOMAIN_SPECIFIED,
};

struct libnet_JoinDomain {
	struct {
		const char *domain_name;
		const char *account_name;
		const char *netbios_name;
		const char *binding;
		enum libnet_JoinDomain_level level;
		uint32_t  acct_type;
		bool recreate_account;
	} in;

	struct {
		const char *error_string;
		const char *join_password;
		struct dom_sid *domain_sid;
		const char *domain_name;
		const char *realm;
		const char *domain_dn_str;
		const char *account_dn_str;
		const char *server_dn_str;
		uint32_t kvno; /* msDS-KeyVersionNumber */
		struct dcerpc_pipe *samr_pipe;
		struct dcerpc_binding *samr_binding;
		struct policy_handle *user_handle;
		struct dom_sid *account_sid;
		struct GUID account_guid;
	} out;
};

struct libnet_Join {
	struct {
		const char *domain_name;
		const char *netbios_name;
		enum netr_SchannelType join_type;
		enum libnet_Join_level level;
	} in;
	
	struct {
		const char *error_string;
		const char *join_password;
		struct dom_sid *domain_sid;
		const char *domain_name;
	} out;
};

struct libnet_set_join_secrets {
	struct {
		const char *domain_name;
		const char *realm;
		const char *netbios_name;
		const char *account_name;
		enum netr_SchannelType join_type;
		const char *join_password;
		int kvno;
		struct dom_sid *domain_sid;
	} in;
	
	struct {
		const char *error_string;
	} out;
};


#endif /* __LIBNET_JOIN_H__ */
