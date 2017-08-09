/*
   Unix SMB/CIFS implementation.

   DRSUAPI schemaInfo unit tests

   Copyright (C) Kamen Mazdrashki <kamenim@samba.org> 2010

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
#include "system/filesys.h"
#include "torture/smbtorture.h"
#include "dsdb/samdb/samdb.h"
#include "dsdb/samdb/ldb_modules/util.h"
#include "ldb_wrap.h"
#include <ldb_module.h>
#include "torture/rpc/drsuapi.h"
#include "librpc/ndr/libndr.h"
#include "param/param.h"


/**
 * schemaInfo to init ldb context with
 *   Rev:  0
 *   GUID: 00000000-0000-0000-0000-000000000000
 */
#define SCHEMA_INFO_INIT_STR		"FF0000000000000000000000000000000000000000"

/**
 * Default schema_info string to be used for testing
 *   Rev:  01
 *   GUID: 071c82fd-45c7-4351-a3db-51f75a630a7f
 */
#define SCHEMA_INFO_DEFAULT_STR 	"FF00000001FD821C07C7455143A3DB51F75A630A7F"

/**
 * Schema info data to test with
 */
struct schemainfo_data {
	DATA_BLOB 	ndr_blob;
	struct dsdb_schema_info schi;
	WERROR		werr_expected;
	bool 		test_both_ways;
};

/**
 * Schema info test data in human-readable format (... kind of)
 */
static const struct {
	const char 	*schema_info_str;
	uint32_t	revision;
	const char	*guid_str;
	WERROR		werr_expected;
	bool 		test_both_ways;
} _schemainfo_test_data[] = {
	{
		.schema_info_str = "FF0000000000000000000000000000000000000000",
		.revision = 0,
		.guid_str = "00000000-0000-0000-0000-000000000000",
		.werr_expected = WERR_OK,
		.test_both_ways = true
	},
	{
		.schema_info_str = "FF00000001FD821C07C7455143A3DB51F75A630A7F",
		.revision = 1,
		.guid_str = "071c82fd-45c7-4351-a3db-51f75a630a7f",
		.werr_expected = WERR_OK,
		.test_both_ways = true
	},
	{
		.schema_info_str = "FFFFFFFFFFFD821C07C7455143A3DB51F75A630A7F",
		.revision = 0xFFFFFFFF,
		.guid_str = "071c82fd-45c7-4351-a3db-51f75a630a7f",
		.werr_expected = WERR_OK,
		.test_both_ways = true
	},
	{ /* len == 21 */
		.schema_info_str = "FF00000001FD821C07C7455143A3DB51F75A630A7F00",
		.revision = 1,
		.guid_str = "071c82fd-45c7-4351-a3db-51f75a630a7f",
		.werr_expected = WERR_INVALID_PARAMETER,
		.test_both_ways = false
	},
	{ /* marker == FF */
		.schema_info_str = "AA00000001FD821C07C7455143A3DB51F75A630A7F",
		.revision = 1,
		.guid_str = "071c82fd-45c7-4351-a3db-51f75a630a7f",
		.werr_expected = WERR_INVALID_PARAMETER,
		.test_both_ways = false
	}
};

/**
 * Private data to be shared among all test in Test case
 */
struct drsut_schemainfo_data {
	struct ldb_context *ldb;
	struct ldb_module  *ldb_module;
	struct dsdb_schema *schema;

	/* Initial schemaInfo set in ldb to test with */
	struct dsdb_schema_info *schema_info;

	uint32_t test_data_count;
	struct schemainfo_data *test_data;
};

/**
 * torture macro to assert for equal dsdb_schema_info's
 */
#define torture_assert_schema_info_equal(torture_ctx,got,expected,cmt)\
	do { const struct dsdb_schema_info *__got = (got), *__expected = (expected); \
	if (__got->revision != __expected->revision) { \
		torture_result(torture_ctx, TORTURE_FAIL, \
			       __location__": "#got".revision %d did not match "#expected".revision %d: %s", \
			       (int)__got->revision, (int)__expected->revision, cmt); \
		return false; \
	} \
	if (!GUID_equal(&__got->invocation_id, &__expected->invocation_id)) { \
		torture_result(torture_ctx, TORTURE_FAIL, \
			       __location__": "#got".invocation_id did not match "#expected".invocation_id: %s", cmt); \
		return false; \
	} \
	} while(0)

/*
 * forward declaration for internal functions
 */
static bool _drsut_ldb_schema_info_reset(struct torture_context *tctx,
					 struct ldb_context *ldb,
					 const char *schema_info_str,
					 bool in_setup);


/**
 * Creates dsdb_schema_info object based on NDR data
 * passed as hex string
 */
static bool _drsut_schemainfo_new(struct torture_context *tctx,
				  const char *schema_info_str, struct dsdb_schema_info **_si)
{
	WERROR werr;
	DATA_BLOB blob;

	blob = strhex_to_data_blob(tctx, schema_info_str);
	if (!blob.data) {
		torture_comment(tctx, "Not enough memory!\n");
		return false;
	}

	werr = dsdb_schema_info_from_blob(&blob, tctx, _si);
	if (!W_ERROR_IS_OK(werr)) {
		torture_comment(tctx,
		                "Failed to create dsdb_schema_info object for %s: %s",
		                schema_info_str,
		                win_errstr(werr));
		return false;
	}

	data_blob_free(&blob);

	return true;
}

/**
 * Creates dsdb_schema_info object based on predefined data
 * Function is public as it is intended to be used by other
 * tests (e.g. prefixMap tests)
 */
bool drsut_schemainfo_new(struct torture_context *tctx, struct dsdb_schema_info **_si)
{
	return _drsut_schemainfo_new(tctx, SCHEMA_INFO_DEFAULT_STR, _si);
}


/*
 * Tests dsdb_schema_info_new() and dsdb_schema_info_blob_new()
 */
static bool test_dsdb_schema_info_new(struct torture_context *tctx,
				      struct drsut_schemainfo_data *priv)
{
	WERROR werr;
	DATA_BLOB ndr_blob;
	DATA_BLOB ndr_blob_expected;
	struct dsdb_schema_info *schi;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_new(priv);
	torture_assert(tctx, mem_ctx, "Not enough memory!");
	ndr_blob_expected = strhex_to_data_blob(mem_ctx, SCHEMA_INFO_INIT_STR);
	torture_assert(tctx, ndr_blob_expected.data, "Not enough memory!");

	werr = dsdb_schema_info_new(mem_ctx, &schi);
	torture_assert_werr_ok(tctx, werr, "dsdb_schema_info_new() failed");
	torture_assert_int_equal(tctx, schi->revision, 0,
				 "dsdb_schema_info_new() creates schemaInfo with invalid revision");
	torture_assert(tctx, GUID_all_zero(&schi->invocation_id),
			"dsdb_schema_info_new() creates schemaInfo with not ZERO GUID");

	werr = dsdb_schema_info_blob_new(mem_ctx, &ndr_blob);
	torture_assert_werr_ok(tctx, werr, "dsdb_schema_info_blob_new() failed");
	torture_assert_data_blob_equal(tctx, ndr_blob, ndr_blob_expected,
				       "dsdb_schema_info_blob_new() returned invalid blob");

	talloc_free(mem_ctx);
	return true;
}

/*
 * Tests dsdb_schema_info_from_blob()
 */
static bool test_dsdb_schema_info_from_blob(struct torture_context *tctx,
					    struct drsut_schemainfo_data *priv)
{
	int i;
	WERROR werr;
	char *msg;
	struct dsdb_schema_info *schema_info;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_new(priv);
	torture_assert(tctx, mem_ctx, "Not enough memory!");

	for (i = 0; i < priv->test_data_count; i++) {
		struct schemainfo_data *data = &priv->test_data[i];

		msg = talloc_asprintf(tctx, "dsdb_schema_info_from_blob() [%d]-[%s]",
		                      i, _schemainfo_test_data[i].schema_info_str);

		werr = dsdb_schema_info_from_blob(&data->ndr_blob, mem_ctx, &schema_info);
		torture_assert_werr_equal(tctx, werr, data->werr_expected, msg);

		/* test returned data */
		if (W_ERROR_IS_OK(werr)) {
			torture_assert_schema_info_equal(tctx,
			                                 schema_info, &data->schi,
			                                 "after dsdb_schema_info_from_blob() call");
		}
	}

	talloc_free(mem_ctx);

	return true;
}

/*
 * Tests dsdb_blob_from_schema_info()
 */
static bool test_dsdb_blob_from_schema_info(struct torture_context *tctx,
					    struct drsut_schemainfo_data *priv)
{
	int i;
	WERROR werr;
	char *msg;
	DATA_BLOB ndr_blob;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_new(priv);
	torture_assert(tctx, mem_ctx, "Not enough memory!");

	for (i = 0; i < priv->test_data_count; i++) {
		struct schemainfo_data *data = &priv->test_data[i];

		/* not all test are valid reverse type of conversion */
		if (!data->test_both_ways) {
			continue;
		}

		msg = talloc_asprintf(tctx, "dsdb_blob_from_schema_info() [%d]-[%s]",
		                      i, _schemainfo_test_data[i].schema_info_str);

		werr = dsdb_blob_from_schema_info(&data->schi, mem_ctx, &ndr_blob);
		torture_assert_werr_equal(tctx, werr, data->werr_expected, msg);

		/* test returned data */
		if (W_ERROR_IS_OK(werr)) {
			torture_assert_data_blob_equal(tctx,
			                               ndr_blob, data->ndr_blob,
			                               "dsdb_blob_from_schema_info()");
		}
	}

	talloc_free(mem_ctx);

	return true;
}

static bool test_dsdb_schema_info_cmp(struct torture_context *tctx,
				      struct drsut_schemainfo_data *priv)
{
	DATA_BLOB blob;
	struct drsuapi_DsReplicaOIDMapping_Ctr *ctr;

	ctr = talloc_zero(priv, struct drsuapi_DsReplicaOIDMapping_Ctr);
	torture_assert(tctx, ctr, "Not enough memory!");

	/* not enough elements */
	torture_assert_werr_equal(tctx,
				  dsdb_schema_info_cmp(priv->schema, ctr),
				  WERR_INVALID_PARAMETER,
				  "dsdb_schema_info_cmp(): unexpected result");

	/* an empty element for schemaInfo */
	ctr->num_mappings = 1;
	ctr->mappings = talloc_zero_array(ctr, struct drsuapi_DsReplicaOIDMapping, 1);
	torture_assert(tctx, ctr->mappings, "Not enough memory!");
	torture_assert_werr_equal(tctx,
				  dsdb_schema_info_cmp(priv->schema, ctr),
				  WERR_INVALID_PARAMETER,
				  "dsdb_schema_info_cmp(): unexpected result");

	/* test with invalid schemaInfo - length != 21 */
	blob = strhex_to_data_blob(ctr, "FF00000001FD821C07C7455143A3DB51F75A630A7F00");
	torture_assert(tctx, blob.data, "Not enough memory!");
	ctr->mappings[0].oid.length     = blob.length;
	ctr->mappings[0].oid.binary_oid = blob.data;
	torture_assert_werr_equal(tctx,
				  dsdb_schema_info_cmp(priv->schema, ctr),
				  WERR_INVALID_PARAMETER,
				  "dsdb_schema_info_cmp(): unexpected result");

	/* test with invalid schemaInfo - marker != 0xFF */
	blob = strhex_to_data_blob(ctr, "AA00000001FD821C07C7455143A3DB51F75A630A7F");
	torture_assert(tctx, blob.data, "Not enough memory!");
	ctr->mappings[0].oid.length     = blob.length;
	ctr->mappings[0].oid.binary_oid = blob.data;
	torture_assert_werr_equal(tctx,
				  dsdb_schema_info_cmp(priv->schema, ctr),
				  WERR_INVALID_PARAMETER,
				  "dsdb_schema_info_cmp(): unexpected result");

	/* test with valid schemaInfo, but not correct one */
	blob = strhex_to_data_blob(ctr, "FF0000000000000000000000000000000000000000");
	torture_assert(tctx, blob.data, "Not enough memory!");
	ctr->mappings[0].oid.length     = blob.length;
	ctr->mappings[0].oid.binary_oid = blob.data;
	torture_assert_werr_equal(tctx,
				  dsdb_schema_info_cmp(priv->schema, ctr),
				  WERR_DS_DRA_SCHEMA_MISMATCH,
				  "dsdb_schema_info_cmp(): unexpected result");

	/* test with correct schemaInfo, but invalid ATTID */
	blob = strhex_to_data_blob(ctr, priv->schema->schema_info);
	torture_assert(tctx, blob.data, "Not enough memory!");
	ctr->mappings[0].id_prefix	= 1;
	ctr->mappings[0].oid.length     = blob.length;
	ctr->mappings[0].oid.binary_oid = blob.data;
	torture_assert_werr_equal(tctx,
				  dsdb_schema_info_cmp(priv->schema, ctr),
				  WERR_INVALID_PARAMETER,
				  "dsdb_schema_info_cmp(): unexpected result");

	/* test with valid schemaInfo */
	blob = strhex_to_data_blob(ctr, priv->schema->schema_info);
	ctr->mappings[0].id_prefix	= 0;
	torture_assert_werr_ok(tctx,
			       dsdb_schema_info_cmp(priv->schema, ctr),
			       "dsdb_schema_info_cmp(): unexpected result");

	talloc_free(ctr);
	return true;
}

/*
 * Tests dsdb_module_schema_info_blob_read()
 *   and dsdb_module_schema_info_blob_write()
 */
static bool test_dsdb_module_schema_info_blob_rw(struct torture_context *tctx,
						struct drsut_schemainfo_data *priv)
{
	int ldb_err;
	DATA_BLOB blob_write;
	DATA_BLOB blob_read;

	/* reset schmeInfo to know value */
	torture_assert(tctx,
		       _drsut_ldb_schema_info_reset(tctx, priv->ldb, SCHEMA_INFO_INIT_STR, false),
		       "_drsut_ldb_schema_info_reset() failed");

	/* write tests' default schemaInfo */
	blob_write = strhex_to_data_blob(priv, SCHEMA_INFO_DEFAULT_STR);
	torture_assert(tctx, blob_write.data, "Not enough memory!");

	ldb_err = dsdb_module_schema_info_blob_write(priv->ldb_module,
						     DSDB_FLAG_TOP_MODULE,
						     &blob_write, NULL);
	torture_assert_int_equal(tctx, ldb_err, LDB_SUCCESS, "dsdb_module_schema_info_blob_write() failed");

	ldb_err = dsdb_module_schema_info_blob_read(priv->ldb_module, DSDB_FLAG_TOP_MODULE,
						    priv, &blob_read, NULL);
	torture_assert_int_equal(tctx, ldb_err, LDB_SUCCESS, "dsdb_module_schema_info_blob_read() failed");

	/* check if we get what we wrote */
	torture_assert_data_blob_equal(tctx, blob_read, blob_write,
				       "Write/Read of schemeInfo blob failed");

	return true;
}

/*
 * Tests dsdb_schema_update_schema_info()
 */
static bool test_dsdb_module_schema_info_update(struct torture_context *tctx,
						struct drsut_schemainfo_data *priv)
{
	int ldb_err;
	WERROR werr;
	DATA_BLOB blob;
	struct dsdb_schema_info *schema_info;

	/* reset schmeInfo to know value */
	torture_assert(tctx,
		       _drsut_ldb_schema_info_reset(tctx, priv->ldb, SCHEMA_INFO_INIT_STR, false),
		       "_drsut_ldb_schema_info_reset() failed");

	ldb_err = dsdb_module_schema_info_update(priv->ldb_module,
						 priv->schema,
						 DSDB_FLAG_TOP_MODULE | DSDB_FLAG_AS_SYSTEM, NULL);
	torture_assert_int_equal(tctx, ldb_err, LDB_SUCCESS, "dsdb_module_schema_info_update() failed");

	/* get updated schemaInfo */
	ldb_err = dsdb_module_schema_info_blob_read(priv->ldb_module, DSDB_FLAG_TOP_MODULE,
						    priv, &blob, NULL);
	torture_assert_int_equal(tctx, ldb_err, LDB_SUCCESS, "dsdb_module_schema_info_blob_read() failed");

	werr = dsdb_schema_info_from_blob(&blob, priv, &schema_info);
	torture_assert_werr_ok(tctx, werr, "dsdb_schema_info_from_blob() failed");

	/* check against default schema_info */
	torture_assert_schema_info_equal(tctx, schema_info, priv->schema_info,
					  "schemaInfo attribute no updated correctly");

	return true;
}


/**
 * Reset schemaInfo record to know value
 */
static bool _drsut_ldb_schema_info_reset(struct torture_context *tctx,
					 struct ldb_context *ldb,
					 const char *schema_info_str,
					 bool in_setup)
{
	bool bret = true;
	int ldb_err;
	DATA_BLOB blob;
	struct ldb_message *msg;
	TALLOC_CTX *mem_ctx = talloc_new(tctx);

	blob = strhex_to_data_blob(mem_ctx, schema_info_str);
	torture_assert_goto(tctx, blob.data, bret, DONE, "Not enough memory!");

	msg = ldb_msg_new(mem_ctx);
	torture_assert_goto(tctx, msg, bret, DONE, "Not enough memory!");

	msg->dn = ldb_get_schema_basedn(ldb);
	ldb_err = ldb_msg_add_value(msg, "schemaInfo", &blob, NULL);
	torture_assert_int_equal_goto(tctx, ldb_err, LDB_SUCCESS, bret, DONE,
				      "ldb_msg_add_value() failed");

	if (in_setup) {
		ldb_err = ldb_add(ldb, msg);
	} else {
		ldb_err = dsdb_replace(ldb, msg, DSDB_MODIFY_PERMISSIVE);
	}
	torture_assert_int_equal_goto(tctx, ldb_err, LDB_SUCCESS, bret, DONE,
				      "dsdb_replace() failed");

DONE:
	talloc_free(mem_ctx);
	return bret;
}

/**
 * Prepare temporary LDB and opens it
 */
static bool _drsut_ldb_setup(struct torture_context *tctx, struct drsut_schemainfo_data *priv)
{
	int ldb_err;
	char *ldb_url;
	bool bret = true;
	char *tempdir = NULL;
	NTSTATUS status;
	TALLOC_CTX* mem_ctx;

	mem_ctx = talloc_new(priv);
	torture_assert(tctx, mem_ctx, "Not enough memory!");

	status = torture_temp_dir(tctx, "drs_", &tempdir);
	torture_assert_ntstatus_ok_goto(tctx, status, bret, DONE, "creating temp dir");

	ldb_url = talloc_asprintf(priv, "%s/drs_schemainfo.ldb", tempdir);
	torture_assert_goto(tctx, ldb_url, bret, DONE, "Not enough memory!");

	/* create LDB */
	priv->ldb = ldb_wrap_connect(priv, tctx->ev, tctx->lp_ctx,
	                             ldb_url, NULL, NULL, 0);
	torture_assert_goto(tctx, priv->ldb, bret, DONE,  "ldb_wrap_connect() failed");

	/* set some schemaNamingContext */
	ldb_err = ldb_set_opaque(priv->ldb,
				 "schemaNamingContext",
				 ldb_dn_new(priv->ldb, priv->ldb, "CN=Schema,CN=Config"));
	torture_assert_int_equal_goto(tctx, ldb_err, LDB_SUCCESS, bret, DONE,
				      "ldb_set_opaque() failed");

	/* add schemaInfo attribute so tested layer could work properly */
	torture_assert_goto(tctx,
			    _drsut_ldb_schema_info_reset(tctx, priv->ldb, SCHEMA_INFO_INIT_STR, true),
			    bret, DONE,
			    "_drsut_ldb_schema_info_reset() failed");

DONE:
	talloc_free(tempdir);
	talloc_free(mem_ctx);
	return bret;
}

/*
 * Setup/Teardown for test case
 */
static bool torture_drs_unit_schemainfo_setup(struct torture_context *tctx,
					      struct drsut_schemainfo_data **_priv)
{
	int i;
	int ldb_err;
	NTSTATUS status;
	DATA_BLOB ndr_blob;
	struct GUID guid;
	struct drsut_schemainfo_data *priv;

	priv = talloc_zero(tctx, struct drsut_schemainfo_data);
	torture_assert(tctx, priv, "Not enough memory!");

	/* returned allocated pointer here
	 * teardown() will be called even in case of failure,
	 * so we'll get a changes to clean up  */
	*_priv = priv;

	/* create initial schemaInfo */
	torture_assert(tctx,
		       _drsut_schemainfo_new(tctx, SCHEMA_INFO_DEFAULT_STR, &priv->schema_info),
		       "Failed to create schema_info test object");

	/* create data to test with */
	priv->test_data_count = ARRAY_SIZE(_schemainfo_test_data);
	priv->test_data = talloc_array(tctx, struct schemainfo_data, priv->test_data_count);

	for (i = 0; i < ARRAY_SIZE(_schemainfo_test_data); i++) {
		struct schemainfo_data *data = &priv->test_data[i];

		ndr_blob = strhex_to_data_blob(priv,
		                               _schemainfo_test_data[i].schema_info_str);
		torture_assert(tctx, ndr_blob.data, "Not enough memory!");

		status = GUID_from_string(_schemainfo_test_data[i].guid_str, &guid);
		torture_assert_ntstatus_ok(tctx, status,
					   talloc_asprintf(tctx,
					                   "GUID_from_string() failed for %s",
					                   _schemainfo_test_data[i].guid_str));

		data->ndr_blob           = ndr_blob;
		data->schi.invocation_id = guid;
		data->schi.revision      = _schemainfo_test_data[i].revision;
		data->werr_expected      = _schemainfo_test_data[i].werr_expected;
		data->test_both_ways     = _schemainfo_test_data[i].test_both_ways;

	}

	/* create temporary LDB and populate with data */
	if (!_drsut_ldb_setup(tctx, priv)) {
		return false;
	}

	/* create ldb_module mockup object */
	priv->ldb_module = ldb_module_new(priv, priv->ldb, "schemaInfo_test_module", NULL);
	torture_assert(tctx, priv->ldb_module, "Not enough memory!");

	/* create schema mockup object */
	priv->schema = dsdb_new_schema(priv);

	/* set schema_info in dsdb_schema for testing */
	priv->schema->schema_info = talloc_strdup(priv->schema, SCHEMA_INFO_DEFAULT_STR);

	/* pre-cache invocationId for samdb_ntds_invocation_id()
	 * to work with our mock ldb */
	ldb_err = ldb_set_opaque(priv->ldb, "cache.invocation_id",
	                         &priv->schema_info->invocation_id);
	torture_assert_int_equal(tctx, ldb_err, LDB_SUCCESS, "ldb_set_opaque() failed");

	/* Perform all tests in transactions so that
	 * underlying modify calls not to fail */
	ldb_err = ldb_transaction_start(priv->ldb);
	torture_assert_int_equal(tctx,
				 ldb_err,
				 LDB_SUCCESS,
				 "ldb_transaction_start() failed");

	return true;
}

static bool torture_drs_unit_schemainfo_teardown(struct torture_context *tctx,
						 struct drsut_schemainfo_data *priv)
{
	int ldb_err;

	/* commit pending transaction so we will
	 * be able to check what LDB state is */
	ldb_err = ldb_transaction_commit(priv->ldb);
	if (ldb_err != LDB_SUCCESS) {
		torture_comment(tctx, "ldb_transaction_commit() - %s (%s)",
		                ldb_strerror(ldb_err),
		                ldb_errstring(priv->ldb));
	}

	talloc_free(priv);

	return true;
}

/**
 * Test case initialization for
 * drs.unit.schemaInfo
 */
struct torture_tcase * torture_drs_unit_schemainfo(struct torture_suite *suite)
{
	typedef bool (*pfn_setup)(struct torture_context *, void **);
	typedef bool (*pfn_teardown)(struct torture_context *, void *);
	typedef bool (*pfn_run)(struct torture_context *, void *);

	struct torture_tcase * tc = torture_suite_add_tcase(suite, "schemaInfo");

	torture_tcase_set_fixture(tc,
				  (pfn_setup)torture_drs_unit_schemainfo_setup,
				  (pfn_teardown)torture_drs_unit_schemainfo_teardown);

	tc->description = talloc_strdup(tc, "Unit tests for DRSUAPI::schemaInfo implementation");

	torture_tcase_add_simple_test(tc, "dsdb_schema_info_new",
				      (pfn_run)test_dsdb_schema_info_new);
	torture_tcase_add_simple_test(tc, "dsdb_schema_info_from_blob",
				      (pfn_run)test_dsdb_schema_info_from_blob);
	torture_tcase_add_simple_test(tc, "dsdb_blob_from_schema_info",
				      (pfn_run)test_dsdb_blob_from_schema_info);
	torture_tcase_add_simple_test(tc, "dsdb_schema_info_cmp",
				      (pfn_run)test_dsdb_schema_info_cmp);
	torture_tcase_add_simple_test(tc, "dsdb_module_schema_info_blob read|write",
				      (pfn_run)test_dsdb_module_schema_info_blob_rw);
	torture_tcase_add_simple_test(tc, "dsdb_module_schema_info_update",
				      (pfn_run)test_dsdb_module_schema_info_update);


	return tc;
}
