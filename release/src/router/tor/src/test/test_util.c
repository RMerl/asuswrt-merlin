/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"
#define COMPAT_PRIVATE
#define CONTROL_PRIVATE
#define MEMPOOL_PRIVATE
#define UTIL_PRIVATE
#include "or.h"
#include "config.h"
#include "control.h"
#include "test.h"
#ifdef ENABLE_MEMPOOLS
#include "mempool.h"
#endif /* ENABLE_MEMPOOLS */
#include "memarea.h"
#include "util_process.h"

#ifdef _WIN32
#include <tchar.h>
#endif
#include <math.h>

/* XXXX this is a minimal wrapper to make the unit tests compile with the
 * changed tor_timegm interface. */
static time_t
tor_timegm_wrapper(const struct tm *tm)
{
  time_t t;
  if (tor_timegm(tm, &t) < 0)
    return -1;
  return t;
}

#define tor_timegm tor_timegm_wrapper

static void
test_util_read_until_eof_impl(const char *fname, size_t file_len,
                              size_t read_limit)
{
  char *fifo_name = NULL;
  char *test_str = NULL;
  char *str = NULL;
  size_t sz = 9999999;
  int fd = -1;
  int r;

  fifo_name = tor_strdup(get_fname(fname));
  test_str = tor_malloc(file_len);
  crypto_rand(test_str, file_len);

  r = write_bytes_to_file(fifo_name, test_str, file_len, 1);
  tt_int_op(r, ==, 0);

  fd = open(fifo_name, O_RDONLY|O_BINARY);
  tt_int_op(fd, >=, 0);
  str = read_file_to_str_until_eof(fd, read_limit, &sz);
  tt_assert(str != NULL);

  if (read_limit < file_len)
    tt_int_op(sz, ==, read_limit);
  else
    tt_int_op(sz, ==, file_len);

  test_mem_op(test_str, ==, str, sz);
  test_assert(str[sz] == '\0');

 done:
  unlink(fifo_name);
  tor_free(fifo_name);
  tor_free(test_str);
  tor_free(str);
  if (fd >= 0)
    close(fd);
}

static void
test_util_read_file_eof_tiny_limit(void *arg)
{
  (void)arg;
  // purposely set limit shorter than what we wrote to the FIFO to
  // test the maximum, and that it puts the NUL in the right spot

  test_util_read_until_eof_impl("tor_test_fifo_tiny", 5, 4);
}

static void
test_util_read_file_eof_two_loops(void *arg)
{
  (void)arg;
  // write more than 1024 bytes to the FIFO to test two passes through
  // the loop in the method; if the re-alloc size is changed this
  // should be updated as well.

  test_util_read_until_eof_impl("tor_test_fifo_2k", 2048, 10000);
}

static void
test_util_read_file_eof_zero_bytes(void *arg)
{
  (void)arg;
  // zero-byte fifo
  test_util_read_until_eof_impl("tor_test_fifo_empty", 0, 10000);
}

/* Test the basic expected behaviour for write_chunks_to_file.
 * NOTE: This will need to be updated if we ever change the tempfile location
 * or extension */
static void
test_util_write_chunks_to_file(void *arg)
{
  char *fname = NULL;
  char *tempname = NULL;
  char *str = NULL;
  int r;
  struct stat st;

  /* These should be two different sizes to ensure the data is different
   * between the data file and the temp file's 'known string' */
  int temp_str_len = 1024;
  int data_str_len = 512;
  char *data_str = tor_malloc(data_str_len);
  char *temp_str = tor_malloc(temp_str_len);

  smartlist_t *chunks = smartlist_new();
  sized_chunk_t c = {data_str, data_str_len/2};
  sized_chunk_t c2 = {data_str + data_str_len/2, data_str_len/2};
  (void)arg;

  crypto_rand(temp_str, temp_str_len);
  crypto_rand(data_str, data_str_len);

  // Ensure it can write multiple chunks

  smartlist_add(chunks, &c);
  smartlist_add(chunks, &c2);

  /*
  * Check if it writes using a tempfile
  */
  fname = tor_strdup(get_fname("write_chunks_with_tempfile"));
  tor_asprintf(&tempname, "%s.tmp", fname);

  // write a known string to a file where the tempfile will be
  r = write_bytes_to_file(tempname, temp_str, temp_str_len, 1);
  tt_int_op(r, ==, 0);

  // call write_chunks_to_file
  r = write_chunks_to_file(fname, chunks, 1, 0);
  tt_int_op(r, ==, 0);

  // assert the file has been written (expected size)
  str = read_file_to_str(fname, RFTS_BIN, &st);
  tt_assert(str != NULL);
  tt_u64_op((uint64_t)st.st_size, ==, data_str_len);
  test_mem_op(data_str, ==, str, data_str_len);
  tor_free(str);

  // assert that the tempfile is removed (should not leave artifacts)
  str = read_file_to_str(tempname, RFTS_BIN|RFTS_IGNORE_MISSING, &st);
  tt_assert(str == NULL);

  // Remove old testfile for second test
  r = unlink(fname);
  tt_int_op(r, ==, 0);
  tor_free(fname);
  tor_free(tempname);

  /*
  *  Check if it skips using a tempfile with flags
  */
  fname = tor_strdup(get_fname("write_chunks_with_no_tempfile"));
  tor_asprintf(&tempname, "%s.tmp", fname);

  // write a known string to a file where the tempfile will be
  r = write_bytes_to_file(tempname, temp_str, temp_str_len, 1);
  tt_int_op(r, ==, 0);

  // call write_chunks_to_file with no_tempfile = true
  r = write_chunks_to_file(fname, chunks, 1, 1);
  tt_int_op(r, ==, 0);

  // assert the file has been written (expected size)
  str = read_file_to_str(fname, RFTS_BIN, &st);
  tt_assert(str != NULL);
  tt_u64_op((uint64_t)st.st_size, ==, data_str_len);
  test_mem_op(data_str, ==, str, data_str_len);
  tor_free(str);

  // assert the tempfile still contains the known string
  str = read_file_to_str(tempname, RFTS_BIN, &st);
  tt_assert(str != NULL);
  tt_u64_op((uint64_t)st.st_size, ==, temp_str_len);
  test_mem_op(temp_str, ==, str, temp_str_len);

 done:
  unlink(fname);
  unlink(tempname);
  smartlist_free(chunks);
  tor_free(fname);
  tor_free(tempname);
  tor_free(str);
  tor_free(data_str);
  tor_free(temp_str);
}

static void
test_util_time(void)
{
  struct timeval start, end;
  struct tm a_time;
  char timestr[128];
  time_t t_res;
  int i;
  struct timeval tv;

  /* Test tv_udiff */

  start.tv_sec = 5;
  start.tv_usec = 5000;

  end.tv_sec = 5;
  end.tv_usec = 5000;

  test_eq(0L, tv_udiff(&start, &end));

  end.tv_usec = 7000;

  test_eq(2000L, tv_udiff(&start, &end));

  end.tv_sec = 6;

  test_eq(1002000L, tv_udiff(&start, &end));

  end.tv_usec = 0;

  test_eq(995000L, tv_udiff(&start, &end));

  end.tv_sec = 4;

  test_eq(-1005000L, tv_udiff(&start, &end));

  /* Test tor_timegm */

  /* The test values here are confirmed to be correct on a platform
   * with a working timegm. */
  a_time.tm_year = 2003-1900;
  a_time.tm_mon = 7;
  a_time.tm_mday = 30;
  a_time.tm_hour = 6;
  a_time.tm_min = 14;
  a_time.tm_sec = 55;
  test_eq((time_t) 1062224095UL, tor_timegm(&a_time));
  a_time.tm_year = 2004-1900; /* Try a leap year, after feb. */
  test_eq((time_t) 1093846495UL, tor_timegm(&a_time));
  a_time.tm_mon = 1;          /* Try a leap year, in feb. */
  a_time.tm_mday = 10;
  test_eq((time_t) 1076393695UL, tor_timegm(&a_time));
  a_time.tm_mon = 0;
  a_time.tm_mday = 10;
  test_eq((time_t) 1073715295UL, tor_timegm(&a_time));
  a_time.tm_mon = 12;          /* Wrong month, it's 0-based */
  a_time.tm_mday = 10;
  test_eq((time_t) -1, tor_timegm(&a_time));
  a_time.tm_mon = -1;          /* Wrong month */
  a_time.tm_mday = 10;
  test_eq((time_t) -1, tor_timegm(&a_time));

  /* Test {format,parse}_rfc1123_time */

  format_rfc1123_time(timestr, 0);
  test_streq("Thu, 01 Jan 1970 00:00:00 GMT", timestr);
  format_rfc1123_time(timestr, (time_t)1091580502UL);
  test_streq("Wed, 04 Aug 2004 00:48:22 GMT", timestr);

  t_res = 0;
  i = parse_rfc1123_time(timestr, &t_res);
  test_eq(0,i);
  test_eq(t_res, (time_t)1091580502UL);
  /* The timezone doesn't matter */
  t_res = 0;
  test_eq(0, parse_rfc1123_time("Wed, 04 Aug 2004 00:48:22 ZUL", &t_res));
  test_eq(t_res, (time_t)1091580502UL);
  test_eq(-1, parse_rfc1123_time("Wed, zz Aug 2004 99-99x99 GMT", &t_res));
  test_eq(-1, parse_rfc1123_time("Wed, 32 Mar 2011 00:00:00 GMT", &t_res));
  test_eq(-1, parse_rfc1123_time("Wed, 30 Mar 2011 24:00:00 GMT", &t_res));
  test_eq(-1, parse_rfc1123_time("Wed, 30 Mar 2011 23:60:00 GMT", &t_res));
  test_eq(-1, parse_rfc1123_time("Wed, 30 Mar 2011 23:59:62 GMT", &t_res));
  test_eq(-1, parse_rfc1123_time("Wed, 30 Mar 1969 23:59:59 GMT", &t_res));
  test_eq(-1, parse_rfc1123_time("Wed, 30 Ene 2011 23:59:59 GMT", &t_res));
  test_eq(-1, parse_rfc1123_time("Wed, 30 Mar 2011 23:59:59 GM", &t_res));

#if 0
  /* This fails, I imagine it's important and should be fixed? */
  test_eq(-1, parse_rfc1123_time("Wed, 29 Feb 2011 16:00:00 GMT", &t_res));
  /* Why is this string valid (ie. the test fails because it doesn't
     return -1)? */
  test_eq(-1, parse_rfc1123_time("Wed, 30 Mar 2011 23:59:61 GMT", &t_res));
#endif

  /* Test parse_iso_time */

  t_res = 0;
  i = parse_iso_time("", &t_res);
  test_eq(-1, i);
  t_res = 0;
  i = parse_iso_time("2004-08-32 00:48:22", &t_res);
  test_eq(-1, i);
  t_res = 0;
  i = parse_iso_time("1969-08-03 00:48:22", &t_res);
  test_eq(-1, i);

  t_res = 0;
  i = parse_iso_time("2004-08-04 00:48:22", &t_res);
  test_eq(0,i);
  test_eq(t_res, (time_t)1091580502UL);
  t_res = 0;
  i = parse_iso_time("2004-8-4 0:48:22", &t_res);
  test_eq(0, i);
  test_eq(t_res, (time_t)1091580502UL);
  test_eq(-1, parse_iso_time("2004-08-zz 99-99x99 GMT", &t_res));
  test_eq(-1, parse_iso_time("2011-03-32 00:00:00 GMT", &t_res));
  test_eq(-1, parse_iso_time("2011-03-30 24:00:00 GMT", &t_res));
  test_eq(-1, parse_iso_time("2011-03-30 23:60:00 GMT", &t_res));
  test_eq(-1, parse_iso_time("2011-03-30 23:59:62 GMT", &t_res));
  test_eq(-1, parse_iso_time("1969-03-30 23:59:59 GMT", &t_res));
  test_eq(-1, parse_iso_time("2011-00-30 23:59:59 GMT", &t_res));
  test_eq(-1, parse_iso_time("2147483647-08-29 14:00:00", &t_res));
  test_eq(-1, parse_iso_time("2011-03-30 23:59", &t_res));

  /* Test tor_gettimeofday */

  end.tv_sec = 4;
  end.tv_usec = 999990;
  start.tv_sec = 1;
  start.tv_usec = 500;

  tor_gettimeofday(&start);
  /* now make sure time works. */
  tor_gettimeofday(&end);
  /* We might've timewarped a little. */
  tt_int_op(tv_udiff(&start, &end), >=, -5000);

  /* Test format_iso_time */

  tv.tv_sec = (time_t)1326296338;
  tv.tv_usec = 3060;
  format_iso_time(timestr, (time_t)tv.tv_sec);
  test_streq("2012-01-11 15:38:58", timestr);
  /* The output of format_local_iso_time will vary by timezone, and setting
     our timezone for testing purposes would be a nontrivial flaky pain.
     Skip this test for now.
  format_local_iso_time(timestr, tv.tv_sec);
  test_streq("2012-01-11 10:38:58", timestr);
  */
  format_iso_time_nospace(timestr, (time_t)tv.tv_sec);
  test_streq("2012-01-11T15:38:58", timestr);
  test_eq(strlen(timestr), ISO_TIME_LEN);
  format_iso_time_nospace_usec(timestr, &tv);
  test_streq("2012-01-11T15:38:58.003060", timestr);
  test_eq(strlen(timestr), ISO_TIME_USEC_LEN);

 done:
  ;
}

static void
test_util_parse_http_time(void *arg)
{
  struct tm a_time;
  char b[ISO_TIME_LEN+1];
  (void)arg;

#define T(s) do {                               \
    format_iso_time(b, tor_timegm(&a_time));    \
    tt_str_op(b, ==, (s));                      \
    b[0]='\0';                                  \
  } while (0)

  /* Test parse_http_time */

  test_eq(-1, parse_http_time("", &a_time));
  test_eq(-1, parse_http_time("Sunday, 32 Aug 2004 00:48:22 GMT", &a_time));
  test_eq(-1, parse_http_time("Sunday, 3 Aug 1869 00:48:22 GMT", &a_time));
  test_eq(-1, parse_http_time("Sunday, 32-Aug-94 00:48:22 GMT", &a_time));
  test_eq(-1, parse_http_time("Sunday, 3-Ago-04 00:48:22", &a_time));
  test_eq(-1, parse_http_time("Sunday, August the third", &a_time));
  test_eq(-1, parse_http_time("Wednesday,,04 Aug 1994 00:48:22 GMT", &a_time));

  test_eq(0, parse_http_time("Wednesday, 04 Aug 1994 00:48:22 GMT", &a_time));
  test_eq((time_t)775961302UL, tor_timegm(&a_time));
  T("1994-08-04 00:48:22");
  test_eq(0, parse_http_time("Wednesday, 4 Aug 1994 0:48:22 GMT", &a_time));
  test_eq((time_t)775961302UL, tor_timegm(&a_time));
  T("1994-08-04 00:48:22");
  test_eq(0, parse_http_time("Miercoles, 4 Aug 1994 0:48:22 GMT", &a_time));
  test_eq((time_t)775961302UL, tor_timegm(&a_time));
  T("1994-08-04 00:48:22");
  test_eq(0, parse_http_time("Wednesday, 04-Aug-94 00:48:22 GMT", &a_time));
  test_eq((time_t)775961302UL, tor_timegm(&a_time));
  T("1994-08-04 00:48:22");
  test_eq(0, parse_http_time("Wednesday, 4-Aug-94 0:48:22 GMT", &a_time));
  test_eq((time_t)775961302UL, tor_timegm(&a_time));
  T("1994-08-04 00:48:22");
  test_eq(0, parse_http_time("Miercoles, 4-Aug-94 0:48:22 GMT", &a_time));
  test_eq((time_t)775961302UL, tor_timegm(&a_time));
  T("1994-08-04 00:48:22");
  test_eq(0, parse_http_time("Wed Aug 04 00:48:22 1994", &a_time));
  test_eq((time_t)775961302UL, tor_timegm(&a_time));
  T("1994-08-04 00:48:22");
  test_eq(0, parse_http_time("Wed Aug 4 0:48:22 1994", &a_time));
  test_eq((time_t)775961302UL, tor_timegm(&a_time));
  T("1994-08-04 00:48:22");
  test_eq(0, parse_http_time("Mie Aug 4 0:48:22 1994", &a_time));
  test_eq((time_t)775961302UL, tor_timegm(&a_time));
  T("1994-08-04 00:48:22");
  test_eq(0, parse_http_time("Sun, 1 Jan 2012 00:00:00 GMT", &a_time));
  test_eq((time_t)1325376000UL, tor_timegm(&a_time));
  T("2012-01-01 00:00:00");
  test_eq(0, parse_http_time("Mon, 31 Dec 2012 00:00:00 GMT", &a_time));
  test_eq((time_t)1356912000UL, tor_timegm(&a_time));
  T("2012-12-31 00:00:00");
  test_eq(-1, parse_http_time("2004-08-zz 99-99x99 GMT", &a_time));
  test_eq(-1, parse_http_time("2011-03-32 00:00:00 GMT", &a_time));
  test_eq(-1, parse_http_time("2011-03-30 24:00:00 GMT", &a_time));
  test_eq(-1, parse_http_time("2011-03-30 23:60:00 GMT", &a_time));
  test_eq(-1, parse_http_time("2011-03-30 23:59:62 GMT", &a_time));
  test_eq(-1, parse_http_time("1969-03-30 23:59:59 GMT", &a_time));
  test_eq(-1, parse_http_time("2011-00-30 23:59:59 GMT", &a_time));
  test_eq(-1, parse_http_time("2011-03-30 23:59", &a_time));

#undef T
 done:
  ;
}

static void
test_util_config_line(void)
{
  char buf[1024];
  char *k=NULL, *v=NULL;
  const char *str;

  /* Test parse_config_line_from_str */
  strlcpy(buf, "k v\n" " key    value with spaces   \n" "keykey val\n"
          "k2\n"
          "k3 \n" "\n" "   \n" "#comment\n"
          "k4#a\n" "k5#abc\n" "k6 val #with comment\n"
          "kseven   \"a quoted 'string\"\n"
          "k8 \"a \\x71uoted\\n\\\"str\\\\ing\\t\\001\\01\\1\\\"\"\n"
          "k9 a line that\\\n spans two lines.\n\n"
          "k10 more than\\\n one contin\\\nuation\n"
          "k11  \\\ncontinuation at the start\n"
          "k12 line with a\\\n#comment\n embedded\n"
          "k13\\\ncontinuation at the very start\n"
          "k14 a line that has a comment and # ends with a slash \\\n"
          "k15 this should be the next new line\n"
          "k16 a line that has a comment and # ends without a slash \n"
          "k17 this should be the next new line\n"
          , sizeof(buf));
  str = buf;

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "k");
  test_streq(v, "v");
  tor_free(k); tor_free(v);
  test_assert(!strcmpstart(str, "key    value with"));

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "key");
  test_streq(v, "value with spaces");
  tor_free(k); tor_free(v);
  test_assert(!strcmpstart(str, "keykey"));

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "keykey");
  test_streq(v, "val");
  tor_free(k); tor_free(v);
  test_assert(!strcmpstart(str, "k2\n"));

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "k2");
  test_streq(v, "");
  tor_free(k); tor_free(v);
  test_assert(!strcmpstart(str, "k3 \n"));

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "k3");
  test_streq(v, "");
  tor_free(k); tor_free(v);
  test_assert(!strcmpstart(str, "#comment"));

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "k4");
  test_streq(v, "");
  tor_free(k); tor_free(v);
  test_assert(!strcmpstart(str, "k5#abc"));

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "k5");
  test_streq(v, "");
  tor_free(k); tor_free(v);
  test_assert(!strcmpstart(str, "k6"));

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "k6");
  test_streq(v, "val");
  tor_free(k); tor_free(v);
  test_assert(!strcmpstart(str, "kseven"));

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "kseven");
  test_streq(v, "a quoted \'string");
  tor_free(k); tor_free(v);
  test_assert(!strcmpstart(str, "k8 "));

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "k8");
  test_streq(v, "a quoted\n\"str\\ing\t\x01\x01\x01\"");
  tor_free(k); tor_free(v);

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "k9");
  test_streq(v, "a line that spans two lines.");
  tor_free(k); tor_free(v);

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "k10");
  test_streq(v, "more than one continuation");
  tor_free(k); tor_free(v);

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "k11");
  test_streq(v, "continuation at the start");
  tor_free(k); tor_free(v);

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "k12");
  test_streq(v, "line with a embedded");
  tor_free(k); tor_free(v);

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "k13");
  test_streq(v, "continuation at the very start");
  tor_free(k); tor_free(v);

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "k14");
  test_streq(v, "a line that has a comment and" );
  tor_free(k); tor_free(v);

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "k15");
  test_streq(v, "this should be the next new line");
  tor_free(k); tor_free(v);

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "k16");
  test_streq(v, "a line that has a comment and" );
  tor_free(k); tor_free(v);

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "k17");
  test_streq(v, "this should be the next new line");
  tor_free(k); tor_free(v);

  test_streq(str, "");

 done:
  tor_free(k);
  tor_free(v);
}

static void
test_util_config_line_quotes(void)
{
  char buf1[1024];
  char buf2[128];
  char buf3[128];
  char buf4[128];
  char *k=NULL, *v=NULL;
  const char *str;

  /* Test parse_config_line_from_str */
  strlcpy(buf1, "kTrailingSpace \"quoted value\"   \n"
          "kTrailingGarbage \"quoted value\"trailing garbage\n"
          , sizeof(buf1));
  strlcpy(buf2, "kTrailingSpaceAndGarbage \"quoted value\" trailing space+g\n"
          , sizeof(buf2));
  strlcpy(buf3, "kMultilineTrailingSpace \"mline\\ \nvalue w/ trailing sp\"\n"
          , sizeof(buf3));
  strlcpy(buf4, "kMultilineNoTrailingBackslash \"naked multiline\nvalue\"\n"
          , sizeof(buf4));
  str = buf1;

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "kTrailingSpace");
  test_streq(v, "quoted value");
  tor_free(k); tor_free(v);

  str = parse_config_line_from_str(str, &k, &v);
  test_eq_ptr(str, NULL);
  tor_free(k); tor_free(v);

  str = buf2;

  str = parse_config_line_from_str(str, &k, &v);
  test_eq_ptr(str, NULL);
  tor_free(k); tor_free(v);

  str = buf3;

  str = parse_config_line_from_str(str, &k, &v);
  test_eq_ptr(str, NULL);
  tor_free(k); tor_free(v);

  str = buf4;

  str = parse_config_line_from_str(str, &k, &v);
  test_eq_ptr(str, NULL);
  tor_free(k); tor_free(v);

 done:
  tor_free(k);
  tor_free(v);
}

static void
test_util_config_line_comment_character(void)
{
  char buf[1024];
  char *k=NULL, *v=NULL;
  const char *str;

  /* Test parse_config_line_from_str */
  strlcpy(buf, "k1 \"# in quotes\"\n"
          "k2 some value    # some comment\n"
          "k3 /home/user/myTorNetwork#2\n"    /* Testcase for #1323 */
          , sizeof(buf));
  str = buf;

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "k1");
  test_streq(v, "# in quotes");
  tor_free(k); tor_free(v);

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "k2");
  test_streq(v, "some value");
  tor_free(k); tor_free(v);

  test_streq(str, "k3 /home/user/myTorNetwork#2\n");

#if 0
  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "k3");
  test_streq(v, "/home/user/myTorNetwork#2");
  tor_free(k); tor_free(v);

  test_streq(str, "");
#endif

 done:
  tor_free(k);
  tor_free(v);
}

static void
test_util_config_line_escaped_content(void)
{
  char buf1[1024];
  char buf2[128];
  char buf3[128];
  char buf4[128];
  char buf5[128];
  char buf6[128];
  char *k=NULL, *v=NULL;
  const char *str;

  /* Test parse_config_line_from_str */
  strlcpy(buf1, "HexadecimalLower \"\\x2a\"\n"
          "HexadecimalUpper \"\\x2A\"\n"
          "HexadecimalUpperX \"\\X2A\"\n"
          "Octal \"\\52\"\n"
          "Newline \"\\n\"\n"
          "Tab \"\\t\"\n"
          "CarriageReturn \"\\r\"\n"
          "DoubleQuote \"\\\"\"\n"
          "SimpleQuote \"\\'\"\n"
          "Backslash \"\\\\\"\n"
          "Mix \"This is a \\\"star\\\":\\t\\'\\x2a\\'\\nAnd second line\"\n"
          , sizeof(buf1));

  strlcpy(buf2, "BrokenEscapedContent \"\\a\"\n"
          , sizeof(buf2));

  strlcpy(buf3, "BrokenEscapedContent \"\\x\"\n"
          , sizeof(buf3));

  strlcpy(buf4, "BrokenOctal \"\\8\"\n"
          , sizeof(buf4));

  strlcpy(buf5, "BrokenHex \"\\xg4\"\n"
          , sizeof(buf5));

  strlcpy(buf6, "BrokenEscape \"\\"
          , sizeof(buf6));

  str = buf1;

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "HexadecimalLower");
  test_streq(v, "*");
  tor_free(k); tor_free(v);

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "HexadecimalUpper");
  test_streq(v, "*");
  tor_free(k); tor_free(v);

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "HexadecimalUpperX");
  test_streq(v, "*");
  tor_free(k); tor_free(v);

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "Octal");
  test_streq(v, "*");
  tor_free(k); tor_free(v);

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "Newline");
  test_streq(v, "\n");
  tor_free(k); tor_free(v);

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "Tab");
  test_streq(v, "\t");
  tor_free(k); tor_free(v);

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "CarriageReturn");
  test_streq(v, "\r");
  tor_free(k); tor_free(v);

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "DoubleQuote");
  test_streq(v, "\"");
  tor_free(k); tor_free(v);

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "SimpleQuote");
  test_streq(v, "'");
  tor_free(k); tor_free(v);

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "Backslash");
  test_streq(v, "\\");
  tor_free(k); tor_free(v);

  str = parse_config_line_from_str(str, &k, &v);
  test_streq(k, "Mix");
  test_streq(v, "This is a \"star\":\t'*'\nAnd second line");
  tor_free(k); tor_free(v);
  test_streq(str, "");

  str = buf2;

  str = parse_config_line_from_str(str, &k, &v);
  test_eq_ptr(str, NULL);
  tor_free(k); tor_free(v);

  str = buf3;

  str = parse_config_line_from_str(str, &k, &v);
  test_eq_ptr(str, NULL);
  tor_free(k); tor_free(v);

  str = buf4;

  str = parse_config_line_from_str(str, &k, &v);
  test_eq_ptr(str, NULL);
  tor_free(k); tor_free(v);

#if 0
  str = buf5;

  str = parse_config_line_from_str(str, &k, &v);
  test_eq_ptr(str, NULL);
  tor_free(k); tor_free(v);
#endif

  str = buf6;

  str = parse_config_line_from_str(str, &k, &v);
  test_eq_ptr(str, NULL);
  tor_free(k); tor_free(v);

 done:
  tor_free(k);
  tor_free(v);
}

#ifndef _WIN32
static void
test_util_expand_filename(void)
{
  char *str;

  setenv("HOME", "/home/itv", 1); /* For "internal test value" */

  str = expand_filename("");
  test_streq("", str);
  tor_free(str);

  str = expand_filename("/normal/path");
  test_streq("/normal/path", str);
  tor_free(str);

  str = expand_filename("/normal/trailing/path/");
  test_streq("/normal/trailing/path/", str);
  tor_free(str);

  str = expand_filename("~");
  test_streq("/home/itv/", str);
  tor_free(str);

  str = expand_filename("$HOME/nodice");
  test_streq("$HOME/nodice", str);
  tor_free(str);

  str = expand_filename("~/");
  test_streq("/home/itv/", str);
  tor_free(str);

  str = expand_filename("~/foobarqux");
  test_streq("/home/itv/foobarqux", str);
  tor_free(str);

  str = expand_filename("~/../../etc/passwd");
  test_streq("/home/itv/../../etc/passwd", str);
  tor_free(str);

  str = expand_filename("~/trailing/");
  test_streq("/home/itv/trailing/", str);
  tor_free(str);
  /* Ideally we'd test ~anotheruser, but that's shady to test (we'd
     have to somehow inject/fake the get_user_homedir call) */

  /* $HOME ending in a trailing slash */
  setenv("HOME", "/home/itv/", 1);

  str = expand_filename("~");
  test_streq("/home/itv/", str);
  tor_free(str);

  str = expand_filename("~/");
  test_streq("/home/itv/", str);
  tor_free(str);

  str = expand_filename("~/foo");
  test_streq("/home/itv/foo", str);
  tor_free(str);

  /* Try with empty $HOME */

  setenv("HOME", "", 1);

  str = expand_filename("~");
  test_streq("/", str);
  tor_free(str);

  str = expand_filename("~/");
  test_streq("/", str);
  tor_free(str);

  str = expand_filename("~/foobar");
  test_streq("/foobar", str);
  tor_free(str);

  /* Try with $HOME unset */

  unsetenv("HOME");

  str = expand_filename("~");
  test_streq("/", str);
  tor_free(str);

  str = expand_filename("~/");
  test_streq("/", str);
  tor_free(str);

  str = expand_filename("~/foobar");
  test_streq("/foobar", str);
  tor_free(str);

 done:
  tor_free(str);
}
#endif

/** Test tor_escape_str_for_pt_args(). */
static void
test_util_escape_string_socks(void)
{
  char *escaped_string = NULL;

  /** Simple backslash escape. */
  escaped_string = tor_escape_str_for_pt_args("This is a backslash: \\",";\\");
  test_assert(escaped_string);
  test_streq(escaped_string, "This is a backslash: \\\\");
  tor_free(escaped_string);

  /** Simple semicolon escape. */
  escaped_string = tor_escape_str_for_pt_args("First rule:Do not use ;",";\\");
  test_assert(escaped_string);
  test_streq(escaped_string, "First rule:Do not use \\;");
  tor_free(escaped_string);

  /** Empty string. */
  escaped_string = tor_escape_str_for_pt_args("", ";\\");
  test_assert(escaped_string);
  test_streq(escaped_string, "");
  tor_free(escaped_string);

  /** Escape all characters. */
  escaped_string = tor_escape_str_for_pt_args(";\\;\\", ";\\");
  test_assert(escaped_string);
  test_streq(escaped_string, "\\;\\\\\\;\\\\");
  tor_free(escaped_string);

  escaped_string = tor_escape_str_for_pt_args(";", ";\\");
  test_assert(escaped_string);
  test_streq(escaped_string, "\\;");
  tor_free(escaped_string);

 done:
  tor_free(escaped_string);
}

static void
test_util_string_is_key_value(void *ptr)
{
  (void)ptr;
  test_assert(string_is_key_value(LOG_WARN, "key=value"));
  test_assert(string_is_key_value(LOG_WARN, "k=v"));
  test_assert(string_is_key_value(LOG_WARN, "key="));
  test_assert(string_is_key_value(LOG_WARN, "x="));
  test_assert(string_is_key_value(LOG_WARN, "xx="));
  test_assert(!string_is_key_value(LOG_WARN, "=value"));
  test_assert(!string_is_key_value(LOG_WARN, "=x"));
  test_assert(!string_is_key_value(LOG_WARN, "="));

  /* ??? */
  /* test_assert(!string_is_key_value(LOG_WARN, "===")); */
 done:
  ;
}

/** Test basic string functionality. */
static void
test_util_strmisc(void)
{
  char buf[1024];
  int i;
  char *cp, *cp_tmp = NULL;

  /* Test strl operations */
  test_eq(5, strlcpy(buf, "Hello", 0));
  test_eq(5, strlcpy(buf, "Hello", 10));
  test_streq(buf, "Hello");
  test_eq(5, strlcpy(buf, "Hello", 6));
  test_streq(buf, "Hello");
  test_eq(5, strlcpy(buf, "Hello", 5));
  test_streq(buf, "Hell");
  strlcpy(buf, "Hello", sizeof(buf));
  test_eq(10, strlcat(buf, "Hello", 5));

  /* Test strstrip() */
  strlcpy(buf, "Testing 1 2 3", sizeof(buf));
  tor_strstrip(buf, ",!");
  test_streq(buf, "Testing 1 2 3");
  strlcpy(buf, "!Testing 1 2 3?", sizeof(buf));
  tor_strstrip(buf, "!? ");
  test_streq(buf, "Testing123");
  strlcpy(buf, "!!!Testing 1 2 3??", sizeof(buf));
  tor_strstrip(buf, "!? ");
  test_streq(buf, "Testing123");

  /* Test parse_long */
  /* Empty/zero input */
  test_eq(0L, tor_parse_long("",10,0,100,&i,NULL));
  test_eq(0, i);
  test_eq(0L, tor_parse_long("0",10,0,100,&i,NULL));
  test_eq(1, i);
  /* Normal cases */
  test_eq(10L, tor_parse_long("10",10,0,100,&i,NULL));
  test_eq(1, i);
  test_eq(10L, tor_parse_long("10",10,0,10,&i,NULL));
  test_eq(1, i);
  test_eq(10L, tor_parse_long("10",10,10,100,&i,NULL));
  test_eq(1, i);
  test_eq(-50L, tor_parse_long("-50",10,-100,100,&i,NULL));
  test_eq(1, i);
  test_eq(-50L, tor_parse_long("-50",10,-100,0,&i,NULL));
  test_eq(1, i);
  test_eq(-50L, tor_parse_long("-50",10,-50,0,&i,NULL));
  test_eq(1, i);
  /* Extra garbage */
  test_eq(0L, tor_parse_long("10m",10,0,100,&i,NULL));
  test_eq(0, i);
  test_eq(0L, tor_parse_long("-50 plus garbage",10,-100,100,&i,NULL));
  test_eq(0, i);
  test_eq(10L, tor_parse_long("10m",10,0,100,&i,&cp));
  test_eq(1, i);
  test_streq(cp, "m");
  test_eq(-50L, tor_parse_long("-50 plus garbage",10,-100,100,&i,&cp));
  test_eq(1, i);
  test_streq(cp, " plus garbage");
  /* Out of bounds */
  test_eq(0L,  tor_parse_long("10",10,50,100,&i,NULL));
  test_eq(0, i);
  test_eq(0L,   tor_parse_long("-50",10,0,100,&i,NULL));
  test_eq(0, i);
  /* Base different than 10 */
  test_eq(2L,   tor_parse_long("10",2,0,100,NULL,NULL));
  test_eq(0L,   tor_parse_long("2",2,0,100,NULL,NULL));
  test_eq(0L,   tor_parse_long("10",-2,0,100,NULL,NULL));
  test_eq(68284L, tor_parse_long("10abc",16,0,70000,NULL,NULL));
  test_eq(68284L, tor_parse_long("10ABC",16,0,70000,NULL,NULL));
  test_eq(0, tor_parse_long("10ABC",-1,0,70000,&i,NULL));
  test_eq(i, 0);

  /* Test parse_ulong */
  test_eq(0UL, tor_parse_ulong("",10,0,100,NULL,NULL));
  test_eq(0UL, tor_parse_ulong("0",10,0,100,NULL,NULL));
  test_eq(10UL, tor_parse_ulong("10",10,0,100,NULL,NULL));
  test_eq(0UL, tor_parse_ulong("10",10,50,100,NULL,NULL));
  test_eq(10UL, tor_parse_ulong("10",10,0,10,NULL,NULL));
  test_eq(10UL, tor_parse_ulong("10",10,10,100,NULL,NULL));
  test_eq(0UL, tor_parse_ulong("8",8,0,100,NULL,NULL));
  test_eq(50UL, tor_parse_ulong("50",10,50,100,NULL,NULL));
  test_eq(0UL, tor_parse_ulong("-50",10,-100,100,NULL,NULL));
  test_eq(0UL, tor_parse_ulong("50",-1,50,100,&i,NULL));
  test_eq(0, i);

  /* Test parse_uint64 */
  test_assert(U64_LITERAL(10) == tor_parse_uint64("10 x",10,0,100, &i, &cp));
  test_eq(1, i);
  test_streq(cp, " x");
  test_assert(U64_LITERAL(12345678901) ==
              tor_parse_uint64("12345678901",10,0,UINT64_MAX, &i, &cp));
  test_eq(1, i);
  test_streq(cp, "");
  test_assert(U64_LITERAL(0) ==
              tor_parse_uint64("12345678901",10,500,INT32_MAX, &i, &cp));
  test_eq(0, i);
  test_assert(U64_LITERAL(0) ==
              tor_parse_uint64("123",-1,0,INT32_MAX, &i, &cp));
  test_eq(0, i);

  {
  /* Test parse_double */
  double d = tor_parse_double("10", 0, UINT64_MAX,&i,NULL);
  test_eq(1, i);
  test_assert(DBL_TO_U64(d) == 10);
  d = tor_parse_double("0", 0, UINT64_MAX,&i,NULL);
  test_eq(1, i);
  test_assert(DBL_TO_U64(d) == 0);
  d = tor_parse_double(" ", 0, UINT64_MAX,&i,NULL);
  test_eq(0, i);
  d = tor_parse_double(".0a", 0, UINT64_MAX,&i,NULL);
  test_eq(0, i);
  d = tor_parse_double(".0a", 0, UINT64_MAX,&i,&cp);
  test_eq(1, i);
  d = tor_parse_double("-.0", 0, UINT64_MAX,&i,NULL);
  test_eq(1, i);
  test_assert(DBL_TO_U64(d) == 0);
  d = tor_parse_double("-10", -100.0, 100.0,&i,NULL);
  test_eq(1, i);
  test_eq(-10.0, d);
  }

  {
    /* Test tor_parse_* where we overflow/underflow the underlying type. */
    /* This string should overflow 64-bit ints. */
#define TOOBIG "100000000000000000000000000"
    test_eq(0L, tor_parse_long(TOOBIG, 10, LONG_MIN, LONG_MAX, &i, NULL));
    test_eq(i, 0);
    test_eq(0L, tor_parse_long("-"TOOBIG, 10, LONG_MIN, LONG_MAX, &i, NULL));
    test_eq(i, 0);
    test_eq(0UL, tor_parse_ulong(TOOBIG, 10, 0, ULONG_MAX, &i, NULL));
    test_eq(i, 0);
    tt_u64_op(U64_LITERAL(0), ==, tor_parse_uint64(TOOBIG, 10,
                                             0, UINT64_MAX, &i, NULL));
    test_eq(i, 0);
  }

  /* Test snprintf */
  /* Returning -1 when there's not enough room in the output buffer */
  test_eq(-1, tor_snprintf(buf, 0, "Foo"));
  test_eq(-1, tor_snprintf(buf, 2, "Foo"));
  test_eq(-1, tor_snprintf(buf, 3, "Foo"));
  test_neq(-1, tor_snprintf(buf, 4, "Foo"));
  /* Always NUL-terminate the output */
  tor_snprintf(buf, 5, "abcdef");
  test_eq(0, buf[4]);
  tor_snprintf(buf, 10, "abcdef");
  test_eq(0, buf[6]);
  /* uint64 */
  tor_snprintf(buf, sizeof(buf), "x!"U64_FORMAT"!x",
               U64_PRINTF_ARG(U64_LITERAL(12345678901)));
  test_streq("x!12345678901!x", buf);

  /* Test str{,case}cmpstart */
  test_assert(strcmpstart("abcdef", "abcdef")==0);
  test_assert(strcmpstart("abcdef", "abc")==0);
  test_assert(strcmpstart("abcdef", "abd")<0);
  test_assert(strcmpstart("abcdef", "abb")>0);
  test_assert(strcmpstart("ab", "abb")<0);
  test_assert(strcmpstart("ab", "")==0);
  test_assert(strcmpstart("ab", "ab ")<0);
  test_assert(strcasecmpstart("abcdef", "abCdEF")==0);
  test_assert(strcasecmpstart("abcDeF", "abc")==0);
  test_assert(strcasecmpstart("abcdef", "Abd")<0);
  test_assert(strcasecmpstart("Abcdef", "abb")>0);
  test_assert(strcasecmpstart("ab", "Abb")<0);
  test_assert(strcasecmpstart("ab", "")==0);
  test_assert(strcasecmpstart("ab", "ab ")<0);

  /* Test str{,case}cmpend */
  test_assert(strcmpend("abcdef", "abcdef")==0);
  test_assert(strcmpend("abcdef", "def")==0);
  test_assert(strcmpend("abcdef", "deg")<0);
  test_assert(strcmpend("abcdef", "dee")>0);
  test_assert(strcmpend("ab", "aab")>0);
  test_assert(strcasecmpend("AbcDEF", "abcdef")==0);
  test_assert(strcasecmpend("abcdef", "dEF")==0);
  test_assert(strcasecmpend("abcdef", "Deg")<0);
  test_assert(strcasecmpend("abcDef", "dee")>0);
  test_assert(strcasecmpend("AB", "abb")<0);

  /* Test digest_is_zero */
  memset(buf,0,20);
  buf[20] = 'x';
  test_assert(tor_digest_is_zero(buf));
  buf[19] = 'x';
  test_assert(!tor_digest_is_zero(buf));

  /* Test mem_is_zero */
  memset(buf,0,128);
  buf[128] = 'x';
  test_assert(tor_mem_is_zero(buf, 10));
  test_assert(tor_mem_is_zero(buf, 20));
  test_assert(tor_mem_is_zero(buf, 128));
  test_assert(!tor_mem_is_zero(buf, 129));
  buf[60] = (char)255;
  test_assert(!tor_mem_is_zero(buf, 128));
  buf[0] = (char)1;
  test_assert(!tor_mem_is_zero(buf, 10));

  /* Test 'escaped' */
  test_assert(NULL == escaped(NULL));
  test_streq("\"\"", escaped(""));
  test_streq("\"abcd\"", escaped("abcd"));
  test_streq("\"\\\\ \\n\\r\\t\\\"\\'\"", escaped("\\ \n\r\t\"'"));
  test_streq("\"unnecessary \\'backslashes\\'\"",
             escaped("unnecessary \'backslashes\'"));
  /* Non-printable characters appear as octal */
  test_streq("\"z\\001abc\\277d\"",  escaped("z\001abc\277d"));
  test_streq("\"z\\336\\255 ;foo\"", escaped("z\xde\xad\x20;foo"));

  /* Test strndup and memdup */
  {
    const char *s = "abcdefghijklmnopqrstuvwxyz";
    cp_tmp = tor_strndup(s, 30);
    test_streq(cp_tmp, s); /* same string, */
    test_neq_ptr(cp_tmp, s); /* but different pointers. */
    tor_free(cp_tmp);

    cp_tmp = tor_strndup(s, 5);
    test_streq(cp_tmp, "abcde");
    tor_free(cp_tmp);

    s = "a\0b\0c\0d\0e\0";
    cp_tmp = tor_memdup(s,10);
    test_memeq(cp_tmp, s, 10); /* same ram, */
    test_neq_ptr(cp_tmp, s); /* but different pointers. */
    tor_free(cp_tmp);
  }

  /* Test str-foo functions */
  cp_tmp = tor_strdup("abcdef");
  test_assert(tor_strisnonupper(cp_tmp));
  cp_tmp[3] = 'D';
  test_assert(!tor_strisnonupper(cp_tmp));
  tor_strupper(cp_tmp);
  test_streq(cp_tmp, "ABCDEF");
  tor_strlower(cp_tmp);
  test_streq(cp_tmp, "abcdef");
  test_assert(tor_strisnonupper(cp_tmp));
  test_assert(tor_strisprint(cp_tmp));
  cp_tmp[3] = 3;
  test_assert(!tor_strisprint(cp_tmp));
  tor_free(cp_tmp);

  /* Test memmem and memstr */
  {
    const char *haystack = "abcde";
    test_assert(!tor_memmem(haystack, 5, "ef", 2));
    test_eq_ptr(tor_memmem(haystack, 5, "cd", 2), haystack + 2);
    test_eq_ptr(tor_memmem(haystack, 5, "cde", 3), haystack + 2);
    test_assert(!tor_memmem(haystack, 4, "cde", 3));
    haystack = "ababcad";
    test_eq_ptr(tor_memmem(haystack, 7, "abc", 3), haystack + 2);
    test_eq_ptr(tor_memmem(haystack, 7, "ad", 2), haystack + 5);
    test_eq_ptr(tor_memmem(haystack, 7, "cad", 3), haystack + 4);
    test_assert(!tor_memmem(haystack, 7, "dadad", 5));
    test_assert(!tor_memmem(haystack, 7, "abcdefghij", 10));
    /* memstr */
    test_eq_ptr(tor_memstr(haystack, 7, "abc"), haystack + 2);
    test_eq_ptr(tor_memstr(haystack, 7, "cad"), haystack + 4);
    test_assert(!tor_memstr(haystack, 6, "cad"));
    test_assert(!tor_memstr(haystack, 7, "cadd"));
    test_assert(!tor_memstr(haystack, 7, "fe"));
    test_assert(!tor_memstr(haystack, 7, "ababcade"));
  }

  /* Test hex_str */
  {
    char binary_data[68];
    size_t i;
    for (i = 0; i < sizeof(binary_data); ++i)
      binary_data[i] = i;
    test_streq(hex_str(binary_data, 0), "");
    test_streq(hex_str(binary_data, 1), "00");
    test_streq(hex_str(binary_data, 17), "000102030405060708090A0B0C0D0E0F10");
    test_streq(hex_str(binary_data, 32),
               "000102030405060708090A0B0C0D0E0F"
               "101112131415161718191A1B1C1D1E1F");
    test_streq(hex_str(binary_data, 34),
               "000102030405060708090A0B0C0D0E0F"
               "101112131415161718191A1B1C1D1E1F");
    /* Repeat these tests for shorter strings after longer strings
       have been tried, to make sure we're correctly terminating strings */
    test_streq(hex_str(binary_data, 1), "00");
    test_streq(hex_str(binary_data, 0), "");
  }

  /* Test strcmp_opt */
  tt_int_op(strcmp_opt("",   "foo"), <, 0);
  tt_int_op(strcmp_opt("",    ""),  ==, 0);
  tt_int_op(strcmp_opt("foo", ""),   >, 0);

  tt_int_op(strcmp_opt(NULL,  ""),    <, 0);
  tt_int_op(strcmp_opt(NULL,  NULL), ==, 0);
  tt_int_op(strcmp_opt("",    NULL),  >, 0);

  tt_int_op(strcmp_opt(NULL,  "foo"), <, 0);
  tt_int_op(strcmp_opt("foo", NULL),  >, 0);

  /* Test strcmp_len */
  tt_int_op(strcmp_len("foo", "bar", 3),   >, 0);
  tt_int_op(strcmp_len("foo", "bar", 2),   <, 0); /* First len, then lexical */
  tt_int_op(strcmp_len("foo2", "foo1", 4), >, 0);
  tt_int_op(strcmp_len("foo2", "foo1", 3), <, 0); /* Really stop at len */
  tt_int_op(strcmp_len("foo2", "foo", 3), ==, 0);   /* Really stop at len */
  tt_int_op(strcmp_len("blah", "", 4),     >, 0);
  tt_int_op(strcmp_len("blah", "", 0),    ==, 0);

 done:
  tor_free(cp_tmp);
}

static void
test_util_pow2(void)
{
  /* Test tor_log2(). */
  test_eq(tor_log2(64), 6);
  test_eq(tor_log2(65), 6);
  test_eq(tor_log2(63), 5);
  test_eq(tor_log2(0), 0); /* incorrect mathematically, but as specified */
  test_eq(tor_log2(1), 0);
  test_eq(tor_log2(2), 1);
  test_eq(tor_log2(3), 1);
  test_eq(tor_log2(4), 2);
  test_eq(tor_log2(5), 2);
  test_eq(tor_log2(U64_LITERAL(40000000000000000)), 55);
  test_eq(tor_log2(UINT64_MAX), 63);

  /* Test round_to_power_of_2 */
  tt_u64_op(round_to_power_of_2(120), ==, 128);
  tt_u64_op(round_to_power_of_2(128), ==, 128);
  tt_u64_op(round_to_power_of_2(130), ==, 128);
  tt_u64_op(round_to_power_of_2(U64_LITERAL(40000000000000000)), ==,
            U64_LITERAL(1)<<55);
  tt_u64_op(round_to_power_of_2(U64_LITERAL(0xffffffffffffffff)), ==,
          U64_LITERAL(1)<<63);
  tt_u64_op(round_to_power_of_2(0), ==, 1);
  tt_u64_op(round_to_power_of_2(1), ==, 1);
  tt_u64_op(round_to_power_of_2(2), ==, 2);
  tt_u64_op(round_to_power_of_2(3), ==, 2);
  tt_u64_op(round_to_power_of_2(4), ==, 4);
  tt_u64_op(round_to_power_of_2(5), ==, 4);
  tt_u64_op(round_to_power_of_2(6), ==, 4);
  tt_u64_op(round_to_power_of_2(7), ==, 8);

 done:
  ;
}

/** mutex for thread test to stop the threads hitting data at the same time. */
static tor_mutex_t *thread_test_mutex_ = NULL;
/** mutexes for the thread test to make sure that the threads have to
 * interleave somewhat. */
static tor_mutex_t *thread_test_start1_ = NULL,
                   *thread_test_start2_ = NULL;
/** Shared strmap for the thread test. */
static strmap_t *thread_test_strmap_ = NULL;
/** The name of thread1 for the thread test */
static char *thread1_name_ = NULL;
/** The name of thread2 for the thread test */
static char *thread2_name_ = NULL;

static void thread_test_func_(void* _s) ATTR_NORETURN;

/** How many iterations have the threads in the unit test run? */
static int t1_count = 0, t2_count = 0;

/** Helper function for threading unit tests: This function runs in a
 * subthread. It grabs its own mutex (start1 or start2) to make sure that it
 * should start, then it repeatedly alters _test_thread_strmap protected by
 * thread_test_mutex_. */
static void
thread_test_func_(void* _s)
{
  char *s = _s;
  int i, *count;
  tor_mutex_t *m;
  char buf[64];
  char **cp;
  if (!strcmp(s, "thread 1")) {
    m = thread_test_start1_;
    cp = &thread1_name_;
    count = &t1_count;
  } else {
    m = thread_test_start2_;
    cp = &thread2_name_;
    count = &t2_count;
  }

  tor_snprintf(buf, sizeof(buf), "%lu", tor_get_thread_id());
  *cp = tor_strdup(buf);

  tor_mutex_acquire(m);

  for (i=0; i<10000; ++i) {
    tor_mutex_acquire(thread_test_mutex_);
    strmap_set(thread_test_strmap_, "last to run", *cp);
    ++*count;
    tor_mutex_release(thread_test_mutex_);
  }
  tor_mutex_acquire(thread_test_mutex_);
  strmap_set(thread_test_strmap_, s, *cp);
  tor_mutex_release(thread_test_mutex_);

  tor_mutex_release(m);

  spawn_exit();
}

/** Run unit tests for threading logic. */
static void
test_util_threads(void)
{
  char *s1 = NULL, *s2 = NULL;
  int done = 0, timedout = 0;
  time_t started;
#ifndef _WIN32
  struct timeval tv;
  tv.tv_sec=0;
  tv.tv_usec=100*1000;
#endif
#ifndef TOR_IS_MULTITHREADED
  /* Skip this test if we aren't threading. We should be threading most
   * everywhere by now. */
  if (1)
    return;
#endif
  thread_test_mutex_ = tor_mutex_new();
  thread_test_start1_ = tor_mutex_new();
  thread_test_start2_ = tor_mutex_new();
  thread_test_strmap_ = strmap_new();
  s1 = tor_strdup("thread 1");
  s2 = tor_strdup("thread 2");
  tor_mutex_acquire(thread_test_start1_);
  tor_mutex_acquire(thread_test_start2_);
  spawn_func(thread_test_func_, s1);
  spawn_func(thread_test_func_, s2);
  tor_mutex_release(thread_test_start2_);
  tor_mutex_release(thread_test_start1_);
  started = time(NULL);
  while (!done) {
    tor_mutex_acquire(thread_test_mutex_);
    strmap_assert_ok(thread_test_strmap_);
    if (strmap_get(thread_test_strmap_, "thread 1") &&
        strmap_get(thread_test_strmap_, "thread 2")) {
      done = 1;
    } else if (time(NULL) > started + 150) {
      timedout = done = 1;
    }
    tor_mutex_release(thread_test_mutex_);
#ifndef _WIN32
    /* Prevent the main thread from starving the worker threads. */
    select(0, NULL, NULL, NULL, &tv);
#endif
  }
  tor_mutex_acquire(thread_test_start1_);
  tor_mutex_release(thread_test_start1_);
  tor_mutex_acquire(thread_test_start2_);
  tor_mutex_release(thread_test_start2_);

  tor_mutex_free(thread_test_mutex_);

  if (timedout) {
    printf("\nTimed out: %d %d", t1_count, t2_count);
    test_assert(strmap_get(thread_test_strmap_, "thread 1"));
    test_assert(strmap_get(thread_test_strmap_, "thread 2"));
    test_assert(!timedout);
  }

  /* different thread IDs. */
  test_assert(strcmp(strmap_get(thread_test_strmap_, "thread 1"),
                     strmap_get(thread_test_strmap_, "thread 2")));
  test_assert(!strcmp(strmap_get(thread_test_strmap_, "thread 1"),
                      strmap_get(thread_test_strmap_, "last to run")) ||
              !strcmp(strmap_get(thread_test_strmap_, "thread 2"),
                      strmap_get(thread_test_strmap_, "last to run")));

 done:
  tor_free(s1);
  tor_free(s2);
  tor_free(thread1_name_);
  tor_free(thread2_name_);
  if (thread_test_strmap_)
    strmap_free(thread_test_strmap_, NULL);
  if (thread_test_start1_)
    tor_mutex_free(thread_test_start1_);
  if (thread_test_start2_)
    tor_mutex_free(thread_test_start2_);
}

/** Run unit tests for compression functions */
static void
test_util_gzip(void)
{
  char *buf1=NULL, *buf2=NULL, *buf3=NULL, *cp1, *cp2;
  const char *ccp2;
  size_t len1, len2;
  tor_zlib_state_t *state = NULL;

  buf1 = tor_strdup("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAZAAAAAAAAAAAAAAAAAAAZ");
  test_assert(detect_compression_method(buf1, strlen(buf1)) == UNKNOWN_METHOD);
  if (is_gzip_supported()) {
    test_assert(!tor_gzip_compress(&buf2, &len1, buf1, strlen(buf1)+1,
                                   GZIP_METHOD));
    test_assert(buf2);
    test_assert(len1 < strlen(buf1));
    test_assert(detect_compression_method(buf2, len1) == GZIP_METHOD);

    test_assert(!tor_gzip_uncompress(&buf3, &len2, buf2, len1,
                                     GZIP_METHOD, 1, LOG_INFO));
    test_assert(buf3);
    test_eq(strlen(buf1) + 1, len2);
    test_streq(buf1, buf3);

    tor_free(buf2);
    tor_free(buf3);
  }

  test_assert(!tor_gzip_compress(&buf2, &len1, buf1, strlen(buf1)+1,
                                 ZLIB_METHOD));
  test_assert(buf2);
  test_assert(detect_compression_method(buf2, len1) == ZLIB_METHOD);

  test_assert(!tor_gzip_uncompress(&buf3, &len2, buf2, len1,
                                   ZLIB_METHOD, 1, LOG_INFO));
  test_assert(buf3);
  test_eq(strlen(buf1) + 1, len2);
  test_streq(buf1, buf3);

  /* Check whether we can uncompress concatenated, compressed strings. */
  tor_free(buf3);
  buf2 = tor_realloc(buf2, len1*2);
  memcpy(buf2+len1, buf2, len1);
  test_assert(!tor_gzip_uncompress(&buf3, &len2, buf2, len1*2,
                                   ZLIB_METHOD, 1, LOG_INFO));
  test_eq((strlen(buf1)+1)*2, len2);
  test_memeq(buf3,
             "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAZAAAAAAAAAAAAAAAAAAAZ\0"
             "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAZAAAAAAAAAAAAAAAAAAAZ\0",
             (strlen(buf1)+1)*2);

  tor_free(buf1);
  tor_free(buf2);
  tor_free(buf3);

  /* Check whether we can uncompress partial strings. */
  buf1 =
    tor_strdup("String with low redundancy that won't be compressed much.");
  test_assert(!tor_gzip_compress(&buf2, &len1, buf1, strlen(buf1)+1,
                                 ZLIB_METHOD));
  tt_assert(len1>16);
  /* when we allow an incomplete string, we should succeed.*/
  tt_assert(!tor_gzip_uncompress(&buf3, &len2, buf2, len1-16,
                                  ZLIB_METHOD, 0, LOG_INFO));
  tt_assert(len2 > 5);
  buf3[len2]='\0';
  tt_assert(!strcmpstart(buf1, buf3));

  /* when we demand a complete string, this must fail. */
  tor_free(buf3);
  tt_assert(tor_gzip_uncompress(&buf3, &len2, buf2, len1-16,
                                 ZLIB_METHOD, 1, LOG_INFO));
  tt_assert(!buf3);

  /* Now, try streaming compression. */
  tor_free(buf1);
  tor_free(buf2);
  tor_free(buf3);
  state = tor_zlib_new(1, ZLIB_METHOD);
  tt_assert(state);
  cp1 = buf1 = tor_malloc(1024);
  len1 = 1024;
  ccp2 = "ABCDEFGHIJABCDEFGHIJ";
  len2 = 21;
  test_assert(tor_zlib_process(state, &cp1, &len1, &ccp2, &len2, 0)
              == TOR_ZLIB_OK);
  test_eq(0, len2); /* Make sure we compressed it all. */
  test_assert(cp1 > buf1);

  len2 = 0;
  cp2 = cp1;
  test_assert(tor_zlib_process(state, &cp1, &len1, &ccp2, &len2, 1)
              == TOR_ZLIB_DONE);
  test_eq(0, len2);
  test_assert(cp1 > cp2); /* Make sure we really added something. */

  tt_assert(!tor_gzip_uncompress(&buf3, &len2, buf1, 1024-len1,
                                  ZLIB_METHOD, 1, LOG_WARN));
  test_streq(buf3, "ABCDEFGHIJABCDEFGHIJ"); /*Make sure it compressed right.*/
  test_eq(21, len2);

 done:
  if (state)
    tor_zlib_free(state);
  tor_free(buf2);
  tor_free(buf3);
  tor_free(buf1);
}

/** Run unit tests for mmap() wrapper functionality. */
static void
test_util_mmap(void)
{
  char *fname1 = tor_strdup(get_fname("mapped_1"));
  char *fname2 = tor_strdup(get_fname("mapped_2"));
  char *fname3 = tor_strdup(get_fname("mapped_3"));
  const size_t buflen = 17000;
  char *buf = tor_malloc(17000);
  tor_mmap_t *mapping = NULL;

  crypto_rand(buf, buflen);

  mapping = tor_mmap_file(fname1);
  test_assert(! mapping);

  write_str_to_file(fname1, "Short file.", 1);

  mapping = tor_mmap_file(fname1);
  test_assert(mapping);
  test_eq(mapping->size, strlen("Short file."));
  test_streq(mapping->data, "Short file.");
#ifdef _WIN32
  tt_int_op(0, ==, tor_munmap_file(mapping));
  mapping = NULL;
  test_assert(unlink(fname1) == 0);
#else
  /* make sure we can unlink. */
  test_assert(unlink(fname1) == 0);
  test_streq(mapping->data, "Short file.");
  tt_int_op(0, ==, tor_munmap_file(mapping));
  mapping = NULL;
#endif

  /* Now a zero-length file. */
  write_str_to_file(fname1, "", 1);
  mapping = tor_mmap_file(fname1);
  test_eq_ptr(mapping, NULL);
  test_eq(ERANGE, errno);
  unlink(fname1);

  /* Make sure that we fail to map a no-longer-existent file. */
  mapping = tor_mmap_file(fname1);
  test_assert(! mapping);

  /* Now try a big file that stretches across a few pages and isn't aligned */
  write_bytes_to_file(fname2, buf, buflen, 1);
  mapping = tor_mmap_file(fname2);
  test_assert(mapping);
  test_eq(mapping->size, buflen);
  test_memeq(mapping->data, buf, buflen);
  tt_int_op(0, ==, tor_munmap_file(mapping));
  mapping = NULL;

  /* Now try a big aligned file. */
  write_bytes_to_file(fname3, buf, 16384, 1);
  mapping = tor_mmap_file(fname3);
  test_assert(mapping);
  test_eq(mapping->size, 16384);
  test_memeq(mapping->data, buf, 16384);
  tt_int_op(0, ==, tor_munmap_file(mapping));
  mapping = NULL;

 done:
  unlink(fname1);
  unlink(fname2);
  unlink(fname3);

  tor_free(fname1);
  tor_free(fname2);
  tor_free(fname3);
  tor_free(buf);

  tor_munmap_file(mapping);
}

/** Run unit tests for escaping/unescaping data for use by controllers. */
static void
test_util_control_formats(void)
{
  char *out = NULL;
  const char *inp =
    "..This is a test\r\n.of the emergency \n..system.\r\n\rZ.\r\n";
  size_t sz;

  sz = read_escaped_data(inp, strlen(inp), &out);
  test_streq(out,
             ".This is a test\nof the emergency \n.system.\n\rZ.\n");
  test_eq(sz, strlen(out));

 done:
  tor_free(out);
}

#define test_feq(value1,value2) do {                               \
    double v1 = (value1), v2=(value2);                             \
    double tf_diff = v1-v2;                                        \
    double tf_tolerance = ((v1+v2)/2.0)/1e8;                       \
    if (tf_diff<0) tf_diff=-tf_diff;                               \
    if (tf_tolerance<0) tf_tolerance=-tf_tolerance;                \
    if (tf_diff<tf_tolerance) {                                    \
      TT_BLATHER(("%s ~~ %s: %f ~~ %f",#value1,#value2,v1,v2));  \
    } else {                                                       \
      TT_FAIL(("%s ~~ %s: %f != %f",#value1,#value2,v1,v2)); \
    }                                                              \
  } while (0)

static void
test_util_sscanf(void)
{
  unsigned u1, u2, u3;
  char s1[20], s2[10], s3[10], ch;
  int r;
  long lng1,lng2;
  int int1, int2;
  double d1,d2,d3,d4;

  /* Simple tests (malformed patterns, literal matching, ...) */
  test_eq(-1, tor_sscanf("123", "%i", &r)); /* %i is not supported */
  test_eq(-1, tor_sscanf("wrong", "%5c", s1)); /* %c cannot have a number. */
  test_eq(-1, tor_sscanf("hello", "%s", s1)); /* %s needs a number. */
  test_eq(-1, tor_sscanf("prettylongstring", "%999999s", s1));
#if 0
  /* GCC thinks these two are illegal. */
  test_eq(-1, tor_sscanf("prettylongstring", "%0s", s1));
  test_eq(0, tor_sscanf("prettylongstring", "%10s", NULL));
#endif
  /* No '%'-strings: always "success" */
  test_eq(0, tor_sscanf("hello world", "hello world"));
  test_eq(0, tor_sscanf("hello world", "good bye"));
  /* Excess data */
  test_eq(0, tor_sscanf("hello 3", "%u", &u1));  /* have to match the start */
  test_eq(0, tor_sscanf(" 3 hello", "%u", &u1));
  test_eq(0, tor_sscanf(" 3 hello", "%2u", &u1)); /* not even in this case */
  test_eq(1, tor_sscanf("3 hello", "%u", &u1));  /* but trailing is alright */

  /* Numbers (ie. %u) */
  test_eq(0, tor_sscanf("hello world 3", "hello worlb %u", &u1)); /* d vs b */
  test_eq(1, tor_sscanf("12345", "%u", &u1));
  test_eq(12345u, u1);
  test_eq(1, tor_sscanf("12346 ", "%u", &u1));
  test_eq(12346u, u1);
  test_eq(0, tor_sscanf(" 12347", "%u", &u1));
  test_eq(1, tor_sscanf(" 12348", " %u", &u1));
  test_eq(12348u, u1);
  test_eq(1, tor_sscanf("0", "%u", &u1));
  test_eq(0u, u1);
  test_eq(1, tor_sscanf("0000", "%u", &u2));
  test_eq(0u, u2);
  test_eq(0, tor_sscanf("", "%u", &u1)); /* absent number */
  test_eq(0, tor_sscanf("A", "%u", &u1)); /* bogus number */
  test_eq(0, tor_sscanf("-1", "%u", &u1)); /* negative number */
  test_eq(1, tor_sscanf("4294967295", "%u", &u1)); /* UINT32_MAX should work */
  test_eq(4294967295u, u1);
  test_eq(0, tor_sscanf("4294967296", "%u", &u1)); /* But not at 32 bits */
  test_eq(1, tor_sscanf("4294967296", "%9u", &u1)); /* but parsing only 9... */
  test_eq(429496729u, u1);

  /* Numbers with size (eg. %2u) */
  test_eq(0, tor_sscanf("-1", "%2u", &u1));
  test_eq(2, tor_sscanf("123456", "%2u%u", &u1, &u2));
  test_eq(12u, u1);
  test_eq(3456u, u2);
  test_eq(1, tor_sscanf("123456", "%8u", &u1));
  test_eq(123456u, u1);
  test_eq(1, tor_sscanf("123457  ", "%8u", &u1));
  test_eq(123457u, u1);
  test_eq(0, tor_sscanf("  123456", "%8u", &u1));
  test_eq(3, tor_sscanf("!12:3:456", "!%2u:%2u:%3u", &u1, &u2, &u3));
  test_eq(12u, u1);
  test_eq(3u, u2);
  test_eq(456u, u3);
  test_eq(3, tor_sscanf("67:8:099", "%2u:%2u:%3u", &u1, &u2, &u3)); /* 0s */
  test_eq(67u, u1);
  test_eq(8u, u2);
  test_eq(99u, u3);
  /* %u does not match space.*/
  test_eq(2, tor_sscanf("12:3: 45", "%2u:%2u:%3u", &u1, &u2, &u3));
  test_eq(12u, u1);
  test_eq(3u, u2);
  /* %u does not match negative numbers. */
  test_eq(2, tor_sscanf("67:8:-9", "%2u:%2u:%3u", &u1, &u2, &u3));
  test_eq(67u, u1);
  test_eq(8u, u2);
  /* Arbitrary amounts of 0-padding are okay */
  test_eq(3, tor_sscanf("12:03:000000000000000099", "%2u:%2u:%u",
                        &u1, &u2, &u3));
  test_eq(12u, u1);
  test_eq(3u, u2);
  test_eq(99u, u3);

  /* Hex (ie. %x) */
  test_eq(3, tor_sscanf("1234 02aBcdEf ff", "%x %x %x", &u1, &u2, &u3));
  test_eq(0x1234, u1);
  test_eq(0x2ABCDEF, u2);
  test_eq(0xFF, u3);
  /* Width works on %x */
  test_eq(3, tor_sscanf("f00dcafe444", "%4x%4x%u", &u1, &u2, &u3));
  test_eq(0xf00d, u1);
  test_eq(0xcafe, u2);
  test_eq(444, u3);

  /* Literal '%' (ie. '%%') */
  test_eq(1, tor_sscanf("99% fresh", "%3u%% fresh", &u1));
  test_eq(99, u1);
  test_eq(0, tor_sscanf("99 fresh", "%% %3u %s", &u1, s1));
  test_eq(1, tor_sscanf("99 fresh", "%3u%% %s", &u1, s1));
  test_eq(2, tor_sscanf("99 fresh", "%3u %5s %%", &u1, s1));
  test_eq(99, u1);
  test_streq(s1, "fresh");
  test_eq(1, tor_sscanf("% boo", "%% %3s", s1));
  test_streq("boo", s1);

  /* Strings (ie. %s) */
  test_eq(2, tor_sscanf("hello", "%3s%7s", s1, s2));
  test_streq(s1, "hel");
  test_streq(s2, "lo");
  test_eq(2, tor_sscanf("WD40", "%2s%u", s3, &u1)); /* %s%u */
  test_streq(s3, "WD");
  test_eq(40, u1);
  test_eq(2, tor_sscanf("WD40", "%3s%u", s3, &u1)); /* %s%u */
  test_streq(s3, "WD4");
  test_eq(0, u1);
  test_eq(2, tor_sscanf("76trombones", "%6u%9s", &u1, s1)); /* %u%s */
  test_eq(76, u1);
  test_streq(s1, "trombones");
  test_eq(1, tor_sscanf("prettylongstring", "%999s", s1));
  test_streq(s1, "prettylongstring");
  /* %s doesn't eat spaces */
  test_eq(2, tor_sscanf("hello world", "%9s %9s", s1, s2));
  test_streq(s1, "hello");
  test_streq(s2, "world");
  test_eq(2, tor_sscanf("bye   world?", "%9s %9s", s1, s2));
  test_streq(s1, "bye");
  test_streq(s2, "");
  test_eq(3, tor_sscanf("hi", "%9s%9s%3s", s1, s2, s3)); /* %s can be empty. */
  test_streq(s1, "hi");
  test_streq(s2, "");
  test_streq(s3, "");

  test_eq(3, tor_sscanf("1.2.3", "%u.%u.%u%c", &u1, &u2, &u3, &ch));
  test_eq(4, tor_sscanf("1.2.3 foobar", "%u.%u.%u%c", &u1, &u2, &u3, &ch));
  test_eq(' ', ch);

  r = tor_sscanf("12345 -67890 -1", "%d %ld %d", &int1, &lng1, &int2);
  test_eq(r,3);
  test_eq(int1, 12345);
  test_eq(lng1, -67890);
  test_eq(int2, -1);

#if SIZEOF_INT == 4
  r = tor_sscanf("-2147483648. 2147483647.", "%d. %d.", &int1, &int2);
  test_eq(r,2);
  test_eq(int1, -2147483647-1);
  test_eq(int2, 2147483647);

  r = tor_sscanf("-2147483679.", "%d.", &int1);
  test_eq(r,0);

  r = tor_sscanf("2147483678.", "%d.", &int1);
  test_eq(r,0);
#elif SIZEOF_INT == 8
  r = tor_sscanf("-9223372036854775808. 9223372036854775807.",
                 "%d. %d.", &int1, &int2);
  test_eq(r,2);
  test_eq(int1, -9223372036854775807-1);
  test_eq(int2, 9223372036854775807);

  r = tor_sscanf("-9223372036854775809.", "%d.", &int1);
  test_eq(r,0);

  r = tor_sscanf("9223372036854775808.", "%d.", &int1);
  test_eq(r,0);
#endif

#if SIZEOF_LONG == 4
  r = tor_sscanf("-2147483648. 2147483647.", "%ld. %ld.", &lng1, &lng2);
  test_eq(r,2);
  test_eq(lng1, -2147483647 - 1);
  test_eq(lng2, 2147483647);
#elif SIZEOF_LONG == 8
  r = tor_sscanf("-9223372036854775808. 9223372036854775807.",
                 "%ld. %ld.", &lng1, &lng2);
  test_eq(r,2);
  test_eq(lng1, -9223372036854775807L - 1);
  test_eq(lng2, 9223372036854775807L);

  r = tor_sscanf("-9223372036854775808. 9223372036854775808.",
                 "%ld. %ld.", &lng1, &lng2);
  test_eq(r,1);
  r = tor_sscanf("-9223372036854775809. 9223372036854775808.",
                 "%ld. %ld.", &lng1, &lng2);
  test_eq(r,0);
#endif

  r = tor_sscanf("123.456 .000007 -900123123.2000787 00003.2",
                 "%lf %lf %lf %lf", &d1,&d2,&d3,&d4);
  test_eq(r,4);
  test_feq(d1, 123.456);
  test_feq(d2, .000007);
  test_feq(d3, -900123123.2000787);
  test_feq(d4, 3.2);

 done:
  ;
}

static void
test_util_path_is_relative(void)
{
  /* OS-independent tests */
  test_eq(1, path_is_relative(""));
  test_eq(1, path_is_relative("dir"));
  test_eq(1, path_is_relative("dir/"));
  test_eq(1, path_is_relative("./dir"));
  test_eq(1, path_is_relative("../dir"));

  test_eq(0, path_is_relative("/"));
  test_eq(0, path_is_relative("/dir"));
  test_eq(0, path_is_relative("/dir/"));

  /* Windows */
#ifdef _WIN32
  /* I don't have Windows so I can't test this, hence the "#ifdef
     0". These are tests that look useful, so please try to get them
     running and uncomment if it all works as it should */
  test_eq(1, path_is_relative("dir"));
  test_eq(1, path_is_relative("dir\\"));
  test_eq(1, path_is_relative("dir\\a:"));
  test_eq(1, path_is_relative("dir\\a:\\"));
  test_eq(1, path_is_relative("http:\\dir"));

  test_eq(0, path_is_relative("\\dir"));
  test_eq(0, path_is_relative("a:\\dir"));
  test_eq(0, path_is_relative("z:\\dir"));
#endif

 done:
  ;
}

#ifdef ENABLE_MEMPOOLS

/** Run unittests for memory pool allocator */
static void
test_util_mempool(void)
{
  mp_pool_t *pool = NULL;
  smartlist_t *allocated = NULL;
  int i;

  pool = mp_pool_new(1, 100);
  test_assert(pool);
  test_assert(pool->new_chunk_capacity >= 100);
  test_assert(pool->item_alloc_size >= sizeof(void*)+1);
  mp_pool_destroy(pool);
  pool = NULL;

  pool = mp_pool_new(241, 2500);
  test_assert(pool);
  test_assert(pool->new_chunk_capacity >= 10);
  test_assert(pool->item_alloc_size >= sizeof(void*)+241);
  test_eq(pool->item_alloc_size & 0x03, 0);
  test_assert(pool->new_chunk_capacity < 60);

  allocated = smartlist_new();
  for (i = 0; i < 20000; ++i) {
    if (smartlist_len(allocated) < 20 || crypto_rand_int(2)) {
      void *m = mp_pool_get(pool);
      memset(m, 0x09, 241);
      smartlist_add(allocated, m);
      //printf("%d: %p\n", i, m);
      //mp_pool_assert_ok(pool);
    } else {
      int idx = crypto_rand_int(smartlist_len(allocated));
      void *m = smartlist_get(allocated, idx);
      //printf("%d: free %p\n", i, m);
      smartlist_del(allocated, idx);
      mp_pool_release(m);
      //mp_pool_assert_ok(pool);
    }
    if (crypto_rand_int(777)==0)
      mp_pool_clean(pool, 1, 1);

    if (i % 777)
      mp_pool_assert_ok(pool);
  }

 done:
  if (allocated) {
    SMARTLIST_FOREACH(allocated, void *, m, mp_pool_release(m));
    mp_pool_assert_ok(pool);
    mp_pool_clean(pool, 0, 0);
    mp_pool_assert_ok(pool);
    smartlist_free(allocated);
  }

  if (pool)
    mp_pool_destroy(pool);
}

#endif /* ENABLE_MEMPOOLS */

/** Run unittests for memory area allocator */
static void
test_util_memarea(void)
{
  memarea_t *area = memarea_new();
  char *p1, *p2, *p3, *p1_orig;
  void *malloced_ptr = NULL;
  int i;

  test_assert(area);

  p1_orig = p1 = memarea_alloc(area,64);
  p2 = memarea_alloc_zero(area,52);
  p3 = memarea_alloc(area,11);

  test_assert(memarea_owns_ptr(area, p1));
  test_assert(memarea_owns_ptr(area, p2));
  test_assert(memarea_owns_ptr(area, p3));
  /* Make sure we left enough space. */
  test_assert(p1+64 <= p2);
  test_assert(p2+52 <= p3);
  /* Make sure we aligned. */
  test_eq(((uintptr_t)p1) % sizeof(void*), 0);
  test_eq(((uintptr_t)p2) % sizeof(void*), 0);
  test_eq(((uintptr_t)p3) % sizeof(void*), 0);
  test_assert(!memarea_owns_ptr(area, p3+8192));
  test_assert(!memarea_owns_ptr(area, p3+30));
  test_assert(tor_mem_is_zero(p2, 52));
  /* Make sure we don't overalign. */
  p1 = memarea_alloc(area, 1);
  p2 = memarea_alloc(area, 1);
  test_eq_ptr(p1+sizeof(void*), p2);
  {
    malloced_ptr = tor_malloc(64);
    test_assert(!memarea_owns_ptr(area, malloced_ptr));
    tor_free(malloced_ptr);
  }

  /* memarea_memdup */
  {
    malloced_ptr = tor_malloc(64);
    crypto_rand((char*)malloced_ptr, 64);
    p1 = memarea_memdup(area, malloced_ptr, 64);
    test_assert(p1 != malloced_ptr);
    test_memeq(p1, malloced_ptr, 64);
    tor_free(malloced_ptr);
  }

  /* memarea_strdup. */
  p1 = memarea_strdup(area,"");
  p2 = memarea_strdup(area, "abcd");
  test_assert(p1);
  test_assert(p2);
  test_streq(p1, "");
  test_streq(p2, "abcd");

  /* memarea_strndup. */
  {
    const char *s = "Ad ogni porta batte la morte e grida: il nome!";
    /* (From Turandot, act 3.) */
    size_t len = strlen(s);
    p1 = memarea_strndup(area, s, 1000);
    p2 = memarea_strndup(area, s, 10);
    test_streq(p1, s);
    test_assert(p2 >= p1 + len + 1);
    test_memeq(s, p2, 10);
    test_eq(p2[10], '\0');
    p3 = memarea_strndup(area, s, len);
    test_streq(p3, s);
    p3 = memarea_strndup(area, s, len-1);
    test_memeq(s, p3, len-1);
    test_eq(p3[len-1], '\0');
  }

  memarea_clear(area);
  p1 = memarea_alloc(area, 1);
  test_eq_ptr(p1, p1_orig);
  memarea_clear(area);

  /* Check for running over an area's size. */
  for (i = 0; i < 512; ++i) {
    p1 = memarea_alloc(area, crypto_rand_int(5)+1);
    test_assert(memarea_owns_ptr(area, p1));
  }
  memarea_assert_ok(area);
  /* Make sure we can allocate a too-big object. */
  p1 = memarea_alloc_zero(area, 9000);
  p2 = memarea_alloc_zero(area, 16);
  test_assert(memarea_owns_ptr(area, p1));
  test_assert(memarea_owns_ptr(area, p2));

 done:
  memarea_drop_all(area);
  tor_free(malloced_ptr);
}

/** Run unit tests for utility functions to get file names relative to
 * the data directory. */
static void
test_util_datadir(void)
{
  char buf[1024];
  char *f = NULL;
  char *temp_dir = NULL;

  temp_dir = get_datadir_fname(NULL);
  f = get_datadir_fname("state");
  tor_snprintf(buf, sizeof(buf), "%s"PATH_SEPARATOR"state", temp_dir);
  test_streq(f, buf);
  tor_free(f);
  f = get_datadir_fname2("cache", "thingy");
  tor_snprintf(buf, sizeof(buf),
               "%s"PATH_SEPARATOR"cache"PATH_SEPARATOR"thingy", temp_dir);
  test_streq(f, buf);
  tor_free(f);
  f = get_datadir_fname2_suffix("cache", "thingy", ".foo");
  tor_snprintf(buf, sizeof(buf),
               "%s"PATH_SEPARATOR"cache"PATH_SEPARATOR"thingy.foo", temp_dir);
  test_streq(f, buf);
  tor_free(f);
  f = get_datadir_fname_suffix("cache", ".foo");
  tor_snprintf(buf, sizeof(buf), "%s"PATH_SEPARATOR"cache.foo",
               temp_dir);
  test_streq(f, buf);

 done:
  tor_free(f);
  tor_free(temp_dir);
}

static void
test_util_strtok(void)
{
  char buf[128];
  char buf2[128];
  int i;
  char *cp1, *cp2;

  for (i = 0; i < 3; i++) {
    const char *pad1="", *pad2="";
    switch (i) {
    case 0:
      break;
    case 1:
      pad1 = " ";
      pad2 = "!";
      break;
    case 2:
      pad1 = "  ";
      pad2 = ";!";
      break;
    }
    tor_snprintf(buf, sizeof(buf), "%s", pad1);
    tor_snprintf(buf2, sizeof(buf2), "%s", pad2);
    test_assert(NULL == tor_strtok_r_impl(buf, " ", &cp1));
    test_assert(NULL == tor_strtok_r_impl(buf2, ".!..;!", &cp2));

    tor_snprintf(buf, sizeof(buf),
                 "%sGraved on the dark  in gestures of descent%s", pad1, pad1);
    tor_snprintf(buf2, sizeof(buf2),
                "%sthey.seemed;;their!.own;most.perfect;monument%s",pad2,pad2);
    /*  -- "Year's End", Richard Wilbur */

    test_streq("Graved", tor_strtok_r_impl(buf, " ", &cp1));
    test_streq("they", tor_strtok_r_impl(buf2, ".!..;!", &cp2));
#define S1() tor_strtok_r_impl(NULL, " ", &cp1)
#define S2() tor_strtok_r_impl(NULL, ".!..;!", &cp2)
    test_streq("on", S1());
    test_streq("the", S1());
    test_streq("dark", S1());
    test_streq("seemed", S2());
    test_streq("their", S2());
    test_streq("own", S2());
    test_streq("in", S1());
    test_streq("gestures", S1());
    test_streq("of", S1());
    test_streq("most", S2());
    test_streq("perfect", S2());
    test_streq("descent", S1());
    test_streq("monument", S2());
    test_eq_ptr(NULL, S1());
    test_eq_ptr(NULL, S2());
  }

  buf[0] = 0;
  test_eq_ptr(NULL, tor_strtok_r_impl(buf, " ", &cp1));
  test_eq_ptr(NULL, tor_strtok_r_impl(buf, "!", &cp1));

  strlcpy(buf, "Howdy!", sizeof(buf));
  test_streq("Howdy", tor_strtok_r_impl(buf, "!", &cp1));
  test_eq_ptr(NULL, tor_strtok_r_impl(NULL, "!", &cp1));

  strlcpy(buf, " ", sizeof(buf));
  test_eq_ptr(NULL, tor_strtok_r_impl(buf, " ", &cp1));
  strlcpy(buf, "  ", sizeof(buf));
  test_eq_ptr(NULL, tor_strtok_r_impl(buf, " ", &cp1));

  strlcpy(buf, "something  ", sizeof(buf));
  test_streq("something", tor_strtok_r_impl(buf, " ", &cp1));
  test_eq_ptr(NULL, tor_strtok_r_impl(NULL, ";", &cp1));
 done:
  ;
}

static void
test_util_find_str_at_start_of_line(void *ptr)
{
  const char *long_string =
    "howdy world. how are you? i hope it's fine.\n"
    "hello kitty\n"
    "third line";
  char *line2 = strchr(long_string,'\n')+1;
  char *line3 = strchr(line2,'\n')+1;
  const char *short_string = "hello kitty\n"
    "second line\n";
  char *short_line2 = strchr(short_string,'\n')+1;

  (void)ptr;

  test_eq_ptr(long_string, find_str_at_start_of_line(long_string, ""));
  test_eq_ptr(NULL, find_str_at_start_of_line(short_string, "nonsense"));
  test_eq_ptr(NULL, find_str_at_start_of_line(long_string, "nonsense"));
  test_eq_ptr(NULL, find_str_at_start_of_line(long_string, "\n"));
  test_eq_ptr(NULL, find_str_at_start_of_line(long_string, "how "));
  test_eq_ptr(NULL, find_str_at_start_of_line(long_string, "kitty"));
  test_eq_ptr(long_string, find_str_at_start_of_line(long_string, "h"));
  test_eq_ptr(long_string, find_str_at_start_of_line(long_string, "how"));
  test_eq_ptr(line2, find_str_at_start_of_line(long_string, "he"));
  test_eq_ptr(line2, find_str_at_start_of_line(long_string, "hell"));
  test_eq_ptr(line2, find_str_at_start_of_line(long_string, "hello k"));
  test_eq_ptr(line2, find_str_at_start_of_line(long_string, "hello kitty\n"));
  test_eq_ptr(line2, find_str_at_start_of_line(long_string, "hello kitty\nt"));
  test_eq_ptr(line3, find_str_at_start_of_line(long_string, "third"));
  test_eq_ptr(line3, find_str_at_start_of_line(long_string, "third line"));
  test_eq_ptr(NULL,  find_str_at_start_of_line(long_string, "third line\n"));
  test_eq_ptr(short_line2, find_str_at_start_of_line(short_string,
                                                     "second line\n"));
 done:
  ;
}

static void
test_util_string_is_C_identifier(void *ptr)
{
  (void)ptr;

  test_eq(1, string_is_C_identifier("string_is_C_identifier"));
  test_eq(1, string_is_C_identifier("_string_is_C_identifier"));
  test_eq(1, string_is_C_identifier("_"));
  test_eq(1, string_is_C_identifier("i"));
  test_eq(1, string_is_C_identifier("_____"));
  test_eq(1, string_is_C_identifier("__00__"));
  test_eq(1, string_is_C_identifier("__init__"));
  test_eq(1, string_is_C_identifier("_0"));
  test_eq(1, string_is_C_identifier("_0string_is_C_identifier"));
  test_eq(1, string_is_C_identifier("_0"));

  test_eq(0, string_is_C_identifier("0_string_is_C_identifier"));
  test_eq(0, string_is_C_identifier("0"));
  test_eq(0, string_is_C_identifier(""));
  test_eq(0, string_is_C_identifier(";"));
  test_eq(0, string_is_C_identifier("i;"));
  test_eq(0, string_is_C_identifier("_;"));
  test_eq(0, string_is_C_identifier("í"));
  test_eq(0, string_is_C_identifier("ñ"));

 done:
  ;
}

static void
test_util_asprintf(void *ptr)
{
#define LOREMIPSUM                                              \
  "Lorem ipsum dolor sit amet, consectetur adipisicing elit"
  char *cp=NULL, *cp2=NULL;
  int r;
  (void)ptr;

  /* simple string */
  r = tor_asprintf(&cp, "simple string 100%% safe");
  test_assert(cp);
  test_streq("simple string 100% safe", cp);
  test_eq(strlen(cp), r);
  tor_free(cp);

  /* empty string */
  r = tor_asprintf(&cp, "%s", "");
  test_assert(cp);
  test_streq("", cp);
  test_eq(strlen(cp), r);
  tor_free(cp);

  /* numbers (%i) */
  r = tor_asprintf(&cp, "I like numbers-%2i, %i, etc.", -1, 2);
  test_assert(cp);
  test_streq("I like numbers--1, 2, etc.", cp);
  test_eq(strlen(cp), r);
  /* don't free cp; next test uses it. */

  /* numbers (%d) */
  r = tor_asprintf(&cp2, "First=%d, Second=%d", 101, 202);
  test_assert(cp2);
  test_eq(strlen(cp2), r);
  test_streq("First=101, Second=202", cp2);
  test_assert(cp != cp2);
  tor_free(cp);
  tor_free(cp2);

  /* Glass-box test: a string exactly 128 characters long. */
  r = tor_asprintf(&cp, "Lorem1: %sLorem2: %s", LOREMIPSUM, LOREMIPSUM);
  test_assert(cp);
  test_eq(128, r);
  test_assert(cp[128] == '\0');
  test_streq("Lorem1: "LOREMIPSUM"Lorem2: "LOREMIPSUM, cp);
  tor_free(cp);

  /* String longer than 128 characters */
  r = tor_asprintf(&cp, "1: %s 2: %s 3: %s",
                   LOREMIPSUM, LOREMIPSUM, LOREMIPSUM);
  test_assert(cp);
  test_eq(strlen(cp), r);
  test_streq("1: "LOREMIPSUM" 2: "LOREMIPSUM" 3: "LOREMIPSUM, cp);

 done:
  tor_free(cp);
  tor_free(cp2);
}

static void
test_util_listdir(void *ptr)
{
  smartlist_t *dir_contents = NULL;
  char *fname1=NULL, *fname2=NULL, *fname3=NULL, *dir1=NULL, *dirname=NULL;
  int r;
  (void)ptr;

  fname1 = tor_strdup(get_fname("hopscotch"));
  fname2 = tor_strdup(get_fname("mumblety-peg"));
  fname3 = tor_strdup(get_fname(".hidden-file"));
  dir1   = tor_strdup(get_fname("some-directory"));
  dirname = tor_strdup(get_fname(NULL));

  test_eq(0, write_str_to_file(fname1, "X\n", 0));
  test_eq(0, write_str_to_file(fname2, "Y\n", 0));
  test_eq(0, write_str_to_file(fname3, "Z\n", 0));
#ifdef _WIN32
  r = mkdir(dir1);
#else
  r = mkdir(dir1, 0700);
#endif
  if (r) {
    fprintf(stderr, "Can't create directory %s:", dir1);
    perror("");
    exit(1);
  }

  dir_contents = tor_listdir(dirname);
  test_assert(dir_contents);
  /* make sure that each filename is listed. */
  test_assert(smartlist_contains_string_case(dir_contents, "hopscotch"));
  test_assert(smartlist_contains_string_case(dir_contents, "mumblety-peg"));
  test_assert(smartlist_contains_string_case(dir_contents, ".hidden-file"));
  test_assert(smartlist_contains_string_case(dir_contents, "some-directory"));

  test_assert(!smartlist_contains_string(dir_contents, "."));
  test_assert(!smartlist_contains_string(dir_contents, ".."));

 done:
  tor_free(fname1);
  tor_free(fname2);
  tor_free(fname3);
  tor_free(dir1);
  tor_free(dirname);
  if (dir_contents) {
    SMARTLIST_FOREACH(dir_contents, char *, cp, tor_free(cp));
    smartlist_free(dir_contents);
  }
}

static void
test_util_parent_dir(void *ptr)
{
  char *cp;
  (void)ptr;

#define T(output,expect_ok,input)               \
  do {                                          \
    int ok;                                     \
    cp = tor_strdup(input);                     \
    ok = get_parent_directory(cp);              \
    tt_int_op(expect_ok, ==, ok);               \
    if (ok==0)                                  \
      tt_str_op(output, ==, cp);                \
    tor_free(cp);                               \
  } while (0);

  T("/home/wombat", 0, "/home/wombat/knish");
  T("/home/wombat", 0, "/home/wombat/knish/");
  T("/home/wombat", 0, "/home/wombat/knish///");
  T("./home/wombat", 0, "./home/wombat/knish/");
  T("/", 0, "/home");
  T("/", 0, "/home//");
  T(".", 0, "./wombat");
  T(".", 0, "./wombat/");
  T(".", 0, "./wombat//");
  T("wombat", 0, "wombat/foo");
  T("wombat/..", 0, "wombat/../foo");
  T("wombat/../", 0, "wombat/..//foo"); /* Is this correct? */
  T("wombat/.", 0, "wombat/./foo");
  T("wombat/./", 0, "wombat/.//foo"); /* Is this correct? */
  T("wombat", 0, "wombat/..//");
  T("wombat", 0, "wombat/foo/");
  T("wombat", 0, "wombat/.foo");
  T("wombat", 0, "wombat/.foo/");

  T("wombat", -1, "");
  T("w", -1, "");
  T("wombat", 0, "wombat/knish");

  T("/", 0, "/");
  T("/", 0, "////");

 done:
  tor_free(cp);
}

#ifdef _WIN32
static void
test_util_load_win_lib(void *ptr)
{
  HANDLE h = load_windows_system_library(_T("advapi32.dll"));
  (void) ptr;

  tt_assert(h);
 done:
  if (h)
    FreeLibrary(h);
}
#endif

#ifndef _WIN32
static void
clear_hex_errno(char *hex_errno)
{
  memset(hex_errno, '\0', HEX_ERRNO_SIZE + 1);
}

static void
test_util_exit_status(void *ptr)
{
  /* Leave an extra byte for a \0 so we can do string comparison */
  char hex_errno[HEX_ERRNO_SIZE + 1];
  int n;

  (void)ptr;

  clear_hex_errno(hex_errno);
  n = format_helper_exit_status(0, 0, hex_errno);
  test_streq("0/0\n", hex_errno);
  test_eq(n, strlen(hex_errno));

  clear_hex_errno(hex_errno);
  n = format_helper_exit_status(0, 0x7FFFFFFF, hex_errno);
  test_streq("0/7FFFFFFF\n", hex_errno);
  test_eq(n, strlen(hex_errno));

  clear_hex_errno(hex_errno);
  n = format_helper_exit_status(0xFF, -0x80000000, hex_errno);
  test_streq("FF/-80000000\n", hex_errno);
  test_eq(n, strlen(hex_errno));
  test_eq(n, HEX_ERRNO_SIZE);

  clear_hex_errno(hex_errno);
  n = format_helper_exit_status(0x7F, 0, hex_errno);
  test_streq("7F/0\n", hex_errno);
  test_eq(n, strlen(hex_errno));

  clear_hex_errno(hex_errno);
  n = format_helper_exit_status(0x08, -0x242, hex_errno);
  test_streq("8/-242\n", hex_errno);
  test_eq(n, strlen(hex_errno));

 done:
  ;
}
#endif

#ifndef _WIN32
/** Check that fgets waits until a full line, and not return a partial line, on
 * a EAGAIN with a non-blocking pipe */
static void
test_util_fgets_eagain(void *ptr)
{
  int test_pipe[2] = {-1, -1};
  int retval;
  ssize_t retlen;
  char *retptr;
  FILE *test_stream = NULL;
  char buf[10];

  (void)ptr;

  /* Set up a pipe to test on */
  retval = pipe(test_pipe);
  tt_int_op(retval, >=, 0);

  /* Set up the read-end to be non-blocking */
  retval = fcntl(test_pipe[0], F_SETFL, O_NONBLOCK);
  tt_int_op(retval, >=, 0);

  /* Open it as a stdio stream */
  test_stream = fdopen(test_pipe[0], "r");
  tt_ptr_op(test_stream, !=, NULL);

  /* Send in a partial line */
  retlen = write(test_pipe[1], "A", 1);
  tt_int_op(retlen, ==, 1);
  retptr = fgets(buf, sizeof(buf), test_stream);
  tt_want(retptr == NULL);
  tt_int_op(errno, ==, EAGAIN);

  /* Send in the rest */
  retlen = write(test_pipe[1], "B\n", 2);
  tt_int_op(retlen, ==, 2);
  retptr = fgets(buf, sizeof(buf), test_stream);
  tt_ptr_op(retptr, ==, buf);
  tt_str_op(buf, ==, "AB\n");

  /* Send in a full line */
  retlen = write(test_pipe[1], "CD\n", 3);
  tt_int_op(retlen, ==, 3);
  retptr = fgets(buf, sizeof(buf), test_stream);
  tt_ptr_op(retptr, ==, buf);
  tt_str_op(buf, ==, "CD\n");

  /* Send in a partial line */
  retlen = write(test_pipe[1], "E", 1);
  tt_int_op(retlen, ==, 1);
  retptr = fgets(buf, sizeof(buf), test_stream);
  tt_ptr_op(retptr, ==, NULL);
  tt_int_op(errno, ==, EAGAIN);

  /* Send in the rest */
  retlen = write(test_pipe[1], "F\n", 2);
  tt_int_op(retlen, ==, 2);
  retptr = fgets(buf, sizeof(buf), test_stream);
  tt_ptr_op(retptr, ==, buf);
  tt_str_op(buf, ==, "EF\n");

  /* Send in a full line and close */
  retlen = write(test_pipe[1], "GH", 2);
  tt_int_op(retlen, ==, 2);
  retval = close(test_pipe[1]);
  test_pipe[1] = -1;
  tt_int_op(retval, ==, 0);
  retptr = fgets(buf, sizeof(buf), test_stream);
  tt_ptr_op(retptr, ==, buf);
  tt_str_op(buf, ==, "GH");

  /* Check for EOF */
  retptr = fgets(buf, sizeof(buf), test_stream);
  tt_ptr_op(retptr, ==, NULL);
  tt_int_op(feof(test_stream), >, 0);

 done:
  if (test_stream != NULL)
    fclose(test_stream);
  if (test_pipe[0] != -1)
    close(test_pipe[0]);
  if (test_pipe[1] != -1)
    close(test_pipe[1]);
}
#endif

#ifndef BUILDDIR
#define BUILDDIR "."
#endif

#ifdef _WIN32
#define notify_pending_waitpid_callbacks() STMT_NIL
#define TEST_CHILD "test-child.exe"
#define EOL "\r\n"
#else
#define TEST_CHILD (BUILDDIR "/src/test/test-child")
#define EOL "\n"
#endif

/** Helper function for testing tor_spawn_background */
static void
run_util_spawn_background(const char *argv[], const char *expected_out,
                          const char *expected_err, int expected_exit,
                          int expected_status)
{
  int retval, exit_code;
  ssize_t pos;
  process_handle_t *process_handle=NULL;
  char stdout_buf[100], stderr_buf[100];
  int status;

  /* Start the program */
#ifdef _WIN32
  status = tor_spawn_background(NULL, argv, NULL, &process_handle);
#else
  status = tor_spawn_background(argv[0], argv, NULL, &process_handle);
#endif

  notify_pending_waitpid_callbacks();

  test_eq(expected_status, status);
  if (status == PROCESS_STATUS_ERROR) {
    tt_ptr_op(process_handle, ==, NULL);
    return;
  }

  test_assert(process_handle != NULL);
  test_eq(expected_status, process_handle->status);

#ifndef _WIN32
  notify_pending_waitpid_callbacks();
  tt_ptr_op(process_handle->waitpid_cb, !=, NULL);
#endif

#ifdef _WIN32
  test_assert(process_handle->stdout_pipe != INVALID_HANDLE_VALUE);
  test_assert(process_handle->stderr_pipe != INVALID_HANDLE_VALUE);
#else
  test_assert(process_handle->stdout_pipe >= 0);
  test_assert(process_handle->stderr_pipe >= 0);
#endif

  /* Check stdout */
  pos = tor_read_all_from_process_stdout(process_handle, stdout_buf,
                                         sizeof(stdout_buf) - 1);
  tt_assert(pos >= 0);
  stdout_buf[pos] = '\0';
  test_eq(strlen(expected_out), pos);
  test_streq(expected_out, stdout_buf);

  notify_pending_waitpid_callbacks();

  /* Check it terminated correctly */
  retval = tor_get_exit_code(process_handle, 1, &exit_code);
  test_eq(PROCESS_EXIT_EXITED, retval);
  test_eq(expected_exit, exit_code);
  // TODO: Make test-child exit with something other than 0

#ifndef _WIN32
  notify_pending_waitpid_callbacks();
  tt_ptr_op(process_handle->waitpid_cb, ==, NULL);
#endif

  /* Check stderr */
  pos = tor_read_all_from_process_stderr(process_handle, stderr_buf,
                                         sizeof(stderr_buf) - 1);
  test_assert(pos >= 0);
  stderr_buf[pos] = '\0';
  test_streq(expected_err, stderr_buf);
  test_eq(strlen(expected_err), pos);

  notify_pending_waitpid_callbacks();

 done:
  if (process_handle)
    tor_process_handle_destroy(process_handle, 1);
}

/** Check that we can launch a process and read the output */
static void
test_util_spawn_background_ok(void *ptr)
{
  const char *argv[] = {TEST_CHILD, "--test", NULL};
  const char *expected_out = "OUT"EOL "--test"EOL "SLEEPING"EOL "DONE" EOL;
  const char *expected_err = "ERR"EOL;

  (void)ptr;

  run_util_spawn_background(argv, expected_out, expected_err, 0,
      PROCESS_STATUS_RUNNING);
}

/** Check that failing to find the executable works as expected */
static void
test_util_spawn_background_fail(void *ptr)
{
  const char *argv[] = {BUILDDIR "/src/test/no-such-file", "--test", NULL};
  const char *expected_err = "";
  char expected_out[1024];
  char code[32];
#ifdef _WIN32
  const int expected_status = PROCESS_STATUS_ERROR;
#else
  /* TODO: Once we can signal failure to exec, set this to be
   * PROCESS_STATUS_ERROR */
  const int expected_status = PROCESS_STATUS_RUNNING;
#endif

  (void)ptr;

  tor_snprintf(code, sizeof(code), "%x/%x",
    9 /* CHILD_STATE_FAILEXEC */ , ENOENT);
  tor_snprintf(expected_out, sizeof(expected_out),
    "ERR: Failed to spawn background process - code %s\n", code);

  run_util_spawn_background(argv, expected_out, expected_err, 255,
      expected_status);
}

/** Test that reading from a handle returns a partial read rather than
 * blocking */
static void
test_util_spawn_background_partial_read_impl(int exit_early)
{
  const int expected_exit = 0;
  const int expected_status = PROCESS_STATUS_RUNNING;

  int retval, exit_code;
  ssize_t pos = -1;
  process_handle_t *process_handle=NULL;
  int status;
  char stdout_buf[100], stderr_buf[100];

  const char *argv[] = {TEST_CHILD, "--test", NULL};
  const char *expected_out[] = { "OUT" EOL "--test" EOL "SLEEPING" EOL,
                                 "DONE" EOL,
                                 NULL };
  const char *expected_err = "ERR" EOL;

#ifndef _WIN32
  int eof = 0;
#endif
  int expected_out_ctr;

  if (exit_early) {
    argv[1] = "--hang";
    expected_out[0] = "OUT"EOL "--hang"EOL "SLEEPING" EOL;
  }

  /* Start the program */
#ifdef _WIN32
  status = tor_spawn_background(NULL, argv, NULL, &process_handle);
#else
  status = tor_spawn_background(argv[0], argv, NULL, &process_handle);
#endif
  test_eq(expected_status, status);
  test_assert(process_handle);
  test_eq(expected_status, process_handle->status);

  /* Check stdout */
  for (expected_out_ctr = 0; expected_out[expected_out_ctr] != NULL;) {
#ifdef _WIN32
    pos = tor_read_all_handle(process_handle->stdout_pipe, stdout_buf,
                              sizeof(stdout_buf) - 1, NULL);
#else
    /* Check that we didn't read the end of file last time */
    test_assert(!eof);
    pos = tor_read_all_handle(process_handle->stdout_handle, stdout_buf,
                              sizeof(stdout_buf) - 1, NULL, &eof);
#endif
    log_info(LD_GENERAL, "tor_read_all_handle() returned %d", (int)pos);

    /* We would have blocked, keep on trying */
    if (0 == pos)
      continue;

    test_assert(pos > 0);
    stdout_buf[pos] = '\0';
    test_streq(expected_out[expected_out_ctr], stdout_buf);
    test_eq(strlen(expected_out[expected_out_ctr]), pos);
    expected_out_ctr++;
  }

  if (exit_early) {
    tor_process_handle_destroy(process_handle, 1);
    process_handle = NULL;
    goto done;
  }

  /* The process should have exited without writing more */
#ifdef _WIN32
  pos = tor_read_all_handle(process_handle->stdout_pipe, stdout_buf,
                            sizeof(stdout_buf) - 1,
                            process_handle);
  test_eq(0, pos);
#else
  if (!eof) {
    /* We should have got all the data, but maybe not the EOF flag */
    pos = tor_read_all_handle(process_handle->stdout_handle, stdout_buf,
                              sizeof(stdout_buf) - 1,
                              process_handle, &eof);
    test_eq(0, pos);
    test_assert(eof);
  }
  /* Otherwise, we got the EOF on the last read */
#endif

  /* Check it terminated correctly */
  retval = tor_get_exit_code(process_handle, 1, &exit_code);
  test_eq(PROCESS_EXIT_EXITED, retval);
  test_eq(expected_exit, exit_code);

  // TODO: Make test-child exit with something other than 0

  /* Check stderr */
  pos = tor_read_all_from_process_stderr(process_handle, stderr_buf,
                                         sizeof(stderr_buf) - 1);
  test_assert(pos >= 0);
  stderr_buf[pos] = '\0';
  test_streq(expected_err, stderr_buf);
  test_eq(strlen(expected_err), pos);

 done:
  tor_process_handle_destroy(process_handle, 1);
}

static void
test_util_spawn_background_partial_read(void *arg)
{
  (void)arg;
  test_util_spawn_background_partial_read_impl(0);
}

static void
test_util_spawn_background_exit_early(void *arg)
{
  (void)arg;
  test_util_spawn_background_partial_read_impl(1);
}

static void
test_util_spawn_background_waitpid_notify(void *arg)
{
  int retval, exit_code;
  process_handle_t *process_handle=NULL;
  int status;
  int ms_timer;

  const char *argv[] = {TEST_CHILD, "--fast", NULL};

  (void) arg;

#ifdef _WIN32
  status = tor_spawn_background(NULL, argv, NULL, &process_handle);
#else
  status = tor_spawn_background(argv[0], argv, NULL, &process_handle);
#endif

  tt_int_op(status, ==, PROCESS_STATUS_RUNNING);
  tt_ptr_op(process_handle, !=, NULL);

  /* We're not going to look at the stdout/stderr output this time. Instead,
   * we're testing whether notify_pending_waitpid_calbacks() can report the
   * process exit (on unix) and/or whether tor_get_exit_code() can notice it
   * (on windows) */

#ifndef _WIN32
  ms_timer = 30*1000;
  tt_ptr_op(process_handle->waitpid_cb, !=, NULL);
  while (process_handle->waitpid_cb && ms_timer > 0) {
    tor_sleep_msec(100);
    ms_timer -= 100;
    notify_pending_waitpid_callbacks();
  }
  tt_int_op(ms_timer, >, 0);
  tt_ptr_op(process_handle->waitpid_cb, ==, NULL);
#endif

  ms_timer = 30*1000;
  while (((retval = tor_get_exit_code(process_handle, 0, &exit_code))
                == PROCESS_EXIT_RUNNING) && ms_timer > 0) {
    tor_sleep_msec(100);
    ms_timer -= 100;
  }
  tt_int_op(ms_timer, >, 0);

  tt_int_op(retval, ==, PROCESS_EXIT_EXITED);

 done:
  tor_process_handle_destroy(process_handle, 1);
}

#undef TEST_CHILD
#undef EOL

/**
 * Test for format_hex_number_sigsafe()
 */

static void
test_util_format_hex_number(void *ptr)
{
  int i, len;
  char buf[33];
  const struct {
    const char *str;
    unsigned int x;
  } test_data[] = {
    {"0", 0},
    {"1", 1},
    {"273A", 0x273a},
    {"FFFF", 0xffff},
    {"7FFFFFFF", 0x7fffffff},
    {"FFFFFFFF", 0xffffffff},
#if UINT_MAX >= 0xffffffff
    {"31BC421D", 0x31bc421d},
    {"FFFFFFFF", 0xffffffff},
#endif
    {NULL, 0}
  };

  (void)ptr;

  for (i = 0; test_data[i].str != NULL; ++i) {
    len = format_hex_number_sigsafe(test_data[i].x, buf, sizeof(buf));
    test_neq(len, 0);
    test_eq(len, strlen(buf));
    test_streq(buf, test_data[i].str);
  }

  test_eq(4, format_hex_number_sigsafe(0xffff, buf, 5));
  test_streq(buf, "FFFF");
  test_eq(0, format_hex_number_sigsafe(0xffff, buf, 4));
  test_eq(0, format_hex_number_sigsafe(0, buf, 1));

 done:
  return;
}

/**
 * Test for format_hex_number_sigsafe()
 */

static void
test_util_format_dec_number(void *ptr)
{
  int i, len;
  char buf[33];
  const struct {
    const char *str;
    unsigned int x;
  } test_data[] = {
    {"0", 0},
    {"1", 1},
    {"1234", 1234},
    {"12345678", 12345678},
    {"99999999",  99999999},
    {"100000000", 100000000},
    {"4294967295", 4294967295u},
#if UINT_MAX > 0xffffffff
    {"18446744073709551615", 18446744073709551615u },
#endif
    {NULL, 0}
  };

  (void)ptr;

  for (i = 0; test_data[i].str != NULL; ++i) {
    len = format_dec_number_sigsafe(test_data[i].x, buf, sizeof(buf));
    test_neq(len, 0);
    test_eq(len, strlen(buf));
    test_streq(buf, test_data[i].str);

    len = format_dec_number_sigsafe(test_data[i].x, buf,
                                    (int)(strlen(test_data[i].str) + 1));
    test_eq(len, strlen(buf));
    test_streq(buf, test_data[i].str);
  }

  test_eq(4, format_dec_number_sigsafe(7331, buf, 5));
  test_streq(buf, "7331");
  test_eq(0, format_dec_number_sigsafe(7331, buf, 4));
  test_eq(1, format_dec_number_sigsafe(0, buf, 2));
  test_eq(0, format_dec_number_sigsafe(0, buf, 1));

 done:
  return;
}

/**
 * Test that we can properly format a Windows command line
 */
static void
test_util_join_win_cmdline(void *ptr)
{
  /* Based on some test cases from "Parsing C++ Command-Line Arguments" in
   * MSDN but we don't exercise all quoting rules because tor_join_win_cmdline
   * will try to only generate simple cases for the child process to parse;
   * i.e. we never embed quoted strings in arguments. */

  const char *argvs[][4] = {
    {"a", "bb", "CCC", NULL}, // Normal
    {NULL, NULL, NULL, NULL}, // Empty argument list
    {"", NULL, NULL, NULL}, // Empty argument
    {"\"a", "b\"b", "CCC\"", NULL}, // Quotes
    {"a\tbc", "dd  dd", "E", NULL}, // Whitespace
    {"a\\\\\\b", "de fg", "H", NULL}, // Backslashes
    {"a\\\"b", "\\c", "D\\", NULL}, // Backslashes before quote
    {"a\\\\b c", "d", "E", NULL}, // Backslashes not before quote
    { NULL } // Terminator
  };

  const char *cmdlines[] = {
    "a bb CCC",
    "",
    "\"\"",
    "\\\"a b\\\"b CCC\\\"",
    "\"a\tbc\" \"dd  dd\" E",
    "a\\\\\\b \"de fg\" H",
    "a\\\\\\\"b \\c D\\",
    "\"a\\\\b c\" d E",
    NULL // Terminator
  };

  int i;
  char *joined_argv = NULL;

  (void)ptr;

  for (i=0; cmdlines[i]!=NULL; i++) {
    log_info(LD_GENERAL, "Joining argvs[%d], expecting <%s>", i, cmdlines[i]);
    joined_argv = tor_join_win_cmdline(argvs[i]);
    test_streq(cmdlines[i], joined_argv);
    tor_free(joined_argv);
  }

 done:
  tor_free(joined_argv);
}

#define MAX_SPLIT_LINE_COUNT 4
struct split_lines_test_t {
  const char *orig_line; // Line to be split (may contain \0's)
  int orig_length; // Length of orig_line
  const char *split_line[MAX_SPLIT_LINE_COUNT]; // Split lines
};

/**
 * Test that we properly split a buffer into lines
 */
static void
test_util_split_lines(void *ptr)
{
  /* Test cases. orig_line of last test case must be NULL.
   * The last element of split_line[i] must be NULL. */
  struct split_lines_test_t tests[] = {
    {"", 0, {NULL}},
    {"foo", 3, {"foo", NULL}},
    {"\n\rfoo\n\rbar\r\n", 12, {"foo", "bar", NULL}},
    {"fo o\r\nb\tar", 10, {"fo o", "b.ar", NULL}},
    {"\x0f""f\0o\0\n\x01""b\0r\0\r", 12, {".f.o.", ".b.r.", NULL}},
    {"line 1\r\nline 2", 14, {"line 1", "line 2", NULL}},
    {"line 1\r\n\r\nline 2", 16, {"line 1", "line 2", NULL}},
    {"line 1\r\n\r\r\r\nline 2", 18, {"line 1", "line 2", NULL}},
    {"line 1\r\n\n\n\n\rline 2", 18, {"line 1", "line 2", NULL}},
    {"line 1\r\n\r\t\r\nline 3", 18, {"line 1", ".", "line 3", NULL}},
    {"\n\t\r\t\nline 3", 11, {".", ".", "line 3", NULL}},
    {NULL, 0, { NULL }}
  };

  int i, j;
  char *orig_line=NULL;
  smartlist_t *sl=NULL;

  (void)ptr;

  for (i=0; tests[i].orig_line; i++) {
    sl = smartlist_new();
    /* Allocate space for string and trailing NULL */
    orig_line = tor_memdup(tests[i].orig_line, tests[i].orig_length + 1);
    tor_split_lines(sl, orig_line, tests[i].orig_length);

    j = 0;
    log_info(LD_GENERAL, "Splitting test %d of length %d",
             i, tests[i].orig_length);
    SMARTLIST_FOREACH_BEGIN(sl, const char *, line) {
      /* Check we have not got too many lines */
      test_assert(j < MAX_SPLIT_LINE_COUNT);
      /* Check that there actually should be a line here */
      test_assert(tests[i].split_line[j] != NULL);
      log_info(LD_GENERAL, "Line %d of test %d, should be <%s>",
               j, i, tests[i].split_line[j]);
      /* Check that the line is as expected */
      test_streq(line, tests[i].split_line[j]);
      j++;
    } SMARTLIST_FOREACH_END(line);
    /* Check that we didn't miss some lines */
    test_eq_ptr(NULL, tests[i].split_line[j]);
    tor_free(orig_line);
    smartlist_free(sl);
    sl = NULL;
  }

 done:
  tor_free(orig_line);
  smartlist_free(sl);
}

static void
test_util_di_ops(void)
{
#define LT -1
#define GT 1
#define EQ 0
  const struct {
    const char *a; int want_sign; const char *b;
  } examples[] = {
    { "Foo", EQ, "Foo" },
    { "foo", GT, "bar", },
    { "foobar", EQ ,"foobar" },
    { "foobar", LT, "foobaw" },
    { "foobar", GT, "f00bar" },
    { "foobar", GT, "boobar" },
    { "", EQ, "" },
    { NULL, 0, NULL },
  };

  int i;

  for (i = 0; examples[i].a; ++i) {
    size_t len = strlen(examples[i].a);
    int eq1, eq2, neq1, neq2, cmp1, cmp2;
    test_eq(len, strlen(examples[i].b));
    /* We do all of the operations, with operands in both orders. */
    eq1 = tor_memeq(examples[i].a, examples[i].b, len);
    eq2 = tor_memeq(examples[i].b, examples[i].a, len);
    neq1 = tor_memneq(examples[i].a, examples[i].b, len);
    neq2 = tor_memneq(examples[i].b, examples[i].a, len);
    cmp1 = tor_memcmp(examples[i].a, examples[i].b, len);
    cmp2 = tor_memcmp(examples[i].b, examples[i].a, len);

    /* Check for correctness of cmp1 */
    if (cmp1 < 0 && examples[i].want_sign != LT)
      test_fail();
    else if (cmp1 > 0 && examples[i].want_sign != GT)
      test_fail();
    else if (cmp1 == 0 && examples[i].want_sign != EQ)
      test_fail();

    /* Check for consistency of everything else with cmp1 */
    test_eq(eq1, eq2);
    test_eq(neq1, neq2);
    test_eq(cmp1, -cmp2);
    test_eq(eq1, cmp1 == 0);
    test_eq(neq1, !eq1);
  }

  tt_int_op(1, ==, safe_mem_is_zero("", 0));
  tt_int_op(1, ==, safe_mem_is_zero("", 1));
  tt_int_op(0, ==, safe_mem_is_zero("a", 1));
  tt_int_op(0, ==, safe_mem_is_zero("a", 2));
  tt_int_op(0, ==, safe_mem_is_zero("\0a", 2));
  tt_int_op(1, ==, safe_mem_is_zero("\0\0a", 2));
  tt_int_op(1, ==, safe_mem_is_zero("\0\0\0\0\0\0\0\0", 8));
  tt_int_op(1, ==, safe_mem_is_zero("\0\0\0\0\0\0\0\0a", 8));
  tt_int_op(0, ==, safe_mem_is_zero("\0\0\0\0\0\0\0\0a", 9));

 done:
  ;
}

/**
 * Test counting high bits
 */
static void
test_util_n_bits_set(void *ptr)
{
  (void)ptr;
  test_eq(0, n_bits_set_u8(0));
  test_eq(1, n_bits_set_u8(1));
  test_eq(3, n_bits_set_u8(7));
  test_eq(1, n_bits_set_u8(8));
  test_eq(2, n_bits_set_u8(129));
  test_eq(8, n_bits_set_u8(255));
 done:
  ;
}

/**
 * Test LHS whitespace (and comment) eater
 */
static void
test_util_eat_whitespace(void *ptr)
{
  const char ws[] = { ' ', '\t', '\r' }; /* Except NL */
  char str[80];
  size_t i;

  (void)ptr;

  /* Try one leading ws */
  strlcpy(str, "fuubaar", sizeof(str));
  for (i = 0; i < sizeof(ws); ++i) {
    str[0] = ws[i];
    test_eq_ptr(str + 1, eat_whitespace(str));
    test_eq_ptr(str + 1, eat_whitespace_eos(str, str + strlen(str)));
    test_eq_ptr(str + 1, eat_whitespace_no_nl(str));
    test_eq_ptr(str + 1, eat_whitespace_eos_no_nl(str, str + strlen(str)));
  }
  str[0] = '\n';
  test_eq_ptr(str + 1, eat_whitespace(str));
  test_eq_ptr(str + 1, eat_whitespace_eos(str, str + strlen(str)));
  test_eq_ptr(str,     eat_whitespace_no_nl(str));
  test_eq_ptr(str,     eat_whitespace_eos_no_nl(str, str + strlen(str)));

  /* Empty string */
  strlcpy(str, "", sizeof(str));
  test_eq_ptr(str, eat_whitespace(str));
  test_eq_ptr(str, eat_whitespace_eos(str, str));
  test_eq_ptr(str, eat_whitespace_no_nl(str));
  test_eq_ptr(str, eat_whitespace_eos_no_nl(str, str));

  /* Only ws */
  strlcpy(str, " \t\r\n", sizeof(str));
  test_eq_ptr(str + strlen(str), eat_whitespace(str));
  test_eq_ptr(str + strlen(str), eat_whitespace_eos(str, str + strlen(str)));
  test_eq_ptr(str + strlen(str) - 1,
              eat_whitespace_no_nl(str));
  test_eq_ptr(str + strlen(str) - 1,
              eat_whitespace_eos_no_nl(str, str + strlen(str)));

  strlcpy(str, " \t\r ", sizeof(str));
  test_eq_ptr(str + strlen(str), eat_whitespace(str));
  test_eq_ptr(str + strlen(str),
              eat_whitespace_eos(str, str + strlen(str)));
  test_eq_ptr(str + strlen(str), eat_whitespace_no_nl(str));
  test_eq_ptr(str + strlen(str),
              eat_whitespace_eos_no_nl(str, str + strlen(str)));

  /* Multiple ws */
  strlcpy(str, "fuubaar", sizeof(str));
  for (i = 0; i < sizeof(ws); ++i)
    str[i] = ws[i];
  test_eq_ptr(str + sizeof(ws), eat_whitespace(str));
  test_eq_ptr(str + sizeof(ws), eat_whitespace_eos(str, str + strlen(str)));
  test_eq_ptr(str + sizeof(ws), eat_whitespace_no_nl(str));
  test_eq_ptr(str + sizeof(ws),
              eat_whitespace_eos_no_nl(str, str + strlen(str)));

  /* Eat comment */
  strlcpy(str, "# Comment \n No Comment", sizeof(str));
  test_streq("No Comment", eat_whitespace(str));
  test_streq("No Comment", eat_whitespace_eos(str, str + strlen(str)));
  test_eq_ptr(str, eat_whitespace_no_nl(str));
  test_eq_ptr(str, eat_whitespace_eos_no_nl(str, str + strlen(str)));

  /* Eat comment & ws mix */
  strlcpy(str, " # \t Comment \n\t\nNo Comment", sizeof(str));
  test_streq("No Comment", eat_whitespace(str));
  test_streq("No Comment", eat_whitespace_eos(str, str + strlen(str)));
  test_eq_ptr(str + 1, eat_whitespace_no_nl(str));
  test_eq_ptr(str + 1, eat_whitespace_eos_no_nl(str, str + strlen(str)));

  /* Eat entire comment */
  strlcpy(str, "#Comment", sizeof(str));
  test_eq_ptr(str + strlen(str), eat_whitespace(str));
  test_eq_ptr(str + strlen(str), eat_whitespace_eos(str, str + strlen(str)));
  test_eq_ptr(str, eat_whitespace_no_nl(str));
  test_eq_ptr(str, eat_whitespace_eos_no_nl(str, str + strlen(str)));

  /* Blank line, then comment */
  strlcpy(str, " \t\n # Comment", sizeof(str));
  test_eq_ptr(str + strlen(str), eat_whitespace(str));
  test_eq_ptr(str + strlen(str), eat_whitespace_eos(str, str + strlen(str)));
  test_eq_ptr(str + 2, eat_whitespace_no_nl(str));
  test_eq_ptr(str + 2, eat_whitespace_eos_no_nl(str, str + strlen(str)));

 done:
  ;
}

/** Return a newly allocated smartlist containing the lines of text in
 * <b>lines</b>.  The returned strings are heap-allocated, and must be
 * freed by the caller.
 *
 * XXXX? Move to container.[hc] ? */
static smartlist_t *
smartlist_new_from_text_lines(const char *lines)
{
  smartlist_t *sl = smartlist_new();
  char *last_line;

  smartlist_split_string(sl, lines, "\n", 0, 0);

  last_line = smartlist_pop_last(sl);
  if (last_line != NULL && *last_line != '\0') {
    smartlist_add(sl, last_line);
  } else {
    tor_free(last_line);
  }

  return sl;
}

/** Test smartlist_new_from_text_lines */
static void
test_util_sl_new_from_text_lines(void *ptr)
{
  (void)ptr;

  { /* Normal usage */
    smartlist_t *sl = smartlist_new_from_text_lines("foo\nbar\nbaz\n");
    int sl_len = smartlist_len(sl);

    tt_want_int_op(sl_len, ==, 3);

    if (sl_len > 0) tt_want_str_op(smartlist_get(sl, 0), ==, "foo");
    if (sl_len > 1) tt_want_str_op(smartlist_get(sl, 1), ==, "bar");
    if (sl_len > 2) tt_want_str_op(smartlist_get(sl, 2), ==, "baz");

    SMARTLIST_FOREACH(sl, void *, x, tor_free(x));
    smartlist_free(sl);
  }

  { /* No final newline */
    smartlist_t *sl = smartlist_new_from_text_lines("foo\nbar\nbaz");
    int sl_len = smartlist_len(sl);

    tt_want_int_op(sl_len, ==, 3);

    if (sl_len > 0) tt_want_str_op(smartlist_get(sl, 0), ==, "foo");
    if (sl_len > 1) tt_want_str_op(smartlist_get(sl, 1), ==, "bar");
    if (sl_len > 2) tt_want_str_op(smartlist_get(sl, 2), ==, "baz");

    SMARTLIST_FOREACH(sl, void *, x, tor_free(x));
    smartlist_free(sl);
  }

  { /* No newlines */
    smartlist_t *sl = smartlist_new_from_text_lines("foo");
    int sl_len = smartlist_len(sl);

    tt_want_int_op(sl_len, ==, 1);

    if (sl_len > 0) tt_want_str_op(smartlist_get(sl, 0), ==, "foo");

    SMARTLIST_FOREACH(sl, void *, x, tor_free(x));
    smartlist_free(sl);
  }

  { /* No text at all */
    smartlist_t *sl = smartlist_new_from_text_lines("");
    int sl_len = smartlist_len(sl);

    tt_want_int_op(sl_len, ==, 0);

    SMARTLIST_FOREACH(sl, void *, x, tor_free(x));
    smartlist_free(sl);
  }
}

static void
test_util_envnames(void *ptr)
{
  (void) ptr;

  tt_assert(environment_variable_names_equal("abc", "abc"));
  tt_assert(environment_variable_names_equal("abc", "abc="));
  tt_assert(environment_variable_names_equal("abc", "abc=def"));
  tt_assert(environment_variable_names_equal("abc=def", "abc"));
  tt_assert(environment_variable_names_equal("abc=def", "abc=ghi"));

  tt_assert(environment_variable_names_equal("abc", "abc"));
  tt_assert(environment_variable_names_equal("abc", "abc="));
  tt_assert(environment_variable_names_equal("abc", "abc=def"));
  tt_assert(environment_variable_names_equal("abc=def", "abc"));
  tt_assert(environment_variable_names_equal("abc=def", "abc=ghi"));

  tt_assert(!environment_variable_names_equal("abc", "abcd"));
  tt_assert(!environment_variable_names_equal("abc=", "abcd"));
  tt_assert(!environment_variable_names_equal("abc=", "abcd"));
  tt_assert(!environment_variable_names_equal("abc=", "def"));
  tt_assert(!environment_variable_names_equal("abc=", "def="));
  tt_assert(!environment_variable_names_equal("abc=x", "def=x"));

  tt_assert(!environment_variable_names_equal("", "a=def"));
  /* A bit surprising. */
  tt_assert(environment_variable_names_equal("", "=def"));
  tt_assert(environment_variable_names_equal("=y", "=x"));

 done:
  ;
}

/** Test process_environment_make */
static void
test_util_make_environment(void *ptr)
{
  const char *env_vars_string =
    "PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/bin\n"
    "HOME=/home/foozer\n";
  const char expected_windows_env_block[] =
    "HOME=/home/foozer\000"
    "PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/bin\000"
    "\000";
  size_t expected_windows_env_block_len =
    sizeof(expected_windows_env_block) - 1;

  smartlist_t *env_vars = smartlist_new_from_text_lines(env_vars_string);
  smartlist_t *env_vars_sorted = smartlist_new();
  smartlist_t *env_vars_in_unixoid_env_block_sorted = smartlist_new();

  process_environment_t *env;

  (void)ptr;

  env = process_environment_make(env_vars);

  /* Check that the Windows environment block is correct. */
  tt_want(tor_memeq(expected_windows_env_block, env->windows_environment_block,
                    expected_windows_env_block_len));

  /* Now for the Unixoid environment block.  We don't care which order
   * these environment variables are in, so we sort both lists first. */

  smartlist_add_all(env_vars_sorted, env_vars);

  {
    char **v;
    for (v = env->unixoid_environment_block; *v; ++v) {
      smartlist_add(env_vars_in_unixoid_env_block_sorted, *v);
    }
  }

  smartlist_sort_strings(env_vars_sorted);
  smartlist_sort_strings(env_vars_in_unixoid_env_block_sorted);

  tt_want_int_op(smartlist_len(env_vars_sorted), ==,
                 smartlist_len(env_vars_in_unixoid_env_block_sorted));
  {
    int len = smartlist_len(env_vars_sorted);
    int i;

    if (smartlist_len(env_vars_in_unixoid_env_block_sorted) < len) {
      len = smartlist_len(env_vars_in_unixoid_env_block_sorted);
    }

    for (i = 0; i < len; ++i) {
      tt_want_str_op(smartlist_get(env_vars_sorted, i), ==,
                     smartlist_get(env_vars_in_unixoid_env_block_sorted, i));
    }
  }

  /* Clean up. */
  smartlist_free(env_vars_in_unixoid_env_block_sorted);
  smartlist_free(env_vars_sorted);

  SMARTLIST_FOREACH(env_vars, char *, x, tor_free(x));
  smartlist_free(env_vars);

  process_environment_free(env);
}

/** Test set_environment_variable_in_smartlist */
static void
test_util_set_env_var_in_sl(void *ptr)
{
  /* The environment variables in these strings are in arbitrary
   * order; we sort the resulting lists before comparing them.
   *
   * (They *will not* end up in the order shown in
   * expected_resulting_env_vars_string.) */

  const char *base_env_vars_string =
    "PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/bin\n"
    "HOME=/home/foozer\n"
    "TERM=xterm\n"
    "SHELL=/bin/ksh\n"
    "USER=foozer\n"
    "LOGNAME=foozer\n"
    "USERNAME=foozer\n"
    "LANG=en_US.utf8\n"
    ;

  const char *new_env_vars_string =
    "TERM=putty\n"
    "DISPLAY=:18.0\n"
    ;

  const char *expected_resulting_env_vars_string =
    "PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/bin\n"
    "HOME=/home/foozer\n"
    "TERM=putty\n"
    "SHELL=/bin/ksh\n"
    "USER=foozer\n"
    "LOGNAME=foozer\n"
    "USERNAME=foozer\n"
    "LANG=en_US.utf8\n"
    "DISPLAY=:18.0\n"
    ;

  smartlist_t *merged_env_vars =
    smartlist_new_from_text_lines(base_env_vars_string);
  smartlist_t *new_env_vars =
    smartlist_new_from_text_lines(new_env_vars_string);
  smartlist_t *expected_resulting_env_vars =
    smartlist_new_from_text_lines(expected_resulting_env_vars_string);

  /* Elements of merged_env_vars are heap-allocated, and must be
   * freed.  Some of them are (or should) be freed by
   * set_environment_variable_in_smartlist.
   *
   * Elements of new_env_vars are heap-allocated, but are copied into
   * merged_env_vars, so they are not freed separately at the end of
   * the function.
   *
   * Elements of expected_resulting_env_vars are heap-allocated, and
   * must be freed. */

  (void)ptr;

  SMARTLIST_FOREACH(new_env_vars, char *, env_var,
                    set_environment_variable_in_smartlist(merged_env_vars,
                                                          env_var,
                                                          tor_free_,
                                                          1));

  smartlist_sort_strings(merged_env_vars);
  smartlist_sort_strings(expected_resulting_env_vars);

  tt_want_int_op(smartlist_len(merged_env_vars), ==,
                 smartlist_len(expected_resulting_env_vars));
  {
    int len = smartlist_len(merged_env_vars);
    int i;

    if (smartlist_len(expected_resulting_env_vars) < len) {
      len = smartlist_len(expected_resulting_env_vars);
    }

    for (i = 0; i < len; ++i) {
      tt_want_str_op(smartlist_get(merged_env_vars, i), ==,
                     smartlist_get(expected_resulting_env_vars, i));
    }
  }

  /* Clean up. */
  SMARTLIST_FOREACH(merged_env_vars, char *, x, tor_free(x));
  smartlist_free(merged_env_vars);

  smartlist_free(new_env_vars);

  SMARTLIST_FOREACH(expected_resulting_env_vars, char *, x, tor_free(x));
  smartlist_free(expected_resulting_env_vars);
}

static void
test_util_weak_random(void *arg)
{
  int i, j, n[16];
  tor_weak_rng_t rng;
  (void) arg;

  tor_init_weak_random(&rng, (unsigned)time(NULL));

  for (i = 1; i <= 256; ++i) {
    for (j=0;j<100;++j) {
      int r = tor_weak_random_range(&rng, i);
      tt_int_op(0, <=, r);
      tt_int_op(r, <, i);
    }
  }

  memset(n,0,sizeof(n));
  for (j=0;j<8192;++j) {
    n[tor_weak_random_range(&rng, 16)]++;
  }

  for (i=0;i<16;++i)
    tt_int_op(n[i], >, 0);
 done:
  ;
}

static void
test_util_mathlog(void *arg)
{
  double d;
  (void) arg;

  d = tor_mathlog(2.718281828);
  tt_double_op(fabs(d - 1.0), <, .000001);
  d = tor_mathlog(10);
  tt_double_op(fabs(d - 2.30258509), <, .000001);
 done:
  ;
}

static void
test_util_round_to_next_multiple_of(void *arg)
{
  (void)arg;

  test_assert(round_uint64_to_next_multiple_of(0,1) == 0);
  test_assert(round_uint64_to_next_multiple_of(0,7) == 0);

  test_assert(round_uint64_to_next_multiple_of(99,1) == 99);
  test_assert(round_uint64_to_next_multiple_of(99,7) == 105);
  test_assert(round_uint64_to_next_multiple_of(99,9) == 99);

 done:
  ;
}

static void
test_util_strclear(void *arg)
{
  static const char *vals[] = { "", "a", "abcdef", "abcdefgh", NULL };
  int i;
  char *v = NULL;
  (void)arg;

  for (i = 0; vals[i]; ++i) {
    size_t n;
    v = tor_strdup(vals[i]);
    n = strlen(v);
    tor_strclear(v);
    tt_assert(tor_mem_is_zero(v, n+1));
    tor_free(v);
  }
 done:
  tor_free(v);
}

#define UTIL_LEGACY(name)                                               \
  { #name, legacy_test_helper, 0, &legacy_setup, test_util_ ## name }

#define UTIL_TEST(name, flags)                          \
  { #name, test_util_ ## name, flags, NULL, NULL }

#ifdef FD_CLOEXEC
#define CAN_CHECK_CLOEXEC
static int
fd_is_cloexec(tor_socket_t fd)
{
  int flags = fcntl(fd, F_GETFD, 0);
  return (flags & FD_CLOEXEC) == FD_CLOEXEC;
}
#endif

#ifndef _WIN32
#define CAN_CHECK_NONBLOCK
static int
fd_is_nonblocking(tor_socket_t fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  return (flags & O_NONBLOCK) == O_NONBLOCK;
}
#endif

static void
test_util_socket(void *arg)
{
  tor_socket_t fd1 = TOR_INVALID_SOCKET;
  tor_socket_t fd2 = TOR_INVALID_SOCKET;
  tor_socket_t fd3 = TOR_INVALID_SOCKET;
  tor_socket_t fd4 = TOR_INVALID_SOCKET;
  int n = get_n_open_sockets();

  TT_BLATHER(("Starting with %d open sockets.", n));

  (void)arg;

  fd1 = tor_open_socket_with_extensions(AF_INET, SOCK_STREAM, 0, 0, 0);
  fd2 = tor_open_socket_with_extensions(AF_INET, SOCK_STREAM, 0, 0, 1);
  tt_assert(SOCKET_OK(fd1));
  tt_assert(SOCKET_OK(fd2));
  tt_int_op(get_n_open_sockets(), ==, n + 2);
  //fd3 = tor_open_socket_with_extensions(AF_INET, SOCK_STREAM, 0, 1, 0);
  //fd4 = tor_open_socket_with_extensions(AF_INET, SOCK_STREAM, 0, 1, 1);
  fd3 = tor_open_socket(AF_INET, SOCK_STREAM, 0);
  fd4 = tor_open_socket_nonblocking(AF_INET, SOCK_STREAM, 0);
  tt_assert(SOCKET_OK(fd3));
  tt_assert(SOCKET_OK(fd4));
  tt_int_op(get_n_open_sockets(), ==, n + 4);

#ifdef CAN_CHECK_CLOEXEC
  tt_int_op(fd_is_cloexec(fd1), ==, 0);
  tt_int_op(fd_is_cloexec(fd2), ==, 0);
  tt_int_op(fd_is_cloexec(fd3), ==, 1);
  tt_int_op(fd_is_cloexec(fd4), ==, 1);
#endif
#ifdef CAN_CHECK_NONBLOCK
  tt_int_op(fd_is_nonblocking(fd1), ==, 0);
  tt_int_op(fd_is_nonblocking(fd2), ==, 1);
  tt_int_op(fd_is_nonblocking(fd3), ==, 0);
  tt_int_op(fd_is_nonblocking(fd4), ==, 1);
#endif

  tor_close_socket(fd1);
  tor_close_socket(fd2);
  fd1 = fd2 = TOR_INVALID_SOCKET;
  tt_int_op(get_n_open_sockets(), ==, n + 2);
  tor_close_socket(fd3);
  tor_close_socket(fd4);
  fd3 = fd4 = TOR_INVALID_SOCKET;
  tt_int_op(get_n_open_sockets(), ==, n);

 done:
  if (SOCKET_OK(fd1))
    tor_close_socket(fd1);
  if (SOCKET_OK(fd2))
    tor_close_socket(fd2);
  if (SOCKET_OK(fd3))
    tor_close_socket(fd3);
  if (SOCKET_OK(fd4))
    tor_close_socket(fd4);
}

static void *
socketpair_test_setup(const struct testcase_t *testcase)
{
  return testcase->setup_data;
}
static int
socketpair_test_cleanup(const struct testcase_t *testcase, void *ptr)
{
  (void)testcase;
  (void)ptr;
  return 1;
}

static const struct testcase_setup_t socketpair_setup = {
  socketpair_test_setup, socketpair_test_cleanup
};

/* Test for socketpair and ersatz_socketpair().  We test them both, since
 * the latter is a tolerably good way to exersize tor_accept_socket(). */
static void
test_util_socketpair(void *arg)
{
  const int ersatz = !strcmp(arg, "1");
  int (*const tor_socketpair_fn)(int, int, int, tor_socket_t[2]) =
    ersatz ? tor_ersatz_socketpair : tor_socketpair;
  int n = get_n_open_sockets();
  tor_socket_t fds[2] = {TOR_INVALID_SOCKET, TOR_INVALID_SOCKET};
  const int family = AF_UNIX;

  tt_int_op(0, ==, tor_socketpair_fn(family, SOCK_STREAM, 0, fds));
  tt_assert(SOCKET_OK(fds[0]));
  tt_assert(SOCKET_OK(fds[1]));
  tt_int_op(get_n_open_sockets(), ==, n + 2);
#ifdef CAN_CHECK_CLOEXEC
  tt_int_op(fd_is_cloexec(fds[0]), ==, 1);
  tt_int_op(fd_is_cloexec(fds[1]), ==, 1);
#endif
#ifdef CAN_CHECK_NONBLOCK
  tt_int_op(fd_is_nonblocking(fds[0]), ==, 0);
  tt_int_op(fd_is_nonblocking(fds[1]), ==, 0);
#endif

 done:
  if (SOCKET_OK(fds[0]))
    tor_close_socket(fds[0]);
  if (SOCKET_OK(fds[1]))
    tor_close_socket(fds[1]);
}

static void
test_util_max_mem(void *arg)
{
  size_t memory1, memory2;
  int r, r2;
  (void) arg;

  r = get_total_system_memory(&memory1);
  r2 = get_total_system_memory(&memory2);
  tt_int_op(r, ==, r2);
  tt_uint_op(memory2, ==, memory1);

  TT_BLATHER(("System memory: "U64_FORMAT, U64_PRINTF_ARG(memory1)));

  if (r==0) {
    /* You have at least a megabyte. */
    tt_uint_op(memory1, >, (1<<20));
  } else {
    /* You do not have a petabyte. */
#if SIZEOF_SIZE_T == SIZEOF_UINT64_T
    tt_u64_op(memory1, <, (U64_LITERAL(1)<<50));
#endif
  }

 done:
  ;
}

struct testcase_t util_tests[] = {
  UTIL_LEGACY(time),
  UTIL_TEST(parse_http_time, 0),
  UTIL_LEGACY(config_line),
  UTIL_LEGACY(config_line_quotes),
  UTIL_LEGACY(config_line_comment_character),
  UTIL_LEGACY(config_line_escaped_content),
#ifndef _WIN32
  UTIL_LEGACY(expand_filename),
#endif
  UTIL_LEGACY(escape_string_socks),
  UTIL_LEGACY(string_is_key_value),
  UTIL_LEGACY(strmisc),
  UTIL_LEGACY(pow2),
  UTIL_LEGACY(gzip),
  UTIL_LEGACY(datadir),
#ifdef ENABLE_MEMPOOLS
  UTIL_LEGACY(mempool),
#endif
  UTIL_LEGACY(memarea),
  UTIL_LEGACY(control_formats),
  UTIL_LEGACY(mmap),
  UTIL_LEGACY(threads),
  UTIL_LEGACY(sscanf),
  UTIL_LEGACY(path_is_relative),
  UTIL_LEGACY(strtok),
  UTIL_LEGACY(di_ops),
  UTIL_TEST(round_to_next_multiple_of, 0),
  UTIL_TEST(strclear, 0),
  UTIL_TEST(find_str_at_start_of_line, 0),
  UTIL_TEST(string_is_C_identifier, 0),
  UTIL_TEST(asprintf, 0),
  UTIL_TEST(listdir, 0),
  UTIL_TEST(parent_dir, 0),
#ifdef _WIN32
  UTIL_TEST(load_win_lib, 0),
#endif
#ifndef _WIN32
  UTIL_TEST(exit_status, 0),
  UTIL_TEST(fgets_eagain, TT_SKIP),
#endif
  UTIL_TEST(spawn_background_ok, 0),
  UTIL_TEST(spawn_background_fail, 0),
  UTIL_TEST(spawn_background_partial_read, 0),
  UTIL_TEST(spawn_background_exit_early, 0),
  UTIL_TEST(spawn_background_waitpid_notify, 0),
  UTIL_TEST(format_hex_number, 0),
  UTIL_TEST(format_dec_number, 0),
  UTIL_TEST(join_win_cmdline, 0),
  UTIL_TEST(split_lines, 0),
  UTIL_TEST(n_bits_set, 0),
  UTIL_TEST(eat_whitespace, 0),
  UTIL_TEST(sl_new_from_text_lines, 0),
  UTIL_TEST(envnames, 0),
  UTIL_TEST(make_environment, 0),
  UTIL_TEST(set_env_var_in_sl, 0),
  UTIL_TEST(read_file_eof_tiny_limit, 0),
  UTIL_TEST(read_file_eof_two_loops, 0),
  UTIL_TEST(read_file_eof_zero_bytes, 0),
  UTIL_TEST(write_chunks_to_file, 0),
  UTIL_TEST(mathlog, 0),
  UTIL_TEST(weak_random, 0),
  UTIL_TEST(socket, TT_FORK),
  { "socketpair", test_util_socketpair, TT_FORK, &socketpair_setup,
    (void*)"0" },
  { "socketpair_ersatz", test_util_socketpair, TT_FORK,
    &socketpair_setup, (void*)"1" },
  UTIL_TEST(max_mem, 0),
  END_OF_TESTCASES
};

