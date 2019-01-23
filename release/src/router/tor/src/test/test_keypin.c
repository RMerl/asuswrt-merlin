/* Copyright (c) 2014-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"
#define KEYPIN_PRIVATE
#include "or.h"
#include "keypin.h"
#include "util.h"

#include "test.h"

static void
test_keypin_parse_line(void *arg)
{
  (void)arg;
  keypin_ent_t *ent = NULL;

  /* Good line */
  ent = keypin_parse_journal_line(
                "aGVyZSBpcyBhIGdvb2Qgc2hhMSE "
                "VGhpcyBlZDI1NTE5IHNjb2ZmcyBhdCB0aGUgc2hhMS4");
  tt_assert(ent);
  tt_mem_op(ent->rsa_id, ==, "here is a good sha1!", 20);
  tt_mem_op(ent->ed25519_key, ==, "This ed25519 scoffs at the sha1.", 32);
  tor_free(ent); ent = NULL;

  /* Good line with extra stuff we will ignore. */
  ent = keypin_parse_journal_line(
                "aGVyZSBpcyBhIGdvb2Qgc2hhMSE "
                "VGhpcyBlZDI1NTE5IHNjb2ZmcyBhdCB0aGUgc2hhMS4helloworld");
  tt_assert(ent);
  tt_mem_op(ent->rsa_id, ==, "here is a good sha1!", 20);
  tt_mem_op(ent->ed25519_key, ==, "This ed25519 scoffs at the sha1.", 32);
  tor_free(ent); ent = NULL;

  /* Bad line: no space in the middle. */
  ent = keypin_parse_journal_line(
                "aGVyZSBpcyBhIGdvb2Qgc2hhMSE?"
                "VGhpcyBlZDI1NTE5IHNjb2ZmcyBhdCB0aGUgc2hhMS4");
  tt_assert(! ent);

  /* Bad line: bad base64 in RSA ID */
  ent = keypin_parse_journal_line(
                "aGVyZSBpcyBhIGdv!2Qgc2hhMSE "
                "VGhpcyBlZDI1NTE5IHNjb2ZmcyBhdCB0aGUgc2hhMS4");
  tt_assert(! ent);

  /* Bad line: bad base64 in Ed25519 */
  ent = keypin_parse_journal_line(
                "aGVyZSBpcyBhIGdvb2Qgc2hhMSE "
                "VGhpcyBlZDI1NTE5IHNjb2ZmcyB!dCB0aGUgc2hhMS4");
  tt_assert(! ent);

 done:
  tor_free(ent);
}

static smartlist_t *mock_addent_got = NULL;
static void
mock_addent(keypin_ent_t *ent)
{
  smartlist_add(mock_addent_got, ent);
  keypin_add_entry_to_map__real(ent);
}

static void
test_keypin_parse_file(void *arg)
{
  (void)arg;

  mock_addent_got = smartlist_new();
  MOCK(keypin_add_entry_to_map, mock_addent);

  /* Simple, minimal, correct example. */
  const char data1[] =
"PT09PT09PT09PT09PT09PT09PT0 PT09PT09PT09PT09PT09PT09PT09PT09PT09PT09PT0\n"
"TG9yYXggaXBzdW0gZ3J1dnZ1bHU cyB0aG5lZWQgYW1ldCwgc25lcmdlbGx5IG9uY2UtbGU\n"
"ciBsZXJraW0sIHNlZCBkbyBiYXI YmFsb290IHRlbXBvciBnbHVwcGl0dXMgdXQgbGFib3I\n"
"ZSBldCB0cnVmZnVsYSBtYWduYSA YWxpcXVhLiBVdCBlbmltIGFkIGdyaWNrbGUtZ3Jhc3M\n"
"dmVuaWFtLCBxdWlzIG1pZmYtbXU ZmZlcmVkIGdhLXp1bXBjbyBsYWJvcmlzIG5pc2kgdXQ\n"
"Y3J1ZmZ1bHVzIGV4IGVhIHNjaGw b3BwaXR5IGNvbnNlcXVhdC4gRHVpcyBhdXRlIHNuYXI\n"
"Z2dsZSBpbiBzd29tZWVzd2FucyA aW4gdm9sdXB0YXRlIGF4ZS1oYWNrZXIgZXNzZSByaXA\n"
"cHVsdXMgY3J1bW1paSBldSBtb28 ZiBudWxsYSBzbnV2di5QTFVHSFBMT1ZFUlhZWlpZLi4\n";

  tt_int_op(0, ==, keypin_load_journal_impl(data1, strlen(data1)));
  tt_int_op(8, ==, smartlist_len(mock_addent_got));
  keypin_ent_t *ent = smartlist_get(mock_addent_got, 2);
  tt_mem_op(ent->rsa_id, ==, "r lerkim, sed do bar", 20);
  tt_mem_op(ent->ed25519_key, ==, "baloot tempor gluppitus ut labor", 32);

  /* More complex example: weird lines, bogus lines,
     duplicate/conflicting lines */
  const char data2[] =
    "PT09PT09PT09PT09PT09PT09PT0 PT09PT09PT09PT09PT09PT09PT09PT09PT09PT09PT0\n"
    "# This is a comment.\n"
    "     \n"
    "QXQgdGhlIGVuZCBvZiB0aGUgeWU YXIgS3VycmVta2FybWVycnVrIHNhaWQgdG8gaGltLCA\n"
    "IllvdSBoYXZlIG1hZGUgYSBnb28 ZCBiZWdpbm5pbmcuIiBCdXQgbm8gbW9yZS4gV2l6YXI\n"
    "\n"
    "ZHMgc3BlYWsgdHJ1dGgsIGFuZCA aXQgd2FzIHRydWUgdGhhdCBhbGwgdGhlIG1hc3Rlcgo\n"
    "@reserved for a future extension \n"
    "eSBvZiBOYW1lcyB0aGF0IEdlZCA aGFkIHRvaWxlZCbyB3aW4gdGhhdCB5ZWFyIHdhcyA\n"
    "eSBvZiBOYW1lcyB0aGF0IEdlZCA aGFkIHRvaWxlZCbyB3aW4gdGhhdCB5ZWFyIHdhcy"
              "A line too long\n"
    "dGhlIG1lcmUgc3RhcnQgb2Ygd2g YXQgaGUgbXVzdCBnbyBvb!BsZWFybmluZy4uLi4uLi4\n"
    "ZHMgc3BlYWsgdaJ1dGgsIGFuZCA aXQgd2FzIHRydWUgdGhhdCBhbGwgdGhlIG1hc3Rlcgo\n"
    "ZHMgc3BlYWsgdHJ1dGgsIGFuZCA aXQgd2FzIHRydaUgdGhhdCBhbGwgdGhlIG1hc3Rlcgo\n"
    ;

  tt_int_op(0, ==, keypin_load_journal_impl(data2, strlen(data2)));
  tt_int_op(13, ==, smartlist_len(mock_addent_got));
  ent = smartlist_get(mock_addent_got, 9);
  tt_mem_op(ent->rsa_id, ==, "\"You have made a goo", 20);
  tt_mem_op(ent->ed25519_key, ==, "d beginning.\" But no more. Wizar", 32);

  ent = smartlist_get(mock_addent_got, 12);
  tt_mem_op(ent->rsa_id, ==, "ds speak truth, and ", 20);
  tt_mem_op(ent->ed25519_key, ==, "it was tru\xa5 that all the master\n", 32);

  /* File truncated before NL */
  const char data3[] =
    "Tm8gZHJhZ29uIGNhbiByZXNpc3Q IHRoZSBmYXNjaW5hdGlvbiBvZiByaWRkbGluZyB0YWw";
  tt_int_op(0, ==, keypin_load_journal_impl(data3, strlen(data3)));
  tt_int_op(14, ==, smartlist_len(mock_addent_got));
  ent = smartlist_get(mock_addent_got, 13);
  tt_mem_op(ent->rsa_id, ==, "No dragon can resist", 20);
  tt_mem_op(ent->ed25519_key, ==, " the fascination of riddling tal", 32);

 done:
  keypin_clear();
  smartlist_free(mock_addent_got);
}

#define ADD(a,b) keypin_check_and_add((const uint8_t*)(a),\
                                      (const uint8_t*)(b),0)
#define LONE_RSA(a) keypin_check_lone_rsa((const uint8_t*)(a))

static void
test_keypin_add_entry(void *arg)
{
  (void)arg;
  keypin_clear();

  tt_int_op(KEYPIN_ADDED, ==, ADD("ambassadors-at-large",
                                  "bread-and-butter thing-in-itself"));
  tt_int_op(KEYPIN_ADDED, ==, ADD("gentleman-adventurer",
                                  "cloak-and-dagger what's-his-face"));

  tt_int_op(KEYPIN_FOUND, ==, ADD("ambassadors-at-large",
                                  "bread-and-butter thing-in-itself"));
  tt_int_op(KEYPIN_FOUND, ==, ADD("ambassadors-at-large",
                                  "bread-and-butter thing-in-itself"));
  tt_int_op(KEYPIN_FOUND, ==, ADD("gentleman-adventurer",
                                  "cloak-and-dagger what's-his-face"));

  tt_int_op(KEYPIN_ADDED, ==, ADD("Johnnies-come-lately",
                                  "run-of-the-mill root-mean-square"));

  tt_int_op(KEYPIN_MISMATCH, ==, ADD("gentleman-adventurer",
                                     "hypersentimental closefistedness"));

  tt_int_op(KEYPIN_MISMATCH, ==, ADD("disestablismentarian",
                                     "cloak-and-dagger what's-his-face"));

  tt_int_op(KEYPIN_FOUND, ==, ADD("gentleman-adventurer",
                                  "cloak-and-dagger what's-his-face"));

  tt_int_op(KEYPIN_NOT_FOUND, ==, LONE_RSA("Llanfairpwllgwyngyll"));
  tt_int_op(KEYPIN_MISMATCH, ==, LONE_RSA("Johnnies-come-lately"));

 done:
  keypin_clear();
}

static void
test_keypin_journal(void *arg)
{
  (void)arg;
  char *contents = NULL;
  const char *fname = get_fname("keypin-journal");

  tt_int_op(0, ==, keypin_load_journal(fname)); /* ENOENT is okay */
  update_approx_time(1217709000);
  tt_int_op(0, ==, keypin_open_journal(fname));

  tt_int_op(KEYPIN_ADDED, ==, ADD("king-of-the-herrings",
                                  "good-for-nothing attorney-at-law"));
  tt_int_op(KEYPIN_ADDED, ==, ADD("yellowish-red-yellow",
                                  "salt-and-pepper high-muck-a-muck"));
  tt_int_op(KEYPIN_FOUND, ==, ADD("yellowish-red-yellow",
                                  "salt-and-pepper high-muck-a-muck"));
  keypin_close_journal();
  keypin_clear();

  tt_int_op(0, ==, keypin_load_journal(fname));
  update_approx_time(1231041600);
  tt_int_op(0, ==, keypin_open_journal(fname));
  tt_int_op(KEYPIN_FOUND, ==, ADD("yellowish-red-yellow",
                                  "salt-and-pepper high-muck-a-muck"));
  tt_int_op(KEYPIN_ADDED, ==, ADD("theatre-in-the-round",
                                  "holier-than-thou jack-in-the-box"));
  tt_int_op(KEYPIN_ADDED, ==, ADD("no-deposit-no-return",
                                  "across-the-board will-o-the-wisp"));
  tt_int_op(KEYPIN_MISMATCH, ==, ADD("intellectualizations",
                                     "salt-and-pepper high-muck-a-muck"));
  keypin_close_journal();
  keypin_clear();

  tt_int_op(0, ==, keypin_load_journal(fname));
  update_approx_time(1412278354);
  tt_int_op(0, ==, keypin_open_journal(fname));
  tt_int_op(KEYPIN_FOUND, ==, ADD("yellowish-red-yellow",
                                  "salt-and-pepper high-muck-a-muck"));
  tt_int_op(KEYPIN_MISMATCH, ==, ADD("intellectualizations",
                                     "salt-and-pepper high-muck-a-muck"));
  tt_int_op(KEYPIN_FOUND, ==, ADD("theatre-in-the-round",
                                  "holier-than-thou jack-in-the-box"));
  tt_int_op(KEYPIN_MISMATCH, ==, ADD("counterrevolutionary",
                                     "holier-than-thou jack-in-the-box"));
  tt_int_op(KEYPIN_MISMATCH, ==, ADD("no-deposit-no-return",
                                     "floccinaucinihilipilificationism"));
  keypin_close_journal();

  contents = read_file_to_str(fname, RFTS_BIN, NULL);
  tt_assert(contents);
  tt_str_op(contents,==,
    "\n"
    "@opened-at 2008-08-02 20:30:00\n"
    "a2luZy1vZi10aGUtaGVycmluZ3M Z29vZC1mb3Itbm90aGluZyBhdHRvcm5leS1hdC1sYXc\n"
    "eWVsbG93aXNoLXJlZC15ZWxsb3c c2FsdC1hbmQtcGVwcGVyIGhpZ2gtbXVjay1hLW11Y2s\n"
    "\n"
    "@opened-at 2009-01-04 04:00:00\n"
    "dGhlYXRyZS1pbi10aGUtcm91bmQ aG9saWVyLXRoYW4tdGhvdSBqYWNrLWluLXRoZS1ib3g\n"
    "bm8tZGVwb3NpdC1uby1yZXR1cm4 YWNyb3NzLXRoZS1ib2FyZCB3aWxsLW8tdGhlLXdpc3A\n"
    "\n"
    "@opened-at 2014-10-02 19:32:34\n");

 done:
  tor_free(contents);
  keypin_clear();
}

#undef ADD
#undef LONE_RSA

#define TEST(name, flags)                                       \
  { #name , test_keypin_ ## name, (flags), NULL, NULL }

struct testcase_t keypin_tests[] = {
  TEST( parse_line, 0 ),
  TEST( parse_file, TT_FORK ),
  TEST( add_entry, TT_FORK ),
  TEST( journal, TT_FORK ),
  END_OF_TESTCASES
};

