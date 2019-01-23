/*
   Unix SMB/CIFS implementation.

   local testing of registry library - registry backend

   Copyright (C) Jelmer Vernooij 2005-2007

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"
#include "lib/registry/registry.h"
#include "torture/torture.h"
#include "librpc/gen_ndr/winreg.h"
#include "libcli/security/security.h"
#include "system/filesys.h"

/**
 * Test obtaining a predefined key.
 */
static bool test_get_predefined(struct torture_context *tctx, void *_data)
{
	struct registry_context *rctx = (struct registry_context *)_data;
	struct registry_key *root;
	WERROR error;

	error = reg_get_predefined_key(rctx, HKEY_CLASSES_ROOT, &root);
	torture_assert_werr_ok(tctx, error,
			       "getting predefined key failed");
	return true;
}

/**
 * Test obtaining a predefined key.
 */
static bool test_get_predefined_unknown(struct torture_context *tctx,
		void *_data)
{
	struct registry_context *rctx = _data;
	struct registry_key *root;
	WERROR error;

	error = reg_get_predefined_key(rctx, 1337, &root);
	torture_assert_werr_equal(tctx, error, WERR_BADFILE,
				  "getting predefined key failed");
	return true;
}

static bool test_predef_key_by_name(struct torture_context *tctx, void *_data)
{
	struct registry_context *rctx = (struct registry_context *)_data;
	struct registry_key *root;
	WERROR error;

	error = reg_get_predefined_key_by_name(rctx, "HKEY_CLASSES_ROOT",
					       &root);
	torture_assert_werr_ok(tctx, error,
			       "getting predefined key failed");

	error = reg_get_predefined_key_by_name(rctx, "HKEY_classes_ROOT",
					       &root);
	torture_assert_werr_ok(tctx, error,
			       "getting predefined key case insensitively failed");

	return true;
}

static bool test_predef_key_by_name_invalid(struct torture_context *tctx,
		void *_data)
{
	struct registry_context *rctx = (struct registry_context *)_data;
	struct registry_key *root;
	WERROR error;

	error = reg_get_predefined_key_by_name(rctx, "BLA", &root);
	torture_assert_werr_equal(tctx, error, WERR_BADFILE,
				  "getting predefined key failed");
	return true;
}

/**
 * Test creating a new subkey
 */
static bool test_create_subkey(struct torture_context *tctx, void *_data)
{
	struct registry_context *rctx = (struct registry_context *)_data;
	struct registry_key *root, *newkey;
	WERROR error;

	error = reg_get_predefined_key(rctx, HKEY_CLASSES_ROOT, &root);
	torture_assert_werr_ok(tctx, error,
			       "getting predefined key failed");

	error = reg_key_add_name(rctx, root, "Bad Bentheim", NULL, NULL,
				 &newkey);
	torture_assert_werr_ok(tctx, error, "Creating key return code");
	torture_assert(tctx, newkey != NULL, "Creating new key");

	return true;
}

/**
 * Test creating a new nested subkey
 */
static bool test_create_nested_subkey(struct torture_context *tctx, void *_data)
{
	struct registry_context *rctx = (struct registry_context *)_data;
	struct registry_key *root, *newkey;
	WERROR error;

	error = reg_get_predefined_key(rctx, HKEY_CLASSES_ROOT, &root);
	torture_assert_werr_ok(tctx, error,
			       "getting predefined key failed");

	error = reg_key_add_name(rctx, root, "Hamburg\\Hamburg", NULL, NULL,
				 &newkey);
	torture_assert_werr_ok(tctx, error, "Creating key return code");
	torture_assert(tctx, newkey != NULL, "Creating new key");

	return true;
}

/**
 * Test creating a new subkey
 */
static bool test_key_add_abs_top(struct torture_context *tctx, void *_data)
{
	struct registry_context *rctx = (struct registry_context *)_data;
	struct registry_key *root;
	WERROR error;

	error = reg_key_add_abs(tctx, rctx, "HKEY_CLASSES_ROOT", 0, NULL,
				&root);
	torture_assert_werr_equal(tctx, error, WERR_ALREADY_EXISTS,
				  "create top level");

	return true;
}

/**
 * Test creating a new subkey
 */
static bool test_key_add_abs(struct torture_context *tctx, void *_data)
{
	WERROR error;
	struct registry_context *rctx = (struct registry_context *)_data;
	struct registry_key *root, *result1, *result2;

	error = reg_key_add_abs(tctx, rctx,  "HKEY_CLASSES_ROOT\\bloe", 0, NULL,
				&result1);
	torture_assert_werr_ok(tctx, error, "create lowest");

	error = reg_key_add_abs(tctx, rctx,  "HKEY_CLASSES_ROOT\\bloe\\bla", 0,
				NULL, &result1);
	torture_assert_werr_ok(tctx, error, "create nested");

	error = reg_get_predefined_key(rctx, HKEY_CLASSES_ROOT, &root);
	torture_assert_werr_ok(tctx, error,
			       "getting predefined key failed");

	error = reg_open_key(tctx, root, "bloe", &result2);
	torture_assert_werr_ok(tctx, error, "opening key");

	error = reg_open_key(tctx, root, "bloe\\bla", &result2);
	torture_assert_werr_ok(tctx, error, "opening key");

	return true;
}


static bool test_del_key(struct torture_context *tctx, void *_data)
{
	struct registry_context *rctx = (struct registry_context *)_data;
	struct registry_key *root, *newkey;
	WERROR error;

	error = reg_get_predefined_key(rctx, HKEY_CLASSES_ROOT, &root);
	torture_assert_werr_ok(tctx, error,
			       "getting predefined key failed");

	error = reg_key_add_name(rctx, root, "Polen", NULL, NULL, &newkey);

	torture_assert_werr_ok(tctx, error, "Creating key return code");
	torture_assert(tctx, newkey != NULL, "Creating new key");

	error = reg_key_del(tctx, root, "Polen");
	torture_assert_werr_ok(tctx, error, "Delete key");

	error = reg_key_del(tctx, root, "Polen");
	torture_assert_werr_equal(tctx, error, WERR_BADFILE,
				  "Delete missing key");

	return true;
}

/**
 * Convenience function for opening the HKEY_CLASSES_ROOT hive and
 * creating a single key for testing purposes.
 */
static bool create_test_key(struct torture_context *tctx,
			    struct registry_context *rctx,
			    const char *name,
			    struct registry_key **root,
			    struct registry_key **subkey)
{
	WERROR error;

	error = reg_get_predefined_key(rctx, HKEY_CLASSES_ROOT, root);
	torture_assert_werr_ok(tctx, error,
			       "getting predefined key failed");

	error = reg_key_add_name(rctx, *root, name, NULL, NULL, subkey);
	torture_assert_werr_ok(tctx, error, "Creating key return code");

	return true;
}


static bool test_flush_key(struct torture_context *tctx, void *_data)
{
	struct registry_context *rctx = (struct registry_context *)_data;
	struct registry_key *root, *subkey;
	WERROR error;

	if (!create_test_key(tctx, rctx, "Bremen", &root, &subkey))
		return false;

	error = reg_key_flush(subkey);
	torture_assert_werr_ok(tctx, error, "flush key");

	torture_assert_werr_equal(tctx, reg_key_flush(NULL),
				  WERR_INVALID_PARAM, "flush key");

	return true;
}

static bool test_query_key(struct torture_context *tctx, void *_data)
{
	struct registry_context *rctx = (struct registry_context *)_data;
	struct registry_key *root, *subkey;
	WERROR error;
	NTTIME last_changed_time;
	uint32_t num_subkeys, num_values;
	const char *classname;

	if (!create_test_key(tctx, rctx, "Munchen", &root, &subkey))
		return false;

	error = reg_key_get_info(tctx, subkey, &classname,
				 &num_subkeys, &num_values,
				 &last_changed_time, NULL, NULL, NULL);

	torture_assert_werr_ok(tctx, error, "get info key");
	torture_assert(tctx, classname == NULL, "classname");
	torture_assert_int_equal(tctx, num_subkeys, 0, "num subkeys");
	torture_assert_int_equal(tctx, num_values, 0, "num values");

	return true;
}

static bool test_query_key_nums(struct torture_context *tctx, void *_data)
{
	struct registry_context *rctx = (struct registry_context *)_data;
	struct registry_key *root, *subkey1, *subkey2;
	WERROR error;
	uint32_t num_subkeys, num_values;
	char data[4];
	SIVAL(data, 0, 42);

	if (!create_test_key(tctx, rctx, "Berlin", &root, &subkey1))
		return false;

	error = reg_key_add_name(rctx, subkey1, "Bentheim", NULL, NULL,
				 &subkey2);
	torture_assert_werr_ok(tctx, error, "Creating key return code");

	error = reg_val_set(subkey1, "Answer", REG_DWORD,
			    data_blob_talloc(tctx, &data, sizeof(data)));
	torture_assert_werr_ok(tctx, error, "set value");

	error = reg_key_get_info(tctx, subkey1, NULL, &num_subkeys,
				 &num_values, NULL, NULL, NULL, NULL);

	torture_assert_werr_ok(tctx, error, "get info key");
	torture_assert_int_equal(tctx, num_subkeys, 1, "num subkeys");
	torture_assert_int_equal(tctx, num_values, 1, "num values");

	return true;
}

/**
 * Test that the subkeys of a key can be enumerated, that
 * the returned parameters for get_subkey_by_index are optional and
 * that enumerating the parents of a non-top-level node works.
 */
static bool test_list_subkeys(struct torture_context *tctx, void *_data)
{
	struct registry_context *rctx = (struct registry_context *)_data;
	struct registry_key *subkey = NULL, *root;
	WERROR error;
	NTTIME last_mod_time;
	const char *classname, *name;

	if (!create_test_key(tctx, rctx, "Goettingen", &root, &subkey))
		return false;

	error = reg_key_get_subkey_by_index(tctx, root, 0, &name, &classname,
					    &last_mod_time);

	torture_assert_werr_ok(tctx, error, "Enum keys return code");
	torture_assert_str_equal(tctx, name, "Goettingen", "Enum keys data");


	error = reg_key_get_subkey_by_index(tctx, root, 0, NULL, NULL, NULL);

	torture_assert_werr_ok(tctx, error,
			       "Enum keys with NULL arguments return code");

	error = reg_key_get_subkey_by_index(tctx, root, 1, NULL, NULL, NULL);

	torture_assert_werr_equal(tctx, error, WERR_NO_MORE_ITEMS,
				  "Invalid error for no more items");

	error = reg_key_get_subkey_by_index(tctx, subkey, 0, NULL, NULL, NULL);

	torture_assert_werr_equal(tctx, error, WERR_NO_MORE_ITEMS,
				  "Invalid error for no more items");

	return true;
}

/**
 * Test setting a value
 */
static bool test_set_value(struct torture_context *tctx, void *_data)
{
	struct registry_context *rctx = (struct registry_context *)_data;
	struct registry_key *subkey = NULL, *root;
	WERROR error;
	char data[4];

	SIVAL(data, 0, 42);

	if (!create_test_key(tctx, rctx, "Dusseldorf", &root, &subkey))
		return false;

	error = reg_val_set(subkey, "Answer", REG_DWORD,
			    data_blob_talloc(tctx, data, sizeof(data)));
	torture_assert_werr_ok (tctx, error, "setting value");

	return true;
}

/**
 * Test getting/setting security descriptors
 */
static bool test_security(struct torture_context *tctx, void *_data)
{
	struct registry_context *rctx =	(struct registry_context *)_data;
	struct registry_key *subkey = NULL, *root;
	WERROR error;
	struct security_descriptor *osd, *nsd;

	if (!create_test_key(tctx, rctx, "Düsseldorf", &root, &subkey))
		return false;

	osd = security_descriptor_dacl_create(tctx,
					 0,
					 NULL, NULL,
					 SID_NT_AUTHENTICATED_USERS,
					 SEC_ACE_TYPE_ACCESS_ALLOWED,
					 SEC_GENERIC_ALL,
					 SEC_ACE_FLAG_OBJECT_INHERIT,
					 NULL);

	error = reg_set_sec_desc(subkey, osd);
	torture_assert_werr_ok(tctx, error, "setting security descriptor");

	error = reg_get_sec_desc(tctx, subkey, &nsd);
	torture_assert_werr_ok (tctx, error, "getting security descriptor");

	torture_assert(tctx, security_descriptor_equal(osd, nsd),
		       "security descriptor changed!");

	return true;
}

/**
 * Test getting a value
 */
static bool test_get_value(struct torture_context *tctx, void *_data)
{
	struct registry_context *rctx =	(struct registry_context *)_data;
	struct registry_key *subkey = NULL, *root;
	WERROR error;
	DATA_BLOB data;
	char value[4];
	uint32_t type;
	SIVAL(value, 0, 42);

	if (!create_test_key(tctx, rctx, "Duisburg", &root, &subkey))
		return false;

	error = reg_key_get_value_by_name(tctx, subkey, __FUNCTION__, &type,
					  &data);
	torture_assert_werr_equal(tctx, error, WERR_BADFILE,
				  "getting missing value");

	error = reg_val_set(subkey, __FUNCTION__, REG_DWORD,
			    data_blob_talloc(tctx, value, sizeof(value)));
	torture_assert_werr_ok(tctx, error, "setting value");

	error = reg_key_get_value_by_name(tctx, subkey, __FUNCTION__, &type,
					  &data);
	torture_assert_werr_ok(tctx, error, "getting value");

	torture_assert_int_equal(tctx, sizeof(value), data.length, "value length ok");
	torture_assert_mem_equal(tctx, data.data, value, sizeof(value),
				 "value content ok");
	torture_assert_int_equal(tctx, REG_DWORD, type, "value type");

	return true;
}

/**
 * Test unsetting a value
 */
static bool test_del_value(struct torture_context *tctx, void *_data)
{
	struct registry_context *rctx =(struct registry_context *)_data;
	struct registry_key *subkey = NULL, *root;
	WERROR error;
	DATA_BLOB data;
	uint32_t type;
	char value[4];
	SIVAL(value, 0, 42);

	if (!create_test_key(tctx, rctx, "Warschau", &root, &subkey))
		return false;

	error = reg_key_get_value_by_name(tctx, subkey, __FUNCTION__, &type,
					  &data);
	torture_assert_werr_equal(tctx, error, WERR_BADFILE,
				  "getting missing value");

	error = reg_val_set(subkey, __FUNCTION__, REG_DWORD,
			    data_blob_talloc(tctx, value, sizeof(value)));
	torture_assert_werr_ok (tctx, error, "setting value");

	error = reg_del_value(tctx, subkey, __FUNCTION__);
	torture_assert_werr_ok (tctx, error, "unsetting value");

	error = reg_key_get_value_by_name(tctx, subkey, __FUNCTION__,
					  &type, &data);
	torture_assert_werr_equal(tctx, error, WERR_BADFILE,
				  "getting missing value");

	return true;
}

/**
 * Test listing values
 */
static bool test_list_values(struct torture_context *tctx, void *_data)
{
	struct registry_context *rctx = (struct registry_context *)_data;
	struct registry_key *subkey = NULL, *root;
	WERROR error;
	DATA_BLOB data;
	uint32_t type;
	const char *name;
	char value[4];
	SIVAL(value, 0, 42);

	if (!create_test_key(tctx, rctx, "Bonn", &root, &subkey))
		return false;

	error = reg_val_set(subkey, "bar", REG_DWORD,
			    data_blob_talloc(tctx, value, sizeof(value)));
	torture_assert_werr_ok (tctx, error, "setting value");

	error = reg_key_get_value_by_index(tctx, subkey, 0, &name,
					   &type, &data);
	torture_assert_werr_ok(tctx, error, "getting value");

	torture_assert_str_equal(tctx, name, "bar", "value name");
	torture_assert_int_equal(tctx, sizeof(value), data.length, "value length");
	torture_assert_mem_equal(tctx, data.data, value, sizeof(value),
		       "value content");
	torture_assert_int_equal(tctx, REG_DWORD, type, "value type");

	error = reg_key_get_value_by_index(tctx, subkey, 1, &name,
					   &type, &data);
	torture_assert_werr_equal(tctx, error, WERR_NO_MORE_ITEMS,
				  "getting missing value");

	return true;
}

static bool setup_local_registry(struct torture_context *tctx, void **data)
{
	struct registry_context *rctx;
	WERROR error;
	char *tempdir;
	NTSTATUS status;
	struct hive_key *hive_key;
	const char *filename;

	error = reg_open_local(tctx, &rctx);
	torture_assert_werr_ok(tctx, error, "Opening local registry failed");

	status = torture_temp_dir(tctx, "registry-local", &tempdir);
	torture_assert_ntstatus_ok(tctx, status, "Creating temp dir failed");

	filename = talloc_asprintf(tctx, "%s/classes_root.ldb", tempdir);
	error = reg_open_ldb_file(tctx, filename, NULL, NULL, tctx->ev, tctx->lp_ctx, &hive_key);
	torture_assert_werr_ok(tctx, error, "Opening classes_root file failed");

	error = reg_mount_hive(rctx, hive_key, HKEY_CLASSES_ROOT, NULL);
	torture_assert_werr_ok(tctx, error, "Mounting hive failed");

	*data = rctx;

	return true;
}

static void tcase_add_tests(struct torture_tcase *tcase)
{
	torture_tcase_add_simple_test(tcase, "list_subkeys",
					test_list_subkeys);
	torture_tcase_add_simple_test(tcase, "get_predefined_key",
					test_get_predefined);
	torture_tcase_add_simple_test(tcase, "get_predefined_key",
					test_get_predefined_unknown);
	torture_tcase_add_simple_test(tcase, "create_key",
					test_create_subkey);
	torture_tcase_add_simple_test(tcase, "create_key",
					test_create_nested_subkey);
	torture_tcase_add_simple_test(tcase, "key_add_abs",
					test_key_add_abs);
	torture_tcase_add_simple_test(tcase, "key_add_abs_top",
					test_key_add_abs_top);
	torture_tcase_add_simple_test(tcase, "set_value",
					test_set_value);
	torture_tcase_add_simple_test(tcase, "get_value",
					test_get_value);
	torture_tcase_add_simple_test(tcase, "list_values",
					test_list_values);
	torture_tcase_add_simple_test(tcase, "del_key",
					test_del_key);
	torture_tcase_add_simple_test(tcase, "del_value",
					test_del_value);
	torture_tcase_add_simple_test(tcase, "flush_key",
					test_flush_key);
	torture_tcase_add_simple_test(tcase, "query_key",
					test_query_key);
	torture_tcase_add_simple_test(tcase, "query_key_nums",
					test_query_key_nums);
	torture_tcase_add_simple_test(tcase, "test_predef_key_by_name",
					test_predef_key_by_name);
	torture_tcase_add_simple_test(tcase, "security",
					test_security);
	torture_tcase_add_simple_test(tcase,"test_predef_key_by_name_invalid",
					test_predef_key_by_name_invalid);
}

struct torture_suite *torture_registry_registry(TALLOC_CTX *mem_ctx)
{
	struct torture_tcase *tcase;
	struct torture_suite *suite = torture_suite_create(mem_ctx, "registry");

	tcase = torture_suite_add_tcase(suite, "local");
	torture_tcase_set_fixture(tcase, setup_local_registry, NULL);
	tcase_add_tests(tcase);

	return suite;
}
