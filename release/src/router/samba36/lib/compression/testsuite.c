/* 
   Unix SMB/CIFS implementation.
   test suite for the compression functions

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
#include "talloc.h"
#include "mszip.h"
#include "lzxpress.h"

/*
  test lzxpress
 */
static bool test_lzxpress(struct torture_context *test)
{
	TALLOC_CTX *tmp_ctx = talloc_new(test);
	uint8_t *data;
	const char *fixed_data = "this is a test. and this is a test too";
	const uint8_t fixed_out[] = { 0x00, 0x20, 0x00, 0x04, 0x74, 0x68, 0x69, 0x73,
				      0x20, 0x10, 0x00, 0x61, 0x20, 0x74, 0x65, 0x73,
				      0x74, 0x2E, 0x20, 0x61, 0x6E, 0x64, 0x20, 0x9F,
				      0x00, 0x04, 0x20, 0x74, 0x6F, 0x6F, 0x00, 0x00,
				      0x00, 0x00 };
	ssize_t c_size;
	uint8_t *out, *out2;

	data = talloc_size(tmp_ctx, 1023);
	out  = talloc_size(tmp_ctx, 2048);
	memset(out, 0x42, talloc_get_size(out));

	torture_comment(test, "lzxpress fixed compression\n");
	c_size = lzxpress_compress((const uint8_t *)fixed_data,
				   strlen(fixed_data),
				   out,
				   talloc_get_size(out));

	torture_assert_int_equal(test, c_size, sizeof(fixed_out), "fixed lzxpress_compress size");
	torture_assert_mem_equal(test, out, fixed_out, c_size, "fixed lzxpress_compress data");

	torture_comment(test, "lzxpress fixed decompression\n");
	out2  = talloc_size(tmp_ctx, strlen(fixed_data));
	c_size = lzxpress_decompress(out,
				     sizeof(fixed_out),
				     out2,
				     talloc_get_size(out2));

	torture_assert_int_equal(test, c_size, strlen(fixed_data), "fixed lzxpress_decompress size");
	torture_assert_mem_equal(test, out2, fixed_data, c_size, "fixed lzxpress_decompress data");

	return true;
}


struct torture_suite *torture_local_compression(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "compression");

	torture_suite_add_simple_test(suite, "lzxpress", test_lzxpress);

	return suite;
}
