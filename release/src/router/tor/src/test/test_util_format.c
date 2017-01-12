/* Copyright (c) 2010-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"
#include "or.h"

#include "test.h"

#define UTIL_FORMAT_PRIVATE
#include "util_format.h"

#define NS_MODULE util_format

#if !defined(HAVE_HTONLL) && !defined(htonll)
#ifdef WORDS_BIGENDIAN
#define htonll(x) (x)
#else
static uint64_t
htonll(uint64_t a)
{
  return htonl((uint32_t)(a>>32)) | (((uint64_t)htonl((uint32_t)a))<<32);
}
#endif
#endif

static void
test_util_format_unaligned_accessors(void *ignored)
{
  (void)ignored;
  char buf[9] = "onionsoup"; // 6f6e696f6e736f7570

  tt_u64_op(get_uint64(buf+1), OP_EQ, htonll(U64_LITERAL(0x6e696f6e736f7570)));
  tt_uint_op(get_uint32(buf+1), OP_EQ, htonl(0x6e696f6e));
  tt_uint_op(get_uint16(buf+1), OP_EQ, htons(0x6e69));
  tt_uint_op(get_uint8(buf+1), OP_EQ, 0x6e);

  set_uint8(buf+7, 0x61);
  tt_mem_op(buf, OP_EQ, "onionsoap", 9);

  set_uint16(buf+6, htons(0x746f));
  tt_mem_op(buf, OP_EQ, "onionstop", 9);

  set_uint32(buf+1, htonl(0x78696465));
  tt_mem_op(buf, OP_EQ, "oxidestop", 9);

  set_uint64(buf+1, htonll(U64_LITERAL(0x6266757363617465)));
  tt_mem_op(buf, OP_EQ, "obfuscate", 9);
 done:
  ;
}

static void
test_util_format_base64_encode(void *ignored)
{
  (void)ignored;
  int res;
  int i;
  char *src;
  char *dst;

  src = tor_malloc_zero(256);
  dst = tor_malloc_zero(1000);

  for (i=0;i<256;i++) {
    src[i] = (char)i;
  }

  res = base64_encode(NULL, 1, src, 1, 0);
  tt_int_op(res, OP_EQ, -1);

  res = base64_encode(dst, 1, NULL, 1, 0);
  tt_int_op(res, OP_EQ, -1);

  res = base64_encode(dst, 1, src, 10, 0);
  tt_int_op(res, OP_EQ, -1);

  res = base64_encode(dst, SSIZE_MAX-1, src, 1, 0);
  tt_int_op(res, OP_EQ, -1);

  res = base64_encode(dst, SSIZE_MAX-1, src, 10, 0);
  tt_int_op(res, OP_EQ, -1);

  res = base64_encode(dst, 1000, src, 256, 0);
  tt_int_op(res, OP_EQ, 344);
  tt_str_op(dst, OP_EQ, "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh"
            "8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZH"
            "SElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3"
            "BxcnN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeY"
            "mZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6/wM"
            "HCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj5OXm5+jp"
            "6uvs7e7v8PHy8/T19vf4+fr7/P3+/w==");

  res = base64_encode(dst, 1000, src, 256, BASE64_ENCODE_MULTILINE);
  tt_int_op(res, OP_EQ, 350);
  tt_str_op(dst, OP_EQ,
          "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4v\n"
          "MDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5f\n"
          "YGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6P\n"
          "kJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6/\n"
          "wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj5OXm5+jp6uvs7e7v\n"
          "8PHy8/T19vf4+fr7/P3+/w==\n");

  res = base64_encode(dst, 1000, src+1, 255, BASE64_ENCODE_MULTILINE);
  tt_int_op(res, OP_EQ, 346);

  for (i = 0;i<50;i++) {
    src[i] = 0;
  }
  src[50] = (char)255;
  src[51] = (char)255;
  src[52] = (char)255;
  src[53] = (char)255;

  res = base64_encode(dst, 1000, src, 54, BASE64_ENCODE_MULTILINE);
  tt_int_op(res, OP_EQ, 74);

  res = base64_encode(dst, 1000, src+1, 53, BASE64_ENCODE_MULTILINE);
  tt_int_op(res, OP_EQ, 74);

  res = base64_encode(dst, 1000, src+2, 52, BASE64_ENCODE_MULTILINE);
  tt_int_op(res, OP_EQ, 74);

  res = base64_encode(dst, 1000, src+3, 51, BASE64_ENCODE_MULTILINE);
  tt_int_op(res, OP_EQ, 70);

  res = base64_encode(dst, 1000, src+4, 50, BASE64_ENCODE_MULTILINE);
  tt_int_op(res, OP_EQ, 70);

  res = base64_encode(dst, 1000, src+5, 49, BASE64_ENCODE_MULTILINE);
  tt_int_op(res, OP_EQ, 70);

  res = base64_encode(dst, 1000, src+6, 48, BASE64_ENCODE_MULTILINE);
  tt_int_op(res, OP_EQ, 65);

  res = base64_encode(dst, 1000, src+7, 47, BASE64_ENCODE_MULTILINE);
  tt_int_op(res, OP_EQ, 65);

  res = base64_encode(dst, 1000, src+8, 46, BASE64_ENCODE_MULTILINE);
  tt_int_op(res, OP_EQ, 65);

 done:
  tor_free(src);
  tor_free(dst);
}

static void
test_util_format_base64_decode_nopad(void *ignored)
{
  (void)ignored;
  int res;
  int i;
  char *src;
  uint8_t *dst, *real_dst;
  uint8_t expected[] = {0x65, 0x78, 0x61, 0x6D, 0x70, 0x6C, 0x65};
  char real_src[] = "ZXhhbXBsZQ";

  src = tor_malloc_zero(256);
  dst = tor_malloc_zero(1000);
  real_dst = tor_malloc_zero(10);

  for (i=0;i<256;i++) {
    src[i] = (char)i;
  }

  res = base64_decode_nopad(dst, 1, src, SIZE_T_CEILING);
  tt_int_op(res, OP_EQ, -1);

  res = base64_decode_nopad(dst, 1, src, 5);
  tt_int_op(res, OP_EQ, -1);

  const char *s = "SGVsbG8gd29ybGQ";
  res = base64_decode_nopad(dst, 1000, s, strlen(s));
  tt_int_op(res, OP_EQ, 11);
  tt_mem_op(dst, OP_EQ, "Hello world", 11);

  s = "T3BhIG11bmRv";
  res = base64_decode_nopad(dst, 9, s, strlen(s));
  tt_int_op(res, OP_EQ, 9);
  tt_mem_op(dst, OP_EQ, "Opa mundo", 9);

  res = base64_decode_nopad(real_dst, 10, real_src, 10);
  tt_int_op(res, OP_EQ, 7);
  tt_mem_op(real_dst, OP_EQ, expected, 7);

 done:
  tor_free(src);
  tor_free(dst);
  tor_free(real_dst);
}

static void
test_util_format_base64_decode(void *ignored)
{
  (void)ignored;
  int res;
  int i;
  char *src;
  char *dst, *real_dst;
  uint8_t expected[] = {0x65, 0x78, 0x61, 0x6D, 0x70, 0x6C, 0x65};
  char real_src[] = "ZXhhbXBsZQ==";

  src = tor_malloc_zero(256);
  dst = tor_malloc_zero(1000);
  real_dst = tor_malloc_zero(10);

  for (i=0;i<256;i++) {
    src[i] = (char)i;
  }

  res = base64_decode(dst, 1, src, SIZE_T_CEILING);
  tt_int_op(res, OP_EQ, -1);

  res = base64_decode(dst, SIZE_T_CEILING+1, src, 10);
  tt_int_op(res, OP_EQ, -1);

  const char *s = "T3BhIG11bmRv";
  res = base64_decode(dst, 9, s, strlen(s));
  tt_int_op(res, OP_EQ, 9);
  tt_mem_op(dst, OP_EQ, "Opa mundo", 9);

  memset(dst, 0, 1000);
  res = base64_decode(dst, 100, s, strlen(s));
  tt_int_op(res, OP_EQ, 9);
  tt_mem_op(dst, OP_EQ, "Opa mundo", 9);

  s = "SGVsbG8gd29ybGQ=";
  res = base64_decode(dst, 100, s, strlen(s));
  tt_int_op(res, OP_EQ, 11);
  tt_mem_op(dst, OP_EQ, "Hello world", 11);

  res = base64_decode(real_dst, 10, real_src, 10);
  tt_int_op(res, OP_EQ, 7);
  tt_mem_op(real_dst, OP_EQ, expected, 7);

 done:
  tor_free(src);
  tor_free(dst);
  tor_free(real_dst);
}

static void
test_util_format_base16_decode(void *ignored)
{
  (void)ignored;
  int res;
  int i;
  char *src;
  char *dst, *real_dst;
  char expected[] = {0x65, 0x78, 0x61, 0x6D, 0x70, 0x6C, 0x65};
  char real_src[] = "6578616D706C65";

  src = tor_malloc_zero(256);
  dst = tor_malloc_zero(1000);
  real_dst = tor_malloc_zero(10);

  for (i=0;i<256;i++) {
    src[i] = (char)i;
  }

  res = base16_decode(dst, 3, src, 3);
  tt_int_op(res, OP_EQ, -1);

  res = base16_decode(dst, 1, src, 10);
  tt_int_op(res, OP_EQ, -1);

  res = base16_decode(dst, ((size_t)INT_MAX)+1, src, 10);
  tt_int_op(res, OP_EQ, -1);

  res = base16_decode(dst, 1000, "", 0);
  tt_int_op(res, OP_EQ, 0);

  res = base16_decode(dst, 1000, "aabc", 4);
  tt_int_op(res, OP_EQ, 2);
  tt_mem_op(dst, OP_EQ, "\xaa\xbc", 2);

  res = base16_decode(dst, 1000, "aabcd", 6);
  tt_int_op(res, OP_EQ, -1);

  res = base16_decode(dst, 1000, "axxx", 4);
  tt_int_op(res, OP_EQ, -1);

  res = base16_decode(real_dst, 10, real_src, 14);
  tt_int_op(res, OP_EQ, 7);
  tt_mem_op(real_dst, OP_EQ, expected, 7);

 done:
  tor_free(src);
  tor_free(dst);
  tor_free(real_dst);
}

static void
test_util_format_base32_encode(void *arg)
{
  (void) arg;
  size_t real_dstlen = 32;
  char *dst = tor_malloc_zero(real_dstlen);

  /* Basic use case that doesn't require a source length correction. */
  {
    /* Length of 10 bytes. */
    const char *src = "blahbleh12";
    size_t srclen = strlen(src);
    /* Expected result encoded base32. This was created using python as
     * such (and same goes for all test case.):
     *
     *  b = bytes("blahbleh12", 'utf-8')
     *  base64.b32encode(b)
     *  (result in lower case)
     */
    const char *expected = "mjwgc2dcnrswqmjs";

    base32_encode(dst, base32_encoded_size(srclen), src, srclen);
    tt_mem_op(expected, OP_EQ, dst, strlen(expected));
    /* Encode but to a larger size destination. */
    memset(dst, 0, real_dstlen);
    base32_encode(dst, real_dstlen, src, srclen);
    tt_mem_op(expected, OP_EQ, dst, strlen(expected));
  }

  /* Non multiple of 5 for the source buffer length. */
  {
    /* Length of 8 bytes. */
    const char *expected = "mjwgc2dcnrswq";
    const char *src = "blahbleh";
    size_t srclen = strlen(src);

    memset(dst, 0, real_dstlen);
    base32_encode(dst, base32_encoded_size(srclen), src, srclen);
    tt_mem_op(expected, OP_EQ, dst, strlen(expected));
  }

 done:
  tor_free(dst);
}

static void
test_util_format_base32_decode(void *arg)
{
  (void) arg;
  int ret;
  size_t real_dstlen = 32;
  char *dst = tor_malloc_zero(real_dstlen);

  /* Basic use case. */
  {
    /* Length of 10 bytes. */
    const char *expected = "blahbleh12";
    /* Expected result encoded base32. */
    const char *src = "mjwgc2dcnrswqmjs";

    ret = base32_decode(dst, strlen(expected), src, strlen(src));
    tt_int_op(ret, ==, 0);
    tt_str_op(expected, OP_EQ, dst);
  }

  /* Non multiple of 5 for the source buffer length. */
  {
    /* Length of 8 bytes. */
    const char *expected = "blahbleh";
    const char *src = "mjwgc2dcnrswq";

    ret = base32_decode(dst, strlen(expected), src, strlen(src));
    tt_int_op(ret, ==, 0);
    tt_mem_op(expected, OP_EQ, dst, strlen(expected));
  }

  /* Invalid values. */
  {
    /* Invalid character '#'. */
    ret = base32_decode(dst, real_dstlen, "#abcde", 6);
    tt_int_op(ret, ==, -1);
    /* Make sure the destination buffer has been zeroed even on error. */
    tt_int_op(tor_mem_is_zero(dst, real_dstlen), ==, 1);
  }

 done:
  tor_free(dst);
}

struct testcase_t util_format_tests[] = {
  { "unaligned_accessors", test_util_format_unaligned_accessors, 0,
    NULL, NULL },
  { "base64_encode", test_util_format_base64_encode, 0, NULL, NULL },
  { "base64_decode_nopad", test_util_format_base64_decode_nopad, 0,
    NULL, NULL },
  { "base64_decode", test_util_format_base64_decode, 0, NULL, NULL },
  { "base16_decode", test_util_format_base16_decode, 0, NULL, NULL },
  { "base32_encode", test_util_format_base32_encode, 0,
    NULL, NULL },
  { "base32_decode", test_util_format_base32_decode, 0,
    NULL, NULL },
  END_OF_TESTCASES
};

