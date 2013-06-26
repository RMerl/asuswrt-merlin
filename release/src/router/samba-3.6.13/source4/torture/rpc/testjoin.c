/* 
   Unix SMB/CIFS implementation.

   utility code to join/leave a domain

   Copyright (C) Andrew Tridgell 2004
   
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

/*
  this code is used by other torture modules to join/leave a domain
  as either a member, bdc or thru a trust relationship
*/

#include "includes.h"
#include "system/time.h"
#include "../lib/crypto/crypto.h"
#include "libnet/libnet.h"
#include "lib/cmdline/popt_common.h"
#include "librpc/gen_ndr/ndr_samr_c.h"

#include "libcli/auth/libcli_auth.h"
#include "torture/rpc/torture_rpc.h"
#include "libcli/security/security.h"
#include "param/param.h"

struct test_join {
	struct dcerpc_pipe *p;
	struct policy_handle user_handle;
	struct libnet_JoinDomain *libnet_r;
	struct dom_sid *dom_sid;
	const char *dom_netbios_name;
	const char *dom_dns_name;
	struct dom_sid *user_sid;
	struct GUID user_guid;
	const char *netbios_name;
};


static NTSTATUS DeleteUser_byname(struct dcerpc_binding_handle *b,
				  TALLOC_CTX *mem_ctx,
				  struct policy_handle *handle, const char *name)
{
	NTSTATUS status;
	struct samr_DeleteUser d;
	struct policy_handle user_handle;
	uint32_t rid;
	struct samr_LookupNames n;
	struct samr_Ids rids, types;
	struct lsa_String sname;
	struct samr_OpenUser r;

	sname.string = name;

	n.in.domain_handle = handle;
	n.in.num_names = 1;
	n.in.names = &sname;
	n.out.rids = &rids;
	n.out.types = &types;

	status = dcerpc_samr_LookupNames_r(b, mem_ctx, &n);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (NT_STATUS_IS_OK(n.out.result)) {
		rid = n.out.rids->ids[0];
	} else {
		return n.out.result;
	}

	r.in.domain_handle = handle;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.in.rid = rid;
	r.out.user_handle = &user_handle;

	status = dcerpc_samr_OpenUser_r(b, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("OpenUser(%s) failed - %s\n", name, nt_errstr(status));
		return status;
	}
	if (!NT_STATUS_IS_OK(r.out.result)) {
		printf("OpenUser(%s) failed - %s\n", name, nt_errstr(r.out.result));
		return r.out.result;
	}

	d.in.user_handle = &user_handle;
	d.out.user_handle = &user_handle;
	status = dcerpc_samr_DeleteUser_r(b, mem_ctx, &d);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(d.out.result)) {
		return d.out.result;
	}

	return NT_STATUS_OK;
}

/*
  create a test user in the domain
  an opaque pointer is returned. Pass it to torture_leave_domain() 
  when finished
*/

struct test_join *torture_create_testuser_max_pwlen(struct torture_context *torture,
						    const char *username,
						    const char *domain,
						    uint16_t acct_type,
						    const char **random_password,
						    int max_pw_len)
{
	NTSTATUS status;
	struct samr_Connect c;
	struct samr_CreateUser2 r;
	struct samr_OpenDomain o;
	struct samr_LookupDomain l;
	struct dom_sid2 *sid = NULL;
	struct samr_GetUserPwInfo pwp;
	struct samr_PwInfo info;
	struct samr_SetUserInfo s;
	union samr_UserInfo u;
	struct policy_handle handle;
	struct policy_handle domain_handle;
	uint32_t access_granted;
	uint32_t rid;
	DATA_BLOB session_key;
	struct lsa_String name;
	
	int policy_min_pw_len = 0;
	struct test_join *join;
	char *random_pw;
	const char *dc_binding = torture_setting_string(torture, "dc_binding", NULL);
	struct dcerpc_binding_handle *b = NULL;

	join = talloc(NULL, struct test_join);
	if (join == NULL) {
		return NULL;
	}

	ZERO_STRUCTP(join);

	printf("Connecting to SAMR\n");
	
	if (dc_binding) {
		status = dcerpc_pipe_connect(join,
					     &join->p,
					     dc_binding,
					     &ndr_table_samr,
					     cmdline_credentials, NULL, torture->lp_ctx);
					     
	} else {
		status = torture_rpc_connection(torture, 
						&join->p, 
						&ndr_table_samr);
	}
	if (!NT_STATUS_IS_OK(status)) {
		return NULL;
	}
	b = join->p->binding_handle;

	c.in.system_name = NULL;
	c.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	c.out.connect_handle = &handle;

	status = dcerpc_samr_Connect_r(b, join, &c);
	if (!NT_STATUS_IS_OK(status)) {
		const char *errstr = nt_errstr(status);
		printf("samr_Connect failed - %s\n", errstr);
		return NULL;
	}
	if (!NT_STATUS_IS_OK(c.out.result)) {
		const char *errstr = nt_errstr(c.out.result);
		printf("samr_Connect failed - %s\n", errstr);
		return NULL;
	}

	if (domain) {
		printf("Opening domain %s\n", domain);

		name.string = domain;
		l.in.connect_handle = &handle;
		l.in.domain_name = &name;
		l.out.sid = &sid;

		status = dcerpc_samr_LookupDomain_r(b, join, &l);
		if (!NT_STATUS_IS_OK(status)) {
			printf("LookupDomain failed - %s\n", nt_errstr(status));
			goto failed;
		}
		if (!NT_STATUS_IS_OK(l.out.result)) {
			printf("LookupDomain failed - %s\n", nt_errstr(l.out.result));
			goto failed;
		}
	} else {
		struct samr_EnumDomains e;
		uint32_t resume_handle = 0, num_entries;
		struct samr_SamArray *sam;
		int i;

		e.in.connect_handle = &handle;
		e.in.buf_size = (uint32_t)-1;
		e.in.resume_handle = &resume_handle;
		e.out.sam = &sam;
		e.out.num_entries = &num_entries;
		e.out.resume_handle = &resume_handle;

		status = dcerpc_samr_EnumDomains_r(b, join, &e);
		if (!NT_STATUS_IS_OK(status)) {
			printf("EnumDomains failed - %s\n", nt_errstr(status));
			goto failed;
		}
		if (!NT_STATUS_IS_OK(e.out.result)) {
			printf("EnumDomains failed - %s\n", nt_errstr(e.out.result));
			goto failed;
		}
		if ((num_entries != 2) || (sam && sam->count != 2)) {
			printf("unexpected number of domains\n");
			goto failed;
		}
		for (i=0; i < 2; i++) {
			if (!strequal(sam->entries[i].name.string, "builtin")) {
				domain = sam->entries[i].name.string;
				break;
			}
		}
		if (domain) {
			printf("Opening domain %s\n", domain);

			name.string = domain;
			l.in.connect_handle = &handle;
			l.in.domain_name = &name;
			l.out.sid = &sid;

			status = dcerpc_samr_LookupDomain_r(b, join, &l);
			if (!NT_STATUS_IS_OK(status)) {
				printf("LookupDomain failed - %s\n", nt_errstr(status));
				goto failed;
			}
			if (!NT_STATUS_IS_OK(l.out.result)) {
				printf("LookupDomain failed - %s\n", nt_errstr(l.out.result));
				goto failed;
			}
		} else {
			printf("cannot proceed without domain name\n");
			goto failed;
		}
	}

	talloc_steal(join, *l.out.sid);
	join->dom_sid = *l.out.sid;
	join->dom_netbios_name = talloc_strdup(join, domain);
	if (!join->dom_netbios_name) goto failed;

	o.in.connect_handle = &handle;
	o.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	o.in.sid = *l.out.sid;
	o.out.domain_handle = &domain_handle;

	status = dcerpc_samr_OpenDomain_r(b, join, &o);
	if (!NT_STATUS_IS_OK(status)) {
		printf("OpenDomain failed - %s\n", nt_errstr(status));
		goto failed;
	}
	if (!NT_STATUS_IS_OK(o.out.result)) {
		printf("OpenDomain failed - %s\n", nt_errstr(o.out.result));
		goto failed;
	}

	printf("Creating account %s\n", username);

again:
	name.string = username;
	r.in.domain_handle = &domain_handle;
	r.in.account_name = &name;
	r.in.acct_flags = acct_type;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.user_handle = &join->user_handle;
	r.out.access_granted = &access_granted;
	r.out.rid = &rid;

	status = dcerpc_samr_CreateUser2_r(b, join, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("CreateUser2 failed - %s\n", nt_errstr(status));
		goto failed;
	}

	if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_USER_EXISTS)) {
		status = DeleteUser_byname(b, join, &domain_handle, name.string);
		if (NT_STATUS_IS_OK(status)) {
			goto again;
		}
	}

	if (!NT_STATUS_IS_OK(r.out.result)) {
		printf("CreateUser2 failed - %s\n", nt_errstr(r.out.result));
		goto failed;
	}

	join->user_sid = dom_sid_add_rid(join, join->dom_sid, rid);

	pwp.in.user_handle = &join->user_handle;
	pwp.out.info = &info;

	status = dcerpc_samr_GetUserPwInfo_r(b, join, &pwp);
	if (NT_STATUS_IS_OK(status) && NT_STATUS_IS_OK(pwp.out.result)) {
		policy_min_pw_len = pwp.out.info->min_password_length;
	}

	random_pw = generate_random_password(join, MAX(8, policy_min_pw_len), max_pw_len);

	printf("Setting account password '%s'\n", random_pw);

	ZERO_STRUCT(u);
	s.in.user_handle = &join->user_handle;
	s.in.info = &u;
	s.in.level = 24;

	encode_pw_buffer(u.info24.password.data, random_pw, STR_UNICODE);
	u.info24.password_expired = 0;

	status = dcerpc_fetch_session_key(join->p, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		printf("SetUserInfo level %u - no session key - %s\n",
		       s.in.level, nt_errstr(status));
		torture_leave_domain(torture, join);
		goto failed;
	}

	arcfour_crypt_blob(u.info24.password.data, 516, &session_key);

	status = dcerpc_samr_SetUserInfo_r(b, join, &s);
	if (!NT_STATUS_IS_OK(status)) {
		printf("SetUserInfo failed - %s\n", nt_errstr(status));
		goto failed;
	}
	if (!NT_STATUS_IS_OK(s.out.result)) {
		printf("SetUserInfo failed - %s\n", nt_errstr(s.out.result));
		goto failed;
	}

	ZERO_STRUCT(u);
	s.in.user_handle = &join->user_handle;
	s.in.info = &u;
	s.in.level = 21;

	u.info21.acct_flags = acct_type | ACB_PWNOEXP;
	u.info21.fields_present = SAMR_FIELD_ACCT_FLAGS | SAMR_FIELD_DESCRIPTION | SAMR_FIELD_COMMENT | SAMR_FIELD_FULL_NAME;

	u.info21.comment.string = talloc_asprintf(join, 
						  "Tortured by Samba4: %s", 
						  timestring(join, time(NULL)));
	
	u.info21.full_name.string = talloc_asprintf(join, 
						    "Torture account for Samba4: %s", 
						    timestring(join, time(NULL)));
	
	u.info21.description.string = talloc_asprintf(join, 
					 "Samba4 torture account created by host %s: %s", 
					 lpcfg_netbios_name(torture->lp_ctx),
					 timestring(join, time(NULL)));

	printf("Resetting ACB flags, force pw change time\n");

	status = dcerpc_samr_SetUserInfo_r(b, join, &s);
	if (!NT_STATUS_IS_OK(status)) {
		printf("SetUserInfo failed - %s\n", nt_errstr(status));
		goto failed;
	}
	if (!NT_STATUS_IS_OK(s.out.result)) {
		printf("SetUserInfo failed - %s\n", nt_errstr(s.out.result));
		goto failed;
	}

	if (random_password) {
		*random_password = random_pw;
	}

	return join;

failed:
	torture_leave_domain(torture, join);
	return NULL;
}


struct test_join *torture_create_testuser(struct torture_context *torture,
					  const char *username,
					  const char *domain,
					  uint16_t acct_type,
					  const char **random_password)
{
	return torture_create_testuser_max_pwlen(torture, username, domain, acct_type, random_password, 255);
}

_PUBLIC_ struct test_join *torture_join_domain(struct torture_context *tctx,
					       const char *machine_name, 
				      uint32_t acct_flags,
				      struct cli_credentials **machine_credentials)
{
	NTSTATUS status;
	struct libnet_context *libnet_ctx;
	struct libnet_JoinDomain *libnet_r;
	struct test_join *tj;
	struct samr_SetUserInfo s;
	union samr_UserInfo u;
	
	tj = talloc(tctx, struct test_join);
	if (!tj) return NULL;

	libnet_r = talloc(tj, struct libnet_JoinDomain);
	if (!libnet_r) {
		talloc_free(tj);
		return NULL;
	}
	
	libnet_ctx = libnet_context_init(tctx->ev, tctx->lp_ctx);
	if (!libnet_ctx) {
		talloc_free(tj);
		return NULL;
	}
	
	tj->libnet_r = libnet_r;
		
	libnet_ctx->cred = cmdline_credentials;
	libnet_r->in.binding = torture_setting_string(tctx, "binding", NULL);
	if (!libnet_r->in.binding) {
		libnet_r->in.binding = talloc_asprintf(libnet_r, "ncacn_np:%s", torture_setting_string(tctx, "host", NULL));
	}
	libnet_r->in.level = LIBNET_JOINDOMAIN_SPECIFIED;
	libnet_r->in.netbios_name = machine_name;
	libnet_r->in.account_name = talloc_asprintf(libnet_r, "%s$", machine_name);
	if (!libnet_r->in.account_name) {
		talloc_free(tj);
		return NULL;
	}
	
	libnet_r->in.acct_type = acct_flags;
	libnet_r->in.recreate_account = true;

	status = libnet_JoinDomain(libnet_ctx, libnet_r, libnet_r);
	if (!NT_STATUS_IS_OK(status)) {
		if (libnet_r->out.error_string) {
			DEBUG(0, ("Domain join failed - %s\n", libnet_r->out.error_string));
		} else {
			DEBUG(0, ("Domain join failed - %s\n", nt_errstr(status)));
		}
		talloc_free(tj);
                return NULL;
	}
	tj->p = libnet_r->out.samr_pipe;
	tj->user_handle = *libnet_r->out.user_handle;
	tj->dom_sid = libnet_r->out.domain_sid;
	talloc_steal(tj, libnet_r->out.domain_sid);
	tj->dom_netbios_name	= libnet_r->out.domain_name;
	talloc_steal(tj, libnet_r->out.domain_name);
	tj->dom_dns_name	= libnet_r->out.realm;
	talloc_steal(tj, libnet_r->out.realm);
	tj->user_guid = libnet_r->out.account_guid;
	tj->netbios_name = talloc_strdup(tj, machine_name);
	if (!tj->netbios_name) {
		talloc_free(tj);
		return NULL;
	}

	ZERO_STRUCT(u);
	s.in.user_handle = &tj->user_handle;
	s.in.info = &u;
	s.in.level = 21;

	u.info21.fields_present = SAMR_FIELD_DESCRIPTION | SAMR_FIELD_COMMENT | SAMR_FIELD_FULL_NAME;
	u.info21.comment.string = talloc_asprintf(tj, 
						  "Tortured by Samba4: %s", 
						  timestring(tj, time(NULL)));
	u.info21.full_name.string = talloc_asprintf(tj, 
						    "Torture account for Samba4: %s", 
						    timestring(tj, time(NULL)));
	
	u.info21.description.string = talloc_asprintf(tj, 
						      "Samba4 torture account created by host %s: %s", 
						      lpcfg_netbios_name(tctx->lp_ctx), timestring(tj, time(NULL)));

	status = dcerpc_samr_SetUserInfo_r(tj->p->binding_handle, tj, &s);
	if (!NT_STATUS_IS_OK(status)) {
		printf("SetUserInfo (non-critical) failed - %s\n", nt_errstr(status));
	}
	if (!NT_STATUS_IS_OK(s.out.result)) {
		printf("SetUserInfo (non-critical) failed - %s\n", nt_errstr(s.out.result));
	}

	*machine_credentials = cli_credentials_init(tj);
	cli_credentials_set_conf(*machine_credentials, tctx->lp_ctx);
	cli_credentials_set_workstation(*machine_credentials, machine_name, CRED_SPECIFIED);
	cli_credentials_set_domain(*machine_credentials, libnet_r->out.domain_name, CRED_SPECIFIED);
	if (libnet_r->out.realm) {
		cli_credentials_set_realm(*machine_credentials, libnet_r->out.realm, CRED_SPECIFIED);
	}
	cli_credentials_set_username(*machine_credentials, libnet_r->in.account_name, CRED_SPECIFIED);
	cli_credentials_set_password(*machine_credentials, libnet_r->out.join_password, CRED_SPECIFIED);
	cli_credentials_set_kvno(*machine_credentials, libnet_r->out.kvno);
	if (acct_flags & ACB_SVRTRUST) {
		cli_credentials_set_secure_channel_type(*machine_credentials,
							SEC_CHAN_BDC);
	} else if (acct_flags & ACB_WSTRUST) {
		cli_credentials_set_secure_channel_type(*machine_credentials,
							SEC_CHAN_WKSTA);
	} else {
		DEBUG(0, ("Invalid account type specificed to torture_join_domain\n"));
		talloc_free(*machine_credentials);
		return NULL;
	}

	return tj;
}

struct dcerpc_pipe *torture_join_samr_pipe(struct test_join *join) 
{
	return join->p;
}

struct policy_handle *torture_join_samr_user_policy(struct test_join *join) 
{
	return &join->user_handle;
}

static NTSTATUS torture_leave_ads_domain(struct torture_context *torture,
					 TALLOC_CTX *mem_ctx,
					 struct libnet_JoinDomain *libnet_r)
{
	int rtn;
	TALLOC_CTX *tmp_ctx;

	struct ldb_dn *server_dn;
	struct ldb_context *ldb_ctx;

	char *remote_ldb_url; 
	 
	/* Check if we are a domain controller. If not, exit. */
	if (!libnet_r->out.server_dn_str) {
		return NT_STATUS_OK;
	}

	tmp_ctx = talloc_named(mem_ctx, 0, "torture_leave temporary context");
	if (!tmp_ctx) {
		libnet_r->out.error_string = NULL;
		return NT_STATUS_NO_MEMORY;
	}

	ldb_ctx = ldb_init(tmp_ctx, torture->ev);
	if (!ldb_ctx) {
		libnet_r->out.error_string = NULL;
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	/* Remove CN=Servers,... entry from the AD. */ 
	server_dn = ldb_dn_new(tmp_ctx, ldb_ctx, libnet_r->out.server_dn_str);
	if (! ldb_dn_validate(server_dn)) {
		libnet_r->out.error_string = NULL;
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	remote_ldb_url = talloc_asprintf(tmp_ctx, "ldap://%s", libnet_r->out.samr_binding->host);
	if (!remote_ldb_url) {
		libnet_r->out.error_string = NULL;
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	ldb_set_opaque(ldb_ctx, "credentials", cmdline_credentials);
	ldb_set_opaque(ldb_ctx, "loadparm", cmdline_lp_ctx);

	rtn = ldb_connect(ldb_ctx, remote_ldb_url, 0, NULL);
	if (rtn != 0) {
		libnet_r->out.error_string = NULL;
		talloc_free(tmp_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	rtn = ldb_delete(ldb_ctx, server_dn);
	if (rtn != 0) {
		libnet_r->out.error_string = NULL;
		talloc_free(tmp_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	DEBUG(0, ("%s removed successfully.\n", libnet_r->out.server_dn_str));

	talloc_free(tmp_ctx); 
	return NT_STATUS_OK;
}

/*
  leave the domain, deleting the machine acct
*/

_PUBLIC_ void torture_leave_domain(struct torture_context *torture, struct test_join *join)
{
	struct samr_DeleteUser d;
	NTSTATUS status;

	if (!join) {
		return;
	}
	d.in.user_handle = &join->user_handle;
	d.out.user_handle = &join->user_handle;

	/* Delete machine account */
	status = dcerpc_samr_DeleteUser_r(join->p->binding_handle, join, &d);
	if (!NT_STATUS_IS_OK(status)) {
		printf("DeleteUser failed\n");
	}
	if (!NT_STATUS_IS_OK(d.out.result)) {
		printf("Delete of machine account %s failed\n",
		       join->netbios_name);
	} else {
		printf("Delete of machine account %s was successful.\n",
		       join->netbios_name);
	}

	if (join->libnet_r) {
		status = torture_leave_ads_domain(torture, join, join->libnet_r);
	}
	
	talloc_free(join);
}

/*
  return the dom sid for a test join
*/
_PUBLIC_ const struct dom_sid *torture_join_sid(struct test_join *join)
{
	return join->dom_sid;
}

const struct dom_sid *torture_join_user_sid(struct test_join *join)
{
	return join->user_sid;
}

const char *torture_join_netbios_name(struct test_join *join)
{
	return join->netbios_name;
}

const struct GUID *torture_join_user_guid(struct test_join *join)
{
	return &join->user_guid;
}

const char *torture_join_dom_netbios_name(struct test_join *join)
{
	return join->dom_netbios_name;
}

const char *torture_join_dom_dns_name(struct test_join *join)
{
	return join->dom_dns_name;
}

const char *torture_join_server_dn_str(struct test_join *join)
{
	if (join->libnet_r) {
		return join->libnet_r->out.server_dn_str;
	}
	return NULL;
}


#if 0 /* Left as the documentation of the join process, but see new implementation in libnet_become_dc.c */
struct test_join_ads_dc {
	struct test_join *join;
};

struct test_join_ads_dc *torture_join_domain_ads_dc(const char *machine_name, 
						    const char *domain,
						    struct cli_credentials **machine_credentials)
{
	struct test_join_ads_dc *join;

	join = talloc(NULL, struct test_join_ads_dc);
	if (join == NULL) {
		return NULL;
	}

	join->join = torture_join_domain(machine_name, 
					ACB_SVRTRUST,
					machine_credentials);

	if (!join->join) {
		return NULL;
	}

/* W2K: */
	/* W2K: modify userAccountControl from 4096 to 532480 */
	
	/* W2K: modify RDN to OU=Domain Controllers and skip the $ from server name */

	/* ask objectVersion of Schema Partition */

	/* ask rIDManagerReferenz of the Domain Partition */

	/* ask fsMORoleOwner of the RID-Manager$ object
	 * returns CN=NTDS Settings,CN=<DC>,CN=Servers,CN=Default-First-Site-Name, ...
	 */

	/* ask for dnsHostName of CN=<DC>,CN=Servers,CN=Default-First-Site-Name, ... */

	/* ask for objectGUID of CN=NTDS Settings,CN=<DC>,CN=Servers,CN=Default-First-Site-Name, ... */

	/* ask for * of CN=Default-First-Site-Name, ... */

	/* search (&(|(objectClass=user)(objectClass=computer))(sAMAccountName=<machine_name>$)) in Domain Partition 
	 * attributes : distinguishedName, userAccountControl
	 */

	/* ask * for CN=<machine_name>,CN=Servers,CN=Default-First-Site-Name,... 
	 * should fail with noSuchObject
	 */

	/* add CN=<machine_name>,CN=Servers,CN=Default-First-Site-Name,... 
	 *
	 * objectClass = server
	 * systemFlags = 50000000
	 * serverReferenz = CN=<machine_name>,OU=Domain Controllers,...
	 */

	/* ask for * of CN=NTDS Settings,CN=<machine_name>,CN=Servers,CN=Default-First-Site-Name, ...
	 * should fail with noSuchObject
	 */

	/* search for (ncname=<domain_nc>) in CN=Partitions,CN=Configuration,... 
	 * attributes: ncName, dnsRoot
	 */

	/* modify add CN=<machine_name>,CN=Servers,CN=Default-First-Site-Name,...
	 * serverReferenz = CN=<machine_name>,OU=Domain Controllers,...
	 * should fail with attributeOrValueExists
	 */

	/* modify replace CN=<machine_name>,CN=Servers,CN=Default-First-Site-Name,...
	 * serverReferenz = CN=<machine_name>,OU=Domain Controllers,...
	 */

	/* DsAddEntry to create the CN=NTDS Settings,CN=<machine_name>,CN=Servers,CN=Default-First-Site-Name, ...
	 *
	 */

	/* replicate CN=Schema,CN=Configuration,...
	 * using DRSUAPI_DS_BIND_GUID_W2K ("6abec3d1-3054-41c8-a362-5a0c5b7d5d71")
	 *
	 */

	/* replicate CN=Configuration,...
	 * using DRSUAPI_DS_BIND_GUID_W2K ("6abec3d1-3054-41c8-a362-5a0c5b7d5d71")
	 *
	 */

	/* replicate Domain Partition
	 * using DRSUAPI_DS_BIND_GUID_W2K ("6abec3d1-3054-41c8-a362-5a0c5b7d5d71")
	 *
	 */

	/* call DsReplicaUpdateRefs() for all partitions like this:
	 *     req1: struct drsuapi_DsReplicaUpdateRefsRequest1
	 *           naming_context           : *
	 *                 naming_context: struct drsuapi_DsReplicaObjectIdentifier
	 *                     __ndr_size               : 0x000000ae (174)
	 *                     __ndr_size_sid           : 0x00000000 (0)
	 *                     guid                     : 00000000-0000-0000-0000-000000000000
	 *                     sid                      : S-0-0
	 *                     dn                       : 'CN=Schema,CN=Configuration,DC=w2k3,DC=vmnet1,DC=vm,DC=base'
	 *           dest_dsa_dns_name        : *
	 *                 dest_dsa_dns_name        : '4a0df188-a0b8-47ea-bbe5-e614723f16dd._msdcs.w2k3.vmnet1.vm.base'
	 *           dest_dsa_guid            : 4a0df188-a0b8-47ea-bbe5-e614723f16dd
	 *           options                  : 0x0000001c (28)
	 *                 0: DRSUAPI_DS_REPLICA_UPDATE_ASYNCHRONOUS_OPERATION
	 *                 0: DRSUAPI_DS_REPLICA_UPDATE_WRITEABLE
	 *                 1: DRSUAPI_DS_REPLICA_UPDATE_ADD_REFERENCE
	 *                 1: DRSUAPI_DS_REPLICA_UPDATE_DELETE_REFERENCE
	 *                 1: DRSUAPI_DS_REPLICA_UPDATE_0x00000010      
	 *
	 * 4a0df188-a0b8-47ea-bbe5-e614723f16dd is the objectGUID the DsAddEntry() returned for the
	 * CN=NTDS Settings,CN=<machine_name>,CN=Servers,CN=Default-First-Site-Name, ...
	 */

/* W2K3: see libnet/libnet_become_dc.c */
	return join;
}

#endif
