/*
   Unix SMB/CIFS implementation.
   Test suite for libnet calls.

   Copyright (C) Rafal Szczesniak 2005

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
#include "system/time.h"
#include "lib/cmdline/popt_common.h"
#include "libnet/libnet.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "torture/rpc/torture_rpc.h"
#include "torture/libnet/usertest.h"
#include "torture/libnet/proto.h"
#include "param/param.h"



bool torture_createuser(struct torture_context *torture)
{
	NTSTATUS status;
	TALLOC_CTX *mem_ctx;
	struct libnet_context *ctx = NULL;
	struct libnet_CreateUser req;
	bool ret = true;

	mem_ctx = talloc_init("test_createuser");

	if (!test_libnet_context_init(torture, true, &ctx)) {
		return false;
	}

	req.in.user_name = TEST_USERNAME;
	req.in.domain_name = lpcfg_workgroup(torture->lp_ctx);
	req.out.error_string = NULL;

	status = libnet_CreateUser(ctx, mem_ctx, &req);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "libnet_CreateUser call failed: %s\n", nt_errstr(status));
		ret = false;
		goto done;
	}

	if (!test_user_cleanup(torture, ctx->samr.pipe->binding_handle,
	                       mem_ctx, &ctx->samr.handle, TEST_USERNAME)) {
		torture_comment(torture, "cleanup failed\n");
		ret = false;
		goto done;
	}

	if (!test_samr_close_handle(torture,
	                            ctx->samr.pipe->binding_handle, mem_ctx, &ctx->samr.handle)) {
		torture_comment(torture, "domain close failed\n");
		ret = false;
	}

done:
	talloc_free(ctx);
	talloc_free(mem_ctx);
	return ret;
}


bool torture_deleteuser(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p;
	TALLOC_CTX *mem_ctx;
	struct policy_handle h;
	struct lsa_String domain_name;
	const char *name = TEST_USERNAME;
	struct libnet_context *ctx = NULL;
	struct libnet_DeleteUser req;
	bool ret = false;

	status = torture_rpc_connection(torture,
					&p,
					&ndr_table_samr);
	torture_assert_ntstatus_ok(torture, status, "torture_rpc_connection() failed");

	mem_ctx = talloc_init("torture_deleteuser");

	/*
	 * Pre-create a user to be deleted later
	 */
	domain_name.string = lpcfg_workgroup(torture->lp_ctx);
	ret = test_domain_open(torture, p->binding_handle, &domain_name, mem_ctx, &h, NULL);
	torture_assert_goto(torture, ret, ret, done, "test_domain_open() failed");

	ret = test_user_create(torture, p->binding_handle, mem_ctx, &h, name, NULL);
	torture_assert_goto(torture, ret, ret, done, "test_user_create() failed");

	/*
	 * Delete the user using libnet layer
	 */
	ret = test_libnet_context_init(torture, true, &ctx);
	torture_assert_goto(torture, ret, ret, done, "test_libnet_context_init() failed");

	req.in.user_name = TEST_USERNAME;
	req.in.domain_name = lpcfg_workgroup(torture->lp_ctx);

	status = libnet_DeleteUser(ctx, mem_ctx, &req);
	torture_assert_ntstatus_ok_goto(torture, status, ret, done, "libnet_DeleteUser() failed");

	/* mark test as successful */
	ret = true;

done:
	talloc_free(ctx);
	talloc_free(mem_ctx);
	return ret;
}


/*
  Generate testing set of random changes
*/

static void set_test_changes(struct torture_context *tctx,
			     TALLOC_CTX *mem_ctx, struct libnet_ModifyUser *r,
			     int num_changes, char **user_name, enum test_fields req_change)
{
	const char* logon_scripts[] = { "start_login.cmd", "login.bat", "start.cmd" };
	const char* home_dirs[] = { "\\\\srv\\home", "\\\\homesrv\\home\\user", "\\\\pdcsrv\\domain" };
	const char* home_drives[] = { "H:", "z:", "I:", "J:", "n:" };
	const uint32_t flags[] = { (ACB_DISABLED | ACB_NORMAL | ACB_PW_EXPIRED),
				   (ACB_NORMAL | ACB_PWNOEXP),
				   (ACB_NORMAL | ACB_PW_EXPIRED) };
	const char *homedir, *homedrive, *logonscript;
	struct timeval now;
	int i, testfld;

	torture_comment(tctx, "Fields to change: [");

	for (i = 0; i < num_changes && i <= USER_FIELD_LAST; i++) {
		const char *fldname;

		testfld = (req_change == none) ? (random() % USER_FIELD_LAST) + 1 : req_change;

		/* get one in case we hit time field this time */
		gettimeofday(&now, NULL);

		switch (testfld) {
		case acct_name:
			continue_if_field_set(r->in.account_name);
			r->in.account_name = talloc_asprintf(mem_ctx, TEST_CHG_ACCOUNTNAME,
							     (int)(random() % 100));
			fldname = "account_name";

			/* update the test's user name in case it's about to change */
			*user_name = talloc_strdup(mem_ctx, r->in.account_name);
			break;

		case acct_full_name:
			continue_if_field_set(r->in.full_name);
			r->in.full_name = talloc_asprintf(mem_ctx, TEST_CHG_FULLNAME,
							  (unsigned int)random(), (unsigned int)random());
			fldname = "full_name";
			break;

		case acct_description:
			continue_if_field_set(r->in.description);
			r->in.description = talloc_asprintf(mem_ctx, TEST_CHG_DESCRIPTION,
							    (long)random());
			fldname = "description";
			break;

		case acct_home_directory:
			continue_if_field_set(r->in.home_directory);
			homedir = home_dirs[random() % ARRAY_SIZE(home_dirs)];
			r->in.home_directory = talloc_strdup(mem_ctx, homedir);
			fldname = "home_dir";
			break;

		case acct_home_drive:
			continue_if_field_set(r->in.home_drive);
			homedrive = home_drives[random() % ARRAY_SIZE(home_drives)];
			r->in.home_drive = talloc_strdup(mem_ctx, homedrive);
			fldname = "home_drive";
			break;

		case acct_comment:
			continue_if_field_set(r->in.comment);
			r->in.comment = talloc_asprintf(mem_ctx, TEST_CHG_COMMENT,
							(unsigned long)random(), (unsigned long)random());
			fldname = "comment";
			break;

		case acct_logon_script:
			continue_if_field_set(r->in.logon_script);
			logonscript = logon_scripts[random() % ARRAY_SIZE(logon_scripts)];
			r->in.logon_script = talloc_strdup(mem_ctx, logonscript);
			fldname = "logon_script";
			break;

		case acct_profile_path:
			continue_if_field_set(r->in.profile_path);
			r->in.profile_path = talloc_asprintf(mem_ctx, TEST_CHG_PROFILEPATH,
							     (unsigned long)random(), (unsigned int)random());
			fldname = "profile_path";
			break;

		case acct_expiry:
			continue_if_field_set(r->in.acct_expiry);
			now = timeval_add(&now, (random() % (31*24*60*60)), 0);
			r->in.acct_expiry = (struct timeval *)talloc_memdup(mem_ctx, &now, sizeof(now));
			fldname = "acct_expiry";
			break;

		case acct_flags:
			continue_if_field_set(r->in.acct_flags);
			r->in.acct_flags = flags[random() % ARRAY_SIZE(flags)];
			fldname = "acct_flags";
			break;

		default:
			fldname = "unknown_field";
		}

		torture_comment(tctx, ((i < num_changes - 1) ? "%s," : "%s"), fldname);

		/* disable requested field (it's supposed to be the only one used) */
		if (req_change != none) req_change = none;
	}

	torture_comment(tctx, "]\n");
}


#define TEST_STR_FLD(fld) \
	if (!strequal(req.in.fld, user_req.out.fld)) { \
		torture_comment(torture, "failed to change '%s'\n", #fld); \
		ret = false; \
		goto cleanup; \
	}

#define TEST_TIME_FLD(fld) \
	if (timeval_compare(req.in.fld, user_req.out.fld)) { \
		torture_comment(torture, "failed to change '%s'\n", #fld); \
		ret = false; \
		goto cleanup; \
	}

#define TEST_NUM_FLD(fld) \
	if (req.in.fld != user_req.out.fld) { \
		torture_comment(torture, "failed to change '%s'\n", #fld); \
		ret = false; \
		goto cleanup; \
	}


bool torture_modifyuser(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p;
	TALLOC_CTX *prep_mem_ctx;
	struct policy_handle h;
	struct lsa_String domain_name;
	char *name;
	struct libnet_context *ctx = NULL;
	struct libnet_ModifyUser req;
	struct libnet_UserInfo user_req;
	int fld;
	bool ret = true;
	struct dcerpc_binding_handle *b;

	prep_mem_ctx = talloc_init("prepare test_deleteuser");

	status = torture_rpc_connection(torture,
					&p,
					&ndr_table_samr);
	if (!NT_STATUS_IS_OK(status)) {
		ret = false;
		goto done;
	}
	b = p->binding_handle;

	name = talloc_strdup(prep_mem_ctx, TEST_USERNAME);

	domain_name.string = lpcfg_workgroup(torture->lp_ctx);
	if (!test_domain_open(torture, b, &domain_name, prep_mem_ctx, &h, NULL)) {
		ret = false;
		goto done;
	}

	if (!test_user_create(torture, b, prep_mem_ctx, &h, name, NULL)) {
		ret = false;
		goto done;
	}

	torture_comment(torture, "Testing change of all fields - each single one in turn\n");

	if (!test_libnet_context_init(torture, true, &ctx)) {
		return false;
	}

	for (fld = USER_FIELD_FIRST; fld <= USER_FIELD_LAST; fld++) {
		ZERO_STRUCT(req);
		req.in.domain_name = lpcfg_workgroup(torture->lp_ctx);
		req.in.user_name = name;

		set_test_changes(torture, torture, &req, 1, &name, fld);

		status = libnet_ModifyUser(ctx, torture, &req);
		if (!NT_STATUS_IS_OK(status)) {
			torture_comment(torture, "libnet_ModifyUser call failed: %s\n", nt_errstr(status));
			ret = false;
			continue;
		}

		ZERO_STRUCT(user_req);
		user_req.in.domain_name = lpcfg_workgroup(torture->lp_ctx);
		user_req.in.data.user_name = name;
		user_req.in.level = USER_INFO_BY_NAME;

		status = libnet_UserInfo(ctx, torture, &user_req);
		if (!NT_STATUS_IS_OK(status)) {
			torture_comment(torture, "libnet_UserInfo call failed: %s\n", nt_errstr(status));
			ret = false;
			continue;
		}

		switch (fld) {
		case acct_name: TEST_STR_FLD(account_name);
			break;
		case acct_full_name: TEST_STR_FLD(full_name);
			break;
		case acct_comment: TEST_STR_FLD(comment);
			break;
		case acct_description: TEST_STR_FLD(description);
			break;
		case acct_home_directory: TEST_STR_FLD(home_directory);
			break;
		case acct_home_drive: TEST_STR_FLD(home_drive);
			break;
		case acct_logon_script: TEST_STR_FLD(logon_script);
			break;
		case acct_profile_path: TEST_STR_FLD(profile_path);
			break;
		case acct_expiry: TEST_TIME_FLD(acct_expiry);
			break;
		case acct_flags: TEST_NUM_FLD(acct_flags);
			break;
		default:
			break;
		}
	}

cleanup:
	if (!test_user_cleanup(torture, ctx->samr.pipe->binding_handle,
	                       torture, &ctx->samr.handle, TEST_USERNAME)) {
		torture_comment(torture, "cleanup failed\n");
		ret = false;
		goto done;
	}

	if (!test_samr_close_handle(torture,
	                            ctx->samr.pipe->binding_handle, torture, &ctx->samr.handle)) {
		torture_comment(torture, "domain close failed\n");
		ret = false;
	}

done:
	talloc_free(ctx);
	talloc_free(prep_mem_ctx);
	return ret;
}


bool torture_userinfo_api(struct torture_context *torture)
{
	const char *name = TEST_USERNAME;
	bool ret = true;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx = NULL, *prep_mem_ctx;
	struct libnet_context *ctx = NULL;
	struct dcerpc_pipe *p;
	struct policy_handle h;
	struct lsa_String domain_name;
	struct libnet_UserInfo req;
	struct dcerpc_binding_handle *b;

	prep_mem_ctx = talloc_init("prepare torture user info");

	status = torture_rpc_connection(torture,
					&p,
					&ndr_table_samr);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}
	b = p->binding_handle;

	domain_name.string = lpcfg_workgroup(torture->lp_ctx);
	if (!test_domain_open(torture, b, &domain_name, prep_mem_ctx, &h, NULL)) {
		ret = false;
		goto done;
	}

	if (!test_user_create(torture, b, prep_mem_ctx, &h, name, NULL)) {
		ret = false;
		goto done;
	}

	mem_ctx = talloc_init("torture user info");

	if (!test_libnet_context_init(torture, true, &ctx)) {
		return false;
	}

	ZERO_STRUCT(req);

	req.in.domain_name = domain_name.string;
	req.in.data.user_name   = name;
	req.in.level = USER_INFO_BY_NAME;

	status = libnet_UserInfo(ctx, mem_ctx, &req);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "libnet_UserInfo call failed: %s\n", nt_errstr(status));
		ret = false;
		goto done;
	}

	if (!test_user_cleanup(torture, ctx->samr.pipe->binding_handle,
	                       mem_ctx, &ctx->samr.handle, TEST_USERNAME)) {
		torture_comment(torture, "cleanup failed\n");
		ret = false;
		goto done;
	}

	if (!test_samr_close_handle(torture,
	                            ctx->samr.pipe->binding_handle, mem_ctx, &ctx->samr.handle)) {
		torture_comment(torture, "domain close failed\n");
		ret = false;
	}

done:
	talloc_free(ctx);
	talloc_free(mem_ctx);
	return ret;
}


bool torture_userlist(struct torture_context *torture)
{
	bool ret = true;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx = NULL;
	struct libnet_context *ctx;
	struct lsa_String domain_name;
	struct libnet_UserList req;
	int i;

	ctx = libnet_context_init(torture->ev, torture->lp_ctx);
	ctx->cred = cmdline_credentials;

	domain_name.string = lpcfg_workgroup(torture->lp_ctx);
	mem_ctx = talloc_init("torture user list");

	ZERO_STRUCT(req);

	torture_comment(torture, "listing user accounts:\n");

	do {

		req.in.domain_name = domain_name.string;
		req.in.page_size   = 128;
		req.in.resume_index = req.out.resume_index;

		status = libnet_UserList(ctx, mem_ctx, &req);
		if (!NT_STATUS_IS_OK(status) &&
		    !NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES)) break;

		for (i = 0; i < req.out.count; i++) {
			torture_comment(torture, "\tuser: %s, sid=%s\n",
			                req.out.users[i].username, req.out.users[i].sid);
		}

	} while (NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES));

	if (!(NT_STATUS_IS_OK(status) ||
	      NT_STATUS_EQUAL(status, NT_STATUS_NO_MORE_ENTRIES))) {
		torture_comment(torture, "libnet_UserList call failed: %s\n", nt_errstr(status));
		ret = false;
		goto done;
	}

	if (!test_samr_close_handle(torture,
	                            ctx->samr.pipe->binding_handle, mem_ctx, &ctx->samr.handle)) {
		torture_comment(torture, "samr domain close failed\n");
		ret = false;
		goto done;
	}

	if (!test_lsa_close_handle(torture,
	                           ctx->lsa.pipe->binding_handle, mem_ctx, &ctx->lsa.handle)) {
		torture_comment(torture, "lsa domain close failed\n");
		ret = false;
	}

	talloc_free(ctx);

done:
	talloc_free(mem_ctx);
	return ret;
}
