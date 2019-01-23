/* 
   Unix SMB/CIFS implementation.

   Test LDB attribute functions

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

#define DSDB_DN_TEST_SID "S-1-5-21-4177067393-1453636373-93818737"

static bool torture_dsdb_dn_attrs(struct torture_context *torture)
{
	TALLOC_CTX *mem_ctx = talloc_new(torture);
	struct ldb_context *ldb;
	const struct ldb_schema_syntax *syntax;
	struct ldb_val dn1, dn2, dn3;

	torture_assert(torture, 
		       ldb = ldb_init(mem_ctx, torture->ev),
		       "Failed to init ldb");

	torture_assert_int_equal(torture, 
				 ldb_register_samba_handlers(ldb), LDB_SUCCESS,
				 "Failed to register Samba handlers");

	ldb_set_utf8_fns(ldb, NULL, wrap_casefold);

	/* Test DN+Binary behaviour */
	torture_assert(torture, syntax = ldb_samba_syntax_by_name(ldb, DSDB_SYNTAX_BINARY_DN), 
		       "Failed to get DN+Binary schema attribute");
	/* Test compare with different case of HEX string */
	dn1 = data_blob_string_const("B:6:abcdef:dc=samba,dc=org");
	dn2 = data_blob_string_const("B:6:ABCDef:dc=samba,dc=org");
	torture_assert_int_equal(torture, 
				 syntax->comparison_fn(ldb, mem_ctx, &dn1, &dn2), 0,
				 "Failed to compare different case of binary in DN+Binary");
	torture_assert_int_equal(torture, 
				 syntax->canonicalise_fn(ldb, mem_ctx, &dn1, &dn3), 0,
				 "Failed to canonicalise DN+Binary");
	torture_assert_data_blob_equal(torture, dn3, data_blob_string_const("B:6:ABCDEF:DC=SAMBA,DC=ORG"), 
				       "Failed to canonicalise DN+Binary");
	/* Test compare with different case of DN */
	dn1 = data_blob_string_const("B:6:abcdef:dc=samba,dc=org");
	dn2 = data_blob_string_const("B:6:abcdef:dc=SAMBa,dc=ORg");
	torture_assert_int_equal(torture, 
				 syntax->comparison_fn(ldb, mem_ctx, &dn1, &dn2), 0,
				 "Failed to compare different case of DN in DN+Binary");
	
	/* Test compare (false) with binary and non-binary prefix */
	dn1 = data_blob_string_const("B:6:abcdef:dc=samba,dc=org");
	dn2 = data_blob_string_const("dc=samba,dc=org");
	torture_assert(torture, 
		       syntax->comparison_fn(ldb, mem_ctx, &dn1, &dn2) != 0,
		       "compare of binary+dn an dn should have failed");

	/* Test DN+String behaviour */
	torture_assert(torture, syntax = ldb_samba_syntax_by_name(ldb, DSDB_SYNTAX_STRING_DN), 
		       "Failed to get DN+String schema attribute");
	
	/* Test compare with different case of string */
	dn1 = data_blob_string_const("S:8:hihohiho:dc=samba,dc=org");
	dn2 = data_blob_string_const("S:8:HIHOHIHO:dc=samba,dc=org");
	torture_assert(torture, 
		       syntax->comparison_fn(ldb, mem_ctx, &dn1, &dn2) != 0,
		       "compare of string+dn an different case of string+dn should have failed");

	/* Test compare with different case of DN */
	dn1 = data_blob_string_const("S:8:hihohiho:dc=samba,dc=org");
	dn2 = data_blob_string_const("S:8:hihohiho:dc=SAMBA,dc=org");
	torture_assert_int_equal(torture, 
				 syntax->comparison_fn(ldb, mem_ctx, &dn1, &dn2), 0,
				 "Failed to compare different case of DN in DN+String");
	torture_assert_int_equal(torture, 
				 syntax->canonicalise_fn(ldb, mem_ctx, &dn1, &dn3), 0,
				 "Failed to canonicalise DN+String");
	torture_assert_data_blob_equal(torture, dn3, data_blob_string_const("S:8:hihohiho:DC=SAMBA,DC=ORG"), 
				       "Failed to canonicalise DN+String");

	/* Test compare (false) with string and non-string prefix */
	dn1 = data_blob_string_const("S:6:abcdef:dc=samba,dc=org");
	dn2 = data_blob_string_const("dc=samba,dc=org");
	torture_assert(torture, 
		       syntax->comparison_fn(ldb, mem_ctx, &dn1, &dn2) != 0,
		       "compare of string+dn an dn should have failed");

	talloc_free(mem_ctx);
	return true;
}

static bool torture_dsdb_dn_valid(struct torture_context *torture)
{
	TALLOC_CTX *mem_ctx = talloc_new(torture);
	struct ldb_context *ldb;
	struct ldb_dn *dn;
	struct dsdb_dn *dsdb_dn;

	struct ldb_val val;

	DATA_BLOB abcd_blob = data_blob_talloc(mem_ctx, "\xa\xb\xc\xd", 4);

	torture_assert(torture, 
		       ldb = ldb_init(mem_ctx, torture->ev),
		       "Failed to init ldb");

	torture_assert_int_equal(torture, 
				 ldb_register_samba_handlers(ldb), LDB_SUCCESS,
				 "Failed to register Samba handlers");

	ldb_set_utf8_fns(ldb, NULL, wrap_casefold);

	/* Check behaviour of a normal DN */
	torture_assert(torture, 
		       dn = ldb_dn_new(mem_ctx, ldb, NULL), 
		       "Failed to create a NULL DN");
	torture_assert(torture, 
		       ldb_dn_validate(dn),
		       "Failed to validate NULL DN");
	torture_assert(torture, 
		       ldb_dn_add_base_fmt(dn, "dc=org"), 
		       "Failed to add base DN");
	torture_assert(torture, 
		       ldb_dn_add_child_fmt(dn, "dc=samba"), 
		       "Failed to add base DN");
	torture_assert_str_equal(torture, ldb_dn_get_linearized(dn), "dc=samba,dc=org", 
				 "linearized DN incorrect");
	torture_assert(torture, dsdb_dn =  dsdb_dn_construct(mem_ctx, dn, data_blob_null, LDB_SYNTAX_DN), 
		"Failed to build dsdb dn");
	torture_assert_str_equal(torture, dsdb_dn_get_extended_linearized(mem_ctx, dsdb_dn, 0), "dc=samba,dc=org", 
				 "extended linearized DN incorrect");
	torture_assert_str_equal(torture, dsdb_dn_get_linearized(mem_ctx, dsdb_dn), "dc=samba,dc=org", 
				 "linearized DN incorrect");
	torture_assert_str_equal(torture, dsdb_dn_get_casefold(mem_ctx, dsdb_dn), "DC=SAMBA,DC=ORG", 
				 "casefold DN incorrect");


	/* Test constructing a binary DN */
	torture_assert(torture, dsdb_dn = dsdb_dn_construct(mem_ctx, dn, abcd_blob, DSDB_SYNTAX_BINARY_DN), 
		       "Failed to build binary dsdb dn");
	torture_assert_str_equal(torture, dsdb_dn_get_extended_linearized(mem_ctx, dsdb_dn, 0), "B:8:0A0B0C0D:dc=samba,dc=org", 
				 "extended linearized DN incorrect");
	torture_assert_str_equal(torture, dsdb_dn_get_linearized(mem_ctx, dsdb_dn), "B:8:0A0B0C0D:dc=samba,dc=org", 
				 "linearized DN incorrect");
	torture_assert_str_equal(torture, dsdb_dn_get_casefold(mem_ctx, dsdb_dn), "B:8:0A0B0C0D:DC=SAMBA,DC=ORG", 
				 "casefold DN incorrect");
	torture_assert_int_equal(torture, dsdb_dn->extra_part.length, 4, "length of extra-part should be 2");
		

	/* Test constructing a string DN */
	torture_assert(torture, dsdb_dn =  dsdb_dn_construct(mem_ctx, dn, data_blob_talloc(mem_ctx, "hello", 5), DSDB_SYNTAX_STRING_DN),
		       "Failed to build string dsdb dn");
	torture_assert_str_equal(torture, dsdb_dn_get_extended_linearized(mem_ctx, dsdb_dn, 0), "S:5:hello:dc=samba,dc=org", 
				 "extended linearized DN incorrect");
	torture_assert_str_equal(torture, dsdb_dn_get_linearized(mem_ctx, dsdb_dn), "S:5:hello:dc=samba,dc=org", 
				 "linearized DN incorrect");
	torture_assert_str_equal(torture, dsdb_dn_get_casefold(mem_ctx, dsdb_dn), "S:5:hello:DC=SAMBA,DC=ORG", 
				 "casefold DN incorrect");
	torture_assert_int_equal(torture, dsdb_dn->extra_part.length, 5, "length of extra-part should be 5");


	/* Test compose of binary+DN */
	val = data_blob_string_const("B:0::CN=Zer0,DC=SAMBA,DC=org");
	torture_assert(torture,
		       dsdb_dn = dsdb_dn_parse(mem_ctx, ldb, &val,
				     DSDB_SYNTAX_BINARY_DN),
		       "Failed to create a DN with a zero binary part in it");
	torture_assert_int_equal(torture, dsdb_dn->extra_part.length, 0, "length of extra-part should be 0");
	torture_assert_str_equal(torture, dsdb_dn_get_extended_linearized(mem_ctx, dsdb_dn, 0), "B:0::CN=Zer0,DC=SAMBA,DC=org", 
				 "extended linearized DN incorrect");
	torture_assert_str_equal(torture, dsdb_dn_get_linearized(mem_ctx, dsdb_dn), "B:0::CN=Zer0,DC=SAMBA,DC=org", 
				 "linearized DN incorrect");
	torture_assert_str_equal(torture, dsdb_dn_get_casefold(mem_ctx, dsdb_dn), "B:0::CN=ZER0,DC=SAMBA,DC=ORG", 
				 "casefold DN incorrect");

	/* Test parse of binary DN */
	val = data_blob_string_const("B:8:abcdabcd:CN=4,DC=Samba,DC=org");
	torture_assert(torture,
		       dsdb_dn = dsdb_dn_parse(mem_ctx, ldb, &val,
				     DSDB_SYNTAX_BINARY_DN),
		       "Failed to create a DN with a binary part in it");
	torture_assert_int_equal(torture, dsdb_dn->extra_part.length, 4, "length of extra-part should be 4");

	torture_assert_str_equal(torture, dsdb_dn_get_extended_linearized(mem_ctx, dsdb_dn, 0), "B:8:ABCDABCD:CN=4,DC=Samba,DC=org", 
				 "extended linearized DN incorrect");
	torture_assert_str_equal(torture, dsdb_dn_get_linearized(mem_ctx, dsdb_dn), "B:8:ABCDABCD:CN=4,DC=Samba,DC=org", 
				 "linearized DN incorrect");
	torture_assert_str_equal(torture, dsdb_dn_get_casefold(mem_ctx, dsdb_dn), "B:8:ABCDABCD:CN=4,DC=SAMBA,DC=ORG", 
				 "casefold DN incorrect");

	/* Test parse of string+DN */
	val = data_blob_string_const("S:8:Goodbye!:CN=S,DC=Samba,DC=org");
	torture_assert(torture,
		       dsdb_dn = dsdb_dn_parse(mem_ctx, ldb, &val,
				     DSDB_SYNTAX_STRING_DN),
		       "Failed to create a DN with a string part in it");
	torture_assert_int_equal(torture, dsdb_dn->extra_part.length, 8, "length of extra-part should be 8");
	torture_assert_str_equal(torture, dsdb_dn_get_extended_linearized(mem_ctx, dsdb_dn, 0), "S:8:Goodbye!:CN=S,DC=Samba,DC=org", 
				 "extended linearized DN incorrect");

	/* Test that the linearised DN is the postfix of the lineairsed dsdb_dn */
	torture_assert_str_equal(torture, ldb_dn_get_extended_linearized(mem_ctx, dsdb_dn->dn, 0), "CN=S,DC=Samba,DC=org", 
				 "extended linearized DN incorrect");
	torture_assert_str_equal(torture, dsdb_dn_get_linearized(mem_ctx, dsdb_dn), "S:8:Goodbye!:CN=S,DC=Samba,DC=org", 
				 "linearized DN incorrect");
	torture_assert_str_equal(torture, ldb_dn_get_linearized(dsdb_dn->dn), "CN=S,DC=Samba,DC=org", 
				 "linearized DN incorrect");
	torture_assert_str_equal(torture, dsdb_dn_get_casefold(mem_ctx, dsdb_dn), "S:8:Goodbye!:CN=S,DC=SAMBA,DC=ORG", 
				 "casefold DN incorrect");

	/* Test that the casefold DN is the postfix of the casefolded dsdb_dn */
	torture_assert_str_equal(torture, ldb_dn_get_casefold(dsdb_dn->dn), "CN=S,DC=SAMBA,DC=ORG", 
				 "casefold DN incorrect");

	talloc_free(mem_ctx);
	return true;
}

static bool torture_dsdb_dn_invalid(struct torture_context *torture)
{
	TALLOC_CTX *mem_ctx = talloc_new(torture);
	struct ldb_context *ldb;
	struct ldb_val val;

	torture_assert(torture, 
		       ldb = ldb_init(mem_ctx, torture->ev),
		       "Failed to init ldb");

	torture_assert_int_equal(torture, 
				 ldb_register_samba_handlers(ldb), LDB_SUCCESS,
				 "Failed to register Samba handlers");

	ldb_set_utf8_fns(ldb, NULL, wrap_casefold);

	/* Check behaviour of a normal DN */
	val = data_blob_string_const("samba,dc=org");
	torture_assert(torture, 
		       dsdb_dn_parse(mem_ctx, ldb, &val, LDB_SYNTAX_DN) == NULL, 
		       "Should have failed to create a 'normal' invalid DN");

	/* Test invalid binary DNs */
	val = data_blob_string_const("B:5:AB:dc=samba,dc=org");
	torture_assert(torture, 
		       dsdb_dn_parse(mem_ctx, ldb, &val,
				     DSDB_SYNTAX_BINARY_DN) == NULL, 
		       "Should have Failed to create an invalid 'binary' DN");
	val = data_blob_string_const("B:5:ABCDEFG:dc=samba,dc=org");
	torture_assert(torture, 
		       dsdb_dn_parse(mem_ctx, ldb, &val,
				     DSDB_SYNTAX_BINARY_DN) == NULL, 
		       "Should have Failed to create an invalid 'binary' DN");
	val = data_blob_string_const("B:10:AB:dc=samba,dc=org");
	torture_assert(torture, 
		       dsdb_dn_parse(mem_ctx, ldb, &val,
				     DSDB_SYNTAX_BINARY_DN) == NULL, 
		       "Should have Failed to create an invalid 'binary' DN");
	val = data_blob_string_const("B:4:0xAB:dc=samba,dc=org");
	torture_assert(torture, 
		       dsdb_dn_parse(mem_ctx, ldb, &val,
				     DSDB_SYNTAX_BINARY_DN) == NULL, 
		       "Should have Failed to create an invalid 0x preifx 'binary' DN");
	val = data_blob_string_const("B:2:0xAB:dc=samba,dc=org");
	torture_assert(torture, 
		       dsdb_dn_parse(mem_ctx, ldb, &val,
				     DSDB_SYNTAX_BINARY_DN) == NULL, 
		       "Should have Failed to create an invalid 0x preifx 'binary' DN");
	val = data_blob_string_const("B:10:XXXXXXXXXX:dc=samba,dc=org");
	torture_assert(torture, 
		       dsdb_dn_parse(mem_ctx, ldb, &val,
				     DSDB_SYNTAX_BINARY_DN) == NULL, 
		       "Should have Failed to create an invalid 'binary' DN");

	val = data_blob_string_const("B:60::dc=samba,dc=org");
	torture_assert(torture, 
		       dsdb_dn_parse(mem_ctx, ldb, &val,
				     DSDB_SYNTAX_BINARY_DN) == NULL, 
		       "Should have Failed to create an invalid 'binary' DN");

	/* Test invalid string DNs */
	val = data_blob_string_const("S:5:hi:dc=samba,dc=org");
	torture_assert(torture, 
		       dsdb_dn_parse(mem_ctx, ldb, &val,
				     DSDB_SYNTAX_STRING_DN) == NULL, 
		       "Should have Failed to create an invalid 'string' DN");
	val = data_blob_string_const("S:5:hihohiho:dc=samba,dc=org");
	torture_assert(torture, 
		       dsdb_dn_parse(mem_ctx, ldb, &val,
				     DSDB_SYNTAX_STRING_DN) == NULL, 
		       "Should have Failed to create an invalid 'string' DN");

	val = data_blob_string_const("<SID=" DSDB_DN_TEST_SID">;dc=samba,dc=org");
	torture_assert(torture, 
		       dsdb_dn_parse(mem_ctx, ldb, &val, DSDB_SYNTAX_BINARY_DN) == NULL, 
		       "Should have failed to create an 'extended' DN marked as a binary DN");

	/* Check DN based on MS-ADTS:3.1.1.5.1.2 Naming Constraints*/
	val = data_blob_string_const("CN=New\nLine,DC=SAMBA,DC=org");

	/* changed to a warning until we understand the DEL: DNs */
	if (dsdb_dn_parse(mem_ctx, ldb, &val, LDB_SYNTAX_DN) != NULL) {
		torture_warning(torture,
				"Should have Failed to create a DN with 0xA in it");
	}

	val = data_blob_string_const("B:4:ABAB:CN=New\nLine,DC=SAMBA,DC=org");
	torture_assert(torture,
		       dsdb_dn_parse(mem_ctx, ldb, &val, LDB_SYNTAX_DN) == NULL,
		       "Should have Failed to create a DN with 0xA in it");

	val = data_blob_const("CN=Zer\0,DC=SAMBA,DC=org", 23);
	torture_assert(torture,
		       dsdb_dn_parse(mem_ctx, ldb, &val, LDB_SYNTAX_DN) == NULL,
		       "Should have Failed to create a DN with 0x0 in it");

	val = data_blob_const("B:4:ABAB:CN=Zer\0,DC=SAMBA,DC=org", 23+9);
	torture_assert(torture,
		       dsdb_dn_parse(mem_ctx, ldb, &val, DSDB_SYNTAX_BINARY_DN) == NULL,
		       "Should have Failed to create a DN with 0x0 in it");

	return true;
}

struct torture_suite *torture_dsdb_dn(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "dsdb.dn");

	if (suite == NULL) {
		return NULL;
	}

	torture_suite_add_simple_test(suite, "valid", torture_dsdb_dn_valid);
	torture_suite_add_simple_test(suite, "invalid", torture_dsdb_dn_invalid);
	torture_suite_add_simple_test(suite, "attrs", torture_dsdb_dn_attrs);

	suite->description = talloc_strdup(suite, "DSDB DN tests");

	return suite;
}
