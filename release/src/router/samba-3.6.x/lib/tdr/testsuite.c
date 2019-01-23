/* 
   Unix SMB/CIFS implementation.
   test suite for basic tdr functions

   Copyright (C) Jelmer Vernooij 2007
   
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
#include "lib/tdr/tdr.h"

static bool test_push_uint8(struct torture_context *tctx)
{
	uint8_t v = 4;
	struct tdr_push *tdr = tdr_push_init(tctx);

	torture_assert_ntstatus_ok(tctx, tdr_push_uint8(tdr, &v), "push failed");
	torture_assert_int_equal(tctx, tdr->data.length, 1, "length incorrect");
	torture_assert_int_equal(tctx, tdr->data.data[0], 4, "data incorrect");
	return true;
}

static bool test_pull_uint8(struct torture_context *tctx)
{
	uint8_t d = 2;
	uint8_t l;
	struct tdr_pull *tdr = tdr_pull_init(tctx);
	tdr->data.data = &d;
	tdr->data.length = 1;
	tdr->offset = 0;
	tdr->flags = 0;
	torture_assert_ntstatus_ok(tctx, tdr_pull_uint8(tdr, tctx, &l), 
							   "pull failed");
	torture_assert_int_equal(tctx, 1, tdr->offset, 
							 "offset invalid");
	return true;
}

static bool test_push_uint16(struct torture_context *tctx)
{
	uint16_t v = 0xF32;
	struct tdr_push *tdr = tdr_push_init(tctx);

	torture_assert_ntstatus_ok(tctx, tdr_push_uint16(tdr, &v), "push failed");
	torture_assert_int_equal(tctx, tdr->data.length, 2, "length incorrect");
	torture_assert_int_equal(tctx, tdr->data.data[0], 0x32, "data incorrect");
	torture_assert_int_equal(tctx, tdr->data.data[1], 0x0F, "data incorrect");
	return true;
}

static bool test_pull_uint16(struct torture_context *tctx)
{
	uint8_t d[2] = { 782 & 0xFF, (782 & 0xFF00) / 0x100 };
	uint16_t l;
	struct tdr_pull *tdr = tdr_pull_init(tctx);
	tdr->data.data = d;
	tdr->data.length = 2;
	tdr->offset = 0;
	tdr->flags = 0;
	torture_assert_ntstatus_ok(tctx, tdr_pull_uint16(tdr, tctx, &l), 
							   "pull failed");
	torture_assert_int_equal(tctx, 2, tdr->offset, "offset invalid");
	torture_assert_int_equal(tctx, 782, l, "right int read");
	return true;
}

static bool test_push_uint32(struct torture_context *tctx)
{
	uint32_t v = 0x100F32;
	struct tdr_push *tdr = tdr_push_init(tctx);

	torture_assert_ntstatus_ok(tctx, tdr_push_uint32(tdr, &v), "push failed");
	torture_assert_int_equal(tctx, tdr->data.length, 4, "length incorrect");
	torture_assert_int_equal(tctx, tdr->data.data[0], 0x32, "data incorrect");
	torture_assert_int_equal(tctx, tdr->data.data[1], 0x0F, "data incorrect");
	torture_assert_int_equal(tctx, tdr->data.data[2], 0x10, "data incorrect");
	torture_assert_int_equal(tctx, tdr->data.data[3], 0x00, "data incorrect");
	return true;
}

static bool test_pull_uint32(struct torture_context *tctx)
{
	uint8_t d[4] = { 782 & 0xFF, (782 & 0xFF00) / 0x100, 0, 0 };
	uint32_t l;
	struct tdr_pull *tdr = tdr_pull_init(tctx);
	tdr->data.data = d;
	tdr->data.length = 4;
	tdr->offset = 0;
	tdr->flags = 0;
	torture_assert_ntstatus_ok(tctx, tdr_pull_uint32(tdr, tctx, &l), 
							   "pull failed");
	torture_assert_int_equal(tctx, 4, tdr->offset, "offset invalid");
	torture_assert_int_equal(tctx, 782, l, "right int read");
	return true;
}

static bool test_pull_charset(struct torture_context *tctx)
{
	struct tdr_pull *tdr = tdr_pull_init(tctx);
	const char *l = NULL;
	tdr->data.data = (uint8_t *)talloc_strdup(tctx, "bla");
	tdr->data.length = 4;
	tdr->offset = 0;
	tdr->flags = 0;
	torture_assert_ntstatus_ok(tctx, tdr_pull_charset(tdr, tctx, &l, -1, 1, CH_DOS), 
							   "pull failed");
	torture_assert_int_equal(tctx, 4, tdr->offset, "offset invalid");
	torture_assert_str_equal(tctx, "bla", l, "right int read");

	tdr->offset = 0;
	torture_assert_ntstatus_ok(tctx, tdr_pull_charset(tdr, tctx, &l, 2, 1, CH_UNIX), 
							   "pull failed");
	torture_assert_int_equal(tctx, 2, tdr->offset, "offset invalid");
	torture_assert_str_equal(tctx, "bl", l, "right int read");

	return true;
}

static bool test_pull_charset_empty(struct torture_context *tctx)
{
	struct tdr_pull *tdr = tdr_pull_init(tctx);
	const char *l = NULL;
	tdr->data.data = (uint8_t *)talloc_strdup(tctx, "bla");
	tdr->data.length = 4;
	tdr->offset = 0;
	tdr->flags = 0;
	torture_assert_ntstatus_ok(tctx, tdr_pull_charset(tdr, tctx, &l, 0, 1, CH_DOS), 
							   "pull failed");
	torture_assert_int_equal(tctx, 0, tdr->offset, "offset invalid");
	torture_assert_str_equal(tctx, "", l, "right string read");

	return true;
}



static bool test_push_charset(struct torture_context *tctx)
{
	const char *l = "bloe";
	struct tdr_push *tdr = tdr_push_init(tctx);
	torture_assert_ntstatus_ok(tctx, tdr_push_charset(tdr, &l, 4, 1, CH_UTF8), 
							   "push failed");
	torture_assert_int_equal(tctx, 4, tdr->data.length, "offset invalid");
	torture_assert(tctx, strncmp("bloe", (const char *)tdr->data.data, 4) == 0, "right string push");

	torture_assert_ntstatus_ok(tctx, tdr_push_charset(tdr, &l, -1, 1, CH_UTF8), 
							   "push failed");
	torture_assert_int_equal(tctx, 9, tdr->data.length, "offset invalid");
	torture_assert_str_equal(tctx, "bloe", (const char *)tdr->data.data+4, "right string read");

	return true;
}

struct torture_suite *torture_local_tdr(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "tdr");

	torture_suite_add_simple_test(suite, "pull_uint8", test_pull_uint8);
	torture_suite_add_simple_test(suite, "push_uint8", test_push_uint8);
	
	torture_suite_add_simple_test(suite, "pull_uint16", test_pull_uint16);
	torture_suite_add_simple_test(suite, "push_uint16", test_push_uint16);

	torture_suite_add_simple_test(suite, "pull_uint32", test_pull_uint32);
	torture_suite_add_simple_test(suite, "push_uint32", test_push_uint32);

	torture_suite_add_simple_test(suite, "pull_charset", test_pull_charset);
	torture_suite_add_simple_test(suite, "pull_charset", test_pull_charset_empty);
	torture_suite_add_simple_test(suite, "push_charset", test_push_charset);

	return suite;
}
