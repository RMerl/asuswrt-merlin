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
#include "torture/torture.h"
#include "torture/rpc/rpc.h"
#include "torture/libnet/usertest.h"
#include "param/param.h"


static bool test_cleanup(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			 struct policy_handle *domain_handle, const char *username)
{
	NTSTATUS status;
	struct samr_LookupNames r1;
	struct samr_OpenUser r2;
	struct samr_DeleteUser r3;
	struct lsa_String names[2];
	uint32_t rid;
	struct policy_handle user_handle;
	struct samr_Ids rids, types;

	names[0].string = username;

	r1.in.domain_handle  = domain_handle;
	r1.in.num_names      = 1;
	r1.in.names          = names;
	r1.out.rids          = &rids;
	r1.out.types         = &types;
	
	printf("user account lookup '%s'\n", username);

	status = dcerpc_samr_LookupNames(p, mem_ctx, &r1);
	if (!NT_STATUS_IS_OK(status)) {
		printf("LookupNames failed - %s\n", nt_errstr(status));
		return false;
	}

	rid = r1.out.rids->ids[0];
	
	r2.in.domain_handle  = domain_handle;
	r2.in.access_mask    = SEC_FLAG_MAXIMUM_ALLOWED;
	r2.in.rid            = rid;
	r2.out.user_handle   = &user_handle;

	printf("opening user account\n");

	status = dcerpc_samr_OpenUser(p, mem_ctx, &r2);
	if (!NT_STATUS_IS_OK(status)) {
		printf("OpenUser failed - %s\n", nt_errstr(status));
		return false;
	}

	r3.in.user_handle  = &user_handle;
	r3.out.user_handle = &user_handle;

	printf("deleting user account\n");
	
	status = dcerpc_samr_DeleteUser(p, mem_ctx, &r3);
	if (!NT_STATUS_IS_OK(status)) {
		printf("DeleteUser failed - %s\n", nt_errstr(status));
		return false;
	}

	return true;
}


static bool test_opendomain(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			    struct policy_handle *handle, struct lsa_String *domname)
{
	NTSTATUS status;
	struct policy_handle h, domain_handle;
	struct samr_Connect r1;
	struct samr_LookupDomain r2;
	struct dom_sid2 *sid = NULL;
	struct samr_OpenDomain r3;
	
	printf("connecting\n");
	
	r1.in.system_name = 0;
	r1.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r1.out.connect_handle = &h;
	
	status = dcerpc_samr_Connect(p, mem_ctx, &r1);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Connect failed - %s\n", nt_errstr(status));
		return false;
	}
	
	r2.in.connect_handle = &h;
	r2.in.domain_name = domname;
	r2.out.sid = &sid;

	printf("domain lookup on %s\n", domname->string);

	status = dcerpc_samr_LookupDomain(p, mem_ctx, &r2);
	if (!NT_STATUS_IS_OK(status)) {
		printf("LookupDomain failed - %s\n", nt_errstr(status));
		return false;
	}

	r3.in.connect_handle = &h;
	r3.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r3.in.sid = *r2.out.sid;
	r3.out.domain_handle = &domain_handle;

	printf("opening domain\n");

	status = dcerpc_samr_OpenDomain(p, mem_ctx, &r3);
	if (!NT_STATUS_IS_OK(status)) {
		printf("OpenDomain failed - %s\n", nt_errstr(status));
		return false;
	} else {
		*handle = domain_handle;
	}

	return true;
}


static bool test_samr_close(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			    struct policy_handle *domain_handle)
{
	NTSTATUS status;
	struct samr_Close r;
  
	r.in.handle = domain_handle;
	r.out.handle = domain_handle;

	status = dcerpc_samr_Close(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Close samr domain failed - %s\n", nt_errstr(status));
		return false;
	}
	
	return true;
}


static bool test_lsa_close(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			   struct policy_handle *domain_handle)
{
	NTSTATUS status;
	struct lsa_Close r;

	r.in.handle = domain_handle;
	r.out.handle = domain_handle;
	
	status = dcerpc_lsa_Close(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Close lsa domain failed - %s\n", nt_errstr(status));
		return false;
	}

	return true;
}


static bool test_createuser(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
 			    struct policy_handle *handle, const char* user)
{
	NTSTATUS status;
	struct policy_handle user_handle;
	struct lsa_String username;
	struct samr_CreateUser r1;
	struct samr_Close r2;
	uint32_t user_rid;

	username.string = user;
	
	r1.in.domain_handle = handle;
	r1.in.account_name = &username;
	r1.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r1.out.user_handle = &user_handle;
	r1.out.rid = &user_rid;

	printf("creating user '%s'\n", username.string);
	
	status = dcerpc_samr_CreateUser(p, mem_ctx, &r1);
	if (!NT_STATUS_IS_OK(status)) {
		printf("CreateUser failed - %s\n", nt_errstr(status));

		if (NT_STATUS_EQUAL(status, NT_STATUS_USER_EXISTS)) {
			printf("User (%s) already exists - attempting to delete and recreate account again\n", user);
			if (!test_cleanup(p, mem_ctx, handle, TEST_USERNAME)) {
				return false;
			}

			printf("creating user account\n");
			
			status = dcerpc_samr_CreateUser(p, mem_ctx, &r1);
			if (!NT_STATUS_IS_OK(status)) {
				printf("CreateUser failed - %s\n", nt_errstr(status));
				return false;
			}
			return true;
		}		
		return false;
	}

	r2.in.handle = &user_handle;
	r2.out.handle = &user_handle;
	
	printf("closing user '%s'\n", username.string);

	status = dcerpc_samr_Close(p, mem_ctx, &r2);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Close failed - %s\n", nt_errstr(status));
		return false;
	}

	return true;
}


bool torture_createuser(struct torture_context *torture)
{
	NTSTATUS status;
	TALLOC_CTX *mem_ctx;
	struct libnet_context *ctx;
	struct libnet_CreateUser req;
	bool ret = true;

	mem_ctx = talloc_init("test_createuser");

	ctx = libnet_context_init(torture->ev, torture->lp_ctx);
	ctx->cred = cmdline_credentials;

	req.in.user_name = TEST_USERNAME;
	req.in.domain_name = lp_workgroup(torture->lp_ctx);
	req.out.error_string = NULL;

	status = libnet_CreateUser(ctx, mem_ctx, &req);
	if (!NT_STATUS_IS_OK(status)) {
		printf("libnet_CreateUser call failed: %s\n", nt_errstr(status));
		ret = false;
		goto done;
	}

	if (!test_cleanup(ctx->samr.pipe, mem_ctx, &ctx->samr.handle, TEST_USERNAME)) {
		printf("cleanup failed\n");
		ret = false;
		goto done;
	}

	if (!test_samr_close(ctx->samr.pipe, mem_ctx, &ctx->samr.handle)) {
		printf("domain close failed\n");
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
	TALLOC_CTX *prep_mem_ctx, *mem_ctx;
	struct policy_handle h;
	struct lsa_String domain_name;
	const char *name = TEST_USERNAME;
	struct libnet_context *ctx;
	struct libnet_DeleteUser req;
	bool ret = true;

	prep_mem_ctx = talloc_init("prepare test_deleteuser");

	ctx = libnet_context_init(torture->ev, torture->lp_ctx);
	ctx->cred = cmdline_credentials;

	req.in.user_name = TEST_USERNAME;
	req.in.domain_name = lp_workgroup(torture->lp_ctx);

	status = torture_rpc_connection(torture,
					&p,
					&ndr_table_samr);
	if (!NT_STATUS_IS_OK(status)) {
		ret = false;
		goto done;
	}

	domain_name.string = lp_workgroup(torture->lp_ctx);
	if (!test_opendomain(p, prep_mem_ctx, &h, &domain_name)) {
		ret = false;
		goto done;
	}

	if (!test_createuser(p, prep_mem_ctx, &h, name)) {
		ret = false;
		goto done;
	}

	mem_ctx = talloc_init("test_deleteuser");

	status = libnet_DeleteUser(ctx, mem_ctx, &req);
	if (!NT_STATUS_IS_OK(status)) {
		printf("libnet_DeleteUser call failed: %s\n", nt_errstr(status));
		ret = false;
	}

	talloc_free(mem_ctx);

done:
	talloc_free(ctx);
	talloc_free(prep_mem_ctx);
	return ret;
}


/*
  Generate testing set of random changes
*/

static void set_test_changes(TALLOC_CTX *mem_ctx, struct libnet_ModifyUser *r,
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

	printf("Fields to change: [");

	for (i = 0; i < num_changes && i < FIELDS_NUM; i++) {
		const char *fldname;

		testfld = (req_change == none) ? (random() % FIELDS_NUM) : req_change;

		/* get one in case we hit time field this time */
		gettimeofday(&now, NULL);
		
		switch (testfld) {
		case account_name:
			continue_if_field_set(r->in.account_name);
			r->in.account_name = talloc_asprintf(mem_ctx, TEST_CHG_ACCOUNTNAME,
							     (int)(random() % 100));
			fldname = "account_name";
			
			/* update the test's user name in case it's about to change */
			*user_name = talloc_strdup(mem_ctx, r->in.account_name);
			break;

		case full_name:
			continue_if_field_set(r->in.full_name);
			r->in.full_name = talloc_asprintf(mem_ctx, TEST_CHG_FULLNAME,
							  (unsigned int)random(), (unsigned int)random());
			fldname = "full_name";
			break;

		case description:
			continue_if_field_set(r->in.description);
			r->in.description = talloc_asprintf(mem_ctx, TEST_CHG_DESCRIPTION,
							    (long)random());
			fldname = "description";
			break;

		case home_directory:
			continue_if_field_set(r->in.home_directory);
			homedir = home_dirs[random() % ARRAY_SIZE(home_dirs)];
			r->in.home_directory = talloc_strdup(mem_ctx, homedir);
			fldname = "home_dir";
			break;

		case home_drive:
			continue_if_field_set(r->in.home_drive);
			homedrive = home_drives[random() % ARRAY_SIZE(home_drives)];
			r->in.home_drive = talloc_strdup(mem_ctx, homedrive);
			fldname = "home_drive";
			break;

		case comment:
			continue_if_field_set(r->in.comment);
			r->in.comment = talloc_asprintf(mem_ctx, TEST_CHG_COMMENT,
							(unsigned long)random(), (unsigned long)random());
			fldname = "comment";
			break;

		case logon_script:
			continue_if_field_set(r->in.logon_script);
			logonscript = logon_scripts[random() % ARRAY_SIZE(logon_scripts)];
			r->in.logon_script = talloc_strdup(mem_ctx, logonscript);
			fldname = "logon_script";
			break;
			
		case profile_path:
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
		
		printf(((i < num_changes - 1) ? "%s," : "%s"), fldname);

		/* disable requested field (it's supposed to be the only one used) */
		if (req_change != none) req_change = none;
	}

	printf("]\n");
}


#define TEST_STR_FLD(fld) \
	if (!strequal(req.in.fld, user_req.out.fld)) { \
		printf("failed to change '%s'\n", #fld); \
		ret = false; \
		goto cleanup; \
	}

#define TEST_TIME_FLD(fld) \
	if (timeval_compare(req.in.fld, user_req.out.fld)) { \
		printf("failed to change '%s'\n", #fld); \
		ret = false; \
		goto cleanup; \
	}

#define TEST_NUM_FLD(fld) \
	if (req.in.fld != user_req.out.fld) { \
		printf("failed to change '%s'\n", #fld); \
		ret = false; \
		goto cleanup; \
	}


bool torture_modifyuser(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_binding *binding;
	struct dcerpc_pipe *p;
	TALLOC_CTX *prep_mem_ctx;
	struct policy_handle h;
	struct lsa_String domain_name;
	char *name;
	struct libnet_context *ctx;
	struct libnet_ModifyUser req;
	struct libnet_UserInfo user_req;
	int fld;
	bool ret = true;

	prep_mem_ctx = talloc_init("prepare test_deleteuser");

	ctx = libnet_context_init(torture->ev, torture->lp_ctx);
	ctx->cred = cmdline_credentials;

	status = torture_rpc_connection(torture,
					&p,
					&ndr_table_samr);
	if (!NT_STATUS_IS_OK(status)) {
		ret = false;
		goto done;
	}

	name = talloc_strdup(prep_mem_ctx, TEST_USERNAME);

	domain_name.string = lp_workgroup(torture->lp_ctx);
	if (!test_opendomain(p, prep_mem_ctx, &h, &domain_name)) {
		ret = false;
		goto done;
	}

	if (!test_createuser(p, prep_mem_ctx, &h, name)) {
		ret = false;
		goto done;
	}

	status = torture_rpc_binding(torture, &binding);
	if (!NT_STATUS_IS_OK(status)) {
		ret = false;
		goto done;
	}

	printf("Testing change of all fields - each single one in turn\n");

	for (fld = 1; fld < FIELDS_NUM - 1; fld++) {
		ZERO_STRUCT(req);
		req.in.domain_name = lp_workgroup(torture->lp_ctx);
		req.in.user_name = name;

		set_test_changes(torture, &req, 1, &name, fld);

		status = libnet_ModifyUser(ctx, torture, &req);
		if (!NT_STATUS_IS_OK(status)) {
			printf("libnet_ModifyUser call failed: %s\n", nt_errstr(status));
			ret = false;
			continue;
		}

		ZERO_STRUCT(user_req);
		user_req.in.domain_name = lp_workgroup(torture->lp_ctx);
		user_req.in.data.user_name = name;
		user_req.in.level = USER_INFO_BY_NAME;

		status = libnet_UserInfo(ctx, torture, &user_req);
		if (!NT_STATUS_IS_OK(status)) {
			printf("libnet_UserInfo call failed: %s\n", nt_errstr(status));
			ret = false;
			continue;
		}

		switch (fld) {
		case account_name: TEST_STR_FLD(account_name);
			break;
		case full_name: TEST_STR_FLD(full_name);
			break;
		case comment: TEST_STR_FLD(comment);
			break;
		case description: TEST_STR_FLD(description);
			break;
		case home_directory: TEST_STR_FLD(home_directory);
			break;
		case home_drive: TEST_STR_FLD(home_drive);
			break;
		case logon_script: TEST_STR_FLD(logon_script);
			break;
		case profile_path: TEST_STR_FLD(profile_path);
			break;
		case acct_expiry: TEST_TIME_FLD(acct_expiry);
			break;
		case acct_flags: TEST_NUM_FLD(acct_flags);
			break;
		default:
			break;
		}

		if (fld == account_name) {
			/* restore original testing username - it's useful when test fails
			   because it prevents from problems with recreating account */
			ZERO_STRUCT(req);
			req.in.domain_name = lp_workgroup(torture->lp_ctx);
			req.in.user_name = name;
			req.in.account_name = TEST_USERNAME;
			
			status = libnet_ModifyUser(ctx, torture, &req);
			if (!NT_STATUS_IS_OK(status)) {
				printf("libnet_ModifyUser call failed: %s\n", nt_errstr(status));
				ret = false;
				goto done;
			}
			
			name = talloc_strdup(torture, TEST_USERNAME);
		}
	}

cleanup:
	if (!test_cleanup(ctx->samr.pipe, torture, &ctx->samr.handle, name)) {
		printf("cleanup failed\n");
		ret = false;
		goto done;
	}

	if (!test_samr_close(ctx->samr.pipe, torture, &ctx->samr.handle)) {
		printf("domain close failed\n");
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
	struct libnet_context *ctx;
	struct dcerpc_pipe *p;
	struct policy_handle h;
	struct lsa_String domain_name;
	struct libnet_UserInfo req;

	prep_mem_ctx = talloc_init("prepare torture user info");

	ctx = libnet_context_init(torture->ev, torture->lp_ctx);
	ctx->cred = cmdline_credentials;

	status = torture_rpc_connection(torture,
					&p,
					&ndr_table_samr);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	domain_name.string = lp_workgroup(torture->lp_ctx);
	if (!test_opendomain(p, prep_mem_ctx, &h, &domain_name)) {
		ret = false;
		goto done;
	}

	if (!test_createuser(p, prep_mem_ctx, &h, name)) {
		ret = false;
		goto done;
	}

	mem_ctx = talloc_init("torture user info");

	ZERO_STRUCT(req);
	
	req.in.domain_name = domain_name.string;
	req.in.data.user_name   = name;
	req.in.level = USER_INFO_BY_NAME;

	status = libnet_UserInfo(ctx, mem_ctx, &req);
	if (!NT_STATUS_IS_OK(status)) {
		printf("libnet_UserInfo call failed: %s\n", nt_errstr(status));
		ret = false;
		talloc_free(mem_ctx);
		goto done;
	}

	if (!test_cleanup(ctx->samr.pipe, mem_ctx, &ctx->samr.handle, TEST_USERNAME)) {
		printf("cleanup failed\n");
		ret = false;
		goto done;
	}

	if (!test_samr_close(ctx->samr.pipe, mem_ctx, &ctx->samr.handle)) {
		printf("domain close failed\n");
		ret = false;
	}

	talloc_free(ctx);

done:
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

	domain_name.string = lp_workgroup(torture->lp_ctx);
	mem_ctx = talloc_init("torture user list");

	ZERO_STRUCT(req);

	printf("listing user accounts:\n");
	
	do {

		req.in.domain_name = domain_name.string;
		req.in.page_size   = 128;
		req.in.resume_index = req.out.resume_index;

		status = libnet_UserList(ctx, mem_ctx, &req);
		if (!NT_STATUS_IS_OK(status) &&
		    !NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES)) break;

		for (i = 0; i < req.out.count; i++) {
			printf("\tuser: %s, sid=%s\n",
			       req.out.users[i].username, req.out.users[i].sid);
		}

	} while (NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES));

	if (!(NT_STATUS_IS_OK(status) ||
	      NT_STATUS_EQUAL(status, NT_STATUS_NO_MORE_ENTRIES))) {
		printf("libnet_UserList call failed: %s\n", nt_errstr(status));
		ret = false;
		goto done;
	}

	if (!test_samr_close(ctx->samr.pipe, mem_ctx, &ctx->samr.handle)) {
		printf("samr domain close failed\n");
		ret = false;
		goto done;
	}

	if (!test_lsa_close(ctx->lsa.pipe, mem_ctx, &ctx->lsa.handle)) {
		printf("lsa domain close failed\n");
		ret = false;
	}

	talloc_free(ctx);

done:
	talloc_free(mem_ctx);
	return ret;
}
