/*
   Unix SMB/CIFS implementation.

   local testing of registry diff functionality

   Copyright (C) Jelmer Vernooij 2007
   Copyright (C) Wilco Baan Hofman 2008

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
#include "lib/registry/registry.h"
#include "torture/torture.h"
#include "librpc/gen_ndr/winreg.h"
#include "param/param.h"

struct diff_tcase_data {
	struct registry_context *r1_ctx;
	struct registry_context *r2_ctx;
	struct reg_diff_callbacks *callbacks;
	void *callback_data;
	char *tempdir;
	char *filename;
};

static bool test_generate_diff(struct torture_context *tctx, void *tcase_data)
{
	WERROR error;
	struct diff_tcase_data *td = tcase_data;

	error = reg_generate_diff(td->r1_ctx, td->r2_ctx, 
			td->callbacks,
			td->callback_data);
	torture_assert_werr_ok(tctx, error, "reg_generate_diff");

	return true;
}

#if 0
static bool test_diff_load(struct torture_context *tctx, void *tcase_data)
{
	struct diff_tcase_data *td = tcase_data;
	struct smb_iconv_convenience *ic;
	struct reg_diff_callbacks *callbacks;
	void *data;
	WERROR error;

	ic = lp_iconv_convenience(tctx->lp_ctx);

	error = reg_diff_load(td->filename, iconv_convenience, callbacks, data);
	torture_assert_werr_ok(tctx, error, "reg_diff_load");

	return true;
}
#endif
static bool test_diff_apply(struct torture_context *tctx, void *tcase_data)
{
	struct diff_tcase_data *td = tcase_data;
	struct registry_key *key;
	WERROR error;

	error = reg_diff_apply(td->r1_ctx, lp_iconv_convenience(tctx->lp_ctx), td->filename);
	torture_assert_werr_ok(tctx, error, "reg_diff_apply");

	error = td->r1_ctx->ops->get_predefined_key(td->r1_ctx, HKEY_LOCAL_MACHINE, &key);
	torture_assert_werr_ok(tctx, error, "Opening HKEY_LOCAL_MACHINE failed");

	/* If this generates an error it could be that the apply doesn't work,
	 * but also that the reg_generate_diff didn't work. */
	error = td->r1_ctx->ops->open_key(td->r1_ctx, key, "Software", &key);
	torture_assert_werr_ok(tctx, error, "Opening HKLM\\Software failed");
	error = td->r1_ctx->ops->open_key(td->r1_ctx, key, "Microsoft", &key);
	torture_assert_werr_ok(tctx, error, "Opening HKLM\\Software\\Microsoft failed");
	error = td->r1_ctx->ops->open_key(td->r1_ctx, key, "Windows", &key);
	torture_assert_werr_ok(tctx, error, "Opening HKLM\\..\\Microsoft\\Windows failed");
	error = td->r1_ctx->ops->open_key(td->r1_ctx, key, "CurrentVersion", &key);
	torture_assert_werr_ok(tctx, error, "Opening HKLM\\..\\Windows\\CurrentVersion failed");
	error = td->r1_ctx->ops->open_key(td->r1_ctx, key, "Policies", &key);
	torture_assert_werr_ok(tctx, error, "Opening HKLM\\..\\CurrentVersion\\Policies failed");
	error = td->r1_ctx->ops->open_key(td->r1_ctx, key, "Explorer", &key);
	torture_assert_werr_ok(tctx, error, "Opening HKLM\\..\\Policies\\Explorer failed");

	return true;
}

static const char *added_key = NULL;

static WERROR test_add_key(void *callback_data, const char *key_name)
{
	added_key = talloc_strdup(callback_data, key_name);

	return WERR_OK;
}

static bool test_generate_diff_key_add(struct torture_context *tctx, void *tcase_data)
{
	struct reg_diff_callbacks cb;
	struct registry_key rk;

	return true;

	ZERO_STRUCT(cb);

	cb.add_key = test_add_key;

	if (W_ERROR_IS_OK(reg_generate_diff_key(&rk, NULL, "bla", &cb, tctx)))
		return false;

	torture_assert_str_equal(tctx, added_key, "bla", "key added");

	return true;
}

static bool test_generate_diff_key_null(struct torture_context *tctx, void *tcase_data)
{
	struct reg_diff_callbacks cb;

	ZERO_STRUCT(cb);

	if (!W_ERROR_IS_OK(reg_generate_diff_key(NULL, NULL, "", &cb, NULL)))
		return false;

	return true;
}

static void tcase_add_tests (struct torture_tcase *tcase) 
{
	torture_tcase_add_simple_test(tcase, "test_generate_diff_key_add",
			test_generate_diff_key_add);
	torture_tcase_add_simple_test(tcase, "test_generate_diff_key_null",
			test_generate_diff_key_null);
	torture_tcase_add_simple_test(tcase, "test_generate_diff",
			test_generate_diff);
	torture_tcase_add_simple_test(tcase, "test_diff_apply",
			test_diff_apply);
/*	torture_tcase_add_simple_test(tcase, "test_diff_load",
			test_diff_load);
*/
}

static bool diff_setup_tcase(struct torture_context *tctx, void **data)
{
	struct registry_context *r1_ctx, *r2_ctx;
	WERROR error;
	NTSTATUS status;
	struct hive_key *r1_hklm, *r1_hkcu;
	struct hive_key *r2_hklm, *r2_hkcu;
	const char *filename;
	struct diff_tcase_data *td;
	struct registry_key *key, *newkey;
	DATA_BLOB blob;

	td = talloc(tctx, struct diff_tcase_data);

	/* Create two registry contexts */
	error = reg_open_local(tctx, &r1_ctx);
	torture_assert_werr_ok(tctx, error, "Opening registry 1 for patch tests failed");
	
	error = reg_open_local(tctx, &r2_ctx);
	torture_assert_werr_ok(tctx, error, "Opening registry 2 for patch tests failed");

	/* Create temp directory */
	status = torture_temp_dir(tctx, "patchfile", &td->tempdir);
	torture_assert_ntstatus_ok(tctx, status, "Creating temp dir failed");

	/* Create and mount HKLM and HKCU hives for registry 1 */
	filename = talloc_asprintf(tctx, "%s/r1_local_machine.ldb", td->tempdir);
	error = reg_open_ldb_file(tctx, filename, NULL, NULL, tctx->ev, tctx->lp_ctx, &r1_hklm);
	torture_assert_werr_ok(tctx, error, "Opening local machine file failed");

	error = reg_mount_hive(r1_ctx, r1_hklm, HKEY_LOCAL_MACHINE, NULL);
	torture_assert_werr_ok(tctx, error, "Mounting hive failed");
	
	filename = talloc_asprintf(tctx, "%s/r1_current_user.ldb", td->tempdir);
	error = reg_open_ldb_file(tctx, filename, NULL, NULL, tctx->ev, tctx->lp_ctx, &r1_hkcu);
	torture_assert_werr_ok(tctx, error, "Opening current user file failed");

	error = reg_mount_hive(r1_ctx, r1_hkcu, HKEY_CURRENT_USER, NULL);
	torture_assert_werr_ok(tctx, error, "Mounting hive failed");
	
	/* Create and mount HKLM and HKCU hives for registry 2 */
	filename = talloc_asprintf(tctx, "%s/r2_local_machine.ldb", td->tempdir);
	error = reg_open_ldb_file(tctx, filename, NULL, NULL, tctx->ev, tctx->lp_ctx, &r2_hklm);
	torture_assert_werr_ok(tctx, error, "Opening local machine file failed");

	error = reg_mount_hive(r2_ctx, r2_hklm, HKEY_LOCAL_MACHINE, NULL);
	torture_assert_werr_ok(tctx, error, "Mounting hive failed");
	
	filename = talloc_asprintf(tctx, "%s/r2_current_user.ldb", td->tempdir);
	error = reg_open_ldb_file(tctx, filename, NULL, NULL, tctx->ev, tctx->lp_ctx, &r2_hkcu);
	torture_assert_werr_ok(tctx, error, "Opening current user file failed");
	
	error = reg_mount_hive(r2_ctx, r2_hkcu, HKEY_CURRENT_USER, NULL);
	torture_assert_werr_ok(tctx, error, "Mounting hive failed");

	error = r1_ctx->ops->get_predefined_key(r1_ctx, HKEY_CURRENT_USER, &key);
	torture_assert_werr_ok(tctx, error, "Opening HKEY_CURRENT_USER failed");
	error = r1_ctx->ops->create_key(r1_ctx, key, "Network", NULL, NULL, &newkey);
	torture_assert_werr_ok(tctx, error, "Opening HKCU\\Network failed");
	error = r1_ctx->ops->create_key(r1_ctx, newkey, "L", NULL, NULL, &newkey);
	torture_assert_werr_ok(tctx, error, "Opening HKCU\\Network\\L failed");

	error = r2_ctx->ops->get_predefined_key(r2_ctx, HKEY_LOCAL_MACHINE, &key);
	torture_assert_werr_ok(tctx, error, "Opening HKEY_LOCAL_MACHINE failed");
	error = r2_ctx->ops->create_key(r2_ctx, key, "Software", NULL, NULL, &newkey);
	torture_assert_werr_ok(tctx, error, "Creating HKLM\\Sofware failed");
	error = r2_ctx->ops->create_key(r2_ctx, newkey, "Microsoft", NULL, NULL, &newkey);
	torture_assert_werr_ok(tctx, error, "Creating HKLM\\Software\\Microsoft failed");
	error = r2_ctx->ops->create_key(r2_ctx, newkey, "Windows", NULL, NULL, &newkey);
	torture_assert_werr_ok(tctx, error, "Creating HKLM\\Software\\Microsoft\\Windows failed");
	error = r2_ctx->ops->create_key(r2_ctx, newkey, "CurrentVersion", NULL, NULL, &newkey);
	torture_assert_werr_ok(tctx, error, "Creating HKLM\\..\\Windows\\CurrentVersion failed");
	error = r2_ctx->ops->create_key(r2_ctx, newkey, "Policies", NULL, NULL, &newkey);
	torture_assert_werr_ok(tctx, error, "Creating HKLM\\..\\CurrentVersion\\Policies failed");
	error = r2_ctx->ops->create_key(r2_ctx, newkey, "Explorer", NULL, NULL, &newkey);
	torture_assert_werr_ok(tctx, error, "Creating HKLM\\..\\Policies\\Explorer failed");


	blob.data = (void *)talloc(r2_ctx, uint32_t);
	SIVAL(blob.data, 0, 0x03ffffff);
	blob.length = sizeof(uint32_t);

	r1_ctx->ops->set_value(newkey, "NoDrives", REG_DWORD, blob);

	/* Set test case data */
	td->r1_ctx = r1_ctx;
	td->r2_ctx = r2_ctx;

	*data = td;

	return true;
}

static bool diff_setup_preg_tcase (struct torture_context *tctx, void **data)
{
	struct diff_tcase_data *td;
	struct smb_iconv_convenience *ic;
	WERROR error;

	diff_setup_tcase(tctx, data);
	td = *data;

	ic = lp_iconv_convenience(tctx->lp_ctx);

	td->filename = talloc_asprintf(tctx, "%s/test.pol", td->tempdir);
	error = reg_preg_diff_save(tctx, td->filename, ic, &td->callbacks, &td->callback_data);
	torture_assert_werr_ok(tctx, error, "reg_preg_diff_save");

	return true;
}

static bool diff_setup_dotreg_tcase (struct torture_context *tctx, void **data)
{
	struct diff_tcase_data *td;
	struct smb_iconv_convenience *ic;
	WERROR error;

	diff_setup_tcase(tctx, data);
	td = *data;

	ic = lp_iconv_convenience(tctx->lp_ctx);
	
	td->filename = talloc_asprintf(tctx, "%s/test.reg", td->tempdir);
	error = reg_dotreg_diff_save(tctx, td->filename, ic, &td->callbacks, &td->callback_data);
	torture_assert_werr_ok(tctx, error, "reg_dotreg_diff_save");

	return true;
}

struct torture_suite *torture_registry_diff(TALLOC_CTX *mem_ctx)
{
	struct torture_tcase *tcase;
	struct torture_suite *suite = torture_suite_create(mem_ctx, "DIFF");

	tcase = torture_suite_add_tcase(suite, "PReg");
	torture_tcase_set_fixture(tcase, diff_setup_preg_tcase, NULL);
	tcase_add_tests(tcase);

	tcase = torture_suite_add_tcase(suite, "dotreg");
	torture_tcase_set_fixture(tcase, diff_setup_dotreg_tcase, NULL);
	tcase_add_tests(tcase);

	return suite;
}
