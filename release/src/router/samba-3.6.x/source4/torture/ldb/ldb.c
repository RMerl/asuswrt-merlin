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

static const char *sid = "S-1-5-21-4177067393-1453636373-93818737";
static const char *hex_sid = "01040000000000051500000081fdf8f815bba456718f9705";
static const char *guid = "975ac5fa-35d9-431d-b86a-845bcd34fff9";
static const char *guid2 = "{975ac5fa-35d9-431d-b86a-845bcd34fff9}";
static const char *hex_guid = "fac55a97d9351d43b86a845bcd34fff9";

static const char *prefix_map_newline = "2:1.2.840.113556.1.2\n5:2.16.840.1.101.2.2.3";
static const char *prefix_map_semi = "2:1.2.840.113556.1.2;5:2.16.840.1.101.2.2.3";

static bool torture_ldb_attrs(struct torture_context *torture)
{
	TALLOC_CTX *mem_ctx = talloc_new(torture);
	struct ldb_context *ldb;
	const struct ldb_schema_attribute *attr;
	struct ldb_val string_sid_blob, binary_sid_blob;
	struct ldb_val string_guid_blob, string_guid_blob2, binary_guid_blob;
	struct ldb_val string_prefix_map_newline_blob, string_prefix_map_semi_blob, string_prefix_map_blob;
	struct ldb_val prefix_map_blob;

	DATA_BLOB sid_blob = strhex_to_data_blob(mem_ctx, hex_sid);
	DATA_BLOB guid_blob = strhex_to_data_blob(mem_ctx, hex_guid);

	torture_assert(torture, 
		       ldb = ldb_init(mem_ctx, torture->ev),
		       "Failed to init ldb");

	torture_assert_int_equal(torture, 
				 ldb_register_samba_handlers(ldb), LDB_SUCCESS,
				 "Failed to register Samba handlers");

	ldb_set_utf8_fns(ldb, NULL, wrap_casefold);

	/* Test SID behaviour */
	torture_assert(torture, attr = ldb_schema_attribute_by_name(ldb, "objectSid"), 
		       "Failed to get objectSid schema attribute");
	
	string_sid_blob = data_blob_string_const(sid);

	torture_assert_int_equal(torture, 
				 attr->syntax->ldif_read_fn(ldb, mem_ctx, 
							    &string_sid_blob, &binary_sid_blob), 0,
				 "Failed to parse string SID");
	
	torture_assert_data_blob_equal(torture, binary_sid_blob, sid_blob, 
				       "Read SID into blob form failed");
	
	torture_assert_int_equal(torture, 
				 attr->syntax->ldif_read_fn(ldb, mem_ctx, 
							    &sid_blob, &binary_sid_blob), -1,
				 "Should have failed to parse binary SID");
	
	torture_assert_int_equal(torture, 
				 attr->syntax->ldif_write_fn(ldb, mem_ctx, &binary_sid_blob, &string_sid_blob), 0,
				 "Failed to parse binary SID");
	
	torture_assert_data_blob_equal(torture, 
				       string_sid_blob, data_blob_string_const(sid),
				       "Write SID into string form failed");
	
	torture_assert_int_equal(torture, 
				 attr->syntax->comparison_fn(ldb, mem_ctx, &binary_sid_blob, &string_sid_blob), 0,
				 "Failed to compare binary and string SID");
	
	torture_assert_int_equal(torture, 
				 attr->syntax->comparison_fn(ldb, mem_ctx, &string_sid_blob, &binary_sid_blob), 0,
				 "Failed to compare string and binary binary SID");
	
	torture_assert_int_equal(torture, 
				 attr->syntax->comparison_fn(ldb, mem_ctx, &string_sid_blob, &string_sid_blob), 0,
				 "Failed to compare string and string SID");
	
	torture_assert_int_equal(torture, 
				 attr->syntax->comparison_fn(ldb, mem_ctx, &binary_sid_blob, &binary_sid_blob), 0,
				 "Failed to compare binary and binary SID");
	
	torture_assert(torture, attr->syntax->comparison_fn(ldb, mem_ctx, &guid_blob, &binary_sid_blob) != 0,
		       "Failed to distinguish binary GUID and binary SID");


	/* Test GUID behaviour */
	torture_assert(torture, attr = ldb_schema_attribute_by_name(ldb, "objectGUID"), 
		       "Failed to get objectGUID schema attribute");
	
	string_guid_blob = data_blob_string_const(guid);

	torture_assert_int_equal(torture, 
				 attr->syntax->ldif_read_fn(ldb, mem_ctx, 
							    &string_guid_blob, &binary_guid_blob), 0,
				 "Failed to parse string GUID");
	
	torture_assert_data_blob_equal(torture, binary_guid_blob, guid_blob, 
				       "Read GUID into blob form failed");
	
	string_guid_blob2 = data_blob_string_const(guid2);
	
	torture_assert_int_equal(torture, 
				 attr->syntax->ldif_read_fn(ldb, mem_ctx, 
							    &string_guid_blob2, &binary_guid_blob), 0,
				 "Failed to parse string GUID");
	
	torture_assert_data_blob_equal(torture, binary_guid_blob, guid_blob, 
				       "Read GUID into blob form failed");
	
	torture_assert_int_equal(torture, 
				 attr->syntax->ldif_read_fn(ldb, mem_ctx, 
							    &guid_blob, &binary_guid_blob), 0,
				 "Failed to parse binary GUID");
	
	torture_assert_data_blob_equal(torture, binary_guid_blob, guid_blob, 
				       "Read GUID into blob form failed");
	
	torture_assert_int_equal(torture, 
				 attr->syntax->ldif_write_fn(ldb, mem_ctx, &binary_guid_blob, &string_guid_blob), 0,
				 "Failed to print binary GUID as string");

	torture_assert_data_blob_equal(torture, string_sid_blob, data_blob_string_const(sid),
				       "Write SID into string form failed");
	
	torture_assert_int_equal(torture, 
				 attr->syntax->comparison_fn(ldb, mem_ctx, &binary_guid_blob, &string_guid_blob), 0,
				 "Failed to compare binary and string GUID");
	
	torture_assert_int_equal(torture, 
				 attr->syntax->comparison_fn(ldb, mem_ctx, &string_guid_blob, &binary_guid_blob), 0,
				 "Failed to compare string and binary binary GUID");
	
	torture_assert_int_equal(torture, 
				 attr->syntax->comparison_fn(ldb, mem_ctx, &string_guid_blob, &string_guid_blob), 0,
				 "Failed to compare string and string GUID");
	
	torture_assert_int_equal(torture, 
				 attr->syntax->comparison_fn(ldb, mem_ctx, &binary_guid_blob, &binary_guid_blob), 0,
				 "Failed to compare binary and binary GUID");
	
	string_prefix_map_newline_blob = data_blob_string_const(prefix_map_newline);
	
	string_prefix_map_semi_blob = data_blob_string_const(prefix_map_semi);
	
	/* Test prefixMap behaviour */
	torture_assert(torture, attr = ldb_schema_attribute_by_name(ldb, "prefixMap"), 
		       "Failed to get prefixMap schema attribute");
	
	torture_assert_int_equal(torture, 
				 attr->syntax->comparison_fn(ldb, mem_ctx, &string_prefix_map_newline_blob, &string_prefix_map_semi_blob), 0,
				 "Failed to compare prefixMap with newlines and prefixMap with semicolons");
	
	torture_assert_int_equal(torture, 
				 attr->syntax->ldif_read_fn(ldb, mem_ctx, &string_prefix_map_newline_blob, &prefix_map_blob), 0,
				 "Failed to read prefixMap with newlines");
	torture_assert_int_equal(torture, 
				 attr->syntax->comparison_fn(ldb, mem_ctx, &string_prefix_map_newline_blob, &prefix_map_blob), 0,
				 "Failed to compare prefixMap with newlines and prefixMap binary");
	
	torture_assert_int_equal(torture, 
				 attr->syntax->ldif_write_fn(ldb, mem_ctx, &prefix_map_blob, &string_prefix_map_blob), 0,
				 "Failed to write prefixMap");
	torture_assert_int_equal(torture, 
				 attr->syntax->comparison_fn(ldb, mem_ctx, &string_prefix_map_blob, &prefix_map_blob), 0,
				 "Failed to compare prefixMap ldif write and prefixMap binary");
	
	torture_assert_data_blob_equal(torture, string_prefix_map_blob, string_prefix_map_semi_blob,
		"Failed to compare prefixMap ldif write and prefixMap binary");
	


	talloc_free(mem_ctx);
	return true;
}

static bool torture_ldb_dn_attrs(struct torture_context *torture)
{
	TALLOC_CTX *mem_ctx = talloc_new(torture);
	struct ldb_context *ldb;
	const struct ldb_dn_extended_syntax *attr;
	struct ldb_val string_sid_blob, binary_sid_blob;
	struct ldb_val string_guid_blob, binary_guid_blob;
	struct ldb_val hex_sid_blob, hex_guid_blob;

	DATA_BLOB sid_blob = strhex_to_data_blob(mem_ctx, hex_sid);
	DATA_BLOB guid_blob = strhex_to_data_blob(mem_ctx, hex_guid);

	torture_assert(torture, 
		       ldb = ldb_init(mem_ctx, torture->ev),
		       "Failed to init ldb");

	torture_assert_int_equal(torture, 
				 ldb_register_samba_handlers(ldb), LDB_SUCCESS,
				 "Failed to register Samba handlers");

	ldb_set_utf8_fns(ldb, NULL, wrap_casefold);

	/* Test SID behaviour */
	torture_assert(torture, attr = ldb_dn_extended_syntax_by_name(ldb, "SID"), 
		       "Failed to get SID DN syntax");
	
	string_sid_blob = data_blob_string_const(sid);

	torture_assert_int_equal(torture, 
				 attr->read_fn(ldb, mem_ctx, 
					       &string_sid_blob, &binary_sid_blob), 0,
				 "Failed to parse string SID");
	
	torture_assert_data_blob_equal(torture, binary_sid_blob, sid_blob, 
				       "Read SID into blob form failed");

	hex_sid_blob = data_blob_string_const(hex_sid);
	
	torture_assert_int_equal(torture, 
				 attr->read_fn(ldb, mem_ctx, 
					       &hex_sid_blob, &binary_sid_blob), 0,
				 "Failed to parse HEX SID");
	
	torture_assert_data_blob_equal(torture, binary_sid_blob, sid_blob, 
				       "Read SID into blob form failed");
	
	torture_assert_int_equal(torture, 
				 attr->read_fn(ldb, mem_ctx, 
					       &sid_blob, &binary_sid_blob), -1,
				 "Should have failed to parse binary SID");
	
	torture_assert_int_equal(torture, 
				 attr->write_hex_fn(ldb, mem_ctx, &sid_blob, &hex_sid_blob), 0,
				 "Failed to parse binary SID");
	
	torture_assert_data_blob_equal(torture, 
				       hex_sid_blob, data_blob_string_const(hex_sid),
				       "Write SID into HEX string form failed");
	
	torture_assert_int_equal(torture, 
				 attr->write_clear_fn(ldb, mem_ctx, &sid_blob, &string_sid_blob), 0,
				 "Failed to parse binary SID");
	
	torture_assert_data_blob_equal(torture, 
				       string_sid_blob, data_blob_string_const(sid),
				       "Write SID into clear string form failed");
	

	/* Test GUID behaviour */
	torture_assert(torture, attr = ldb_dn_extended_syntax_by_name(ldb, "GUID"), 
		       "Failed to get GUID DN syntax");
	
	string_guid_blob = data_blob_string_const(guid);

	torture_assert_int_equal(torture, 
				 attr->read_fn(ldb, mem_ctx, 
					       &string_guid_blob, &binary_guid_blob), 0,
				 "Failed to parse string GUID");
	
	torture_assert_data_blob_equal(torture, binary_guid_blob, guid_blob, 
				       "Read GUID into blob form failed");
	
	hex_guid_blob = data_blob_string_const(hex_guid);
	
	torture_assert_int_equal(torture, 
				 attr->read_fn(ldb, mem_ctx, 
					       &hex_guid_blob, &binary_guid_blob), 0,
				 "Failed to parse HEX GUID");
	
	torture_assert_data_blob_equal(torture, binary_guid_blob, guid_blob, 
				       "Read GUID into blob form failed");
	
	torture_assert_int_equal(torture, 
				 attr->read_fn(ldb, mem_ctx, 
					       &guid_blob, &binary_guid_blob), -1,
				 "Should have failed to parse binary GUID");
	
	torture_assert_int_equal(torture, 
				 attr->write_hex_fn(ldb, mem_ctx, &guid_blob, &hex_guid_blob), 0,
				 "Failed to parse binary GUID");
	
	torture_assert_data_blob_equal(torture, 
				       hex_guid_blob, data_blob_string_const(hex_guid),
				       "Write GUID into HEX string form failed");
	
	torture_assert_int_equal(torture, 
				 attr->write_clear_fn(ldb, mem_ctx, &guid_blob, &string_guid_blob), 0,
				 "Failed to parse binary GUID");
	
	torture_assert_data_blob_equal(torture, 
				       string_guid_blob, data_blob_string_const(guid),
				       "Write GUID into clear string form failed");
	


	talloc_free(mem_ctx);
	return true;
}

static bool torture_ldb_dn_extended(struct torture_context *torture)
{
	TALLOC_CTX *mem_ctx = talloc_new(torture);
	struct ldb_context *ldb;
	struct ldb_dn *dn, *dn2;

	DATA_BLOB sid_blob = strhex_to_data_blob(mem_ctx, hex_sid);
	DATA_BLOB guid_blob = strhex_to_data_blob(mem_ctx, hex_guid);

	const char *dn_str = "cn=admin,cn=users,dc=samba,dc=org";

	torture_assert(torture, 
		       ldb = ldb_init(mem_ctx, torture->ev),
		       "Failed to init ldb");

	torture_assert_int_equal(torture, 
				 ldb_register_samba_handlers(ldb), LDB_SUCCESS,
				 "Failed to register Samba handlers");

	ldb_set_utf8_fns(ldb, NULL, wrap_casefold);

	/* Check behaviour of a normal DN */
	torture_assert(torture, 
		       dn = ldb_dn_new(mem_ctx, ldb, dn_str), 
		       "Failed to create a 'normal' DN");

	torture_assert(torture, 
		       ldb_dn_validate(dn),
		       "Failed to validate 'normal' DN");

	torture_assert(torture, ldb_dn_has_extended(dn) == false, 
		       "Should not find plain DN to be 'extended'");

	torture_assert(torture, ldb_dn_get_extended_component(dn, "SID") == NULL, 
		       "Should not find an SID on plain DN");

	torture_assert(torture, ldb_dn_get_extended_component(dn, "GUID") == NULL, 
		       "Should not find an GUID on plain DN");
	
	torture_assert(torture, ldb_dn_get_extended_component(dn, "WKGUID") == NULL, 
		       "Should not find an WKGUID on plain DN");
	
	/* Now make an extended DN */
	torture_assert(torture, 
		       dn = ldb_dn_new_fmt(mem_ctx, ldb, "<GUID=%s>;<SID=%s>;%s",
					   guid, sid, dn_str), 
		       "Failed to create an 'extended' DN");

	torture_assert(torture, 
		       dn2 = ldb_dn_copy(mem_ctx, dn), 
		       "Failed to copy the 'extended' DN");
	talloc_free(dn);
	dn = dn2;

	torture_assert(torture, 
		       ldb_dn_validate(dn),
		       "Failed to validate 'extended' DN");

	torture_assert(torture, ldb_dn_has_extended(dn) == true, 
		       "Should find extended DN to be 'extended'");

	torture_assert(torture, ldb_dn_get_extended_component(dn, "SID") != NULL, 
		       "Should find an SID on extended DN");

	torture_assert(torture, ldb_dn_get_extended_component(dn, "GUID") != NULL, 
		       "Should find an GUID on extended DN");
	
	torture_assert_data_blob_equal(torture, *ldb_dn_get_extended_component(dn, "SID"), sid_blob, 
				       "Extended DN SID incorect");

	torture_assert_data_blob_equal(torture, *ldb_dn_get_extended_component(dn, "GUID"), guid_blob, 
				       "Extended DN GUID incorect");

	torture_assert_str_equal(torture, ldb_dn_get_linearized(dn), dn_str, 
				 "linearized DN incorrect");

	torture_assert_str_equal(torture, ldb_dn_get_casefold(dn), strupper_talloc(mem_ctx, dn_str), 
				 "casefolded DN incorrect");

	torture_assert_str_equal(torture, ldb_dn_get_component_name(dn, 0), "cn", 
				 "componet zero incorrect");

	torture_assert_data_blob_equal(torture, *ldb_dn_get_component_val(dn, 0), data_blob_string_const("admin"), 
				 "componet zero incorrect");

	torture_assert_str_equal(torture, ldb_dn_get_extended_linearized(mem_ctx, dn, 1),
				 talloc_asprintf(mem_ctx, "<GUID=%s>;<SID=%s>;%s", 
						 guid, sid, dn_str),
				 "Clear extended linearized DN incorrect");

	torture_assert_str_equal(torture, ldb_dn_get_extended_linearized(mem_ctx, dn, 0),
				 talloc_asprintf(mem_ctx, "<GUID=%s>;<SID=%s>;%s", 
						 hex_guid, hex_sid, dn_str),
				 "HEX extended linearized DN incorrect");

	torture_assert(torture, ldb_dn_remove_child_components(dn, 1) == true,
				 "Failed to remove DN child");
		       
	torture_assert(torture, ldb_dn_has_extended(dn) == false, 
		       "Extended DN flag should be cleared after child element removal");
	
	torture_assert(torture, ldb_dn_get_extended_component(dn, "SID") == NULL, 
		       "Should not find an SID on DN");

	torture_assert(torture, ldb_dn_get_extended_component(dn, "GUID") == NULL, 
		       "Should not find an GUID on DN");


	/* TODO:  test setting these in the other order, and ensure it still comes out 'GUID first' */
	torture_assert_int_equal(torture, ldb_dn_set_extended_component(dn, "GUID", &guid_blob), 0, 
		       "Failed to set a GUID on DN");
	
	torture_assert_int_equal(torture, ldb_dn_set_extended_component(dn, "SID", &sid_blob), 0, 
		       "Failed to set a SID on DN");

	torture_assert_data_blob_equal(torture, *ldb_dn_get_extended_component(dn, "SID"), sid_blob, 
				       "Extended DN SID incorect");

	torture_assert_data_blob_equal(torture, *ldb_dn_get_extended_component(dn, "GUID"), guid_blob, 
				       "Extended DN GUID incorect");

	torture_assert_str_equal(torture, ldb_dn_get_linearized(dn), "cn=users,dc=samba,dc=org", 
				 "linearized DN incorrect");

	torture_assert_str_equal(torture, ldb_dn_get_extended_linearized(mem_ctx, dn, 1),
				 talloc_asprintf(mem_ctx, "<GUID=%s>;<SID=%s>;%s", 
						 guid, sid, "cn=users,dc=samba,dc=org"),
				 "Clear extended linearized DN incorrect");

	torture_assert_str_equal(torture, ldb_dn_get_extended_linearized(mem_ctx, dn, 0),
				 talloc_asprintf(mem_ctx, "<GUID=%s>;<SID=%s>;%s", 
						 hex_guid, hex_sid, "cn=users,dc=samba,dc=org"),
				 "HEX extended linearized DN incorrect");

	/* Now check a 'just GUID' DN (clear format) */
	torture_assert(torture, 
		       dn = ldb_dn_new_fmt(mem_ctx, ldb, "<GUID=%s>",
					   guid), 
		       "Failed to create an 'extended' DN");

	torture_assert(torture, 
		       ldb_dn_validate(dn),
		       "Failed to validate 'extended' DN");

	torture_assert(torture, ldb_dn_has_extended(dn) == true, 
		       "Should find extended DN to be 'extended'");

	torture_assert(torture, ldb_dn_get_extended_component(dn, "SID") == NULL, 
		       "Should not find an SID on this DN");

	torture_assert_int_equal(torture, ldb_dn_get_comp_num(dn), 0, 
		       "Should not find an 'normal' componet on this DN");

	torture_assert(torture, ldb_dn_get_extended_component(dn, "GUID") != NULL, 
		       "Should find an GUID on this DN");
	
	torture_assert_data_blob_equal(torture, *ldb_dn_get_extended_component(dn, "GUID"), guid_blob, 
				       "Extended DN GUID incorect");

	torture_assert_str_equal(torture, ldb_dn_get_linearized(dn), "", 
				 "linearized DN incorrect");

	torture_assert_str_equal(torture, ldb_dn_get_extended_linearized(mem_ctx, dn, 1),
				 talloc_asprintf(mem_ctx, "<GUID=%s>", 
						 guid),
				 "Clear extended linearized DN incorrect");

	torture_assert_str_equal(torture, ldb_dn_get_extended_linearized(mem_ctx, dn, 0),
				 talloc_asprintf(mem_ctx, "<GUID=%s>", 
						 hex_guid),
				 "HEX extended linearized DN incorrect");

	/* Now check a 'just GUID' DN (HEX format) */
	torture_assert(torture, 
		       dn = ldb_dn_new_fmt(mem_ctx, ldb, "<GUID=%s>",
					   hex_guid), 
		       "Failed to create an 'extended' DN");

	torture_assert(torture, 
		       ldb_dn_validate(dn),
		       "Failed to validate 'extended' DN");

	torture_assert(torture, ldb_dn_has_extended(dn) == true, 
		       "Should find extended DN to be 'extended'");

	torture_assert(torture, ldb_dn_get_extended_component(dn, "SID") == NULL, 
		       "Should not find an SID on this DN");

	torture_assert(torture, ldb_dn_get_extended_component(dn, "GUID") != NULL, 
		       "Should find an GUID on this DN");
	
	torture_assert_data_blob_equal(torture, *ldb_dn_get_extended_component(dn, "GUID"), guid_blob, 
				       "Extended DN GUID incorect");

	torture_assert_str_equal(torture, ldb_dn_get_linearized(dn), "", 
				 "linearized DN incorrect");

	/* Now check a 'just SID' DN (clear format) */
	torture_assert(torture, 
		       dn = ldb_dn_new_fmt(mem_ctx, ldb, "<SID=%s>",
					   sid), 
		       "Failed to create an 'extended' DN");

	torture_assert(torture, 
		       ldb_dn_validate(dn),
		       "Failed to validate 'extended' DN");

	torture_assert(torture, ldb_dn_has_extended(dn) == true, 
		       "Should find extended DN to be 'extended'");

	torture_assert(torture, ldb_dn_get_extended_component(dn, "GUID") == NULL, 
		       "Should not find an SID on this DN");

	torture_assert(torture, ldb_dn_get_extended_component(dn, "SID") != NULL, 
		       "Should find an SID on this DN");
	
	torture_assert_data_blob_equal(torture, *ldb_dn_get_extended_component(dn, "SID"), sid_blob, 
				       "Extended DN SID incorect");

	torture_assert_str_equal(torture, ldb_dn_get_linearized(dn), "", 
				 "linearized DN incorrect");

	torture_assert_str_equal(torture, ldb_dn_get_extended_linearized(mem_ctx, dn, 1),
				 talloc_asprintf(mem_ctx, "<SID=%s>", 
						 sid),
				 "Clear extended linearized DN incorrect");

	torture_assert_str_equal(torture, ldb_dn_get_extended_linearized(mem_ctx, dn, 0),
				 talloc_asprintf(mem_ctx, "<SID=%s>", 
						 hex_sid),
				 "HEX extended linearized DN incorrect");

	/* Now check a 'just SID' DN (HEX format) */
	torture_assert(torture, 
		       dn = ldb_dn_new_fmt(mem_ctx, ldb, "<SID=%s>",
					   hex_sid), 
		       "Failed to create an 'extended' DN");

	torture_assert(torture, 
		       ldb_dn_validate(dn),
		       "Failed to validate 'extended' DN");

	torture_assert(torture, ldb_dn_has_extended(dn) == true, 
		       "Should find extended DN to be 'extended'");

	torture_assert(torture, ldb_dn_get_extended_component(dn, "GUID") == NULL, 
		       "Should not find an SID on this DN");

	torture_assert(torture, ldb_dn_get_extended_component(dn, "SID") != NULL, 
		       "Should find an SID on this DN");
	
	torture_assert_data_blob_equal(torture, *ldb_dn_get_extended_component(dn, "SID"), sid_blob, 
				       "Extended DN SID incorect");

	torture_assert_str_equal(torture, ldb_dn_get_linearized(dn), "", 
				 "linearized DN incorrect");

	talloc_free(mem_ctx);
	return true;
}


static bool torture_ldb_dn(struct torture_context *torture)
{
	TALLOC_CTX *mem_ctx = talloc_new(torture);
	struct ldb_context *ldb;
	struct ldb_dn *dn;
	struct ldb_dn *child_dn;
	struct ldb_dn *typo_dn;
	struct ldb_dn *special_dn;
	struct ldb_val val;

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

	torture_assert_str_equal(torture, ldb_dn_get_extended_linearized(mem_ctx, dn, 0), "dc=samba,dc=org", 
				 "extended linearized DN incorrect");

	/* Check child DN comparisons */
	torture_assert(torture, 
		       child_dn = ldb_dn_new(mem_ctx, ldb, "CN=users,DC=SAMBA,DC=org"), 
		       "Failed to create child DN");

	torture_assert(torture, 
		       ldb_dn_compare(dn, child_dn) != 0,
		       "Comparison on dc=samba,dc=org and CN=users,DC=SAMBA,DC=org should != 0");

	torture_assert(torture, 
		       ldb_dn_compare_base(child_dn, dn) != 0,
		       "Base Comparison of CN=users,DC=SAMBA,DC=org and dc=samba,dc=org should != 0");

	torture_assert(torture, 
		       ldb_dn_compare_base(dn, child_dn) == 0,
		       "Base Comparison on dc=samba,dc=org and CN=users,DC=SAMBA,DC=org should == 0");

	/* Check comparisons with a truncated DN */
	torture_assert(torture, 
		       typo_dn = ldb_dn_new(mem_ctx, ldb, "c=samba,dc=org"), 
		       "Failed to create 'typo' DN");

	torture_assert(torture, 
		       ldb_dn_compare(dn, typo_dn) != 0,
		       "Comparison on dc=samba,dc=org and c=samba,dc=org should != 0");

	torture_assert(torture, 
		       ldb_dn_compare_base(typo_dn, dn) != 0,
		       "Base Comparison of c=samba,dc=org and dc=samba,dc=org should != 0");

	torture_assert(torture, 
		       ldb_dn_compare_base(dn, typo_dn) != 0,
		       "Base Comparison on dc=samba,dc=org and c=samba,dc=org should != 0");

	/* Check comparisons with a special DN */
	torture_assert(torture,
		       special_dn = ldb_dn_new(mem_ctx, ldb, "@special_dn"),
		       "Failed to create 'special' DN");

	torture_assert(torture,
		       ldb_dn_compare(dn, special_dn) != 0,
		       "Comparison on dc=samba,dc=org and @special_dn should != 0");

	torture_assert(torture,
		       ldb_dn_compare_base(special_dn, dn) > 0,
		       "Base Comparison of @special_dn and dc=samba,dc=org should > 0");

	torture_assert(torture,
		       ldb_dn_compare_base(dn, special_dn) < 0,
		       "Base Comparison on dc=samba,dc=org and @special_dn should < 0");

	/* Check DN based on MS-ADTS:3.1.1.5.1.2 Naming Constraints*/
	torture_assert(torture,
		       dn = ldb_dn_new(mem_ctx, ldb, "CN=New\nLine,DC=SAMBA,DC=org"),
		       "Failed to create a DN with 0xA in it");

	/* this is a warning until we work out how the DEL: CNs work */
	if (ldb_dn_validate(dn) != false) {
		torture_warning(torture,
				"should have failed to validate a DN with 0xA in it");
	}

	/* Escaped comma */
	torture_assert(torture,
		       dn = ldb_dn_new(mem_ctx, ldb, "CN=A\\,comma,DC=SAMBA,DC=org"),
		       "Failed to create a DN with an escaped comma in it");


	val = data_blob_const("CN=Zer\0,DC=SAMBA,DC=org", 23);
	torture_assert(torture,
		       NULL == ldb_dn_from_ldb_val(mem_ctx, ldb, &val),
		       "should fail to create a DN with 0x0 in it");

	talloc_free(mem_ctx);
	return true;
}

static bool torture_ldb_dn_invalid_extended(struct torture_context *torture)
{
	TALLOC_CTX *mem_ctx = talloc_new(torture);
	struct ldb_context *ldb;
	struct ldb_dn *dn;

	const char *dn_str = "cn=admin,cn=users,dc=samba,dc=org";

	torture_assert(torture, 
		       ldb = ldb_init(mem_ctx, torture->ev),
		       "Failed to init ldb");

	torture_assert_int_equal(torture, 
				 ldb_register_samba_handlers(ldb), LDB_SUCCESS,
				 "Failed to register Samba handlers");

	ldb_set_utf8_fns(ldb, NULL, wrap_casefold);

	/* Check behaviour of a normal DN */
	torture_assert(torture, 
		       dn = ldb_dn_new(mem_ctx, ldb, "samba,dc=org"), 
		       "Failed to create a 'normal' invalid DN");

	torture_assert(torture, 
		       ldb_dn_validate(dn) == false,
		       "should have failed to validate 'normal' invalid DN");

	/* Now make an extended DN */
	torture_assert(torture, 
		       dn = ldb_dn_new_fmt(mem_ctx, ldb, "<PID=%s>;%s",
					   sid, dn_str), 
		       "Failed to create an invalid 'extended' DN");

	torture_assert(torture, 
		       ldb_dn_validate(dn) == false,
		       "should have failed to validate 'extended' DN");

	torture_assert(torture, 
		       dn = ldb_dn_new_fmt(mem_ctx, ldb, "<GUID=%s>%s",
					   sid, dn_str), 
		       "Failed to create an invalid 'extended' DN");

	torture_assert(torture, 
		       ldb_dn_validate(dn) == false,
		       "should have failed to validate 'extended' DN");

	torture_assert(torture, 
		       dn = ldb_dn_new_fmt(mem_ctx, ldb, "<GUID=%s>;",
					   sid), 
		       "Failed to create an invalid 'extended' DN");

	torture_assert(torture, 
		       ldb_dn_validate(dn) == false,
		       "should have failed to validate 'extended' DN");

	torture_assert(torture, 
		       dn = ldb_dn_new_fmt(mem_ctx, ldb, "<GUID=%s>;",
					   hex_sid), 
		       "Failed to create an invalid 'extended' DN");

	torture_assert(torture, 
		       ldb_dn_validate(dn) == false,
		       "should have failed to validate 'extended' DN");

	torture_assert(torture, 
		       dn = ldb_dn_new_fmt(mem_ctx, ldb, "<SID=%s>;",
					   hex_guid), 
		       "Failed to create an invalid 'extended' DN");

	torture_assert(torture, 
		       ldb_dn_validate(dn) == false,
		       "should have failed to validate 'extended' DN");

	torture_assert(torture, 
		       dn = ldb_dn_new_fmt(mem_ctx, ldb, "<SID=%s>;",
					   guid), 
		       "Failed to create an invalid 'extended' DN");

	torture_assert(torture, 
		       ldb_dn_validate(dn) == false,
		       "should have failed to validate 'extended' DN");

	torture_assert(torture, 
		       dn = ldb_dn_new_fmt(mem_ctx, ldb, "<GUID=>"), 
		       "Failed to create an invalid 'extended' DN");

	torture_assert(torture, 
		       ldb_dn_validate(dn) == false,
		       "should have failed to validate 'extended' DN");

	return true;
}

struct torture_suite *torture_ldb(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "ldb");

	if (suite == NULL) {
		return NULL;
	}

	torture_suite_add_simple_test(suite, "attrs", torture_ldb_attrs);
	torture_suite_add_simple_test(suite, "dn-attrs", torture_ldb_dn_attrs);
	torture_suite_add_simple_test(suite, "dn-extended", torture_ldb_dn_extended);
	torture_suite_add_simple_test(suite, "dn-invalid-extended", torture_ldb_dn_invalid_extended);
	torture_suite_add_simple_test(suite, "dn", torture_ldb_dn);

	suite->description = talloc_strdup(suite, "LDB (samba-specific behaviour) tests");

	return suite;
}
