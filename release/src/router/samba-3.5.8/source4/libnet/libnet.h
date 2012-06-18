/* 
   Unix SMB/CIFS implementation.
   
   Copyright (C) Stefan Metzmacher      2004
   Copyright (C) Rafal Szczesniak       2005-2006
   
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

struct libnet_context {
	/* here we need:
	 * a client env context
	 * a user env context
	 */
	struct cli_credentials *cred;

	/* samr connection parameters - opened handles and related properties */
	struct {
		struct dcerpc_pipe *pipe;
		const char *name;
		struct dom_sid *sid;
		uint32_t access_mask;
		struct policy_handle handle;
		struct policy_handle connect_handle;
		int buf_size;
	} samr;

	/* lsa connection parameters - opened handles and related properties */
	struct {
		struct dcerpc_pipe *pipe;
		const char *name;
		uint32_t access_mask;
		struct policy_handle handle;
	} lsa;

	/* name resolution methods */
	struct resolve_context *resolve_ctx;

	struct tevent_context *event_ctx;

	struct loadparm_context *lp_ctx;
};


#include "lib/ldb/include/ldb.h"
#include "libnet/composite.h"
#include "libnet/userman.h"
#include "libnet/userinfo.h"
#include "libnet/groupinfo.h"
#include "libnet/groupman.h"
#include "libnet/libnet_passwd.h"
#include "libnet/libnet_time.h"
#include "libnet/libnet_rpc.h"
#include "libnet/libnet_join.h"
#include "libnet/libnet_site.h"
#include "libnet/libnet_become_dc.h"
#include "libnet/libnet_unbecome_dc.h"
#include "libnet/libnet_samsync.h"
#include "libnet/libnet_vampire.h"
#include "libnet/libnet_user.h"
#include "libnet/libnet_group.h"
#include "libnet/libnet_share.h"
#include "libnet/libnet_lookup.h"
#include "libnet/libnet_domain.h"
#include "libnet/libnet_export_keytab.h"
#include "libnet/libnet_proto.h"
