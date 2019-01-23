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
#include "librpc/gen_ndr/lsa.h"
#include "libnet/libnet.h"
#include "torture/libnet/proto.h"

NTSTATUS torture_net_init(void)
{
	struct torture_suite *suite = torture_suite_create(
		talloc_autofree_context(), "net");

	torture_suite_add_simple_test(suite, "userinfo", torture_userinfo);
	torture_suite_add_simple_test(suite, "useradd", torture_useradd);
	torture_suite_add_simple_test(suite, "userdel", torture_userdel);
	torture_suite_add_simple_test(suite, "usermod", torture_usermod);
	torture_suite_add_simple_test(suite, "domopen", torture_domainopen);
	torture_suite_add_simple_test(suite, "groupinfo", torture_groupinfo);
	torture_suite_add_simple_test(suite, "groupadd", torture_groupadd);
	torture_suite_add_simple_test(suite, "api.lookup", torture_lookup);
	torture_suite_add_simple_test(suite, "api.lookuphost", torture_lookup_host);
	torture_suite_add_simple_test(suite, "api.lookuppdc", torture_lookup_pdc);
	torture_suite_add_simple_test(suite, "api.lookupname", torture_lookup_sam_name);
	torture_suite_add_simple_test(suite, "api.createuser", torture_createuser);
	torture_suite_add_simple_test(suite, "api.deleteuser", torture_deleteuser);
	torture_suite_add_simple_test(suite, "api.modifyuser", torture_modifyuser);
	torture_suite_add_simple_test(suite, "api.userinfo", torture_userinfo_api);
	torture_suite_add_simple_test(suite, "api.userlist", torture_userlist);
	torture_suite_add_simple_test(suite, "api.groupinfo", torture_groupinfo_api);
	torture_suite_add_simple_test(suite, "api.grouplist", torture_grouplist);
	torture_suite_add_simple_test(suite, "api.creategroup", torture_creategroup);
	torture_suite_add_simple_test(suite, "api.rpcconn.bind", torture_rpc_connect_binding);
	torture_suite_add_simple_test(suite, "api.rpcconn.srv", torture_rpc_connect_srv);
	torture_suite_add_simple_test(suite, "api.rpcconn.pdc", torture_rpc_connect_pdc);
	torture_suite_add_simple_test(suite, "api.rpcconn.dc", torture_rpc_connect_dc);
	torture_suite_add_simple_test(suite, "api.rpcconn.dcinfo", torture_rpc_connect_dc_info);
	torture_suite_add_simple_test(suite, "api.listshares", torture_listshares);
	torture_suite_add_simple_test(suite, "api.delshare", torture_delshare);
	torture_suite_add_simple_test(suite, "api.domopenlsa", torture_domain_open_lsa);
	torture_suite_add_simple_test(suite, "api.domcloselsa", torture_domain_close_lsa);
	torture_suite_add_simple_test(suite, "api.domopensamr", torture_domain_open_samr);
	torture_suite_add_simple_test(suite, "api.domclosesamr", torture_domain_close_samr);
	torture_suite_add_simple_test(suite, "api.become.dc", torture_net_become_dc);
	torture_suite_add_simple_test(suite, "api.domlist", torture_domain_list);

	suite->description = talloc_strdup(suite, "libnet convenience interface tests");

	torture_register_suite(suite);

	return NT_STATUS_OK;
}
