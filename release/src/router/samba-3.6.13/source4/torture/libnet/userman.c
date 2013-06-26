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
#include "torture/rpc/torture_rpc.h"
#include "torture/libnet/usertest.h"
#include "libnet/libnet.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "param/param.h"

#include "torture/libnet/proto.h"


static bool test_useradd(struct torture_context *tctx,
			 struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			 struct policy_handle *domain_handle,
			 const char *name)
{
	NTSTATUS status;
	bool ret = true;
	struct libnet_rpc_useradd user;

	user.in.domain_handle = *domain_handle;
	user.in.username      = name;

	torture_comment(tctx, "Testing libnet_rpc_useradd\n");

	status = libnet_rpc_useradd(p, mem_ctx, &user);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "Failed to call libnet_rpc_useradd - %s\n", nt_errstr(status));
		return false;
	}

	return ret;
}


static bool test_useradd_async(struct torture_context *tctx,
			       struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			       struct policy_handle *handle, const char* username)
{
	NTSTATUS status;
	struct composite_context *c;
	struct libnet_rpc_useradd user;

	user.in.domain_handle = *handle;
	user.in.username      = username;

	torture_comment(tctx, "Testing async libnet_rpc_useradd\n");

	c = libnet_rpc_useradd_send(p, &user, msg_handler);
	if (!c) {
		torture_comment(tctx, "Failed to call async libnet_rpc_useradd\n");
		return false;
	}

	status = libnet_rpc_useradd_recv(c, mem_ctx, &user);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "Calling async libnet_rpc_useradd failed - %s\n", nt_errstr(status));
		return false;
	}

	return true;

}

static bool test_usermod(struct torture_context *tctx, struct dcerpc_pipe *p,
			 TALLOC_CTX *mem_ctx,
			 struct policy_handle *handle, int num_changes,
			 struct libnet_rpc_usermod *mod, char **username)
{
	const char* logon_scripts[] = { "start_login.cmd", "login.bat", "start.cmd" };
	const char* home_dirs[] = { "\\\\srv\\home", "\\\\homesrv\\home\\user", "\\\\pdcsrv\\domain" };
	const char* home_drives[] = { "H:", "z:", "I:", "J:", "n:" };
	const char *homedir, *homedrive, *logonscript;
	const uint32_t flags[] = { (ACB_DISABLED | ACB_NORMAL | ACB_PW_EXPIRED),
				   (ACB_NORMAL | ACB_PWNOEXP),
				   (ACB_NORMAL | ACB_PW_EXPIRED) };

	NTSTATUS status;
	struct timeval now;
	enum test_fields testfld;
	int i;

	ZERO_STRUCT(*mod);
	srandom((unsigned)time(NULL));

	mod->in.username = talloc_strdup(mem_ctx, *username);
	mod->in.domain_handle = *handle;

	torture_comment(tctx, "modifying user (%d simultaneous change(s))\n",
			num_changes);

	torture_comment(tctx, "fields to change: [");

	for (i = 0; i < num_changes && i <= USER_FIELD_LAST; i++) {
		const char *fldname;

		testfld = (random() % USER_FIELD_LAST) + 1;

		GetTimeOfDay(&now);

		switch (testfld) {
		case acct_name:
			continue_if_field_set(mod->in.change.account_name);
			mod->in.change.account_name = talloc_asprintf(mem_ctx, TEST_CHG_ACCOUNTNAME,
								      (int)(random() % 100));
			mod->in.change.fields |= USERMOD_FIELD_ACCOUNT_NAME;
			fldname = "account_name";
			*username = talloc_strdup(mem_ctx, mod->in.change.account_name);
			break;

		case acct_full_name:
			continue_if_field_set(mod->in.change.full_name);
			mod->in.change.full_name = talloc_asprintf(mem_ctx, TEST_CHG_FULLNAME,
								  (int)random(), (int)random());
			mod->in.change.fields |= USERMOD_FIELD_FULL_NAME;
			fldname = "full_name";
			break;

		case acct_description:
			continue_if_field_set(mod->in.change.description);
			mod->in.change.description = talloc_asprintf(mem_ctx, TEST_CHG_DESCRIPTION,
								    random());
			mod->in.change.fields |= USERMOD_FIELD_DESCRIPTION;
			fldname = "description";
			break;

		case acct_home_directory:
			continue_if_field_set(mod->in.change.home_directory);
			homedir = home_dirs[random() % (sizeof(home_dirs)/sizeof(char*))];
			mod->in.change.home_directory = talloc_strdup(mem_ctx, homedir);
			mod->in.change.fields |= USERMOD_FIELD_HOME_DIRECTORY;
			fldname = "home_directory";
			break;

		case acct_home_drive:
			continue_if_field_set(mod->in.change.home_drive);
			homedrive = home_drives[random() % (sizeof(home_drives)/sizeof(char*))];
			mod->in.change.home_drive = talloc_strdup(mem_ctx, homedrive);
			mod->in.change.fields |= USERMOD_FIELD_HOME_DRIVE;
			fldname = "home_drive";
			break;

		case acct_comment:
			continue_if_field_set(mod->in.change.comment);
			mod->in.change.comment = talloc_asprintf(mem_ctx, TEST_CHG_COMMENT,
								random(), random());
			mod->in.change.fields |= USERMOD_FIELD_COMMENT;
			fldname = "comment";
			break;

		case acct_logon_script:
			continue_if_field_set(mod->in.change.logon_script);
			logonscript = logon_scripts[random() % (sizeof(logon_scripts)/sizeof(char*))];
			mod->in.change.logon_script = talloc_strdup(mem_ctx, logonscript);
			mod->in.change.fields |= USERMOD_FIELD_LOGON_SCRIPT;
			fldname = "logon_script";
			break;

		case acct_profile_path:
			continue_if_field_set(mod->in.change.profile_path);
			mod->in.change.profile_path = talloc_asprintf(mem_ctx, TEST_CHG_PROFILEPATH,
								     (long int)random(), (unsigned int)random());
			mod->in.change.fields |= USERMOD_FIELD_PROFILE_PATH;
			fldname = "profile_path";
			break;

		case acct_expiry:
			continue_if_field_set(mod->in.change.acct_expiry);
			now = timeval_add(&now, (random() % (31*24*60*60)), 0);
			mod->in.change.acct_expiry = (struct timeval *)talloc_memdup(mem_ctx, &now, sizeof(now));
			mod->in.change.fields |= USERMOD_FIELD_ACCT_EXPIRY;
			fldname = "acct_expiry";
			break;

		case acct_flags:
			continue_if_field_set(mod->in.change.acct_flags);
			mod->in.change.acct_flags = flags[random() % ARRAY_SIZE(flags)];
			mod->in.change.fields |= USERMOD_FIELD_ACCT_FLAGS;
			fldname = "acct_flags";
			break;

		default:
			fldname = talloc_asprintf(mem_ctx, "unknown_field (%d)", testfld);
			break;
		}

		torture_comment(tctx, ((i < num_changes - 1) ? "%s," : "%s"), fldname);
	}
	torture_comment(tctx, "]\n");

	status = libnet_rpc_usermod(p, mem_ctx, mod);
	torture_assert_ntstatus_ok(tctx, status, "Failed to call sync libnet_rpc_usermod");

	return true;
}


static bool test_userdel(struct torture_context *tctx,
			 struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			 struct policy_handle *handle, const char *username)
{
	NTSTATUS status;
	struct libnet_rpc_userdel user;

	user.in.domain_handle = *handle;
	user.in.username = username;

	status = libnet_rpc_userdel(p, mem_ctx, &user);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "Failed to call sync libnet_rpc_userdel - %s\n", nt_errstr(status));
		return false;
	}

	return true;
}


#define CMP_LSA_STRING_FLD(fld, flags) \
	if ((mod->in.change.fields & flags) && \
	    !strequal(i->fld.string, mod->in.change.fld)) { \
		torture_comment(tctx, "'%s' field does not match\n", #fld); \
		torture_comment(tctx, "received: '%s'\n", i->fld.string); \
		torture_comment(tctx, "expected: '%s'\n", mod->in.change.fld); \
		return false; \
	}


#define CMP_TIME_FLD(fld, flags) \
	if (mod->in.change.fields & flags) { \
		nttime_to_timeval(&t, i->fld); \
		if (timeval_compare(&t, mod->in.change.fld)) { \
			torture_comment(tctx, "'%s' field does not match\n", #fld); \
			torture_comment(tctx, "received: '%s (+%ld us)'\n", \
			       timestring(mem_ctx, t.tv_sec), t.tv_usec); \
			torture_comment(tctx, "expected: '%s (+%ld us)'\n", \
			       timestring(mem_ctx, mod->in.change.fld->tv_sec), \
			       mod->in.change.fld->tv_usec); \
			return false; \
		} \
	}

#define CMP_NUM_FLD(fld, flags) \
	if ((mod->in.change.fields & flags) && \
	    (i->fld != mod->in.change.fld)) { \
		torture_comment(tctx, "'%s' field does not match\n", #fld); \
		torture_comment(tctx, "received: '%04x'\n", i->fld); \
		torture_comment(tctx, "expected: '%04x'\n", mod->in.change.fld); \
		return false; \
	}


static bool test_compare(struct torture_context *tctx,
			 struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			 struct policy_handle *handle, struct libnet_rpc_usermod *mod,
			 const char *username)
{
	NTSTATUS status;
	struct libnet_rpc_userinfo info;
	struct samr_UserInfo21 *i;
	struct timeval t;

	ZERO_STRUCT(info);

	info.in.username = username;
	info.in.domain_handle = *handle;
	info.in.level = 21;             /* the most rich infolevel available */

	status = libnet_rpc_userinfo(p, mem_ctx, &info);
	torture_assert_ntstatus_ok(tctx, status, "Failed to call sync libnet_rpc_userinfo");

	i = &info.out.info.info21;

	CMP_LSA_STRING_FLD(account_name, USERMOD_FIELD_ACCOUNT_NAME);
	CMP_LSA_STRING_FLD(full_name, USERMOD_FIELD_FULL_NAME);
	CMP_LSA_STRING_FLD(description, USERMOD_FIELD_DESCRIPTION);
	CMP_LSA_STRING_FLD(comment, USERMOD_FIELD_COMMENT);
	CMP_LSA_STRING_FLD(logon_script, USERMOD_FIELD_LOGON_SCRIPT);
	CMP_LSA_STRING_FLD(profile_path, USERMOD_FIELD_PROFILE_PATH);
	CMP_LSA_STRING_FLD(home_directory, USERMOD_FIELD_HOME_DIRECTORY);
	CMP_LSA_STRING_FLD(home_drive, USERMOD_FIELD_HOME_DRIVE);
	CMP_TIME_FLD(acct_expiry, USERMOD_FIELD_ACCT_EXPIRY);
	CMP_NUM_FLD(acct_flags, USERMOD_FIELD_ACCT_FLAGS)

	return true;
}


bool torture_useradd(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p;
	struct policy_handle h;
	struct lsa_String domain_name;
	struct dom_sid2 sid;
	const char *name = TEST_USERNAME;
	TALLOC_CTX *mem_ctx;
	bool ret = true;
	struct dcerpc_binding_handle *b;

	mem_ctx = talloc_init("test_useradd");

	status = torture_rpc_connection(torture,
					&p,
					&ndr_table_samr);

	torture_assert_ntstatus_ok(torture, status, "RPC connect failed");
	b = p->binding_handle;

	domain_name.string = lpcfg_workgroup(torture->lp_ctx);
	if (!test_domain_open(torture, b, &domain_name, mem_ctx, &h, &sid)) {
		ret = false;
		goto done;
	}

	if (!test_useradd(torture, p, mem_ctx, &h, name)) {
		ret = false;
		goto done;
	}

	if (!test_user_cleanup(torture, b, mem_ctx, &h, name)) {
		ret = false;
		goto done;
	}

	if (!test_domain_open(torture, b, &domain_name, mem_ctx, &h, &sid)) {
		ret = false;
		goto done;
	}

	if (!test_useradd_async(torture, p, mem_ctx, &h, name)) {
		ret = false;
		goto done;
	}

	if (!test_user_cleanup(torture, b, mem_ctx, &h, name)) {
		ret = false;
		goto done;
	}

done:
	talloc_free(mem_ctx);
	return ret;
}


bool torture_userdel(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p;
	struct policy_handle h;
	struct lsa_String domain_name;
	struct dom_sid2 sid;
	uint32_t rid;
	const char *name = TEST_USERNAME;
	TALLOC_CTX *mem_ctx;
	bool ret = true;
	struct dcerpc_binding_handle *b;

	mem_ctx = talloc_init("test_userdel");

	status = torture_rpc_connection(torture,
					&p,
					&ndr_table_samr);

	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}
	b = p->binding_handle;

	domain_name.string = lpcfg_workgroup(torture->lp_ctx);
	if (!test_domain_open(torture, b, &domain_name, mem_ctx, &h, &sid)) {
		ret = false;
		goto done;
	}

	if (!test_user_create(torture, b, mem_ctx, &h, name, &rid)) {
		ret = false;
		goto done;
	}

	if (!test_userdel(torture, p, mem_ctx, &h, name)) {
		ret = false;
		goto done;
	}

done:
	talloc_free(mem_ctx);
	return ret;
}


bool torture_usermod(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p;
	struct policy_handle h;
	struct lsa_String domain_name;
	struct dom_sid2 sid;
	uint32_t rid;
	int i;
	char *name;
	TALLOC_CTX *mem_ctx;
	bool ret = true;
	struct dcerpc_binding_handle *b;

	mem_ctx = talloc_init("test_userdel");

	status = torture_rpc_connection(torture,
					&p,
					&ndr_table_samr);

	torture_assert_ntstatus_ok(torture, status, "RPC connect");
	b = p->binding_handle;

	domain_name.string = lpcfg_workgroup(torture->lp_ctx);
	name = talloc_strdup(mem_ctx, TEST_USERNAME);

	if (!test_domain_open(torture, b, &domain_name, mem_ctx, &h, &sid)) {
		ret = false;
		goto done;
	}

	if (!test_user_create(torture, b, mem_ctx, &h, name, &rid)) {
		ret = false;
		goto done;
	}

	for (i = USER_FIELD_FIRST; i <= USER_FIELD_LAST; i++) {
		struct libnet_rpc_usermod m;

		if (!test_usermod(torture, p, mem_ctx, &h, i, &m, &name)) {
			ret = false;
			goto cleanup;
		}

		if (!test_compare(torture, p, mem_ctx, &h, &m, name)) {
			ret = false;
			goto cleanup;
		}
	}

cleanup:
	if (!test_user_cleanup(torture, b, mem_ctx, &h, TEST_USERNAME)) {
		ret = false;
		goto done;
	}

done:
	talloc_free(mem_ctx);
	return ret;
}
