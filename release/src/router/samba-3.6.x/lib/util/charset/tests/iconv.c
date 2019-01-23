/* 
   Unix SMB/CIFS implementation.

   local testing of iconv routines. This tests the system iconv code against
   the built-in iconv code

   Copyright (C) Andrew Tridgell 2004
   
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
#include "system/iconv.h"
#include "system/time.h"
#include "libcli/raw/libcliraw.h"
#include "param/param.h"
#include "torture/util.h"
#include "talloc.h"

#if HAVE_NATIVE_ICONV

static bool iconv_untestable(struct torture_context *tctx)
{
	iconv_t cd;

	if (!lpcfg_parm_bool(tctx->lp_ctx, NULL, "iconv", "native", true))
		torture_skip(tctx, "system iconv disabled - skipping test");

	cd = iconv_open("UTF-16LE", "UCS-4LE");
	if (cd == (iconv_t)-1)
		torture_skip(tctx, "unable to test - system iconv library does not support UTF-16LE -> UCS-4LE");
	iconv_close(cd);

	cd = iconv_open("UTF-16LE", "CP850");
	if (cd == (iconv_t)-1)
		torture_skip(tctx, "unable to test - system iconv library does not support UTF-16LE -> CP850\n");
	iconv_close(cd);

	return false;
}

/*
  generate a UTF-16LE buffer for a given unicode codepoint
*/
static int gen_codepoint_utf16(unsigned int codepoint,
			       char *buf, size_t *size)
{
	static iconv_t cd;
	uint8_t in[4];
	char *ptr_in;
	size_t size_in, size_out, ret;
	if (!cd) {
		cd = iconv_open("UTF-16LE", "UCS-4LE");
		if (cd == (iconv_t)-1) {
			cd = NULL;
			return -1;
		}
	}

	in[0] = codepoint & 0xFF;
	in[1] = (codepoint>>8) & 0xFF;
	in[2] = (codepoint>>16) & 0xFF;
	in[3] = (codepoint>>24) & 0xFF;

	ptr_in = (char *)in;
	size_in = 4;
	size_out = 8;

	ret = iconv(cd, &ptr_in, &size_in, &buf, &size_out);

	*size = 8 - size_out;

	return ret;
}


/*
  work out the unicode codepoint of the first UTF-8 character in the buffer
*/
static unsigned int get_codepoint(char *buf, size_t size, const char *charset)
{
	iconv_t cd;
	uint8_t out[4];
	char *ptr_out;
	size_t size_out, size_in, ret;

	cd = iconv_open("UCS-4LE", charset);

	size_in = size;
	ptr_out = (char *)out;
	size_out = sizeof(out);
	memset(out, 0, sizeof(out));

	ret = iconv(cd, &buf, &size_in, &ptr_out, &size_out);

	iconv_close(cd);

	return out[0] | (out[1]<<8) | (out[2]<<16) | (out[3]<<24);
}

/*
  display a buffer with name prefix
*/
static void show_buf(const char *name, uint8_t *buf, size_t size)
{
	int i;
	printf("%s ", name);
	for (i=0;i<size;i++) {
		printf("%02x ", buf[i]);
	}
	printf("\n");
}

/*
  given a UTF-16LE buffer, test the system and built-in iconv code to
  make sure they do exactly the same thing in converting the buffer to
  "charset", then convert it back again and ensure we get the same
  buffer back
*/
static bool test_buffer(struct torture_context *test, 
			uint8_t *inbuf, size_t size, const char *charset)
{
	uint8_t buf1[1000], buf2[1000], buf3[1000];
	size_t outsize1, outsize2, outsize3;
	char *ptr_in;
	char *ptr_out;
	size_t size_in1, size_in2, size_in3;
	size_t ret1, ret2, ret3, len1, len2;
	int errno1, errno2;
	static iconv_t cd;
	static smb_iconv_t cd2, cd3;
	static const char *last_charset;

	if (cd && last_charset) {
		iconv_close(cd);
		smb_iconv_close(cd2);
		smb_iconv_close(cd3);
		cd = NULL;
	}

	if (!cd) {
		cd = iconv_open(charset, "UTF-16LE");
		if (cd == (iconv_t)-1) {
			torture_fail(test, 
				     talloc_asprintf(test, 
						     "failed to open %s to UTF-16LE",
						     charset));
		}
		cd2 = smb_iconv_open_ex(test, charset, "UTF-16LE", lpcfg_parm_bool(test->lp_ctx, NULL, "iconv", "native", true));
		cd3 = smb_iconv_open_ex(test, "UTF-16LE", charset, lpcfg_parm_bool(test->lp_ctx, NULL, "iconv", "native", true));
		last_charset = charset;
	}

	/* internal convert to charset - placing result in buf1 */
	ptr_in = (char *)inbuf;
	ptr_out = (char *)buf1;
	size_in1 = size;
	outsize1 = sizeof(buf1);

	memset(ptr_out, 0, outsize1);
	errno = 0;
	ret1 = smb_iconv(cd2, (const char **) &ptr_in, &size_in1, &ptr_out, &outsize1);
	errno1 = errno;

	/* system convert to charset - placing result in buf2 */
	ptr_in = (char *)inbuf;
	ptr_out = (char *)buf2;
	size_in2 = size;
	outsize2 = sizeof(buf2);
	
	memset(ptr_out, 0, outsize2);
	errno = 0;
	ret2 = iconv(cd, &ptr_in, &size_in2, &ptr_out, &outsize2);
	errno2 = errno;

	len1 = sizeof(buf1) - outsize1;
	len2 = sizeof(buf2) - outsize2;

	/* codepoints above 1M are not interesting for now */
	if (len2 > len1 && 
	    memcmp(buf1, buf2, len1) == 0 && 
	    get_codepoint((char *)(buf2+len1), len2-len1, charset) >= (1<<20)) {
		return true;
	}
	if (len1 > len2 && 
	    memcmp(buf1, buf2, len2) == 0 && 
	    get_codepoint((char *)(buf1+len2), len1-len2, charset) >= (1<<20)) {
		return true;
	}

	torture_assert_int_equal(test, ret1, ret2, "ret mismatch");

	if (errno1 != errno2) {
		show_buf(" rem1:", inbuf+(size-size_in1), size_in1);
		show_buf(" rem2:", inbuf+(size-size_in2), size_in2);
		torture_fail(test, talloc_asprintf(test, 
						   "e1=%d/%s e2=%d/%s", 
						   errno1, strerror(errno1), 
						   errno2, strerror(errno2)));
	}
	
	torture_assert_int_equal(test, outsize1, outsize2, "outsize mismatch");
	
	torture_assert_int_equal(test, size_in1, size_in2, "size_in mismatch");

	if (len1 != len2 ||
	    memcmp(buf1, buf2, len1) != 0) {
		torture_comment(test, "size=%d ret1=%d ret2=%d", (int)size, (int)ret1, (int)ret2);
		show_buf(" IN1:", inbuf, size-size_in1);
		show_buf(" IN2:", inbuf, size-size_in2);
		show_buf("OUT1:", buf1, len1);
		show_buf("OUT2:", buf2, len2);
		if (len2 > len1 && memcmp(buf1, buf2, len1) == 0) {
			torture_comment(test, "next codepoint is %u", 
			       get_codepoint((char *)(buf2+len1), len2-len1, charset));
		}
		if (len1 > len2 && memcmp(buf1, buf2, len2) == 0) {
			torture_comment(test, "next codepoint is %u", 
			       get_codepoint((char *)(buf1+len2),len1-len2, charset));
		}

		torture_fail(test, "failed");
	}

	/* convert back to UTF-16, putting result in buf3 */
	size = size - size_in1;
	ptr_in = (char *)buf1;
	ptr_out = (char *)buf3;
	size_in3 = len1;
	outsize3 = sizeof(buf3);

	memset(ptr_out, 0, outsize3);
	ret3 = smb_iconv(cd3, (const char **) &ptr_in, &size_in3, &ptr_out, &outsize3);

	/* we only internally support the first 1M codepoints */
	if (outsize3 != sizeof(buf3) - size &&
	    get_codepoint((char *)(inbuf+sizeof(buf3) - outsize3), 
			  size - (sizeof(buf3) - outsize3),
			  "UTF-16LE") >= (1<<20)) {
		return true;
	}

	torture_assert_int_equal(test, ret3, 0, talloc_asprintf(test, 
								"pull failed - %s", strerror(errno)));

	if (strncmp(charset, "UTF", 3) != 0) {
		/* don't expect perfect mappings for non UTF charsets */
		return true;
	}


	torture_assert_int_equal(test, outsize3, sizeof(buf3) - size, 
		"wrong outsize3");
	
	if (memcmp(buf3, inbuf, size) != 0) {
		torture_comment(test, "pull bytes mismatch:");
		show_buf("inbuf", inbuf, size);
		show_buf(" buf3", buf3, sizeof(buf3) - outsize3);
		torture_comment(test, "next codepoint is %u\n", 
		       get_codepoint((char *)(inbuf+sizeof(buf3) - outsize3), 
				     size - (sizeof(buf3) - outsize3),
				     "UTF-16LE"));
		torture_fail(test, "");
	}

	return true;
}


/*
  test the push_codepoint() and next_codepoint() functions for a given
  codepoint
*/
static bool test_codepoint(struct torture_context *tctx, unsigned int codepoint)
{
	uint8_t buf[10];
	size_t size, size2;
	codepoint_t c;

	size = push_codepoint_convenience(lpcfg_iconv_convenience(tctx->lp_ctx), (char *)buf, codepoint);
	torture_assert(tctx, size != -1 || (codepoint >= 0xd800 && codepoint <= 0x10000), 
		       "Invalid Codepoint range");

	if (size == -1) return true;

	buf[size] = random();
	buf[size+1] = random();
	buf[size+2] = random();
	buf[size+3] = random();

	c = next_codepoint_convenience(lpcfg_iconv_convenience(tctx->lp_ctx), (char *)buf, &size2);

	torture_assert(tctx, c == codepoint, 
		       talloc_asprintf(tctx, 
				       "next_codepoint(%u) failed - gave %u", codepoint, c));

	torture_assert(tctx, size2 == size, 
			talloc_asprintf(tctx, "next_codepoint(%u) gave wrong size %d (should be %d)\n", 
		       codepoint, (int)size2, (int)size));

	return true;
}

static bool test_next_codepoint(struct torture_context *tctx)
{
	unsigned int codepoint;
	if (iconv_untestable(tctx))
		return true;

	for (codepoint=0;codepoint<(1<<20);codepoint++) {
		if (!test_codepoint(tctx, codepoint))
			return false;
	}
	return true;
}

static bool test_first_1m(struct torture_context *tctx)
{
	unsigned int codepoint;
	size_t size;
	unsigned char inbuf[1000];

	if (iconv_untestable(tctx))
		return true;

	for (codepoint=0;codepoint<(1<<20);codepoint++) {
		if (gen_codepoint_utf16(codepoint, (char *)inbuf, &size) != 0) {
			continue;
		}

		if (codepoint % 1000 == 0) {
			if (torture_setting_bool(tctx, "progress", true)) {
				torture_comment(tctx, "codepoint=%u   \r", codepoint);
				fflush(stdout);
			}
		}

		if (!test_buffer(tctx, inbuf, size, "UTF-8"))
			return false;
	}
	return true;
}

static bool test_random_5m(struct torture_context *tctx)
{
	unsigned char inbuf[1000];
	unsigned int i;

	if (iconv_untestable(tctx))
		return true;

	for (i=0;i<500000;i++) {
		size_t size;
		unsigned int c;

		if (i % 1000 == 0) {
			if (torture_setting_bool(tctx, "progress", true)) {
				torture_comment(tctx, "i=%u              \r", i);
				fflush(stdout);
			}
		}

		size = random() % 100;
		for (c=0;c<size;c++) {
			if (random() % 100 < 80) {
				inbuf[c] = random() % 128;
			} else {
				inbuf[c] = random();
			}
			if (random() % 10 == 0) {
				inbuf[c] |= 0xd8;
			}
			if (random() % 10 == 0) {
				inbuf[c] |= 0xdc;
			}
		}
		if (!test_buffer(tctx, inbuf, size, "UTF-8")) {
			printf("i=%d failed UTF-8\n", i);
			return false;
		}

		if (!test_buffer(tctx, inbuf, size, "CP850")) {
			printf("i=%d failed CP850\n", i);
			return false;
		}
	}
	return true;
}


static bool test_string2key(struct torture_context *tctx)
{
	uint16_t *buf;
	char *dest = NULL;
	TALLOC_CTX *mem_ctx = talloc_new(tctx);
	size_t len = (random()%1000)+1;
	const uint16_t in1[10] = { 'a', 0xd805, 'b', 0xdcf0, 'c', 0, 'd', 'e', 'f', 'g' };
	uint8_t le1[20];
	uint8_t *munged1;
	uint8_t *out1;
	size_t ret;
	int i;
	const char *correct = "a\357\277\275b\357\277\275c\001defg";

	buf = talloc_size(mem_ctx, len*2);
	generate_random_buffer((uint8_t *)buf, len*2);

	torture_comment(tctx, "converting random buffer\n");

	if (!convert_string_talloc(mem_ctx, CH_UTF16MUNGED, CH_UTF8, (void *)buf, len*2, (void**)&dest, &ret, false)) {
		torture_fail(tctx, "Failed to convert random buffer\n");
	}

	for (i=0;i<10;i++) {
		SSVAL(&le1[2*i], 0, in1[i]);
	}

	torture_comment(tctx, "converting fixed buffer to UTF16\n");

	if (!convert_string_talloc(mem_ctx, CH_UTF16MUNGED, CH_UTF16, (void *)le1, 20, (void**)&munged1, &ret, false)) {
		torture_fail(tctx, "Failed to convert fixed buffer to UTF16_MUNGED\n");
	}

	torture_assert(tctx, ret == 20, "conversion should give 20 bytes\n");

	torture_comment(tctx, "converting fixed buffer to UTF8\n");

	if (!convert_string_talloc(mem_ctx, CH_UTF16MUNGED, CH_UTF8, (void *)le1, 20, (void**)&out1, &ret, false)) {
		torture_fail(tctx, "Failed to convert fixed buffer to UTF8\n");
	}

	torture_assert(tctx, strcmp(correct, (const char *) out1) == 0,
		"conversion gave incorrect result\n");

	talloc_free(mem_ctx);

	return true;
}

struct torture_suite *torture_local_iconv(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "iconv");

	torture_suite_add_simple_test(suite, "string2key",
				      test_string2key);

	torture_suite_add_simple_test(suite, "next_codepoint()",
				      test_next_codepoint);

	torture_suite_add_simple_test(suite, "first 1M codepoints",
				      test_first_1m);

	torture_suite_add_simple_test(suite, "5M random UTF-16LE sequences",
				      test_random_5m);

	torture_suite_add_simple_test(suite, "string2key",
				      test_string2key);
	return suite;
}

#else

struct torture_suite *torture_local_iconv(TALLOC_CTX *mem_ctx) 
{
	printf("No native iconv library - can't run iconv test\n");
	return NULL;
}

#endif
