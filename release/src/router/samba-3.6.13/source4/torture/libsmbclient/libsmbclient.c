/*
   Unix SMB/CIFS implementation.
   SMB torture tester
   Copyright (C) Guenther Deschner 2010

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
#include "auth/credentials/credentials.h"
#include "lib/cmdline/popt_common.h"
#include <libsmbclient.h>
#include "torture/libsmbclient/proto.h"

bool torture_libsmbclient_init_context(struct torture_context *tctx,
				       SMBCCTX **ctx_p)
{
	SMBCCTX *ctx;

	ctx = smbc_new_context();
	torture_assert(tctx, ctx, "failed to get new context");
	torture_assert(tctx, smbc_init_context(ctx), "failed to init context");

	smbc_setDebug(ctx, DEBUGLEVEL);
	smbc_setOptionDebugToStderr(ctx, 1);

	/* yes, libsmbclient API frees the username when freeing the context, so
	 * have to pass malloced data here */
	smbc_setUser(ctx, strdup(cli_credentials_get_username(cmdline_credentials)));

	*ctx_p = ctx;

	return true;
}

static bool torture_libsmbclient_version(struct torture_context *tctx)
{
	torture_comment(tctx, "Testing smbc_version\n");

	torture_assert(tctx, smbc_version(), "failed to get version");

	return true;
}

static bool torture_libsmbclient_initialize(struct torture_context *tctx)
{
	SMBCCTX *ctx;

	torture_comment(tctx, "Testing smbc_new_context\n");

	ctx = smbc_new_context();
	torture_assert(tctx, ctx, "failed to get new context");

	torture_comment(tctx, "Testing smbc_init_context\n");

	torture_assert(tctx, smbc_init_context(ctx), "failed to init context");

	smbc_free_context(ctx, 1);

	return true;
}

static bool test_opendir(struct torture_context *tctx,
			 SMBCCTX *ctx,
			 const char *fname,
			 bool expect_success)
{
	int handle, ret;

	torture_comment(tctx, "Testing smbc_opendir(%s)\n", fname);

	handle = smbc_opendir(fname);
	if (!expect_success) {
		return true;
	}
	if (handle < 0) {
		torture_fail(tctx, talloc_asprintf(tctx, "failed to obain file handle for '%s'", fname));
	}

	ret = smbc_closedir(handle);
	torture_assert_int_equal(tctx, ret, 0,
		talloc_asprintf(tctx, "failed to close file handle for '%s'", fname));

	return true;
}

static bool torture_libsmbclient_opendir(struct torture_context *tctx)
{
	int i;
	SMBCCTX *ctx;
	bool ret = true;
	const char *bad_urls[] = {
		"",
		NULL,
		"smb",
		"smb:",
		"smb:/",
		"smb:///",
		"bms://",
		":",
		":/",
		"://",
		":///",
		"/",
		"//",
		"///"
	};
	const char *good_urls[] = {
		"smb://",
		"smb://WORKGROUP",
		"smb://WORKGROUP/"
	};

	torture_assert(tctx, torture_libsmbclient_init_context(tctx, &ctx), "");
	smbc_set_context(ctx);

	for (i=0; i < ARRAY_SIZE(bad_urls); i++) {
		ret &= test_opendir(tctx, ctx, bad_urls[i], false);
	}
	for (i=0; i < ARRAY_SIZE(good_urls); i++) {
		ret &= test_opendir(tctx, ctx, good_urls[i], true);
	}

	smbc_free_context(ctx, 1);

	return ret;
}

/* note the strdup for string options on smbc_set calls. I think libsmbclient is
 * really doing something wrong here: in smbc_free_context libsmbclient just
 * calls free() on the string options so it assumes the callers have malloced
 * them before setting them via smbc_set calls. */

#define TEST_OPTION_INT(option, val) \
	torture_comment(tctx, "Testing smbc_set" #option "\n");\
	smbc_set ##option(ctx, val);\
	torture_comment(tctx, "Testing smbc_get" #option "\n");\
	torture_assert_int_equal(tctx, smbc_get ##option(ctx), val, "failed " #option);

#define TEST_OPTION_STRING(option, val) \
	torture_comment(tctx, "Testing smbc_set" #option "\n");\
	smbc_set ##option(ctx, strdup(val));\
	torture_comment(tctx, "Testing smbc_get" #option "\n");\
	torture_assert_str_equal(tctx, smbc_get ##option(ctx), val, "failed " #option);

bool torture_libsmbclient_configuration(struct torture_context *tctx)
{
	SMBCCTX *ctx;

	ctx = smbc_new_context();
	torture_assert(tctx, ctx, "failed to get new context");
	torture_assert(tctx, smbc_init_context(ctx), "failed to init context");

	TEST_OPTION_INT(Debug, DEBUGLEVEL);
	TEST_OPTION_STRING(NetbiosName, "torture_netbios");
	TEST_OPTION_STRING(Workgroup, "torture_workgroup");
	TEST_OPTION_STRING(User, "torture_user");
	TEST_OPTION_INT(Timeout, 12345);

	smbc_free_context(ctx, 1);

	return true;
}

bool torture_libsmbclient_options(struct torture_context *tctx)
{
	SMBCCTX *ctx;

	ctx = smbc_new_context();
	torture_assert(tctx, ctx, "failed to get new context");
	torture_assert(tctx, smbc_init_context(ctx), "failed to init context");

	TEST_OPTION_INT(OptionDebugToStderr, true);
	TEST_OPTION_INT(OptionFullTimeNames, true);
	TEST_OPTION_INT(OptionOpenShareMode, SMBC_SHAREMODE_DENY_ALL);
	/* FIXME: OptionUserData */
	TEST_OPTION_INT(OptionSmbEncryptionLevel, SMBC_ENCRYPTLEVEL_REQUEST);
	TEST_OPTION_INT(OptionCaseSensitive, false);
	TEST_OPTION_INT(OptionBrowseMaxLmbCount, 2);
	TEST_OPTION_INT(OptionUrlEncodeReaddirEntries, true);
	TEST_OPTION_INT(OptionOneSharePerServer, true);
	TEST_OPTION_INT(OptionUseKerberos, false);
	TEST_OPTION_INT(OptionFallbackAfterKerberos, false);
	TEST_OPTION_INT(OptionNoAutoAnonymousLogin, true);
	TEST_OPTION_INT(OptionUseCCache, true);

	smbc_free_context(ctx, 1);

	return true;
}

NTSTATUS torture_libsmbclient_init(void)
{
	struct torture_suite *suite;

	suite = torture_suite_create(talloc_autofree_context(), "libsmbclient");

	torture_suite_add_simple_test(suite, "version", torture_libsmbclient_version);
	torture_suite_add_simple_test(suite, "initialize", torture_libsmbclient_initialize);
	torture_suite_add_simple_test(suite, "configuration", torture_libsmbclient_configuration);
	torture_suite_add_simple_test(suite, "options", torture_libsmbclient_options);
	torture_suite_add_simple_test(suite, "opendir", torture_libsmbclient_opendir);

	suite->description = talloc_strdup(suite, "libsmbclient interface tests");

	torture_register_suite(suite);

	return NT_STATUS_OK;
}
