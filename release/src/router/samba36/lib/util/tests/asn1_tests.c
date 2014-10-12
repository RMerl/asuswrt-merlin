/*
   Unix SMB/CIFS implementation.

   util_asn1 testing

   Copyright (C) Kamen Mazdrashki <kamen.mazdrashki@postpath.com> 2009

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
#include "../asn1.h"

struct oid_data {
	const char *oid;	/* String OID */
	const char *bin_oid;	/* Binary OID represented as string */
};

/* Data for successful OIDs conversions */
static const struct oid_data oid_data_ok[] = {
	{
		.oid = "2.5.4.0",
		.bin_oid = "550400"
	},
	{
		.oid = "2.5.4.1",
		.bin_oid = "550401"
	},
	{
		.oid = "2.5.4.130",
		.bin_oid = "55048102"
	},
	{
		.oid = "2.5.130.4",
		.bin_oid = "55810204"
	},
	{
		.oid = "2.5.4.16387",
		.bin_oid = "5504818003"
	},
	{
		.oid = "2.5.16387.4",
		.bin_oid = "5581800304"
	},
	{
		.oid = "2.5.2097155.4",
		.bin_oid = "558180800304"
	},
	{
		.oid = "2.5.4.130.16387.2097155.268435459",
		.bin_oid = "55048102818003818080038180808003"
	},
};

/* Data for successful OIDs conversions */
static const char *oid_data_err[] = {
		"",		/* empty OID */
		".2.5.4.130",	/* first sub-identifier is empty */
		"2.5.4.130.",	/* last sub-identifier is empty */
		"2..5.4.130",	/* second sub-identifier is empty */
		"2.5..4.130",	/* third sub-identifier is empty */
		"2.abc.4.130", 	/* invalid sub-identifier */
		"2.5abc.4.130", /* invalid sub-identifier (alpha-numeric)*/
};

/* Data for successful Partial OIDs conversions */
static const struct oid_data partial_oid_data_ok[] = {
	{
		.oid = "2.5.4.130:0x81",
		.bin_oid = "5504810281"
	},
	{
		.oid = "2.5.4.16387:0x8180",
		.bin_oid = "55048180038180"
	},
	{
		.oid = "2.5.4.16387:0x81",
		.bin_oid = "550481800381"
	},
	{
		.oid = "2.5.2097155.4:0x818080",
		.bin_oid = "558180800304818080"
	},
	{
		.oid = "2.5.2097155.4:0x8180",
		.bin_oid = "5581808003048180"
	},
	{
		.oid = "2.5.2097155.4:0x81",
		.bin_oid = "55818080030481"
	},
};


/* Testing ber_write_OID_String() function */
static bool test_ber_write_OID_String(struct torture_context *tctx)
{
	int i;
	char *hex_str;
	DATA_BLOB blob;
	TALLOC_CTX *mem_ctx;
	const struct oid_data *data = oid_data_ok;

	mem_ctx = talloc_new(tctx);

	/* check for valid OIDs */
	for (i = 0; i < ARRAY_SIZE(oid_data_ok); i++) {
		torture_assert(tctx, ber_write_OID_String(mem_ctx, &blob, data[i].oid),
				"ber_write_OID_String failed");

		hex_str = hex_encode_talloc(mem_ctx, blob.data, blob.length);
		torture_assert(tctx, hex_str, "No memory!");

		torture_assert(tctx, strequal(data[i].bin_oid, hex_str),
				talloc_asprintf(mem_ctx,
						"Failed: oid=%s, bin_oid:%s",
						data[i].oid, data[i].bin_oid));
	}

	/* check for invalid OIDs */
	for (i = 0; i < ARRAY_SIZE(oid_data_err); i++) {
		torture_assert(tctx,
			       !ber_write_OID_String(mem_ctx, &blob, oid_data_err[i]),
			       talloc_asprintf(mem_ctx,
					       "Should fail for [%s] -> %s",
					       oid_data_err[i],
					       hex_encode_talloc(mem_ctx, blob.data, blob.length)));
	}

	talloc_free(mem_ctx);

	return true;
}

/* Testing ber_read_OID_String() function */
static bool test_ber_read_OID_String(struct torture_context *tctx)
{
	int i;
	char *oid;
	DATA_BLOB oid_blob;
	TALLOC_CTX *mem_ctx;
	const struct oid_data *data = oid_data_ok;

	mem_ctx = talloc_new(tctx);

	for (i = 0; i < ARRAY_SIZE(oid_data_ok); i++) {
		oid_blob = strhex_to_data_blob(mem_ctx, data[i].bin_oid);

		torture_assert(tctx, ber_read_OID_String(mem_ctx, oid_blob, &oid),
				"ber_read_OID_String failed");

		torture_assert(tctx, strequal(data[i].oid, oid),
				talloc_asprintf(mem_ctx,
						"Failed: oid=%s, bin_oid:%s",
						data[i].oid, data[i].bin_oid));
	}

	talloc_free(mem_ctx);

	return true;
}

/* Testing ber_write_partial_OID_String() function */
static bool test_ber_write_partial_OID_String(struct torture_context *tctx)
{
	int i;
	char *hex_str;
	DATA_BLOB blob;
	TALLOC_CTX *mem_ctx;
	const struct oid_data *data = oid_data_ok;

	mem_ctx = talloc_new(tctx);

	/* ber_write_partial_OID_String() should work with not partial OIDs also */
	for (i = 0; i < ARRAY_SIZE(oid_data_ok); i++) {
		torture_assert(tctx, ber_write_partial_OID_String(mem_ctx, &blob, data[i].oid),
				"ber_write_partial_OID_String failed");

		hex_str = hex_encode_talloc(mem_ctx, blob.data, blob.length);
		torture_assert(tctx, hex_str, "No memory!");

		torture_assert(tctx, strequal(data[i].bin_oid, hex_str),
				talloc_asprintf(mem_ctx,
						"Failed: oid=%s, bin_oid:%s",
						data[i].oid, data[i].bin_oid));
	}

	/* ber_write_partial_OID_String() test with partial OIDs */
	data = partial_oid_data_ok;
	for (i = 0; i < ARRAY_SIZE(partial_oid_data_ok); i++) {
		torture_assert(tctx, ber_write_partial_OID_String(mem_ctx, &blob, data[i].oid),
				"ber_write_partial_OID_String failed");

		hex_str = hex_encode_talloc(mem_ctx, blob.data, blob.length);
		torture_assert(tctx, hex_str, "No memory!");

		torture_assert(tctx, strequal(data[i].bin_oid, hex_str),
				talloc_asprintf(mem_ctx,
						"Failed: oid=%s, bin_oid:%s",
						data[i].oid, data[i].bin_oid));
	}

	talloc_free(mem_ctx);

	return true;
}

/* Testing ber_read_partial_OID_String() function */
static bool test_ber_read_partial_OID_String(struct torture_context *tctx)
{
	int i;
	char *oid;
	DATA_BLOB oid_blob;
	TALLOC_CTX *mem_ctx;
	const struct oid_data *data = oid_data_ok;

	mem_ctx = talloc_new(tctx);

	/* ber_read_partial_OID_String() should work with not partial OIDs also */
	for (i = 0; i < ARRAY_SIZE(oid_data_ok); i++) {
		oid_blob = strhex_to_data_blob(mem_ctx, data[i].bin_oid);

		torture_assert(tctx, ber_read_partial_OID_String(mem_ctx, oid_blob, &oid),
				"ber_read_partial_OID_String failed");

		torture_assert(tctx, strequal(data[i].oid, oid),
				talloc_asprintf(mem_ctx,
						"Failed: oid=%s, bin_oid:%s",
						data[i].oid, data[i].bin_oid));
	}

	/* ber_read_partial_OID_String() test with partial OIDs */
	data = partial_oid_data_ok;
	for (i = 0; i < ARRAY_SIZE(partial_oid_data_ok); i++) {
		oid_blob = strhex_to_data_blob(mem_ctx, data[i].bin_oid);

		torture_assert(tctx, ber_read_partial_OID_String(mem_ctx, oid_blob, &oid),
				"ber_read_partial_OID_String failed");

		torture_assert(tctx, strequal(data[i].oid, oid),
				talloc_asprintf(mem_ctx,
						"Failed: oid=%s, bin_oid:%s",
						data[i].oid, data[i].bin_oid));
	}

	talloc_free(mem_ctx);

	return true;
}


/* LOCAL-ASN1 test suite creation */
struct torture_suite *torture_local_util_asn1(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "asn1");

	torture_suite_add_simple_test(suite, "ber_write_OID_String",
				      test_ber_write_OID_String);

	torture_suite_add_simple_test(suite, "ber_read_OID_String",
				      test_ber_read_OID_String);

	torture_suite_add_simple_test(suite, "ber_write_partial_OID_String",
				      test_ber_write_partial_OID_String);

	torture_suite_add_simple_test(suite, "ber_read_partial_OID_String",
				      test_ber_read_partial_OID_String);

	return suite;
}
