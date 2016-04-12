/* Copyright (c) 2014, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define ROUTERLIST_PRIVATE
#include "or.h"
#include "routerlist.h"
#include "directory.h"
#include "test.h"

/* 4 digests + 3 sep + pre + post + NULL */
static char output[4*BASE64_DIGEST256_LEN+3+2+2+1];

static void
mock_get_from_dirserver(uint8_t dir_purpose, uint8_t router_purpose,
                             const char *resource, int pds_flags)
{
  (void)dir_purpose;
  (void)router_purpose;
  (void)pds_flags;
  tt_assert(resource);
  strlcpy(output, resource, sizeof(output));
 done:
  ;
}

static void
test_routerlist_initiate_descriptor_downloads(void *arg)
{
  const char *prose = "unhurried and wise, we perceive.";
  smartlist_t *digests = smartlist_new();
  (void)arg;

  for (int i = 0; i < 20; i++) {
    smartlist_add(digests, (char*)prose);
  }

  MOCK(directory_get_from_dirserver, mock_get_from_dirserver);
  initiate_descriptor_downloads(NULL, DIR_PURPOSE_FETCH_MICRODESC,
                                digests, 3, 7, 0);
  UNMOCK(directory_get_from_dirserver);

  tt_str_op(output, OP_EQ, "d/"
            "dW5odXJyaWVkIGFuZCB3aXNlLCB3ZSBwZXJjZWl2ZS4-"
            "dW5odXJyaWVkIGFuZCB3aXNlLCB3ZSBwZXJjZWl2ZS4-"
            "dW5odXJyaWVkIGFuZCB3aXNlLCB3ZSBwZXJjZWl2ZS4-"
            "dW5odXJyaWVkIGFuZCB3aXNlLCB3ZSBwZXJjZWl2ZS4"
            ".z");

 done:
  smartlist_free(digests);
}

static int count = 0;

static void
mock_initiate_descriptor_downloads(const routerstatus_t *source,
                                   int purpose, smartlist_t *digests,
                                   int lo, int hi, int pds_flags)
{
  (void)source;
  (void)purpose;
  (void)digests;
  (void)pds_flags;
  (void)hi;
  (void)lo;
  count += 1;
}

static void
test_routerlist_launch_descriptor_downloads(void *arg)
{
  smartlist_t *downloadable = smartlist_new();
  time_t now = time(NULL);
  char *cp;
  (void)arg;

  for (int i = 0; i < 100; i++) {
    cp = tor_malloc(DIGEST256_LEN);
    tt_assert(cp);
    crypto_rand(cp, DIGEST256_LEN);
    smartlist_add(downloadable, cp);
  }

  MOCK(initiate_descriptor_downloads, mock_initiate_descriptor_downloads);
  launch_descriptor_downloads(DIR_PURPOSE_FETCH_MICRODESC, downloadable,
                              NULL, now);
  tt_int_op(3, ==, count);
  UNMOCK(initiate_descriptor_downloads);

 done:
  SMARTLIST_FOREACH(downloadable, char *, cp1, tor_free(cp1));
  smartlist_free(downloadable);
}

#define NODE(name, flags) \
  { #name, test_routerlist_##name, (flags), NULL, NULL }

struct testcase_t routerlist_tests[] = {
  NODE(initiate_descriptor_downloads, 0),
  NODE(launch_descriptor_downloads, 0),
  END_OF_TESTCASES
};

