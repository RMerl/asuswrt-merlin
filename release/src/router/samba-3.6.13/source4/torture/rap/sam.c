/*
   Unix SMB/CIFS implementation.
   test suite for RAP sam operations

   Copyright (C) Guenther Deschner 2010-2011

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
#include "libcli/libcli.h"
#include "torture/torture.h"
#include "torture/util.h"
#include "torture/smbtorture.h"
#include "torture/util.h"
#include "libcli/rap/rap.h"
#include "torture/rap/proto.h"
#include "../lib/crypto/crypto.h"
#include "../libcli/auth/libcli_auth.h"
#include "torture/rpc/torture_rpc.h"

#define TEST_RAP_USER "torture_rap_user"

static char *samr_rand_pass(TALLOC_CTX *mem_ctx, int min_len)
{
	size_t len = MAX(8, min_len);
	char *s = generate_random_password(mem_ctx, len, len+6);
	printf("Generated password '%s'\n", s);
	return s;
}

static bool test_userpasswordset2_args(struct torture_context *tctx,
				       struct smbcli_state *cli,
				       const char *username,
				       const char **password)
{
	struct rap_NetUserPasswordSet2 r;
	char *newpass = samr_rand_pass(tctx, 8);

	ZERO_STRUCT(r);

	r.in.UserName = username;

	memcpy(r.in.OldPassword, *password, MIN(strlen(*password), 16));
	memcpy(r.in.NewPassword, newpass, MIN(strlen(newpass), 16));
	r.in.EncryptedPassword = 0;
	r.in.RealPasswordLength = strlen(newpass);

	torture_comment(tctx, "Testing rap_NetUserPasswordSet2(%s)\n", r.in.UserName);

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netuserpasswordset2(cli->tree, tctx, &r),
		"smbcli_rap_netuserpasswordset2 failed");
	if (!W_ERROR_IS_OK(W_ERROR(r.out.status))) {
		torture_warning(tctx, "RAP NetUserPasswordSet2 gave: %s\n",
			win_errstr(W_ERROR(r.out.status)));
	} else {
		*password = newpass;
	}

	return true;
}

static bool test_userpasswordset2_crypt_args(struct torture_context *tctx,
					     struct smbcli_state *cli,
					     const char *username,
					     const char **password)
{
	struct rap_NetUserPasswordSet2 r;
	char *newpass = samr_rand_pass(tctx, 8);

	r.in.UserName = username;

	E_deshash(*password, r.in.OldPassword);
	E_deshash(newpass, r.in.NewPassword);

	r.in.RealPasswordLength = strlen(newpass);
	r.in.EncryptedPassword = 1;

	torture_comment(tctx, "Testing rap_NetUserPasswordSet2(%s)\n", r.in.UserName);

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netuserpasswordset2(cli->tree, tctx, &r),
		"smbcli_rap_netuserpasswordset2 failed");
	if (!W_ERROR_IS_OK(W_ERROR(r.out.status))) {
		torture_warning(tctx, "RAP NetUserPasswordSet2 gave: %s\n",
			win_errstr(W_ERROR(r.out.status)));
	} else {
		*password = newpass;
	}

	return true;
}

static bool test_userpasswordset2(struct torture_context *tctx,
				  struct smbcli_state *cli)
{
	struct test_join *join_ctx;
	const char *password;
	bool ret = true;

	join_ctx = torture_create_testuser_max_pwlen(tctx, TEST_RAP_USER,
						     torture_setting_string(tctx, "workgroup", NULL),
						     ACB_NORMAL,
						     &password, 14);
	if (join_ctx == NULL) {
		torture_fail(tctx, "failed to create user\n");
	}

	ret &= test_userpasswordset2_args(tctx, cli, TEST_RAP_USER, &password);
	ret &= test_userpasswordset2_crypt_args(tctx, cli, TEST_RAP_USER, &password);

	torture_leave_domain(tctx, join_ctx);

	return ret;
}

static bool test_oemchangepassword_args(struct torture_context *tctx,
					struct smbcli_state *cli,
					const char *username,
					const char **password)
{
	struct rap_NetOEMChangePassword r;

	const char *oldpass = *password;
	char *newpass = samr_rand_pass(tctx, 9);
	uint8_t old_pw_hash[16];
	uint8_t new_pw_hash[16];

	r.in.UserName = username;

	E_deshash(oldpass, old_pw_hash);
	E_deshash(newpass, new_pw_hash);

	encode_pw_buffer(r.in.crypt_password, newpass, STR_ASCII);
	arcfour_crypt(r.in.crypt_password, old_pw_hash, 516);
	E_old_pw_hash(new_pw_hash, old_pw_hash, r.in.password_hash);

	torture_comment(tctx, "Testing rap_NetOEMChangePassword(%s)\n", r.in.UserName);

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netoemchangepassword(cli->tree, tctx, &r),
		"smbcli_rap_netoemchangepassword failed");
	if (!W_ERROR_IS_OK(W_ERROR(r.out.status))) {
		torture_warning(tctx, "RAP NetOEMChangePassword gave: %s\n",
			win_errstr(W_ERROR(r.out.status)));
	} else {
		*password = newpass;
	}

	return true;
}

static bool test_oemchangepassword(struct torture_context *tctx,
				   struct smbcli_state *cli)
{

	struct test_join *join_ctx;
	const char *password;
	bool ret;

	join_ctx = torture_create_testuser_max_pwlen(tctx, TEST_RAP_USER,
						     torture_setting_string(tctx, "workgroup", NULL),
						     ACB_NORMAL,
						     &password, 14);
	if (join_ctx == NULL) {
		torture_fail(tctx, "failed to create user\n");
	}

	ret = test_oemchangepassword_args(tctx, cli, TEST_RAP_USER, &password);

	torture_leave_domain(tctx, join_ctx);

	return ret;
}

static bool test_usergetinfo_byname(struct torture_context *tctx,
				    struct smbcli_state *cli,
				    const char *UserName)
{
	struct rap_NetUserGetInfo r;
	int i;
	uint16_t levels[] = { 0, 1, 2, 10, 11 };

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		r.in.UserName = UserName;
		r.in.level = levels[i];
		r.in.bufsize = 8192;

		torture_comment(tctx,
			"Testing rap_NetUserGetInfo(%s) level %d\n", r.in.UserName, r.in.level);

		torture_assert_ntstatus_ok(tctx,
			smbcli_rap_netusergetinfo(cli->tree, tctx, &r),
			"smbcli_rap_netusergetinfo failed");
		torture_assert_werr_ok(tctx, W_ERROR(r.out.status),
			"smbcli_rap_netusergetinfo failed");
	}

	return true;
}

static bool test_usergetinfo(struct torture_context *tctx,
			     struct smbcli_state *cli)
{

	struct test_join *join_ctx;
	const char *password;
	bool ret;

	join_ctx = torture_create_testuser_max_pwlen(tctx, TEST_RAP_USER,
						     torture_setting_string(tctx, "workgroup", NULL),
						     ACB_NORMAL,
						     &password, 14);
	if (join_ctx == NULL) {
		torture_fail(tctx, "failed to create user\n");
	}

	ret = test_usergetinfo_byname(tctx, cli, TEST_RAP_USER);

	torture_leave_domain(tctx, join_ctx);

	return ret;
}

static bool test_useradd(struct torture_context *tctx,
			 struct smbcli_state *cli)
{

	struct rap_NetUserAdd r;
	struct rap_NetUserInfo1 info1;
	int i;
	uint16_t levels[] = { 1 };
	const char *username = TEST_RAP_USER;

	for (i=0; i < ARRAY_SIZE(levels); i++) {

		const char *pwd;

		pwd = generate_random_password(tctx, 9, 16);

		r.in.level = levels[i];
		r.in.bufsize = 0xffff;
		r.in.pwdlength = strlen(pwd);
		r.in.unknown = 0;

		switch (r.in.level) {
		case 1:
			ZERO_STRUCT(info1);

			info1.Name = username;
			memcpy(info1.Password, pwd, MIN(strlen(pwd), 16));
			info1.Priv = USER_PRIV_USER;
			info1.Flags = 0x21;
			info1.HomeDir = "home_dir";
			info1.Comment = "comment";
			info1.ScriptPath = "logon_script";

			r.in.info.info1 = info1;
			break;
		}

		torture_comment(tctx,
			"Testing rap_NetUserAdd(%s) level %d\n", username, r.in.level);

		torture_assert_ntstatus_ok(tctx,
			smbcli_rap_netuseradd(cli->tree, tctx, &r),
			"smbcli_rap_netuseradd failed");
		torture_assert_werr_ok(tctx, W_ERROR(r.out.status),
			"smbcli_rap_netuseradd failed");

		torture_assert_ntstatus_ok(tctx,
			smbcli_rap_netuseradd(cli->tree, tctx, &r),
			"2nd smbcli_rap_netuseradd failed");
		torture_assert_werr_equal(tctx, W_ERROR(r.out.status), WERR_USEREXISTS,
			"2nd smbcli_rap_netuseradd failed");

		{
			struct rap_NetUserDelete d;

			d.in.UserName = username;

			smbcli_rap_netuserdelete(cli->tree, tctx, &d);
		}
	}

	return true;
}

static bool test_userdelete(struct torture_context *tctx,
			    struct smbcli_state *cli)
{

	struct rap_NetUserDelete r;

	{
		struct rap_NetUserAdd a;
		const char *pwd;

		ZERO_STRUCT(a.in.info.info1);

		pwd = generate_random_password(tctx, 9, 16);

		a.in.level = 1;
		a.in.bufsize = 0xffff;
		a.in.pwdlength = strlen(pwd);
		a.in.unknown = 0;
		a.in.info.info1.Name = TEST_RAP_USER;
		a.in.info.info1.Priv = USER_PRIV_USER;

		memcpy(a.in.info.info1.Password, pwd, MIN(strlen(pwd), 16));

		torture_assert_ntstatus_ok(tctx,
			smbcli_rap_netuseradd(cli->tree, tctx, &a),
			"smbcli_rap_netuseradd failed");
	}

	r.in.UserName = TEST_RAP_USER;

	torture_comment(tctx,
		"Testing rap_NetUserDelete(%s)\n", r.in.UserName);

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netuserdelete(cli->tree, tctx, &r),
		"smbcli_rap_netuserdelete failed");
	torture_assert_werr_ok(tctx, W_ERROR(r.out.status),
		"smbcli_rap_netuserdelete failed");

	torture_assert_ntstatus_ok(tctx,
		smbcli_rap_netuserdelete(cli->tree, tctx, &r),
		"2nd smbcli_rap_netuserdelete failed");
	torture_assert_werr_equal(tctx, W_ERROR(r.out.status), WERR_USER_NOT_FOUND,
		"2nd smbcli_rap_netuserdelete failed");

	return true;
}

struct torture_suite *torture_rap_sam(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "sam");

	torture_suite_add_1smb_test(suite, "userpasswordset2", test_userpasswordset2);
	torture_suite_add_1smb_test(suite, "oemchangepassword", test_oemchangepassword);
	torture_suite_add_1smb_test(suite, "usergetinfo", test_usergetinfo);
	torture_suite_add_1smb_test(suite, "useradd", test_useradd);
	torture_suite_add_1smb_test(suite, "userdelete", test_userdelete);

	return suite;
}
