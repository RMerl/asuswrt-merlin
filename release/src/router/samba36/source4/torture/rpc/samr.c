/*
   Unix SMB/CIFS implementation.
   test suite for samr rpc operations

   Copyright (C) Andrew Tridgell 2003
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2003
   Copyright (C) Guenther Deschner 2008-2010

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
#include "torture/torture.h"
#include <tevent.h>
#include "system/time.h"
#include "librpc/gen_ndr/lsa.h"
#include "librpc/gen_ndr/ndr_netlogon.h"
#include "librpc/gen_ndr/ndr_netlogon_c.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "../lib/crypto/crypto.h"
#include "libcli/auth/libcli_auth.h"
#include "libcli/security/security.h"
#include "torture/rpc/torture_rpc.h"
#include "param/param.h"
#include "auth/gensec/gensec.h"
#include "auth/gensec/gensec_proto.h"
#include "../libcli/auth/schannel.h"

#include <unistd.h>

#define TEST_ACCOUNT_NAME "samrtorturetest"
#define TEST_ACCOUNT_NAME_PWD "samrpwdlastset"
#define TEST_ALIASNAME "samrtorturetestalias"
#define TEST_GROUPNAME "samrtorturetestgroup"
#define TEST_MACHINENAME "samrtestmach$"
#define TEST_DOMAINNAME "samrtestdom$"

enum torture_samr_choice {
	TORTURE_SAMR_PASSWORDS,
	TORTURE_SAMR_PASSWORDS_PWDLASTSET,
	TORTURE_SAMR_PASSWORDS_BADPWDCOUNT,
	TORTURE_SAMR_PASSWORDS_LOCKOUT,
	TORTURE_SAMR_USER_ATTRIBUTES,
	TORTURE_SAMR_USER_PRIVILEGES,
	TORTURE_SAMR_OTHER,
	TORTURE_SAMR_MANY_ACCOUNTS,
	TORTURE_SAMR_MANY_GROUPS,
	TORTURE_SAMR_MANY_ALIASES
};

struct torture_samr_context {
	struct policy_handle handle;
	struct cli_credentials *machine_credentials;
	enum torture_samr_choice choice;
	uint32_t num_objects_large_dc;
};

static bool test_QueryUserInfo(struct dcerpc_binding_handle *b,
			       struct torture_context *tctx,
			       struct policy_handle *handle);

static bool test_QueryUserInfo2(struct dcerpc_binding_handle *b,
				struct torture_context *tctx,
				struct policy_handle *handle);

static bool test_QueryAliasInfo(struct dcerpc_binding_handle *b,
				struct torture_context *tctx,
				struct policy_handle *handle);

static bool test_ChangePassword(struct dcerpc_pipe *p,
				struct torture_context *tctx,
				const char *acct_name,
				struct policy_handle *domain_handle, char **password);

static void init_lsa_String(struct lsa_String *string, const char *s)
{
	string->string = s;
}

static void init_lsa_StringLarge(struct lsa_StringLarge *string, const char *s)
{
	string->string = s;
}

static void init_lsa_BinaryString(struct lsa_BinaryString *string, const char *s, uint32_t length)
{
	string->length = length;
	string->size = length;
	string->array = (uint16_t *)discard_const(s);
}

bool test_samr_handle_Close(struct dcerpc_binding_handle *b,
			    struct torture_context *tctx,
			    struct policy_handle *handle)
{
	struct samr_Close r;

	r.in.handle = handle;
	r.out.handle = handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_Close_r(b, tctx, &r),
		"Close failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "Close failed");

	return true;
}

static bool test_Shutdown(struct dcerpc_binding_handle *b,
			  struct torture_context *tctx,
			  struct policy_handle *handle)
{
	struct samr_Shutdown r;

	if (!torture_setting_bool(tctx, "dangerous", false)) {
		torture_skip(tctx, "samr_Shutdown disabled - enable dangerous tests to use\n");
		return true;
	}

	r.in.connect_handle = handle;

	torture_comment(tctx, "Testing samr_Shutdown\n");

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_Shutdown_r(b, tctx, &r),
		"Shutdown failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "Shutdown failed");

	return true;
}

static bool test_SetDsrmPassword(struct dcerpc_binding_handle *b,
				 struct torture_context *tctx,
				 struct policy_handle *handle)
{
	struct samr_SetDsrmPassword r;
	struct lsa_String string;
	struct samr_Password hash;

	if (!torture_setting_bool(tctx, "dangerous", false)) {
		torture_skip(tctx, "samr_SetDsrmPassword disabled - enable dangerous tests to use");
	}

	E_md4hash("TeSTDSRM123", hash.hash);

	init_lsa_String(&string, "Administrator");

	r.in.name = &string;
	r.in.unknown = 0;
	r.in.hash = &hash;

	torture_comment(tctx, "Testing samr_SetDsrmPassword\n");

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetDsrmPassword_r(b, tctx, &r),
		"SetDsrmPassword failed");
	torture_assert_ntstatus_equal(tctx, r.out.result, NT_STATUS_NOT_SUPPORTED, "SetDsrmPassword failed");

	return true;
}


static bool test_QuerySecurity(struct dcerpc_binding_handle *b,
			       struct torture_context *tctx,
			       struct policy_handle *handle)
{
	struct samr_QuerySecurity r;
	struct samr_SetSecurity s;
	struct sec_desc_buf *sdbuf = NULL;

	r.in.handle = handle;
	r.in.sec_info = 7;
	r.out.sdbuf = &sdbuf;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_QuerySecurity_r(b, tctx, &r),
		"QuerySecurity failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "QuerySecurity failed");

	torture_assert(tctx, sdbuf != NULL, "sdbuf is NULL");

	s.in.handle = handle;
	s.in.sec_info = 7;
	s.in.sdbuf = sdbuf;

	if (torture_setting_bool(tctx, "samba4", false)) {
		torture_skip(tctx, "skipping SetSecurity test against Samba4\n");
	}

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetSecurity_r(b, tctx, &s),
		"SetSecurity failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "SetSecurity failed");

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_QuerySecurity_r(b, tctx, &r),
		"QuerySecurity failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "QuerySecurity failed");

	return true;
}


static bool test_SetUserInfo(struct dcerpc_binding_handle *b, struct torture_context *tctx,
			     struct policy_handle *handle, uint32_t base_acct_flags,
			     const char *base_account_name)
{
	struct samr_SetUserInfo s;
	struct samr_SetUserInfo2 s2;
	struct samr_QueryUserInfo q;
	struct samr_QueryUserInfo q0;
	union samr_UserInfo u;
	union samr_UserInfo *info;
	bool ret = true;
	const char *test_account_name;

	uint32_t user_extra_flags = 0;

	if (!torture_setting_bool(tctx, "samba3", false)) {
		if (base_acct_flags == ACB_NORMAL) {
			/* When created, accounts are expired by default */
			user_extra_flags = ACB_PW_EXPIRED;
		}
	}

	s.in.user_handle = handle;
	s.in.info = &u;

	s2.in.user_handle = handle;
	s2.in.info = &u;

	q.in.user_handle = handle;
	q.out.info = &info;
	q0 = q;

#define TESTCALL(call, r) \
		torture_assert_ntstatus_ok(tctx, dcerpc_samr_ ##call## _r(b, tctx, &r),\
			#call " failed"); \
		if (!NT_STATUS_IS_OK(r.out.result)) { \
			torture_comment(tctx, #call " level %u failed - %s (%s)\n", \
			       r.in.level, nt_errstr(r.out.result), __location__); \
			ret = false; \
			break; \
		}

#define STRING_EQUAL(s1, s2, field) \
		if ((s1 && !s2) || (s2 && !s1) || strcmp(s1, s2)) { \
			torture_comment(tctx, "Failed to set %s to '%s' (%s)\n", \
			       #field, s2, __location__); \
			ret = false; \
			break; \
		}

#define MEM_EQUAL(s1, s2, length, field) \
		if ((s1 && !s2) || (s2 && !s1) || memcmp(s1, s2, length)) { \
			torture_comment(tctx, "Failed to set %s to '%s' (%s)\n", \
			       #field, (const char *)s2, __location__); \
			ret = false; \
			break; \
		}

#define INT_EQUAL(i1, i2, field) \
		if (i1 != i2) { \
			torture_comment(tctx, "Failed to set %s to 0x%llx - got 0x%llx (%s)\n", \
			       #field, (unsigned long long)i2, (unsigned long long)i1, __location__); \
			ret = false; \
			break; \
		}

#define TEST_USERINFO_STRING(lvl1, field1, lvl2, field2, value, fpval) do { \
		torture_comment(tctx, "field test %d/%s vs %d/%s\n", lvl1, #field1, lvl2, #field2); \
		q.in.level = lvl1; \
		TESTCALL(QueryUserInfo, q) \
		s.in.level = lvl1; \
		s2.in.level = lvl1; \
		u = *info; \
		if (lvl1 == 21) { \
			ZERO_STRUCT(u.info21); \
			u.info21.fields_present = fpval; \
		} \
		init_lsa_String(&u.info ## lvl1.field1, value); \
		TESTCALL(SetUserInfo, s) \
		TESTCALL(SetUserInfo2, s2) \
		init_lsa_String(&u.info ## lvl1.field1, ""); \
		TESTCALL(QueryUserInfo, q); \
		u = *info; \
		STRING_EQUAL(u.info ## lvl1.field1.string, value, field1); \
		q.in.level = lvl2; \
		TESTCALL(QueryUserInfo, q) \
		u = *info; \
		STRING_EQUAL(u.info ## lvl2.field2.string, value, field2); \
	} while (0)

#define TEST_USERINFO_BINARYSTRING(lvl1, field1, lvl2, field2, value, fpval) do { \
		torture_comment(tctx, "field test %d/%s vs %d/%s\n", lvl1, #field1, lvl2, #field2); \
		q.in.level = lvl1; \
		TESTCALL(QueryUserInfo, q) \
		s.in.level = lvl1; \
		s2.in.level = lvl1; \
		u = *info; \
		if (lvl1 == 21) { \
			ZERO_STRUCT(u.info21); \
			u.info21.fields_present = fpval; \
		} \
		init_lsa_BinaryString(&u.info ## lvl1.field1, value, strlen(value)); \
		TESTCALL(SetUserInfo, s) \
		TESTCALL(SetUserInfo2, s2) \
		init_lsa_BinaryString(&u.info ## lvl1.field1, "", 1); \
		TESTCALL(QueryUserInfo, q); \
		u = *info; \
		MEM_EQUAL(u.info ## lvl1.field1.array, value, strlen(value), field1); \
		q.in.level = lvl2; \
		TESTCALL(QueryUserInfo, q) \
		u = *info; \
		MEM_EQUAL(u.info ## lvl2.field2.array, value, strlen(value), field2); \
	} while (0)

#define TEST_USERINFO_INT_EXP(lvl1, field1, lvl2, field2, value, exp_value, fpval) do { \
		torture_comment(tctx, "field test %d/%s vs %d/%s\n", lvl1, #field1, lvl2, #field2); \
		q.in.level = lvl1; \
		TESTCALL(QueryUserInfo, q) \
		s.in.level = lvl1; \
		s2.in.level = lvl1; \
		u = *info; \
		if (lvl1 == 21) { \
			uint8_t *bits = u.info21.logon_hours.bits; \
			ZERO_STRUCT(u.info21); \
			if (fpval == SAMR_FIELD_LOGON_HOURS) { \
				u.info21.logon_hours.units_per_week = 168; \
				u.info21.logon_hours.bits = bits; \
			} \
			u.info21.fields_present = fpval; \
		} \
		u.info ## lvl1.field1 = value; \
		TESTCALL(SetUserInfo, s) \
		TESTCALL(SetUserInfo2, s2) \
		u.info ## lvl1.field1 = 0; \
		TESTCALL(QueryUserInfo, q); \
		u = *info; \
		INT_EQUAL(u.info ## lvl1.field1, exp_value, field1); \
		q.in.level = lvl2; \
		TESTCALL(QueryUserInfo, q) \
		u = *info; \
		INT_EQUAL(u.info ## lvl2.field2, exp_value, field1); \
	} while (0)

#define TEST_USERINFO_INT(lvl1, field1, lvl2, field2, value, fpval) do { \
        TEST_USERINFO_INT_EXP(lvl1, field1, lvl2, field2, value, value, fpval); \
        } while (0)

	q0.in.level = 12;
	do { TESTCALL(QueryUserInfo, q0) } while (0);

	TEST_USERINFO_STRING(2, comment,  1, comment, "xx2-1 comment", 0);
	TEST_USERINFO_STRING(2, comment, 21, comment, "xx2-21 comment", 0);
	TEST_USERINFO_STRING(21, comment, 21, comment, "xx21-21 comment",
			   SAMR_FIELD_COMMENT);

	test_account_name = talloc_asprintf(tctx, "%sxx7-1", base_account_name);
	TEST_USERINFO_STRING(7, account_name,  1, account_name, base_account_name, 0);
	test_account_name = talloc_asprintf(tctx, "%sxx7-3", base_account_name);
	TEST_USERINFO_STRING(7, account_name,  3, account_name, base_account_name, 0);
	test_account_name = talloc_asprintf(tctx, "%sxx7-5", base_account_name);
	TEST_USERINFO_STRING(7, account_name,  5, account_name, base_account_name, 0);
	test_account_name = talloc_asprintf(tctx, "%sxx7-6", base_account_name);
	TEST_USERINFO_STRING(7, account_name,  6, account_name, base_account_name, 0);
	test_account_name = talloc_asprintf(tctx, "%sxx7-7", base_account_name);
	TEST_USERINFO_STRING(7, account_name,  7, account_name, base_account_name, 0);
	test_account_name = talloc_asprintf(tctx, "%sxx7-21", base_account_name);
	TEST_USERINFO_STRING(7, account_name, 21, account_name, base_account_name, 0);
	test_account_name = base_account_name;
	TEST_USERINFO_STRING(21, account_name, 21, account_name, base_account_name,
			   SAMR_FIELD_ACCOUNT_NAME);

	TEST_USERINFO_STRING(6, full_name,  1, full_name, "xx6-1 full_name", 0);
	TEST_USERINFO_STRING(6, full_name,  3, full_name, "xx6-3 full_name", 0);
	TEST_USERINFO_STRING(6, full_name,  5, full_name, "xx6-5 full_name", 0);
	TEST_USERINFO_STRING(6, full_name,  6, full_name, "xx6-6 full_name", 0);
	TEST_USERINFO_STRING(6, full_name,  8, full_name, "xx6-8 full_name", 0);
	TEST_USERINFO_STRING(6, full_name, 21, full_name, "xx6-21 full_name", 0);
	TEST_USERINFO_STRING(8, full_name, 21, full_name, "xx8-21 full_name", 0);
	TEST_USERINFO_STRING(21, full_name, 21, full_name, "xx21-21 full_name",
			   SAMR_FIELD_FULL_NAME);

	TEST_USERINFO_STRING(6, full_name,  1, full_name, "", 0);
	TEST_USERINFO_STRING(6, full_name,  3, full_name, "", 0);
	TEST_USERINFO_STRING(6, full_name,  5, full_name, "", 0);
	TEST_USERINFO_STRING(6, full_name,  6, full_name, "", 0);
	TEST_USERINFO_STRING(6, full_name,  8, full_name, "", 0);
	TEST_USERINFO_STRING(6, full_name, 21, full_name, "", 0);
	TEST_USERINFO_STRING(8, full_name, 21, full_name, "", 0);
	TEST_USERINFO_STRING(21, full_name, 21, full_name, "",
			   SAMR_FIELD_FULL_NAME);

	TEST_USERINFO_STRING(11, logon_script, 3, logon_script, "xx11-3 logon_script", 0);
	TEST_USERINFO_STRING(11, logon_script, 5, logon_script, "xx11-5 logon_script", 0);
	TEST_USERINFO_STRING(11, logon_script, 21, logon_script, "xx11-21 logon_script", 0);
	TEST_USERINFO_STRING(21, logon_script, 21, logon_script, "xx21-21 logon_script",
			   SAMR_FIELD_LOGON_SCRIPT);

	TEST_USERINFO_STRING(12, profile_path,  3, profile_path, "xx12-3 profile_path", 0);
	TEST_USERINFO_STRING(12, profile_path,  5, profile_path, "xx12-5 profile_path", 0);
	TEST_USERINFO_STRING(12, profile_path, 21, profile_path, "xx12-21 profile_path", 0);
	TEST_USERINFO_STRING(21, profile_path, 21, profile_path, "xx21-21 profile_path",
			   SAMR_FIELD_PROFILE_PATH);

	TEST_USERINFO_STRING(10, home_directory, 3, home_directory, "xx10-3 home_directory", 0);
	TEST_USERINFO_STRING(10, home_directory, 5, home_directory, "xx10-5 home_directory", 0);
	TEST_USERINFO_STRING(10, home_directory, 21, home_directory, "xx10-21 home_directory", 0);
	TEST_USERINFO_STRING(21, home_directory, 21, home_directory, "xx21-21 home_directory",
			     SAMR_FIELD_HOME_DIRECTORY);
	TEST_USERINFO_STRING(21, home_directory, 10, home_directory, "xx21-10 home_directory",
			     SAMR_FIELD_HOME_DIRECTORY);

	TEST_USERINFO_STRING(10, home_drive, 3, home_drive, "xx10-3 home_drive", 0);
	TEST_USERINFO_STRING(10, home_drive, 5, home_drive, "xx10-5 home_drive", 0);
	TEST_USERINFO_STRING(10, home_drive, 21, home_drive, "xx10-21 home_drive", 0);
	TEST_USERINFO_STRING(21, home_drive, 21, home_drive, "xx21-21 home_drive",
			     SAMR_FIELD_HOME_DRIVE);
	TEST_USERINFO_STRING(21, home_drive, 10, home_drive, "xx21-10 home_drive",
			     SAMR_FIELD_HOME_DRIVE);

	TEST_USERINFO_STRING(13, description,  1, description, "xx13-1 description", 0);
	TEST_USERINFO_STRING(13, description,  5, description, "xx13-5 description", 0);
	TEST_USERINFO_STRING(13, description, 21, description, "xx13-21 description", 0);
	TEST_USERINFO_STRING(21, description, 21, description, "xx21-21 description",
			   SAMR_FIELD_DESCRIPTION);

	TEST_USERINFO_STRING(14, workstations,  3, workstations, "14workstation3", 0);
	TEST_USERINFO_STRING(14, workstations,  5, workstations, "14workstation4", 0);
	TEST_USERINFO_STRING(14, workstations, 21, workstations, "14workstation21", 0);
	TEST_USERINFO_STRING(21, workstations, 21, workstations, "21workstation21",
			   SAMR_FIELD_WORKSTATIONS);
	TEST_USERINFO_STRING(21, workstations, 3, workstations, "21workstation3",
			   SAMR_FIELD_WORKSTATIONS);
	TEST_USERINFO_STRING(21, workstations, 5, workstations, "21workstation5",
			   SAMR_FIELD_WORKSTATIONS);
	TEST_USERINFO_STRING(21, workstations, 14, workstations, "21workstation14",
			   SAMR_FIELD_WORKSTATIONS);

	TEST_USERINFO_BINARYSTRING(20, parameters, 21, parameters, "xx20-21 parameters", 0);
	TEST_USERINFO_BINARYSTRING(21, parameters, 21, parameters, "xx21-21 parameters",
			   SAMR_FIELD_PARAMETERS);
	TEST_USERINFO_BINARYSTRING(21, parameters, 20, parameters, "xx21-20 parameters",
			   SAMR_FIELD_PARAMETERS);
	/* also empty user parameters are allowed */
	TEST_USERINFO_BINARYSTRING(20, parameters, 21, parameters, "", 0);
	TEST_USERINFO_BINARYSTRING(21, parameters, 21, parameters, "",
			   SAMR_FIELD_PARAMETERS);
	TEST_USERINFO_BINARYSTRING(21, parameters, 20, parameters, "",
			   SAMR_FIELD_PARAMETERS);

	/* Samba 3 cannot store country_code and code_page atm. - gd */
	if (!torture_setting_bool(tctx, "samba3", false)) {
		TEST_USERINFO_INT(2, country_code, 2, country_code, __LINE__, 0);
		TEST_USERINFO_INT(2, country_code, 21, country_code, __LINE__, 0);
		TEST_USERINFO_INT(21, country_code, 21, country_code, __LINE__,
				  SAMR_FIELD_COUNTRY_CODE);
		TEST_USERINFO_INT(21, country_code, 2, country_code, __LINE__,
				  SAMR_FIELD_COUNTRY_CODE);

		TEST_USERINFO_INT(2, code_page, 21, code_page, __LINE__, 0);
		TEST_USERINFO_INT(21, code_page, 21, code_page, __LINE__,
				  SAMR_FIELD_CODE_PAGE);
		TEST_USERINFO_INT(21, code_page, 2, code_page, __LINE__,
				  SAMR_FIELD_CODE_PAGE);
	}

	if (!torture_setting_bool(tctx, "samba3", false)) {
		TEST_USERINFO_INT(17, acct_expiry, 21, acct_expiry, __LINE__, 0);
		TEST_USERINFO_INT(17, acct_expiry, 5, acct_expiry, __LINE__, 0);
		TEST_USERINFO_INT(21, acct_expiry, 21, acct_expiry, __LINE__,
				  SAMR_FIELD_ACCT_EXPIRY);
		TEST_USERINFO_INT(21, acct_expiry, 5, acct_expiry, __LINE__,
				  SAMR_FIELD_ACCT_EXPIRY);
		TEST_USERINFO_INT(21, acct_expiry, 17, acct_expiry, __LINE__,
				  SAMR_FIELD_ACCT_EXPIRY);
	} else {
		/* Samba 3 can only store seconds / time_t in passdb - gd */
		NTTIME nt;
		unix_to_nt_time(&nt, time(NULL) + __LINE__);
		TEST_USERINFO_INT(17, acct_expiry, 21, acct_expiry, nt, 0);
		unix_to_nt_time(&nt, time(NULL) + __LINE__);
		TEST_USERINFO_INT(17, acct_expiry, 5, acct_expiry, nt, 0);
		unix_to_nt_time(&nt, time(NULL) + __LINE__);
		TEST_USERINFO_INT(21, acct_expiry, 21, acct_expiry, nt, SAMR_FIELD_ACCT_EXPIRY);
		unix_to_nt_time(&nt, time(NULL) + __LINE__);
		TEST_USERINFO_INT(21, acct_expiry, 5, acct_expiry, nt, SAMR_FIELD_ACCT_EXPIRY);
		unix_to_nt_time(&nt, time(NULL) + __LINE__);
		TEST_USERINFO_INT(21, acct_expiry, 17, acct_expiry, nt, SAMR_FIELD_ACCT_EXPIRY);
	}

	TEST_USERINFO_INT(4, logon_hours.bits[3],  3, logon_hours.bits[3], 1, 0);
	TEST_USERINFO_INT(4, logon_hours.bits[3],  5, logon_hours.bits[3], 2, 0);
	TEST_USERINFO_INT(4, logon_hours.bits[3], 21, logon_hours.bits[3], 3, 0);
	TEST_USERINFO_INT(21, logon_hours.bits[3], 21, logon_hours.bits[3], 4,
			  SAMR_FIELD_LOGON_HOURS);

	TEST_USERINFO_INT_EXP(16, acct_flags, 5, acct_flags,
			      (base_acct_flags  | ACB_DISABLED | ACB_HOMDIRREQ),
			      (base_acct_flags  | ACB_DISABLED | ACB_HOMDIRREQ | user_extra_flags),
			      0);
	TEST_USERINFO_INT_EXP(16, acct_flags, 5, acct_flags,
			      (base_acct_flags  | ACB_DISABLED),
			      (base_acct_flags  | ACB_DISABLED | user_extra_flags),
			      0);

	/* Setting PWNOEXP clears the magic ACB_PW_EXPIRED flag */
	TEST_USERINFO_INT_EXP(16, acct_flags, 5, acct_flags,
			      (base_acct_flags  | ACB_DISABLED | ACB_PWNOEXP),
			      (base_acct_flags  | ACB_DISABLED | ACB_PWNOEXP),
			      0);
	TEST_USERINFO_INT_EXP(16, acct_flags, 21, acct_flags,
			      (base_acct_flags | ACB_DISABLED | ACB_HOMDIRREQ),
			      (base_acct_flags | ACB_DISABLED | ACB_HOMDIRREQ | user_extra_flags),
			      0);


	/* The 'autolock' flag doesn't stick - check this */
	TEST_USERINFO_INT_EXP(16, acct_flags, 21, acct_flags,
			      (base_acct_flags | ACB_DISABLED | ACB_AUTOLOCK),
			      (base_acct_flags | ACB_DISABLED | user_extra_flags),
			      0);
#if 0
	/* Removing the 'disabled' flag doesn't stick - check this */
	TEST_USERINFO_INT_EXP(16, acct_flags, 21, acct_flags,
			      (base_acct_flags),
			      (base_acct_flags | ACB_DISABLED | user_extra_flags),
			      0);
#endif

	/* Samba3 cannot store these atm */
	if (!torture_setting_bool(tctx, "samba3", false)) {
	/* The 'store plaintext' flag does stick */
	TEST_USERINFO_INT_EXP(16, acct_flags, 21, acct_flags,
			      (base_acct_flags | ACB_DISABLED | ACB_ENC_TXT_PWD_ALLOWED),
			      (base_acct_flags | ACB_DISABLED | ACB_ENC_TXT_PWD_ALLOWED | user_extra_flags),
			      0);
	/* The 'use DES' flag does stick */
	TEST_USERINFO_INT_EXP(16, acct_flags, 21, acct_flags,
			      (base_acct_flags | ACB_DISABLED | ACB_USE_DES_KEY_ONLY),
			      (base_acct_flags | ACB_DISABLED | ACB_USE_DES_KEY_ONLY | user_extra_flags),
			      0);
	/* The 'don't require kerberos pre-authentication flag does stick */
	TEST_USERINFO_INT_EXP(16, acct_flags, 21, acct_flags,
			      (base_acct_flags | ACB_DISABLED | ACB_DONT_REQUIRE_PREAUTH),
			      (base_acct_flags | ACB_DISABLED | ACB_DONT_REQUIRE_PREAUTH | user_extra_flags),
			      0);
	/* The 'no kerberos PAC required' flag sticks */
	TEST_USERINFO_INT_EXP(16, acct_flags, 21, acct_flags,
			      (base_acct_flags | ACB_DISABLED | ACB_NO_AUTH_DATA_REQD),
			      (base_acct_flags | ACB_DISABLED | ACB_NO_AUTH_DATA_REQD | user_extra_flags),
			      0);
	}
	TEST_USERINFO_INT_EXP(21, acct_flags, 21, acct_flags,
			      (base_acct_flags | ACB_DISABLED),
			      (base_acct_flags | ACB_DISABLED | user_extra_flags),
			      SAMR_FIELD_ACCT_FLAGS);

#if 0
	/* these fail with win2003 - it appears you can't set the primary gid?
	   the set succeeds, but the gid isn't changed. Very weird! */
	TEST_USERINFO_INT(9, primary_gid,  1, primary_gid, 513);
	TEST_USERINFO_INT(9, primary_gid,  3, primary_gid, 513);
	TEST_USERINFO_INT(9, primary_gid,  5, primary_gid, 513);
	TEST_USERINFO_INT(9, primary_gid, 21, primary_gid, 513);
#endif

	return ret;
}

/*
  generate a random password for password change tests
*/
static char *samr_rand_pass_silent(TALLOC_CTX *mem_ctx, int min_len)
{
	size_t len = MAX(8, min_len);
	char *s = generate_random_password(mem_ctx, len, len+6);
	return s;
}

static char *samr_rand_pass(TALLOC_CTX *mem_ctx, int min_len)
{
	char *s = samr_rand_pass_silent(mem_ctx, min_len);
	printf("Generated password '%s'\n", s);
	return s;

}

/*
  generate a random password for password change tests
*/
static DATA_BLOB samr_very_rand_pass(TALLOC_CTX *mem_ctx, int len)
{
	int i;
	DATA_BLOB password = data_blob_talloc(mem_ctx, NULL, len * 2 /* number of unicode chars */);
	generate_random_buffer(password.data, password.length);

	for (i=0; i < len; i++) {
		if (((uint16_t *)password.data)[i] == 0) {
			((uint16_t *)password.data)[i] = 1;
		}
	}

	return password;
}

/*
  generate a random password for password change tests (fixed length)
*/
static char *samr_rand_pass_fixed_len(TALLOC_CTX *mem_ctx, int len)
{
	char *s = generate_random_password(mem_ctx, len, len);
	printf("Generated password '%s'\n", s);
	return s;
}

static bool test_SetUserPass(struct dcerpc_pipe *p, struct torture_context *tctx,
			     struct policy_handle *handle, char **password)
{
	NTSTATUS status;
	struct samr_SetUserInfo s;
	union samr_UserInfo u;
	bool ret = true;
	DATA_BLOB session_key;
	char *newpass;
	struct dcerpc_binding_handle *b = p->binding_handle;
	struct samr_GetUserPwInfo pwp;
	struct samr_PwInfo info;
	int policy_min_pw_len = 0;
	pwp.in.user_handle = handle;
	pwp.out.info = &info;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetUserPwInfo_r(b, tctx, &pwp),
		"GetUserPwInfo failed");
	if (NT_STATUS_IS_OK(pwp.out.result)) {
		policy_min_pw_len = pwp.out.info->min_password_length;
	}
	newpass = samr_rand_pass(tctx, policy_min_pw_len);

	s.in.user_handle = handle;
	s.in.info = &u;
	s.in.level = 24;

	encode_pw_buffer(u.info24.password.data, newpass, STR_UNICODE);
	u.info24.password_expired = 0;

	status = dcerpc_fetch_session_key(p, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(tctx, "SetUserInfo level %u - no session key - %s\n",
		       s.in.level, nt_errstr(status));
		return false;
	}

	arcfour_crypt_blob(u.info24.password.data, 516, &session_key);

	torture_comment(tctx, "Testing SetUserInfo level 24 (set password)\n");

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetUserInfo_r(b, tctx, &s),
		"SetUserInfo failed");
	if (!NT_STATUS_IS_OK(s.out.result)) {
		torture_warning(tctx, "SetUserInfo level %u failed - %s\n",
		       s.in.level, nt_errstr(s.out.result));
		ret = false;
	} else {
		*password = newpass;
	}

	return ret;
}


static bool test_SetUserPass_23(struct dcerpc_pipe *p, struct torture_context *tctx,
				struct policy_handle *handle, uint32_t fields_present,
				char **password)
{
	NTSTATUS status;
	struct samr_SetUserInfo s;
	union samr_UserInfo u;
	bool ret = true;
	DATA_BLOB session_key;
	struct dcerpc_binding_handle *b = p->binding_handle;
	char *newpass;
	struct samr_GetUserPwInfo pwp;
	struct samr_PwInfo info;
	int policy_min_pw_len = 0;
	pwp.in.user_handle = handle;
	pwp.out.info = &info;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetUserPwInfo_r(b, tctx, &pwp),
		"GetUserPwInfo failed");
	if (NT_STATUS_IS_OK(pwp.out.result)) {
		policy_min_pw_len = pwp.out.info->min_password_length;
	}
	newpass = samr_rand_pass(tctx, policy_min_pw_len);

	s.in.user_handle = handle;
	s.in.info = &u;
	s.in.level = 23;

	ZERO_STRUCT(u);

	u.info23.info.fields_present = fields_present;

	encode_pw_buffer(u.info23.password.data, newpass, STR_UNICODE);

	status = dcerpc_fetch_session_key(p, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(tctx, "SetUserInfo level %u - no session key - %s\n",
		       s.in.level, nt_errstr(status));
		return false;
	}

	arcfour_crypt_blob(u.info23.password.data, 516, &session_key);

	torture_comment(tctx, "Testing SetUserInfo level 23 (set password)\n");

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetUserInfo_r(b, tctx, &s),
		"SetUserInfo failed");
	if (!NT_STATUS_IS_OK(s.out.result)) {
		torture_warning(tctx, "SetUserInfo level %u failed - %s\n",
		       s.in.level, nt_errstr(s.out.result));
		ret = false;
	} else {
		*password = newpass;
	}

	encode_pw_buffer(u.info23.password.data, newpass, STR_UNICODE);

	status = dcerpc_fetch_session_key(p, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(tctx, "SetUserInfo level %u - no session key - %s\n",
		       s.in.level, nt_errstr(status));
		return false;
	}

	/* This should break the key nicely */
	session_key.length--;
	arcfour_crypt_blob(u.info23.password.data, 516, &session_key);

	torture_comment(tctx, "Testing SetUserInfo level 23 (set password) with wrong password\n");

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetUserInfo_r(b, tctx, &s),
		"SetUserInfo failed");
	if (!NT_STATUS_EQUAL(s.out.result, NT_STATUS_WRONG_PASSWORD)) {
		torture_warning(tctx, "SetUserInfo level %u should have failed with WRONG_PASSWORD- %s\n",
		       s.in.level, nt_errstr(s.out.result));
		ret = false;
	}

	return ret;
}


static bool test_SetUserPassEx(struct dcerpc_pipe *p, struct torture_context *tctx,
			       struct policy_handle *handle, bool makeshort,
			       char **password)
{
	NTSTATUS status;
	struct samr_SetUserInfo s;
	union samr_UserInfo u;
	bool ret = true;
	DATA_BLOB session_key;
	DATA_BLOB confounded_session_key = data_blob_talloc(tctx, NULL, 16);
	uint8_t confounder[16];
	char *newpass;
	struct dcerpc_binding_handle *b = p->binding_handle;
	MD5_CTX ctx;
	struct samr_GetUserPwInfo pwp;
	struct samr_PwInfo info;
	int policy_min_pw_len = 0;
	pwp.in.user_handle = handle;
	pwp.out.info = &info;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetUserPwInfo_r(b, tctx, &pwp),
		"GetUserPwInfo failed");
	if (NT_STATUS_IS_OK(pwp.out.result)) {
		policy_min_pw_len = pwp.out.info->min_password_length;
	}
	if (makeshort && policy_min_pw_len) {
		newpass = samr_rand_pass_fixed_len(tctx, policy_min_pw_len - 1);
	} else {
		newpass = samr_rand_pass(tctx, policy_min_pw_len);
	}

	s.in.user_handle = handle;
	s.in.info = &u;
	s.in.level = 26;

	encode_pw_buffer(u.info26.password.data, newpass, STR_UNICODE);
	u.info26.password_expired = 0;

	status = dcerpc_fetch_session_key(p, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(tctx, "SetUserInfo level %u - no session key - %s\n",
		       s.in.level, nt_errstr(status));
		return false;
	}

	generate_random_buffer((uint8_t *)confounder, 16);

	MD5Init(&ctx);
	MD5Update(&ctx, confounder, 16);
	MD5Update(&ctx, session_key.data, session_key.length);
	MD5Final(confounded_session_key.data, &ctx);

	arcfour_crypt_blob(u.info26.password.data, 516, &confounded_session_key);
	memcpy(&u.info26.password.data[516], confounder, 16);

	torture_comment(tctx, "Testing SetUserInfo level 26 (set password ex)\n");

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetUserInfo_r(b, tctx, &s),
		"SetUserInfo failed");
	if (!NT_STATUS_IS_OK(s.out.result)) {
		torture_warning(tctx, "SetUserInfo level %u failed - %s\n",
		       s.in.level, nt_errstr(s.out.result));
		ret = false;
	} else {
		*password = newpass;
	}

	/* This should break the key nicely */
	confounded_session_key.data[0]++;

	arcfour_crypt_blob(u.info26.password.data, 516, &confounded_session_key);
	memcpy(&u.info26.password.data[516], confounder, 16);

	torture_comment(tctx, "Testing SetUserInfo level 26 (set password ex) with wrong session key\n");

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetUserInfo_r(b, tctx, &s),
		"SetUserInfo failed");
	if (!NT_STATUS_EQUAL(s.out.result, NT_STATUS_WRONG_PASSWORD)) {
		torture_warning(tctx, "SetUserInfo level %u should have failed with WRONG_PASSWORD: %s\n",
		       s.in.level, nt_errstr(s.out.result));
		ret = false;
	} else {
		*password = newpass;
	}

	return ret;
}

static bool test_SetUserPass_25(struct dcerpc_pipe *p, struct torture_context *tctx,
				struct policy_handle *handle, uint32_t fields_present,
				char **password)
{
	NTSTATUS status;
	struct samr_SetUserInfo s;
	union samr_UserInfo u;
	bool ret = true;
	DATA_BLOB session_key;
	DATA_BLOB confounded_session_key = data_blob_talloc(tctx, NULL, 16);
	MD5_CTX ctx;
	uint8_t confounder[16];
	char *newpass;
	struct dcerpc_binding_handle *b = p->binding_handle;
	struct samr_GetUserPwInfo pwp;
	struct samr_PwInfo info;
	int policy_min_pw_len = 0;
	pwp.in.user_handle = handle;
	pwp.out.info = &info;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetUserPwInfo_r(b, tctx, &pwp),
		"GetUserPwInfo failed");
	if (NT_STATUS_IS_OK(pwp.out.result)) {
		policy_min_pw_len = pwp.out.info->min_password_length;
	}
	newpass = samr_rand_pass(tctx, policy_min_pw_len);

	s.in.user_handle = handle;
	s.in.info = &u;
	s.in.level = 25;

	ZERO_STRUCT(u);

	u.info25.info.fields_present = fields_present;

	encode_pw_buffer(u.info25.password.data, newpass, STR_UNICODE);

	status = dcerpc_fetch_session_key(p, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(tctx, "SetUserInfo level %u - no session key - %s\n",
		       s.in.level, nt_errstr(status));
		return false;
	}

	generate_random_buffer((uint8_t *)confounder, 16);

	MD5Init(&ctx);
	MD5Update(&ctx, confounder, 16);
	MD5Update(&ctx, session_key.data, session_key.length);
	MD5Final(confounded_session_key.data, &ctx);

	arcfour_crypt_blob(u.info25.password.data, 516, &confounded_session_key);
	memcpy(&u.info25.password.data[516], confounder, 16);

	torture_comment(tctx, "Testing SetUserInfo level 25 (set password ex)\n");

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetUserInfo_r(b, tctx, &s),
		"SetUserInfo failed");
	if (!NT_STATUS_IS_OK(s.out.result)) {
		torture_warning(tctx, "SetUserInfo level %u failed - %s\n",
		       s.in.level, nt_errstr(s.out.result));
		ret = false;
	} else {
		*password = newpass;
	}

	/* This should break the key nicely */
	confounded_session_key.data[0]++;

	arcfour_crypt_blob(u.info25.password.data, 516, &confounded_session_key);
	memcpy(&u.info25.password.data[516], confounder, 16);

	torture_comment(tctx, "Testing SetUserInfo level 25 (set password ex) with wrong session key\n");

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetUserInfo_r(b, tctx, &s),
		"SetUserInfo failed");
	if (!NT_STATUS_EQUAL(s.out.result, NT_STATUS_WRONG_PASSWORD)) {
		torture_warning(tctx, "SetUserInfo level %u should have failed with WRONG_PASSWORD- %s\n",
		       s.in.level, nt_errstr(s.out.result));
		ret = false;
	}

	return ret;
}

static bool test_SetUserPass_18(struct dcerpc_pipe *p, struct torture_context *tctx,
				struct policy_handle *handle, char **password)
{
	NTSTATUS status;
	struct samr_SetUserInfo s;
	union samr_UserInfo u;
	bool ret = true;
	DATA_BLOB session_key;
	char *newpass;
	struct dcerpc_binding_handle *b = p->binding_handle;
	struct samr_GetUserPwInfo pwp;
	struct samr_PwInfo info;
	int policy_min_pw_len = 0;
	uint8_t lm_hash[16], nt_hash[16];

	pwp.in.user_handle = handle;
	pwp.out.info = &info;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetUserPwInfo_r(b, tctx, &pwp),
		"GetUserPwInfo failed");
	if (NT_STATUS_IS_OK(pwp.out.result)) {
		policy_min_pw_len = pwp.out.info->min_password_length;
	}
	newpass = samr_rand_pass(tctx, policy_min_pw_len);

	s.in.user_handle = handle;
	s.in.info = &u;
	s.in.level = 18;

	ZERO_STRUCT(u);

	u.info18.nt_pwd_active = true;
	u.info18.lm_pwd_active = true;

	E_md4hash(newpass, nt_hash);
	E_deshash(newpass, lm_hash);

	status = dcerpc_fetch_session_key(p, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(tctx, "SetUserInfo level %u - no session key - %s\n",
		       s.in.level, nt_errstr(status));
		return false;
	}

	{
		DATA_BLOB in,out;
		in = data_blob_const(nt_hash, 16);
		out = data_blob_talloc_zero(tctx, 16);
		sess_crypt_blob(&out, &in, &session_key, true);
		memcpy(u.info18.nt_pwd.hash, out.data, out.length);
	}
	{
		DATA_BLOB in,out;
		in = data_blob_const(lm_hash, 16);
		out = data_blob_talloc_zero(tctx, 16);
		sess_crypt_blob(&out, &in, &session_key, true);
		memcpy(u.info18.lm_pwd.hash, out.data, out.length);
	}

	torture_comment(tctx, "Testing SetUserInfo level 18 (set password hash)\n");

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetUserInfo_r(b, tctx, &s),
		"SetUserInfo failed");
	if (!NT_STATUS_IS_OK(s.out.result)) {
		torture_warning(tctx, "SetUserInfo level %u failed - %s\n",
		       s.in.level, nt_errstr(s.out.result));
		ret = false;
	} else {
		*password = newpass;
	}

	return ret;
}

static bool test_SetUserPass_21(struct dcerpc_pipe *p, struct torture_context *tctx,
				struct policy_handle *handle, uint32_t fields_present,
				char **password)
{
	NTSTATUS status;
	struct samr_SetUserInfo s;
	union samr_UserInfo u;
	bool ret = true;
	DATA_BLOB session_key;
	char *newpass;
	struct dcerpc_binding_handle *b = p->binding_handle;
	struct samr_GetUserPwInfo pwp;
	struct samr_PwInfo info;
	int policy_min_pw_len = 0;
	uint8_t lm_hash[16], nt_hash[16];

	pwp.in.user_handle = handle;
	pwp.out.info = &info;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetUserPwInfo_r(b, tctx, &pwp),
		"GetUserPwInfo failed");
	if (NT_STATUS_IS_OK(pwp.out.result)) {
		policy_min_pw_len = pwp.out.info->min_password_length;
	}
	newpass = samr_rand_pass(tctx, policy_min_pw_len);

	s.in.user_handle = handle;
	s.in.info = &u;
	s.in.level = 21;

	E_md4hash(newpass, nt_hash);
	E_deshash(newpass, lm_hash);

	ZERO_STRUCT(u);

	u.info21.fields_present = fields_present;

	if (fields_present & SAMR_FIELD_LM_PASSWORD_PRESENT) {
		u.info21.lm_owf_password.length = 16;
		u.info21.lm_owf_password.size = 16;
		u.info21.lm_owf_password.array = (uint16_t *)lm_hash;
		u.info21.lm_password_set = true;
	}

	if (fields_present & SAMR_FIELD_NT_PASSWORD_PRESENT) {
		u.info21.nt_owf_password.length = 16;
		u.info21.nt_owf_password.size = 16;
		u.info21.nt_owf_password.array = (uint16_t *)nt_hash;
		u.info21.nt_password_set = true;
	}

	status = dcerpc_fetch_session_key(p, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(tctx, "SetUserInfo level %u - no session key - %s\n",
		       s.in.level, nt_errstr(status));
		return false;
	}

	if (fields_present & SAMR_FIELD_LM_PASSWORD_PRESENT) {
		DATA_BLOB in,out;
		in = data_blob_const(u.info21.lm_owf_password.array,
				     u.info21.lm_owf_password.length);
		out = data_blob_talloc_zero(tctx, 16);
		sess_crypt_blob(&out, &in, &session_key, true);
		u.info21.lm_owf_password.array = (uint16_t *)out.data;
	}

	if (fields_present & SAMR_FIELD_NT_PASSWORD_PRESENT) {
		DATA_BLOB in,out;
		in = data_blob_const(u.info21.nt_owf_password.array,
				     u.info21.nt_owf_password.length);
		out = data_blob_talloc_zero(tctx, 16);
		sess_crypt_blob(&out, &in, &session_key, true);
		u.info21.nt_owf_password.array = (uint16_t *)out.data;
	}

	torture_comment(tctx, "Testing SetUserInfo level 21 (set password hash)\n");

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetUserInfo_r(b, tctx, &s),
		"SetUserInfo failed");
	if (!NT_STATUS_IS_OK(s.out.result)) {
		torture_warning(tctx, "SetUserInfo level %u failed - %s\n",
		       s.in.level, nt_errstr(s.out.result));
		ret = false;
	} else {
		*password = newpass;
	}

	/* try invalid length */
	if (fields_present & SAMR_FIELD_NT_PASSWORD_PRESENT) {

		u.info21.nt_owf_password.length++;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetUserInfo_r(b, tctx, &s),
			"SetUserInfo failed");
		if (!NT_STATUS_EQUAL(s.out.result, NT_STATUS_INVALID_PARAMETER)) {
			torture_warning(tctx, "SetUserInfo level %u should have failed with NT_STATUS_INVALID_PARAMETER - %s\n",
			       s.in.level, nt_errstr(s.out.result));
			ret = false;
		}
	}

	if (fields_present & SAMR_FIELD_LM_PASSWORD_PRESENT) {

		u.info21.lm_owf_password.length++;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetUserInfo_r(b, tctx, &s),
			"SetUserInfo failed");
		if (!NT_STATUS_EQUAL(s.out.result, NT_STATUS_INVALID_PARAMETER)) {
			torture_warning(tctx, "SetUserInfo level %u should have failed with NT_STATUS_INVALID_PARAMETER - %s\n",
			       s.in.level, nt_errstr(s.out.result));
			ret = false;
		}
	}

	return ret;
}

static bool test_SetUserPass_level_ex(struct dcerpc_pipe *p,
				      struct torture_context *tctx,
				      struct policy_handle *handle,
				      uint16_t level,
				      uint32_t fields_present,
				      char **password, uint8_t password_expired,
				      bool use_setinfo2,
				      bool *matched_expected_error)
{
	NTSTATUS status;
	NTSTATUS expected_error = NT_STATUS_OK;
	struct samr_SetUserInfo s;
	struct samr_SetUserInfo2 s2;
	union samr_UserInfo u;
	bool ret = true;
	DATA_BLOB session_key;
	DATA_BLOB confounded_session_key = data_blob_talloc(tctx, NULL, 16);
	MD5_CTX ctx;
	uint8_t confounder[16];
	char *newpass;
	struct dcerpc_binding_handle *b = p->binding_handle;
	struct samr_GetUserPwInfo pwp;
	struct samr_PwInfo info;
	int policy_min_pw_len = 0;
	const char *comment = NULL;
	uint8_t lm_hash[16], nt_hash[16];

	pwp.in.user_handle = handle;
	pwp.out.info = &info;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetUserPwInfo_r(b, tctx, &pwp),
		"GetUserPwInfo failed");
	if (NT_STATUS_IS_OK(pwp.out.result)) {
		policy_min_pw_len = pwp.out.info->min_password_length;
	}
	newpass = samr_rand_pass_silent(tctx, policy_min_pw_len);

	if (use_setinfo2) {
		s2.in.user_handle = handle;
		s2.in.info = &u;
		s2.in.level = level;
	} else {
		s.in.user_handle = handle;
		s.in.info = &u;
		s.in.level = level;
	}

	if (fields_present & SAMR_FIELD_COMMENT) {
		comment = talloc_asprintf(tctx, "comment: %ld\n", (long int) time(NULL));
	}

	ZERO_STRUCT(u);

	switch (level) {
	case 18:
		E_md4hash(newpass, nt_hash);
		E_deshash(newpass, lm_hash);

		u.info18.nt_pwd_active = true;
		u.info18.lm_pwd_active = true;
		u.info18.password_expired = password_expired;

		memcpy(u.info18.lm_pwd.hash, lm_hash, 16);
		memcpy(u.info18.nt_pwd.hash, nt_hash, 16);

		break;
	case 21:
		E_md4hash(newpass, nt_hash);
		E_deshash(newpass, lm_hash);

		u.info21.fields_present = fields_present;
		u.info21.password_expired = password_expired;
		u.info21.comment.string = comment;

		if (fields_present & SAMR_FIELD_LM_PASSWORD_PRESENT) {
			u.info21.lm_owf_password.length = 16;
			u.info21.lm_owf_password.size = 16;
			u.info21.lm_owf_password.array = (uint16_t *)lm_hash;
			u.info21.lm_password_set = true;
		}

		if (fields_present & SAMR_FIELD_NT_PASSWORD_PRESENT) {
			u.info21.nt_owf_password.length = 16;
			u.info21.nt_owf_password.size = 16;
			u.info21.nt_owf_password.array = (uint16_t *)nt_hash;
			u.info21.nt_password_set = true;
		}

		break;
	case 23:
		u.info23.info.fields_present = fields_present;
		u.info23.info.password_expired = password_expired;
		u.info23.info.comment.string = comment;

		encode_pw_buffer(u.info23.password.data, newpass, STR_UNICODE);

		break;
	case 24:
		u.info24.password_expired = password_expired;

		encode_pw_buffer(u.info24.password.data, newpass, STR_UNICODE);

		break;
	case 25:
		u.info25.info.fields_present = fields_present;
		u.info25.info.password_expired = password_expired;
		u.info25.info.comment.string = comment;

		encode_pw_buffer(u.info25.password.data, newpass, STR_UNICODE);

		break;
	case 26:
		u.info26.password_expired = password_expired;

		encode_pw_buffer(u.info26.password.data, newpass, STR_UNICODE);

		break;
	}

	status = dcerpc_fetch_session_key(p, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(tctx, "SetUserInfo level %u - no session key - %s\n",
		       s.in.level, nt_errstr(status));
		return false;
	}

	generate_random_buffer((uint8_t *)confounder, 16);

	MD5Init(&ctx);
	MD5Update(&ctx, confounder, 16);
	MD5Update(&ctx, session_key.data, session_key.length);
	MD5Final(confounded_session_key.data, &ctx);

	switch (level) {
	case 18:
		{
			DATA_BLOB in,out;
			in = data_blob_const(u.info18.nt_pwd.hash, 16);
			out = data_blob_talloc_zero(tctx, 16);
			sess_crypt_blob(&out, &in, &session_key, true);
			memcpy(u.info18.nt_pwd.hash, out.data, out.length);
		}
		{
			DATA_BLOB in,out;
			in = data_blob_const(u.info18.lm_pwd.hash, 16);
			out = data_blob_talloc_zero(tctx, 16);
			sess_crypt_blob(&out, &in, &session_key, true);
			memcpy(u.info18.lm_pwd.hash, out.data, out.length);
		}

		break;
	case 21:
		if (fields_present & SAMR_FIELD_LM_PASSWORD_PRESENT) {
			DATA_BLOB in,out;
			in = data_blob_const(u.info21.lm_owf_password.array,
					     u.info21.lm_owf_password.length);
			out = data_blob_talloc_zero(tctx, 16);
			sess_crypt_blob(&out, &in, &session_key, true);
			u.info21.lm_owf_password.array = (uint16_t *)out.data;
		}
		if (fields_present & SAMR_FIELD_NT_PASSWORD_PRESENT) {
			DATA_BLOB in,out;
			in = data_blob_const(u.info21.nt_owf_password.array,
					     u.info21.nt_owf_password.length);
			out = data_blob_talloc_zero(tctx, 16);
			sess_crypt_blob(&out, &in, &session_key, true);
			u.info21.nt_owf_password.array = (uint16_t *)out.data;
		}
		break;
	case 23:
		arcfour_crypt_blob(u.info23.password.data, 516, &session_key);
		break;
	case 24:
		arcfour_crypt_blob(u.info24.password.data, 516, &session_key);
		break;
	case 25:
		arcfour_crypt_blob(u.info25.password.data, 516, &confounded_session_key);
		memcpy(&u.info25.password.data[516], confounder, 16);
		break;
	case 26:
		arcfour_crypt_blob(u.info26.password.data, 516, &confounded_session_key);
		memcpy(&u.info26.password.data[516], confounder, 16);
		break;
	}

	if (use_setinfo2) {
		torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetUserInfo2_r(b, tctx, &s2),
			"SetUserInfo2 failed");
		status = s2.out.result;
	} else {
		torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetUserInfo_r(b, tctx, &s),
			"SetUserInfo failed");
		status = s.out.result;
	}

	if (!NT_STATUS_IS_OK(status)) {
		if (fields_present == 0) {
			expected_error = NT_STATUS_INVALID_PARAMETER;
		}
		if (fields_present & SAMR_FIELD_LAST_PWD_CHANGE) {
			expected_error = NT_STATUS_ACCESS_DENIED;
		}
	}

	if (!NT_STATUS_IS_OK(expected_error)) {
		if (use_setinfo2) {
			torture_assert_ntstatus_equal(tctx,
				s2.out.result,
				expected_error, "SetUserInfo2 failed");
		} else {
			torture_assert_ntstatus_equal(tctx,
				s.out.result,
				expected_error, "SetUserInfo failed");
		}
		*matched_expected_error = true;
		return true;
	}

	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(tctx, "SetUserInfo%s level %u failed - %s\n",
		       use_setinfo2 ? "2":"", level, nt_errstr(status));
		ret = false;
	} else {
		*password = newpass;
	}

	return ret;
}

static bool test_SetAliasInfo(struct dcerpc_binding_handle *b,
			      struct torture_context *tctx,
			      struct policy_handle *handle)
{
	struct samr_SetAliasInfo r;
	struct samr_QueryAliasInfo q;
	union samr_AliasInfo *info;
	uint16_t levels[] = {2, 3};
	int i;
	bool ret = true;

	/* Ignoring switch level 1, as that includes the number of members for the alias
	 * and setting this to a wrong value might have negative consequences
	 */

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		torture_comment(tctx, "Testing SetAliasInfo level %u\n", levels[i]);

		r.in.alias_handle = handle;
		r.in.level = levels[i];
		r.in.info  = talloc(tctx, union samr_AliasInfo);
		switch (r.in.level) {
		    case ALIASINFONAME: init_lsa_String(&r.in.info->name,TEST_ALIASNAME); break;
		    case ALIASINFODESCRIPTION: init_lsa_String(&r.in.info->description,
				"Test Description, should test I18N as well"); break;
		    case ALIASINFOALL: torture_comment(tctx, "ALIASINFOALL ignored\n"); break;
		}

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetAliasInfo_r(b, tctx, &r),
			"SetAliasInfo failed");
		if (!NT_STATUS_IS_OK(r.out.result)) {
			torture_warning(tctx, "SetAliasInfo level %u failed - %s\n",
			       levels[i], nt_errstr(r.out.result));
			ret = false;
		}

		q.in.alias_handle = handle;
		q.in.level = levels[i];
		q.out.info = &info;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryAliasInfo_r(b, tctx, &q),
			"QueryAliasInfo failed");
		if (!NT_STATUS_IS_OK(q.out.result)) {
			torture_warning(tctx, "QueryAliasInfo level %u failed - %s\n",
			       levels[i], nt_errstr(q.out.result));
			ret = false;
		}
	}

	return ret;
}

static bool test_GetGroupsForUser(struct dcerpc_binding_handle *b,
				  struct torture_context *tctx,
				  struct policy_handle *user_handle)
{
	struct samr_GetGroupsForUser r;
	struct samr_RidWithAttributeArray *rids = NULL;

	torture_comment(tctx, "Testing GetGroupsForUser\n");

	r.in.user_handle = user_handle;
	r.out.rids = &rids;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetGroupsForUser_r(b, tctx, &r),
		"GetGroupsForUser failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "GetGroupsForUser failed");

	return true;

}

static bool test_GetDomPwInfo(struct dcerpc_pipe *p, struct torture_context *tctx,
			      struct lsa_String *domain_name)
{
	struct samr_GetDomPwInfo r;
	struct samr_PwInfo info;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.domain_name = domain_name;
	r.out.info = &info;

	torture_comment(tctx, "Testing GetDomPwInfo with name %s\n", r.in.domain_name->string);

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetDomPwInfo_r(b, tctx, &r),
		"GetDomPwInfo failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "GetDomPwInfo failed");

	r.in.domain_name->string = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	torture_comment(tctx, "Testing GetDomPwInfo with name %s\n", r.in.domain_name->string);

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetDomPwInfo_r(b, tctx, &r),
		"GetDomPwInfo failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "GetDomPwInfo failed");

	r.in.domain_name->string = "\\\\__NONAME__";
	torture_comment(tctx, "Testing GetDomPwInfo with name %s\n", r.in.domain_name->string);

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetDomPwInfo_r(b, tctx, &r),
		"GetDomPwInfo failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "GetDomPwInfo failed");

	r.in.domain_name->string = "\\\\Builtin";
	torture_comment(tctx, "Testing GetDomPwInfo with name %s\n", r.in.domain_name->string);

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetDomPwInfo_r(b, tctx, &r),
		"GetDomPwInfo failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "GetDomPwInfo failed");

	return true;
}

static bool test_GetUserPwInfo(struct dcerpc_binding_handle *b,
			       struct torture_context *tctx,
			       struct policy_handle *handle)
{
	struct samr_GetUserPwInfo r;
	struct samr_PwInfo info;

	torture_comment(tctx, "Testing GetUserPwInfo\n");

	r.in.user_handle = handle;
	r.out.info = &info;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetUserPwInfo_r(b, tctx, &r),
		"GetUserPwInfo failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "GetUserPwInfo");

	return true;
}

static NTSTATUS test_LookupName(struct dcerpc_binding_handle *b,
				struct torture_context *tctx,
				struct policy_handle *domain_handle, const char *name,
				uint32_t *rid)
{
	NTSTATUS status;
	struct samr_LookupNames n;
	struct lsa_String sname[2];
	struct samr_Ids rids, types;

	init_lsa_String(&sname[0], name);

	n.in.domain_handle = domain_handle;
	n.in.num_names = 1;
	n.in.names = sname;
	n.out.rids = &rids;
	n.out.types = &types;
	status = dcerpc_samr_LookupNames_r(b, tctx, &n);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (NT_STATUS_IS_OK(n.out.result)) {
		*rid = n.out.rids->ids[0];
	} else {
		return n.out.result;
	}

	init_lsa_String(&sname[1], "xxNONAMExx");
	n.in.num_names = 2;
	status = dcerpc_samr_LookupNames_r(b, tctx, &n);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_EQUAL(n.out.result, STATUS_SOME_UNMAPPED)) {
		torture_warning(tctx, "LookupNames[2] failed - %s\n", nt_errstr(n.out.result));
		if (NT_STATUS_IS_OK(n.out.result)) {
			return NT_STATUS_UNSUCCESSFUL;
		}
		return n.out.result;
	}

	n.in.num_names = 0;
	status = dcerpc_samr_LookupNames_r(b, tctx, &n);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(n.out.result)) {
		torture_warning(tctx, "LookupNames[0] failed - %s\n", nt_errstr(status));
		return n.out.result;
	}

	init_lsa_String(&sname[0], "xxNONAMExx");
	n.in.num_names = 1;
	status = dcerpc_samr_LookupNames_r(b, tctx, &n);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_EQUAL(n.out.result, NT_STATUS_NONE_MAPPED)) {
		torture_warning(tctx, "LookupNames[1 bad name] failed - %s\n", nt_errstr(n.out.result));
		if (NT_STATUS_IS_OK(n.out.result)) {
			return NT_STATUS_UNSUCCESSFUL;
		}
		return n.out.result;
	}

	init_lsa_String(&sname[0], "xxNONAMExx");
	init_lsa_String(&sname[1], "xxNONAME2xx");
	n.in.num_names = 2;
	status = dcerpc_samr_LookupNames_r(b, tctx, &n);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_EQUAL(n.out.result, NT_STATUS_NONE_MAPPED)) {
		torture_warning(tctx, "LookupNames[2 bad names] failed - %s\n", nt_errstr(n.out.result));
		if (NT_STATUS_IS_OK(n.out.result)) {
			return NT_STATUS_UNSUCCESSFUL;
		}
		return n.out.result;
	}

	return NT_STATUS_OK;
}

static NTSTATUS test_OpenUser_byname(struct dcerpc_binding_handle *b,
				     struct torture_context *tctx,
				     struct policy_handle *domain_handle,
				     const char *name, struct policy_handle *user_handle)
{
	NTSTATUS status;
	struct samr_OpenUser r;
	uint32_t rid;

	status = test_LookupName(b, tctx, domain_handle, name, &rid);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	r.in.domain_handle = domain_handle;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.in.rid = rid;
	r.out.user_handle = user_handle;
	status = dcerpc_samr_OpenUser_r(b, tctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_warning(tctx, "OpenUser_byname(%s -> %d) failed - %s\n", name, rid, nt_errstr(r.out.result));
	}

	return r.out.result;
}

#if 0
static bool test_ChangePasswordNT3(struct dcerpc_pipe *p,
				   struct torture_context *tctx,
				   struct policy_handle *handle)
{
	NTSTATUS status;
	struct samr_ChangePasswordUser r;
	bool ret = true;
	struct samr_Password hash1, hash2, hash3, hash4, hash5, hash6;
	struct policy_handle user_handle;
	char *oldpass = "test";
	char *newpass = "test2";
	uint8_t old_nt_hash[16], new_nt_hash[16];
	uint8_t old_lm_hash[16], new_lm_hash[16];

	status = test_OpenUser_byname(p, tctx, handle, "testuser", &user_handle);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	torture_comment(tctx, "Testing ChangePasswordUser for user 'testuser'\n");

	torture_comment(tctx, "old password: %s\n", oldpass);
	torture_comment(tctx, "new password: %s\n", newpass);

	E_md4hash(oldpass, old_nt_hash);
	E_md4hash(newpass, new_nt_hash);
	E_deshash(oldpass, old_lm_hash);
	E_deshash(newpass, new_lm_hash);

	E_old_pw_hash(new_lm_hash, old_lm_hash, hash1.hash);
	E_old_pw_hash(old_lm_hash, new_lm_hash, hash2.hash);
	E_old_pw_hash(new_nt_hash, old_nt_hash, hash3.hash);
	E_old_pw_hash(old_nt_hash, new_nt_hash, hash4.hash);
	E_old_pw_hash(old_lm_hash, new_nt_hash, hash5.hash);
	E_old_pw_hash(old_nt_hash, new_lm_hash, hash6.hash);

	r.in.handle = &user_handle;
	r.in.lm_present = 1;
	r.in.old_lm_crypted = &hash1;
	r.in.new_lm_crypted = &hash2;
	r.in.nt_present = 1;
	r.in.old_nt_crypted = &hash3;
	r.in.new_nt_crypted = &hash4;
	r.in.cross1_present = 1;
	r.in.nt_cross = &hash5;
	r.in.cross2_present = 1;
	r.in.lm_cross = &hash6;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_ChangePasswordUser_r(b, tctx, &r),
		"ChangePasswordUser failed");
	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_warning(tctx, "ChangePasswordUser failed - %s\n", nt_errstr(r.out.result));
		ret = false;
	}

	if (!test_samr_handle_Close(p, tctx, &user_handle)) {
		ret = false;
	}

	return ret;
}
#endif

static bool test_ChangePasswordUser(struct dcerpc_binding_handle *b,
				    struct torture_context *tctx,
				    const char *acct_name,
				    struct policy_handle *handle, char **password)
{
	NTSTATUS status;
	struct samr_ChangePasswordUser r;
	bool ret = true;
	struct samr_Password hash1, hash2, hash3, hash4, hash5, hash6;
	struct policy_handle user_handle;
	char *oldpass;
	uint8_t old_nt_hash[16], new_nt_hash[16];
	uint8_t old_lm_hash[16], new_lm_hash[16];
	bool changed = true;

	char *newpass;
	struct samr_GetUserPwInfo pwp;
	struct samr_PwInfo info;
	int policy_min_pw_len = 0;

	status = test_OpenUser_byname(b, tctx, handle, acct_name, &user_handle);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}
	pwp.in.user_handle = &user_handle;
	pwp.out.info = &info;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetUserPwInfo_r(b, tctx, &pwp),
		"GetUserPwInfo failed");
	if (NT_STATUS_IS_OK(pwp.out.result)) {
		policy_min_pw_len = pwp.out.info->min_password_length;
	}
	newpass = samr_rand_pass(tctx, policy_min_pw_len);

	torture_comment(tctx, "Testing ChangePasswordUser\n");

	torture_assert(tctx, *password != NULL,
				   "Failing ChangePasswordUser as old password was NULL.  Previous test failed?");

	oldpass = *password;

	E_md4hash(oldpass, old_nt_hash);
	E_md4hash(newpass, new_nt_hash);
	E_deshash(oldpass, old_lm_hash);
	E_deshash(newpass, new_lm_hash);

	E_old_pw_hash(new_lm_hash, old_lm_hash, hash1.hash);
	E_old_pw_hash(old_lm_hash, new_lm_hash, hash2.hash);
	E_old_pw_hash(new_nt_hash, old_nt_hash, hash3.hash);
	E_old_pw_hash(old_nt_hash, new_nt_hash, hash4.hash);
	E_old_pw_hash(old_lm_hash, new_nt_hash, hash5.hash);
	E_old_pw_hash(old_nt_hash, new_lm_hash, hash6.hash);

	r.in.user_handle = &user_handle;
	r.in.lm_present = 1;
	/* Break the LM hash */
	hash1.hash[0]++;
	r.in.old_lm_crypted = &hash1;
	r.in.new_lm_crypted = &hash2;
	r.in.nt_present = 1;
	r.in.old_nt_crypted = &hash3;
	r.in.new_nt_crypted = &hash4;
	r.in.cross1_present = 1;
	r.in.nt_cross = &hash5;
	r.in.cross2_present = 1;
	r.in.lm_cross = &hash6;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_ChangePasswordUser_r(b, tctx, &r),
		"ChangePasswordUser failed");

	/* Do not proceed if this call has been removed */
	if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_NOT_IMPLEMENTED)) {
		return true;
	}

	if (!NT_STATUS_EQUAL(r.out.result, NT_STATUS_PASSWORD_RESTRICTION)) {
		torture_assert_ntstatus_equal(tctx, r.out.result, NT_STATUS_WRONG_PASSWORD,
			"ChangePasswordUser failed: expected NT_STATUS_WRONG_PASSWORD because we broke the LM hash");
	}

	/* Unbreak the LM hash */
	hash1.hash[0]--;

	r.in.user_handle = &user_handle;
	r.in.lm_present = 1;
	r.in.old_lm_crypted = &hash1;
	r.in.new_lm_crypted = &hash2;
	/* Break the NT hash */
	hash3.hash[0]--;
	r.in.nt_present = 1;
	r.in.old_nt_crypted = &hash3;
	r.in.new_nt_crypted = &hash4;
	r.in.cross1_present = 1;
	r.in.nt_cross = &hash5;
	r.in.cross2_present = 1;
	r.in.lm_cross = &hash6;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_ChangePasswordUser_r(b, tctx, &r),
		"ChangePasswordUser failed");
	torture_assert_ntstatus_equal(tctx, r.out.result, NT_STATUS_WRONG_PASSWORD,
		"expected NT_STATUS_WRONG_PASSWORD because we broke the NT hash");

	/* Unbreak the NT hash */
	hash3.hash[0]--;

	r.in.user_handle = &user_handle;
	r.in.lm_present = 1;
	r.in.old_lm_crypted = &hash1;
	r.in.new_lm_crypted = &hash2;
	r.in.nt_present = 1;
	r.in.old_nt_crypted = &hash3;
	r.in.new_nt_crypted = &hash4;
	r.in.cross1_present = 1;
	r.in.nt_cross = &hash5;
	r.in.cross2_present = 1;
	/* Break the LM cross */
	hash6.hash[0]++;
	r.in.lm_cross = &hash6;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_ChangePasswordUser_r(b, tctx, &r),
		"ChangePasswordUser failed");
	if (!NT_STATUS_EQUAL(r.out.result, NT_STATUS_WRONG_PASSWORD)) {
		torture_warning(tctx, "ChangePasswordUser failed: expected NT_STATUS_WRONG_PASSWORD because we broke the LM cross-hash, got %s\n", nt_errstr(r.out.result));
		ret = false;
	}

	/* Unbreak the LM cross */
	hash6.hash[0]--;

	r.in.user_handle = &user_handle;
	r.in.lm_present = 1;
	r.in.old_lm_crypted = &hash1;
	r.in.new_lm_crypted = &hash2;
	r.in.nt_present = 1;
	r.in.old_nt_crypted = &hash3;
	r.in.new_nt_crypted = &hash4;
	r.in.cross1_present = 1;
	/* Break the NT cross */
	hash5.hash[0]++;
	r.in.nt_cross = &hash5;
	r.in.cross2_present = 1;
	r.in.lm_cross = &hash6;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_ChangePasswordUser_r(b, tctx, &r),
		"ChangePasswordUser failed");
	if (!NT_STATUS_EQUAL(r.out.result, NT_STATUS_WRONG_PASSWORD)) {
		torture_warning(tctx, "ChangePasswordUser failed: expected NT_STATUS_WRONG_PASSWORD because we broke the NT cross-hash, got %s\n", nt_errstr(r.out.result));
		ret = false;
	}

	/* Unbreak the NT cross */
	hash5.hash[0]--;


	/* Reset the hashes to not broken values */
	E_old_pw_hash(new_lm_hash, old_lm_hash, hash1.hash);
	E_old_pw_hash(old_lm_hash, new_lm_hash, hash2.hash);
	E_old_pw_hash(new_nt_hash, old_nt_hash, hash3.hash);
	E_old_pw_hash(old_nt_hash, new_nt_hash, hash4.hash);
	E_old_pw_hash(old_lm_hash, new_nt_hash, hash5.hash);
	E_old_pw_hash(old_nt_hash, new_lm_hash, hash6.hash);

	r.in.user_handle = &user_handle;
	r.in.lm_present = 1;
	r.in.old_lm_crypted = &hash1;
	r.in.new_lm_crypted = &hash2;
	r.in.nt_present = 1;
	r.in.old_nt_crypted = &hash3;
	r.in.new_nt_crypted = &hash4;
	r.in.cross1_present = 1;
	r.in.nt_cross = &hash5;
	r.in.cross2_present = 0;
	r.in.lm_cross = NULL;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_ChangePasswordUser_r(b, tctx, &r),
		"ChangePasswordUser failed");
	if (NT_STATUS_IS_OK(r.out.result)) {
		changed = true;
		*password = newpass;
	} else if (!NT_STATUS_EQUAL(NT_STATUS_PASSWORD_RESTRICTION, r.out.result)) {
		torture_warning(tctx, "ChangePasswordUser failed: expected NT_STATUS_OK, or at least NT_STATUS_PASSWORD_RESTRICTION, got %s\n", nt_errstr(r.out.result));
		ret = false;
	}

	oldpass = newpass;
	newpass = samr_rand_pass(tctx, policy_min_pw_len);

	E_md4hash(oldpass, old_nt_hash);
	E_md4hash(newpass, new_nt_hash);
	E_deshash(oldpass, old_lm_hash);
	E_deshash(newpass, new_lm_hash);


	/* Reset the hashes to not broken values */
	E_old_pw_hash(new_lm_hash, old_lm_hash, hash1.hash);
	E_old_pw_hash(old_lm_hash, new_lm_hash, hash2.hash);
	E_old_pw_hash(new_nt_hash, old_nt_hash, hash3.hash);
	E_old_pw_hash(old_nt_hash, new_nt_hash, hash4.hash);
	E_old_pw_hash(old_lm_hash, new_nt_hash, hash5.hash);
	E_old_pw_hash(old_nt_hash, new_lm_hash, hash6.hash);

	r.in.user_handle = &user_handle;
	r.in.lm_present = 1;
	r.in.old_lm_crypted = &hash1;
	r.in.new_lm_crypted = &hash2;
	r.in.nt_present = 1;
	r.in.old_nt_crypted = &hash3;
	r.in.new_nt_crypted = &hash4;
	r.in.cross1_present = 0;
	r.in.nt_cross = NULL;
	r.in.cross2_present = 1;
	r.in.lm_cross = &hash6;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_ChangePasswordUser_r(b, tctx, &r),
		"ChangePasswordUser failed");
	if (NT_STATUS_IS_OK(r.out.result)) {
		changed = true;
		*password = newpass;
	} else if (!NT_STATUS_EQUAL(NT_STATUS_PASSWORD_RESTRICTION, r.out.result)) {
		torture_warning(tctx, "ChangePasswordUser failed: expected NT_STATUS_OK, or at least NT_STATUS_PASSWORD_RESTRICTION, got %s\n", nt_errstr(r.out.result));
		ret = false;
	}

	oldpass = newpass;
	newpass = samr_rand_pass(tctx, policy_min_pw_len);

	E_md4hash(oldpass, old_nt_hash);
	E_md4hash(newpass, new_nt_hash);
	E_deshash(oldpass, old_lm_hash);
	E_deshash(newpass, new_lm_hash);


	/* Reset the hashes to not broken values */
	E_old_pw_hash(new_lm_hash, old_lm_hash, hash1.hash);
	E_old_pw_hash(old_lm_hash, new_lm_hash, hash2.hash);
	E_old_pw_hash(new_nt_hash, old_nt_hash, hash3.hash);
	E_old_pw_hash(old_nt_hash, new_nt_hash, hash4.hash);
	E_old_pw_hash(old_lm_hash, new_nt_hash, hash5.hash);
	E_old_pw_hash(old_nt_hash, new_lm_hash, hash6.hash);

	r.in.user_handle = &user_handle;
	r.in.lm_present = 1;
	r.in.old_lm_crypted = &hash1;
	r.in.new_lm_crypted = &hash2;
	r.in.nt_present = 1;
	r.in.old_nt_crypted = &hash3;
	r.in.new_nt_crypted = &hash4;
	r.in.cross1_present = 1;
	r.in.nt_cross = &hash5;
	r.in.cross2_present = 1;
	r.in.lm_cross = &hash6;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_ChangePasswordUser_r(b, tctx, &r),
		"ChangePasswordUser failed");
	if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_PASSWORD_RESTRICTION)) {
		torture_comment(tctx, "ChangePasswordUser returned: %s perhaps min password age? (not fatal)\n", nt_errstr(r.out.result));
	} else 	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_warning(tctx, "ChangePasswordUser failed - %s\n", nt_errstr(r.out.result));
		ret = false;
	} else {
		changed = true;
		*password = newpass;
	}

	r.in.user_handle = &user_handle;
	r.in.lm_present = 1;
	r.in.old_lm_crypted = &hash1;
	r.in.new_lm_crypted = &hash2;
	r.in.nt_present = 1;
	r.in.old_nt_crypted = &hash3;
	r.in.new_nt_crypted = &hash4;
	r.in.cross1_present = 1;
	r.in.nt_cross = &hash5;
	r.in.cross2_present = 1;
	r.in.lm_cross = &hash6;

	if (changed) {
		torture_assert_ntstatus_ok(tctx, dcerpc_samr_ChangePasswordUser_r(b, tctx, &r),
			"ChangePasswordUser failed");
		if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_PASSWORD_RESTRICTION)) {
			torture_comment(tctx, "ChangePasswordUser returned: %s perhaps min password age? (not fatal)\n", nt_errstr(r.out.result));
		} else if (!NT_STATUS_EQUAL(r.out.result, NT_STATUS_WRONG_PASSWORD)) {
			torture_warning(tctx, "ChangePasswordUser failed: expected NT_STATUS_WRONG_PASSWORD because we already changed the password, got %s\n", nt_errstr(r.out.result));
			ret = false;
		}
	}


	if (!test_samr_handle_Close(b, tctx, &user_handle)) {
		ret = false;
	}

	return ret;
}


static bool test_OemChangePasswordUser2(struct dcerpc_pipe *p,
					struct torture_context *tctx,
					const char *acct_name,
					struct policy_handle *handle, char **password)
{
	struct samr_OemChangePasswordUser2 r;
	bool ret = true;
	struct samr_Password lm_verifier;
	struct samr_CryptPassword lm_pass;
	struct lsa_AsciiString server, account, account_bad;
	char *oldpass;
	char *newpass;
	struct dcerpc_binding_handle *b = p->binding_handle;
	uint8_t old_lm_hash[16], new_lm_hash[16];

	struct samr_GetDomPwInfo dom_pw_info;
	struct samr_PwInfo info;
	int policy_min_pw_len = 0;

	struct lsa_String domain_name;

	domain_name.string = "";
	dom_pw_info.in.domain_name = &domain_name;
	dom_pw_info.out.info = &info;

	torture_comment(tctx, "Testing OemChangePasswordUser2\n");

	torture_assert(tctx, *password != NULL,
				   "Failing OemChangePasswordUser2 as old password was NULL.  Previous test failed?");

	oldpass = *password;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetDomPwInfo_r(b, tctx, &dom_pw_info),
		"GetDomPwInfo failed");
	if (NT_STATUS_IS_OK(dom_pw_info.out.result)) {
		policy_min_pw_len = dom_pw_info.out.info->min_password_length;
	}

	newpass = samr_rand_pass(tctx, policy_min_pw_len);

	server.string = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	account.string = acct_name;

	E_deshash(oldpass, old_lm_hash);
	E_deshash(newpass, new_lm_hash);

	encode_pw_buffer(lm_pass.data, newpass, STR_ASCII);
	arcfour_crypt(lm_pass.data, old_lm_hash, 516);
	E_old_pw_hash(new_lm_hash, old_lm_hash, lm_verifier.hash);

	r.in.server = &server;
	r.in.account = &account;
	r.in.password = &lm_pass;
	r.in.hash = &lm_verifier;

	/* Break the verification */
	lm_verifier.hash[0]++;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_OemChangePasswordUser2_r(b, tctx, &r),
		"OemChangePasswordUser2 failed");

	if (!NT_STATUS_EQUAL(r.out.result, NT_STATUS_PASSWORD_RESTRICTION)
	    && !NT_STATUS_EQUAL(r.out.result, NT_STATUS_WRONG_PASSWORD)) {
		torture_warning(tctx, "OemChangePasswordUser2 failed, should have returned WRONG_PASSWORD (or at least 'PASSWORD_RESTRICTON') for invalid password verifier - %s\n",
			nt_errstr(r.out.result));
		ret = false;
	}

	encode_pw_buffer(lm_pass.data, newpass, STR_ASCII);
	/* Break the old password */
	old_lm_hash[0]++;
	arcfour_crypt(lm_pass.data, old_lm_hash, 516);
	/* unbreak it for the next operation */
	old_lm_hash[0]--;
	E_old_pw_hash(new_lm_hash, old_lm_hash, lm_verifier.hash);

	r.in.server = &server;
	r.in.account = &account;
	r.in.password = &lm_pass;
	r.in.hash = &lm_verifier;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_OemChangePasswordUser2_r(b, tctx, &r),
		"OemChangePasswordUser2 failed");

	if (!NT_STATUS_EQUAL(r.out.result, NT_STATUS_PASSWORD_RESTRICTION)
	    && !NT_STATUS_EQUAL(r.out.result, NT_STATUS_WRONG_PASSWORD)) {
		torture_warning(tctx, "OemChangePasswordUser2 failed, should have returned WRONG_PASSWORD (or at least 'PASSWORD_RESTRICTON') for invalidly encrpted password - %s\n",
			nt_errstr(r.out.result));
		ret = false;
	}

	encode_pw_buffer(lm_pass.data, newpass, STR_ASCII);
	arcfour_crypt(lm_pass.data, old_lm_hash, 516);

	r.in.server = &server;
	r.in.account = &account;
	r.in.password = &lm_pass;
	r.in.hash = NULL;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_OemChangePasswordUser2_r(b, tctx, &r),
		"OemChangePasswordUser2 failed");

	if (!NT_STATUS_EQUAL(r.out.result, NT_STATUS_PASSWORD_RESTRICTION)
	    && !NT_STATUS_EQUAL(r.out.result, NT_STATUS_INVALID_PARAMETER)) {
		torture_warning(tctx, "OemChangePasswordUser2 failed, should have returned INVALID_PARAMETER (or at least 'PASSWORD_RESTRICTON') for no supplied validation hash - %s\n",
			nt_errstr(r.out.result));
		ret = false;
	}

	/* This shouldn't be a valid name */
	account_bad.string = TEST_ACCOUNT_NAME "XX";
	r.in.account = &account_bad;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_OemChangePasswordUser2_r(b, tctx, &r),
		"OemChangePasswordUser2 failed");

	if (!NT_STATUS_EQUAL(r.out.result, NT_STATUS_INVALID_PARAMETER)) {
		torture_warning(tctx, "OemChangePasswordUser2 failed, should have returned INVALID_PARAMETER for no supplied validation hash and invalid user - %s\n",
			nt_errstr(r.out.result));
		ret = false;
	}

	/* This shouldn't be a valid name */
	account_bad.string = TEST_ACCOUNT_NAME "XX";
	r.in.account = &account_bad;
	r.in.password = &lm_pass;
	r.in.hash = &lm_verifier;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_OemChangePasswordUser2_r(b, tctx, &r),
		"OemChangePasswordUser2 failed");

	if (!NT_STATUS_EQUAL(r.out.result, NT_STATUS_WRONG_PASSWORD)) {
		torture_warning(tctx, "OemChangePasswordUser2 failed, should have returned WRONG_PASSWORD for invalid user - %s\n",
			nt_errstr(r.out.result));
		ret = false;
	}

	/* This shouldn't be a valid name */
	account_bad.string = TEST_ACCOUNT_NAME "XX";
	r.in.account = &account_bad;
	r.in.password = NULL;
	r.in.hash = &lm_verifier;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_OemChangePasswordUser2_r(b, tctx, &r),
		"OemChangePasswordUser2 failed");

	if (!NT_STATUS_EQUAL(r.out.result, NT_STATUS_INVALID_PARAMETER)) {
		torture_warning(tctx, "OemChangePasswordUser2 failed, should have returned INVALID_PARAMETER for no supplied password and invalid user - %s\n",
			nt_errstr(r.out.result));
		ret = false;
	}

	E_deshash(oldpass, old_lm_hash);
	E_deshash(newpass, new_lm_hash);

	encode_pw_buffer(lm_pass.data, newpass, STR_ASCII);
	arcfour_crypt(lm_pass.data, old_lm_hash, 516);
	E_old_pw_hash(new_lm_hash, old_lm_hash, lm_verifier.hash);

	r.in.server = &server;
	r.in.account = &account;
	r.in.password = &lm_pass;
	r.in.hash = &lm_verifier;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_OemChangePasswordUser2_r(b, tctx, &r),
		"OemChangePasswordUser2 failed");

	if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_PASSWORD_RESTRICTION)) {
		torture_comment(tctx, "OemChangePasswordUser2 returned: %s perhaps min password age? (not fatal)\n", nt_errstr(r.out.result));
	} else if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_warning(tctx, "OemChangePasswordUser2 failed - %s\n", nt_errstr(r.out.result));
		ret = false;
	} else {
		*password = newpass;
	}

	return ret;
}


static bool test_ChangePasswordUser2(struct dcerpc_pipe *p, struct torture_context *tctx,
				     const char *acct_name,
				     char **password,
				     char *newpass, bool allow_password_restriction)
{
	struct samr_ChangePasswordUser2 r;
	bool ret = true;
	struct lsa_String server, account;
	struct samr_CryptPassword nt_pass, lm_pass;
	struct samr_Password nt_verifier, lm_verifier;
	char *oldpass;
	struct dcerpc_binding_handle *b = p->binding_handle;
	uint8_t old_nt_hash[16], new_nt_hash[16];
	uint8_t old_lm_hash[16], new_lm_hash[16];

	struct samr_GetDomPwInfo dom_pw_info;
	struct samr_PwInfo info;

	struct lsa_String domain_name;

	domain_name.string = "";
	dom_pw_info.in.domain_name = &domain_name;
	dom_pw_info.out.info = &info;

	torture_comment(tctx, "Testing ChangePasswordUser2 on %s\n", acct_name);

	torture_assert(tctx, *password != NULL,
				   "Failing ChangePasswordUser2 as old password was NULL.  Previous test failed?");
	oldpass = *password;

	if (!newpass) {
		int policy_min_pw_len = 0;
		torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetDomPwInfo_r(b, tctx, &dom_pw_info),
			"GetDomPwInfo failed");
		if (NT_STATUS_IS_OK(dom_pw_info.out.result)) {
			policy_min_pw_len = dom_pw_info.out.info->min_password_length;
		}

		newpass = samr_rand_pass(tctx, policy_min_pw_len);
	}

	server.string = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	init_lsa_String(&account, acct_name);

	E_md4hash(oldpass, old_nt_hash);
	E_md4hash(newpass, new_nt_hash);

	E_deshash(oldpass, old_lm_hash);
	E_deshash(newpass, new_lm_hash);

	encode_pw_buffer(lm_pass.data, newpass, STR_ASCII|STR_TERMINATE);
	arcfour_crypt(lm_pass.data, old_lm_hash, 516);
	E_old_pw_hash(new_nt_hash, old_lm_hash, lm_verifier.hash);

	encode_pw_buffer(nt_pass.data, newpass, STR_UNICODE);
	arcfour_crypt(nt_pass.data, old_nt_hash, 516);
	E_old_pw_hash(new_nt_hash, old_nt_hash, nt_verifier.hash);

	r.in.server = &server;
	r.in.account = &account;
	r.in.nt_password = &nt_pass;
	r.in.nt_verifier = &nt_verifier;
	r.in.lm_change = 1;
	r.in.lm_password = &lm_pass;
	r.in.lm_verifier = &lm_verifier;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_ChangePasswordUser2_r(b, tctx, &r),
		"ChangePasswordUser2 failed");

	if (allow_password_restriction && NT_STATUS_EQUAL(r.out.result, NT_STATUS_PASSWORD_RESTRICTION)) {
		torture_comment(tctx, "ChangePasswordUser2 returned: %s perhaps min password age? (not fatal)\n", nt_errstr(r.out.result));
	} else if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_warning(tctx, "ChangePasswordUser2 failed - %s\n", nt_errstr(r.out.result));
		ret = false;
	} else {
		*password = newpass;
	}

	return ret;
}


bool test_ChangePasswordUser3(struct dcerpc_pipe *p, struct torture_context *tctx,
			      const char *account_string,
			      int policy_min_pw_len,
			      char **password,
			      const char *newpass,
			      NTTIME last_password_change,
			      bool handle_reject_reason)
{
	struct samr_ChangePasswordUser3 r;
	bool ret = true;
	struct lsa_String server, account, account_bad;
	struct samr_CryptPassword nt_pass, lm_pass;
	struct samr_Password nt_verifier, lm_verifier;
	char *oldpass;
	struct dcerpc_binding_handle *b = p->binding_handle;
	uint8_t old_nt_hash[16], new_nt_hash[16];
	uint8_t old_lm_hash[16], new_lm_hash[16];
	NTTIME t;
	struct samr_DomInfo1 *dominfo = NULL;
	struct userPwdChangeFailureInformation *reject = NULL;

	torture_comment(tctx, "Testing ChangePasswordUser3\n");

	if (newpass == NULL) {
		do {
			if (policy_min_pw_len == 0) {
				newpass = samr_rand_pass(tctx, policy_min_pw_len);
			} else {
				newpass = samr_rand_pass_fixed_len(tctx, policy_min_pw_len);
			}
		} while (check_password_quality(newpass) == false);
	} else {
		torture_comment(tctx, "Using password '%s'\n", newpass);
	}

	torture_assert(tctx, *password != NULL,
				   "Failing ChangePasswordUser3 as old password was NULL.  Previous test failed?");

	oldpass = *password;
	server.string = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	init_lsa_String(&account, account_string);

	E_md4hash(oldpass, old_nt_hash);
	E_md4hash(newpass, new_nt_hash);

	E_deshash(oldpass, old_lm_hash);
	E_deshash(newpass, new_lm_hash);

	encode_pw_buffer(lm_pass.data, newpass, STR_UNICODE);
	arcfour_crypt(lm_pass.data, old_nt_hash, 516);
	E_old_pw_hash(new_nt_hash, old_lm_hash, lm_verifier.hash);

	encode_pw_buffer(nt_pass.data, newpass, STR_UNICODE);
	arcfour_crypt(nt_pass.data, old_nt_hash, 516);
	E_old_pw_hash(new_nt_hash, old_nt_hash, nt_verifier.hash);

	/* Break the verification */
	nt_verifier.hash[0]++;

	r.in.server = &server;
	r.in.account = &account;
	r.in.nt_password = &nt_pass;
	r.in.nt_verifier = &nt_verifier;
	r.in.lm_change = 1;
	r.in.lm_password = &lm_pass;
	r.in.lm_verifier = &lm_verifier;
	r.in.password3 = NULL;
	r.out.dominfo = &dominfo;
	r.out.reject = &reject;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_ChangePasswordUser3_r(b, tctx, &r),
		"ChangePasswordUser3 failed");
	if (!NT_STATUS_EQUAL(r.out.result, NT_STATUS_PASSWORD_RESTRICTION) &&
	    (!NT_STATUS_EQUAL(r.out.result, NT_STATUS_WRONG_PASSWORD))) {
		torture_warning(tctx, "ChangePasswordUser3 failed, should have returned WRONG_PASSWORD (or at least 'PASSWORD_RESTRICTON') for invalid password verifier - %s\n",
			nt_errstr(r.out.result));
		ret = false;
	}

	encode_pw_buffer(lm_pass.data, newpass, STR_UNICODE);
	arcfour_crypt(lm_pass.data, old_nt_hash, 516);
	E_old_pw_hash(new_nt_hash, old_lm_hash, lm_verifier.hash);

	encode_pw_buffer(nt_pass.data, newpass, STR_UNICODE);
	/* Break the NT hash */
	old_nt_hash[0]++;
	arcfour_crypt(nt_pass.data, old_nt_hash, 516);
	/* Unbreak it again */
	old_nt_hash[0]--;
	E_old_pw_hash(new_nt_hash, old_nt_hash, nt_verifier.hash);

	r.in.server = &server;
	r.in.account = &account;
	r.in.nt_password = &nt_pass;
	r.in.nt_verifier = &nt_verifier;
	r.in.lm_change = 1;
	r.in.lm_password = &lm_pass;
	r.in.lm_verifier = &lm_verifier;
	r.in.password3 = NULL;
	r.out.dominfo = &dominfo;
	r.out.reject = &reject;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_ChangePasswordUser3_r(b, tctx, &r),
		"ChangePasswordUser3 failed");
	if (!NT_STATUS_EQUAL(r.out.result, NT_STATUS_PASSWORD_RESTRICTION) &&
	    (!NT_STATUS_EQUAL(r.out.result, NT_STATUS_WRONG_PASSWORD))) {
		torture_warning(tctx, "ChangePasswordUser3 failed, should have returned WRONG_PASSWORD (or at least 'PASSWORD_RESTRICTON') for invalidly encrpted password - %s\n",
			nt_errstr(r.out.result));
		ret = false;
	}

	/* This shouldn't be a valid name */
	init_lsa_String(&account_bad, talloc_asprintf(tctx, "%sXX", account_string));

	r.in.account = &account_bad;
	torture_assert_ntstatus_ok(tctx, dcerpc_samr_ChangePasswordUser3_r(b, tctx, &r),
		"ChangePasswordUser3 failed");
	if (!NT_STATUS_EQUAL(r.out.result, NT_STATUS_WRONG_PASSWORD)) {
		torture_warning(tctx, "ChangePasswordUser3 failed, should have returned WRONG_PASSWORD for invalid username - %s\n",
			nt_errstr(r.out.result));
		ret = false;
	}

	E_md4hash(oldpass, old_nt_hash);
	E_md4hash(newpass, new_nt_hash);

	E_deshash(oldpass, old_lm_hash);
	E_deshash(newpass, new_lm_hash);

	encode_pw_buffer(lm_pass.data, newpass, STR_UNICODE);
	arcfour_crypt(lm_pass.data, old_nt_hash, 516);
	E_old_pw_hash(new_nt_hash, old_lm_hash, lm_verifier.hash);

	encode_pw_buffer(nt_pass.data, newpass, STR_UNICODE);
	arcfour_crypt(nt_pass.data, old_nt_hash, 516);
	E_old_pw_hash(new_nt_hash, old_nt_hash, nt_verifier.hash);

	r.in.server = &server;
	r.in.account = &account;
	r.in.nt_password = &nt_pass;
	r.in.nt_verifier = &nt_verifier;
	r.in.lm_change = 1;
	r.in.lm_password = &lm_pass;
	r.in.lm_verifier = &lm_verifier;
	r.in.password3 = NULL;
	r.out.dominfo = &dominfo;
	r.out.reject = &reject;

	unix_to_nt_time(&t, time(NULL));

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_ChangePasswordUser3_r(b, tctx, &r),
		"ChangePasswordUser3 failed");

	if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_PASSWORD_RESTRICTION)
	    && dominfo
	    && reject
	    && handle_reject_reason
	    && (!null_nttime(last_password_change) || !dominfo->min_password_age)) {
		if (dominfo->password_properties & DOMAIN_REFUSE_PASSWORD_CHANGE ) {

			if (reject && (reject->extendedFailureReason != SAM_PWD_CHANGE_NO_ERROR)) {
				torture_warning(tctx, "expected SAM_PWD_CHANGE_NO_ERROR (%d), got %d\n",
					SAM_PWD_CHANGE_NO_ERROR, reject->extendedFailureReason);
				return false;
			}
		}

		/* We tested the order of precendence which is as follows:

		* pwd min_age
		* pwd length
		* pwd complexity
		* pwd history

		Guenther */

		if ((dominfo->min_password_age > 0) && !null_nttime(last_password_change) &&
			   (last_password_change + dominfo->min_password_age > t)) {

			if (reject->extendedFailureReason != SAM_PWD_CHANGE_NO_ERROR) {
				torture_warning(tctx, "expected SAM_PWD_CHANGE_NO_ERROR (%d), got %d\n",
					SAM_PWD_CHANGE_NO_ERROR, reject->extendedFailureReason);
				return false;
			}

		} else if ((dominfo->min_password_length > 0) &&
			   (strlen(newpass) < dominfo->min_password_length)) {

			if (reject->extendedFailureReason != SAM_PWD_CHANGE_PASSWORD_TOO_SHORT) {
				torture_warning(tctx, "expected SAM_PWD_CHANGE_PASSWORD_TOO_SHORT (%d), got %d\n",
					SAM_PWD_CHANGE_PASSWORD_TOO_SHORT, reject->extendedFailureReason);
				return false;
			}

		} else if ((dominfo->password_history_length > 0) &&
			    strequal(oldpass, newpass)) {

			if (reject->extendedFailureReason != SAM_PWD_CHANGE_PWD_IN_HISTORY) {
				torture_warning(tctx, "expected SAM_PWD_CHANGE_PWD_IN_HISTORY (%d), got %d\n",
					SAM_PWD_CHANGE_PWD_IN_HISTORY, reject->extendedFailureReason);
				return false;
			}
		} else if (dominfo->password_properties & DOMAIN_PASSWORD_COMPLEX) {

			if (reject->extendedFailureReason != SAM_PWD_CHANGE_NOT_COMPLEX) {
				torture_warning(tctx, "expected SAM_PWD_CHANGE_NOT_COMPLEX (%d), got %d\n",
					SAM_PWD_CHANGE_NOT_COMPLEX, reject->extendedFailureReason);
				return false;
			}

		}

		if (reject->extendedFailureReason == SAM_PWD_CHANGE_PASSWORD_TOO_SHORT) {
			/* retry with adjusted size */
			return test_ChangePasswordUser3(p, tctx, account_string,
							dominfo->min_password_length,
							password, NULL, 0, false);

		}

	} else if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_PASSWORD_RESTRICTION)) {
		if (reject && reject->extendedFailureReason != SAM_PWD_CHANGE_NO_ERROR) {
			torture_warning(tctx, "expected SAM_PWD_CHANGE_NO_ERROR (%d), got %d\n",
			       SAM_PWD_CHANGE_NO_ERROR, reject->extendedFailureReason);
			return false;
		}
		/* Perhaps the server has a 'min password age' set? */

	} else {
		torture_assert_ntstatus_ok(tctx, r.out.result, "ChangePasswordUser3");

		*password = talloc_strdup(tctx, newpass);
	}

	return ret;
}

bool test_ChangePasswordRandomBytes(struct dcerpc_pipe *p, struct torture_context *tctx,
				    const char *account_string,
				    struct policy_handle *handle,
				    char **password)
{
	NTSTATUS status;
	struct samr_ChangePasswordUser3 r;
	struct samr_SetUserInfo s;
	union samr_UserInfo u;
	DATA_BLOB session_key;
	DATA_BLOB confounded_session_key = data_blob_talloc(tctx, NULL, 16);
	uint8_t confounder[16];
	MD5_CTX ctx;

	bool ret = true;
	struct lsa_String server, account;
	struct samr_CryptPassword nt_pass;
	struct samr_Password nt_verifier;
	DATA_BLOB new_random_pass;
	char *newpass;
	char *oldpass;
	struct dcerpc_binding_handle *b = p->binding_handle;
	uint8_t old_nt_hash[16], new_nt_hash[16];
	NTTIME t;
	struct samr_DomInfo1 *dominfo = NULL;
	struct userPwdChangeFailureInformation *reject = NULL;

	new_random_pass = samr_very_rand_pass(tctx, 128);

	torture_assert(tctx, *password != NULL,
				   "Failing ChangePasswordUser3 as old password was NULL.  Previous test failed?");

	oldpass = *password;
	server.string = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	init_lsa_String(&account, account_string);

	s.in.user_handle = handle;
	s.in.info = &u;
	s.in.level = 25;

	ZERO_STRUCT(u);

	u.info25.info.fields_present = SAMR_FIELD_NT_PASSWORD_PRESENT;

	set_pw_in_buffer(u.info25.password.data, &new_random_pass);

	status = dcerpc_fetch_session_key(p, &session_key);
	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(tctx, "SetUserInfo level %u - no session key - %s\n",
		       s.in.level, nt_errstr(status));
		return false;
	}

	generate_random_buffer((uint8_t *)confounder, 16);

	MD5Init(&ctx);
	MD5Update(&ctx, confounder, 16);
	MD5Update(&ctx, session_key.data, session_key.length);
	MD5Final(confounded_session_key.data, &ctx);

	arcfour_crypt_blob(u.info25.password.data, 516, &confounded_session_key);
	memcpy(&u.info25.password.data[516], confounder, 16);

	torture_comment(tctx, "Testing SetUserInfo level 25 (set password ex) with a password made up of only random bytes\n");

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetUserInfo_r(b, tctx, &s),
		"SetUserInfo failed");
	if (!NT_STATUS_IS_OK(s.out.result)) {
		torture_warning(tctx, "SetUserInfo level %u failed - %s\n",
		       s.in.level, nt_errstr(s.out.result));
		ret = false;
	}

	torture_comment(tctx, "Testing ChangePasswordUser3 with a password made up of only random bytes\n");

	mdfour(old_nt_hash, new_random_pass.data, new_random_pass.length);

	new_random_pass = samr_very_rand_pass(tctx, 128);

	mdfour(new_nt_hash, new_random_pass.data, new_random_pass.length);

	set_pw_in_buffer(nt_pass.data, &new_random_pass);
	arcfour_crypt(nt_pass.data, old_nt_hash, 516);
	E_old_pw_hash(new_nt_hash, old_nt_hash, nt_verifier.hash);

	r.in.server = &server;
	r.in.account = &account;
	r.in.nt_password = &nt_pass;
	r.in.nt_verifier = &nt_verifier;
	r.in.lm_change = 0;
	r.in.lm_password = NULL;
	r.in.lm_verifier = NULL;
	r.in.password3 = NULL;
	r.out.dominfo = &dominfo;
	r.out.reject = &reject;

	unix_to_nt_time(&t, time(NULL));

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_ChangePasswordUser3_r(b, tctx, &r),
		"ChangePasswordUser3 failed");

	if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_PASSWORD_RESTRICTION)) {
		if (reject && reject->extendedFailureReason != SAM_PWD_CHANGE_NO_ERROR) {
			torture_warning(tctx, "expected SAM_PWD_CHANGE_NO_ERROR (%d), got %d\n",
			       SAM_PWD_CHANGE_NO_ERROR, reject->extendedFailureReason);
			return false;
		}
		/* Perhaps the server has a 'min password age' set? */

	} else if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_warning(tctx, "ChangePasswordUser3 failed - %s\n", nt_errstr(r.out.result));
		ret = false;
	}

	newpass = samr_rand_pass(tctx, 128);

	mdfour(old_nt_hash, new_random_pass.data, new_random_pass.length);

	E_md4hash(newpass, new_nt_hash);

	encode_pw_buffer(nt_pass.data, newpass, STR_UNICODE);
	arcfour_crypt(nt_pass.data, old_nt_hash, 516);
	E_old_pw_hash(new_nt_hash, old_nt_hash, nt_verifier.hash);

	r.in.server = &server;
	r.in.account = &account;
	r.in.nt_password = &nt_pass;
	r.in.nt_verifier = &nt_verifier;
	r.in.lm_change = 0;
	r.in.lm_password = NULL;
	r.in.lm_verifier = NULL;
	r.in.password3 = NULL;
	r.out.dominfo = &dominfo;
	r.out.reject = &reject;

	unix_to_nt_time(&t, time(NULL));

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_ChangePasswordUser3_r(b, tctx, &r),
		"ChangePasswordUser3 failed");

	if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_PASSWORD_RESTRICTION)) {
		if (reject && reject->extendedFailureReason != SAM_PWD_CHANGE_NO_ERROR) {
			torture_warning(tctx, "expected SAM_PWD_CHANGE_NO_ERROR (%d), got %d\n",
			       SAM_PWD_CHANGE_NO_ERROR, reject->extendedFailureReason);
			return false;
		}
		/* Perhaps the server has a 'min password age' set? */

	} else {
		torture_assert_ntstatus_ok(tctx, r.out.result, "ChangePasswordUser3 (on second random password)");
		*password = talloc_strdup(tctx, newpass);
	}

	return ret;
}


static bool test_GetMembersInAlias(struct dcerpc_binding_handle *b,
				   struct torture_context *tctx,
				   struct policy_handle *alias_handle)
{
	struct samr_GetMembersInAlias r;
	struct lsa_SidArray sids;

	torture_comment(tctx, "Testing GetMembersInAlias\n");

	r.in.alias_handle = alias_handle;
	r.out.sids = &sids;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetMembersInAlias_r(b, tctx, &r),
		"GetMembersInAlias failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "GetMembersInAlias failed");

	return true;
}

static bool test_AddMemberToAlias(struct dcerpc_binding_handle *b,
				  struct torture_context *tctx,
				  struct policy_handle *alias_handle,
				  const struct dom_sid *domain_sid)
{
	struct samr_AddAliasMember r;
	struct samr_DeleteAliasMember d;
	struct dom_sid *sid;

	sid = dom_sid_add_rid(tctx, domain_sid, 512);

	torture_comment(tctx, "Testing AddAliasMember\n");
	r.in.alias_handle = alias_handle;
	r.in.sid = sid;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_AddAliasMember_r(b, tctx, &r),
		"AddAliasMember failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "AddAliasMember failed");

	d.in.alias_handle = alias_handle;
	d.in.sid = sid;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_DeleteAliasMember_r(b, tctx, &d),
		"DeleteAliasMember failed");
	torture_assert_ntstatus_ok(tctx, d.out.result, "DelAliasMember failed");

	return true;
}

static bool test_AddMultipleMembersToAlias(struct dcerpc_binding_handle *b,
					   struct torture_context *tctx,
					   struct policy_handle *alias_handle)
{
	struct samr_AddMultipleMembersToAlias a;
	struct samr_RemoveMultipleMembersFromAlias r;
	struct lsa_SidArray sids;

	torture_comment(tctx, "Testing AddMultipleMembersToAlias\n");
	a.in.alias_handle = alias_handle;
	a.in.sids = &sids;

	sids.num_sids = 3;
	sids.sids = talloc_array(tctx, struct lsa_SidPtr, 3);

	sids.sids[0].sid = dom_sid_parse_talloc(tctx, "S-1-5-32-1-2-3-1");
	sids.sids[1].sid = dom_sid_parse_talloc(tctx, "S-1-5-32-1-2-3-2");
	sids.sids[2].sid = dom_sid_parse_talloc(tctx, "S-1-5-32-1-2-3-3");

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_AddMultipleMembersToAlias_r(b, tctx, &a),
		"AddMultipleMembersToAlias failed");
	torture_assert_ntstatus_ok(tctx, a.out.result, "AddMultipleMembersToAlias");


	torture_comment(tctx, "Testing RemoveMultipleMembersFromAlias\n");
	r.in.alias_handle = alias_handle;
	r.in.sids = &sids;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_RemoveMultipleMembersFromAlias_r(b, tctx, &r),
		"RemoveMultipleMembersFromAlias failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "RemoveMultipleMembersFromAlias failed");

	/* strange! removing twice doesn't give any error */
	torture_assert_ntstatus_ok(tctx, dcerpc_samr_RemoveMultipleMembersFromAlias_r(b, tctx, &r),
		"RemoveMultipleMembersFromAlias failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "RemoveMultipleMembersFromAlias failed");

	/* but removing an alias that isn't there does */
	sids.sids[2].sid = dom_sid_parse_talloc(tctx, "S-1-5-32-1-2-3-4");

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_RemoveMultipleMembersFromAlias_r(b, tctx, &r),
		"RemoveMultipleMembersFromAlias failed");
	torture_assert_ntstatus_equal(tctx, r.out.result, NT_STATUS_OBJECT_NAME_NOT_FOUND, "RemoveMultipleMembersFromAlias");

	return true;
}

static bool test_GetAliasMembership(struct dcerpc_binding_handle *b,
				    struct torture_context *tctx,
				    struct policy_handle *domain_handle)
{
	struct samr_GetAliasMembership r;
	struct lsa_SidArray sids;
	struct samr_Ids rids;

	torture_comment(tctx, "Testing GetAliasMembership\n");

	r.in.domain_handle	= domain_handle;
	r.in.sids		= &sids;
	r.out.rids		= &rids;

	sids.num_sids = 0;
	sids.sids = talloc_zero_array(tctx, struct lsa_SidPtr, sids.num_sids);

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetAliasMembership_r(b, tctx, &r),
		"GetAliasMembership failed");
	torture_assert_ntstatus_ok(tctx, r.out.result,
		"samr_GetAliasMembership failed");

	torture_assert_int_equal(tctx, sids.num_sids, rids.count,
		"protocol misbehaviour");

	sids.num_sids = 1;
	sids.sids = talloc_zero_array(tctx, struct lsa_SidPtr, sids.num_sids);
	sids.sids[0].sid = dom_sid_parse_talloc(tctx, "S-1-5-32-1-2-3-1");

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetAliasMembership_r(b, tctx, &r),
		"samr_GetAliasMembership failed");
	torture_assert_ntstatus_ok(tctx, r.out.result,
		"samr_GetAliasMembership failed");

#if 0
	/* only true for w2k8 it seems
	 * win7, xp, w2k3 will return a 0 length array pointer */

	if (rids.ids && (rids.count == 0)) {
		torture_fail(tctx, "samr_GetAliasMembership returned 0 count and a rids array");
	}
#endif
	if (!rids.ids && rids.count) {
		torture_fail(tctx, "samr_GetAliasMembership returned non-0 count but no rids");
	}

	return true;
}

static bool test_TestPrivateFunctionsUser(struct dcerpc_binding_handle *b,
					  struct torture_context *tctx,
					  struct policy_handle *user_handle)
{
    	struct samr_TestPrivateFunctionsUser r;

	torture_comment(tctx, "Testing TestPrivateFunctionsUser\n");

	r.in.user_handle = user_handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_TestPrivateFunctionsUser_r(b, tctx, &r),
		"TestPrivateFunctionsUser failed");
	torture_assert_ntstatus_equal(tctx, r.out.result, NT_STATUS_NOT_IMPLEMENTED, "TestPrivateFunctionsUser");

	return true;
}

static bool test_QueryUserInfo_pwdlastset(struct dcerpc_binding_handle *b,
					  struct torture_context *tctx,
					  struct policy_handle *handle,
					  bool use_info2,
					  NTTIME *pwdlastset)
{
	NTSTATUS status;
	uint16_t levels[] = { /* 3, */ 5, 21 };
	int i;
	NTTIME pwdlastset3 = 0;
	NTTIME pwdlastset5 = 0;
	NTTIME pwdlastset21 = 0;

	torture_comment(tctx, "Testing QueryUserInfo%s level 5 and 21 call ",
			use_info2 ? "2":"");

	for (i=0; i<ARRAY_SIZE(levels); i++) {

		struct samr_QueryUserInfo r;
		struct samr_QueryUserInfo2 r2;
		union samr_UserInfo *info;

		if (use_info2) {
			r2.in.user_handle = handle;
			r2.in.level = levels[i];
			r2.out.info = &info;
			torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryUserInfo2_r(b, tctx, &r2),
				"QueryUserInfo2 failed");
			status = r2.out.result;

		} else {
			r.in.user_handle = handle;
			r.in.level = levels[i];
			r.out.info = &info;
			torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryUserInfo_r(b, tctx, &r),
				"QueryUserInfo failed");
			status = r.out.result;
		}

		if (!NT_STATUS_IS_OK(status) &&
		    !NT_STATUS_EQUAL(status, NT_STATUS_INVALID_INFO_CLASS)) {
			torture_warning(tctx, "QueryUserInfo%s level %u failed - %s\n",
			       use_info2 ? "2":"", levels[i], nt_errstr(status));
			return false;
		}

		switch (levels[i]) {
		case 3:
			pwdlastset3 = info->info3.last_password_change;
			break;
		case 5:
			pwdlastset5 = info->info5.last_password_change;
			break;
		case 21:
			pwdlastset21 = info->info21.last_password_change;
			break;
		default:
			return false;
		}
	}
	/* torture_assert_int_equal(tctx, pwdlastset3, pwdlastset5,
				    "pwdlastset mixup"); */
	torture_assert_int_equal(tctx, pwdlastset5, pwdlastset21,
				 "pwdlastset mixup");

	*pwdlastset = pwdlastset21;

	torture_comment(tctx, "(pwdlastset: %llu)\n",
			(unsigned long long) *pwdlastset);

	return true;
}

static bool test_SamLogon(struct torture_context *tctx,
			  struct dcerpc_pipe *p,
			  struct cli_credentials *test_credentials,
			  NTSTATUS expected_result,
			  bool interactive)
{
	NTSTATUS status;
	struct netr_LogonSamLogonEx r;
	union netr_LogonLevel logon;
	union netr_Validation validation;
	uint8_t authoritative;
	struct netr_IdentityInfo identity;
	struct netr_NetworkInfo ninfo;
	struct netr_PasswordInfo pinfo;
	DATA_BLOB names_blob, chal, lm_resp, nt_resp;
	int flags = CLI_CRED_NTLM_AUTH;
	uint32_t samlogon_flags = 0;
	struct netlogon_creds_CredentialState *creds;
	struct netr_Authenticator a;
	struct dcerpc_binding_handle *b = p->binding_handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_schannel_creds(p->conn->security_state.generic_state, tctx, &creds), "");

	if (lpcfg_client_lanman_auth(tctx->lp_ctx)) {
		flags |= CLI_CRED_LANMAN_AUTH;
	}

	if (lpcfg_client_ntlmv2_auth(tctx->lp_ctx)) {
		flags |= CLI_CRED_NTLMv2_AUTH;
	}

	cli_credentials_get_ntlm_username_domain(test_credentials, tctx,
						 &identity.account_name.string,
						 &identity.domain_name.string);

	identity.parameter_control =
		MSV1_0_ALLOW_SERVER_TRUST_ACCOUNT |
		MSV1_0_ALLOW_WORKSTATION_TRUST_ACCOUNT;
	identity.logon_id_low = 0;
	identity.logon_id_high = 0;
	identity.workstation.string = cli_credentials_get_workstation(test_credentials);

	if (interactive) {
		netlogon_creds_client_authenticator(creds, &a);

		if (!E_deshash(cli_credentials_get_password(test_credentials), pinfo.lmpassword.hash)) {
			ZERO_STRUCT(pinfo.lmpassword.hash);
		}
		E_md4hash(cli_credentials_get_password(test_credentials), pinfo.ntpassword.hash);

		if (creds->negotiate_flags & NETLOGON_NEG_ARCFOUR) {
			netlogon_creds_arcfour_crypt(creds, pinfo.lmpassword.hash, 16);
			netlogon_creds_arcfour_crypt(creds, pinfo.ntpassword.hash, 16);
		} else {
			netlogon_creds_des_encrypt(creds, &pinfo.lmpassword);
			netlogon_creds_des_encrypt(creds, &pinfo.ntpassword);
		}

		pinfo.identity_info = identity;
		logon.password = &pinfo;

		r.in.logon_level = NetlogonInteractiveInformation;
	} else {
		generate_random_buffer(ninfo.challenge,
				       sizeof(ninfo.challenge));
		chal = data_blob_const(ninfo.challenge,
				       sizeof(ninfo.challenge));

		names_blob = NTLMv2_generate_names_blob(tctx, cli_credentials_get_workstation(test_credentials),
							cli_credentials_get_domain(test_credentials));

		status = cli_credentials_get_ntlm_response(test_credentials, tctx,
							   &flags,
							   chal,
							   names_blob,
							   &lm_resp, &nt_resp,
							   NULL, NULL);
		torture_assert_ntstatus_ok(tctx, status, "cli_credentials_get_ntlm_response failed");

		ninfo.lm.data = lm_resp.data;
		ninfo.lm.length = lm_resp.length;

		ninfo.nt.data = nt_resp.data;
		ninfo.nt.length = nt_resp.length;

		ninfo.identity_info = identity;
		logon.network = &ninfo;

		r.in.logon_level = NetlogonNetworkInformation;
	}

	r.in.server_name = talloc_asprintf(tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.computer_name = cli_credentials_get_workstation(test_credentials);
	r.in.logon = &logon;
	r.in.flags = &samlogon_flags;
	r.out.flags = &samlogon_flags;
	r.out.validation = &validation;
	r.out.authoritative = &authoritative;

	torture_comment(tctx, "Testing LogonSamLogon with name %s\n", identity.account_name.string);

	r.in.validation_level = 6;

	torture_assert_ntstatus_ok(tctx, dcerpc_netr_LogonSamLogonEx_r(b, tctx, &r),
		"netr_LogonSamLogonEx failed");
	if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_INVALID_INFO_CLASS)) {
		r.in.validation_level = 3;
		torture_assert_ntstatus_ok(tctx, dcerpc_netr_LogonSamLogonEx_r(b, tctx, &r),
			"netr_LogonSamLogonEx failed");
	}
	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_assert_ntstatus_equal(tctx, r.out.result, expected_result, "LogonSamLogonEx failed");
		return true;
	} else {
		torture_assert_ntstatus_ok(tctx, r.out.result, "LogonSamLogonEx failed");
	}

	return true;
}

static bool test_SamLogon_with_creds(struct torture_context *tctx,
				     struct dcerpc_pipe *p,
				     struct cli_credentials *machine_creds,
				     const char *acct_name,
				     const char *password,
				     NTSTATUS expected_samlogon_result,
				     bool interactive)
{
	bool ret = true;
	struct cli_credentials *test_credentials;

	test_credentials = cli_credentials_init(tctx);

	cli_credentials_set_workstation(test_credentials,
					cli_credentials_get_workstation(machine_creds), CRED_SPECIFIED);
	cli_credentials_set_domain(test_credentials,
				   cli_credentials_get_domain(machine_creds), CRED_SPECIFIED);
	cli_credentials_set_username(test_credentials,
				     acct_name, CRED_SPECIFIED);
	cli_credentials_set_password(test_credentials,
				     password, CRED_SPECIFIED);

	torture_comment(tctx, "Testing samlogon (%s) as %s password: %s\n",
		interactive ? "interactive" : "network", acct_name, password);

	if (!test_SamLogon(tctx, p, test_credentials,
			    expected_samlogon_result, interactive)) {
		torture_warning(tctx, "new password did not work\n");
		ret = false;
	}

	return ret;
}

static bool test_SetPassword_level(struct dcerpc_pipe *p,
				   struct dcerpc_pipe *np,
				   struct torture_context *tctx,
				   struct policy_handle *handle,
				   uint16_t level,
				   uint32_t fields_present,
				   uint8_t password_expired,
				   bool *matched_expected_error,
				   bool use_setinfo2,
				   const char *acct_name,
				   char **password,
				   struct cli_credentials *machine_creds,
				   bool use_queryinfo2,
				   NTTIME *pwdlastset,
				   NTSTATUS expected_samlogon_result)
{
	const char *fields = NULL;
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;

	switch (level) {
	case 21:
	case 23:
	case 25:
		fields = talloc_asprintf(tctx, "(fields_present: 0x%08x)",
					 fields_present);
		break;
	default:
		break;
	}

	torture_comment(tctx, "Testing SetUserInfo%s level %d call "
		"(password_expired: %d) %s\n",
		use_setinfo2 ? "2":"", level, password_expired,
		fields ? fields : "");

	if (!test_SetUserPass_level_ex(p, tctx, handle, level,
				       fields_present,
				       password,
				       password_expired,
				       use_setinfo2,
				       matched_expected_error)) {
		ret = false;
	}

	if (!test_QueryUserInfo_pwdlastset(b, tctx, handle,
					   use_queryinfo2,
					   pwdlastset)) {
		ret = false;
	}

	if (*matched_expected_error == true) {
		return ret;
	}

	if (!test_SamLogon_with_creds(tctx, np,
				      machine_creds,
				      acct_name,
				      *password,
				      expected_samlogon_result,
				      false)) {
		ret = false;
	}

	return ret;
}

static bool setup_schannel_netlogon_pipe(struct torture_context *tctx,
					 struct cli_credentials *credentials,
					 struct dcerpc_pipe **p)
{
	struct dcerpc_binding *b;

	torture_assert_ntstatus_ok(tctx, torture_rpc_binding(tctx, &b),
		"failed to get rpc binding");

	/* We have to use schannel, otherwise the SamLogonEx fails
	 * with INTERNAL_ERROR */

	b->flags &= ~DCERPC_AUTH_OPTIONS;
	b->flags |= DCERPC_SCHANNEL | DCERPC_SIGN | DCERPC_SCHANNEL_128;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_pipe_connect_b(tctx, p, b, &ndr_table_netlogon,
				      credentials, tctx->ev, tctx->lp_ctx),
		"failed to bind to netlogon");

	return true;
}

static bool test_SetPassword_pwdlastset(struct dcerpc_pipe *p,
					struct torture_context *tctx,
					uint32_t acct_flags,
					const char *acct_name,
					struct policy_handle *handle,
					char **password,
					struct cli_credentials *machine_credentials)
{
	int s = 0, q = 0, f = 0, l = 0, z = 0;
	bool ret = true;
	int delay = 50000;
	bool set_levels[] = { false, true };
	bool query_levels[] = { false, true };
	uint32_t levels[] = { 18, 21, 26, 23, 24, 25 }; /* Second half only used when TEST_ALL_LEVELS defined */
	uint32_t nonzeros[] = { 1, 24 };
	uint32_t fields_present[] = {
		0,
		SAMR_FIELD_EXPIRED_FLAG,
		SAMR_FIELD_LAST_PWD_CHANGE,
		SAMR_FIELD_EXPIRED_FLAG | SAMR_FIELD_LAST_PWD_CHANGE,
		SAMR_FIELD_COMMENT,
		SAMR_FIELD_NT_PASSWORD_PRESENT,
		SAMR_FIELD_NT_PASSWORD_PRESENT | SAMR_FIELD_LAST_PWD_CHANGE,
		SAMR_FIELD_NT_PASSWORD_PRESENT | SAMR_FIELD_LM_PASSWORD_PRESENT,
		SAMR_FIELD_NT_PASSWORD_PRESENT | SAMR_FIELD_LM_PASSWORD_PRESENT | SAMR_FIELD_LAST_PWD_CHANGE,
		SAMR_FIELD_NT_PASSWORD_PRESENT | SAMR_FIELD_EXPIRED_FLAG,
		SAMR_FIELD_NT_PASSWORD_PRESENT | SAMR_FIELD_LM_PASSWORD_PRESENT | SAMR_FIELD_EXPIRED_FLAG,
		SAMR_FIELD_NT_PASSWORD_PRESENT | SAMR_FIELD_LM_PASSWORD_PRESENT | SAMR_FIELD_LAST_PWD_CHANGE | SAMR_FIELD_EXPIRED_FLAG
	};
	struct dcerpc_pipe *np = NULL;

	if (torture_setting_bool(tctx, "samba3", false) ||
	    torture_setting_bool(tctx, "samba4", false)) {
		delay = 999999;
		torture_comment(tctx, "Samba3 has second granularity, setting delay to: %d\n",
			delay);
	}

	torture_assert(tctx, setup_schannel_netlogon_pipe(tctx, machine_credentials, &np), "");

	/* set to 1 to enable testing for all possible opcode
	   (SetUserInfo, SetUserInfo2, QueryUserInfo, QueryUserInfo2)
	   combinations */
#if 0
#define TEST_ALL_LEVELS 1
#define TEST_SET_LEVELS 1
#define TEST_QUERY_LEVELS 1
#endif
#ifdef TEST_ALL_LEVELS
	for (l=0; l<ARRAY_SIZE(levels); l++) {
#else
	for (l=0; l<(ARRAY_SIZE(levels))/2; l++) {
#endif
	for (z=0; z<ARRAY_SIZE(nonzeros); z++) {
	for (f=0; f<ARRAY_SIZE(fields_present); f++) {
#ifdef TEST_SET_LEVELS
	for (s=0; s<ARRAY_SIZE(set_levels); s++) {
#endif
#ifdef TEST_QUERY_LEVELS
	for (q=0; q<ARRAY_SIZE(query_levels); q++) {
#endif
		NTTIME pwdlastset_old = 0;
		NTTIME pwdlastset_new = 0;
		bool matched_expected_error = false;
		NTSTATUS expected_samlogon_result = NT_STATUS_ACCOUNT_DISABLED;

		torture_comment(tctx, "------------------------------\n"
				"Testing pwdLastSet attribute for flags: 0x%08x "
				"(s: %d (l: %d), q: %d)\n",
				acct_flags, s, levels[l], q);

		switch (levels[l]) {
		case 21:
		case 23:
		case 25:
			if (!((fields_present[f] & SAMR_FIELD_NT_PASSWORD_PRESENT) ||
			      (fields_present[f] & SAMR_FIELD_LM_PASSWORD_PRESENT))) {
				expected_samlogon_result = NT_STATUS_WRONG_PASSWORD;
			}
			break;
		}


		/* set #1 */

		/* set a password and force password change (pwdlastset 0) by
		 * setting the password expired flag to a non-0 value */

		if (!test_SetPassword_level(p, np, tctx, handle,
					    levels[l],
					    fields_present[f],
					    nonzeros[z],
					    &matched_expected_error,
					    set_levels[s],
					    acct_name,
					    password,
					    machine_credentials,
					    query_levels[q],
					    &pwdlastset_new,
					    expected_samlogon_result)) {
			ret = false;
		}

		if (matched_expected_error == true) {
			/* skipping on expected failure */
			continue;
		}

		/* pwdlastset must be 0 afterwards, except for a level 21, 23 and 25
		 * set without the SAMR_FIELD_EXPIRED_FLAG */

		switch (levels[l]) {
		case 21:
		case 23:
		case 25:
			if ((pwdlastset_new != 0) &&
			    !(fields_present[f] & SAMR_FIELD_EXPIRED_FLAG)) {
				torture_comment(tctx, "not considering a non-0 "
					"pwdLastSet as a an error as the "
					"SAMR_FIELD_EXPIRED_FLAG has not "
					"been set\n");
				break;
			}
			break;
		default:
			if (pwdlastset_new != 0) {
				torture_warning(tctx, "pwdLastSet test failed: "
					"expected pwdLastSet 0 but got %llu\n",
					(unsigned long long) pwdlastset_old);
				ret = false;
			}
			break;
		}

		switch (levels[l]) {
		case 21:
		case 23:
		case 25:
			if (((fields_present[f] & SAMR_FIELD_NT_PASSWORD_PRESENT) ||
			     (fields_present[f] & SAMR_FIELD_LM_PASSWORD_PRESENT)) &&
			     (pwdlastset_old > 0) && (pwdlastset_new > 0) &&
			     (pwdlastset_old >= pwdlastset_new)) {
				torture_warning(tctx, "pwdlastset not increasing\n");
				ret = false;
			}
			break;
		}

		pwdlastset_old = pwdlastset_new;

		usleep(delay);

		/* set #2 */

		/* set a password, pwdlastset needs to get updated (increased
		 * value), password_expired value used here is 0 */

		if (!test_SetPassword_level(p, np, tctx, handle,
					    levels[l],
					    fields_present[f],
					    0,
					    &matched_expected_error,
					    set_levels[s],
					    acct_name,
					    password,
					    machine_credentials,
					    query_levels[q],
					    &pwdlastset_new,
					    expected_samlogon_result)) {
			ret = false;
		}

		/* when a password has been changed, pwdlastset must not be 0 afterwards
		 * and must be larger then the old value */

		switch (levels[l]) {
		case 21:
		case 23:
		case 25:
			/* SAMR_FIELD_EXPIRED_FLAG has not been set and no
			 * password has been changed, old and new pwdlastset
			 * need to be the same value */

			if (!(fields_present[f] & SAMR_FIELD_EXPIRED_FLAG) &&
			    !((fields_present[f] & SAMR_FIELD_NT_PASSWORD_PRESENT) ||
			      (fields_present[f] & SAMR_FIELD_LM_PASSWORD_PRESENT)))
			{
				torture_assert_int_equal(tctx, pwdlastset_old,
					pwdlastset_new, "pwdlastset must be equal");
				break;
			}
			break;
		default:
			if (pwdlastset_old >= pwdlastset_new) {
				torture_warning(tctx, "pwdLastSet test failed: "
					"expected last pwdlastset (%llu) < new pwdlastset (%llu)\n",
					(unsigned long long) pwdlastset_old,
					(unsigned long long) pwdlastset_new);
				ret = false;
			}
			if (pwdlastset_new == 0) {
				torture_warning(tctx, "pwdLastSet test failed: "
					"expected non-0 pwdlastset, got: %llu\n",
					(unsigned long long) pwdlastset_new);
				ret = false;
			}
			break;
		}

		switch (levels[l]) {
		case 21:
		case 23:
		case 25:
			if (((fields_present[f] & SAMR_FIELD_NT_PASSWORD_PRESENT) ||
			     (fields_present[f] & SAMR_FIELD_LM_PASSWORD_PRESENT)) &&
			     (pwdlastset_old > 0) && (pwdlastset_new > 0) &&
			     (pwdlastset_old >= pwdlastset_new)) {
				torture_warning(tctx, "pwdlastset not increasing\n");
				ret = false;
			}
			break;
		}

		pwdlastset_old = pwdlastset_new;

		usleep(delay);

		/* set #2b */

		/* set a password, pwdlastset needs to get updated (increased
		 * value), password_expired value used here is 0 */

		if (!test_SetPassword_level(p, np, tctx, handle,
					    levels[l],
					    fields_present[f],
					    0,
					    &matched_expected_error,
					    set_levels[s],
					    acct_name,
					    password,
					    machine_credentials,
					    query_levels[q],
					    &pwdlastset_new,
					    expected_samlogon_result)) {
			ret = false;
		}

		/* when a password has been changed, pwdlastset must not be 0 afterwards
		 * and must be larger then the old value */

		switch (levels[l]) {
		case 21:
		case 23:
		case 25:

			/* SAMR_FIELD_EXPIRED_FLAG has not been set and no
			 * password has been changed, old and new pwdlastset
			 * need to be the same value */

			if (!(fields_present[f] & SAMR_FIELD_EXPIRED_FLAG) &&
			    !((fields_present[f] & SAMR_FIELD_NT_PASSWORD_PRESENT) ||
			      (fields_present[f] & SAMR_FIELD_LM_PASSWORD_PRESENT)))
			{
				torture_assert_int_equal(tctx, pwdlastset_old,
					pwdlastset_new, "pwdlastset must be equal");
				break;
			}
			break;
		default:
			if (pwdlastset_old >= pwdlastset_new) {
				torture_warning(tctx, "pwdLastSet test failed: "
					"expected last pwdlastset (%llu) < new pwdlastset (%llu)\n",
					(unsigned long long) pwdlastset_old,
					(unsigned long long) pwdlastset_new);
				ret = false;
			}
			if (pwdlastset_new == 0) {
				torture_warning(tctx, "pwdLastSet test failed: "
					"expected non-0 pwdlastset, got: %llu\n",
					(unsigned long long) pwdlastset_new);
				ret = false;
			}
			break;
		}

		switch (levels[l]) {
		case 21:
		case 23:
		case 25:
			if (((fields_present[f] & SAMR_FIELD_NT_PASSWORD_PRESENT) ||
			     (fields_present[f] & SAMR_FIELD_LM_PASSWORD_PRESENT)) &&
			     (pwdlastset_old > 0) && (pwdlastset_new > 0) &&
			     (pwdlastset_old >= pwdlastset_new)) {
				torture_warning(tctx, "pwdlastset not increasing\n");
				ret = false;
			}
			break;
		}

		pwdlastset_old = pwdlastset_new;

		usleep(delay);

		/* set #3 */

		/* set a password and force password change (pwdlastset 0) by
		 * setting the password expired flag to a non-0 value */

		if (!test_SetPassword_level(p, np, tctx, handle,
					    levels[l],
					    fields_present[f],
					    nonzeros[z],
					    &matched_expected_error,
					    set_levels[s],
					    acct_name,
					    password,
					    machine_credentials,
					    query_levels[q],
					    &pwdlastset_new,
					    expected_samlogon_result)) {
			ret = false;
		}

		/* pwdlastset must be 0 afterwards, except for a level 21, 23 and 25
		 * set without the SAMR_FIELD_EXPIRED_FLAG */

		switch (levels[l]) {
		case 21:
		case 23:
		case 25:
			if ((pwdlastset_new != 0) &&
			    !(fields_present[f] & SAMR_FIELD_EXPIRED_FLAG)) {
				torture_comment(tctx, "not considering a non-0 "
					"pwdLastSet as a an error as the "
					"SAMR_FIELD_EXPIRED_FLAG has not "
					"been set\n");
				break;
			}

			/* SAMR_FIELD_EXPIRED_FLAG has not been set and no
			 * password has been changed, old and new pwdlastset
			 * need to be the same value */

			if (!(fields_present[f] & SAMR_FIELD_EXPIRED_FLAG) &&
			    !((fields_present[f] & SAMR_FIELD_NT_PASSWORD_PRESENT) ||
			      (fields_present[f] & SAMR_FIELD_LM_PASSWORD_PRESENT)))
			{
				torture_assert_int_equal(tctx, pwdlastset_old,
					pwdlastset_new, "pwdlastset must be equal");
				break;
			}
			break;
		default:
			if (pwdlastset_new != 0) {
				torture_warning(tctx, "pwdLastSet test failed: "
					"expected pwdLastSet 0, got %llu\n",
					(unsigned long long) pwdlastset_old);
				ret = false;
			}
			break;
		}

		switch (levels[l]) {
		case 21:
		case 23:
		case 25:
			if (((fields_present[f] & SAMR_FIELD_NT_PASSWORD_PRESENT) ||
			     (fields_present[f] & SAMR_FIELD_LM_PASSWORD_PRESENT)) &&
			     (pwdlastset_old > 0) && (pwdlastset_new > 0) &&
			     (pwdlastset_old >= pwdlastset_new)) {
				torture_warning(tctx, "pwdlastset not increasing\n");
				ret = false;
			}
			break;
		}

		/* if the level we are testing does not have a fields_present
		 * field, skip all fields present tests by setting f to to
		 * arraysize */
		switch (levels[l]) {
		case 18:
		case 24:
		case 26:
			f = ARRAY_SIZE(fields_present);
			break;
		}

#ifdef TEST_QUERY_LEVELS
	}
#endif
#ifdef TEST_SET_LEVELS
	}
#endif
	} /* fields present */
	} /* nonzeros */
	} /* levels */

#undef TEST_SET_LEVELS
#undef TEST_QUERY_LEVELS

	talloc_free(np);

	return ret;
}

static bool test_QueryUserInfo_badpwdcount(struct dcerpc_binding_handle *b,
					   struct torture_context *tctx,
					   struct policy_handle *handle,
					   uint32_t *badpwdcount)
{
	union samr_UserInfo *info;
	struct samr_QueryUserInfo r;

	r.in.user_handle = handle;
	r.in.level = 3;
	r.out.info = &info;

	torture_comment(tctx, "Testing QueryUserInfo level %d", r.in.level);

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryUserInfo_r(b, tctx, &r),
		"failed to query userinfo");
	torture_assert_ntstatus_ok(tctx, r.out.result,
		"failed to query userinfo");

	*badpwdcount = info->info3.bad_password_count;

	torture_comment(tctx, " (bad password count: %d)\n", *badpwdcount);

	return true;
}

static bool test_SetUserInfo_acct_flags(struct dcerpc_binding_handle *b,
					struct torture_context *tctx,
					struct policy_handle *user_handle,
					uint32_t acct_flags)
{
	struct samr_SetUserInfo r;
	union samr_UserInfo user_info;

	torture_comment(tctx, "Testing SetUserInfo level 16\n");

	user_info.info16.acct_flags = acct_flags;

	r.in.user_handle = user_handle;
	r.in.level = 16;
	r.in.info = &user_info;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetUserInfo_r(b, tctx, &r),
		"failed to set account flags");
	torture_assert_ntstatus_ok(tctx, r.out.result,
		"failed to set account flags");

	return true;
}

static bool test_reset_badpwdcount(struct dcerpc_pipe *p,
				   struct torture_context *tctx,
				   struct policy_handle *user_handle,
				   uint32_t acct_flags,
				   char **password)
{
	struct dcerpc_binding_handle *b = p->binding_handle;

	torture_assert(tctx, test_SetUserPass(p, tctx, user_handle, password),
		"failed to set password");

	torture_comment(tctx, "Testing SetUserInfo level 16 (enable account)\n");

	torture_assert(tctx,
		       test_SetUserInfo_acct_flags(b, tctx, user_handle,
						   acct_flags & ~ACB_DISABLED),
		       "failed to enable user");

	torture_assert(tctx, test_SetUserPass(p, tctx, user_handle, password),
		"failed to set password");

	return true;
}

static bool test_SetDomainInfo(struct dcerpc_binding_handle *b,
			       struct torture_context *tctx,
			       struct policy_handle *domain_handle,
			       enum samr_DomainInfoClass level,
			       union samr_DomainInfo *info)
{
	struct samr_SetDomainInfo r;

	r.in.domain_handle = domain_handle;
	r.in.level = level;
	r.in.info = info;

	torture_assert_ntstatus_ok(tctx,
				   dcerpc_samr_SetDomainInfo_r(b, tctx, &r),
				   "failed to set domain info");
	torture_assert_ntstatus_ok(tctx, r.out.result,
				   "failed to set domain info");

	return true;
}

static bool test_SetDomainInfo_ntstatus(struct dcerpc_binding_handle *b,
					struct torture_context *tctx,
					struct policy_handle *domain_handle,
					enum samr_DomainInfoClass level,
					union samr_DomainInfo *info,
					NTSTATUS expected)
{
	struct samr_SetDomainInfo r;

	r.in.domain_handle = domain_handle;
	r.in.level = level;
	r.in.info = info;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetDomainInfo_r(b, tctx, &r),
		"SetDomainInfo failed");
	torture_assert_ntstatus_equal(tctx, r.out.result, expected, "");

	return true;
}

static bool test_QueryDomainInfo2_level(struct dcerpc_binding_handle *b,
					struct torture_context *tctx,
					struct policy_handle *domain_handle,
					enum samr_DomainInfoClass level,
					union samr_DomainInfo **q_info)
{
	struct samr_QueryDomainInfo2 r;

	r.in.domain_handle = domain_handle;
	r.in.level = level;
	r.out.info = q_info;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryDomainInfo2_r(b, tctx, &r),
		"failed to query domain info");
	torture_assert_ntstatus_ok(tctx, r.out.result,
		"failed to query domain info");

	return true;
}

static bool test_Password_badpwdcount(struct dcerpc_pipe *p,
				      struct dcerpc_pipe *np,
				      struct torture_context *tctx,
				      uint32_t acct_flags,
				      const char *acct_name,
				      struct policy_handle *domain_handle,
				      struct policy_handle *user_handle,
				      char **password,
				      struct cli_credentials *machine_credentials,
				      const char *comment,
				      bool disable,
				      bool interactive,
				      NTSTATUS expected_success_status,
				      struct samr_DomInfo1 *info1,
				      struct samr_DomInfo12 *info12)
{
	union samr_DomainInfo info;
	char **passwords;
	int i;
	uint32_t badpwdcount, tmp;
	uint32_t password_history_length = 12;
	uint32_t lockout_threshold = 15;
	struct dcerpc_binding_handle *b = p->binding_handle;

	torture_comment(tctx, "\nTesting bad pwd count with: %s\n", comment);

	torture_assert(tctx, password_history_length < lockout_threshold,
		"password history length needs to be smaller than account lockout threshold for this test");


	/* set policies */

	info.info1 = *info1;
	info.info1.password_history_length = password_history_length;

	torture_assert(tctx,
		       test_SetDomainInfo(b, tctx, domain_handle,
					  DomainPasswordInformation, &info),
		       "failed to set password history length");

	info.info12 = *info12;
	info.info12.lockout_threshold = lockout_threshold;

	torture_assert(tctx,
		       test_SetDomainInfo(b, tctx, domain_handle,
					  DomainLockoutInformation, &info),
		       "failed to set lockout threshold");

	/* reset bad pwd count */

	torture_assert(tctx,
		test_reset_badpwdcount(p, tctx, user_handle, acct_flags, password), "");


	/* enable or disable account */
	if (disable) {
		torture_assert(tctx,
			       test_SetUserInfo_acct_flags(b, tctx, user_handle,
						acct_flags | ACB_DISABLED),
			       "failed to disable user");
	} else {
		torture_assert(tctx,
			       test_SetUserInfo_acct_flags(b, tctx, user_handle,
						acct_flags & ~ACB_DISABLED),
			       "failed to enable user");
	}


	/* setup password history */

	passwords = talloc_array(tctx, char *, password_history_length);

	for (i=0; i < password_history_length; i++) {

		torture_assert(tctx, test_SetUserPass(p, tctx, user_handle, password),
			"failed to set password");
		passwords[i] = talloc_strdup(tctx, *password);

		if (!test_SamLogon_with_creds(tctx, np, machine_credentials,
					      acct_name, passwords[i],
					      expected_success_status, interactive)) {
			torture_fail(tctx, "failed to auth with latest password");
		}

		torture_assert(tctx,
			test_QueryUserInfo_badpwdcount(b, tctx, user_handle, &badpwdcount), "");

		torture_assert_int_equal(tctx, badpwdcount, 0, "expected badpwdcount to be 0");
	}


	/* test with wrong password */

	if (!test_SamLogon_with_creds(tctx, np, machine_credentials,
				      acct_name, "random_crap",
				      NT_STATUS_WRONG_PASSWORD, interactive)) {
		torture_fail(tctx, "succeeded to authenticate with wrong password");
	}

	torture_assert(tctx,
		test_QueryUserInfo_badpwdcount(b, tctx, user_handle, &badpwdcount), "");

	torture_assert_int_equal(tctx, badpwdcount, 1, "expected badpwdcount to be 1");


	/* test with latest good password */

	if (!test_SamLogon_with_creds(tctx, np, machine_credentials, acct_name,
				      passwords[password_history_length-1],
				      expected_success_status, interactive)) {
		torture_fail(tctx, "succeeded to authenticate with wrong password");
	}

	torture_assert(tctx,
		test_QueryUserInfo_badpwdcount(b, tctx, user_handle, &badpwdcount), "");

	if (disable) {
		torture_assert_int_equal(tctx, badpwdcount, 1, "expected badpwdcount to be 1");
	} else {
		/* only enabled accounts get the bad pwd count reset upon
		 * successful logon */
		torture_assert_int_equal(tctx, badpwdcount, 0, "expected badpwdcount to be 0");
	}

	tmp = badpwdcount;


	/* test password history */

	for (i=0; i < password_history_length; i++) {

		torture_comment(tctx, "Testing bad password count behavior with "
				      "password #%d of #%d\n", i, password_history_length);

		/* - network samlogon will succeed auth and not
		 *   increase badpwdcount for 2 last entries
		 * - interactive samlogon only for the last one */

		if (i == password_history_length - 1 ||
		    (i == password_history_length - 2 && !interactive)) {

			if (!test_SamLogon_with_creds(tctx, np, machine_credentials,
						      acct_name, passwords[i],
						      expected_success_status, interactive)) {
				torture_fail(tctx, talloc_asprintf(tctx, "succeeded to authenticate with old password (#%d of #%d in history)", i, password_history_length));
			}

			torture_assert(tctx,
				test_QueryUserInfo_badpwdcount(b, tctx, user_handle, &badpwdcount), "");

			if (disable) {
				/* torture_comment(tctx, "expecting bad pwd count to *NOT INCREASE* for pwd history entry %d\n", i); */
				torture_assert_int_equal(tctx, badpwdcount, tmp, "unexpected badpwdcount");
			} else {
				/* torture_comment(tctx, "expecting bad pwd count to be 0 for pwd history entry %d\n", i); */
				torture_assert_int_equal(tctx, badpwdcount, 0, "expected badpwdcount to be 0");
			}

			tmp = badpwdcount;

			continue;
		}

		if (!test_SamLogon_with_creds(tctx, np, machine_credentials,
					      acct_name, passwords[i],
					      NT_STATUS_WRONG_PASSWORD, interactive)) {
			torture_fail(tctx, talloc_asprintf(tctx, "succeeded to authenticate with old password (#%d of #%d in history)", i, password_history_length));
		}

		torture_assert(tctx,
			test_QueryUserInfo_badpwdcount(b, tctx, user_handle, &badpwdcount), "");

		/* - network samlogon will fail auth but not increase
		 *   badpwdcount for 3rd last entry
		 * - interactive samlogon for 3rd and 2nd last entry */

		if (i == password_history_length - 3 ||
		    (i == password_history_length - 2 && interactive)) {
			/* torture_comment(tctx, "expecting bad pwd count to *NOT INCREASE * by one for pwd history entry %d\n", i); */
			torture_assert_int_equal(tctx, badpwdcount, tmp, "unexpected badpwdcount");
		} else {
			/* torture_comment(tctx, "expecting bad pwd count to increase by one for pwd history entry %d\n", i); */
			torture_assert_int_equal(tctx, badpwdcount, tmp + 1, "unexpected badpwdcount");
		}

		tmp = badpwdcount;
	}

	return true;
}

static bool test_Password_badpwdcount_wrap(struct dcerpc_pipe *p,
					   struct torture_context *tctx,
					   uint32_t acct_flags,
					   const char *acct_name,
					   struct policy_handle *domain_handle,
					   struct policy_handle *user_handle,
					   char **password,
					   struct cli_credentials *machine_credentials)
{
	union samr_DomainInfo *q_info, s_info;
	struct samr_DomInfo1 info1, _info1;
	struct samr_DomInfo12 info12, _info12;
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;
	struct dcerpc_pipe *np;
	int i;

	struct {
		const char *comment;
		bool disabled;
		bool interactive;
		NTSTATUS expected_success_status;
	} creds[] = {
		{
			.comment		= "network logon (disabled account)",
			.disabled		= true,
			.interactive		= false,
			.expected_success_status= NT_STATUS_ACCOUNT_DISABLED
		},
		{
			.comment		= "network logon (enabled account)",
			.disabled		= false,
			.interactive		= false,
			.expected_success_status= NT_STATUS_OK
		},
		{
			.comment		= "interactive logon (disabled account)",
			.disabled		= true,
			.interactive		= true,
			.expected_success_status= NT_STATUS_ACCOUNT_DISABLED
		},
		{
			.comment		= "interactive logon (enabled account)",
			.disabled		= false,
			.interactive		= true,
			.expected_success_status= NT_STATUS_OK
		},
	};

	torture_assert(tctx, setup_schannel_netlogon_pipe(tctx, machine_credentials, &np), "");

	/* backup old policies */

	torture_assert(tctx,
		test_QueryDomainInfo2_level(b, tctx, domain_handle,
					    DomainPasswordInformation, &q_info),
		"failed to query domain info level 1");

	info1 = q_info->info1;
	_info1 = info1;

	torture_assert(tctx,
		test_QueryDomainInfo2_level(b, tctx, domain_handle,
					    DomainLockoutInformation, &q_info),
		"failed to query domain info level 12");

	info12 = q_info->info12;
	_info12 = info12;

	/* run tests */

	for (i=0; i < ARRAY_SIZE(creds); i++) {

		/* skip trust tests for now */
		if (acct_flags & ACB_WSTRUST ||
		    acct_flags & ACB_SVRTRUST ||
		    acct_flags & ACB_DOMTRUST) {
			continue;
		}

		ret &= test_Password_badpwdcount(p, np, tctx, acct_flags, acct_name,
						 domain_handle, user_handle, password,
						 machine_credentials,
						 creds[i].comment,
						 creds[i].disabled,
						 creds[i].interactive,
						 creds[i].expected_success_status,
						 &_info1, &_info12);
		if (!ret) {
			torture_warning(tctx, "TEST #%d (%s) failed\n", i, creds[i].comment);
		} else {
			torture_comment(tctx, "TEST #%d (%s) succeeded\n", i, creds[i].comment);
		}
	}

	/* restore policies */

	s_info.info1 = info1;

	torture_assert(tctx,
		       test_SetDomainInfo(b, tctx, domain_handle,
					  DomainPasswordInformation, &s_info),
		       "failed to set password information");

	s_info.info12 = info12;

	torture_assert(tctx,
		       test_SetDomainInfo(b, tctx, domain_handle,
					  DomainLockoutInformation, &s_info),
		       "failed to set lockout information");

	return ret;
}

static bool test_QueryUserInfo_acct_flags(struct dcerpc_binding_handle *b,
					  struct torture_context *tctx,
					  struct policy_handle *handle,
					  uint32_t *acct_flags)
{
	union samr_UserInfo *info;
	struct samr_QueryUserInfo r;

	r.in.user_handle = handle;
	r.in.level = 16;
	r.out.info = &info;

	torture_comment(tctx, "Testing QueryUserInfo level %d", r.in.level);

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryUserInfo_r(b, tctx, &r),
		"failed to query userinfo");
	torture_assert_ntstatus_ok(tctx, r.out.result,
		"failed to query userinfo");

	*acct_flags = info->info16.acct_flags;

	torture_comment(tctx, "  (acct_flags: 0x%08x)\n", *acct_flags);

	return true;
}

static bool test_Password_lockout(struct dcerpc_pipe *p,
				  struct dcerpc_pipe *np,
				  struct torture_context *tctx,
				  uint32_t acct_flags,
				  const char *acct_name,
				  struct policy_handle *domain_handle,
				  struct policy_handle *user_handle,
				  char **password,
				  struct cli_credentials *machine_credentials,
				  const char *comment,
				  bool disable,
				  bool interactive,
				  NTSTATUS expected_success_status,
				  struct samr_DomInfo1 *info1,
				  struct samr_DomInfo12 *info12)
{
	union samr_DomainInfo info;
	uint32_t badpwdcount;
	uint32_t password_history_length = 1;
	uint64_t lockout_threshold = 1;
	uint32_t lockout_seconds = 5;
	uint64_t delta_time_factor = 10 * 1000 * 1000;
	struct dcerpc_binding_handle *b = p->binding_handle;

	torture_comment(tctx, "\nTesting account lockout: %s\n", comment);

	/* set policies */

	info.info1 = *info1;

	torture_comment(tctx, "setting password history length.\n");
	info.info1.password_history_length = password_history_length;

	torture_assert(tctx,
		       test_SetDomainInfo(b, tctx, domain_handle,
					  DomainPasswordInformation, &info),
		       "failed to set password history length");

	info.info12 = *info12;
	info.info12.lockout_threshold = lockout_threshold;

	/* set lockout duration < lockout window: should fail */
	info.info12.lockout_duration = ~(lockout_seconds * delta_time_factor);
	info.info12.lockout_window = ~((lockout_seconds + 1) * delta_time_factor);

	torture_assert(tctx,
		test_SetDomainInfo_ntstatus(b, tctx, domain_handle,
					    DomainLockoutInformation, &info,
					    NT_STATUS_INVALID_PARAMETER),
		"setting lockout duration < lockout window gave unexpected result");

	info.info12.lockout_duration = 0;
	info.info12.lockout_window = 0;

	torture_assert(tctx,
		       test_SetDomainInfo(b, tctx, domain_handle,
					  DomainLockoutInformation, &info),
		       "failed to set lockout window and duration to 0");


	/* set lockout duration of 5 seconds */
	info.info12.lockout_duration = ~(lockout_seconds * delta_time_factor);
	info.info12.lockout_window = ~(lockout_seconds * delta_time_factor);

	torture_assert(tctx,
		       test_SetDomainInfo(b, tctx, domain_handle,
					  DomainLockoutInformation, &info),
		       "failed to set lockout window and duration to 5 seconds");

	/* reset bad pwd count */

	torture_assert(tctx,
		test_reset_badpwdcount(p, tctx, user_handle, acct_flags, password), "");


	/* enable or disable account */

	if (disable) {
		torture_assert(tctx,
			       test_SetUserInfo_acct_flags(b, tctx, user_handle,
						acct_flags | ACB_DISABLED),
			       "failed to disable user");
	} else {
		torture_assert(tctx,
			       test_SetUserInfo_acct_flags(b, tctx, user_handle,
						acct_flags & ~ACB_DISABLED),
			       "failed to enable user");
	}


	/* test logon with right password */

	if (!test_SamLogon_with_creds(tctx, np, machine_credentials,
				      acct_name, *password,
				      expected_success_status, interactive)) {
		torture_fail(tctx, "failed to auth with latest password");
	}

	torture_assert(tctx,
		test_QueryUserInfo_badpwdcount(b, tctx, user_handle, &badpwdcount), "");
	torture_assert_int_equal(tctx, badpwdcount, 0, "expected badpwdcount to be 0");


	/* test with wrong password ==> lockout */

	if (!test_SamLogon_with_creds(tctx, np, machine_credentials,
				      acct_name, "random_crap",
				      NT_STATUS_WRONG_PASSWORD, interactive)) {
		torture_fail(tctx, "succeeded to authenticate with wrong password");
	}

	torture_assert(tctx,
		test_QueryUserInfo_badpwdcount(b, tctx, user_handle, &badpwdcount), "");
	torture_assert_int_equal(tctx, badpwdcount, 1, "expected badpwdcount to be 1");

	torture_assert(tctx,
		test_QueryUserInfo_acct_flags(b, tctx, user_handle, &acct_flags), "");
	torture_assert_int_equal(tctx, acct_flags & ACB_AUTOLOCK, 0,
				 "expected account to be locked");


	/* test with good password */

	if (!test_SamLogon_with_creds(tctx, np, machine_credentials, acct_name,
				     *password,
				     NT_STATUS_ACCOUNT_LOCKED_OUT, interactive))
	{
		torture_fail(tctx, "authenticate did not return NT_STATUS_ACCOUNT_LOCKED_OUT");
	}

	/* bad pwd count should not get updated */
	torture_assert(tctx,
		test_QueryUserInfo_badpwdcount(b, tctx, user_handle, &badpwdcount), "");
	torture_assert_int_equal(tctx, badpwdcount, 1, "expected badpwdcount to be 1");

	/* curiously, windows does _not_ set the autlock flag */
	torture_assert(tctx,
		test_QueryUserInfo_acct_flags(b, tctx, user_handle, &acct_flags), "");
	torture_assert_int_equal(tctx, acct_flags & ACB_AUTOLOCK, 0,
				 "expected account to be locked");


	/* with bad password */

	if (!test_SamLogon_with_creds(tctx, np, machine_credentials,
				      acct_name, "random_crap2",
				      NT_STATUS_ACCOUNT_LOCKED_OUT, interactive))
	{
		torture_fail(tctx, "authenticate did not return NT_STATUS_ACCOUNT_LOCKED_OUT");
	}

	/* bad pwd count should not get updated */
	torture_assert(tctx,
		test_QueryUserInfo_badpwdcount(b, tctx, user_handle, &badpwdcount), "");
	torture_assert_int_equal(tctx, badpwdcount, 1, "expected badpwdcount to be 1");

	/* curiously, windows does _not_ set the autlock flag */
	torture_assert(tctx,
		test_QueryUserInfo_acct_flags(b, tctx, user_handle, &acct_flags), "");
	torture_assert_int_equal(tctx, acct_flags & ACB_AUTOLOCK, 0,
				 "expected account to be locked");


	/* let lockout duration expire ==> unlock */

	torture_comment(tctx, "let lockout duration expire...\n");
	sleep(lockout_seconds + 1);

	if (!test_SamLogon_with_creds(tctx, np, machine_credentials, acct_name,
				     *password,
				     expected_success_status, interactive))
	{
		torture_fail(tctx, "failed to authenticate after lockout expired");
	}

	torture_assert(tctx,
		test_QueryUserInfo_acct_flags(b, tctx, user_handle, &acct_flags), "");
	torture_assert_int_equal(tctx, acct_flags & ACB_AUTOLOCK, 0,
				 "expected account not to be locked");

	return true;
}

static bool test_Password_lockout_wrap(struct dcerpc_pipe *p,
				       struct torture_context *tctx,
				       uint32_t acct_flags,
				       const char *acct_name,
				       struct policy_handle *domain_handle,
				       struct policy_handle *user_handle,
				       char **password,
				       struct cli_credentials *machine_credentials)
{
	union samr_DomainInfo *q_info, s_info;
	struct samr_DomInfo1 info1, _info1;
	struct samr_DomInfo12 info12, _info12;
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;
	struct dcerpc_pipe *np;
	int i;

	struct {
		const char *comment;
		bool disabled;
		bool interactive;
		NTSTATUS expected_success_status;
	} creds[] = {
		{
			.comment		= "network logon (disabled account)",
			.disabled		= true,
			.interactive		= false,
			.expected_success_status= NT_STATUS_ACCOUNT_DISABLED
		},
		{
			.comment		= "network logon (enabled account)",
			.disabled		= false,
			.interactive		= false,
			.expected_success_status= NT_STATUS_OK
		},
		{
			.comment		= "interactive logon (disabled account)",
			.disabled		= true,
			.interactive		= true,
			.expected_success_status= NT_STATUS_ACCOUNT_DISABLED
		},
		{
			.comment		= "interactive logon (enabled account)",
			.disabled		= false,
			.interactive		= true,
			.expected_success_status= NT_STATUS_OK
		},
	};

	torture_assert(tctx, setup_schannel_netlogon_pipe(tctx, machine_credentials, &np), "");

	/* backup old policies */

	torture_assert(tctx,
		test_QueryDomainInfo2_level(b, tctx, domain_handle,
					    DomainPasswordInformation, &q_info),
		"failed to query domain info level 1");

	info1 = q_info->info1;
	_info1 = info1;

	torture_assert(tctx,
		test_QueryDomainInfo2_level(b, tctx, domain_handle,
					    DomainLockoutInformation, &q_info),
		"failed to query domain info level 12");

	info12 = q_info->info12;
	_info12 = info12;

	/* run tests */

	for (i=0; i < ARRAY_SIZE(creds); i++) {

		/* skip trust tests for now */
		if (acct_flags & ACB_WSTRUST ||
		    acct_flags & ACB_SVRTRUST ||
		    acct_flags & ACB_DOMTRUST) {
			continue;
		}

		ret &= test_Password_lockout(p, np, tctx, acct_flags, acct_name,
					     domain_handle, user_handle, password,
					     machine_credentials,
					     creds[i].comment,
					     creds[i].disabled,
					     creds[i].interactive,
					     creds[i].expected_success_status,
					     &_info1, &_info12);
		if (!ret) {
			torture_warning(tctx, "TEST #%d (%s) failed\n", i, creds[i].comment);
		} else {
			torture_comment(tctx, "TEST #%d (%s) succeeded\n", i, creds[i].comment);
		}
	}

	/* restore policies */

	s_info.info1 = info1;

	torture_assert(tctx,
		       test_SetDomainInfo(b, tctx, domain_handle,
					  DomainPasswordInformation, &s_info),
		       "failed to set password information");

	s_info.info12 = info12;

	torture_assert(tctx,
		       test_SetDomainInfo(b, tctx, domain_handle,
					  DomainLockoutInformation, &s_info),
		       "failed to set lockout information");

	return ret;
}

static bool test_DeleteUser_with_privs(struct dcerpc_pipe *p,
				       struct dcerpc_pipe *lp,
				       struct torture_context *tctx,
				       struct policy_handle *domain_handle,
				       struct policy_handle *lsa_handle,
				       struct policy_handle *user_handle,
				       const struct dom_sid *domain_sid,
				       uint32_t rid,
				       struct cli_credentials *machine_credentials)
{
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;
	struct dcerpc_binding_handle *lb = lp->binding_handle;

	struct policy_handle lsa_acct_handle;
	struct dom_sid *user_sid;

	user_sid = dom_sid_add_rid(tctx, domain_sid, rid);

	{
		struct lsa_EnumAccountRights r;
		struct lsa_RightSet rights;

		torture_comment(tctx, "Testing LSA EnumAccountRights\n");

		r.in.handle = lsa_handle;
		r.in.sid = user_sid;
		r.out.rights = &rights;

		torture_assert_ntstatus_ok(tctx, dcerpc_lsa_EnumAccountRights_r(lb, tctx, &r),
			"lsa_EnumAccountRights failed");
		torture_assert_ntstatus_equal(tctx, r.out.result, NT_STATUS_OBJECT_NAME_NOT_FOUND,
			"Expected enum rights for account to fail");
	}

	{
		struct lsa_RightSet rights;
		struct lsa_StringLarge names[2];
		struct lsa_AddAccountRights r;

		torture_comment(tctx, "Testing LSA AddAccountRights\n");

		init_lsa_StringLarge(&names[0], "SeMachineAccountPrivilege");
		init_lsa_StringLarge(&names[1], NULL);

		rights.count = 1;
		rights.names = names;

		r.in.handle = lsa_handle;
		r.in.sid = user_sid;
		r.in.rights = &rights;

		torture_assert_ntstatus_ok(tctx, dcerpc_lsa_AddAccountRights_r(lb, tctx, &r),
			"lsa_AddAccountRights failed");
		torture_assert_ntstatus_ok(tctx, r.out.result,
			"Failed to add privileges");
	}

	{
		struct lsa_EnumAccounts r;
		uint32_t resume_handle = 0;
		struct lsa_SidArray lsa_sid_array;
		int i;
		bool found_sid = false;

		torture_comment(tctx, "Testing LSA EnumAccounts\n");

		r.in.handle = lsa_handle;
		r.in.num_entries = 0x1000;
		r.in.resume_handle = &resume_handle;
		r.out.sids = &lsa_sid_array;
		r.out.resume_handle = &resume_handle;

		torture_assert_ntstatus_ok(tctx, dcerpc_lsa_EnumAccounts_r(lb, tctx, &r),
			"lsa_EnumAccounts failed");
		torture_assert_ntstatus_ok(tctx, r.out.result,
			"Failed to enum accounts");

		for (i=0; i < lsa_sid_array.num_sids; i++) {
			if (dom_sid_equal(user_sid, lsa_sid_array.sids[i].sid)) {
				found_sid = true;
			}
		}

		torture_assert(tctx, found_sid,
			"failed to list privileged account");
	}

	{
		struct lsa_EnumAccountRights r;
		struct lsa_RightSet user_rights;

		torture_comment(tctx, "Testing LSA EnumAccountRights\n");

		r.in.handle = lsa_handle;
		r.in.sid = user_sid;
		r.out.rights = &user_rights;

		torture_assert_ntstatus_ok(tctx, dcerpc_lsa_EnumAccountRights_r(lb, tctx, &r),
			"lsa_EnumAccountRights failed");
		torture_assert_ntstatus_ok(tctx, r.out.result,
			"Failed to enum rights for account");

		if (user_rights.count < 1) {
			torture_warning(tctx, "failed to find newly added rights");
			return false;
		}
	}

	{
		struct lsa_OpenAccount r;

		torture_comment(tctx, "Testing LSA OpenAccount\n");

		r.in.handle = lsa_handle;
		r.in.sid = user_sid;
		r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
		r.out.acct_handle = &lsa_acct_handle;

		torture_assert_ntstatus_ok(tctx, dcerpc_lsa_OpenAccount_r(lb, tctx, &r),
			"lsa_OpenAccount failed");
		torture_assert_ntstatus_ok(tctx, r.out.result,
			"Failed to open lsa account");
	}

	{
		struct lsa_GetSystemAccessAccount r;
		uint32_t access_mask;

		torture_comment(tctx, "Testing LSA GetSystemAccessAccount\n");

		r.in.handle = &lsa_acct_handle;
		r.out.access_mask = &access_mask;

		torture_assert_ntstatus_ok(tctx, dcerpc_lsa_GetSystemAccessAccount_r(lb, tctx, &r),
			"lsa_GetSystemAccessAccount failed");
		torture_assert_ntstatus_ok(tctx, r.out.result,
			"Failed to get lsa system access account");
	}

	{
		struct lsa_Close r;

		torture_comment(tctx, "Testing LSA Close\n");

		r.in.handle = &lsa_acct_handle;
		r.out.handle = &lsa_acct_handle;

		torture_assert_ntstatus_ok(tctx, dcerpc_lsa_Close_r(lb, tctx, &r),
			"lsa_Close failed");
		torture_assert_ntstatus_ok(tctx, r.out.result,
			"Failed to close lsa");
	}

	{
		struct samr_DeleteUser r;

		torture_comment(tctx, "Testing SAMR DeleteUser\n");

		r.in.user_handle = user_handle;
		r.out.user_handle = user_handle;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_DeleteUser_r(b, tctx, &r),
			"DeleteUser failed");
		torture_assert_ntstatus_ok(tctx, r.out.result,
			"DeleteUser failed");
	}

	{
		struct lsa_EnumAccounts r;
		uint32_t resume_handle = 0;
		struct lsa_SidArray lsa_sid_array;
		int i;
		bool found_sid = false;

		torture_comment(tctx, "Testing LSA EnumAccounts\n");

		r.in.handle = lsa_handle;
		r.in.num_entries = 0x1000;
		r.in.resume_handle = &resume_handle;
		r.out.sids = &lsa_sid_array;
		r.out.resume_handle = &resume_handle;

		torture_assert_ntstatus_ok(tctx, dcerpc_lsa_EnumAccounts_r(lb, tctx, &r),
			"lsa_EnumAccounts failed");
		torture_assert_ntstatus_ok(tctx, r.out.result,
			"Failed to enum accounts");

		for (i=0; i < lsa_sid_array.num_sids; i++) {
			if (dom_sid_equal(user_sid, lsa_sid_array.sids[i].sid)) {
				found_sid = true;
			}
		}

		torture_assert(tctx, found_sid,
			"failed to list privileged account");
	}

	{
		struct lsa_EnumAccountRights r;
		struct lsa_RightSet user_rights;

		torture_comment(tctx, "Testing LSA EnumAccountRights\n");

		r.in.handle = lsa_handle;
		r.in.sid = user_sid;
		r.out.rights = &user_rights;

		torture_assert_ntstatus_ok(tctx, dcerpc_lsa_EnumAccountRights_r(lb, tctx, &r),
			"lsa_EnumAccountRights failed");
		torture_assert_ntstatus_ok(tctx, r.out.result,
			"Failed to enum rights for account");

		if (user_rights.count < 1) {
			torture_warning(tctx, "failed to find newly added rights");
			return false;
		}
	}

	{
		struct lsa_OpenAccount r;

		torture_comment(tctx, "Testing LSA OpenAccount\n");

		r.in.handle = lsa_handle;
		r.in.sid = user_sid;
		r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
		r.out.acct_handle = &lsa_acct_handle;

		torture_assert_ntstatus_ok(tctx, dcerpc_lsa_OpenAccount_r(lb, tctx, &r),
			"lsa_OpenAccount failed");
		torture_assert_ntstatus_ok(tctx, r.out.result,
			"Failed to open lsa account");
	}

	{
		struct lsa_GetSystemAccessAccount r;
		uint32_t access_mask;

		torture_comment(tctx, "Testing LSA GetSystemAccessAccount\n");

		r.in.handle = &lsa_acct_handle;
		r.out.access_mask = &access_mask;

		torture_assert_ntstatus_ok(tctx, dcerpc_lsa_GetSystemAccessAccount_r(lb, tctx, &r),
			"lsa_GetSystemAccessAccount failed");
		torture_assert_ntstatus_ok(tctx, r.out.result,
			"Failed to get lsa system access account");
	}

	{
		struct lsa_DeleteObject r;

		torture_comment(tctx, "Testing LSA DeleteObject\n");

		r.in.handle = &lsa_acct_handle;
		r.out.handle = &lsa_acct_handle;

		torture_assert_ntstatus_ok(tctx, dcerpc_lsa_DeleteObject_r(lb, tctx, &r),
			"lsa_DeleteObject failed");
		torture_assert_ntstatus_ok(tctx, r.out.result,
			"Failed to delete object");
	}

	{
		struct lsa_EnumAccounts r;
		uint32_t resume_handle = 0;
		struct lsa_SidArray lsa_sid_array;
		int i;
		bool found_sid = false;

		torture_comment(tctx, "Testing LSA EnumAccounts\n");

		r.in.handle = lsa_handle;
		r.in.num_entries = 0x1000;
		r.in.resume_handle = &resume_handle;
		r.out.sids = &lsa_sid_array;
		r.out.resume_handle = &resume_handle;

		torture_assert_ntstatus_ok(tctx, dcerpc_lsa_EnumAccounts_r(lb, tctx, &r),
			"lsa_EnumAccounts failed");
		torture_assert_ntstatus_ok(tctx, r.out.result,
			"Failed to enum accounts");

		for (i=0; i < lsa_sid_array.num_sids; i++) {
			if (dom_sid_equal(user_sid, lsa_sid_array.sids[i].sid)) {
				found_sid = true;
			}
		}

		torture_assert(tctx, !found_sid,
			"should not have listed privileged account");
	}

	{
		struct lsa_EnumAccountRights r;
		struct lsa_RightSet user_rights;

		torture_comment(tctx, "Testing LSA EnumAccountRights\n");

		r.in.handle = lsa_handle;
		r.in.sid = user_sid;
		r.out.rights = &user_rights;

		torture_assert_ntstatus_ok(tctx, dcerpc_lsa_EnumAccountRights_r(lb, tctx, &r),
			"lsa_EnumAccountRights failed");
		torture_assert_ntstatus_equal(tctx, r.out.result, NT_STATUS_OBJECT_NAME_NOT_FOUND,
			"Failed to enum rights for account");
	}

	return ret;
}

static bool test_user_ops(struct dcerpc_pipe *p,
			  struct torture_context *tctx,
			  struct policy_handle *user_handle,
			  struct policy_handle *domain_handle,
			  const struct dom_sid *domain_sid,
			  uint32_t base_acct_flags,
			  const char *base_acct_name, enum torture_samr_choice which_ops,
			  struct cli_credentials *machine_credentials)
{
	char *password = NULL;
	struct samr_QueryUserInfo q;
	union samr_UserInfo *info;
	NTSTATUS status;
	struct dcerpc_binding_handle *b = p->binding_handle;

	bool ret = true;
	int i;
	uint32_t rid;
	const uint32_t password_fields[] = {
		SAMR_FIELD_NT_PASSWORD_PRESENT,
		SAMR_FIELD_LM_PASSWORD_PRESENT,
		SAMR_FIELD_NT_PASSWORD_PRESENT | SAMR_FIELD_LM_PASSWORD_PRESENT,
		0
	};

	status = test_LookupName(b, tctx, domain_handle, base_acct_name, &rid);
	if (!NT_STATUS_IS_OK(status)) {
		ret = false;
	}

	switch (which_ops) {
	case TORTURE_SAMR_USER_ATTRIBUTES:
		if (!test_QuerySecurity(b, tctx, user_handle)) {
			ret = false;
		}

		if (!test_QueryUserInfo(b, tctx, user_handle)) {
			ret = false;
		}

		if (!test_QueryUserInfo2(b, tctx, user_handle)) {
			ret = false;
		}

		if (!test_SetUserInfo(b, tctx, user_handle, base_acct_flags,
				      base_acct_name)) {
			ret = false;
		}

		if (!test_GetUserPwInfo(b, tctx, user_handle)) {
			ret = false;
		}

		if (!test_TestPrivateFunctionsUser(b, tctx, user_handle)) {
			ret = false;
		}

		if (!test_SetUserPass(p, tctx, user_handle, &password)) {
			ret = false;
		}
		break;
	case TORTURE_SAMR_PASSWORDS:
		if (base_acct_flags & (ACB_WSTRUST|ACB_DOMTRUST|ACB_SVRTRUST)) {
			char simple_pass[9];
			char *v = generate_random_str(tctx, 1);

			ZERO_STRUCT(simple_pass);
			memset(simple_pass, *v, sizeof(simple_pass) - 1);

			torture_comment(tctx, "Testing machine account password policy rules\n");

			/* Workstation trust accounts don't seem to need to honour password quality policy */
			if (!test_SetUserPassEx(p, tctx, user_handle, true, &password)) {
				ret = false;
			}

			if (!test_ChangePasswordUser2(p, tctx, base_acct_name, &password, simple_pass, false)) {
				ret = false;
			}

			/* reset again, to allow another 'user' password change */
			if (!test_SetUserPassEx(p, tctx, user_handle, true, &password)) {
				ret = false;
			}

			/* Try a 'short' password */
			if (!test_ChangePasswordUser2(p, tctx, base_acct_name, &password, samr_rand_pass(tctx, 4), false)) {
				ret = false;
			}

			/* Try a compleatly random password */
			if (!test_ChangePasswordRandomBytes(p, tctx, base_acct_name, user_handle, &password)) {
				ret = false;
			}
		}

		for (i = 0; password_fields[i]; i++) {
			if (!test_SetUserPass_23(p, tctx, user_handle, password_fields[i], &password)) {
				ret = false;
			}

			/* check it was set right */
			if (!test_ChangePasswordUser3(p, tctx, base_acct_name, 0, &password, NULL, 0, false)) {
				ret = false;
			}
		}

		for (i = 0; password_fields[i]; i++) {
			if (!test_SetUserPass_25(p, tctx, user_handle, password_fields[i], &password)) {
				ret = false;
			}

			/* check it was set right */
			if (!test_ChangePasswordUser3(p, tctx, base_acct_name, 0, &password, NULL, 0, false)) {
				ret = false;
			}
		}

		if (!test_SetUserPassEx(p, tctx, user_handle, false, &password)) {
			ret = false;
		}

		if (!test_ChangePassword(p, tctx, base_acct_name, domain_handle, &password)) {
			ret = false;
		}

		if (!test_SetUserPass_18(p, tctx, user_handle, &password)) {
			ret = false;
		}

		if (!test_ChangePasswordUser3(p, tctx, base_acct_name, 0, &password, NULL, 0, false)) {
			ret = false;
		}

		for (i = 0; password_fields[i]; i++) {

			if (password_fields[i] == SAMR_FIELD_LM_PASSWORD_PRESENT) {
				/* we need to skip as that would break
				 * the ChangePasswordUser3 verify */
				continue;
			}

			if (!test_SetUserPass_21(p, tctx, user_handle, password_fields[i], &password)) {
				ret = false;
			}

			/* check it was set right */
			if (!test_ChangePasswordUser3(p, tctx, base_acct_name, 0, &password, NULL, 0, false)) {
				ret = false;
			}
		}

		q.in.user_handle = user_handle;
		q.in.level = 5;
		q.out.info = &info;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryUserInfo_r(b, tctx, &q),
			"QueryUserInfo failed");
		if (!NT_STATUS_IS_OK(q.out.result)) {
			torture_warning(tctx, "QueryUserInfo level %u failed - %s\n",
			       q.in.level, nt_errstr(q.out.result));
			ret = false;
		} else {
			uint32_t expected_flags = (base_acct_flags | ACB_PWNOTREQ | ACB_DISABLED);
			if ((info->info5.acct_flags) != expected_flags) {
				torture_warning(tctx, "QueryUserInfo level 5 failed, it returned 0x%08x when we expected flags of 0x%08x\n",
				       info->info5.acct_flags,
				       expected_flags);
				/* FIXME: GD */
				if (!torture_setting_bool(tctx, "samba3", false)) {
					ret = false;
				}
			}
			if (info->info5.rid != rid) {
				torture_warning(tctx, "QueryUserInfo level 5 failed, it returned %u when we expected rid of %u\n",
				       info->info5.rid, rid);

			}
		}

		break;

	case TORTURE_SAMR_PASSWORDS_PWDLASTSET:

		/* test last password change timestamp behaviour */
		if (!test_SetPassword_pwdlastset(p, tctx, base_acct_flags,
						 base_acct_name,
						 user_handle, &password,
						 machine_credentials)) {
			ret = false;
		}

		if (ret == true) {
			torture_comment(tctx, "pwdLastSet test succeeded\n");
		} else {
			torture_warning(tctx, "pwdLastSet test failed\n");
		}

		break;

	case TORTURE_SAMR_PASSWORDS_BADPWDCOUNT:

		/* test bad pwd count change behaviour */
		if (!test_Password_badpwdcount_wrap(p, tctx, base_acct_flags,
						    base_acct_name,
						    domain_handle,
						    user_handle, &password,
						    machine_credentials)) {
			ret = false;
		}

		if (ret == true) {
			torture_comment(tctx, "badPwdCount test succeeded\n");
		} else {
			torture_warning(tctx, "badPwdCount test failed\n");
		}

		break;

	case TORTURE_SAMR_PASSWORDS_LOCKOUT:

		if (!test_Password_lockout_wrap(p, tctx, base_acct_flags,
						base_acct_name,
						domain_handle,
						user_handle, &password,
						machine_credentials))
		{
			ret = false;
		}

		if (ret == true) {
			torture_comment(tctx, "lockout test succeeded\n");
		} else {
			torture_warning(tctx, "lockout test failed\n");
		}

		break;


	case TORTURE_SAMR_USER_PRIVILEGES: {

		struct dcerpc_pipe *lp;
		struct policy_handle *lsa_handle;
		struct dcerpc_binding_handle *lb;

		status = torture_rpc_connection(tctx, &lp, &ndr_table_lsarpc);
		torture_assert_ntstatus_ok(tctx, status, "Failed to open LSA pipe");
		lb = lp->binding_handle;

		if (!test_lsa_OpenPolicy2(lb, tctx, &lsa_handle)) {
			ret = false;
		}

		if (!test_DeleteUser_with_privs(p, lp, tctx,
						domain_handle, lsa_handle, user_handle,
						domain_sid, rid,
						machine_credentials)) {
			ret = false;
		}

		if (!test_lsa_Close(lb, tctx, lsa_handle)) {
			ret = false;
		}

		if (!ret) {
			torture_warning(tctx, "privileged user delete test failed\n");
		}

		break;
	}
	case TORTURE_SAMR_OTHER:
	case TORTURE_SAMR_MANY_ACCOUNTS:
	case TORTURE_SAMR_MANY_GROUPS:
	case TORTURE_SAMR_MANY_ALIASES:
		/* We just need the account to exist */
		break;
	}
	return ret;
}

static bool test_alias_ops(struct dcerpc_binding_handle *b,
			   struct torture_context *tctx,
			   struct policy_handle *alias_handle,
			   const struct dom_sid *domain_sid)
{
	bool ret = true;

	if (!torture_setting_bool(tctx, "samba3", false)) {
		if (!test_QuerySecurity(b, tctx, alias_handle)) {
			ret = false;
		}
	}

	if (!test_QueryAliasInfo(b, tctx, alias_handle)) {
		ret = false;
	}

	if (!test_SetAliasInfo(b, tctx, alias_handle)) {
		ret = false;
	}

	if (!test_AddMemberToAlias(b, tctx, alias_handle, domain_sid)) {
		ret = false;
	}

	if (torture_setting_bool(tctx, "samba3", false) ||
	    torture_setting_bool(tctx, "samba4", false)) {
		torture_comment(tctx, "skipping MultipleMembers Alias tests against Samba\n");
		return ret;
	}

	if (!test_AddMultipleMembersToAlias(b, tctx, alias_handle)) {
		ret = false;
	}

	return ret;
}


static bool test_DeleteUser(struct dcerpc_binding_handle *b,
			    struct torture_context *tctx,
			    struct policy_handle *user_handle)
{
    	struct samr_DeleteUser d;
	torture_comment(tctx, "Testing DeleteUser\n");

	d.in.user_handle = user_handle;
	d.out.user_handle = user_handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_DeleteUser_r(b, tctx, &d),
		"DeleteUser failed");
	torture_assert_ntstatus_ok(tctx, d.out.result, "DeleteUser");

	return true;
}

bool test_DeleteUser_byname(struct dcerpc_binding_handle *b,
			    struct torture_context *tctx,
			    struct policy_handle *handle, const char *name)
{
	NTSTATUS status;
	struct samr_DeleteUser d;
	struct policy_handle user_handle;
	uint32_t rid;

	status = test_LookupName(b, tctx, handle, name, &rid);
	if (!NT_STATUS_IS_OK(status)) {
		goto failed;
	}

	status = test_OpenUser_byname(b, tctx, handle, name, &user_handle);
	if (!NT_STATUS_IS_OK(status)) {
		goto failed;
	}

	d.in.user_handle = &user_handle;
	d.out.user_handle = &user_handle;
	torture_assert_ntstatus_ok(tctx, dcerpc_samr_DeleteUser_r(b, tctx, &d),
		"DeleteUser failed");
	if (!NT_STATUS_IS_OK(d.out.result)) {
		status = d.out.result;
		goto failed;
	}

	return true;

failed:
	torture_warning(tctx, "DeleteUser_byname(%s) failed - %s\n", name, nt_errstr(status));
	return false;
}


static bool test_DeleteGroup_byname(struct dcerpc_binding_handle *b,
				    struct torture_context *tctx,
				    struct policy_handle *handle, const char *name)
{
	NTSTATUS status;
	struct samr_OpenGroup r;
	struct samr_DeleteDomainGroup d;
	struct policy_handle group_handle;
	uint32_t rid;

	status = test_LookupName(b, tctx, handle, name, &rid);
	if (!NT_STATUS_IS_OK(status)) {
		goto failed;
	}

	r.in.domain_handle = handle;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.in.rid = rid;
	r.out.group_handle = &group_handle;
	torture_assert_ntstatus_ok(tctx, dcerpc_samr_OpenGroup_r(b, tctx, &r),
		"OpenGroup failed");
	if (!NT_STATUS_IS_OK(r.out.result)) {
		status = r.out.result;
		goto failed;
	}

	d.in.group_handle = &group_handle;
	d.out.group_handle = &group_handle;
	torture_assert_ntstatus_ok(tctx, dcerpc_samr_DeleteDomainGroup_r(b, tctx, &d),
		"DeleteDomainGroup failed");
	if (!NT_STATUS_IS_OK(d.out.result)) {
		status = d.out.result;
		goto failed;
	}

	return true;

failed:
	torture_warning(tctx, "DeleteGroup_byname(%s) failed - %s\n", name, nt_errstr(status));
	return false;
}


static bool test_DeleteAlias_byname(struct dcerpc_binding_handle *b,
				    struct torture_context *tctx,
				    struct policy_handle *domain_handle,
				    const char *name)
{
	NTSTATUS status;
	struct samr_OpenAlias r;
	struct samr_DeleteDomAlias d;
	struct policy_handle alias_handle;
	uint32_t rid;

	torture_comment(tctx, "Testing DeleteAlias_byname\n");

	status = test_LookupName(b, tctx, domain_handle, name, &rid);
	if (!NT_STATUS_IS_OK(status)) {
		goto failed;
	}

	r.in.domain_handle = domain_handle;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.in.rid = rid;
	r.out.alias_handle = &alias_handle;
	torture_assert_ntstatus_ok(tctx, dcerpc_samr_OpenAlias_r(b, tctx, &r),
		"OpenAlias failed");
	if (!NT_STATUS_IS_OK(r.out.result)) {
		status = r.out.result;
		goto failed;
	}

	d.in.alias_handle = &alias_handle;
	d.out.alias_handle = &alias_handle;
	torture_assert_ntstatus_ok(tctx, dcerpc_samr_DeleteDomAlias_r(b, tctx, &d),
		"DeleteDomAlias failed");
	if (!NT_STATUS_IS_OK(d.out.result)) {
		status = d.out.result;
		goto failed;
	}

	return true;

failed:
	torture_warning(tctx, "DeleteAlias_byname(%s) failed - %s\n", name, nt_errstr(status));
	return false;
}

static bool test_DeleteAlias(struct dcerpc_binding_handle *b,
			     struct torture_context *tctx,
			     struct policy_handle *alias_handle)
{
    	struct samr_DeleteDomAlias d;
	bool ret = true;

	torture_comment(tctx, "Testing DeleteAlias\n");

	d.in.alias_handle = alias_handle;
	d.out.alias_handle = alias_handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_DeleteDomAlias_r(b, tctx, &d),
		"DeleteDomAlias failed");
	if (!NT_STATUS_IS_OK(d.out.result)) {
		torture_warning(tctx, "DeleteAlias failed - %s\n", nt_errstr(d.out.result));
		ret = false;
	}

	return ret;
}

static bool test_CreateAlias(struct dcerpc_binding_handle *b,
			     struct torture_context *tctx,
			     struct policy_handle *domain_handle,
			     const char *alias_name,
			     struct policy_handle *alias_handle,
			     const struct dom_sid *domain_sid,
			     bool test_alias)
{
	struct samr_CreateDomAlias r;
	struct lsa_String name;
	uint32_t rid;
	bool ret = true;

	init_lsa_String(&name, alias_name);
	r.in.domain_handle = domain_handle;
	r.in.alias_name = &name;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.alias_handle = alias_handle;
	r.out.rid = &rid;

	torture_comment(tctx, "Testing CreateAlias (%s)\n", r.in.alias_name->string);

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_CreateDomAlias_r(b, tctx, &r),
		"CreateDomAlias failed");

	if (dom_sid_equal(domain_sid, dom_sid_parse_talloc(tctx, SID_BUILTIN))) {
		if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_ACCESS_DENIED)) {
			torture_comment(tctx, "Server correctly refused create of '%s'\n", r.in.alias_name->string);
			return true;
		} else {
			torture_warning(tctx, "Server should have refused create of '%s', got %s instead\n", r.in.alias_name->string,
			       nt_errstr(r.out.result));
			return false;
		}
	}

	if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_ALIAS_EXISTS)) {
		if (!test_DeleteAlias_byname(b, tctx, domain_handle, r.in.alias_name->string)) {
			return false;
		}
		torture_assert_ntstatus_ok(tctx, dcerpc_samr_CreateDomAlias_r(b, tctx, &r),
			"CreateDomAlias failed");
	}

	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_warning(tctx, "CreateAlias failed - %s\n", nt_errstr(r.out.result));
		return false;
	}

	if (!test_alias) {
		return ret;
	}

	if (!test_alias_ops(b, tctx, alias_handle, domain_sid)) {
		ret = false;
	}

	return ret;
}

static bool test_ChangePassword(struct dcerpc_pipe *p,
				struct torture_context *tctx,
				const char *acct_name,
				struct policy_handle *domain_handle, char **password)
{
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!*password) {
		return false;
	}

	if (!test_ChangePasswordUser(b, tctx, acct_name, domain_handle, password)) {
		ret = false;
	}

	if (!test_ChangePasswordUser2(p, tctx, acct_name, password, 0, true)) {
		ret = false;
	}

	if (!test_OemChangePasswordUser2(p, tctx, acct_name, domain_handle, password)) {
		ret = false;
	}

	/* test what happens when setting the old password again */
	if (!test_ChangePasswordUser3(p, tctx, acct_name, 0, password, *password, 0, true)) {
		ret = false;
	}

	{
		char simple_pass[9];
		char *v = generate_random_str(tctx, 1);

		ZERO_STRUCT(simple_pass);
		memset(simple_pass, *v, sizeof(simple_pass) - 1);

		/* test what happens when picking a simple password */
		if (!test_ChangePasswordUser3(p, tctx, acct_name, 0, password, simple_pass, 0, true)) {
			ret = false;
		}
	}

	/* set samr_SetDomainInfo level 1 with min_length 5 */
	{
		struct samr_QueryDomainInfo r;
		union samr_DomainInfo *info = NULL;
		struct samr_SetDomainInfo s;
		uint16_t len_old, len;
		uint32_t pwd_prop_old;
		int64_t min_pwd_age_old;

		len = 5;

		r.in.domain_handle = domain_handle;
		r.in.level = 1;
		r.out.info = &info;

		torture_comment(tctx, "Testing samr_QueryDomainInfo level 1\n");
		torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryDomainInfo_r(b, tctx, &r),
			"QueryDomainInfo failed");
		if (!NT_STATUS_IS_OK(r.out.result)) {
			return false;
		}

		s.in.domain_handle = domain_handle;
		s.in.level = 1;
		s.in.info = info;

		/* remember the old min length, so we can reset it */
		len_old = s.in.info->info1.min_password_length;
		s.in.info->info1.min_password_length = len;
		pwd_prop_old = s.in.info->info1.password_properties;
		/* turn off password complexity checks for this test */
		s.in.info->info1.password_properties &= ~DOMAIN_PASSWORD_COMPLEX;

		min_pwd_age_old = s.in.info->info1.min_password_age;
		s.in.info->info1.min_password_age = 0;

		torture_comment(tctx, "Testing samr_SetDomainInfo level 1\n");
		torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetDomainInfo_r(b, tctx, &s),
			"SetDomainInfo failed");
		if (!NT_STATUS_IS_OK(s.out.result)) {
			return false;
		}

		torture_comment(tctx, "calling test_ChangePasswordUser3 with too short password\n");

		if (!test_ChangePasswordUser3(p, tctx, acct_name, len - 1, password, NULL, 0, true)) {
			ret = false;
		}

		s.in.info->info1.min_password_length = len_old;
		s.in.info->info1.password_properties = pwd_prop_old;
		s.in.info->info1.min_password_age = min_pwd_age_old;

		torture_comment(tctx, "Testing samr_SetDomainInfo level 1\n");
		torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetDomainInfo_r(b, tctx, &s),
			"SetDomainInfo failed");
		if (!NT_STATUS_IS_OK(s.out.result)) {
			return false;
		}

	}

	{
		struct samr_OpenUser r;
		struct samr_QueryUserInfo q;
		union samr_UserInfo *info;
		struct samr_LookupNames n;
		struct policy_handle user_handle;
		struct samr_Ids rids, types;

		n.in.domain_handle = domain_handle;
		n.in.num_names = 1;
		n.in.names = talloc_array(tctx, struct lsa_String, 1);
		n.in.names[0].string = acct_name;
		n.out.rids = &rids;
		n.out.types = &types;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_LookupNames_r(b, tctx, &n),
			"LookupNames failed");
		if (!NT_STATUS_IS_OK(n.out.result)) {
			torture_warning(tctx, "LookupNames failed - %s\n", nt_errstr(n.out.result));
			return false;
		}

		r.in.domain_handle = domain_handle;
		r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
		r.in.rid = n.out.rids->ids[0];
		r.out.user_handle = &user_handle;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_OpenUser_r(b, tctx, &r),
			"OpenUser failed");
		if (!NT_STATUS_IS_OK(r.out.result)) {
			torture_warning(tctx, "OpenUser(%u) failed - %s\n", n.out.rids->ids[0], nt_errstr(r.out.result));
			return false;
		}

		q.in.user_handle = &user_handle;
		q.in.level = 5;
		q.out.info = &info;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryUserInfo_r(b, tctx, &q),
			"QueryUserInfo failed");
		if (!NT_STATUS_IS_OK(q.out.result)) {
			torture_warning(tctx, "QueryUserInfo failed - %s\n", nt_errstr(q.out.result));
			return false;
		}

		torture_comment(tctx, "calling test_ChangePasswordUser3 with too early password change\n");

		if (!test_ChangePasswordUser3(p, tctx, acct_name, 0, password, NULL,
					      info->info5.last_password_change, true)) {
			ret = false;
		}
	}

	/* we change passwords twice - this has the effect of verifying
	   they were changed correctly for the final call */
	if (!test_ChangePasswordUser3(p, tctx, acct_name, 0, password, NULL, 0, true)) {
		ret = false;
	}

	if (!test_ChangePasswordUser3(p, tctx, acct_name, 0, password, NULL, 0, true)) {
		ret = false;
	}

	return ret;
}

static bool test_CreateUser(struct dcerpc_pipe *p, struct torture_context *tctx,
			    struct policy_handle *domain_handle,
			    const char *user_name,
			    struct policy_handle *user_handle_out,
			    struct dom_sid *domain_sid,
			    enum torture_samr_choice which_ops,
			    struct cli_credentials *machine_credentials,
			    bool test_user)
{

	TALLOC_CTX *user_ctx;

	struct samr_CreateUser r;
	struct samr_QueryUserInfo q;
	union samr_UserInfo *info;
	struct samr_DeleteUser d;
	uint32_t rid;

	/* This call creates a 'normal' account - check that it really does */
	const uint32_t acct_flags = ACB_NORMAL;
	struct lsa_String name;
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;

	struct policy_handle user_handle;
	user_ctx = talloc_named(tctx, 0, "test_CreateUser2 per-user context");
	init_lsa_String(&name, user_name);

	r.in.domain_handle = domain_handle;
	r.in.account_name = &name;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.user_handle = &user_handle;
	r.out.rid = &rid;

	torture_comment(tctx, "Testing CreateUser(%s)\n", r.in.account_name->string);

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_CreateUser_r(b, user_ctx, &r),
		"CreateUser failed");

	if (dom_sid_equal(domain_sid, dom_sid_parse_talloc(tctx, SID_BUILTIN))) {
		if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_ACCESS_DENIED) || NT_STATUS_EQUAL(r.out.result, NT_STATUS_INVALID_PARAMETER)) {
			torture_comment(tctx, "Server correctly refused create of '%s'\n", r.in.account_name->string);
			return true;
		} else {
			torture_warning(tctx, "Server should have refused create of '%s', got %s instead\n", r.in.account_name->string,
			       nt_errstr(r.out.result));
			return false;
		}
	}

	if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_USER_EXISTS)) {
		if (!test_DeleteUser_byname(b, tctx, domain_handle, r.in.account_name->string)) {
			talloc_free(user_ctx);
			return false;
		}
		torture_assert_ntstatus_ok(tctx, dcerpc_samr_CreateUser_r(b, user_ctx, &r),
			"CreateUser failed");
	}

	if (!NT_STATUS_IS_OK(r.out.result)) {
		talloc_free(user_ctx);
		torture_warning(tctx, "CreateUser failed - %s\n", nt_errstr(r.out.result));
		return false;
	}

	if (!test_user) {
		if (user_handle_out) {
			*user_handle_out = user_handle;
		}
		return ret;
	}

	{
		q.in.user_handle = &user_handle;
		q.in.level = 16;
		q.out.info = &info;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryUserInfo_r(b, user_ctx, &q),
			"QueryUserInfo failed");
		if (!NT_STATUS_IS_OK(q.out.result)) {
			torture_warning(tctx, "QueryUserInfo level %u failed - %s\n",
			       q.in.level, nt_errstr(q.out.result));
			ret = false;
		} else {
			if ((info->info16.acct_flags & acct_flags) != acct_flags) {
				torture_warning(tctx, "QueryUserInfo level 16 failed, it returned 0x%08x when we expected flags of 0x%08x\n",
				       info->info16.acct_flags,
				       acct_flags);
				ret = false;
			}
		}

		if (!test_user_ops(p, tctx, &user_handle, domain_handle,
				   domain_sid, acct_flags, name.string, which_ops,
				   machine_credentials)) {
			ret = false;
		}

		if (user_handle_out) {
			*user_handle_out = user_handle;
		} else {
			torture_comment(tctx, "Testing DeleteUser (createuser test)\n");

			d.in.user_handle = &user_handle;
			d.out.user_handle = &user_handle;

			torture_assert_ntstatus_ok(tctx, dcerpc_samr_DeleteUser_r(b, user_ctx, &d),
				"DeleteUser failed");
			if (!NT_STATUS_IS_OK(d.out.result)) {
				torture_warning(tctx, "DeleteUser failed - %s\n", nt_errstr(d.out.result));
				ret = false;
			}
		}

	}

	talloc_free(user_ctx);

	return ret;
}


static bool test_CreateUser2(struct dcerpc_pipe *p, struct torture_context *tctx,
			     struct policy_handle *domain_handle,
			     struct dom_sid *domain_sid,
			     enum torture_samr_choice which_ops,
			     struct cli_credentials *machine_credentials)
{
	struct samr_CreateUser2 r;
	struct samr_QueryUserInfo q;
	union samr_UserInfo *info;
	struct samr_DeleteUser d;
	struct policy_handle user_handle;
	uint32_t rid;
	struct lsa_String name;
	bool ret = true;
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	struct {
		uint32_t acct_flags;
		const char *account_name;
		NTSTATUS nt_status;
	} account_types[] = {
		{ ACB_NORMAL, TEST_ACCOUNT_NAME, NT_STATUS_OK },
		{ ACB_NORMAL | ACB_DISABLED, TEST_ACCOUNT_NAME, NT_STATUS_INVALID_PARAMETER },
		{ ACB_NORMAL | ACB_PWNOEXP, TEST_ACCOUNT_NAME, NT_STATUS_INVALID_PARAMETER },
		{ ACB_WSTRUST, TEST_MACHINENAME, NT_STATUS_OK },
		{ ACB_WSTRUST | ACB_DISABLED, TEST_MACHINENAME, NT_STATUS_INVALID_PARAMETER },
		{ ACB_WSTRUST | ACB_PWNOEXP, TEST_MACHINENAME, NT_STATUS_INVALID_PARAMETER },
		{ ACB_SVRTRUST, TEST_MACHINENAME, NT_STATUS_OK },
		{ ACB_SVRTRUST | ACB_DISABLED, TEST_MACHINENAME, NT_STATUS_INVALID_PARAMETER },
		{ ACB_SVRTRUST | ACB_PWNOEXP, TEST_MACHINENAME, NT_STATUS_INVALID_PARAMETER },
		{ ACB_DOMTRUST, TEST_DOMAINNAME, NT_STATUS_ACCESS_DENIED },
		{ ACB_DOMTRUST | ACB_DISABLED, TEST_DOMAINNAME, NT_STATUS_INVALID_PARAMETER },
		{ ACB_DOMTRUST | ACB_PWNOEXP, TEST_DOMAINNAME, NT_STATUS_INVALID_PARAMETER },
		{ 0, TEST_ACCOUNT_NAME, NT_STATUS_INVALID_PARAMETER },
		{ ACB_DISABLED, TEST_ACCOUNT_NAME, NT_STATUS_INVALID_PARAMETER },
		{ 0, NULL, NT_STATUS_INVALID_PARAMETER }
	};

	for (i = 0; account_types[i].account_name; i++) {
		TALLOC_CTX *user_ctx;
		uint32_t acct_flags = account_types[i].acct_flags;
		uint32_t access_granted;
		user_ctx = talloc_named(tctx, 0, "test_CreateUser2 per-user context");
		init_lsa_String(&name, account_types[i].account_name);

		r.in.domain_handle = domain_handle;
		r.in.account_name = &name;
		r.in.acct_flags = acct_flags;
		r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
		r.out.user_handle = &user_handle;
		r.out.access_granted = &access_granted;
		r.out.rid = &rid;

		torture_comment(tctx, "Testing CreateUser2(%s, 0x%x)\n", r.in.account_name->string, acct_flags);

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_CreateUser2_r(b, user_ctx, &r),
			"CreateUser2 failed");

		if (dom_sid_equal(domain_sid, dom_sid_parse_talloc(tctx, SID_BUILTIN))) {
			if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_ACCESS_DENIED) || NT_STATUS_EQUAL(r.out.result, NT_STATUS_INVALID_PARAMETER)) {
				torture_comment(tctx, "Server correctly refused create of '%s'\n", r.in.account_name->string);
				continue;
			} else {
				torture_warning(tctx, "Server should have refused create of '%s', got %s instead\n", r.in.account_name->string,
				       nt_errstr(r.out.result));
				ret = false;
				continue;
			}
		}

		if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_USER_EXISTS)) {
			if (!test_DeleteUser_byname(b, tctx, domain_handle, r.in.account_name->string)) {
				talloc_free(user_ctx);
				ret = false;
				continue;
			}
			torture_assert_ntstatus_ok(tctx, dcerpc_samr_CreateUser2_r(b, user_ctx, &r),
				"CreateUser2 failed");

		}
		if (!NT_STATUS_EQUAL(r.out.result, account_types[i].nt_status)) {
			torture_warning(tctx, "CreateUser2 failed gave incorrect error return - %s (should be %s)\n",
			       nt_errstr(r.out.result), nt_errstr(account_types[i].nt_status));
			ret = false;
		}

		if (NT_STATUS_IS_OK(r.out.result)) {
			q.in.user_handle = &user_handle;
			q.in.level = 5;
			q.out.info = &info;

			torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryUserInfo_r(b, user_ctx, &q),
				"QueryUserInfo failed");
			if (!NT_STATUS_IS_OK(q.out.result)) {
				torture_warning(tctx, "QueryUserInfo level %u failed - %s\n",
				       q.in.level, nt_errstr(q.out.result));
				ret = false;
			} else {
				uint32_t expected_flags = (acct_flags | ACB_PWNOTREQ | ACB_DISABLED);
				if (acct_flags == ACB_NORMAL) {
					expected_flags |= ACB_PW_EXPIRED;
				}
				if ((info->info5.acct_flags) != expected_flags) {
					torture_warning(tctx, "QueryUserInfo level 5 failed, it returned 0x%08x when we expected flags of 0x%08x\n",
					       info->info5.acct_flags,
					       expected_flags);
					ret = false;
				}
				switch (acct_flags) {
				case ACB_SVRTRUST:
					if (info->info5.primary_gid != DOMAIN_RID_DCS) {
						torture_warning(tctx, "QueryUserInfo level 5: DC should have had Primary Group %d, got %d\n",
						       DOMAIN_RID_DCS, info->info5.primary_gid);
						ret = false;
					}
					break;
				case ACB_WSTRUST:
					if (info->info5.primary_gid != DOMAIN_RID_DOMAIN_MEMBERS) {
						torture_warning(tctx, "QueryUserInfo level 5: Domain Member should have had Primary Group %d, got %d\n",
						       DOMAIN_RID_DOMAIN_MEMBERS, info->info5.primary_gid);
						ret = false;
					}
					break;
				case ACB_NORMAL:
					if (info->info5.primary_gid != DOMAIN_RID_USERS) {
						torture_warning(tctx, "QueryUserInfo level 5: Users should have had Primary Group %d, got %d\n",
						       DOMAIN_RID_USERS, info->info5.primary_gid);
						ret = false;
					}
					break;
				}
			}

			if (!test_user_ops(p, tctx, &user_handle, domain_handle,
					   domain_sid, acct_flags, name.string, which_ops,
					   machine_credentials)) {
				ret = false;
			}

			if (!policy_handle_empty(&user_handle)) {
				torture_comment(tctx, "Testing DeleteUser (createuser2 test)\n");

				d.in.user_handle = &user_handle;
				d.out.user_handle = &user_handle;

				torture_assert_ntstatus_ok(tctx, dcerpc_samr_DeleteUser_r(b, user_ctx, &d),
					"DeleteUser failed");
				if (!NT_STATUS_IS_OK(d.out.result)) {
					torture_warning(tctx, "DeleteUser failed - %s\n", nt_errstr(d.out.result));
					ret = false;
				}
			}
		}
		talloc_free(user_ctx);
	}

	return ret;
}

static bool test_QueryAliasInfo(struct dcerpc_binding_handle *b,
				struct torture_context *tctx,
				struct policy_handle *handle)
{
	struct samr_QueryAliasInfo r;
	union samr_AliasInfo *info;
	uint16_t levels[] = {1, 2, 3};
	int i;
	bool ret = true;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		torture_comment(tctx, "Testing QueryAliasInfo level %u\n", levels[i]);

		r.in.alias_handle = handle;
		r.in.level = levels[i];
		r.out.info = &info;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryAliasInfo_r(b, tctx, &r),
			"QueryAliasInfo failed");
		if (!NT_STATUS_IS_OK(r.out.result)) {
			torture_warning(tctx, "QueryAliasInfo level %u failed - %s\n",
			       levels[i], nt_errstr(r.out.result));
			ret = false;
		}
	}

	return ret;
}

static bool test_QueryGroupInfo(struct dcerpc_binding_handle *b,
				struct torture_context *tctx,
				struct policy_handle *handle)
{
	struct samr_QueryGroupInfo r;
	union samr_GroupInfo *info;
	uint16_t levels[] = {1, 2, 3, 4, 5};
	int i;
	bool ret = true;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		torture_comment(tctx, "Testing QueryGroupInfo level %u\n", levels[i]);

		r.in.group_handle = handle;
		r.in.level = levels[i];
		r.out.info = &info;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryGroupInfo_r(b, tctx, &r),
			"QueryGroupInfo failed");
		if (!NT_STATUS_IS_OK(r.out.result)) {
			torture_warning(tctx, "QueryGroupInfo level %u failed - %s\n",
			       levels[i], nt_errstr(r.out.result));
			ret = false;
		}
	}

	return ret;
}

static bool test_QueryGroupMember(struct dcerpc_binding_handle *b,
				  struct torture_context *tctx,
				  struct policy_handle *handle)
{
	struct samr_QueryGroupMember r;
	struct samr_RidAttrArray *rids = NULL;
	bool ret = true;

	torture_comment(tctx, "Testing QueryGroupMember\n");

	r.in.group_handle = handle;
	r.out.rids = &rids;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryGroupMember_r(b, tctx, &r),
		"QueryGroupMember failed");
	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_warning(tctx, "QueryGroupMember failed - %s\n", nt_errstr(r.out.result));
		ret = false;
	}

	return ret;
}


static bool test_SetGroupInfo(struct dcerpc_binding_handle *b,
			      struct torture_context *tctx,
			      struct policy_handle *handle)
{
	struct samr_QueryGroupInfo r;
	union samr_GroupInfo *info;
	struct samr_SetGroupInfo s;
	uint16_t levels[] = {1, 2, 3, 4};
	uint16_t set_ok[] = {0, 1, 1, 1};
	int i;
	bool ret = true;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		torture_comment(tctx, "Testing QueryGroupInfo level %u\n", levels[i]);

		r.in.group_handle = handle;
		r.in.level = levels[i];
		r.out.info = &info;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryGroupInfo_r(b, tctx, &r),
			"QueryGroupInfo failed");
		if (!NT_STATUS_IS_OK(r.out.result)) {
			torture_warning(tctx, "QueryGroupInfo level %u failed - %s\n",
			       levels[i], nt_errstr(r.out.result));
			ret = false;
		}

		torture_comment(tctx, "Testing SetGroupInfo level %u\n", levels[i]);

		s.in.group_handle = handle;
		s.in.level = levels[i];
		s.in.info = *r.out.info;

#if 0
		/* disabled this, as it changes the name only from the point of view of samr,
		   but leaves the name from the point of view of w2k3 internals (and ldap). This means
		   the name is still reserved, so creating the old name fails, but deleting by the old name
		   also fails */
		if (s.in.level == 2) {
			init_lsa_String(&s.in.info->string, "NewName");
		}
#endif

		if (s.in.level == 4) {
			init_lsa_String(&s.in.info->description, "test description");
		}

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetGroupInfo_r(b, tctx, &s),
			"SetGroupInfo failed");
		if (set_ok[i]) {
			if (!NT_STATUS_IS_OK(s.out.result)) {
				torture_warning(tctx, "SetGroupInfo level %u failed - %s\n",
				       r.in.level, nt_errstr(s.out.result));
				ret = false;
				continue;
			}
		} else {
			if (!NT_STATUS_EQUAL(NT_STATUS_INVALID_INFO_CLASS, s.out.result)) {
				torture_warning(tctx, "SetGroupInfo level %u gave %s - should have been NT_STATUS_INVALID_INFO_CLASS\n",
				       r.in.level, nt_errstr(s.out.result));
				ret = false;
				continue;
			}
		}
	}

	return ret;
}

static bool test_QueryUserInfo(struct dcerpc_binding_handle *b,
			       struct torture_context *tctx,
			       struct policy_handle *handle)
{
	struct samr_QueryUserInfo r;
	union samr_UserInfo *info;
	uint16_t levels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
			   11, 12, 13, 14, 16, 17, 20, 21};
	int i;
	bool ret = true;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		torture_comment(tctx, "Testing QueryUserInfo level %u\n", levels[i]);

		r.in.user_handle = handle;
		r.in.level = levels[i];
		r.out.info = &info;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryUserInfo_r(b, tctx, &r),
			"QueryUserInfo failed");
		if (!NT_STATUS_IS_OK(r.out.result)) {
			torture_warning(tctx, "QueryUserInfo level %u failed - %s\n",
			       levels[i], nt_errstr(r.out.result));
			ret = false;
		}
	}

	return ret;
}

static bool test_QueryUserInfo2(struct dcerpc_binding_handle *b,
				struct torture_context *tctx,
				struct policy_handle *handle)
{
	struct samr_QueryUserInfo2 r;
	union samr_UserInfo *info;
	uint16_t levels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
			   11, 12, 13, 14, 16, 17, 20, 21};
	int i;
	bool ret = true;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		torture_comment(tctx, "Testing QueryUserInfo2 level %u\n", levels[i]);

		r.in.user_handle = handle;
		r.in.level = levels[i];
		r.out.info = &info;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryUserInfo2_r(b, tctx, &r),
			"QueryUserInfo2 failed");
		if (!NT_STATUS_IS_OK(r.out.result)) {
			torture_warning(tctx, "QueryUserInfo2 level %u failed - %s\n",
			       levels[i], nt_errstr(r.out.result));
			ret = false;
		}
	}

	return ret;
}

static bool test_OpenUser(struct dcerpc_binding_handle *b,
			  struct torture_context *tctx,
			  struct policy_handle *handle, uint32_t rid)
{
	struct samr_OpenUser r;
	struct policy_handle user_handle;
	bool ret = true;

	torture_comment(tctx, "Testing OpenUser(%u)\n", rid);

	r.in.domain_handle = handle;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.in.rid = rid;
	r.out.user_handle = &user_handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_OpenUser_r(b, tctx, &r),
		"OpenUser failed");
	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_warning(tctx, "OpenUser(%u) failed - %s\n", rid, nt_errstr(r.out.result));
		return false;
	}

	if (!test_QuerySecurity(b, tctx, &user_handle)) {
		ret = false;
	}

	if (!test_QueryUserInfo(b, tctx, &user_handle)) {
		ret = false;
	}

	if (!test_QueryUserInfo2(b, tctx, &user_handle)) {
		ret = false;
	}

	if (!test_GetUserPwInfo(b, tctx, &user_handle)) {
		ret = false;
	}

	if (!test_GetGroupsForUser(b, tctx, &user_handle)) {
		ret = false;
	}

	if (!test_samr_handle_Close(b, tctx, &user_handle)) {
		ret = false;
	}

	return ret;
}

static bool test_OpenGroup(struct dcerpc_binding_handle *b,
			   struct torture_context *tctx,
			   struct policy_handle *handle, uint32_t rid)
{
	struct samr_OpenGroup r;
	struct policy_handle group_handle;
	bool ret = true;

	torture_comment(tctx, "Testing OpenGroup(%u)\n", rid);

	r.in.domain_handle = handle;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.in.rid = rid;
	r.out.group_handle = &group_handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_OpenGroup_r(b, tctx, &r),
		"OpenGroup failed");
	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_warning(tctx, "OpenGroup(%u) failed - %s\n", rid, nt_errstr(r.out.result));
		return false;
	}

	if (!torture_setting_bool(tctx, "samba3", false)) {
		if (!test_QuerySecurity(b, tctx, &group_handle)) {
			ret = false;
		}
	}

	if (!test_QueryGroupInfo(b, tctx, &group_handle)) {
		ret = false;
	}

	if (!test_QueryGroupMember(b, tctx, &group_handle)) {
		ret = false;
	}

	if (!test_samr_handle_Close(b, tctx, &group_handle)) {
		ret = false;
	}

	return ret;
}

static bool test_OpenAlias(struct dcerpc_binding_handle *b,
			   struct torture_context *tctx,
			   struct policy_handle *handle, uint32_t rid)
{
	struct samr_OpenAlias r;
	struct policy_handle alias_handle;
	bool ret = true;

	torture_comment(tctx, "Testing OpenAlias(%u)\n", rid);

	r.in.domain_handle = handle;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.in.rid = rid;
	r.out.alias_handle = &alias_handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_OpenAlias_r(b, tctx, &r),
		"OpenAlias failed");
	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_warning(tctx, "OpenAlias(%u) failed - %s\n", rid, nt_errstr(r.out.result));
		return false;
	}

	if (!torture_setting_bool(tctx, "samba3", false)) {
		if (!test_QuerySecurity(b, tctx, &alias_handle)) {
			ret = false;
		}
	}

	if (!test_QueryAliasInfo(b, tctx, &alias_handle)) {
		ret = false;
	}

	if (!test_GetMembersInAlias(b, tctx, &alias_handle)) {
		ret = false;
	}

	if (!test_samr_handle_Close(b, tctx, &alias_handle)) {
		ret = false;
	}

	return ret;
}

static bool check_mask(struct dcerpc_binding_handle *b,
		       struct torture_context *tctx,
		       struct policy_handle *handle, uint32_t rid,
		       uint32_t acct_flag_mask)
{
	struct samr_OpenUser r;
	struct samr_QueryUserInfo q;
	union samr_UserInfo *info;
	struct policy_handle user_handle;
	bool ret = true;

	torture_comment(tctx, "Testing OpenUser(%u)\n", rid);

	r.in.domain_handle = handle;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.in.rid = rid;
	r.out.user_handle = &user_handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_OpenUser_r(b, tctx, &r),
		"OpenUser failed");
	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_warning(tctx, "OpenUser(%u) failed - %s\n", rid, nt_errstr(r.out.result));
		return false;
	}

	q.in.user_handle = &user_handle;
	q.in.level = 16;
	q.out.info = &info;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryUserInfo_r(b, tctx, &q),
		"QueryUserInfo failed");
	if (!NT_STATUS_IS_OK(q.out.result)) {
		torture_warning(tctx, "QueryUserInfo level 16 failed - %s\n",
		       nt_errstr(q.out.result));
		ret = false;
	} else {
		if ((acct_flag_mask & info->info16.acct_flags) == 0) {
			torture_warning(tctx, "Server failed to filter for 0x%x, allowed 0x%x (%d) on EnumDomainUsers\n",
			       acct_flag_mask, info->info16.acct_flags, rid);
			ret = false;
		}
	}

	if (!test_samr_handle_Close(b, tctx, &user_handle)) {
		ret = false;
	}

	return ret;
}

static bool test_EnumDomainUsers_all(struct dcerpc_binding_handle *b,
				     struct torture_context *tctx,
				     struct policy_handle *handle)
{
	struct samr_EnumDomainUsers r;
	uint32_t mask, resume_handle=0;
	int i, mask_idx;
	bool ret = true;
	struct samr_LookupNames n;
	struct samr_LookupRids  lr ;
	struct lsa_Strings names;
	struct samr_Ids rids, types;
	struct samr_SamArray *sam = NULL;
	uint32_t num_entries = 0;

	uint32_t masks[] = {ACB_NORMAL, ACB_DOMTRUST, ACB_WSTRUST,
			    ACB_DISABLED, ACB_NORMAL | ACB_DISABLED,
			    ACB_SVRTRUST | ACB_DOMTRUST | ACB_WSTRUST,
			    ACB_PWNOEXP, 0};

	torture_comment(tctx, "Testing EnumDomainUsers\n");

	for (mask_idx=0;mask_idx<ARRAY_SIZE(masks);mask_idx++) {
		r.in.domain_handle = handle;
		r.in.resume_handle = &resume_handle;
		r.in.acct_flags = mask = masks[mask_idx];
		r.in.max_size = (uint32_t)-1;
		r.out.resume_handle = &resume_handle;
		r.out.num_entries = &num_entries;
		r.out.sam = &sam;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_EnumDomainUsers_r(b, tctx, &r),
			"EnumDomainUsers failed");
		if (!NT_STATUS_EQUAL(r.out.result, STATUS_MORE_ENTRIES) &&
		    !NT_STATUS_IS_OK(r.out.result)) {
			torture_warning(tctx, "EnumDomainUsers failed - %s\n", nt_errstr(r.out.result));
			return false;
		}

		torture_assert(tctx, sam, "EnumDomainUsers failed: r.out.sam unexpectedly NULL");

		if (sam->count == 0) {
			continue;
		}

		for (i=0;i<sam->count;i++) {
			if (mask) {
				if (!check_mask(b, tctx, handle, sam->entries[i].idx, mask)) {
					ret = false;
				}
			} else if (!test_OpenUser(b, tctx, handle, sam->entries[i].idx)) {
				ret = false;
			}
		}
	}

	torture_comment(tctx, "Testing LookupNames\n");
	n.in.domain_handle = handle;
	n.in.num_names = sam->count;
	n.in.names = talloc_array(tctx, struct lsa_String, sam->count);
	n.out.rids = &rids;
	n.out.types = &types;
	for (i=0;i<sam->count;i++) {
		n.in.names[i].string = sam->entries[i].name.string;
	}
	torture_assert_ntstatus_ok(tctx, dcerpc_samr_LookupNames_r(b, tctx, &n),
		"LookupNames failed");
	if (!NT_STATUS_IS_OK(n.out.result)) {
		torture_warning(tctx, "LookupNames failed - %s\n", nt_errstr(n.out.result));
		ret = false;
	}


	torture_comment(tctx, "Testing LookupRids\n");
	lr.in.domain_handle = handle;
	lr.in.num_rids = sam->count;
	lr.in.rids = talloc_array(tctx, uint32_t, sam->count);
	lr.out.names = &names;
	lr.out.types = &types;
	for (i=0;i<sam->count;i++) {
		lr.in.rids[i] = sam->entries[i].idx;
	}
	torture_assert_ntstatus_ok(tctx, dcerpc_samr_LookupRids_r(b, tctx, &lr),
		"LookupRids failed");
	torture_assert_ntstatus_ok(tctx, lr.out.result, "LookupRids");

	return ret;
}

/*
  try blasting the server with a bunch of sync requests
*/
static bool test_EnumDomainUsers_async(struct dcerpc_pipe *p, struct torture_context *tctx,
				       struct policy_handle *handle)
{
	struct samr_EnumDomainUsers r;
	uint32_t resume_handle=0;
	int i;
#define ASYNC_COUNT 100
	struct tevent_req *req[ASYNC_COUNT];

	if (!torture_setting_bool(tctx, "dangerous", false)) {
		torture_skip(tctx, "samr async test disabled - enable dangerous tests to use\n");
	}

	torture_comment(tctx, "Testing EnumDomainUsers_async\n");

	r.in.domain_handle = handle;
	r.in.resume_handle = &resume_handle;
	r.in.acct_flags = 0;
	r.in.max_size = (uint32_t)-1;
	r.out.resume_handle = &resume_handle;

	for (i=0;i<ASYNC_COUNT;i++) {
		req[i] = dcerpc_samr_EnumDomainUsers_r_send(tctx, tctx->ev, p->binding_handle, &r);
	}

	for (i=0;i<ASYNC_COUNT;i++) {
		tevent_req_poll(req[i], tctx->ev);
		torture_assert_ntstatus_ok(tctx, dcerpc_samr_EnumDomainUsers_r_recv(req[i], tctx),
			talloc_asprintf(tctx, "EnumDomainUsers[%d] failed - %s\n",
			       i, nt_errstr(r.out.result)));
	}

	torture_comment(tctx, "%d async requests OK\n", i);

	return true;
}

static bool test_EnumDomainGroups_all(struct dcerpc_binding_handle *b,
				      struct torture_context *tctx,
				      struct policy_handle *handle)
{
	struct samr_EnumDomainGroups r;
	uint32_t resume_handle=0;
	struct samr_SamArray *sam = NULL;
	uint32_t num_entries = 0;
	int i;
	bool ret = true;
	bool universal_group_found = false;

	torture_comment(tctx, "Testing EnumDomainGroups\n");

	r.in.domain_handle = handle;
	r.in.resume_handle = &resume_handle;
	r.in.max_size = (uint32_t)-1;
	r.out.resume_handle = &resume_handle;
	r.out.num_entries = &num_entries;
	r.out.sam = &sam;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_EnumDomainGroups_r(b, tctx, &r),
		"EnumDomainGroups failed");
	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_warning(tctx, "EnumDomainGroups failed - %s\n", nt_errstr(r.out.result));
		return false;
	}

	if (!sam) {
		return false;
	}

	for (i=0;i<sam->count;i++) {
		if (!test_OpenGroup(b, tctx, handle, sam->entries[i].idx)) {
			ret = false;
		}
		if ((ret == true) && (strcasecmp(sam->entries[i].name.string,
						 "Enterprise Admins") == 0)) {
			universal_group_found = true;
		}
	}

	/* when we are running this on s4 we should get back at least the
	 * "Enterprise Admins" universal group. If we don't get a group entry
	 * at all we probably are performing the test on the builtin domain.
	 * So ignore this case. */
	if (torture_setting_bool(tctx, "samba4", false)) {
		if ((sam->count > 0) && (!universal_group_found)) {
			ret = false;
		}
	}

	return ret;
}

static bool test_EnumDomainAliases_all(struct dcerpc_binding_handle *b,
				       struct torture_context *tctx,
				       struct policy_handle *handle)
{
	struct samr_EnumDomainAliases r;
	uint32_t resume_handle=0;
	struct samr_SamArray *sam = NULL;
	uint32_t num_entries = 0;
	int i;
	bool ret = true;

	torture_comment(tctx, "Testing EnumDomainAliases\n");

	r.in.domain_handle = handle;
	r.in.resume_handle = &resume_handle;
	r.in.max_size = (uint32_t)-1;
	r.out.sam = &sam;
	r.out.num_entries = &num_entries;
	r.out.resume_handle = &resume_handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_EnumDomainAliases_r(b, tctx, &r),
		"EnumDomainAliases failed");
	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_warning(tctx, "EnumDomainAliases failed - %s\n", nt_errstr(r.out.result));
		return false;
	}

	if (!sam) {
		return false;
	}

	for (i=0;i<sam->count;i++) {
		if (!test_OpenAlias(b, tctx, handle, sam->entries[i].idx)) {
			ret = false;
		}
	}

	return ret;
}

static bool test_GetDisplayEnumerationIndex(struct dcerpc_binding_handle *b,
					    struct torture_context *tctx,
					    struct policy_handle *handle)
{
	struct samr_GetDisplayEnumerationIndex r;
	bool ret = true;
	uint16_t levels[] = {1, 2, 3, 4, 5};
	uint16_t ok_lvl[] = {1, 1, 1, 0, 0};
	struct lsa_String name;
	uint32_t idx = 0;
	int i;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		torture_comment(tctx, "Testing GetDisplayEnumerationIndex level %u\n", levels[i]);

		init_lsa_String(&name, TEST_ACCOUNT_NAME);

		r.in.domain_handle = handle;
		r.in.level = levels[i];
		r.in.name = &name;
		r.out.idx = &idx;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetDisplayEnumerationIndex_r(b, tctx, &r),
			"GetDisplayEnumerationIndex failed");

		if (ok_lvl[i] &&
		    !NT_STATUS_IS_OK(r.out.result) &&
		    !NT_STATUS_EQUAL(NT_STATUS_NO_MORE_ENTRIES, r.out.result)) {
			torture_warning(tctx, "GetDisplayEnumerationIndex level %u failed - %s\n",
			       levels[i], nt_errstr(r.out.result));
			ret = false;
		}

		init_lsa_String(&name, "zzzzzzzz");

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetDisplayEnumerationIndex_r(b, tctx, &r),
			"GetDisplayEnumerationIndex failed");

		if (ok_lvl[i] && !NT_STATUS_EQUAL(NT_STATUS_NO_MORE_ENTRIES, r.out.result)) {
			torture_warning(tctx, "GetDisplayEnumerationIndex level %u failed - %s\n",
			       levels[i], nt_errstr(r.out.result));
			ret = false;
		}
	}

	return ret;
}

static bool test_GetDisplayEnumerationIndex2(struct dcerpc_binding_handle *b,
					     struct torture_context *tctx,
					     struct policy_handle *handle)
{
	struct samr_GetDisplayEnumerationIndex2 r;
	bool ret = true;
	uint16_t levels[] = {1, 2, 3, 4, 5};
	uint16_t ok_lvl[] = {1, 1, 1, 0, 0};
	struct lsa_String name;
	uint32_t idx = 0;
	int i;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		torture_comment(tctx, "Testing GetDisplayEnumerationIndex2 level %u\n", levels[i]);

		init_lsa_String(&name, TEST_ACCOUNT_NAME);

		r.in.domain_handle = handle;
		r.in.level = levels[i];
		r.in.name = &name;
		r.out.idx = &idx;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetDisplayEnumerationIndex2_r(b, tctx, &r),
			"GetDisplayEnumerationIndex2 failed");
		if (ok_lvl[i] &&
		    !NT_STATUS_IS_OK(r.out.result) &&
		    !NT_STATUS_EQUAL(NT_STATUS_NO_MORE_ENTRIES, r.out.result)) {
			torture_warning(tctx, "GetDisplayEnumerationIndex2 level %u failed - %s\n",
			       levels[i], nt_errstr(r.out.result));
			ret = false;
		}

		init_lsa_String(&name, "zzzzzzzz");

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_GetDisplayEnumerationIndex2_r(b, tctx, &r),
			"GetDisplayEnumerationIndex2 failed");
		if (ok_lvl[i] && !NT_STATUS_EQUAL(NT_STATUS_NO_MORE_ENTRIES, r.out.result)) {
			torture_warning(tctx, "GetDisplayEnumerationIndex2 level %u failed - %s\n",
			       levels[i], nt_errstr(r.out.result));
			ret = false;
		}
	}

	return ret;
}

#define STRING_EQUAL_QUERY(s1, s2, user)					\
	if (s1.string == NULL && s2.string != NULL && s2.string[0] == '\0') { \
		/* odd, but valid */						\
	} else if ((s1.string && !s2.string) || (s2.string && !s1.string) || strcmp(s1.string, s2.string)) { \
			torture_warning(tctx, "%s mismatch for %s: %s != %s (%s)\n", \
			       #s1, user.string,  s1.string, s2.string, __location__);   \
			ret = false; \
	}
#define INT_EQUAL_QUERY(s1, s2, user)		\
		if (s1 != s2) { \
			torture_warning(tctx, "%s mismatch for %s: 0x%llx != 0x%llx (%s)\n", \
			       #s1, user.string, (unsigned long long)s1, (unsigned long long)s2, __location__); \
			ret = false; \
		}

static bool test_each_DisplayInfo_user(struct dcerpc_binding_handle *b,
				       struct torture_context *tctx,
				       struct samr_QueryDisplayInfo *querydisplayinfo,
				       bool *seen_testuser)
{
	struct samr_OpenUser r;
	struct samr_QueryUserInfo q;
	union samr_UserInfo *info;
	struct policy_handle user_handle;
	int i, ret = true;
	r.in.domain_handle = querydisplayinfo->in.domain_handle;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	for (i = 0; ; i++) {
		switch (querydisplayinfo->in.level) {
		case 1:
			if (i >= querydisplayinfo->out.info->info1.count) {
				return ret;
			}
			r.in.rid = querydisplayinfo->out.info->info1.entries[i].rid;
			break;
		case 2:
			if (i >= querydisplayinfo->out.info->info2.count) {
				return ret;
			}
			r.in.rid = querydisplayinfo->out.info->info2.entries[i].rid;
			break;
		case 3:
			/* Groups */
		case 4:
		case 5:
			/* Not interested in validating just the account name */
			return true;
		}

		r.out.user_handle = &user_handle;

		switch (querydisplayinfo->in.level) {
		case 1:
		case 2:
			torture_assert_ntstatus_ok(tctx, dcerpc_samr_OpenUser_r(b, tctx, &r),
				"OpenUser failed");
			if (!NT_STATUS_IS_OK(r.out.result)) {
				torture_warning(tctx, "OpenUser(%u) failed - %s\n", r.in.rid, nt_errstr(r.out.result));
				return false;
			}
		}

		q.in.user_handle = &user_handle;
		q.in.level = 21;
		q.out.info = &info;
		torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryUserInfo_r(b, tctx, &q),
			"QueryUserInfo failed");
		if (!NT_STATUS_IS_OK(r.out.result)) {
			torture_warning(tctx, "QueryUserInfo(%u) failed - %s\n", r.in.rid, nt_errstr(r.out.result));
			return false;
		}

		switch (querydisplayinfo->in.level) {
		case 1:
			if (seen_testuser && strcmp(info->info21.account_name.string, TEST_ACCOUNT_NAME) == 0) {
				*seen_testuser = true;
			}
			STRING_EQUAL_QUERY(querydisplayinfo->out.info->info1.entries[i].full_name,
					   info->info21.full_name, info->info21.account_name);
			STRING_EQUAL_QUERY(querydisplayinfo->out.info->info1.entries[i].account_name,
					   info->info21.account_name, info->info21.account_name);
			STRING_EQUAL_QUERY(querydisplayinfo->out.info->info1.entries[i].description,
					   info->info21.description, info->info21.account_name);
			INT_EQUAL_QUERY(querydisplayinfo->out.info->info1.entries[i].rid,
					info->info21.rid, info->info21.account_name);
			INT_EQUAL_QUERY(querydisplayinfo->out.info->info1.entries[i].acct_flags,
					info->info21.acct_flags, info->info21.account_name);

			break;
		case 2:
			STRING_EQUAL_QUERY(querydisplayinfo->out.info->info2.entries[i].account_name,
					   info->info21.account_name, info->info21.account_name);
			STRING_EQUAL_QUERY(querydisplayinfo->out.info->info2.entries[i].description,
					   info->info21.description, info->info21.account_name);
			INT_EQUAL_QUERY(querydisplayinfo->out.info->info2.entries[i].rid,
					info->info21.rid, info->info21.account_name);
			INT_EQUAL_QUERY((querydisplayinfo->out.info->info2.entries[i].acct_flags & ~ACB_NORMAL),
					info->info21.acct_flags, info->info21.account_name);

			if (!(querydisplayinfo->out.info->info2.entries[i].acct_flags & ACB_NORMAL)) {
				torture_warning(tctx, "Missing ACB_NORMAL in querydisplayinfo->out.info.info2.entries[i].acct_flags on %s\n",
				       info->info21.account_name.string);
			}

			if (!(info->info21.acct_flags & (ACB_WSTRUST | ACB_SVRTRUST))) {
				torture_warning(tctx, "Found non-trust account %s in trust account listing: 0x%x 0x%x\n",
				       info->info21.account_name.string,
				       querydisplayinfo->out.info->info2.entries[i].acct_flags,
				       info->info21.acct_flags);
				return false;
			}

			break;
		}

		if (!test_samr_handle_Close(b, tctx, &user_handle)) {
			return false;
		}
	}
	return ret;
}

static bool test_QueryDisplayInfo(struct dcerpc_binding_handle *b,
				  struct torture_context *tctx,
				  struct policy_handle *handle)
{
	struct samr_QueryDisplayInfo r;
	struct samr_QueryDomainInfo dom_info;
	union samr_DomainInfo *info = NULL;
	bool ret = true;
	uint16_t levels[] = {1, 2, 3, 4, 5};
	int i;
	bool seen_testuser = false;
	uint32_t total_size;
	uint32_t returned_size;
	union samr_DispInfo disp_info;


	for (i=0;i<ARRAY_SIZE(levels);i++) {
		torture_comment(tctx, "Testing QueryDisplayInfo level %u\n", levels[i]);

		r.in.start_idx = 0;
		r.out.result = STATUS_MORE_ENTRIES;
		while (NT_STATUS_EQUAL(r.out.result, STATUS_MORE_ENTRIES)) {
			r.in.domain_handle = handle;
			r.in.level = levels[i];
			r.in.max_entries = 2;
			r.in.buf_size = (uint32_t)-1;
			r.out.total_size = &total_size;
			r.out.returned_size = &returned_size;
			r.out.info = &disp_info;

			torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryDisplayInfo_r(b, tctx, &r),
				"QueryDisplayInfo failed");
			if (!NT_STATUS_EQUAL(r.out.result, STATUS_MORE_ENTRIES) && !NT_STATUS_IS_OK(r.out.result)) {
				torture_warning(tctx, "QueryDisplayInfo level %u failed - %s\n",
				       levels[i], nt_errstr(r.out.result));
				ret = false;
			}
			switch (r.in.level) {
			case 1:
				if (!test_each_DisplayInfo_user(b, tctx, &r, &seen_testuser)) {
					ret = false;
				}
				r.in.start_idx += r.out.info->info1.count;
				break;
			case 2:
				if (!test_each_DisplayInfo_user(b, tctx, &r, NULL)) {
					ret = false;
				}
				r.in.start_idx += r.out.info->info2.count;
				break;
			case 3:
				r.in.start_idx += r.out.info->info3.count;
				break;
			case 4:
				r.in.start_idx += r.out.info->info4.count;
				break;
			case 5:
				r.in.start_idx += r.out.info->info5.count;
				break;
			}
		}
		dom_info.in.domain_handle = handle;
		dom_info.in.level = 2;
		dom_info.out.info = &info;

		/* Check number of users returned is correct */
		torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryDomainInfo_r(b, tctx, &dom_info),
			"QueryDomainInfo failed");
		if (!NT_STATUS_IS_OK(dom_info.out.result)) {
			torture_warning(tctx, "QueryDomainInfo level %u failed - %s\n",
			       r.in.level, nt_errstr(dom_info.out.result));
			ret = false;
			break;
		}
		switch (r.in.level) {
		case 1:
		case 4:
			if (info->general.num_users < r.in.start_idx) {
				/* On AD deployments this numbers don't match
				 * since QueryDisplayInfo returns universal and
				 * global groups, QueryDomainInfo only global
				 * ones. */
				if (torture_setting_bool(tctx, "samba3", false)) {
					torture_warning(tctx, "QueryDomainInfo indicates that QueryDisplayInfo returned more users (%d/%d) than the domain %s is said to contain!\n",
					       r.in.start_idx, info->general.num_groups,
					       info->general.domain_name.string);
					ret = false;
				}
			}
			if (!seen_testuser) {
				struct policy_handle user_handle;
				if (NT_STATUS_IS_OK(test_OpenUser_byname(b, tctx, handle, TEST_ACCOUNT_NAME, &user_handle))) {
					torture_warning(tctx, "Didn't find test user " TEST_ACCOUNT_NAME " in enumeration of %s\n",
					       info->general.domain_name.string);
					ret = false;
					test_samr_handle_Close(b, tctx, &user_handle);
				}
			}
			break;
		case 3:
		case 5:
			if (info->general.num_groups != r.in.start_idx) {
				/* On AD deployments this numbers don't match
				 * since QueryDisplayInfo returns universal and
				 * global groups, QueryDomainInfo only global
				 * ones. */
				if (torture_setting_bool(tctx, "samba3", false)) {
					torture_warning(tctx, "QueryDomainInfo indicates that QueryDisplayInfo didn't return all (%d/%d) the groups in %s\n",
					       r.in.start_idx, info->general.num_groups,
					       info->general.domain_name.string);
					ret = false;
				}
			}

			break;
		}

	}

	return ret;
}

static bool test_QueryDisplayInfo2(struct dcerpc_binding_handle *b,
				   struct torture_context *tctx,
				   struct policy_handle *handle)
{
	struct samr_QueryDisplayInfo2 r;
	bool ret = true;
	uint16_t levels[] = {1, 2, 3, 4, 5};
	int i;
	uint32_t total_size;
	uint32_t returned_size;
	union samr_DispInfo info;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		torture_comment(tctx, "Testing QueryDisplayInfo2 level %u\n", levels[i]);

		r.in.domain_handle = handle;
		r.in.level = levels[i];
		r.in.start_idx = 0;
		r.in.max_entries = 1000;
		r.in.buf_size = (uint32_t)-1;
		r.out.total_size = &total_size;
		r.out.returned_size = &returned_size;
		r.out.info = &info;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryDisplayInfo2_r(b, tctx, &r),
			"QueryDisplayInfo2 failed");
		if (!NT_STATUS_IS_OK(r.out.result)) {
			torture_warning(tctx, "QueryDisplayInfo2 level %u failed - %s\n",
			       levels[i], nt_errstr(r.out.result));
			ret = false;
		}
	}

	return ret;
}

static bool test_QueryDisplayInfo3(struct dcerpc_binding_handle *b,
				   struct torture_context *tctx,
				   struct policy_handle *handle)
{
	struct samr_QueryDisplayInfo3 r;
	bool ret = true;
	uint16_t levels[] = {1, 2, 3, 4, 5};
	int i;
	uint32_t total_size;
	uint32_t returned_size;
	union samr_DispInfo info;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		torture_comment(tctx, "Testing QueryDisplayInfo3 level %u\n", levels[i]);

		r.in.domain_handle = handle;
		r.in.level = levels[i];
		r.in.start_idx = 0;
		r.in.max_entries = 1000;
		r.in.buf_size = (uint32_t)-1;
		r.out.total_size = &total_size;
		r.out.returned_size = &returned_size;
		r.out.info = &info;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryDisplayInfo3_r(b, tctx, &r),
			"QueryDisplayInfo3 failed");
		if (!NT_STATUS_IS_OK(r.out.result)) {
			torture_warning(tctx, "QueryDisplayInfo3 level %u failed - %s\n",
			       levels[i], nt_errstr(r.out.result));
			ret = false;
		}
	}

	return ret;
}


static bool test_QueryDisplayInfo_continue(struct dcerpc_binding_handle *b,
					   struct torture_context *tctx,
					   struct policy_handle *handle)
{
	struct samr_QueryDisplayInfo r;
	bool ret = true;
	uint32_t total_size;
	uint32_t returned_size;
	union samr_DispInfo info;

	torture_comment(tctx, "Testing QueryDisplayInfo continuation\n");

	r.in.domain_handle = handle;
	r.in.level = 1;
	r.in.start_idx = 0;
	r.in.max_entries = 1;
	r.in.buf_size = (uint32_t)-1;
	r.out.total_size = &total_size;
	r.out.returned_size = &returned_size;
	r.out.info = &info;

	do {
		torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryDisplayInfo_r(b, tctx, &r),
			"QueryDisplayInfo failed");
		if (NT_STATUS_IS_OK(r.out.result) && *r.out.returned_size != 0) {
			if (r.out.info->info1.entries[0].idx != r.in.start_idx + 1) {
				torture_warning(tctx, "expected idx %d but got %d\n",
				       r.in.start_idx + 1,
				       r.out.info->info1.entries[0].idx);
				break;
			}
		}
		if (!NT_STATUS_EQUAL(r.out.result, STATUS_MORE_ENTRIES) &&
		    !NT_STATUS_IS_OK(r.out.result)) {
			torture_warning(tctx, "QueryDisplayInfo level %u failed - %s\n",
			       r.in.level, nt_errstr(r.out.result));
			ret = false;
			break;
		}
		r.in.start_idx++;
	} while ((NT_STATUS_EQUAL(r.out.result, STATUS_MORE_ENTRIES) ||
		  NT_STATUS_IS_OK(r.out.result)) &&
		 *r.out.returned_size != 0);

	return ret;
}

static bool test_QueryDomainInfo(struct dcerpc_pipe *p,
				 struct torture_context *tctx,
				 struct policy_handle *handle)
{
	struct samr_QueryDomainInfo r;
	union samr_DomainInfo *info = NULL;
	struct samr_SetDomainInfo s;
	uint16_t levels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13};
	uint16_t set_ok[] = {1, 0, 1, 1, 0, 1, 1, 0, 1,  0,  1,  0};
	int i;
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;
	const char *domain_comment = talloc_asprintf(tctx,
				  "Tortured by Samba4 RPC-SAMR: %s",
				  timestring(tctx, time(NULL)));

	s.in.domain_handle = handle;
	s.in.level = 4;
	s.in.info = talloc(tctx, union samr_DomainInfo);

	s.in.info->oem.oem_information.string = domain_comment;
	torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetDomainInfo_r(b, tctx, &s),
		"SetDomainInfo failed");
	if (!NT_STATUS_IS_OK(s.out.result)) {
		torture_warning(tctx, "SetDomainInfo level %u (set comment) failed - %s\n",
		       s.in.level, nt_errstr(s.out.result));
		return false;
	}

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		torture_comment(tctx, "Testing QueryDomainInfo level %u\n", levels[i]);

		r.in.domain_handle = handle;
		r.in.level = levels[i];
		r.out.info = &info;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryDomainInfo_r(b, tctx, &r),
			"QueryDomainInfo failed");
		if (!NT_STATUS_IS_OK(r.out.result)) {
			torture_warning(tctx, "QueryDomainInfo level %u failed - %s\n",
			       r.in.level, nt_errstr(r.out.result));
			ret = false;
			continue;
		}

		switch (levels[i]) {
		case 2:
			if (strcmp(info->general.oem_information.string, domain_comment) != 0) {
				torture_warning(tctx, "QueryDomainInfo level %u returned different oem_information (comment) (%s, expected %s)\n",
				       levels[i], info->general.oem_information.string, domain_comment);
				if (!torture_setting_bool(tctx, "samba3", false)) {
					ret = false;
				}
			}
			if (!info->general.primary.string) {
				torture_warning(tctx, "QueryDomainInfo level %u returned no PDC name\n",
				       levels[i]);
				ret = false;
			} else if (info->general.role == SAMR_ROLE_DOMAIN_PDC) {
				if (dcerpc_server_name(p) && strcasecmp_m(dcerpc_server_name(p), info->general.primary.string) != 0) {
					if (torture_setting_bool(tctx, "samba3", false)) {
						torture_warning(tctx, "QueryDomainInfo level %u returned different PDC name (%s) compared to server name (%s), despite claiming to be the PDC\n",
						       levels[i], info->general.primary.string, dcerpc_server_name(p));
					}
				}
			}
			break;
		case 4:
			if (strcmp(info->oem.oem_information.string, domain_comment) != 0) {
				torture_warning(tctx, "QueryDomainInfo level %u returned different oem_information (comment) (%s, expected %s)\n",
				       levels[i], info->oem.oem_information.string, domain_comment);
				if (!torture_setting_bool(tctx, "samba3", false)) {
					ret = false;
				}
			}
			break;
		case 6:
			if (!info->info6.primary.string) {
				torture_warning(tctx, "QueryDomainInfo level %u returned no PDC name\n",
				       levels[i]);
				ret = false;
			}
			break;
		case 11:
			if (strcmp(info->general2.general.oem_information.string, domain_comment) != 0) {
				torture_warning(tctx, "QueryDomainInfo level %u returned different comment (%s, expected %s)\n",
				       levels[i], info->general2.general.oem_information.string, domain_comment);
				if (!torture_setting_bool(tctx, "samba3", false)) {
					ret = false;
				}
			}
			break;
		}

		torture_comment(tctx, "Testing SetDomainInfo level %u\n", levels[i]);

		s.in.domain_handle = handle;
		s.in.level = levels[i];
		s.in.info = info;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetDomainInfo_r(b, tctx, &s),
			"SetDomainInfo failed");
		if (set_ok[i]) {
			if (!NT_STATUS_IS_OK(s.out.result)) {
				torture_warning(tctx, "SetDomainInfo level %u failed - %s\n",
				       r.in.level, nt_errstr(s.out.result));
				ret = false;
				continue;
			}
		} else {
			if (!NT_STATUS_EQUAL(NT_STATUS_INVALID_INFO_CLASS, s.out.result)) {
				torture_warning(tctx, "SetDomainInfo level %u gave %s - should have been NT_STATUS_INVALID_INFO_CLASS\n",
				       r.in.level, nt_errstr(s.out.result));
				ret = false;
				continue;
			}
		}

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryDomainInfo_r(b, tctx, &r),
			"QueryDomainInfo failed");
		if (!NT_STATUS_IS_OK(r.out.result)) {
			torture_warning(tctx, "QueryDomainInfo level %u failed - %s\n",
			       r.in.level, nt_errstr(r.out.result));
			ret = false;
			continue;
		}
	}

	return ret;
}


static bool test_QueryDomainInfo2(struct dcerpc_binding_handle *b,
				  struct torture_context *tctx,
				  struct policy_handle *handle)
{
	struct samr_QueryDomainInfo2 r;
	union samr_DomainInfo *info = NULL;
	uint16_t levels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13};
	int i;
	bool ret = true;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		torture_comment(tctx, "Testing QueryDomainInfo2 level %u\n", levels[i]);

		r.in.domain_handle = handle;
		r.in.level = levels[i];
		r.out.info = &info;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryDomainInfo2_r(b, tctx, &r),
			"QueryDomainInfo2 failed");
		if (!NT_STATUS_IS_OK(r.out.result)) {
			torture_warning(tctx, "QueryDomainInfo2 level %u failed - %s\n",
			       r.in.level, nt_errstr(r.out.result));
			ret = false;
			continue;
		}
	}

	return true;
}

/* Test whether querydispinfo level 5 and enumdomgroups return the same
   set of group names. */
static bool test_GroupList(struct dcerpc_binding_handle *b,
			   struct torture_context *tctx,
			   struct dom_sid *domain_sid,
			   struct policy_handle *handle)
{
	struct samr_EnumDomainGroups q1;
	struct samr_QueryDisplayInfo q2;
	NTSTATUS status;
	uint32_t resume_handle=0;
	struct samr_SamArray *sam = NULL;
	uint32_t num_entries = 0;
	int i;
	bool ret = true;
	uint32_t total_size;
	uint32_t returned_size;
	union samr_DispInfo info;

	int num_names = 0;
	const char **names = NULL;

	bool builtin_domain = dom_sid_compare(domain_sid,
					      &global_sid_Builtin) == 0;

	torture_comment(tctx, "Testing coherency of querydispinfo vs enumdomgroups\n");

	q1.in.domain_handle = handle;
	q1.in.resume_handle = &resume_handle;
	q1.in.max_size = 5;
	q1.out.resume_handle = &resume_handle;
	q1.out.num_entries = &num_entries;
	q1.out.sam = &sam;

	status = STATUS_MORE_ENTRIES;
	while (NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES)) {
		torture_assert_ntstatus_ok(tctx, dcerpc_samr_EnumDomainGroups_r(b, tctx, &q1),
			"EnumDomainGroups failed");
		status = q1.out.result;

		if (!NT_STATUS_IS_OK(status) &&
		    !NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES))
			break;

		for (i=0; i<*q1.out.num_entries; i++) {
			add_string_to_array(tctx,
					    sam->entries[i].name.string,
					    &names, &num_names);
		}
	}

	torture_assert_ntstatus_ok(tctx, status, "EnumDomainGroups");

	torture_assert(tctx, sam, "EnumDomainGroups failed to return sam");

	if (builtin_domain) {
		torture_assert(tctx, num_names == 0,
			       "EnumDomainGroups shouldn't return any group in the builtin domain!");
	}

	q2.in.domain_handle = handle;
	q2.in.level = 5;
	q2.in.start_idx = 0;
	q2.in.max_entries = 5;
	q2.in.buf_size = (uint32_t)-1;
	q2.out.total_size = &total_size;
	q2.out.returned_size = &returned_size;
	q2.out.info = &info;

	status = STATUS_MORE_ENTRIES;
	while (NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES)) {
		torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryDisplayInfo_r(b, tctx, &q2),
			"QueryDisplayInfo failed");
		status = q2.out.result;
		if (!NT_STATUS_IS_OK(status) &&
		    !NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES))
			break;

		for (i=0; i<q2.out.info->info5.count; i++) {
			int j;
			const char *name = q2.out.info->info5.entries[i].account_name.string;
			bool found = false;
			for (j=0; j<num_names; j++) {
				if (names[j] == NULL)
					continue;
				if (strequal(names[j], name)) {
					names[j] = NULL;
					found = true;
					break;
				}
			}

			if ((!found) && (!builtin_domain)) {
				torture_warning(tctx, "QueryDisplayInfo gave name [%s] that EnumDomainGroups did not\n",
				       name);
				ret = false;
			}
		}
		q2.in.start_idx += q2.out.info->info5.count;
	}

	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(tctx, "QueryDisplayInfo level 5 failed - %s\n",
		       nt_errstr(status));
		ret = false;
	}

	if (builtin_domain) {
		torture_assert(tctx, q2.in.start_idx != 0,
			       "QueryDisplayInfo should return all domain groups also on the builtin domain handle!");
	}

	for (i=0; i<num_names; i++) {
		if (names[i] != NULL) {
			torture_warning(tctx, "EnumDomainGroups gave name [%s] that QueryDisplayInfo did not\n",
			       names[i]);
			ret = false;
		}
	}

	return ret;
}

static bool test_DeleteDomainGroup(struct dcerpc_binding_handle *b,
				   struct torture_context *tctx,
				   struct policy_handle *group_handle)
{
    	struct samr_DeleteDomainGroup d;

	torture_comment(tctx, "Testing DeleteDomainGroup\n");

	d.in.group_handle = group_handle;
	d.out.group_handle = group_handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_DeleteDomainGroup_r(b, tctx, &d),
		"DeleteDomainGroup failed");
	torture_assert_ntstatus_ok(tctx, d.out.result, "DeleteDomainGroup");

	return true;
}

static bool test_TestPrivateFunctionsDomain(struct dcerpc_binding_handle *b,
					    struct torture_context *tctx,
					    struct policy_handle *domain_handle)
{
    	struct samr_TestPrivateFunctionsDomain r;
	bool ret = true;

	torture_comment(tctx, "Testing TestPrivateFunctionsDomain\n");

	r.in.domain_handle = domain_handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_TestPrivateFunctionsDomain_r(b, tctx, &r),
		"TestPrivateFunctionsDomain failed");
	torture_assert_ntstatus_equal(tctx, r.out.result, NT_STATUS_NOT_IMPLEMENTED, "TestPrivateFunctionsDomain");

	return ret;
}

static bool test_RidToSid(struct dcerpc_binding_handle *b,
			  struct torture_context *tctx,
			  struct dom_sid *domain_sid,
			  struct policy_handle *domain_handle)
{
    	struct samr_RidToSid r;
	bool ret = true;
	struct dom_sid *calc_sid, *out_sid;
	int rids[] = { 0, 42, 512, 10200 };
	int i;

	for (i=0;i<ARRAY_SIZE(rids);i++) {
		torture_comment(tctx, "Testing RidToSid\n");

		calc_sid = dom_sid_dup(tctx, domain_sid);
		r.in.domain_handle = domain_handle;
		r.in.rid = rids[i];
		r.out.sid = &out_sid;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_RidToSid_r(b, tctx, &r),
			"RidToSid failed");
		if (!NT_STATUS_IS_OK(r.out.result)) {
			torture_warning(tctx, "RidToSid for %d failed - %s\n", rids[i], nt_errstr(r.out.result));
			ret = false;
		} else {
			calc_sid = dom_sid_add_rid(calc_sid, calc_sid, rids[i]);

			if (!dom_sid_equal(calc_sid, out_sid)) {
				torture_warning(tctx, "RidToSid for %d failed - got %s, expected %s\n", rids[i],
				       dom_sid_string(tctx, out_sid),
				       dom_sid_string(tctx, calc_sid));
				ret = false;
			}
		}
	}

	return ret;
}

static bool test_GetBootKeyInformation(struct dcerpc_binding_handle *b,
				       struct torture_context *tctx,
				       struct policy_handle *domain_handle)
{
	struct samr_GetBootKeyInformation r;
	bool ret = true;
	uint32_t unknown = 0;
	NTSTATUS status;

	torture_comment(tctx, "Testing GetBootKeyInformation\n");

	r.in.domain_handle = domain_handle;
	r.out.unknown = &unknown;

	status = dcerpc_samr_GetBootKeyInformation_r(b, tctx, &r);
	if (NT_STATUS_IS_OK(status) && !NT_STATUS_IS_OK(r.out.result)) {
		status = r.out.result;
	}
	if (!NT_STATUS_IS_OK(status)) {
		/* w2k3 seems to fail this sometimes and pass it sometimes */
		torture_comment(tctx, "GetBootKeyInformation (ignored) - %s\n", nt_errstr(status));
	}

	return ret;
}

static bool test_AddGroupMember(struct dcerpc_binding_handle *b,
				struct torture_context *tctx,
				struct policy_handle *domain_handle,
				struct policy_handle *group_handle)
{
	NTSTATUS status;
	struct samr_AddGroupMember r;
	struct samr_DeleteGroupMember d;
	struct samr_QueryGroupMember q;
	struct samr_RidAttrArray *rids = NULL;
	struct samr_SetMemberAttributesOfGroup s;
	uint32_t rid;
	bool found_member = false;
	int i;

	status = test_LookupName(b, tctx, domain_handle, TEST_ACCOUNT_NAME, &rid);
	torture_assert_ntstatus_ok(tctx, status, "test_AddGroupMember looking up name " TEST_ACCOUNT_NAME);

	r.in.group_handle = group_handle;
	r.in.rid = rid;
	r.in.flags = 0; /* ??? */

	torture_comment(tctx, "Testing AddGroupMember, QueryGroupMember and DeleteGroupMember\n");

	d.in.group_handle = group_handle;
	d.in.rid = rid;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_DeleteGroupMember_r(b, tctx, &d),
		"DeleteGroupMember failed");
	torture_assert_ntstatus_equal(tctx, NT_STATUS_MEMBER_NOT_IN_GROUP, d.out.result, "DeleteGroupMember");

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_AddGroupMember_r(b, tctx, &r),
		"AddGroupMember failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "AddGroupMember");

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_AddGroupMember_r(b, tctx, &r),
		"AddGroupMember failed");
	torture_assert_ntstatus_equal(tctx, NT_STATUS_MEMBER_IN_GROUP, r.out.result, "AddGroupMember");

	if (torture_setting_bool(tctx, "samba4", false) ||
	    torture_setting_bool(tctx, "samba3", false)) {
		torture_comment(tctx, "skipping SetMemberAttributesOfGroup test against Samba\n");
	} else {
		/* this one is quite strange. I am using random inputs in the
		   hope of triggering an error that might give us a clue */

		s.in.group_handle = group_handle;
		s.in.unknown1 = random();
		s.in.unknown2 = random();

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_SetMemberAttributesOfGroup_r(b, tctx, &s),
			"SetMemberAttributesOfGroup failed");
		torture_assert_ntstatus_ok(tctx, s.out.result, "SetMemberAttributesOfGroup");
	}

	q.in.group_handle = group_handle;
	q.out.rids = &rids;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryGroupMember_r(b, tctx, &q),
		"QueryGroupMember failed");
	torture_assert_ntstatus_ok(tctx, q.out.result, "QueryGroupMember");
	torture_assert(tctx, rids, "QueryGroupMember did not fill in rids structure");

	for (i=0; i < rids->count; i++) {
		if (rids->rids[i] == rid) {
			found_member = true;
		}
	}

	torture_assert(tctx, found_member, "QueryGroupMember did not list newly added member");

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_DeleteGroupMember_r(b, tctx, &d),
		"DeleteGroupMember failed");
	torture_assert_ntstatus_ok(tctx, d.out.result, "DeleteGroupMember");

	rids = NULL;
	found_member = false;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryGroupMember_r(b, tctx, &q),
		"QueryGroupMember failed");
	torture_assert_ntstatus_ok(tctx, q.out.result, "QueryGroupMember");
	torture_assert(tctx, rids, "QueryGroupMember did not fill in rids structure");

	for (i=0; i < rids->count; i++) {
		if (rids->rids[i] == rid) {
			found_member = true;
		}
	}

	torture_assert(tctx, !found_member, "QueryGroupMember does still list removed member");

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_AddGroupMember_r(b, tctx, &r),
		"AddGroupMember failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "AddGroupMember");

	return true;
}


static bool test_CreateDomainGroup(struct dcerpc_binding_handle *b,
				   struct torture_context *tctx,
				   struct policy_handle *domain_handle,
				   const char *group_name,
				   struct policy_handle *group_handle,
				   struct dom_sid *domain_sid,
				   bool test_group)
{
	struct samr_CreateDomainGroup r;
	uint32_t rid;
	struct lsa_String name;
	bool ret = true;

	init_lsa_String(&name, group_name);

	r.in.domain_handle = domain_handle;
	r.in.name = &name;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.group_handle = group_handle;
	r.out.rid = &rid;

	torture_comment(tctx, "Testing CreateDomainGroup(%s)\n", r.in.name->string);

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_CreateDomainGroup_r(b, tctx, &r),
		"CreateDomainGroup failed");

	if (dom_sid_equal(domain_sid, dom_sid_parse_talloc(tctx, SID_BUILTIN))) {
		if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_ACCESS_DENIED)) {
			torture_comment(tctx, "Server correctly refused create of '%s'\n", r.in.name->string);
			return true;
		} else {
			torture_warning(tctx, "Server should have refused create of '%s', got %s instead\n", r.in.name->string,
			       nt_errstr(r.out.result));
			return false;
		}
	}

	if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_GROUP_EXISTS)) {
		if (!test_DeleteGroup_byname(b, tctx, domain_handle, r.in.name->string)) {
			torture_warning(tctx, "CreateDomainGroup failed: Could not delete domain group %s - %s\n", r.in.name->string,
			       nt_errstr(r.out.result));
			return false;
		}
		torture_assert_ntstatus_ok(tctx, dcerpc_samr_CreateDomainGroup_r(b, tctx, &r),
			"CreateDomainGroup failed");
	}
	if (NT_STATUS_EQUAL(r.out.result, NT_STATUS_USER_EXISTS)) {
		if (!test_DeleteUser_byname(b, tctx, domain_handle, r.in.name->string)) {

			torture_warning(tctx, "CreateDomainGroup failed: Could not delete user %s - %s\n", r.in.name->string,
			       nt_errstr(r.out.result));
			return false;
		}
		torture_assert_ntstatus_ok(tctx, dcerpc_samr_CreateDomainGroup_r(b, tctx, &r),
			"CreateDomainGroup failed");
	}
	torture_assert_ntstatus_ok(tctx, r.out.result, "CreateDomainGroup");

	if (!test_group) {
		return ret;
	}

	if (!test_AddGroupMember(b, tctx, domain_handle, group_handle)) {
		torture_warning(tctx, "CreateDomainGroup failed - %s\n", nt_errstr(r.out.result));
		ret = false;
	}

	if (!test_SetGroupInfo(b, tctx, group_handle)) {
		ret = false;
	}

	return ret;
}


/*
  its not totally clear what this does. It seems to accept any sid you like.
*/
static bool test_RemoveMemberFromForeignDomain(struct dcerpc_binding_handle *b,
					       struct torture_context *tctx,
					       struct policy_handle *domain_handle)
{
	struct samr_RemoveMemberFromForeignDomain r;

	r.in.domain_handle = domain_handle;
	r.in.sid = dom_sid_parse_talloc(tctx, "S-1-5-32-12-34-56-78");

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_RemoveMemberFromForeignDomain_r(b, tctx, &r),
		"RemoveMemberFromForeignDomain failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "RemoveMemberFromForeignDomain");

	return true;
}

static bool test_EnumDomainUsers(struct dcerpc_binding_handle *b,
				 struct torture_context *tctx,
				 struct policy_handle *domain_handle,
				 uint32_t *total_num_entries_p)
{
	NTSTATUS status;
	struct samr_EnumDomainUsers r;
	uint32_t resume_handle = 0;
	uint32_t num_entries = 0;
	uint32_t total_num_entries = 0;
	struct samr_SamArray *sam;

	r.in.domain_handle = domain_handle;
	r.in.acct_flags = 0;
	r.in.max_size = (uint32_t)-1;
	r.in.resume_handle = &resume_handle;

	r.out.sam = &sam;
	r.out.num_entries = &num_entries;
	r.out.resume_handle = &resume_handle;

	torture_comment(tctx, "Testing EnumDomainUsers\n");

	do {
		torture_assert_ntstatus_ok(tctx, dcerpc_samr_EnumDomainUsers_r(b, tctx, &r),
			"EnumDomainUsers failed");
		if (NT_STATUS_IS_ERR(r.out.result)) {
			torture_assert_ntstatus_ok(tctx, r.out.result,
				"failed to enumerate users");
		}
		status = r.out.result;

		total_num_entries += num_entries;
	} while (NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES));

	if (total_num_entries_p) {
		*total_num_entries_p = total_num_entries;
	}

	return true;
}

static bool test_EnumDomainGroups(struct dcerpc_binding_handle *b,
				  struct torture_context *tctx,
				  struct policy_handle *domain_handle,
				  uint32_t *total_num_entries_p)
{
	NTSTATUS status;
	struct samr_EnumDomainGroups r;
	uint32_t resume_handle = 0;
	uint32_t num_entries = 0;
	uint32_t total_num_entries = 0;
	struct samr_SamArray *sam;

	r.in.domain_handle = domain_handle;
	r.in.max_size = (uint32_t)-1;
	r.in.resume_handle = &resume_handle;

	r.out.sam = &sam;
	r.out.num_entries = &num_entries;
	r.out.resume_handle = &resume_handle;

	torture_comment(tctx, "Testing EnumDomainGroups\n");

	do {
		torture_assert_ntstatus_ok(tctx, dcerpc_samr_EnumDomainGroups_r(b, tctx, &r),
			"EnumDomainGroups failed");
		if (NT_STATUS_IS_ERR(r.out.result)) {
			torture_assert_ntstatus_ok(tctx, r.out.result,
				"failed to enumerate groups");
		}
		status = r.out.result;

		total_num_entries += num_entries;
	} while (NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES));

	if (total_num_entries_p) {
		*total_num_entries_p = total_num_entries;
	}

	return true;
}

static bool test_EnumDomainAliases(struct dcerpc_binding_handle *b,
				   struct torture_context *tctx,
				   struct policy_handle *domain_handle,
				   uint32_t *total_num_entries_p)
{
	NTSTATUS status;
	struct samr_EnumDomainAliases r;
	uint32_t resume_handle = 0;
	uint32_t num_entries = 0;
	uint32_t total_num_entries = 0;
	struct samr_SamArray *sam;

	r.in.domain_handle = domain_handle;
	r.in.max_size = (uint32_t)-1;
	r.in.resume_handle = &resume_handle;

	r.out.sam = &sam;
	r.out.num_entries = &num_entries;
	r.out.resume_handle = &resume_handle;

	torture_comment(tctx, "Testing EnumDomainAliases\n");

	do {
		torture_assert_ntstatus_ok(tctx, dcerpc_samr_EnumDomainAliases_r(b, tctx, &r),
			"EnumDomainAliases failed");
		if (NT_STATUS_IS_ERR(r.out.result)) {
			torture_assert_ntstatus_ok(tctx, r.out.result,
				"failed to enumerate aliases");
		}
		status = r.out.result;

		total_num_entries += num_entries;
	} while (NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES));

	if (total_num_entries_p) {
		*total_num_entries_p = total_num_entries;
	}

	return true;
}

static bool test_QueryDisplayInfo_level(struct dcerpc_binding_handle *b,
					struct torture_context *tctx,
					struct policy_handle *handle,
					uint16_t level,
					uint32_t *total_num_entries_p)
{
	NTSTATUS status;
	struct samr_QueryDisplayInfo r;
	uint32_t total_num_entries = 0;

	r.in.domain_handle = handle;
	r.in.level = level;
	r.in.start_idx = 0;
	r.in.max_entries = (uint32_t)-1;
	r.in.buf_size = (uint32_t)-1;

	torture_comment(tctx, "Testing QueryDisplayInfo\n");

	do {
		uint32_t total_size;
		uint32_t returned_size;
		union samr_DispInfo info;

		r.out.total_size = &total_size;
		r.out.returned_size = &returned_size;
		r.out.info = &info;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryDisplayInfo_r(b, tctx, &r),
			"failed to query displayinfo");
		if (NT_STATUS_IS_ERR(r.out.result)) {
			torture_assert_ntstatus_ok(tctx, r.out.result,
				"failed to query displayinfo");
		}
		status = r.out.result;

		if (*r.out.returned_size == 0) {
			break;
		}

		switch (r.in.level) {
		case 1:
			total_num_entries += info.info1.count;
			r.in.start_idx += info.info1.entries[info.info1.count - 1].idx + 1;
			break;
		case 2:
			total_num_entries += info.info2.count;
			r.in.start_idx += info.info2.entries[info.info2.count - 1].idx + 1;
			break;
		case 3:
			total_num_entries += info.info3.count;
			r.in.start_idx += info.info3.entries[info.info3.count - 1].idx + 1;
			break;
		case 4:
			total_num_entries += info.info4.count;
			r.in.start_idx += info.info4.entries[info.info4.count - 1].idx + 1;
			break;
		case 5:
			total_num_entries += info.info5.count;
			r.in.start_idx += info.info5.entries[info.info5.count - 1].idx + 1;
			break;
		default:
			return false;
		}

	} while (NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES));

	if (total_num_entries_p) {
		*total_num_entries_p = total_num_entries;
	}

	return true;
}

static bool test_ManyObjects(struct dcerpc_pipe *p,
			     struct torture_context *tctx,
			     struct policy_handle *domain_handle,
			     struct dom_sid *domain_sid,
			     struct torture_samr_context *ctx)
{
	uint32_t num_total = ctx->num_objects_large_dc;
	uint32_t num_enum = 0;
	uint32_t num_disp = 0;
	uint32_t num_created = 0;
	uint32_t num_anounced = 0;
	uint32_t i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	struct policy_handle *handles = talloc_zero_array(tctx, struct policy_handle, num_total);

	/* query */

	{
		struct samr_QueryDomainInfo2 r;
		union samr_DomainInfo *info;
		r.in.domain_handle = domain_handle;
		r.in.level = 2;
		r.out.info = &info;

		torture_assert_ntstatus_ok(tctx, dcerpc_samr_QueryDomainInfo2_r(b, tctx, &r),
			"QueryDomainInfo2 failed");
		torture_assert_ntstatus_ok(tctx, r.out.result,
			"failed to query domain info");

		switch (ctx->choice) {
		case TORTURE_SAMR_MANY_ACCOUNTS:
			num_anounced = info->general.num_users;
			break;
		case TORTURE_SAMR_MANY_GROUPS:
			num_anounced = info->general.num_groups;
			break;
		case TORTURE_SAMR_MANY_ALIASES:
			num_anounced = info->general.num_aliases;
			break;
		default:
			return false;
		}
	}

	/* create */

	for (i=0; i < num_total; i++) {

		const char *name = NULL;

		switch (ctx->choice) {
		case TORTURE_SAMR_MANY_ACCOUNTS:
			name = talloc_asprintf(tctx, "%s%04d", TEST_ACCOUNT_NAME, i);
			torture_assert(tctx,
				test_CreateUser(p, tctx, domain_handle, name, &handles[i], domain_sid, 0, NULL, false),
				"failed to create user");
			break;
		case TORTURE_SAMR_MANY_GROUPS:
			name = talloc_asprintf(tctx, "%s%04d", TEST_GROUPNAME, i);
			torture_assert(tctx,
				test_CreateDomainGroup(b, tctx, domain_handle, name, &handles[i], domain_sid, false),
				"failed to create group");
			break;
		case TORTURE_SAMR_MANY_ALIASES:
			name = talloc_asprintf(tctx, "%s%04d", TEST_ALIASNAME, i);
			torture_assert(tctx,
				test_CreateAlias(b, tctx, domain_handle, name, &handles[i], domain_sid, false),
				"failed to create alias");
			break;
		default:
			return false;
		}
		if (!policy_handle_empty(&handles[i])) {
			num_created++;
		}
	}

	/* enum */

	switch (ctx->choice) {
	case TORTURE_SAMR_MANY_ACCOUNTS:
		torture_assert(tctx,
			test_EnumDomainUsers(b, tctx, domain_handle, &num_enum),
			"failed to enum users");
		break;
	case TORTURE_SAMR_MANY_GROUPS:
		torture_assert(tctx,
			test_EnumDomainGroups(b, tctx, domain_handle, &num_enum),
			"failed to enum groups");
		break;
	case TORTURE_SAMR_MANY_ALIASES:
		torture_assert(tctx,
			test_EnumDomainAliases(b, tctx, domain_handle, &num_enum),
			"failed to enum aliases");
		break;
	default:
		return false;
	}

	/* dispinfo */

	switch (ctx->choice) {
	case TORTURE_SAMR_MANY_ACCOUNTS:
		torture_assert(tctx,
			test_QueryDisplayInfo_level(b, tctx, domain_handle, 1, &num_disp),
			"failed to query display info");
		break;
	case TORTURE_SAMR_MANY_GROUPS:
		torture_assert(tctx,
			test_QueryDisplayInfo_level(b, tctx, domain_handle, 3, &num_disp),
			"failed to query display info");
		break;
	case TORTURE_SAMR_MANY_ALIASES:
		/* no aliases in dispinfo */
		break;
	default:
		return false;
	}

	/* close or delete */

	for (i=0; i < num_total; i++) {

		if (policy_handle_empty(&handles[i])) {
			continue;
		}

		if (torture_setting_bool(tctx, "samba3", false)) {
			torture_assert(tctx,
				test_samr_handle_Close(b, tctx, &handles[i]),
				"failed to close handle");
		} else {
			switch (ctx->choice) {
			case TORTURE_SAMR_MANY_ACCOUNTS:
				torture_assert(tctx,
					test_DeleteUser(b, tctx, &handles[i]),
					"failed to delete user");
				break;
			case TORTURE_SAMR_MANY_GROUPS:
				torture_assert(tctx,
					test_DeleteDomainGroup(b, tctx, &handles[i]),
					"failed to delete group");
				break;
			case TORTURE_SAMR_MANY_ALIASES:
				torture_assert(tctx,
					test_DeleteAlias(b, tctx, &handles[i]),
					"failed to delete alias");
				break;
			default:
				return false;
			}
		}
	}

	talloc_free(handles);

	if (ctx->choice == TORTURE_SAMR_MANY_ACCOUNTS && num_enum != num_anounced + num_created) {
		torture_comment(tctx,
				"unexpected number of results (%u) returned in enum call, expected %u\n",
				num_enum, num_anounced + num_created);

		torture_comment(tctx,
				"unexpected number of results (%u) returned in dispinfo, call, expected %u\n",
				num_disp, num_anounced + num_created);
	}

	return true;
}

static bool test_Connect(struct dcerpc_binding_handle *b,
			 struct torture_context *tctx,
			 struct policy_handle *handle);

static bool test_OpenDomain(struct dcerpc_pipe *p, struct torture_context *tctx,
			    struct torture_samr_context *ctx, struct dom_sid *sid)
{
	struct samr_OpenDomain r;
	struct policy_handle domain_handle;
	struct policy_handle alias_handle;
	struct policy_handle user_handle;
	struct policy_handle group_handle;
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(alias_handle);
	ZERO_STRUCT(user_handle);
	ZERO_STRUCT(group_handle);
	ZERO_STRUCT(domain_handle);

	torture_comment(tctx, "Testing OpenDomain of %s\n", dom_sid_string(tctx, sid));

	r.in.connect_handle = &ctx->handle;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.in.sid = sid;
	r.out.domain_handle = &domain_handle;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_OpenDomain_r(b, tctx, &r),
		"OpenDomain failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "OpenDomain failed");

	/* run the domain tests with the main handle closed - this tests
	   the servers reference counting */
	torture_assert(tctx, test_samr_handle_Close(b, tctx, &ctx->handle), "Failed to close SAMR handle");

	switch (ctx->choice) {
	case TORTURE_SAMR_PASSWORDS:
	case TORTURE_SAMR_USER_PRIVILEGES:
		if (!torture_setting_bool(tctx, "samba3", false)) {
			ret &= test_CreateUser2(p, tctx, &domain_handle, sid, ctx->choice, NULL);
		}
		ret &= test_CreateUser(p, tctx, &domain_handle, TEST_ACCOUNT_NAME, &user_handle, sid, ctx->choice, NULL, true);
		if (!ret) {
			torture_warning(tctx, "Testing PASSWORDS or PRIVILEGES on domain %s failed!\n", dom_sid_string(tctx, sid));
		}
		break;
	case TORTURE_SAMR_USER_ATTRIBUTES:
		if (!torture_setting_bool(tctx, "samba3", false)) {
			ret &= test_CreateUser2(p, tctx, &domain_handle, sid, ctx->choice, NULL);
		}
		ret &= test_CreateUser(p, tctx, &domain_handle, TEST_ACCOUNT_NAME, &user_handle, sid, ctx->choice, NULL, true);
		/* This test needs 'complex' users to validate */
		ret &= test_QueryDisplayInfo(b, tctx, &domain_handle);
		if (!ret) {
			torture_warning(tctx, "Testing ATTRIBUTES on domain %s failed!\n", dom_sid_string(tctx, sid));
		}
		break;
	case TORTURE_SAMR_PASSWORDS_PWDLASTSET:
	case TORTURE_SAMR_PASSWORDS_BADPWDCOUNT:
	case TORTURE_SAMR_PASSWORDS_LOCKOUT:
		if (!torture_setting_bool(tctx, "samba3", false)) {
			ret &= test_CreateUser2(p, tctx, &domain_handle, sid, ctx->choice, ctx->machine_credentials);
		}
		ret &= test_CreateUser(p, tctx, &domain_handle, TEST_ACCOUNT_NAME, &user_handle, sid, ctx->choice, ctx->machine_credentials, true);
		if (!ret) {
			torture_warning(tctx, "Testing PASSWORDS PWDLASTSET or BADPWDCOUNT on domain %s failed!\n", dom_sid_string(tctx, sid));
		}
		break;
	case TORTURE_SAMR_MANY_ACCOUNTS:
	case TORTURE_SAMR_MANY_GROUPS:
	case TORTURE_SAMR_MANY_ALIASES:
		ret &= test_ManyObjects(p, tctx, &domain_handle, sid, ctx);
		if (!ret) {
			torture_warning(tctx, "Testing MANY-{ACCOUNTS,GROUPS,ALIASES} on domain %s failed!\n", dom_sid_string(tctx, sid));
		}
		break;
	case TORTURE_SAMR_OTHER:
		ret &= test_CreateUser(p, tctx, &domain_handle, TEST_ACCOUNT_NAME, &user_handle, sid, ctx->choice, NULL, true);
		if (!ret) {
			torture_warning(tctx, "Failed to CreateUser in SAMR-OTHER on domain %s!\n", dom_sid_string(tctx, sid));
		}
		if (!torture_setting_bool(tctx, "samba3", false)) {
			ret &= test_QuerySecurity(b, tctx, &domain_handle);
		}
		ret &= test_RemoveMemberFromForeignDomain(b, tctx, &domain_handle);
		ret &= test_CreateAlias(b, tctx, &domain_handle, TEST_ALIASNAME, &alias_handle, sid, true);
		ret &= test_CreateDomainGroup(b, tctx, &domain_handle, TEST_GROUPNAME, &group_handle, sid, true);
		ret &= test_GetAliasMembership(b, tctx, &domain_handle);
		ret &= test_QueryDomainInfo(p, tctx, &domain_handle);
		ret &= test_QueryDomainInfo2(b, tctx, &domain_handle);
		ret &= test_EnumDomainUsers_all(b, tctx, &domain_handle);
		ret &= test_EnumDomainUsers_async(p, tctx, &domain_handle);
		ret &= test_EnumDomainGroups_all(b, tctx, &domain_handle);
		ret &= test_EnumDomainAliases_all(b, tctx, &domain_handle);
		ret &= test_QueryDisplayInfo2(b, tctx, &domain_handle);
		ret &= test_QueryDisplayInfo3(b, tctx, &domain_handle);
		ret &= test_QueryDisplayInfo_continue(b, tctx, &domain_handle);

		if (torture_setting_bool(tctx, "samba4", false)) {
			torture_comment(tctx, "skipping GetDisplayEnumerationIndex test against Samba4\n");
		} else {
			ret &= test_GetDisplayEnumerationIndex(b, tctx, &domain_handle);
			ret &= test_GetDisplayEnumerationIndex2(b, tctx, &domain_handle);
		}
		ret &= test_GroupList(b, tctx, sid, &domain_handle);
		ret &= test_TestPrivateFunctionsDomain(b, tctx, &domain_handle);
		ret &= test_RidToSid(b, tctx, sid, &domain_handle);
		ret &= test_GetBootKeyInformation(b, tctx, &domain_handle);
		if (!ret) {
			torture_comment(tctx, "Testing SAMR-OTHER on domain %s failed!\n", dom_sid_string(tctx, sid));
		}
		break;
	}

	if (!policy_handle_empty(&user_handle) &&
	    !test_DeleteUser(b, tctx, &user_handle)) {
		ret = false;
	}

	if (!policy_handle_empty(&alias_handle) &&
	    !test_DeleteAlias(b, tctx, &alias_handle)) {
		ret = false;
	}

	if (!policy_handle_empty(&group_handle) &&
	    !test_DeleteDomainGroup(b, tctx, &group_handle)) {
		ret = false;
	}

	torture_assert(tctx, test_samr_handle_Close(b, tctx, &domain_handle), "Failed to close SAMR domain handle");

	torture_assert(tctx, test_Connect(b, tctx, &ctx->handle), "Faile to re-connect SAMR handle");
	/* reconnect the main handle */

	if (!ret) {
		torture_warning(tctx, "Testing domain %s failed!\n", dom_sid_string(tctx, sid));
	}

	return ret;
}

static bool test_LookupDomain(struct dcerpc_pipe *p, struct torture_context *tctx,
			      struct torture_samr_context *ctx, const char *domain)
{
	struct samr_LookupDomain r;
	struct dom_sid2 *sid = NULL;
	struct lsa_String n1;
	struct lsa_String n2;
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;

	torture_comment(tctx, "Testing LookupDomain(%s)\n", domain);

	/* check for correct error codes */
	r.in.connect_handle = &ctx->handle;
	r.in.domain_name = &n2;
	r.out.sid = &sid;
	n2.string = NULL;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_LookupDomain_r(b, tctx, &r),
		"LookupDomain failed");
	torture_assert_ntstatus_equal(tctx, NT_STATUS_INVALID_PARAMETER, r.out.result, "LookupDomain expected NT_STATUS_INVALID_PARAMETER");

	init_lsa_String(&n2, "xxNODOMAINxx");

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_LookupDomain_r(b, tctx, &r),
		"LookupDomain failed");
	torture_assert_ntstatus_equal(tctx, NT_STATUS_NO_SUCH_DOMAIN, r.out.result, "LookupDomain expected NT_STATUS_NO_SUCH_DOMAIN");

	r.in.connect_handle = &ctx->handle;

	init_lsa_String(&n1, domain);
	r.in.domain_name = &n1;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_LookupDomain_r(b, tctx, &r),
		"LookupDomain failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "LookupDomain");

	if (!test_GetDomPwInfo(p, tctx, &n1)) {
		ret = false;
	}

	if (!test_OpenDomain(p, tctx, ctx, *r.out.sid)) {
		ret = false;
	}

	return ret;
}


static bool test_EnumDomains(struct dcerpc_pipe *p, struct torture_context *tctx,
			     struct torture_samr_context *ctx)
{
	struct samr_EnumDomains r;
	uint32_t resume_handle = 0;
	uint32_t num_entries = 0;
	struct samr_SamArray *sam = NULL;
	int i;
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.connect_handle = &ctx->handle;
	r.in.resume_handle = &resume_handle;
	r.in.buf_size = (uint32_t)-1;
	r.out.resume_handle = &resume_handle;
	r.out.num_entries = &num_entries;
	r.out.sam = &sam;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_EnumDomains_r(b, tctx, &r),
		"EnumDomains failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "EnumDomains failed");

	if (!*r.out.sam) {
		return false;
	}

	for (i=0;i<sam->count;i++) {
		if (!test_LookupDomain(p, tctx, ctx,
				       sam->entries[i].name.string)) {
			ret = false;
		}
	}

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_EnumDomains_r(b, tctx, &r),
		"EnumDomains failed");
	torture_assert_ntstatus_ok(tctx, r.out.result, "EnumDomains failed");

	return ret;
}


static bool test_Connect(struct dcerpc_binding_handle *b,
			 struct torture_context *tctx,
			 struct policy_handle *handle)
{
	struct samr_Connect r;
	struct samr_Connect2 r2;
	struct samr_Connect3 r3;
	struct samr_Connect4 r4;
	struct samr_Connect5 r5;
	union samr_ConnectInfo info;
	struct policy_handle h;
	uint32_t level_out = 0;
	bool ret = true, got_handle = false;

	torture_comment(tctx, "Testing samr_Connect\n");

	r.in.system_name = 0;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.connect_handle = &h;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_Connect_r(b, tctx, &r),
		"Connect failed");
	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_comment(tctx, "Connect failed - %s\n", nt_errstr(r.out.result));
		ret = false;
	} else {
		got_handle = true;
		*handle = h;
	}

	torture_comment(tctx, "Testing samr_Connect2\n");

	r2.in.system_name = NULL;
	r2.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r2.out.connect_handle = &h;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_Connect2_r(b, tctx, &r2),
		"Connect2 failed");
	if (!NT_STATUS_IS_OK(r2.out.result)) {
		torture_comment(tctx, "Connect2 failed - %s\n", nt_errstr(r2.out.result));
		ret = false;
	} else {
		if (got_handle) {
			test_samr_handle_Close(b, tctx, handle);
		}
		got_handle = true;
		*handle = h;
	}

	torture_comment(tctx, "Testing samr_Connect3\n");

	r3.in.system_name = NULL;
	r3.in.unknown = 0;
	r3.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r3.out.connect_handle = &h;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_Connect3_r(b, tctx, &r3),
		"Connect3 failed");
	if (!NT_STATUS_IS_OK(r3.out.result)) {
		torture_warning(tctx, "Connect3 failed - %s\n", nt_errstr(r3.out.result));
		ret = false;
	} else {
		if (got_handle) {
			test_samr_handle_Close(b, tctx, handle);
		}
		got_handle = true;
		*handle = h;
	}

	torture_comment(tctx, "Testing samr_Connect4\n");

	r4.in.system_name = "";
	r4.in.client_version = 0;
	r4.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r4.out.connect_handle = &h;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_Connect4_r(b, tctx, &r4),
		"Connect4 failed");
	if (!NT_STATUS_IS_OK(r4.out.result)) {
		torture_warning(tctx, "Connect4 failed - %s\n", nt_errstr(r4.out.result));
		ret = false;
	} else {
		if (got_handle) {
			test_samr_handle_Close(b, tctx, handle);
		}
		got_handle = true;
		*handle = h;
	}

	torture_comment(tctx, "Testing samr_Connect5\n");

	info.info1.client_version = 0;
	info.info1.unknown2 = 0;

	r5.in.system_name = "";
	r5.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r5.in.level_in = 1;
	r5.out.level_out = &level_out;
	r5.in.info_in = &info;
	r5.out.info_out = &info;
	r5.out.connect_handle = &h;

	torture_assert_ntstatus_ok(tctx, dcerpc_samr_Connect5_r(b, tctx, &r5),
		"Connect5 failed");
	if (!NT_STATUS_IS_OK(r5.out.result)) {
		torture_warning(tctx, "Connect5 failed - %s\n", nt_errstr(r5.out.result));
		ret = false;
	} else {
		if (got_handle) {
			test_samr_handle_Close(b, tctx, handle);
		}
		got_handle = true;
		*handle = h;
	}

	return ret;
}


static bool test_samr_ValidatePassword(struct torture_context *tctx,
				       struct dcerpc_pipe *p)
{
	struct samr_ValidatePassword r;
	union samr_ValidatePasswordReq req;
	union samr_ValidatePasswordRep *repp = NULL;
	NTSTATUS status;
	const char *passwords[] = { "penguin", "p@ssw0rd", "p@ssw0rd123$", NULL };
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	torture_comment(tctx, "Testing samr_ValidatePassword\n");

	if (p->conn->transport.transport != NCACN_IP_TCP) {
		torture_comment(tctx, "samr_ValidatePassword only should succeed over NCACN_IP_TCP!\n");
	}

	ZERO_STRUCT(r);
	r.in.level = NetValidatePasswordReset;
	r.in.req = &req;
	r.out.rep = &repp;

	ZERO_STRUCT(req);
	req.req3.account.string = "non-existant-account-aklsdji";

	for (i=0; passwords[i]; i++) {
		req.req3.password.string = passwords[i];

		status = dcerpc_samr_ValidatePassword_r(b, tctx, &r);
		if (NT_STATUS_EQUAL(status, NT_STATUS_RPC_PROCNUM_OUT_OF_RANGE)) {
			torture_skip(tctx, "ValidatePassword not supported by server\n");
		}
		torture_assert_ntstatus_ok(tctx, status,
					   "samr_ValidatePassword failed");
		torture_assert_ntstatus_ok(tctx, r.out.result,
					   "samr_ValidatePassword failed");
		torture_comment(tctx, "Server %s password '%s' with code %i\n",
				repp->ctr3.status==SAMR_VALIDATION_STATUS_SUCCESS?"allowed":"refused",
				req.req3.password.string, repp->ctr3.status);
	}

	return true;
}

bool torture_rpc_samr(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p;
	bool ret = true;
	struct torture_samr_context *ctx;
	struct dcerpc_binding_handle *b;

	status = torture_rpc_connection(torture, &p, &ndr_table_samr);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}
	b = p->binding_handle;

	ctx = talloc_zero(torture, struct torture_samr_context);

	ctx->choice = TORTURE_SAMR_OTHER;

	ret &= test_Connect(b, torture, &ctx->handle);

	if (!torture_setting_bool(torture, "samba3", false)) {
		ret &= test_QuerySecurity(b, torture, &ctx->handle);
	}

	ret &= test_EnumDomains(p, torture, ctx);

	ret &= test_SetDsrmPassword(b, torture, &ctx->handle);

	ret &= test_Shutdown(b, torture, &ctx->handle);

	ret &= test_samr_handle_Close(b, torture, &ctx->handle);

	return ret;
}


bool torture_rpc_samr_users(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p;
	bool ret = true;
	struct torture_samr_context *ctx;
	struct dcerpc_binding_handle *b;

	status = torture_rpc_connection(torture, &p, &ndr_table_samr);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}
	b = p->binding_handle;

	ctx = talloc_zero(torture, struct torture_samr_context);

	ctx->choice = TORTURE_SAMR_USER_ATTRIBUTES;

	ret &= test_Connect(b, torture, &ctx->handle);

	if (!torture_setting_bool(torture, "samba3", false)) {
		ret &= test_QuerySecurity(b, torture, &ctx->handle);
	}

	ret &= test_EnumDomains(p, torture, ctx);

	ret &= test_SetDsrmPassword(b, torture, &ctx->handle);

	ret &= test_Shutdown(b, torture, &ctx->handle);

	ret &= test_samr_handle_Close(b, torture, &ctx->handle);

	return ret;
}


bool torture_rpc_samr_passwords(struct torture_context *torture)
{
	NTSTATUS status;
	struct dcerpc_pipe *p;
	bool ret = true;
	struct torture_samr_context *ctx;
	struct dcerpc_binding_handle *b;

	status = torture_rpc_connection(torture, &p, &ndr_table_samr);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}
	b = p->binding_handle;

	ctx = talloc_zero(torture, struct torture_samr_context);

	ctx->choice = TORTURE_SAMR_PASSWORDS;

	ret &= test_Connect(b, torture, &ctx->handle);

	ret &= test_EnumDomains(p, torture, ctx);

	ret &= test_samr_handle_Close(b, torture, &ctx->handle);

	return ret;
}

static bool torture_rpc_samr_pwdlastset(struct torture_context *torture,
					struct dcerpc_pipe *p2,
					struct cli_credentials *machine_credentials)
{
	NTSTATUS status;
	struct dcerpc_pipe *p;
	bool ret = true;
	struct torture_samr_context *ctx;
	struct dcerpc_binding_handle *b;

	status = torture_rpc_connection(torture, &p, &ndr_table_samr);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}
	b = p->binding_handle;

	ctx = talloc_zero(torture, struct torture_samr_context);

	ctx->choice = TORTURE_SAMR_PASSWORDS_PWDLASTSET;
	ctx->machine_credentials = machine_credentials;

	ret &= test_Connect(b, torture, &ctx->handle);

	ret &= test_EnumDomains(p, torture, ctx);

	ret &= test_samr_handle_Close(b, torture, &ctx->handle);

	return ret;
}

struct torture_suite *torture_rpc_samr_passwords_pwdlastset(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "samr.passwords.pwdlastset");
	struct torture_rpc_tcase *tcase;

	tcase = torture_suite_add_machine_bdc_rpc_iface_tcase(suite, "samr",
							  &ndr_table_samr,
							  TEST_ACCOUNT_NAME_PWD);

	torture_rpc_tcase_add_test_creds(tcase, "pwdLastSet",
					 torture_rpc_samr_pwdlastset);

	return suite;
}

static bool torture_rpc_samr_users_privileges_delete_user(struct torture_context *torture,
							  struct dcerpc_pipe *p2,
							  struct cli_credentials *machine_credentials)
{
	NTSTATUS status;
	struct dcerpc_pipe *p;
	bool ret = true;
	struct torture_samr_context *ctx;
	struct dcerpc_binding_handle *b;

	status = torture_rpc_connection(torture, &p, &ndr_table_samr);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}
	b = p->binding_handle;

	ctx = talloc_zero(torture, struct torture_samr_context);

	ctx->choice = TORTURE_SAMR_USER_PRIVILEGES;
	ctx->machine_credentials = machine_credentials;

	ret &= test_Connect(b, torture, &ctx->handle);

	ret &= test_EnumDomains(p, torture, ctx);

	ret &= test_samr_handle_Close(b, torture, &ctx->handle);

	return ret;
}

struct torture_suite *torture_rpc_samr_user_privileges(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "samr.users.privileges");
	struct torture_rpc_tcase *tcase;

	tcase = torture_suite_add_machine_bdc_rpc_iface_tcase(suite, "samr",
							  &ndr_table_samr,
							  TEST_ACCOUNT_NAME_PWD);

	torture_rpc_tcase_add_test_creds(tcase, "delete_privileged_user",
					 torture_rpc_samr_users_privileges_delete_user);

	return suite;
}

static bool torture_rpc_samr_many_accounts(struct torture_context *torture,
					   struct dcerpc_pipe *p2,
					   void *data)
{
	NTSTATUS status;
	struct dcerpc_pipe *p;
	bool ret = true;
	struct torture_samr_context *ctx =
		talloc_get_type_abort(data, struct torture_samr_context);
	struct dcerpc_binding_handle *b;

	status = torture_rpc_connection(torture, &p, &ndr_table_samr);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}
	b = p->binding_handle;

	ctx->choice = TORTURE_SAMR_MANY_ACCOUNTS;
	ctx->num_objects_large_dc = torture_setting_int(torture, "large_dc",
							ctx->num_objects_large_dc);

	ret &= test_Connect(b, torture, &ctx->handle);

	ret &= test_EnumDomains(p, torture, ctx);

	ret &= test_samr_handle_Close(b, torture, &ctx->handle);

	return ret;
}

static bool torture_rpc_samr_many_groups(struct torture_context *torture,
					 struct dcerpc_pipe *p2,
					 void *data)
{
	NTSTATUS status;
	struct dcerpc_pipe *p;
	bool ret = true;
	struct torture_samr_context *ctx =
		talloc_get_type_abort(data, struct torture_samr_context);
	struct dcerpc_binding_handle *b;

	status = torture_rpc_connection(torture, &p, &ndr_table_samr);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}
	b = p->binding_handle;

	ctx->choice = TORTURE_SAMR_MANY_GROUPS;
	ctx->num_objects_large_dc = torture_setting_int(torture, "large_dc",
							ctx->num_objects_large_dc);

	ret &= test_Connect(b, torture, &ctx->handle);

	ret &= test_EnumDomains(p, torture, ctx);

	ret &= test_samr_handle_Close(b, torture, &ctx->handle);

	return ret;
}

static bool torture_rpc_samr_many_aliases(struct torture_context *torture,
					  struct dcerpc_pipe *p2,
					  void *data)
{
	NTSTATUS status;
	struct dcerpc_pipe *p;
	bool ret = true;
	struct torture_samr_context *ctx =
		talloc_get_type_abort(data, struct torture_samr_context);
	struct dcerpc_binding_handle *b;

	status = torture_rpc_connection(torture, &p, &ndr_table_samr);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}
	b = p->binding_handle;

	ctx->choice = TORTURE_SAMR_MANY_ALIASES;
	ctx->num_objects_large_dc = torture_setting_int(torture, "large_dc",
							ctx->num_objects_large_dc);

	ret &= test_Connect(b, torture, &ctx->handle);

	ret &= test_EnumDomains(p, torture, ctx);

	ret &= test_samr_handle_Close(b, torture, &ctx->handle);

	return ret;
}

struct torture_suite *torture_rpc_samr_large_dc(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "samr.large-dc");
	struct torture_rpc_tcase *tcase;
	struct torture_samr_context *ctx;

	tcase = torture_suite_add_rpc_iface_tcase(suite, "samr", &ndr_table_samr);

	ctx = talloc_zero(suite, struct torture_samr_context);
	ctx->num_objects_large_dc = 150;

	torture_rpc_tcase_add_test_ex(tcase, "many_aliases",
				      torture_rpc_samr_many_aliases, ctx);
	torture_rpc_tcase_add_test_ex(tcase, "many_groups",
				      torture_rpc_samr_many_groups, ctx);
	torture_rpc_tcase_add_test_ex(tcase, "many_accounts",
				      torture_rpc_samr_many_accounts, ctx);

	return suite;
}

static bool torture_rpc_samr_badpwdcount(struct torture_context *torture,
					 struct dcerpc_pipe *p2,
					 struct cli_credentials *machine_credentials)
{
	NTSTATUS status;
	struct dcerpc_pipe *p;
	bool ret = true;
	struct torture_samr_context *ctx;
	struct dcerpc_binding_handle *b;

	status = torture_rpc_connection(torture, &p, &ndr_table_samr);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}
	b = p->binding_handle;

	ctx = talloc_zero(torture, struct torture_samr_context);

	ctx->choice = TORTURE_SAMR_PASSWORDS_BADPWDCOUNT;
	ctx->machine_credentials = machine_credentials;

	ret &= test_Connect(b, torture, &ctx->handle);

	ret &= test_EnumDomains(p, torture, ctx);

	ret &= test_samr_handle_Close(b, torture, &ctx->handle);

	return ret;
}

struct torture_suite *torture_rpc_samr_passwords_badpwdcount(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "samr.passwords.badpwdcount");
	struct torture_rpc_tcase *tcase;

	tcase = torture_suite_add_machine_bdc_rpc_iface_tcase(suite, "samr",
							  &ndr_table_samr,
							  TEST_ACCOUNT_NAME_PWD);

	torture_rpc_tcase_add_test_creds(tcase, "badPwdCount",
					 torture_rpc_samr_badpwdcount);

	return suite;
}

static bool torture_rpc_samr_lockout(struct torture_context *torture,
				     struct dcerpc_pipe *p2,
				     struct cli_credentials *machine_credentials)
{
	NTSTATUS status;
	struct dcerpc_pipe *p;
	bool ret = true;
	struct torture_samr_context *ctx;
	struct dcerpc_binding_handle *b;

	status = torture_rpc_connection(torture, &p, &ndr_table_samr);
	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}
	b = p->binding_handle;

	ctx = talloc_zero(torture, struct torture_samr_context);

	ctx->choice = TORTURE_SAMR_PASSWORDS_LOCKOUT;
	ctx->machine_credentials = machine_credentials;

	ret &= test_Connect(b, torture, &ctx->handle);

	ret &= test_EnumDomains(p, torture, ctx);

	ret &= test_samr_handle_Close(b, torture, &ctx->handle);

	return ret;
}

struct torture_suite *torture_rpc_samr_passwords_lockout(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "samr.passwords.lockout");
	struct torture_rpc_tcase *tcase;

	tcase = torture_suite_add_machine_bdc_rpc_iface_tcase(suite, "samr",
							  &ndr_table_samr,
							  TEST_ACCOUNT_NAME_PWD);

	torture_rpc_tcase_add_test_creds(tcase, "lockout",
					 torture_rpc_samr_lockout);

	return suite;
}

struct torture_suite *torture_rpc_samr_passwords_validate(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "samr.passwords.validate");
	struct torture_rpc_tcase *tcase;

	tcase = torture_suite_add_rpc_iface_tcase(suite, "samr",
						  &ndr_table_samr);
	torture_rpc_tcase_add_test(tcase, "validate",
				   test_samr_ValidatePassword);

	return suite;
}
