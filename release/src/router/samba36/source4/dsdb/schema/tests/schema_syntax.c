/* 
   Unix SMB/CIFS implementation.

   Test DSDB syntax functions

   Copyright (C) Andrew Bartlet <abartlet@samba.org> 2008
   
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
#include "lib/events/events.h"
#include <ldb.h>
#include <ldb_errors.h>
#include "lib/ldb-samba/ldif_handlers.h"
#include "ldb_wrap.h"
#include "dsdb/samdb/samdb.h"
#include "param/param.h"
#include "torture/smbtorture.h"
#include "torture/local/proto.h"
#include "param/provision.h"


struct torture_dsdb_syntax {
	struct ldb_context *ldb;
	struct dsdb_schema *schema;
};

DATA_BLOB hexstr_to_data_blob(TALLOC_CTX *mem_ctx, const char *string) 
{
	DATA_BLOB binary = data_blob_talloc(mem_ctx, NULL, strlen(string)/2);
	binary.length = strhex_to_str((char *)binary.data, binary.length, string, strlen(string));
	return binary;
}

static bool torture_syntax_add_OR_Name(struct torture_context *tctx,
				       struct ldb_context *ldb,
				       struct dsdb_schema *schema)
{
	WERROR werr;
	int ldb_res;
	struct ldb_ldif *ldif;
	const char *ldif_str =	"dn: CN=ms-Exch-Auth-Orig,CN=Schema,CN=Configuration,DC=kma-exch,DC=devel\n"
				"changetype: add\n"
				"cn: ms-Exch-Auth-Orig\n"
				"attributeID: 1.2.840.113556.1.2.129\n"
				"attributeSyntax: 2.5.5.7\n"
				"isSingleValued: FALSE\n"
				"linkID: 110\n"
				"showInAdvancedViewOnly: TRUE\n"
				"adminDisplayName: ms-Exch-Auth-Orig\n"
				"oMObjectClass:: VgYBAgULHQ==\n"
				"adminDescription: ms-Exch-Auth-Orig\n"
				"oMSyntax: 127\n"
				"searchFlags: 16\n"
				"lDAPDisplayName: authOrig\n"
				"name: ms-Exch-Auth-Orig\n"
				"objectGUID:: 7tqEWktjAUqsZXqsFPQpRg==\n"
				"schemaIDGUID:: l3PfqOrF0RG7ywCAx2ZwwA==\n"
				"attributeSecurityGUID:: VAGN5Pi80RGHAgDAT7lgUA==\n"
				"isMemberOfPartialAttributeSet: TRUE\n";

	ldif = ldb_ldif_read_string(ldb, &ldif_str);
	torture_assert(tctx, ldif, "Failed to parse LDIF for authOrig");

	werr = dsdb_attribute_from_ldb(ldb, schema, ldif->msg);
	ldb_ldif_read_free(ldb, ldif);
	torture_assert_werr_ok(tctx, werr, "dsdb_attribute_from_ldb() failed!");

	ldb_res = dsdb_set_schema(ldb, schema);
	torture_assert_int_equal(tctx, ldb_res, LDB_SUCCESS, "dsdb_set_schema() failed");

	return true;
};

static bool torture_test_syntax(struct torture_context *torture,
				struct torture_dsdb_syntax *priv,
				const char *oid,
				const char *attr_string,
				const char *ldb_str,
				const char *drs_str)
{
	TALLOC_CTX *tmp_ctx = talloc_new(torture);
	DATA_BLOB drs_binary = hexstr_to_data_blob(tmp_ctx, drs_str);
	DATA_BLOB ldb_blob = data_blob_string_const(ldb_str);
	struct drsuapi_DsReplicaAttribute drs, drs2;
	struct drsuapi_DsAttributeValue val;
	const struct dsdb_syntax *syntax;
	const struct dsdb_attribute *attr;
	struct ldb_message_element el;
	struct ldb_context *ldb = priv->ldb;
	struct dsdb_schema *schema = priv->schema;
	struct dsdb_syntax_ctx syntax_ctx;

	/* use default syntax conversion context */
	dsdb_syntax_ctx_init(&syntax_ctx, ldb, schema);

	drs.value_ctr.num_values = 1;
	drs.value_ctr.values = &val;
	val.blob = &drs_binary;

	torture_assert(torture, syntax = find_syntax_map_by_standard_oid(oid), "Failed to find syntax handler");
	torture_assert(torture, attr = dsdb_attribute_by_lDAPDisplayName(schema, attr_string), "Failed to find attribute handler");
	torture_assert_str_equal(torture, attr->syntax->name, syntax->name, "Syntax from schema not as expected");
	

	torture_assert_werr_ok(torture, syntax->drsuapi_to_ldb(&syntax_ctx, attr, &drs, tmp_ctx, &el), "Failed to convert from DRS to ldb format");

	torture_assert_data_blob_equal(torture, el.values[0], ldb_blob, "Incorrect conversion from DRS to ldb format");

	torture_assert_werr_ok(torture, syntax->ldb_to_drsuapi(&syntax_ctx, attr, &el, tmp_ctx, &drs2), "Failed to convert from ldb to DRS format");
	
	torture_assert(torture, drs2.value_ctr.values[0].blob, "No blob returned from conversion");

	torture_assert_data_blob_equal(torture, *drs2.value_ctr.values[0].blob, drs_binary, "Incorrect conversion from ldb to DRS format");
	return true;
}

static bool torture_dsdb_drs_DN_BINARY(struct torture_context *torture, struct torture_dsdb_syntax *priv)
{
	bool ret;
	const char *ldb_str = "B:32:A9D1CA15768811D1ADED00C04FD8D5CD:<GUID=a8378c29-6319-45b3-b216-6a3108452d6c>;CN=Users,DC=ad,DC=ruth,DC=abartlet,DC=net";
	const char *drs_str = "8C00000000000000298C37A81963B345B2166A3108452D6C000000000000000000000000000000000000000000000000000000002900000043004E003D00550073006500720073002C00440043003D00610064002C00440043003D0072007500740068002C00440043003D00610062006100720074006C00650074002C00440043003D006E0065007400000014000000A9D1CA15768811D1ADED00C04FD8D5CD";
	const char *ldb_str2 = "B:8:00000002:<GUID=2b475208-3180-4ad4-b6bb-a26cfb44ac50>;<SID=S-1-5-21-3686369990-3108025515-1819299124>;DC=ad,DC=ruth,DC=abartlet,DC=net";
	const char *drs_str2 = "7A000000180000000852472B8031D44AB6BBA26CFB44AC50010400000000000515000000C68AB9DBABB440B9344D706C0000000020000000440043003D00610064002C00440043003D0072007500740068002C00440043003D00610062006100720074006C00650074002C00440043003D006E0065007400000000000800000000000002";
	ret = torture_test_syntax(torture, priv, DSDB_SYNTAX_BINARY_DN, "wellKnownObjects", ldb_str, drs_str);
	if (!ret) return false;
	return torture_test_syntax(torture, priv, DSDB_SYNTAX_BINARY_DN, "msDS-HasInstantiatedNCs", ldb_str2, drs_str2);
}

static bool torture_dsdb_drs_DN(struct torture_context *torture, struct torture_dsdb_syntax *priv)
{
	const char *ldb_str = "<GUID=fbee08fd-6f75-4bd4-af3f-e4f063a6379e>;OU=Domain Controllers,DC=ad,DC=naomi,DC=abartlet,DC=net";
	const char *drs_str = "A800000000000000FD08EEFB756FD44BAF3FE4F063A6379E00000000000000000000000000000000000000000000000000000000370000004F0055003D0044006F006D00610069006E00200043006F006E00740072006F006C006C006500720073002C00440043003D00610064002C00440043003D006E0061006F006D0069002C00440043003D00610062006100720074006C00650074002C00440043003D006E00650074000000";
	if (!torture_test_syntax(torture, priv, LDB_SYNTAX_DN, "lastKnownParent", ldb_str, drs_str)) {
		return false;
	}

	/* extended_dn with GUID and SID in it */
	ldb_str = "<GUID=23cc7d16-3da0-4f3a-9921-0ad60a99230f>;<SID=S-1-5-21-3427639452-1671929926-2759570404-500>;CN=Administrator,CN=Users,DC=kma-exch,DC=devel";
	drs_str = "960000001C000000167DCC23A03D3A4F99210AD60A99230F0105000000000005150000009CA04DCC46A0A763E4B37BA4F40100002E00000043004E003D00410064006D0069006E006900730074007200610074006F0072002C0043004E003D00550073006500720073002C00440043003D006B006D0061002D0065007800630068002C00440043003D0064006500760065006C000000";
	return torture_test_syntax(torture, priv, LDB_SYNTAX_DN, "lastKnownParent", ldb_str, drs_str);
}

static bool torture_dsdb_drs_OR_Name(struct torture_context *torture, struct torture_dsdb_syntax *priv)
{
	const char *ldb_str = "<GUID=23cc7d16-3da0-4f3a-9921-0ad60a99230f>;<SID=S-1-5-21-3427639452-1671929926-2759570404-500>;CN=Administrator,CN=Users,DC=kma-exch,DC=devel";
	const char *drs_str = "960000001C000000167DCC23A03D3A4F99210AD60A99230F0105000000000005150000009CA04DCC46A0A763E4B37BA4F40100002E00000043004E003D00410064006D0069006E006900730074007200610074006F0072002C0043004E003D00550073006500720073002C00440043003D006B006D0061002D0065007800630068002C00440043003D0064006500760065006C000000000004000000";
	return torture_test_syntax(torture, priv, DSDB_SYNTAX_OR_NAME, "authOrig", ldb_str, drs_str);
}

static bool torture_dsdb_drs_INT32(struct torture_context *torture, struct torture_dsdb_syntax *priv)
{
	const char *ldb_str = "532480";
	const char *drs_str = "00200800";
	return torture_test_syntax(torture, priv, LDB_SYNTAX_INTEGER, "userAccountControl", ldb_str, drs_str);
}

static bool torture_dsdb_drs_INT64(struct torture_context *torture, struct torture_dsdb_syntax *priv)
{
	const char *ldb_str = "129022979538281250";
	const char *drs_str = "22E33D5FB761CA01";
	return torture_test_syntax(torture, priv, "1.2.840.113556.1.4.906", "pwdLastSet", ldb_str, drs_str);
}

static bool torture_dsdb_drs_NTTIME(struct torture_context *torture, struct torture_dsdb_syntax *priv)
{
	const char *ldb_str = "20091109003446.0Z";
	const char *drs_str = "A6F4070103000000";
	return torture_test_syntax(torture, priv, "1.3.6.1.4.1.1466.115.121.1.24", "whenCreated", ldb_str, drs_str);
}

static bool torture_dsdb_drs_BOOL(struct torture_context *torture, struct torture_dsdb_syntax *priv)
{
	const char *ldb_str = "TRUE";
	const char *drs_str = "01000000";
	return torture_test_syntax(torture, priv, LDB_SYNTAX_BOOLEAN, "isDeleted", ldb_str, drs_str);
}

static bool torture_dsdb_drs_UNICODE(struct torture_context *torture, struct torture_dsdb_syntax *priv)
{
	const char *ldb_str = "primaryTelexNumber,Numéro de télex";
	const char *drs_str = "7000720069006D00610072007900540065006C00650078004E0075006D006200650072002C004E0075006D00E90072006F0020006400650020007400E9006C0065007800";
	return torture_test_syntax(torture, priv, LDB_SYNTAX_DIRECTORY_STRING, "attributeDisplayNames", ldb_str, drs_str);
}

/*
 * DSDB-SYNTAX fixture setup/teardown handlers implementation
 */
static bool torture_dsdb_syntax_tcase_setup(struct torture_context *tctx, void **data)
{
	struct torture_dsdb_syntax *priv;

	priv = talloc_zero(tctx, struct torture_dsdb_syntax);
	torture_assert(tctx, priv, "No memory");

	priv->ldb = provision_get_schema(priv, tctx->lp_ctx, NULL);
	torture_assert(tctx, priv->ldb, "Failed to load schema from disk");

	priv->schema = dsdb_get_schema(priv->ldb, NULL);
	torture_assert(tctx, priv->schema, "Failed to fetch schema");

	/* add 'authOrig' attribute with OR-Name syntax to schema */
	if (!torture_syntax_add_OR_Name(tctx, priv->ldb, priv->schema)) {
		return false;
	}

	*data = priv;
	return true;
}

static bool torture_dsdb_syntax_tcase_teardown(struct torture_context *tctx, void *data)
{
	struct torture_dsdb_syntax *priv;

	priv = talloc_get_type_abort(data, struct torture_dsdb_syntax);
	talloc_free(priv);

	return true;
}

/**
 * DSDB-SYNTAX test suite creation
 */
struct torture_suite *torture_dsdb_syntax(TALLOC_CTX *mem_ctx)
{
	typedef bool (*pfn_run)(struct torture_context *, void *);

	struct torture_tcase *tc;
	struct torture_suite *suite = torture_suite_create(mem_ctx, "dsdb.syntax");

	if (suite == NULL) {
		return NULL;
	}

	tc = torture_suite_add_tcase(suite, "tc");
	if (!tc) {
		return NULL;
	}

	torture_tcase_set_fixture(tc,
				  torture_dsdb_syntax_tcase_setup,
				  torture_dsdb_syntax_tcase_teardown);

	torture_tcase_add_simple_test(tc, "DN-BINARY", (pfn_run)torture_dsdb_drs_DN_BINARY);
	torture_tcase_add_simple_test(tc, "DN", (pfn_run)torture_dsdb_drs_DN);
	torture_tcase_add_simple_test(tc, "OR-Name", (pfn_run)torture_dsdb_drs_OR_Name);
	torture_tcase_add_simple_test(tc, "INT32", (pfn_run)torture_dsdb_drs_INT32);
	torture_tcase_add_simple_test(tc, "INT64", (pfn_run)torture_dsdb_drs_INT64);
	torture_tcase_add_simple_test(tc, "NTTIME", (pfn_run)torture_dsdb_drs_NTTIME);
	torture_tcase_add_simple_test(tc, "BOOL", (pfn_run)torture_dsdb_drs_BOOL);
	torture_tcase_add_simple_test(tc, "UNICODE", (pfn_run)torture_dsdb_drs_UNICODE);

	suite->description = talloc_strdup(suite, "DSDB syntax tests");

	return suite;
}
