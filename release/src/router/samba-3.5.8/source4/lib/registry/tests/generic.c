/*
   Unix SMB/CIFS implementation.

   local testing of registry library

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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "lib/registry/registry.h"
#include "torture/torture.h"
#include "librpc/gen_ndr/winreg.h"
#include "param/param.h"

struct torture_suite *torture_registry_hive(TALLOC_CTX *mem_ctx);
struct torture_suite *torture_registry_registry(TALLOC_CTX *mem_ctx);
struct torture_suite *torture_registry_diff(TALLOC_CTX *mem_ctx);

static bool test_str_regtype(struct torture_context *ctx)
{
	torture_assert_str_equal(ctx, str_regtype(1),
				 "REG_SZ", "REG_SZ failed");
	torture_assert_str_equal(ctx, str_regtype(4),
				 "REG_DWORD", "REG_DWORD failed");

	return true;
}


static bool test_reg_val_data_string_dword(struct torture_context *ctx)
{
	uint32_t d = 0x20;
	DATA_BLOB db = { (uint8_t *)&d, sizeof(d) };
	torture_assert_str_equal(ctx, "0x20",
				 reg_val_data_string(ctx, lp_iconv_convenience(ctx->lp_ctx), REG_DWORD, db),
				 "dword failed");
	return true;
}

static bool test_reg_val_data_string_sz(struct torture_context *ctx)
{
	DATA_BLOB db;
	convert_string_talloc_convenience(ctx, lp_iconv_convenience(ctx->lp_ctx), CH_UTF8, CH_UTF16,
					  "bla", 3, (void **)&db.data, &db.length, false);
	torture_assert_str_equal(ctx, "bla",
				 reg_val_data_string(ctx, lp_iconv_convenience(ctx->lp_ctx), REG_SZ, db),
				 "sz failed");
	db.length = 4;
	torture_assert_str_equal(ctx, "bl",
				 reg_val_data_string(ctx, lp_iconv_convenience(ctx->lp_ctx), REG_SZ, db),
				 "sz failed");
	return true;
}

static bool test_reg_val_data_string_binary(struct torture_context *ctx)
{
	uint8_t x[] = { 0x1, 0x2, 0x3, 0x4 };
	DATA_BLOB db = { x, 4 };
	torture_assert_str_equal(ctx, "01020304",
				 reg_val_data_string(ctx, lp_iconv_convenience(ctx->lp_ctx), REG_BINARY, db),
				 "binary failed");
	return true;
}


static bool test_reg_val_data_string_empty(struct torture_context *ctx)
{
	DATA_BLOB db = { NULL, 0 };
	torture_assert_str_equal(ctx, "",
				 reg_val_data_string(ctx, lp_iconv_convenience(ctx->lp_ctx), REG_BINARY, db),
				 "empty failed");
	return true;
}

static bool test_reg_val_description(struct torture_context *ctx)
{
	DATA_BLOB data;
	convert_string_talloc_convenience(ctx, lp_iconv_convenience(ctx->lp_ctx), CH_UTF8, CH_UTF16,
					    "stationary traveller",
					    strlen("stationary traveller"),
					    (void **)&data.data, &data.length, false);
	torture_assert_str_equal(ctx, "camel = REG_SZ : stationary traveller",
				 reg_val_description(ctx, lp_iconv_convenience(ctx->lp_ctx), "camel", REG_SZ, data),
				 "reg_val_description failed");
	return true;
}


static bool test_reg_val_description_nullname(struct torture_context *ctx)
{
	DATA_BLOB data;
	convert_string_talloc_convenience(ctx, lp_iconv_convenience(ctx->lp_ctx), CH_UTF8, CH_UTF16,
					    "west berlin",
					    strlen("west berlin"),
					    (void **)&data.data, &data.length, false);
	torture_assert_str_equal(ctx, "<No Name> = REG_SZ : west berlin",
				 reg_val_description(ctx, lp_iconv_convenience(ctx->lp_ctx), NULL, REG_SZ, data),
				 "description with null name failed");
	return true;
}

struct torture_suite *torture_registry(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "REGISTRY");
	torture_suite_add_simple_test(suite, "str_regtype",
				      test_str_regtype);
	torture_suite_add_simple_test(suite, "reg_val_data_string dword",
				      test_reg_val_data_string_dword);
	torture_suite_add_simple_test(suite, "reg_val_data_string sz",
				      test_reg_val_data_string_sz);
	torture_suite_add_simple_test(suite, "reg_val_data_string binary",
				      test_reg_val_data_string_binary);
	torture_suite_add_simple_test(suite, "reg_val_data_string empty",
				      test_reg_val_data_string_empty);
	torture_suite_add_simple_test(suite, "reg_val_description",
				      test_reg_val_description);
	torture_suite_add_simple_test(suite, "reg_val_description null",
				      test_reg_val_description_nullname);

	torture_suite_add_suite(suite, torture_registry_hive(suite));
	torture_suite_add_suite(suite, torture_registry_registry(suite));
	torture_suite_add_suite(suite, torture_registry_diff(suite));

	return suite;
}
