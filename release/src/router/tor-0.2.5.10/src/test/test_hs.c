/* Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file test_hs.c
 * \brief Unit tests for hidden service.
 **/

#define CONTROL_PRIVATE
#include "or.h"
#include "test.h"
#include "control.h"

/* mock ID digest and longname for node that's in nodelist */
#define HSDIR_EXIST_ID "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA" \
                       "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
#define STR_HSDIR_EXIST_LONGNAME \
                       "$AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=TestDir"
/* mock ID digest and longname for node that's not in nodelist */
#define HSDIR_NONE_EXIST_ID "\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB" \
                            "\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB"
#define STR_HSDIR_NONE_EXIST_LONGNAME \
                       "$BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"

/* Helper global variable for hidden service descriptor event test.
 * It's used as a pointer to dynamically created message buffer in
 * send_control_event_string_replacement function, which mocks
 * send_control_event_string function.
 *
 * Always free it after use! */
static char *received_msg = NULL;

/** Mock function for send_control_event_string
 */
static void
send_control_event_string_replacement(uint16_t event, event_format_t which,
                                      const char *msg)
{
  (void) event;
  (void) which;
  tor_free(received_msg);
  received_msg = tor_strdup(msg);
}

/** Mock function for node_describe_longname_by_id, it returns either
 * STR_HSDIR_EXIST_LONGNAME or STR_HSDIR_NONE_EXIST_LONGNAME
 */
static const char *
node_describe_longname_by_id_replacement(const char *id_digest)
{
  if (!strcmp(id_digest, HSDIR_EXIST_ID)) {
    return STR_HSDIR_EXIST_LONGNAME;
  } else {
    return STR_HSDIR_NONE_EXIST_LONGNAME;
  }
}

/** Make sure each hidden service descriptor async event generation
 *
 * function generates the message in expected format.
 */
static void
test_hs_desc_event(void *arg)
{
  #define STR_HS_ADDR "ajhb7kljbiru65qo"
  #define STR_HS_ID "b3oeducbhjmbqmgw2i3jtz4fekkrinwj"

  rend_data_t rend_query;
  const char *expected_msg;

  (void) arg;
  MOCK(send_control_event_string,
       send_control_event_string_replacement);
  MOCK(node_describe_longname_by_id,
       node_describe_longname_by_id_replacement);

  /* setup rend_query struct */
  strncpy(rend_query.onion_address, STR_HS_ADDR,
          REND_SERVICE_ID_LEN_BASE32+1);
  rend_query.auth_type = 0;

  /* test request event */
  control_event_hs_descriptor_requested(&rend_query, HSDIR_EXIST_ID,
                                        STR_HS_ID);
  expected_msg = "650 HS_DESC REQUESTED "STR_HS_ADDR" NO_AUTH "\
                  STR_HSDIR_EXIST_LONGNAME" "STR_HS_ID"\r\n";
  test_assert(received_msg);
  test_streq(received_msg, expected_msg);
  tor_free(received_msg);

  /* test received event */
  rend_query.auth_type = 1;
  control_event_hs_descriptor_received(&rend_query, HSDIR_EXIST_ID);
  expected_msg = "650 HS_DESC RECEIVED "STR_HS_ADDR" BASIC_AUTH "\
                  STR_HSDIR_EXIST_LONGNAME"\r\n";
  test_assert(received_msg);
  test_streq(received_msg, expected_msg);
  tor_free(received_msg);

  /* test failed event */
  rend_query.auth_type = 2;
  control_event_hs_descriptor_failed(&rend_query, HSDIR_NONE_EXIST_ID);
  expected_msg = "650 HS_DESC FAILED "STR_HS_ADDR" STEALTH_AUTH "\
                  STR_HSDIR_NONE_EXIST_LONGNAME"\r\n";
  test_assert(received_msg);
  test_streq(received_msg, expected_msg);
  tor_free(received_msg);

  /* test invalid auth type */
  rend_query.auth_type = 999;
  control_event_hs_descriptor_failed(&rend_query, HSDIR_EXIST_ID);
  expected_msg = "650 HS_DESC FAILED "STR_HS_ADDR" UNKNOWN "\
                  STR_HSDIR_EXIST_LONGNAME"\r\n";
  test_assert(received_msg);
  test_streq(received_msg, expected_msg);
  tor_free(received_msg);

 done:
  UNMOCK(send_control_event_string);
  UNMOCK(node_describe_longname_by_id);
  tor_free(received_msg);
}

struct testcase_t hs_tests[] = {
  { "hs_desc_event", test_hs_desc_event, TT_FORK,
    NULL, NULL },
  END_OF_TESTCASES
};

