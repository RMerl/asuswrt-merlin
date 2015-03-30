/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define BUFFERS_PRIVATE
#include "or.h"
#include "buffers.h"
#include "ext_orport.h"
#include "test.h"

/** Run unit tests for buffers.c */
static void
test_buffers_basic(void *arg)
{
  char str[256];
  char str2[256];

  buf_t *buf = NULL, *buf2 = NULL;
  const char *cp;

  int j;
  size_t r;
  (void) arg;

  /****
   * buf_new
   ****/
  if (!(buf = buf_new()))
    test_fail();

  //test_eq(buf_capacity(buf), 4096);
  test_eq(buf_datalen(buf), 0);

  /****
   * General pointer frobbing
   */
  for (j=0;j<256;++j) {
    str[j] = (char)j;
  }
  write_to_buf(str, 256, buf);
  write_to_buf(str, 256, buf);
  test_eq(buf_datalen(buf), 512);
  fetch_from_buf(str2, 200, buf);
  test_memeq(str, str2, 200);
  test_eq(buf_datalen(buf), 312);
  memset(str2, 0, sizeof(str2));

  fetch_from_buf(str2, 256, buf);
  test_memeq(str+200, str2, 56);
  test_memeq(str, str2+56, 200);
  test_eq(buf_datalen(buf), 56);
  memset(str2, 0, sizeof(str2));
  /* Okay, now we should be 512 bytes into the 4096-byte buffer.  If we add
   * another 3584 bytes, we hit the end. */
  for (j=0;j<15;++j) {
    write_to_buf(str, 256, buf);
  }
  assert_buf_ok(buf);
  test_eq(buf_datalen(buf), 3896);
  fetch_from_buf(str2, 56, buf);
  test_eq(buf_datalen(buf), 3840);
  test_memeq(str+200, str2, 56);
  for (j=0;j<15;++j) {
    memset(str2, 0, sizeof(str2));
    fetch_from_buf(str2, 256, buf);
    test_memeq(str, str2, 256);
  }
  test_eq(buf_datalen(buf), 0);
  buf_free(buf);
  buf = NULL;

  /* Okay, now make sure growing can work. */
  buf = buf_new_with_capacity(16);
  //test_eq(buf_capacity(buf), 16);
  write_to_buf(str+1, 255, buf);
  //test_eq(buf_capacity(buf), 256);
  fetch_from_buf(str2, 254, buf);
  test_memeq(str+1, str2, 254);
  //test_eq(buf_capacity(buf), 256);
  assert_buf_ok(buf);
  write_to_buf(str, 32, buf);
  //test_eq(buf_capacity(buf), 256);
  assert_buf_ok(buf);
  write_to_buf(str, 256, buf);
  assert_buf_ok(buf);
  //test_eq(buf_capacity(buf), 512);
  test_eq(buf_datalen(buf), 33+256);
  fetch_from_buf(str2, 33, buf);
  test_eq(*str2, str[255]);

  test_memeq(str2+1, str, 32);
  //test_eq(buf_capacity(buf), 512);
  test_eq(buf_datalen(buf), 256);
  fetch_from_buf(str2, 256, buf);
  test_memeq(str, str2, 256);

  /* now try shrinking: case 1. */
  buf_free(buf);
  buf = buf_new_with_capacity(33668);
  for (j=0;j<67;++j) {
    write_to_buf(str,255, buf);
  }
  //test_eq(buf_capacity(buf), 33668);
  test_eq(buf_datalen(buf), 17085);
  for (j=0; j < 40; ++j) {
    fetch_from_buf(str2, 255,buf);
    test_memeq(str2, str, 255);
  }

  /* now try shrinking: case 2. */
  buf_free(buf);
  buf = buf_new_with_capacity(33668);
  for (j=0;j<67;++j) {
    write_to_buf(str,255, buf);
  }
  for (j=0; j < 20; ++j) {
    fetch_from_buf(str2, 255,buf);
    test_memeq(str2, str, 255);
  }
  for (j=0;j<80;++j) {
    write_to_buf(str,255, buf);
  }
  //test_eq(buf_capacity(buf),33668);
  for (j=0; j < 120; ++j) {
    fetch_from_buf(str2, 255,buf);
    test_memeq(str2, str, 255);
  }

  /* Move from buf to buf. */
  buf_free(buf);
  buf = buf_new_with_capacity(4096);
  buf2 = buf_new_with_capacity(4096);
  for (j=0;j<100;++j)
    write_to_buf(str, 255, buf);
  test_eq(buf_datalen(buf), 25500);
  for (j=0;j<100;++j) {
    r = 10;
    move_buf_to_buf(buf2, buf, &r);
    test_eq(r, 0);
  }
  test_eq(buf_datalen(buf), 24500);
  test_eq(buf_datalen(buf2), 1000);
  for (j=0;j<3;++j) {
    fetch_from_buf(str2, 255, buf2);
    test_memeq(str2, str, 255);
  }
  r = 8192; /*big move*/
  move_buf_to_buf(buf2, buf, &r);
  test_eq(r, 0);
  r = 30000; /* incomplete move */
  move_buf_to_buf(buf2, buf, &r);
  test_eq(r, 13692);
  for (j=0;j<97;++j) {
    fetch_from_buf(str2, 255, buf2);
    test_memeq(str2, str, 255);
  }
  buf_free(buf);
  buf_free(buf2);
  buf = buf2 = NULL;

  buf = buf_new_with_capacity(5);
  cp = "Testing. This is a moderately long Testing string.";
  for (j = 0; cp[j]; j++)
    write_to_buf(cp+j, 1, buf);
  test_eq(0, buf_find_string_offset(buf, "Testing", 7));
  test_eq(1, buf_find_string_offset(buf, "esting", 6));
  test_eq(1, buf_find_string_offset(buf, "est", 3));
  test_eq(39, buf_find_string_offset(buf, "ing str", 7));
  test_eq(35, buf_find_string_offset(buf, "Testing str", 11));
  test_eq(32, buf_find_string_offset(buf, "ng ", 3));
  test_eq(43, buf_find_string_offset(buf, "string.", 7));
  test_eq(-1, buf_find_string_offset(buf, "shrdlu", 6));
  test_eq(-1, buf_find_string_offset(buf, "Testing thing", 13));
  test_eq(-1, buf_find_string_offset(buf, "ngx", 3));
  buf_free(buf);
  buf = NULL;

  /* Try adding a string too long for any freelist. */
  {
    char *cp = tor_malloc_zero(65536);
    buf = buf_new();
    write_to_buf(cp, 65536, buf);
    tor_free(cp);

    tt_int_op(buf_datalen(buf), ==, 65536);
    buf_free(buf);
    buf = NULL;
  }

 done:
  if (buf)
    buf_free(buf);
  if (buf2)
    buf_free(buf2);
  buf_shrink_freelists(1);
}

static void
test_buffer_pullup(void *arg)
{
  buf_t *buf;
  char *stuff, *tmp;
  const char *cp;
  size_t sz;
  (void)arg;
  stuff = tor_malloc(16384);
  tmp = tor_malloc(16384);

  /* Note: this test doesn't check the nulterminate argument to buf_pullup,
     since nothing actually uses it.  We should remove it some time. */

  buf = buf_new_with_capacity(3000); /* rounds up to next power of 2. */

  tt_assert(buf);
  tt_int_op(buf_get_default_chunk_size(buf), ==, 4096);

  tt_int_op(buf_get_total_allocation(), ==, 0);

  /* There are a bunch of cases for pullup.  One is the trivial case. Let's
     mess around with an empty buffer. */
  buf_pullup(buf, 16, 1);
  buf_get_first_chunk_data(buf, &cp, &sz);
  tt_ptr_op(cp, ==, NULL);
  tt_uint_op(sz, ==, 0);

  /* Let's make sure nothing got allocated */
  tt_int_op(buf_get_total_allocation(), ==, 0);

  /* Case 1: everything puts into the first chunk with some moving. */

  /* Let's add some data. */
  crypto_rand(stuff, 16384);
  write_to_buf(stuff, 3000, buf);
  write_to_buf(stuff+3000, 3000, buf);
  buf_get_first_chunk_data(buf, &cp, &sz);
  tt_ptr_op(cp, !=, NULL);
  tt_int_op(sz, <=, 4096);

  /* Make room for 3000 bytes in the first chunk, so that the pullup-move code
   * can get tested. */
  tt_int_op(fetch_from_buf(tmp, 3000, buf), ==, 3000);
  test_memeq(tmp, stuff, 3000);
  buf_pullup(buf, 2048, 0);
  assert_buf_ok(buf);
  buf_get_first_chunk_data(buf, &cp, &sz);
  tt_ptr_op(cp, !=, NULL);
  tt_int_op(sz, >=, 2048);
  test_memeq(cp, stuff+3000, 2048);
  tt_int_op(3000, ==, buf_datalen(buf));
  tt_int_op(fetch_from_buf(tmp, 3000, buf), ==, 0);
  test_memeq(tmp, stuff+3000, 2048);

  buf_free(buf);

  /* Now try the large-chunk case. */
  buf = buf_new_with_capacity(3000); /* rounds up to next power of 2. */
  write_to_buf(stuff, 4000, buf);
  write_to_buf(stuff+4000, 4000, buf);
  write_to_buf(stuff+8000, 4000, buf);
  write_to_buf(stuff+12000, 4000, buf);
  tt_int_op(buf_datalen(buf), ==, 16000);
  buf_get_first_chunk_data(buf, &cp, &sz);
  tt_ptr_op(cp, !=, NULL);
  tt_int_op(sz, <=, 4096);

  buf_pullup(buf, 12500, 0);
  assert_buf_ok(buf);
  buf_get_first_chunk_data(buf, &cp, &sz);
  tt_ptr_op(cp, !=, NULL);
  tt_int_op(sz, >=, 12500);
  test_memeq(cp, stuff, 12500);
  tt_int_op(buf_datalen(buf), ==, 16000);

  fetch_from_buf(tmp, 12400, buf);
  test_memeq(tmp, stuff, 12400);
  tt_int_op(buf_datalen(buf), ==, 3600);
  fetch_from_buf(tmp, 3500, buf);
  test_memeq(tmp, stuff+12400, 3500);
  fetch_from_buf(tmp, 100, buf);
  test_memeq(tmp, stuff+15900, 10);

  buf_free(buf);

  /* Make sure that the pull-up-whole-buffer case works */
  buf = buf_new_with_capacity(3000); /* rounds up to next power of 2. */
  write_to_buf(stuff, 4000, buf);
  write_to_buf(stuff+4000, 4000, buf);
  fetch_from_buf(tmp, 100, buf); /* dump 100 bytes from first chunk */
  buf_pullup(buf, 16000, 0); /* Way too much. */
  assert_buf_ok(buf);
  buf_get_first_chunk_data(buf, &cp, &sz);
  tt_ptr_op(cp, !=, NULL);
  tt_int_op(sz, ==, 7900);
  test_memeq(cp, stuff+100, 7900);

  buf_free(buf);
  buf = NULL;

  buf_shrink_freelists(1);

  tt_int_op(buf_get_total_allocation(), ==, 0);
 done:
  buf_free(buf);
  buf_shrink_freelists(1);
  tor_free(stuff);
  tor_free(tmp);
}

static void
test_buffer_copy(void *arg)
{
  generic_buffer_t *buf=NULL, *buf2=NULL;
  const char *s;
  size_t len;
  char b[256];
  int i;
  (void)arg;

  buf = generic_buffer_new();
  tt_assert(buf);

  /* Copy an empty buffer. */
  tt_int_op(0, ==, generic_buffer_set_to_copy(&buf2, buf));
  tt_assert(buf2);
  tt_int_op(0, ==, generic_buffer_len(buf2));

  /* Now try with a short buffer. */
  s = "And now comes an act of enormous enormance!";
  len = strlen(s);
  generic_buffer_add(buf, s, len);
  tt_int_op(len, ==, generic_buffer_len(buf));
  /* Add junk to buf2 so we can test replacing.*/
  generic_buffer_add(buf2, "BLARG", 5);
  tt_int_op(0, ==, generic_buffer_set_to_copy(&buf2, buf));
  tt_int_op(len, ==, generic_buffer_len(buf2));
  generic_buffer_get(buf2, b, len);
  test_mem_op(b, ==, s, len);
  /* Now free buf2 and retry so we can test allocating */
  generic_buffer_free(buf2);
  buf2 = NULL;
  tt_int_op(0, ==, generic_buffer_set_to_copy(&buf2, buf));
  tt_int_op(len, ==, generic_buffer_len(buf2));
  generic_buffer_get(buf2, b, len);
  test_mem_op(b, ==, s, len);
  /* Clear buf for next test */
  generic_buffer_get(buf, b, len);
  tt_int_op(generic_buffer_len(buf),==,0);

  /* Okay, now let's try a bigger buffer. */
  s = "Quis autem vel eum iure reprehenderit qui in ea voluptate velit "
    "esse quam nihil molestiae consequatur, vel illum qui dolorem eum "
    "fugiat quo voluptas nulla pariatur?";
  len = strlen(s);
  for (i = 0; i < 256; ++i) {
    b[0]=i;
    generic_buffer_add(buf, b, 1);
    generic_buffer_add(buf, s, len);
  }
  tt_int_op(0, ==, generic_buffer_set_to_copy(&buf2, buf));
  tt_int_op(generic_buffer_len(buf2), ==, generic_buffer_len(buf));
  for (i = 0; i < 256; ++i) {
    generic_buffer_get(buf2, b, len+1);
    tt_int_op((unsigned char)b[0],==,i);
    test_mem_op(b+1, ==, s, len);
  }

 done:
  if (buf)
    generic_buffer_free(buf);
  if (buf2)
    generic_buffer_free(buf2);
  buf_shrink_freelists(1);
}

static void
test_buffer_ext_or_cmd(void *arg)
{
  ext_or_cmd_t *cmd = NULL;
  generic_buffer_t *buf = generic_buffer_new();
  char *tmp = NULL;
  (void) arg;

  /* Empty -- should give "not there. */
  tt_int_op(0, ==, generic_buffer_fetch_ext_or_cmd(buf, &cmd));
  tt_ptr_op(NULL, ==, cmd);

  /* Three bytes: shouldn't work. */
  generic_buffer_add(buf, "\x00\x20\x00", 3);
  tt_int_op(0, ==, generic_buffer_fetch_ext_or_cmd(buf, &cmd));
  tt_ptr_op(NULL, ==, cmd);
  tt_int_op(3, ==, generic_buffer_len(buf));

  /* 0020 0000: That's a nil command. It should work. */
  generic_buffer_add(buf, "\x00", 1);
  tt_int_op(1, ==, generic_buffer_fetch_ext_or_cmd(buf, &cmd));
  tt_ptr_op(NULL, !=, cmd);
  tt_int_op(0x20, ==, cmd->cmd);
  tt_int_op(0, ==, cmd->len);
  tt_int_op(0, ==, generic_buffer_len(buf));
  ext_or_cmd_free(cmd);
  cmd = NULL;

  /* Now try a length-6 command with one byte missing. */
  generic_buffer_add(buf, "\x10\x21\x00\x06""abcde", 9);
  tt_int_op(0, ==, generic_buffer_fetch_ext_or_cmd(buf, &cmd));
  tt_ptr_op(NULL, ==, cmd);
  generic_buffer_add(buf, "f", 1);
  tt_int_op(1, ==, generic_buffer_fetch_ext_or_cmd(buf, &cmd));
  tt_ptr_op(NULL, !=, cmd);
  tt_int_op(0x1021, ==, cmd->cmd);
  tt_int_op(6, ==, cmd->len);
  test_mem_op("abcdef", ==, cmd->body, 6);
  tt_int_op(0, ==, generic_buffer_len(buf));
  ext_or_cmd_free(cmd);
  cmd = NULL;

  /* Now try a length-10 command with 4 extra bytes. */
  generic_buffer_add(buf, "\xff\xff\x00\x0a"
                     "loremipsum\x10\x00\xff\xff", 18);
  tt_int_op(1, ==, generic_buffer_fetch_ext_or_cmd(buf, &cmd));
  tt_ptr_op(NULL, !=, cmd);
  tt_int_op(0xffff, ==, cmd->cmd);
  tt_int_op(10, ==, cmd->len);
  test_mem_op("loremipsum", ==, cmd->body, 10);
  tt_int_op(4, ==, generic_buffer_len(buf));
  ext_or_cmd_free(cmd);
  cmd = NULL;

  /* Finally, let's try a maximum-length command. We already have the header
   * waiting. */
  tt_int_op(0, ==, generic_buffer_fetch_ext_or_cmd(buf, &cmd));
  tmp = tor_malloc_zero(65535);
  generic_buffer_add(buf, tmp, 65535);
  tt_int_op(1, ==, generic_buffer_fetch_ext_or_cmd(buf, &cmd));
  tt_ptr_op(NULL, !=, cmd);
  tt_int_op(0x1000, ==, cmd->cmd);
  tt_int_op(0xffff, ==, cmd->len);
  test_mem_op(tmp, ==, cmd->body, 65535);
  tt_int_op(0, ==, generic_buffer_len(buf));
  ext_or_cmd_free(cmd);
  cmd = NULL;

 done:
  ext_or_cmd_free(cmd);
  generic_buffer_free(buf);
  tor_free(tmp);
  buf_shrink_freelists(1);
}

static void
test_buffer_allocation_tracking(void *arg)
{
  char *junk = tor_malloc(16384);
  buf_t *buf1 = NULL, *buf2 = NULL;
  int i;

  (void)arg;

  crypto_rand(junk, 16384);
  tt_int_op(buf_get_total_allocation(), ==, 0);

  buf1 = buf_new();
  tt_assert(buf1);
  buf2 = buf_new();
  tt_assert(buf2);

  tt_int_op(buf_allocation(buf1), ==, 0);
  tt_int_op(buf_get_total_allocation(), ==, 0);

  write_to_buf(junk, 4000, buf1);
  write_to_buf(junk, 4000, buf1);
  write_to_buf(junk, 4000, buf1);
  write_to_buf(junk, 4000, buf1);
  tt_int_op(buf_allocation(buf1), ==, 16384);
  fetch_from_buf(junk, 100, buf1);
  tt_int_op(buf_allocation(buf1), ==, 16384); /* still 4 4k chunks */

  tt_int_op(buf_get_total_allocation(), ==, 16384);

  fetch_from_buf(junk, 4096, buf1); /* drop a 1k chunk... */
  tt_int_op(buf_allocation(buf1), ==, 3*4096); /* now 3 4k chunks */

#ifdef ENABLE_BUF_FREELISTS
  tt_int_op(buf_get_total_allocation(), ==, 16384); /* that chunk went onto
                                                       the freelist. */
#else
  tt_int_op(buf_get_total_allocation(), ==, 12288); /* that chunk was really
                                                       freed. */
#endif

  write_to_buf(junk, 4000, buf2);
  tt_int_op(buf_allocation(buf2), ==, 4096); /* another 4k chunk. */
  /*
   * If we're using freelists, size stays at 16384 because we just pulled a
   * chunk from the freelist.  If we aren't, we bounce back up to 16384 by
   * allocating a new chunk.
   */
  tt_int_op(buf_get_total_allocation(), ==, 16384);
  write_to_buf(junk, 4000, buf2);
  tt_int_op(buf_allocation(buf2), ==, 8192); /* another 4k chunk. */
  tt_int_op(buf_get_total_allocation(), ==, 5*4096); /* that chunk was new. */

  /* Make a really huge buffer */
  for (i = 0; i < 1000; ++i) {
    write_to_buf(junk, 4000, buf2);
  }
  tt_int_op(buf_allocation(buf2), >=, 4008000);
  tt_int_op(buf_get_total_allocation(), >=, 4008000);
  buf_free(buf2);
  buf2 = NULL;

  tt_int_op(buf_get_total_allocation(), <, 4008000);
  buf_shrink_freelists(1);
  tt_int_op(buf_get_total_allocation(), ==, buf_allocation(buf1));
  buf_free(buf1);
  buf1 = NULL;
  buf_shrink_freelists(1);
  tt_int_op(buf_get_total_allocation(), ==, 0);

 done:
  buf_free(buf1);
  buf_free(buf2);
  buf_shrink_freelists(1);
  tor_free(junk);
}

static void
test_buffer_time_tracking(void *arg)
{
  buf_t *buf=NULL, *buf2=NULL;
  struct timeval tv0;
  const time_t START = 1389288246;
  const uint32_t START_MSEC = (uint32_t) ((uint64_t)START * 1000);
  int i;
  char tmp[4096];
  (void)arg;

  crypto_rand(tmp, sizeof(tmp));

  tv0.tv_sec = START;
  tv0.tv_usec = 0;

  buf = buf_new_with_capacity(3000); /* rounds up to next power of 2. */
  tt_assert(buf);

  /* Empty buffer means the timestamp is 0. */
  tt_int_op(0, ==, buf_get_oldest_chunk_timestamp(buf, START_MSEC));
  tt_int_op(0, ==, buf_get_oldest_chunk_timestamp(buf, START_MSEC+1000));

  tor_gettimeofday_cache_set(&tv0);
  write_to_buf("ABCDEFG", 7, buf);
  tt_int_op(1000, ==, buf_get_oldest_chunk_timestamp(buf, START_MSEC+1000));

  buf2 = buf_copy(buf);
  tt_assert(buf2);
  tt_int_op(1234, ==, buf_get_oldest_chunk_timestamp(buf2, START_MSEC+1234));

  /* Now add more bytes; enough to overflow the first chunk. */
  tv0.tv_usec += 123 * 1000;
  tor_gettimeofday_cache_set(&tv0);
  for (i = 0; i < 600; ++i)
    write_to_buf("ABCDEFG", 7, buf);
  tt_int_op(4207, ==, buf_datalen(buf));

  /* The oldest bytes are still in the front. */
  tt_int_op(2000, ==, buf_get_oldest_chunk_timestamp(buf, START_MSEC+2000));

  /* Once those bytes are dropped, the chunk is still on the first
   * timestamp. */
  fetch_from_buf(tmp, 100, buf);
  tt_int_op(2000, ==, buf_get_oldest_chunk_timestamp(buf, START_MSEC+2000));

  /* But once we discard the whole first chunk, we get the data in the second
   * chunk. */
  fetch_from_buf(tmp, 4000, buf);
  tt_int_op(107, ==, buf_datalen(buf));
  tt_int_op(2000, ==, buf_get_oldest_chunk_timestamp(buf, START_MSEC+2123));

  /* This time we'll be grabbing a chunk from the freelist, and making sure
     its time gets updated */
  tv0.tv_sec += 5;
  tv0.tv_usec = 617*1000;
  tor_gettimeofday_cache_set(&tv0);
  for (i = 0; i < 600; ++i)
    write_to_buf("ABCDEFG", 7, buf);
  tt_int_op(4307, ==, buf_datalen(buf));

  tt_int_op(2000, ==, buf_get_oldest_chunk_timestamp(buf, START_MSEC+2123));
  fetch_from_buf(tmp, 4000, buf);
  fetch_from_buf(tmp, 306, buf);
  tt_int_op(0, ==, buf_get_oldest_chunk_timestamp(buf, START_MSEC+5617));
  tt_int_op(383, ==, buf_get_oldest_chunk_timestamp(buf, START_MSEC+6000));

 done:
  buf_free(buf);
  buf_free(buf2);
}

static void
test_buffers_zlib_impl(int finalize_with_nil)
{
  char *msg = NULL;
  char *contents = NULL;
  char *expanded = NULL;
  buf_t *buf = NULL;
  tor_zlib_state_t *zlib_state = NULL;
  size_t out_len, in_len;
  int done;

  buf = buf_new_with_capacity(128); /* will round up */
  zlib_state = tor_zlib_new(1, ZLIB_METHOD);

  msg = tor_malloc(512);
  crypto_rand(msg, 512);
  tt_int_op(write_to_buf_zlib(buf, zlib_state, msg, 128, 0), ==, 0);
  tt_int_op(write_to_buf_zlib(buf, zlib_state, msg+128, 128, 0), ==, 0);
  tt_int_op(write_to_buf_zlib(buf, zlib_state, msg+256, 256, 0), ==, 0);
  done = !finalize_with_nil;
  tt_int_op(write_to_buf_zlib(buf, zlib_state, "all done", 9, done), ==, 0);
  if (finalize_with_nil) {
    tt_int_op(write_to_buf_zlib(buf, zlib_state, "", 0, 1), ==, 0);
  }

  in_len = buf_datalen(buf);
  contents = tor_malloc(in_len);

  tt_int_op(fetch_from_buf(contents, in_len, buf), ==, 0);

  tt_int_op(0, ==, tor_gzip_uncompress(&expanded, &out_len,
                                       contents, in_len,
                                       ZLIB_METHOD, 1,
                                       LOG_WARN));

  tt_int_op(out_len, >=, 128);
  tt_mem_op(msg, ==, expanded, 128);
  tt_int_op(out_len, >=, 512);
  tt_mem_op(msg, ==, expanded, 512);
  tt_int_op(out_len, ==, 512+9);
  tt_mem_op("all done", ==, expanded+512, 9);

 done:
  buf_free(buf);
  tor_zlib_free(zlib_state);
  tor_free(contents);
  tor_free(expanded);
  tor_free(msg);
}

static void
test_buffers_zlib(void *arg)
{
  (void) arg;
  test_buffers_zlib_impl(0);
}
static void
test_buffers_zlib_fin_with_nil(void *arg)
{
  (void) arg;
  test_buffers_zlib_impl(1);
}

static void
test_buffers_zlib_fin_at_chunk_end(void *arg)
{
  char *msg = NULL;
  char *contents = NULL;
  char *expanded = NULL;
  buf_t *buf = NULL;
  tor_zlib_state_t *zlib_state = NULL;
  size_t out_len, in_len;
  size_t sz, headerjunk;
  (void) arg;

  buf = buf_new_with_capacity(128); /* will round up */
  sz = buf_get_default_chunk_size(buf);
  msg = tor_malloc_zero(sz);

  write_to_buf(msg, 1, buf);
  tt_assert(buf->head);

  /* Fill up the chunk so the zlib stuff won't fit in one chunk. */
  tt_uint_op(buf->head->memlen, <, sz);
  headerjunk = buf->head->memlen - 7;
  write_to_buf(msg, headerjunk-1, buf);
  tt_uint_op(buf->head->datalen, ==, headerjunk);
  tt_uint_op(buf_datalen(buf), ==, headerjunk);
  /* Write an empty string, with finalization on. */
  zlib_state = tor_zlib_new(1, ZLIB_METHOD);
  tt_int_op(write_to_buf_zlib(buf, zlib_state, "", 0, 1), ==, 0);

  in_len = buf_datalen(buf);
  contents = tor_malloc(in_len);

  tt_int_op(fetch_from_buf(contents, in_len, buf), ==, 0);

  tt_uint_op(in_len, >, headerjunk);

  tt_int_op(0, ==, tor_gzip_uncompress(&expanded, &out_len,
                                  contents + headerjunk, in_len - headerjunk,
                                  ZLIB_METHOD, 1,
                                  LOG_WARN));

  tt_int_op(out_len, ==, 0);
  tt_assert(expanded);

 done:
  buf_free(buf);
  tor_zlib_free(zlib_state);
  tor_free(contents);
  tor_free(expanded);
  tor_free(msg);
}

struct testcase_t buffer_tests[] = {
  { "basic", test_buffers_basic, TT_FORK, NULL, NULL },
  { "copy", test_buffer_copy, TT_FORK, NULL, NULL },
  { "pullup", test_buffer_pullup, TT_FORK, NULL, NULL },
  { "ext_or_cmd", test_buffer_ext_or_cmd, TT_FORK, NULL, NULL },
  { "allocation_tracking", test_buffer_allocation_tracking, TT_FORK,
    NULL, NULL },
  { "time_tracking", test_buffer_time_tracking, TT_FORK, NULL, NULL },
  { "zlib", test_buffers_zlib, TT_FORK, NULL, NULL },
  { "zlib_fin_with_nil", test_buffers_zlib_fin_with_nil, TT_FORK, NULL, NULL },
  { "zlib_fin_at_chunk_end", test_buffers_zlib_fin_at_chunk_end, TT_FORK,
    NULL, NULL},
  END_OF_TESTCASES
};

