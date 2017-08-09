/*
   Unix SMB/CIFS implementation.
   Test suite for libnet calls.

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

/*
 * These are more general use functions shared among the tests.
 */

#include "includes.h"
#include "lib/cmdline/popt_common.h"
#include "torture/rpc/torture_rpc.h"
#include "libnet/libnet.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "torture/libnet/proto.h"
#include "ldb_wrap.h"

/**
 * Opens handle on Domain using SAMR
 *
 * @param _domain_handle [out] Ptr to storage to store Domain handle
 * @param _dom_sid [out] If NULL, Domain SID won't be returned
 */
bool test_domain_open(struct torture_context *tctx,
		      struct dcerpc_binding_handle *b,
		      struct lsa_String *domname,
		      TALLOC_CTX *mem_ctx,
		      struct policy_handle *_domain_handle,
		      struct dom_sid2 *_dom_sid)
{
	struct policy_handle connect_handle;
	struct policy_handle domain_handle;
	struct samr_Connect r1;
	struct samr_LookupDomain r2;
	struct dom_sid2 *sid = NULL;
	struct samr_OpenDomain r3;

	torture_comment(tctx, "connecting\n");

	r1.in.system_name = 0;
	r1.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r1.out.connect_handle = &connect_handle;

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_samr_Connect_r(b, mem_ctx, &r1),
				   "Connect failed");
	torture_assert_ntstatus_ok(tctx, r1.out.result,
				   "Connect failed");

	r2.in.connect_handle = &connect_handle;
	r2.in.domain_name = domname;
	r2.out.sid = &sid;

	torture_comment(tctx, "domain lookup on %s\n", domname->string);

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_samr_LookupDomain_r(b, mem_ctx, &r2),
				   "LookupDomain failed");
	torture_assert_ntstatus_ok(tctx, r2.out.result,
				   "LookupDomain failed");

	r3.in.connect_handle = &connect_handle;
	r3.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r3.in.sid = *r2.out.sid;
	r3.out.domain_handle = &domain_handle;

	torture_comment(tctx, "opening domain %s\n", domname->string);

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_samr_OpenDomain_r(b, mem_ctx, &r3),
				   "OpenDomain failed");
	torture_assert_ntstatus_ok(tctx, r3.out.result,
				   "OpenDomain failed");

	*_domain_handle = domain_handle;

	if (_dom_sid) {
		*_dom_sid = **r2.out.sid;
	}

	/* Close connect_handle, we don't need it anymore */
	test_samr_close_handle(tctx, b, mem_ctx, &connect_handle);

	return true;
}


/**
 * Find out user's samAccountName for given
 * user RDN. We need samAccountName value
 * when deleting users.
 */
static bool _get_account_name_for_user_rdn(struct torture_context *tctx,
					   const char *user_rdn,
					   TALLOC_CTX *mem_ctx,
					   const char **_account_name)
{
	const char *url;
	struct ldb_context *ldb;
	TALLOC_CTX *tmp_ctx;
	bool test_res = true;
	const char *hostname = torture_setting_string(tctx, "host", NULL);
	int ldb_ret;
	struct ldb_result *ldb_res;
	const char *account_name = NULL;
	static const char *attrs[] = {
		"samAccountName",
		NULL
	};

	torture_assert(tctx, hostname != NULL, "Failed to get hostname");

	tmp_ctx = talloc_new(tctx);
	torture_assert(tctx, tmp_ctx != NULL, "Failed to create temporary mem context");

	url = talloc_asprintf(tmp_ctx, "ldap://%s/", hostname);
	torture_assert_goto(tctx, url != NULL, test_res, done, "Failed to allocate URL for ldb");

	ldb = ldb_wrap_connect(tmp_ctx,
	                       tctx->ev, tctx->lp_ctx,
	                       url, NULL, cmdline_credentials, 0);
	torture_assert_goto(tctx, ldb != NULL, test_res, done, "Failed to make LDB connection");

	ldb_ret = ldb_search(ldb, tmp_ctx, &ldb_res,
	                     ldb_get_default_basedn(ldb), LDB_SCOPE_SUBTREE,
	                     attrs,
	                     "(&(objectClass=user)(name=%s))", user_rdn);
	if (LDB_SUCCESS == ldb_ret && 1 == ldb_res->count) {
		account_name = ldb_msg_find_attr_as_string(ldb_res->msgs[0], "samAccountName", NULL);
	}

	/* return user_rdn by default */
	if (!account_name) {
		account_name = user_rdn;
	}

	/* duplicate memory in parent context */
	*_account_name = talloc_strdup(mem_ctx, account_name);

done:
	talloc_free(tmp_ctx);
	return test_res;
}

/**
 * Removes user by RDN through SAMR interface.
 *
 * @param domain_handle [in] Domain handle
 * @param user_rdn [in] User's RDN in ldap database
 */
bool test_user_cleanup(struct torture_context *tctx,
		       struct dcerpc_binding_handle *b,
		       TALLOC_CTX *mem_ctx,
		       struct policy_handle *domain_handle,
		       const char *user_rdn)
{
	struct samr_LookupNames r1;
	struct samr_OpenUser r2;
	struct samr_DeleteUser r3;
	struct lsa_String names[2];
	uint32_t rid;
	struct policy_handle user_handle;
	struct samr_Ids rids, types;
	const char *account_name;

	if (!_get_account_name_for_user_rdn(tctx, user_rdn, mem_ctx, &account_name)) {
		torture_result(tctx, TORTURE_FAIL,
		               __location__": Failed to find samAccountName for %s", user_rdn);
		return false;
	}

	names[0].string = account_name;

	r1.in.domain_handle  = domain_handle;
	r1.in.num_names      = 1;
	r1.in.names          = names;
	r1.out.rids          = &rids;
	r1.out.types         = &types;

	torture_comment(tctx, "user account lookup '%s'\n", account_name);

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_samr_LookupNames_r(b, mem_ctx, &r1),
				   "LookupNames failed");
	torture_assert_ntstatus_ok(tctx, r1.out.result,
				   "LookupNames failed");

	rid = r1.out.rids->ids[0];

	r2.in.domain_handle  = domain_handle;
	r2.in.access_mask    = SEC_FLAG_MAXIMUM_ALLOWED;
	r2.in.rid            = rid;
	r2.out.user_handle   = &user_handle;

	torture_comment(tctx, "opening user account\n");

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_samr_OpenUser_r(b, mem_ctx, &r2),
				   "OpenUser failed");
	torture_assert_ntstatus_ok(tctx, r2.out.result,
				   "OpenUser failed");

	r3.in.user_handle  = &user_handle;
	r3.out.user_handle = &user_handle;

	torture_comment(tctx, "deleting user account\n");

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_samr_DeleteUser_r(b, mem_ctx, &r3),
				   "DeleteUser failed");
	torture_assert_ntstatus_ok(tctx, r3.out.result,
				   "DeleteUser failed");

	return true;
}


/**
 * Creates new user using SAMR
 *
 * @param name [in] Username for user to create
 * @param rid [out] If NULL, User's RID is not returned
 */
bool test_user_create(struct torture_context *tctx,
		      struct dcerpc_binding_handle *b,
		      TALLOC_CTX *mem_ctx,
		      struct policy_handle *domain_handle,
		      const char *name,
		      uint32_t *rid)
{
	struct policy_handle user_handle;
	struct lsa_String username;
	struct samr_CreateUser r;
	uint32_t user_rid;

	username.string = name;

	r.in.domain_handle = domain_handle;
	r.in.account_name  = &username;
	r.in.access_mask   = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.user_handle  = &user_handle;
	/* return user's RID only if requested */
	r.out.rid 	   = rid ? rid : &user_rid;

	torture_comment(tctx, "creating user '%s'\n", username.string);

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_samr_CreateUser_r(b, mem_ctx, &r),
				   "CreateUser RPC call failed");
	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_comment(tctx, "CreateUser failed - %s\n", nt_errstr(r.out.result));

		if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_USER_EXISTS)) {
			torture_comment(tctx,
			                "User (%s) already exists - "
			                "attempting to delete and recreate account again\n",
			                username.string);
			if (!test_user_cleanup(tctx, b, mem_ctx, domain_handle, username.string)) {
				return false;
			}

			torture_comment(tctx, "creating user account\n");

			torture_assert_ntstatus_ok(tctx,
						   dcerpc_samr_CreateUser_r(b, mem_ctx, &r),
						   "CreateUser RPC call failed");
			torture_assert_ntstatus_ok(tctx, r.out.result,
						   "CreateUser failed");

			/* be nice and close opened handles */
			test_samr_close_handle(tctx, b, mem_ctx, &user_handle);

			return true;
		}
		return false;
	}

	/* be nice and close opened handles */
	test_samr_close_handle(tctx, b, mem_ctx, &user_handle);

	return true;
}


/**
 * Deletes a Group using SAMR interface
 */
bool test_group_cleanup(struct torture_context *tctx,
			struct dcerpc_binding_handle *b,
			TALLOC_CTX *mem_ctx,
			struct policy_handle *domain_handle,
			const char *name)
{
	struct samr_LookupNames r1;
	struct samr_OpenGroup r2;
	struct samr_DeleteDomainGroup r3;
	struct lsa_String names[2];
	uint32_t rid;
	struct policy_handle group_handle;
	struct samr_Ids rids, types;

	names[0].string = name;

	r1.in.domain_handle  = domain_handle;
	r1.in.num_names      = 1;
	r1.in.names          = names;
	r1.out.rids          = &rids;
	r1.out.types         = &types;

	torture_comment(tctx, "group account lookup '%s'\n", name);

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_samr_LookupNames_r(b, mem_ctx, &r1),
				   "LookupNames failed");
	torture_assert_ntstatus_ok(tctx, r1.out.result,
				   "LookupNames failed");

	rid = r1.out.rids->ids[0];

	r2.in.domain_handle  = domain_handle;
	r2.in.access_mask    = SEC_FLAG_MAXIMUM_ALLOWED;
	r2.in.rid            = rid;
	r2.out.group_handle  = &group_handle;

	torture_comment(tctx, "opening group account\n");

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_samr_OpenGroup_r(b, mem_ctx, &r2),
				   "OpenGroup failed");
	torture_assert_ntstatus_ok(tctx, r2.out.result,
				   "OpenGroup failed");

	r3.in.group_handle  = &group_handle;
	r3.out.group_handle = &group_handle;

	torture_comment(tctx, "deleting group account\n");

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_samr_DeleteDomainGroup_r(b, mem_ctx, &r3),
				   "DeleteGroup failed");
	torture_assert_ntstatus_ok(tctx, r3.out.result,
				   "DeleteGroup failed");

	return true;
}


/**
 * Creates a Group object using SAMR interface
 *
 * @param group_name [in] Name of the group to create
 * @param rid [out] RID of group created. May be NULL in
 *                  which case RID is not required by caller
 */
bool test_group_create(struct torture_context *tctx,
		       struct dcerpc_binding_handle *b,
		       TALLOC_CTX *mem_ctx,
		       struct policy_handle *handle,
		       const char *group_name,
		       uint32_t *rid)
{
	uint32_t group_rid;
	struct lsa_String groupname;
	struct samr_CreateDomainGroup r;
	struct policy_handle group_handle;

	groupname.string = group_name;

	r.in.domain_handle  = handle;
	r.in.name           = &groupname;
	r.in.access_mask    = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.group_handle  = &group_handle;
	/* use local variable in case caller
	 * don't care about the group RID */
	r.out.rid           = rid ? rid : &group_rid;

	torture_comment(tctx, "creating group account %s\n", group_name);

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_samr_CreateDomainGroup_r(b, mem_ctx, &r),
				   "CreateGroup failed");
	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_comment(tctx, "CreateGroup failed - %s\n", nt_errstr(r.out.result));

		if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_GROUP_EXISTS)) {
			torture_comment(tctx,
			                "Group (%s) already exists - "
			                "attempting to delete and recreate group again\n",
			                group_name);
			if (!test_group_cleanup(tctx, b, mem_ctx, handle, group_name)) {
				return false;
			}

			torture_comment(tctx, "creating group account\n");

			torture_assert_ntstatus_ok(tctx,
						   dcerpc_samr_CreateDomainGroup_r(b, mem_ctx, &r),
						   "CreateGroup failed");
			torture_assert_ntstatus_ok(tctx, r.out.result,
						   "CreateGroup failed");

			/* be nice and close opened handles */
			test_samr_close_handle(tctx, b, mem_ctx, &group_handle);

			return true;
		}
		return false;
	}

	/* be nice and close opened handles */
	test_samr_close_handle(tctx, b, mem_ctx, &group_handle);

	return true;
}

/**
 * Closes SAMR handle obtained from Connect, Open User/Domain, etc
 */
bool test_samr_close_handle(struct torture_context *tctx,
			    struct dcerpc_binding_handle *b,
			    TALLOC_CTX *mem_ctx,
			    struct policy_handle *samr_handle)
{
	struct samr_Close r;

	r.in.handle = samr_handle;
	r.out.handle = samr_handle;

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_samr_Close_r(b, mem_ctx, &r),
				   "Close SAMR handle RPC call failed");
	torture_assert_ntstatus_ok(tctx, r.out.result,
				   "Close SAMR handle failed");

	return true;
}

/**
 * Closes LSA handle obtained from Connect, Open Group, etc
 */
bool test_lsa_close_handle(struct torture_context *tctx,
			   struct dcerpc_binding_handle *b,
			   TALLOC_CTX *mem_ctx,
			   struct policy_handle *lsa_handle)
{
	struct lsa_Close r;

	r.in.handle = lsa_handle;
	r.out.handle = lsa_handle;

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_lsa_Close_r(b, mem_ctx, &r),
				   "Close LSA handle RPC call failed");
	torture_assert_ntstatus_ok(tctx, r.out.result,
				   "Close LSA handle failed");

	return true;
}

/**
 * Create and initialize libnet_context Context.
 * Use this function in cases where we need to have SAMR and LSA pipes
 * of libnet_context to be connected before executing any other
 * libnet call
 *
 * @param rpc_connect [in] Connects SAMR and LSA pipes
 */
bool test_libnet_context_init(struct torture_context *tctx,
			      bool rpc_connect,
			      struct libnet_context **_net_ctx)
{
	NTSTATUS status;
	bool bret = true;
	struct libnet_context *net_ctx;

	net_ctx = libnet_context_init(tctx->ev, tctx->lp_ctx);
	torture_assert(tctx, net_ctx != NULL, "Failed to create libnet_context");

	/* Use command line credentials for testing */
	net_ctx->cred = cmdline_credentials;

	if (rpc_connect) {
		/* connect SAMR pipe */
		status = torture_rpc_connection(tctx,
						&net_ctx->samr.pipe,
						&ndr_table_samr);
		torture_assert_ntstatus_ok_goto(tctx, status, bret, done,
						"Failed to connect SAMR pipe");

		net_ctx->samr.samr_handle = net_ctx->samr.pipe->binding_handle;

		/* connect LSARPC pipe */
		status = torture_rpc_connection(tctx,
						&net_ctx->lsa.pipe,
						&ndr_table_lsarpc);
		torture_assert_ntstatus_ok_goto(tctx, status, bret, done,
						"Failed to connect LSA pipe");

		net_ctx->lsa.lsa_handle = net_ctx->lsa.pipe->binding_handle;
	}

	*_net_ctx = net_ctx;

done:
	if (!bret) {
		/* a previous call has failed,
		 * clean up memory before exit */
		talloc_free(net_ctx);
	}
	return bret;
}


void msg_handler(struct monitor_msg *m)
{
	struct msg_rpc_open_user *msg_open;
	struct msg_rpc_query_user *msg_query;
	struct msg_rpc_close_user *msg_close;
	struct msg_rpc_create_user *msg_create;

	switch (m->type) {
	case mon_SamrOpenUser:
		msg_open = (struct msg_rpc_open_user*)m->data;
		printf("monitor_msg: user opened (rid=%d, access_mask=0x%08x)\n",
		       msg_open->rid, msg_open->access_mask);
		break;
	case mon_SamrQueryUser:
		msg_query = (struct msg_rpc_query_user*)m->data;
		printf("monitor_msg: user queried (level=%d)\n", msg_query->level);
		break;
	case mon_SamrCloseUser:
		msg_close = (struct msg_rpc_close_user*)m->data;
		printf("monitor_msg: user closed (rid=%d)\n", msg_close->rid);
		break;
	case mon_SamrCreateUser:
		msg_create = (struct msg_rpc_create_user*)m->data;
		printf("monitor_msg: user created (rid=%d)\n", msg_create->rid);
		break;
	}
}
