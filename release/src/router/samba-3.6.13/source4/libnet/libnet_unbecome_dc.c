/*
   Unix SMB/CIFS implementation.

   Copyright (C) Stefan Metzmacher <metze@samba.org> 2006

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
#include "libnet/libnet.h"
#include "libcli/composite/composite.h"
#include "libcli/cldap/cldap.h"
#include <ldb.h>
#include <ldb_errors.h>
#include "ldb_wrap.h"
#include "dsdb/samdb/samdb.h"
#include "../libds/common/flags.h"
#include "librpc/gen_ndr/ndr_drsuapi_c.h"
#include "param/param.h"
#include "lib/tsocket/tsocket.h"

/*****************************************************************************
 * Windows 2003 (w2k3) does the following steps when changing the server role
 * from domain controller back to domain member
 *
 * We mostly do the same.
 *****************************************************************************/

/*
 * lookup DC:
 * - using nbt name<1C> request and a samlogon mailslot request
 * or
 * - using a DNS SRV _ldap._tcp.dc._msdcs. request and a CLDAP netlogon request
 *
 * see: unbecomeDC_send_cldap() and unbecomeDC_recv_cldap()
 */

/*
 * Open 1st LDAP connection to the DC using admin credentials
 *
 * see: unbecomeDC_ldap_connect()
 */

/*
 * LDAP search 1st LDAP connection:
 *
 * see: unbecomeDC_ldap_rootdse()
 *
 * Request:
 *	basedn:	""
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:	defaultNamingContext
 *		configurationNamingContext
 * Result:
 *      ""
 *		defaultNamingContext:	<domain_partition>
 *		configurationNamingContext:CN=Configuration,<domain_partition>
 */

/*
 * LDAP search 1st LDAP connection:
 * 
 * see: unbecomeDC_ldap_computer_object()
 *
 * Request:
 *	basedn:	<domain_partition>
 *	scope:	sub
 *	filter:	(&(|(objectClass=user)(objectClass=computer))(sAMAccountName=<new_dc_account_name>))
 *	attrs:	distinguishedName
 *		userAccountControl
 * Result:
 *      CN=<new_dc_netbios_name>,CN=Domain Controllers,<domain_partition>
 *		distinguishedName:	CN=<new_dc_netbios_name>,CN=Domain Controllers,<domain_partition>
 *		userAccoountControl:	532480 <0x82000>
 */

/*
 * LDAP search 1st LDAP connection:
 * 
 * see: unbecomeDC_ldap_modify_computer()
 *
 * Request:
 *	basedn:	CN=<new_dc_netbios_name>,CN=Computers,<domain_partition>
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:	userAccountControl
 * Result:
 *      CN=<new_dc_netbios_name>,CN=Computers,<domain_partition>
 *		userAccoountControl:	532480 <0x82000>
 */

/*
 * LDAP modify 1st LDAP connection:
 *
 * see: unbecomeDC_ldap_modify_computer()
 * 
 * Request (replace):
 *	CN=<new_dc_netbios_name>,CN=Computers,<domain_partition>
 *	userAccoountControl:	4096 <0x1000>
 * Result:
 *	<success>
 */

/*
 * LDAP search 1st LDAP connection:
 * 
 * see: unbecomeDC_ldap_move_computer()
 *
 * Request:
 *	basedn:	<WKGUID=aa312825768811d1aded00c04fd8d5cd,<domain_partition>>
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:	1.1
 * Result:
 *	CN=Computers,<domain_partition>
 */

/*
 * LDAP search 1st LDAP connection:
 *
 * not implemented because it doesn't give any new information
 *
 * Request:
 *	basedn:	CN=Computers,<domain_partition>
 *	scope:	base
 *	filter:	(objectClass=*)
 *	attrs:	distinguishedName
 * Result:
 *	CN=Computers,<domain_partition>
 *		distinguishedName:	CN=Computers,<domain_partition>
 */

/*
 * LDAP modifyRDN 1st LDAP connection:
 * 
 * see: unbecomeDC_ldap_move_computer()
 *
 * Request:
 *      entry:		CN=<new_dc_netbios_name>,CN=Domain Controllers,<domain_partition>
 *	newrdn:		CN=<new_dc_netbios_name>
 *	deleteoldrdn:	TRUE
 *	newparent:	CN=Computers,<domain_partition>
 * Result:
 *	<success>
 */

/*
 * LDAP unbind on the 1st LDAP connection
 *
 * not implemented, because it's not needed...
 */

/*
 * Open 1st DRSUAPI connection to the DC using admin credentials
 * DsBind with DRSUAPI_DS_BIND_GUID ("e24d201a-4fd6-11d1-a3da-0000f875ae0d")
 *
 * see:	unbecomeDC_drsuapi_connect_send(), unbecomeDC_drsuapi_connect_recv(),
 *	unbecomeDC_drsuapi_bind_send() and unbecomeDC_drsuapi_bind_recv()
 */

/*
 * DsRemoveDsServer to remove the 
 * CN=<machine_name>,CN=Servers,CN=<site_name>,CN=Configuration,<domain_partition>
 * and CN=NTDS Settings,CN=<machine_name>,CN=Servers,CN=<site_name>,CN=Configuration,<domain_partition>
 * on the 1st DRSUAPI connection
 *
 * see: unbecomeDC_drsuapi_remove_ds_server_send() and unbecomeDC_drsuapi_remove_ds_server_recv()
 */

/*
 * DsUnbind on the 1st DRSUAPI connection
 *
 * not implemented, because it's not needed...
 */


struct libnet_UnbecomeDC_state {
	struct composite_context *creq;

	struct libnet_context *libnet;

	struct {
		struct cldap_socket *sock;
		struct cldap_netlogon io;
		struct NETLOGON_SAM_LOGON_RESPONSE_EX netlogon;
	} cldap;

	struct {
		struct ldb_context *ldb;
	} ldap;

	struct {
		struct dcerpc_binding *binding;
		struct dcerpc_pipe *pipe;
		struct dcerpc_binding_handle *drsuapi_handle;
		struct drsuapi_DsBind bind_r;
		struct GUID bind_guid;
		struct drsuapi_DsBindInfoCtr bind_info_ctr;
		struct drsuapi_DsBindInfo28 local_info28;
		struct drsuapi_DsBindInfo28 remote_info28;
		struct policy_handle bind_handle;
		struct drsuapi_DsRemoveDSServer rm_ds_srv_r;
	} drsuapi;

	struct {
		/* input */
		const char *dns_name;
		const char *netbios_name;

		/* constructed */
		struct GUID guid;
		const char *dn_str;
	} domain;

	struct {
		/* constructed */
		const char *config_dn_str;
	} forest;

	struct {
		/* input */
		const char *address;

		/* constructed */
		const char *dns_name;
		const char *netbios_name;
		const char *site_name;
	} source_dsa;

	struct {
		/* input */
		const char *netbios_name;

		/* constructed */
		const char *dns_name;
		const char *site_name;
		const char *computer_dn_str;
		const char *server_dn_str;
		uint32_t user_account_control;
	} dest_dsa;
};

static void unbecomeDC_recv_cldap(struct tevent_req *req);

static void unbecomeDC_send_cldap(struct libnet_UnbecomeDC_state *s)
{
	struct composite_context *c = s->creq;
	struct tevent_req *req;
	struct tsocket_address *dest_address;
	int ret;

	s->cldap.io.in.dest_address	= NULL;
	s->cldap.io.in.dest_port	= 0;
	s->cldap.io.in.realm		= s->domain.dns_name;
	s->cldap.io.in.host		= s->dest_dsa.netbios_name;
	s->cldap.io.in.user		= NULL;
	s->cldap.io.in.domain_guid	= NULL;
	s->cldap.io.in.domain_sid	= NULL;
	s->cldap.io.in.acct_control	= -1;
	s->cldap.io.in.version		= NETLOGON_NT_VERSION_5 | NETLOGON_NT_VERSION_5EX;
	s->cldap.io.in.map_response	= true;

	ret = tsocket_address_inet_from_strings(s, "ip",
						s->source_dsa.address,
						lpcfg_cldap_port(s->libnet->lp_ctx),
						&dest_address);
	if (ret != 0) {
		c->status = map_nt_error_from_unix(errno);
		if (!composite_is_ok(c)) return;
	}

	c->status = cldap_socket_init(s, s->libnet->event_ctx,
				      NULL, dest_address, &s->cldap.sock);
	if (!composite_is_ok(c)) return;

	req = cldap_netlogon_send(s, s->cldap.sock, &s->cldap.io);
	if (composite_nomem(req, c)) return;
	tevent_req_set_callback(req, unbecomeDC_recv_cldap, s);
}

static void unbecomeDC_connect_ldap(struct libnet_UnbecomeDC_state *s);

static void unbecomeDC_recv_cldap(struct tevent_req *req)
{
	struct libnet_UnbecomeDC_state *s = tevent_req_callback_data(req,
					    struct libnet_UnbecomeDC_state);
	struct composite_context *c = s->creq;

	c->status = cldap_netlogon_recv(req, s, &s->cldap.io);
	talloc_free(req);
	if (!composite_is_ok(c)) return;

	s->cldap.netlogon = s->cldap.io.out.netlogon.data.nt5_ex;

	s->domain.dns_name		= s->cldap.netlogon.dns_domain;
	s->domain.netbios_name		= s->cldap.netlogon.domain_name;
	s->domain.guid			= s->cldap.netlogon.domain_uuid;

	s->source_dsa.dns_name		= s->cldap.netlogon.pdc_dns_name;
	s->source_dsa.netbios_name	= s->cldap.netlogon.pdc_name;
	s->source_dsa.site_name		= s->cldap.netlogon.server_site;

	s->dest_dsa.site_name		= s->cldap.netlogon.client_site;

	unbecomeDC_connect_ldap(s);
}

static NTSTATUS unbecomeDC_ldap_connect(struct libnet_UnbecomeDC_state *s)
{
	char *url;

	url = talloc_asprintf(s, "ldap://%s/", s->source_dsa.dns_name);
	NT_STATUS_HAVE_NO_MEMORY(url);

	s->ldap.ldb = ldb_wrap_connect(s, s->libnet->event_ctx, s->libnet->lp_ctx, url,
				       NULL,
				       s->libnet->cred,
				       0);
	talloc_free(url);
	if (s->ldap.ldb == NULL) {
		return NT_STATUS_UNEXPECTED_NETWORK_ERROR;
	}

	return NT_STATUS_OK;
}

static NTSTATUS unbecomeDC_ldap_rootdse(struct libnet_UnbecomeDC_state *s)
{
	int ret;
	struct ldb_result *r;
	struct ldb_dn *basedn;
	static const char *attrs[] = {
		"defaultNamingContext",
		"configurationNamingContext",
		NULL
	};

	basedn = ldb_dn_new(s, s->ldap.ldb, NULL);
	NT_STATUS_HAVE_NO_MEMORY(basedn);

	ret = ldb_search(s->ldap.ldb, s, &r, basedn, LDB_SCOPE_BASE, attrs,
			 "(objectClass=*)");
	talloc_free(basedn);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_LDAP(ret);
	} else if (r->count != 1) {
		talloc_free(r);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	s->domain.dn_str	= ldb_msg_find_attr_as_string(r->msgs[0], "defaultNamingContext", NULL);
	if (!s->domain.dn_str) return NT_STATUS_INVALID_NETWORK_RESPONSE;
	talloc_steal(s, s->domain.dn_str);

	s->forest.config_dn_str	= ldb_msg_find_attr_as_string(r->msgs[0], "configurationNamingContext", NULL);
	if (!s->forest.config_dn_str) return NT_STATUS_INVALID_NETWORK_RESPONSE;
	talloc_steal(s, s->forest.config_dn_str);

	s->dest_dsa.server_dn_str = talloc_asprintf(s, "CN=%s,CN=Servers,CN=%s,CN=Sites,%s",
						    s->dest_dsa.netbios_name,
						    s->dest_dsa.site_name,
				                    s->forest.config_dn_str);
	NT_STATUS_HAVE_NO_MEMORY(s->dest_dsa.server_dn_str);

	talloc_free(r);
	return NT_STATUS_OK;
}

static NTSTATUS unbecomeDC_ldap_computer_object(struct libnet_UnbecomeDC_state *s)
{
	int ret;
	struct ldb_result *r;
	struct ldb_dn *basedn;
	static const char *attrs[] = {
		"distinguishedName",
		"userAccountControl",
		NULL
	};

	basedn = ldb_dn_new(s, s->ldap.ldb, s->domain.dn_str);
	NT_STATUS_HAVE_NO_MEMORY(basedn);

	ret = ldb_search(s->ldap.ldb, s, &r, basedn, LDB_SCOPE_SUBTREE, attrs,
			 "(&(|(objectClass=user)(objectClass=computer))(sAMAccountName=%s$))",
			 s->dest_dsa.netbios_name);
	talloc_free(basedn);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_LDAP(ret);
	} else if (r->count != 1) {
		talloc_free(r);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	s->dest_dsa.computer_dn_str	= ldb_msg_find_attr_as_string(r->msgs[0], "distinguishedName", NULL);
	if (!s->dest_dsa.computer_dn_str) return NT_STATUS_INVALID_NETWORK_RESPONSE;
	talloc_steal(s, s->dest_dsa.computer_dn_str);

	s->dest_dsa.user_account_control = ldb_msg_find_attr_as_uint(r->msgs[0], "userAccountControl", 0);

	talloc_free(r);
	return NT_STATUS_OK;
}

static NTSTATUS unbecomeDC_ldap_modify_computer(struct libnet_UnbecomeDC_state *s)
{
	int ret;
	struct ldb_message *msg;
	uint32_t user_account_control = UF_WORKSTATION_TRUST_ACCOUNT;
	unsigned int i;

	/* as the value is already as we want it to be, we're done */
	if (s->dest_dsa.user_account_control == user_account_control) {
		return NT_STATUS_OK;
	}

	/* make a 'modify' msg, and only for serverReference */
	msg = ldb_msg_new(s);
	NT_STATUS_HAVE_NO_MEMORY(msg);
	msg->dn = ldb_dn_new(msg, s->ldap.ldb, s->dest_dsa.computer_dn_str);
	NT_STATUS_HAVE_NO_MEMORY(msg->dn);

	ret = samdb_msg_add_uint(s->ldap.ldb, msg, msg, "userAccountControl",
				 user_account_control);
	if (ret != LDB_SUCCESS) {
		talloc_free(msg);
		return NT_STATUS_NO_MEMORY;
	}

	/* mark all the message elements (should be just one)
	   as LDB_FLAG_MOD_REPLACE */
	for (i=0;i<msg->num_elements;i++) {
		msg->elements[i].flags = LDB_FLAG_MOD_REPLACE;
	}

	ret = ldb_modify(s->ldap.ldb, msg);
	talloc_free(msg);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_LDAP(ret);
	}

	s->dest_dsa.user_account_control = user_account_control;

	return NT_STATUS_OK;
}

static NTSTATUS unbecomeDC_ldap_move_computer(struct libnet_UnbecomeDC_state *s)
{
	int ret;
	struct ldb_result *r;
	struct ldb_dn *basedn;
	struct ldb_dn *old_dn;
	struct ldb_dn *new_dn;
	static const char *_1_1_attrs[] = {
		"1.1",
		NULL
	};

	basedn = ldb_dn_new_fmt(s, s->ldap.ldb, "<WKGUID=aa312825768811d1aded00c04fd8d5cd,%s>",
				s->domain.dn_str);
	NT_STATUS_HAVE_NO_MEMORY(basedn);

	ret = ldb_search(s->ldap.ldb, s, &r, basedn, LDB_SCOPE_BASE,
			 _1_1_attrs, "(objectClass=*)");
	talloc_free(basedn);
	if (ret != LDB_SUCCESS) {
		return NT_STATUS_LDAP(ret);
	} else if (r->count != 1) {
		talloc_free(r);
		return NT_STATUS_INVALID_NETWORK_RESPONSE;
	}

	old_dn = ldb_dn_new(r, s->ldap.ldb, s->dest_dsa.computer_dn_str);
	NT_STATUS_HAVE_NO_MEMORY(old_dn);

	new_dn = r->msgs[0]->dn;

	if (!ldb_dn_add_child_fmt(new_dn, "CN=%s", s->dest_dsa.netbios_name)) {
		talloc_free(r);
		return NT_STATUS_NO_MEMORY;
	}

	if (ldb_dn_compare(old_dn, new_dn) == 0) {
		/* we don't need to rename if the old and new dn match */
		talloc_free(r);
		return NT_STATUS_OK;
	}

	ret = ldb_rename(s->ldap.ldb, old_dn, new_dn);
	if (ret != LDB_SUCCESS) {
		talloc_free(r);
		return NT_STATUS_LDAP(ret);
	}

	s->dest_dsa.computer_dn_str = ldb_dn_alloc_linearized(s, new_dn);
	NT_STATUS_HAVE_NO_MEMORY(s->dest_dsa.computer_dn_str);

	talloc_free(r);

	return NT_STATUS_OK;
}

static void unbecomeDC_drsuapi_connect_send(struct libnet_UnbecomeDC_state *s);

static void unbecomeDC_connect_ldap(struct libnet_UnbecomeDC_state *s)
{
	struct composite_context *c = s->creq;

	c->status = unbecomeDC_ldap_connect(s);
	if (!composite_is_ok(c)) return;

	c->status = unbecomeDC_ldap_rootdse(s);
	if (!composite_is_ok(c)) return;

	c->status = unbecomeDC_ldap_computer_object(s);
	if (!composite_is_ok(c)) return;

	c->status = unbecomeDC_ldap_modify_computer(s);
	if (!composite_is_ok(c)) return;

	c->status = unbecomeDC_ldap_move_computer(s);
	if (!composite_is_ok(c)) return;

	unbecomeDC_drsuapi_connect_send(s);
}

static void unbecomeDC_drsuapi_connect_recv(struct composite_context *creq);

static void unbecomeDC_drsuapi_connect_send(struct libnet_UnbecomeDC_state *s)
{
	struct composite_context *c = s->creq;
	struct composite_context *creq;
	char *binding_str;

	binding_str = talloc_asprintf(s, "ncacn_ip_tcp:%s[seal]", s->source_dsa.dns_name);
	if (composite_nomem(binding_str, c)) return;

	c->status = dcerpc_parse_binding(s, binding_str, &s->drsuapi.binding);
	talloc_free(binding_str);
	if (!composite_is_ok(c)) return;

	creq = dcerpc_pipe_connect_b_send(s, s->drsuapi.binding, &ndr_table_drsuapi,
					  s->libnet->cred, s->libnet->event_ctx,
					  s->libnet->lp_ctx);
	composite_continue(c, creq, unbecomeDC_drsuapi_connect_recv, s);
}

static void unbecomeDC_drsuapi_bind_send(struct libnet_UnbecomeDC_state *s);

static void unbecomeDC_drsuapi_connect_recv(struct composite_context *req)
{
	struct libnet_UnbecomeDC_state *s = talloc_get_type(req->async.private_data,
					    struct libnet_UnbecomeDC_state);
	struct composite_context *c = s->creq;

	c->status = dcerpc_pipe_connect_b_recv(req, s, &s->drsuapi.pipe);
	if (!composite_is_ok(c)) return;

	s->drsuapi.drsuapi_handle = s->drsuapi.pipe->binding_handle;

	unbecomeDC_drsuapi_bind_send(s);
}

static void unbecomeDC_drsuapi_bind_recv(struct tevent_req *subreq);

static void unbecomeDC_drsuapi_bind_send(struct libnet_UnbecomeDC_state *s)
{
	struct composite_context *c = s->creq;
	struct drsuapi_DsBindInfo28 *bind_info28;
	struct tevent_req *subreq;

	GUID_from_string(DRSUAPI_DS_BIND_GUID, &s->drsuapi.bind_guid);

	bind_info28				= &s->drsuapi.local_info28;
	bind_info28->supported_extensions	= 0;
	bind_info28->site_guid			= GUID_zero();
	bind_info28->pid			= 0;
	bind_info28->repl_epoch			= 0;

	s->drsuapi.bind_info_ctr.length		= 28;
	s->drsuapi.bind_info_ctr.info.info28	= *bind_info28;

	s->drsuapi.bind_r.in.bind_guid = &s->drsuapi.bind_guid;
	s->drsuapi.bind_r.in.bind_info = &s->drsuapi.bind_info_ctr;
	s->drsuapi.bind_r.out.bind_handle = &s->drsuapi.bind_handle;

	subreq = dcerpc_drsuapi_DsBind_r_send(s, c->event_ctx,
					      s->drsuapi.drsuapi_handle,
					      &s->drsuapi.bind_r);
	if (composite_nomem(subreq, c)) return;
	tevent_req_set_callback(subreq, unbecomeDC_drsuapi_bind_recv, s);
}

static void unbecomeDC_drsuapi_remove_ds_server_send(struct libnet_UnbecomeDC_state *s);

static void unbecomeDC_drsuapi_bind_recv(struct tevent_req *subreq)
{
	struct libnet_UnbecomeDC_state *s = tevent_req_callback_data(subreq,
					    struct libnet_UnbecomeDC_state);
	struct composite_context *c = s->creq;

	c->status = dcerpc_drsuapi_DsBind_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	if (!W_ERROR_IS_OK(s->drsuapi.bind_r.out.result)) {
		composite_error(c, werror_to_ntstatus(s->drsuapi.bind_r.out.result));
		return;
	}

	ZERO_STRUCT(s->drsuapi.remote_info28);
	if (s->drsuapi.bind_r.out.bind_info) {
		switch (s->drsuapi.bind_r.out.bind_info->length) {
		case 24: {
			struct drsuapi_DsBindInfo24 *info24;
			info24 = &s->drsuapi.bind_r.out.bind_info->info.info24;
			s->drsuapi.remote_info28.supported_extensions	= info24->supported_extensions;
			s->drsuapi.remote_info28.site_guid		= info24->site_guid;
			s->drsuapi.remote_info28.pid			= info24->pid;
			s->drsuapi.remote_info28.repl_epoch		= 0;
			break;
		}
		case 48: {
			struct drsuapi_DsBindInfo48 *info48;
			info48 = &s->drsuapi.bind_r.out.bind_info->info.info48;
			s->drsuapi.remote_info28.supported_extensions	= info48->supported_extensions;
			s->drsuapi.remote_info28.site_guid		= info48->site_guid;
			s->drsuapi.remote_info28.pid			= info48->pid;
			s->drsuapi.remote_info28.repl_epoch		= info48->repl_epoch;
			break;
		}
		case 28:
			s->drsuapi.remote_info28 = s->drsuapi.bind_r.out.bind_info->info.info28;
			break;
		}
	}

	unbecomeDC_drsuapi_remove_ds_server_send(s);
}

static void unbecomeDC_drsuapi_remove_ds_server_recv(struct tevent_req *subreq);

static void unbecomeDC_drsuapi_remove_ds_server_send(struct libnet_UnbecomeDC_state *s)
{
	struct composite_context *c = s->creq;
	struct drsuapi_DsRemoveDSServer *r = &s->drsuapi.rm_ds_srv_r;
	struct tevent_req *subreq;

	r->in.bind_handle	= &s->drsuapi.bind_handle;
	r->in.level		= 1;
	r->in.req		= talloc(s, union drsuapi_DsRemoveDSServerRequest);
	r->in.req->req1.server_dn = s->dest_dsa.server_dn_str;
	r->in.req->req1.domain_dn = s->domain.dn_str;
	r->in.req->req1.commit	= true;

	r->out.level_out	= talloc(s, uint32_t);
	r->out.res		= talloc(s, union drsuapi_DsRemoveDSServerResult);

	subreq = dcerpc_drsuapi_DsRemoveDSServer_r_send(s, c->event_ctx,
							s->drsuapi.drsuapi_handle,
							r);
	if (composite_nomem(subreq, c)) return;
	tevent_req_set_callback(subreq, unbecomeDC_drsuapi_remove_ds_server_recv, s);
}

static void unbecomeDC_drsuapi_remove_ds_server_recv(struct tevent_req *subreq)
{
	struct libnet_UnbecomeDC_state *s = tevent_req_callback_data(subreq,
					    struct libnet_UnbecomeDC_state);
	struct composite_context *c = s->creq;
	struct drsuapi_DsRemoveDSServer *r = &s->drsuapi.rm_ds_srv_r;

	c->status = dcerpc_drsuapi_DsRemoveDSServer_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	if (!W_ERROR_IS_OK(r->out.result)) {
		composite_error(c, werror_to_ntstatus(r->out.result));
		return;
	}

	if (*r->out.level_out != 1) {
		composite_error(c, NT_STATUS_INVALID_NETWORK_RESPONSE);
		return;
	}

	composite_done(c);
}

struct composite_context *libnet_UnbecomeDC_send(struct libnet_context *ctx, TALLOC_CTX *mem_ctx, struct libnet_UnbecomeDC *r)
{
	struct composite_context *c;
	struct libnet_UnbecomeDC_state *s;
	char *tmp_name;

	c = composite_create(mem_ctx, ctx->event_ctx);
	if (c == NULL) return NULL;

	s = talloc_zero(c, struct libnet_UnbecomeDC_state);
	if (composite_nomem(s, c)) return c;
	c->private_data = s;
	s->creq		= c;
	s->libnet	= ctx;

	/* Domain input */
	s->domain.dns_name	= talloc_strdup(s, r->in.domain_dns_name);
	if (composite_nomem(s->domain.dns_name, c)) return c;
	s->domain.netbios_name	= talloc_strdup(s, r->in.domain_netbios_name);
	if (composite_nomem(s->domain.netbios_name, c)) return c;

	/* Source DSA input */
	s->source_dsa.address	= talloc_strdup(s, r->in.source_dsa_address);
	if (composite_nomem(s->source_dsa.address, c)) return c;

	/* Destination DSA input */
	s->dest_dsa.netbios_name= talloc_strdup(s, r->in.dest_dsa_netbios_name);
	if (composite_nomem(s->dest_dsa.netbios_name, c)) return c;

	/* Destination DSA dns_name construction */
	tmp_name		= strlower_talloc(s, s->dest_dsa.netbios_name);
	if (composite_nomem(tmp_name, c)) return c;
	s->dest_dsa.dns_name	= talloc_asprintf_append_buffer(tmp_name, ".%s",
				  			 s->domain.dns_name);
	if (composite_nomem(s->dest_dsa.dns_name, c)) return c;

	unbecomeDC_send_cldap(s);
	return c;
}

NTSTATUS libnet_UnbecomeDC_recv(struct composite_context *c, TALLOC_CTX *mem_ctx, struct libnet_UnbecomeDC *r)
{
	NTSTATUS status;

	status = composite_wait(c);

	ZERO_STRUCT(r->out);

	talloc_free(c);
	return status;
}

NTSTATUS libnet_UnbecomeDC(struct libnet_context *ctx, TALLOC_CTX *mem_ctx, struct libnet_UnbecomeDC *r)
{
	NTSTATUS status;
	struct composite_context *c;
	c = libnet_UnbecomeDC_send(ctx, mem_ctx, r);
	status = libnet_UnbecomeDC_recv(c, mem_ctx, r);
	return status;
}
