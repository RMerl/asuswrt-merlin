/*
   Unix SMB/CIFS implementation.

   DRSUAPI prefixMap unit tests

   Copyright (C) Kamen Mazdrashki <kamenim@samba.org> 2009-2010

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
#include "torture/rpc/drsuapi.h"
#include "torture/drs/proto.h"
#include "param/param.h"


/**
 * Private data to be shared among all test in Test case
 */
struct drsut_prefixmap_data {
	struct dsdb_schema_prefixmap *pfm_new;
	struct dsdb_schema_prefixmap *pfm_full;

	/* default schemaInfo value to test with */
	const char *schi_default_str;
	struct dsdb_schema_info *schi_default;

	struct ldb_context *ldb_ctx;
};

/**
 * Test-oid data structure
 */
struct drsut_pfm_oid_data {
	uint32_t	id;
	const char	*bin_oid;
	const char	*oid_prefix;
};

/**
 * Default prefixMap initialization data.
 * This prefixMap is what dsdb_schema_pfm_new() should return.
 * Based on: MS-DRSR, 5.16.4 ATTRTYP-to-OID Conversion
 *           procedure NewPrefixTable( )
 */
static const struct drsut_pfm_oid_data _prefixmap_test_new_data[] = {
	{.id=0x00000000, .bin_oid="5504",                 .oid_prefix="2.5.4"},
	{.id=0x00000001, .bin_oid="5506",                 .oid_prefix="2.5.6"},
	{.id=0x00000002, .bin_oid="2A864886F7140102",     .oid_prefix="1.2.840.113556.1.2"},
	{.id=0x00000003, .bin_oid="2A864886F7140103",     .oid_prefix="1.2.840.113556.1.3"},
	{.id=0x00000004, .bin_oid="6086480165020201",     .oid_prefix="2.16.840.1.101.2.2.1"},
	{.id=0x00000005, .bin_oid="6086480165020203",     .oid_prefix="2.16.840.1.101.2.2.3"},
	{.id=0x00000006, .bin_oid="6086480165020105",     .oid_prefix="2.16.840.1.101.2.1.5"},
	{.id=0x00000007, .bin_oid="6086480165020104",     .oid_prefix="2.16.840.1.101.2.1.4"},
	{.id=0x00000008, .bin_oid="5505",                 .oid_prefix="2.5.5"},
	{.id=0x00000009, .bin_oid="2A864886F7140104",     .oid_prefix="1.2.840.113556.1.4"},
	{.id=0x0000000A, .bin_oid="2A864886F7140105",     .oid_prefix="1.2.840.113556.1.5"},
	{.id=0x00000013, .bin_oid="0992268993F22C64",     .oid_prefix="0.9.2342.19200300.100"},
	{.id=0x00000014, .bin_oid="6086480186F84203",     .oid_prefix="2.16.840.1.113730.3"},
	{.id=0x00000015, .bin_oid="0992268993F22C6401",   .oid_prefix="0.9.2342.19200300.100.1"},
	{.id=0x00000016, .bin_oid="6086480186F8420301",   .oid_prefix="2.16.840.1.113730.3.1"},
	{.id=0x00000017, .bin_oid="2A864886F7140105B658", .oid_prefix="1.2.840.113556.1.5.7000"},
	{.id=0x00000018, .bin_oid="5515",                 .oid_prefix="2.5.21"},
	{.id=0x00000019, .bin_oid="5512",                 .oid_prefix="2.5.18"},
	{.id=0x0000001A, .bin_oid="5514",                 .oid_prefix="2.5.20"},
};

/**
 * Data to be used for creating full prefix map for testing.
 * 'full-prefixMap' is based on what w2k8 returns as a prefixMap
 * on clean installation - i.e. prefixMap for clean Schema
 */
static const struct drsut_pfm_oid_data _prefixmap_full_map_data[] = {
	{.id=0x00000000, .bin_oid="0x5504",                     .oid_prefix="2.5.4"},
	{.id=0x00000001, .bin_oid="0x5506",                     .oid_prefix="2.5.6"},
	{.id=0x00000002, .bin_oid="0x2A864886F7140102",         .oid_prefix="1.2.840.113556.1.2"},
	{.id=0x00000003, .bin_oid="0x2A864886F7140103",         .oid_prefix="1.2.840.113556.1.3"},
	{.id=0x00000004, .bin_oid="0x6086480165020201",         .oid_prefix="2.16.840.1.101.2.2.1"},
	{.id=0x00000005, .bin_oid="0x6086480165020203",         .oid_prefix="2.16.840.1.101.2.2.3"},
	{.id=0x00000006, .bin_oid="0x6086480165020105",         .oid_prefix="2.16.840.1.101.2.1.5"},
	{.id=0x00000007, .bin_oid="0x6086480165020104",         .oid_prefix="2.16.840.1.101.2.1.4"},
	{.id=0x00000008, .bin_oid="0x5505",                     .oid_prefix="2.5.5"},
	{.id=0x00000009, .bin_oid="0x2A864886F7140104",         .oid_prefix="1.2.840.113556.1.4"},
	{.id=0x0000000a, .bin_oid="0x2A864886F7140105",         .oid_prefix="1.2.840.113556.1.5"},
	{.id=0x00000013, .bin_oid="0x0992268993F22C64",         .oid_prefix="0.9.2342.19200300.100"},
	{.id=0x00000014, .bin_oid="0x6086480186F84203",         .oid_prefix="2.16.840.1.113730.3"},
	{.id=0x00000015, .bin_oid="0x0992268993F22C6401",       .oid_prefix="0.9.2342.19200300.100.1"},
	{.id=0x00000016, .bin_oid="0x6086480186F8420301",       .oid_prefix="2.16.840.1.113730.3.1"},
	{.id=0x00000017, .bin_oid="0x2A864886F7140105B658",     .oid_prefix="1.2.840.113556.1.5.7000"},
	{.id=0x00000018, .bin_oid="0x5515",                     .oid_prefix="2.5.21"},
	{.id=0x00000019, .bin_oid="0x5512",                     .oid_prefix="2.5.18"},
	{.id=0x0000001a, .bin_oid="0x5514",                     .oid_prefix="2.5.20"},
	{.id=0x0000000b, .bin_oid="0x2A864886F71401048204",     .oid_prefix="1.2.840.113556.1.4.260"},
	{.id=0x0000000c, .bin_oid="0x2A864886F714010538",       .oid_prefix="1.2.840.113556.1.5.56"},
	{.id=0x0000000d, .bin_oid="0x2A864886F71401048206",     .oid_prefix="1.2.840.113556.1.4.262"},
	{.id=0x0000000e, .bin_oid="0x2A864886F714010539",       .oid_prefix="1.2.840.113556.1.5.57"},
	{.id=0x0000000f, .bin_oid="0x2A864886F71401048207",     .oid_prefix="1.2.840.113556.1.4.263"},
	{.id=0x00000010, .bin_oid="0x2A864886F71401053A",       .oid_prefix="1.2.840.113556.1.5.58"},
	{.id=0x00000011, .bin_oid="0x2A864886F714010549",       .oid_prefix="1.2.840.113556.1.5.73"},
	{.id=0x00000012, .bin_oid="0x2A864886F71401048231",     .oid_prefix="1.2.840.113556.1.4.305"},
	{.id=0x0000001b, .bin_oid="0x2B060104018B3A6577",       .oid_prefix="1.3.6.1.4.1.1466.101.119"},
	{.id=0x0000001c, .bin_oid="0x6086480186F8420302",       .oid_prefix="2.16.840.1.113730.3.2"},
	{.id=0x0000001d, .bin_oid="0x2B06010401817A01",         .oid_prefix="1.3.6.1.4.1.250.1"},
	{.id=0x0000001e, .bin_oid="0x2A864886F70D0109",         .oid_prefix="1.2.840.113549.1.9"},
	{.id=0x0000001f, .bin_oid="0x0992268993F22C6404",       .oid_prefix="0.9.2342.19200300.100.4"},
	{.id=0x00000020, .bin_oid="0x2A864886F714010617",       .oid_prefix="1.2.840.113556.1.6.23"},
	{.id=0x00000021, .bin_oid="0x2A864886F71401061201",     .oid_prefix="1.2.840.113556.1.6.18.1"},
	{.id=0x00000022, .bin_oid="0x2A864886F71401061202",     .oid_prefix="1.2.840.113556.1.6.18.2"},
	{.id=0x00000023, .bin_oid="0x2A864886F71401060D03",     .oid_prefix="1.2.840.113556.1.6.13.3"},
	{.id=0x00000024, .bin_oid="0x2A864886F71401060D04",     .oid_prefix="1.2.840.113556.1.6.13.4"},
	{.id=0x00000025, .bin_oid="0x2B0601010101",             .oid_prefix="1.3.6.1.1.1.1"},
	{.id=0x00000026, .bin_oid="0x2B0601010102",             .oid_prefix="1.3.6.1.1.1.2"},
	{.id=0x000003ed, .bin_oid="0x2A864886F7140104B65866",   .oid_prefix="1.2.840.113556.1.4.7000.102"},
	{.id=0x00000428, .bin_oid="0x2A864886F7140105B6583E",   .oid_prefix="1.2.840.113556.1.5.7000.62"},
	{.id=0x0000044c, .bin_oid="0x2A864886F7140104B6586683", .oid_prefix="1.2.840.113556.1.4.7000.102:0x83"},
	{.id=0x0000044f, .bin_oid="0x2A864886F7140104B6586681", .oid_prefix="1.2.840.113556.1.4.7000.102:0x81"},
	{.id=0x0000047d, .bin_oid="0x2A864886F7140105B6583E81", .oid_prefix="1.2.840.113556.1.5.7000.62:0x81"},
	{.id=0x00000561, .bin_oid="0x2A864886F7140105B6583E83", .oid_prefix="1.2.840.113556.1.5.7000.62:0x83"},
	{.id=0x000007d1, .bin_oid="0x2A864886F71401061401",     .oid_prefix="1.2.840.113556.1.6.20.1"},
	{.id=0x000007e1, .bin_oid="0x2A864886F71401061402",     .oid_prefix="1.2.840.113556.1.6.20.2"},
	{.id=0x00001b86, .bin_oid="0x2A817A",                   .oid_prefix="1.2.250"},
	{.id=0x00001c78, .bin_oid="0x2A817A81",                 .oid_prefix="1.2.250:0x81"},
	{.id=0x00001c7b, .bin_oid="0x2A817A8180",               .oid_prefix="1.2.250:0x8180"},
};


/**
 * OID-to-ATTID mappings to be used for testing.
 * An entry is marked as 'exists=true' if it exists in
 * base prefixMap (_prefixmap_test_new_data)
 */
static const struct {
	const char 	*oid;
	uint32_t 	id;
	uint32_t 	attid;
	bool		exists;
} _prefixmap_test_data[] = {
	{.oid="2.5.4.0", 		.id=0x00000000, .attid=0x000000,   .exists=true},
	{.oid="2.5.4.42", 		.id=0x00000000, .attid=0x00002a,   .exists=true},
	{.oid="1.2.840.113556.1.2.1", 	.id=0x00000002, .attid=0x020001,   .exists=true},
	{.oid="1.2.840.113556.1.2.13", 	.id=0x00000002, .attid=0x02000d,   .exists=true},
	{.oid="1.2.840.113556.1.2.281", .id=0x00000002, .attid=0x020119,   .exists=true},
	{.oid="1.2.840.113556.1.4.125", .id=0x00000009, .attid=0x09007d,   .exists=true},
	{.oid="1.2.840.113556.1.4.146", .id=0x00000009, .attid=0x090092,   .exists=true},
	{.oid="1.2.250.1", 	 	.id=0x00001b86, .attid=0x1b860001, .exists=false},
	{.oid="1.2.250.16386", 	 	.id=0x00001c78, .attid=0x1c788002, .exists=false},
	{.oid="1.2.250.2097154", 	.id=0x00001c7b, .attid=0x1c7b8002, .exists=false},
};


/**
 * Creates dsdb_schema_prefixmap based on predefined data
 */
static WERROR _drsut_prefixmap_new(const struct drsut_pfm_oid_data *_pfm_init_data, uint32_t count,
				   TALLOC_CTX *mem_ctx, struct dsdb_schema_prefixmap **_pfm)
{
	uint32_t i;
	struct dsdb_schema_prefixmap *pfm;

	pfm = talloc(mem_ctx, struct dsdb_schema_prefixmap);
	W_ERROR_HAVE_NO_MEMORY(pfm);

	pfm->length = count;
	pfm->prefixes = talloc_array(pfm, struct dsdb_schema_prefixmap_oid, pfm->length);
	if (!pfm->prefixes) {
		talloc_free(pfm);
		return WERR_NOMEM;
	}

	for (i = 0; i < pfm->length; i++) {
		pfm->prefixes[i].id = _pfm_init_data[i].id;
		pfm->prefixes[i].bin_oid = strhex_to_data_blob(pfm, _pfm_init_data[i].bin_oid);
		if (!pfm->prefixes[i].bin_oid.data) {
			talloc_free(pfm);
			return WERR_NOMEM;
		}
	}

	*_pfm = pfm;

	return WERR_OK;
}

/**
 * Compares two prefixMaps for being equal - same items on same indexes
 */
static bool _torture_drs_pfm_compare_same(struct torture_context *tctx,
					  const struct dsdb_schema_prefixmap *pfm_left,
					  const struct dsdb_schema_prefixmap *pfm_right,
					  bool quiet)
{
	uint32_t i;
	char *err_msg = NULL;

	if (pfm_left->length != pfm_right->length) {
		err_msg = talloc_asprintf(tctx, "prefixMaps differ in size; left = %d, right = %d",
					  pfm_left->length, pfm_right->length);
		goto failed;
	}

	for (i = 0; i < pfm_left->length; i++) {
		struct dsdb_schema_prefixmap_oid *entry_left = &pfm_left->prefixes[i];
		struct dsdb_schema_prefixmap_oid *entry_right = &pfm_right->prefixes[i];

		if (entry_left->id != entry_right->id) {
			err_msg = talloc_asprintf(tctx, "Different IDs for index=%d", i);
			goto failed;
		}
		if (data_blob_cmp(&entry_left->bin_oid, &entry_right->bin_oid)) {
			err_msg = talloc_asprintf(tctx, "Different bin_oid for index=%d", i);
			goto failed;
		}
	}

	return true;

failed:
	if (!quiet) {
		torture_comment(tctx, "_torture_drs_pfm_compare_same: %s", err_msg);
	}
	talloc_free(err_msg);

	return false;
}

/*
 * Tests dsdb_schema_pfm_new()
 */
static bool torture_drs_unit_pfm_new(struct torture_context *tctx, struct drsut_prefixmap_data *priv)
{
	WERROR werr;
	bool bret;
	TALLOC_CTX *mem_ctx;
	struct dsdb_schema_prefixmap *pfm = NULL;

	mem_ctx = talloc_new(priv);

	/* create new prefix map */
	werr = dsdb_schema_pfm_new(mem_ctx, &pfm);
	torture_assert_werr_ok(tctx, werr, "dsdb_schema_pfm_new() failed!");
	torture_assert(tctx, pfm != NULL, "NULL prefixMap created!");
	torture_assert(tctx, pfm->length > 0, "Empty prefixMap created!");
	torture_assert(tctx, pfm->prefixes != NULL, "No prefixes for newly created prefixMap!");

	/* compare newly created prefixMap with template one */
	bret = _torture_drs_pfm_compare_same(tctx, priv->pfm_new, pfm, false);

	talloc_free(mem_ctx);

	return bret;
}

/**
 * Tests dsdb_schema_pfm_make_attid() using full prefixMap.
 * In this test we know exactly which ATTID and prefixMap->ID
 * should be returned, i.e. no prefixMap entries should be added.
 */
static bool torture_drs_unit_pfm_make_attid_full_map(struct torture_context *tctx, struct drsut_prefixmap_data *priv)
{
	WERROR werr;
	uint32_t i, count;
	uint32_t attid;
	char *err_msg;

	count = ARRAY_SIZE(_prefixmap_test_data);
	for (i = 0; i < count; i++) {
		werr = dsdb_schema_pfm_make_attid(priv->pfm_full, _prefixmap_test_data[i].oid, &attid);
		/* prepare error message */
		err_msg = talloc_asprintf(priv, "dsdb_schema_pfm_make_attid() failed with %s",
						_prefixmap_test_data[i].oid);
		torture_assert(tctx, err_msg, "Unexpected: Have no memory!");
		/* verify result and returned ATTID */
		torture_assert_werr_ok(tctx, werr, err_msg);
		torture_assert_int_equal(tctx, attid, _prefixmap_test_data[i].attid, err_msg);
		/* reclaim memory for prepared error message */
		talloc_free(err_msg);
	}

	return true;
}

/**
 * Tests dsdb_schema_pfm_make_attid() using initially small prefixMap.
 * In this test we don't know exactly which ATTID and prefixMap->ID
 * should be returned, but we can verify lo-word of ATTID.
 * This test verifies implementation branch when a new
 * prefix should be added into prefixMap.
 */
static bool torture_drs_unit_pfm_make_attid_small_map(struct torture_context *tctx, struct drsut_prefixmap_data *priv)
{
	WERROR werr;
	uint32_t i, j;
	uint32_t idx;
	uint32_t attid, attid_2;
	char *err_msg;
	struct dsdb_schema_prefixmap *pfm = NULL;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_new(priv);

	/* create new prefix map */
	werr = dsdb_schema_pfm_new(mem_ctx, &pfm);
	torture_assert_werr_ok(tctx, werr, "dsdb_schema_pfm_new() failed!");

	/* make some ATTIDs and check result */
	for (i = 0; i < ARRAY_SIZE(_prefixmap_test_data); i++) {
		werr = dsdb_schema_pfm_make_attid(pfm, _prefixmap_test_data[i].oid, &attid);

		/* prepare error message */
		err_msg = talloc_asprintf(mem_ctx, "dsdb_schema_pfm_make_attid() failed with %s",
						_prefixmap_test_data[i].oid);
		torture_assert(tctx, err_msg, "Unexpected: Have no memory!");

		/* verify result and returned ATTID */
		torture_assert_werr_ok(tctx, werr, err_msg);
		/* verify ATTID lo-word */
		torture_assert_int_equal(tctx, attid & 0xFFFF, _prefixmap_test_data[i].attid & 0xFFFF, err_msg);

		/* try again, this time verify for whole ATTID */
		werr = dsdb_schema_pfm_make_attid(pfm, _prefixmap_test_data[i].oid, &attid_2);
		torture_assert_werr_ok(tctx, werr, err_msg);
		torture_assert_int_equal(tctx, attid_2, attid, err_msg);

		/* reclaim memory for prepared error message */
		talloc_free(err_msg);

		/* check there is such an index in modified prefixMap */
		idx = (attid >> 16);
		for (j = 0; j < pfm->length; j++) {
			if (pfm->prefixes[j].id == idx)
				break;
		}
		if (j >= pfm->length) {
			torture_result(tctx, TORTURE_FAIL, __location__": No prefix for ATTID=0x%08X", attid);
			return false;
		}

	}

	talloc_free(mem_ctx);

	return true;
}

/**
 * Tests dsdb_schema_pfm_attid_from_oid() using full prefixMap.
 * In this test we know exactly which ATTID and prefixMap->ID
 * should be returned- dsdb_schema_pfm_attid_from_oid() should succeed.
 */
static bool torture_drs_unit_pfm_attid_from_oid_full_map(struct torture_context *tctx,
							 struct drsut_prefixmap_data *priv)
{
	WERROR werr;
	uint32_t i, count;
	uint32_t attid;
	char *err_msg;

	count = ARRAY_SIZE(_prefixmap_test_data);
	for (i = 0; i < count; i++) {
		werr = dsdb_schema_pfm_attid_from_oid(priv->pfm_full,
						      _prefixmap_test_data[i].oid,
						      &attid);
		/* prepare error message */
		err_msg = talloc_asprintf(priv, "dsdb_schema_pfm_attid_from_oid() failed with %s",
						_prefixmap_test_data[i].oid);
		torture_assert(tctx, err_msg, "Unexpected: Have no memory!");
		/* verify result and returned ATTID */
		torture_assert_werr_ok(tctx, werr, err_msg);
		torture_assert_int_equal(tctx, attid, _prefixmap_test_data[i].attid, err_msg);
		/* reclaim memory for prepared error message */
		talloc_free(err_msg);
	}

	return true;
}

/**
 * Tests dsdb_schema_pfm_attid_from_oid() using base (initial) prefixMap.
 * dsdb_schema_pfm_attid_from_oid() should fail when testing with OID
 * that are not already in the prefixMap.
 */
static bool torture_drs_unit_pfm_attid_from_oid_base_map(struct torture_context *tctx,
							 struct drsut_prefixmap_data *priv)
{
	WERROR werr;
	uint32_t i;
	uint32_t attid;
	char *err_msg;
	struct dsdb_schema_prefixmap *pfm = NULL;
	struct dsdb_schema_prefixmap pfm_prev;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_new(priv);
	torture_assert(tctx, mem_ctx, "Unexpected: Have no memory!");

	/* create new prefix map */
	werr = dsdb_schema_pfm_new(mem_ctx, &pfm);
	torture_assert_werr_ok(tctx, werr, "dsdb_schema_pfm_new() failed!");

	/* keep initial pfm around for testing */
	pfm_prev = *pfm;
	pfm_prev.prefixes = talloc_reference(mem_ctx, pfm->prefixes);

	/* get some ATTIDs and check result */
	for (i = 0; i < ARRAY_SIZE(_prefixmap_test_data); i++) {
		werr = dsdb_schema_pfm_attid_from_oid(pfm, _prefixmap_test_data[i].oid, &attid);

		/* prepare error message */
		err_msg = talloc_asprintf(mem_ctx,
					  "dsdb_schema_pfm_attid_from_oid() failed for %s",
					  _prefixmap_test_data[i].oid);
		torture_assert(tctx, err_msg, "Unexpected: Have no memory!");


		/* verify pfm hasn't been altered */
		if (_prefixmap_test_data[i].exists) {
			/* should succeed and return valid ATTID */
			torture_assert_werr_ok(tctx, werr, err_msg);
			/* verify ATTID */
			torture_assert_int_equal(tctx,
						 attid, _prefixmap_test_data[i].attid,
						 err_msg);
		} else {
			/* should fail */
			torture_assert_werr_equal(tctx, werr, WERR_NOT_FOUND, err_msg);
		}

		/* prefixMap should never be changed */
		if (!_torture_drs_pfm_compare_same(tctx, &pfm_prev, pfm, true)) {
			torture_fail(tctx, "schema->prefixmap has changed");
		}

		/* reclaim memory for prepared error message */
		talloc_free(err_msg);
	}

	talloc_free(mem_ctx);

	return true;
}

/**
 * Tests dsdb_schema_pfm_oid_from_attid() using full prefixMap.
 */
static bool torture_drs_unit_pfm_oid_from_attid(struct torture_context *tctx, struct drsut_prefixmap_data *priv)
{
	WERROR werr;
	uint32_t i, count;
	char *err_msg;
	const char *oid;

	count = ARRAY_SIZE(_prefixmap_test_data);
	for (i = 0; i < count; i++) {
		oid = NULL;
		werr = dsdb_schema_pfm_oid_from_attid(priv->pfm_full, _prefixmap_test_data[i].attid,
						      priv, &oid);
		/* prepare error message */
		err_msg = talloc_asprintf(priv, "dsdb_schema_pfm_oid_from_attid() failed with 0x%08X",
						_prefixmap_test_data[i].attid);
		torture_assert(tctx, err_msg, "Unexpected: Have no memory!");
		/* verify result and returned ATTID */
		torture_assert_werr_ok(tctx, werr, err_msg);
		torture_assert(tctx, oid, "dsdb_schema_pfm_oid_from_attid() returned NULL OID!!!");
		torture_assert_str_equal(tctx, oid, _prefixmap_test_data[i].oid, err_msg);
		/* reclaim memory for prepared error message */
		talloc_free(err_msg);
		/* free memory for OID */
		talloc_free(discard_const(oid));
	}

	return true;
}

/**
 * Tests dsdb_schema_pfm_oid_from_attid() for handling
 * correctly different type of attid values.
 * See: MS-ADTS, 3.1.1.2.6 ATTRTYP
 */
static bool torture_drs_unit_pfm_oid_from_attid_check_attid(struct torture_context *tctx,
							    struct drsut_prefixmap_data *priv)
{
	WERROR werr;
	const char *oid;

	/* Test with valid prefixMap attid */
	werr = dsdb_schema_pfm_oid_from_attid(priv->pfm_full, 0x00010001, tctx, &oid);
	torture_assert_werr_ok(tctx, werr, "Testing prefixMap type attid = 0x00010001");

	/* Test with valid attid but invalid index */
	werr = dsdb_schema_pfm_oid_from_attid(priv->pfm_full, 0x01110001, tctx, &oid);
	torture_assert_werr_equal(tctx, werr, WERR_DS_NO_ATTRIBUTE_OR_VALUE,
				  "Testing invalid-index attid = 0x01110001");

	/* Test with attid in msDS-IntId range */
	werr = dsdb_schema_pfm_oid_from_attid(priv->pfm_full, 0x80000000, tctx, &oid);
	torture_assert_werr_equal(tctx, werr, WERR_INVALID_PARAMETER,
				  "Testing msDS-IntId type attid = 0x80000000");
	werr = dsdb_schema_pfm_oid_from_attid(priv->pfm_full, 0xBFFFFFFF, tctx, &oid);
	torture_assert_werr_equal(tctx, werr, WERR_INVALID_PARAMETER,
				  "Testing msDS-IntId type attid = 0xBFFFFFFF");

	/* Test with attid in RESERVED range */
	werr = dsdb_schema_pfm_oid_from_attid(priv->pfm_full, 0xC0000000, tctx, &oid);
	torture_assert_werr_equal(tctx, werr, WERR_INVALID_PARAMETER,
				  "Testing RESERVED type attid = 0xC0000000");
	werr = dsdb_schema_pfm_oid_from_attid(priv->pfm_full, 0xFFFEFFFF, tctx, &oid);
	torture_assert_werr_equal(tctx, werr, WERR_INVALID_PARAMETER,
				  "Testing RESERVED type attid = 0xFFFEFFFF");

	/* Test with attid in INTERNAL range */
	werr = dsdb_schema_pfm_oid_from_attid(priv->pfm_full, 0xFFFF0000, tctx, &oid);
	torture_assert_werr_equal(tctx, werr, WERR_INVALID_PARAMETER,
				  "Testing INTERNAL type attid = 0xFFFF0000");
	werr = dsdb_schema_pfm_oid_from_attid(priv->pfm_full, 0xFFFFFFFF, tctx, &oid);
	torture_assert_werr_equal(tctx, werr, WERR_INVALID_PARAMETER,
				  "Testing INTERNAL type attid = 0xFFFFFFFF");

	return true;
}

/**
 * Test Schema prefixMap conversions to/from drsuapi prefixMap
 * representation.
 */
static bool torture_drs_unit_pfm_to_from_drsuapi(struct torture_context *tctx, struct drsut_prefixmap_data *priv)
{
	WERROR werr;
	const char *schema_info;
	struct dsdb_schema_prefixmap *pfm;
	struct drsuapi_DsReplicaOIDMapping_Ctr *ctr;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_new(tctx);
	torture_assert(tctx, mem_ctx, "Unexpected: Have no memory!");

	/* convert Schema_prefixMap to drsuapi_prefixMap */
	werr = dsdb_drsuapi_pfm_from_schema_pfm(priv->pfm_full, priv->schi_default_str, mem_ctx, &ctr);
	torture_assert_werr_ok(tctx, werr, "dsdb_drsuapi_pfm_from_schema_pfm() failed");
	torture_assert(tctx, ctr && ctr->mappings, "drsuapi_prefixMap not constructed correctly");
	torture_assert_int_equal(tctx, ctr->num_mappings, priv->pfm_full->length + 1,
				 "drs_mappings count does not match");
	/* look for schema_info entry - it should be the last one */
	schema_info = hex_encode_talloc(mem_ctx,
					ctr->mappings[ctr->num_mappings - 1].oid.binary_oid,
					ctr->mappings[ctr->num_mappings - 1].oid.length);
	torture_assert_str_equal(tctx,
				 schema_info,
				 priv->schi_default_str,
				 "schema_info not stored correctly or not last entry");

	/* compare schema_prefixMap and drsuapi_prefixMap */
	werr = dsdb_schema_pfm_contains_drsuapi_pfm(priv->pfm_full, ctr);
	torture_assert_werr_ok(tctx, werr, "dsdb_schema_pfm_contains_drsuapi_pfm() failed");

	/* convert back drsuapi_prefixMap to schema_prefixMap */
	werr = dsdb_schema_pfm_from_drsuapi_pfm(ctr, true, mem_ctx, &pfm, &schema_info);
	torture_assert_werr_ok(tctx, werr, "dsdb_schema_pfm_from_drsuapi_pfm() failed");
	torture_assert_str_equal(tctx, schema_info, priv->schi_default_str, "Fetched schema_info is different");

	/* compare against the original */
	if (!_torture_drs_pfm_compare_same(tctx, priv->pfm_full, pfm, true)) {
		talloc_free(mem_ctx);
		return false;
	}

	/* test conversion with partial drsuapi_prefixMap */
	ctr->num_mappings--;
	werr = dsdb_schema_pfm_from_drsuapi_pfm(ctr, false, mem_ctx, &pfm, NULL);
	torture_assert_werr_ok(tctx, werr, "dsdb_schema_pfm_from_drsuapi_pfm() failed");
	/* compare against the original */
	if (!_torture_drs_pfm_compare_same(tctx, priv->pfm_full, pfm, false)) {
		talloc_free(mem_ctx);
		return false;
	}

	talloc_free(mem_ctx);
	return true;
}


/**
 * Test Schema prefixMap conversions to/from ldb_val
 * blob representation.
 */
static bool torture_drs_unit_pfm_to_from_ldb_val(struct torture_context *tctx, struct drsut_prefixmap_data *priv)
{
	WERROR werr;
	const char *schema_info;
	struct dsdb_schema *schema;
	struct ldb_val pfm_ldb_val;
	struct ldb_val schema_info_ldb_val;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_new(tctx);
	torture_assert(tctx, mem_ctx, "Unexpected: Have no memory!");

	schema = dsdb_new_schema(mem_ctx);
	torture_assert(tctx, schema, "Unexpected: failed to allocate schema object");

	/* set priv->pfm_full as prefixMap for new schema object */
	schema->prefixmap = priv->pfm_full;
	schema->schema_info = priv->schi_default_str;

	/* convert schema_prefixMap to ldb_val blob */
	werr = dsdb_get_oid_mappings_ldb(schema, mem_ctx, &pfm_ldb_val, &schema_info_ldb_val);
	torture_assert_werr_ok(tctx, werr, "dsdb_get_oid_mappings_ldb() failed");
	torture_assert(tctx, pfm_ldb_val.data && pfm_ldb_val.length,
		       "pfm_ldb_val not constructed correctly");
	torture_assert(tctx, schema_info_ldb_val.data && schema_info_ldb_val.length,
		       "schema_info_ldb_val not constructed correctly");
	/* look for schema_info entry - it should be the last one */
	schema_info = hex_encode_talloc(mem_ctx,
					schema_info_ldb_val.data,
					schema_info_ldb_val.length);
	torture_assert_str_equal(tctx,
				 schema_info,
				 priv->schi_default_str,
				 "schema_info not stored correctly or not last entry");

	/* convert pfm_ldb_val back to schema_prefixMap */
	schema->prefixmap = NULL;
	schema->schema_info = NULL;
	werr = dsdb_load_oid_mappings_ldb(schema, &pfm_ldb_val, &schema_info_ldb_val);
	torture_assert_werr_ok(tctx, werr, "dsdb_load_oid_mappings_ldb() failed");
	/* compare against the original */
	if (!_torture_drs_pfm_compare_same(tctx, schema->prefixmap, priv->pfm_full, false)) {
		talloc_free(mem_ctx);
		return false;
	}

	talloc_free(mem_ctx);
	return true;
}

/**
 * Test read/write in ldb implementation
 */
static bool torture_drs_unit_pfm_read_write_ldb(struct torture_context *tctx, struct drsut_prefixmap_data *priv)
{
	WERROR werr;
	struct dsdb_schema *schema;
	struct dsdb_schema_prefixmap *pfm;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_new(tctx);
	torture_assert(tctx, mem_ctx, "Unexpected: Have no memory!");

	/* makeup a dsdb_schema to test with */
	schema = dsdb_new_schema(mem_ctx);
	torture_assert(tctx, schema, "Unexpected: failed to allocate schema object");
	/* set priv->pfm_full as prefixMap for new schema object */
	schema->prefixmap = priv->pfm_full;
	schema->schema_info = priv->schi_default_str;

	/* write prfixMap to ldb */
	werr = dsdb_write_prefixes_from_schema_to_ldb(mem_ctx, priv->ldb_ctx, schema);
	torture_assert_werr_ok(tctx, werr, "dsdb_write_prefixes_from_schema_to_ldb() failed");

	/* read from ldb what we have written */
	werr = dsdb_read_prefixes_from_ldb(priv->ldb_ctx, mem_ctx, &pfm);
	torture_assert_werr_ok(tctx, werr, "dsdb_read_prefixes_from_ldb() failed");

	/* compare data written/read */
	if (!_torture_drs_pfm_compare_same(tctx, schema->prefixmap, priv->pfm_full, false)) {
		torture_fail(tctx, "prefixMap read/write in LDB is not consistent");
	}

	talloc_free(mem_ctx);

	return true;
}

/**
 * Test dsdb_create_prefix_mapping
 */
static bool torture_drs_unit_dsdb_create_prefix_mapping(struct torture_context *tctx, struct drsut_prefixmap_data *priv)
{
	WERROR werr;
	uint32_t i;
	struct dsdb_schema *schema;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_new(tctx);
	torture_assert(tctx, mem_ctx, "Unexpected: Have no memory!");

	/* makeup a dsdb_schema to test with */
	schema = dsdb_new_schema(mem_ctx);
	torture_assert(tctx, schema, "Unexpected: failed to allocate schema object");
	/* set priv->pfm_full as prefixMap for new schema object */
	schema->schema_info = priv->schi_default_str;
	werr = _drsut_prefixmap_new(_prefixmap_test_new_data, ARRAY_SIZE(_prefixmap_test_new_data),
				    schema, &schema->prefixmap);
	torture_assert_werr_ok(tctx, werr, "_drsut_prefixmap_new() failed");
	/* write prfixMap to ldb */
	werr = dsdb_write_prefixes_from_schema_to_ldb(mem_ctx, priv->ldb_ctx, schema);
	torture_assert_werr_ok(tctx, werr, "dsdb_write_prefixes_from_schema_to_ldb() failed");

	for (i = 0; i < ARRAY_SIZE(_prefixmap_test_data); i++) {
		struct dsdb_schema_prefixmap *pfm_ldb;
		struct dsdb_schema_prefixmap *pfm_prev;

		/* add ref to prefixMap so we can use it later */
		pfm_prev = talloc_reference(schema, schema->prefixmap);

		/* call dsdb_create_prefix_mapping() and check result accordingly */
		werr = dsdb_create_prefix_mapping(priv->ldb_ctx, schema, _prefixmap_test_data[i].oid);
		torture_assert_werr_ok(tctx, werr, "dsdb_create_prefix_mapping() failed");

		/* verify pfm has been altered or not if needed */
		if (_prefixmap_test_data[i].exists) {
			torture_assert(tctx, pfm_prev == schema->prefixmap,
				       "schema->prefixmap has been reallocated!");
			if (!_torture_drs_pfm_compare_same(tctx, pfm_prev, schema->prefixmap, true)) {
				torture_fail(tctx, "schema->prefixmap has changed");
			}
		} else {
			torture_assert(tctx, pfm_prev != schema->prefixmap,
				       "schema->prefixmap should be reallocated!");
			if (_torture_drs_pfm_compare_same(tctx, pfm_prev, schema->prefixmap, true)) {
				torture_fail(tctx, "schema->prefixmap should be changed");
			}
		}

		/* read from ldb what we have written */
		werr = dsdb_read_prefixes_from_ldb(priv->ldb_ctx, mem_ctx, &pfm_ldb);
		torture_assert_werr_ok(tctx, werr, "dsdb_read_prefixes_from_ldb() failed");
		/* compare data written/read */
		if (!_torture_drs_pfm_compare_same(tctx, schema->prefixmap, pfm_ldb, true)) {
			torture_fail(tctx, "schema->prefixmap and pfm in LDB are different");
		}
		/* free mem for pfm read from LDB */
		talloc_free(pfm_ldb);

		/* release prefixMap pointer */
		talloc_unlink(schema, pfm_prev);
	}

	talloc_free(mem_ctx);

	return true;
}

/**
 * Prepare temporary LDB and opens it
 */
static bool torture_drs_unit_ldb_setup(struct torture_context *tctx, struct drsut_prefixmap_data *priv)
{
	int ldb_err;
	char *ldb_url;
	bool bret = true;
	TALLOC_CTX* mem_ctx;
	char *tempdir;
	NTSTATUS status;

	mem_ctx = talloc_new(priv);

	status = torture_temp_dir(tctx, "drs_", &tempdir);
	torture_assert_ntstatus_ok(tctx, status, "creating temp dir");

	ldb_url = talloc_asprintf(priv, "%s/drs_test.ldb", tempdir);

	/* create LDB */
	priv->ldb_ctx = ldb_init(priv, tctx->ev);
	ldb_err = ldb_connect(priv->ldb_ctx, ldb_url, 0, NULL);
	torture_assert_int_equal_goto(tctx, ldb_err, LDB_SUCCESS, bret, DONE, "ldb_connect() failed");

	/* set some schemaNamingContext */
	ldb_err = ldb_set_opaque(priv->ldb_ctx,
				 "schemaNamingContext",
				 ldb_dn_new(priv->ldb_ctx, priv->ldb_ctx, "CN=Schema,CN=Config"));
	torture_assert_int_equal_goto(tctx, ldb_err, LDB_SUCCESS, bret, DONE, "ldb_set_opaque() failed");

	/* add prefixMap attribute so tested layer could work properly */
	{
		struct ldb_message *msg = ldb_msg_new(mem_ctx);
		msg->dn = ldb_get_schema_basedn(priv->ldb_ctx);
		ldb_err = ldb_msg_add_string(msg, "prefixMap", "prefixMap");
		torture_assert_int_equal_goto(tctx, ldb_err, LDB_SUCCESS, bret, DONE,
					      "ldb_msg_add_empty() failed");

		ldb_err = ldb_add(priv->ldb_ctx, msg);
		torture_assert_int_equal_goto(tctx, ldb_err, LDB_SUCCESS, bret, DONE, "ldb_add() failed");
	}

DONE:
	talloc_free(mem_ctx);
	return bret;
}

/*
 * Setup/Teardown for test case
 */
static bool torture_drs_unit_prefixmap_setup(struct torture_context *tctx, struct drsut_prefixmap_data **_priv)
{
	WERROR werr;
	DATA_BLOB blob;
	struct drsut_prefixmap_data *priv;

	priv = *_priv = talloc_zero(tctx, struct drsut_prefixmap_data);
	torture_assert(tctx, priv != NULL, "Not enough memory");

	werr = _drsut_prefixmap_new(_prefixmap_test_new_data, ARRAY_SIZE(_prefixmap_test_new_data),
	                            tctx, &priv->pfm_new);
	torture_assert_werr_ok(tctx, werr, "failed to create pfm_new");

	werr = _drsut_prefixmap_new(_prefixmap_full_map_data, ARRAY_SIZE(_prefixmap_full_map_data),
	                            tctx, &priv->pfm_full);
	torture_assert_werr_ok(tctx, werr, "failed to create pfm_test");

	torture_assert(tctx, drsut_schemainfo_new(tctx, &priv->schi_default),
	               "drsut_schemainfo_new() failed");

	werr = dsdb_blob_from_schema_info(priv->schi_default, priv, &blob);
	torture_assert_werr_ok(tctx, werr, "dsdb_blob_from_schema_info() failed");

	priv->schi_default_str = data_blob_hex_string_upper(priv, &blob);

	/* create temporary LDB and populate with data */
	if (!torture_drs_unit_ldb_setup(tctx, priv)) {
		return false;
	}

	return true;
}

static bool torture_drs_unit_prefixmap_teardown(struct torture_context *tctx, struct drsut_prefixmap_data *priv)
{
	talloc_free(priv);

	return true;
}

/**
 * Test case initialization for
 * drs.unit.prefixMap
 */
struct torture_tcase * torture_drs_unit_prefixmap(struct torture_suite *suite)
{
	typedef bool (*pfn_setup)(struct torture_context *, void **);
	typedef bool (*pfn_teardown)(struct torture_context *, void *);
	typedef bool (*pfn_run)(struct torture_context *, void *);

	struct torture_tcase * tc = torture_suite_add_tcase(suite, "prefixMap");

	torture_tcase_set_fixture(tc,
				  (pfn_setup)torture_drs_unit_prefixmap_setup,
				  (pfn_teardown)torture_drs_unit_prefixmap_teardown);

	tc->description = talloc_strdup(tc, "Unit tests for DRSUAPI::prefixMap implementation");

	torture_tcase_add_simple_test(tc, "new", (pfn_run)torture_drs_unit_pfm_new);

	torture_tcase_add_simple_test(tc, "make_attid_full_map", (pfn_run)torture_drs_unit_pfm_make_attid_full_map);
	torture_tcase_add_simple_test(tc, "make_attid_small_map", (pfn_run)torture_drs_unit_pfm_make_attid_small_map);

	torture_tcase_add_simple_test(tc, "attid_from_oid_full_map",
				      (pfn_run)torture_drs_unit_pfm_attid_from_oid_full_map);
	torture_tcase_add_simple_test(tc, "attid_from_oid_empty_map",
				      (pfn_run)torture_drs_unit_pfm_attid_from_oid_base_map);

	torture_tcase_add_simple_test(tc, "oid_from_attid_full_map", (pfn_run)torture_drs_unit_pfm_oid_from_attid);
	torture_tcase_add_simple_test(tc, "oid_from_attid_check_attid",
				      (pfn_run)torture_drs_unit_pfm_oid_from_attid_check_attid);

	torture_tcase_add_simple_test(tc, "pfm_to_from_drsuapi", (pfn_run)torture_drs_unit_pfm_to_from_drsuapi);

	torture_tcase_add_simple_test(tc, "pfm_to_from_ldb_val", (pfn_run)torture_drs_unit_pfm_to_from_ldb_val);

	torture_tcase_add_simple_test(tc, "pfm_read_write_ldb", (pfn_run)torture_drs_unit_pfm_read_write_ldb);

	torture_tcase_add_simple_test(tc, "dsdb_create_prefix_mapping", (pfn_run)torture_drs_unit_dsdb_create_prefix_mapping);

	return tc;
}
