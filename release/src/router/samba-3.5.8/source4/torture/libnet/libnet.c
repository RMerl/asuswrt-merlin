/* 
   Unix SMB/CIFS implementation.
   SMB torture tester
   Copyright (C) Jelmer Vernooij 2006
   
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
#include "torture/smbtorture.h"
#include "librpc/rpc/dcerpc.h"
#include "librpc/gen_ndr/security.h"
#include "librpc/gen_ndr/lsa.h"
#include "libnet/composite.h"
#include "torture/libnet/proto.h"

NTSTATUS torture_net_init(void)
{
	struct torture_suite *suite = torture_suite_create(talloc_autofree_context(), "NET");

	torture_suite_add_simple_test(suite, "USERINFO", torture_userinfo);
	torture_suite_add_simple_test(suite, "USERADD", torture_useradd);
	torture_suite_add_simple_test(suite, "USERDEL", torture_userdel);
	torture_suite_add_simple_test(suite, "USERMOD", torture_usermod);
	torture_suite_add_simple_test(suite, "DOMOPEN", torture_domainopen);
	torture_suite_add_simple_test(suite, "GROUPINFO", torture_groupinfo);
	torture_suite_add_simple_test(suite, "GROUPADD", torture_groupadd);
	torture_suite_add_simple_test(suite, "API-LOOKUP", torture_lookup);
	torture_suite_add_simple_test(suite, "API-LOOKUPHOST", torture_lookup_host);
	torture_suite_add_simple_test(suite, "API-LOOKUPPDC", torture_lookup_pdc);
	torture_suite_add_simple_test(suite, "API-LOOKUPNAME", torture_lookup_sam_name);
	torture_suite_add_simple_test(suite, "API-CREATEUSER", torture_createuser);
	torture_suite_add_simple_test(suite, "API-DELETEUSER", torture_deleteuser);
	torture_suite_add_simple_test(suite, "API-MODIFYUSER", torture_modifyuser);
	torture_suite_add_simple_test(suite, "API-USERINFO", torture_userinfo_api);
	torture_suite_add_simple_test(suite, "API-USERLIST", torture_userlist);
	torture_suite_add_simple_test(suite, "API-GROUPINFO", torture_groupinfo_api);
	torture_suite_add_simple_test(suite, "API-GROUPLIST", torture_grouplist);
	torture_suite_add_simple_test(suite, "API-CREATEGROUP", torture_creategroup);
	torture_suite_add_simple_test(suite, "API-RPCCONN-BIND", torture_rpc_connect_binding);
	torture_suite_add_simple_test(suite, "API-RPCCONN-SRV", torture_rpc_connect_srv);
	torture_suite_add_simple_test(suite, "API-RPCCONN-PDC", torture_rpc_connect_pdc);
	torture_suite_add_simple_test(suite, "API-RPCCONN-DC", torture_rpc_connect_dc);
	torture_suite_add_simple_test(suite, "API-RPCCONN-DCINFO", torture_rpc_connect_dc_info);
	torture_suite_add_simple_test(suite, "API-LISTSHARES", torture_listshares);
	torture_suite_add_simple_test(suite, "API-DELSHARE", torture_delshare);
	torture_suite_add_simple_test(suite, "API-DOMOPENLSA", torture_domain_open_lsa);
	torture_suite_add_simple_test(suite, "API-DOMCLOSELSA", torture_domain_close_lsa);
	torture_suite_add_simple_test(suite, "API-DOMOPENSAMR", torture_domain_open_samr);
	torture_suite_add_simple_test(suite, "API-DOMCLOSESAMR", torture_domain_close_samr);
	torture_suite_add_simple_test(suite, "API-BECOME-DC", torture_net_become_dc);
	torture_suite_add_simple_test(suite, "API-DOMLIST", torture_domain_list);

	suite->description = talloc_strdup(suite, "libnet convenience interface tests");

	torture_register_suite(suite);

	return NT_STATUS_OK;
}
