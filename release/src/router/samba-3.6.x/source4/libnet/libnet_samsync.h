/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
   
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

#include "librpc/gen_ndr/netlogon.h"

struct libnet_SamSync_state {
	struct libnet_context *machine_net_ctx;
	struct dcerpc_pipe *netlogon_pipe;
	const char *domain_name;
	const struct dom_sid *domain_sid;
	const char *realm;
	struct GUID *domain_guid;
};

/* struct and enum for doing a remote domain vampire dump */
struct libnet_SamSync {
	struct {
		const char *binding_string;
		NTSTATUS (*init_fn)(TALLOC_CTX *mem_ctx, 		
				    void *private_data,
				    struct libnet_SamSync_state *samsync_state,
				    char **error_string);
		NTSTATUS (*delta_fn)(TALLOC_CTX *mem_ctx, 		
				     void *private_data,
				     enum netr_SamDatabaseID database,
				     struct netr_DELTA_ENUM *delta,
				     char **error_string);
		void *fn_ctx;
		struct cli_credentials *machine_account;
	} in;
	struct {
		const char *error_string;
	} out;
};

struct libnet_SamDump {
	struct {
		const char *binding_string;
		struct cli_credentials *machine_account;
	} in;
	struct {
		const char *error_string;
	} out;
};

struct libnet_SamDump_keytab {
	struct {
		const char *binding_string;
		const char *keytab_name;
		struct cli_credentials *machine_account;
	} in;
	struct {
		const char *error_string;
	} out;
};

struct libnet_samsync_ldb {
	struct {
		const char *binding_string;
		struct cli_credentials *machine_account;
		struct auth_session_info *session_info;
	} in;
	struct {
		const char *error_string;
	} out;
};

