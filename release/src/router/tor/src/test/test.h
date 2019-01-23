/* Copyright (c) 2001-2003, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TOR_TEST_H
#define TOR_TEST_H

/**
 * \file test.h
 * \brief Macros and functions used by unit tests.
 */

#include "compat.h"
#include "tinytest.h"
#define TT_EXIT_TEST_FUNCTION STMT_BEGIN goto done; STMT_END
#include "tinytest_macros.h"

#ifdef __GNUC__
#define PRETTY_FUNCTION __PRETTY_FUNCTION__
#else
#define PRETTY_FUNCTION ""
#endif

/* As test_mem_op, but decodes 'hex' before comparing.  There must be a
 * local char* variable called mem_op_hex_tmp for this to work. */
#define test_mem_op_hex(expr1, op, hex)                                 \
  STMT_BEGIN                                                            \
  size_t length = strlen(hex);                                          \
  tor_free(mem_op_hex_tmp);                                             \
  mem_op_hex_tmp = tor_malloc(length/2);                                \
  tor_assert((length&1)==0);                                            \
  base16_decode(mem_op_hex_tmp, length/2, hex, length);                 \
  tt_mem_op(expr1, op, mem_op_hex_tmp, length/2);                       \
  STMT_END

#define test_memeq_hex(expr1, hex) test_mem_op_hex(expr1, OP_EQ, hex)

#define tt_double_op(a,op,b)                                            \
  tt_assert_test_type(a,b,#a" "#op" "#b,double,(val1_ op val2_),"%g",   \
                      TT_EXIT_TEST_FUNCTION)

/* Declare "double equal" in a sneaky way, so compiler won't complain about
 * comparing floats with == or !=.  Of course, only do this if you know what
 * you're doing. */
#define tt_double_eq(a,b)     \
  STMT_BEGIN                  \
  tt_double_op((a), >=, (b)); \
  tt_double_op((a), <=, (b)); \
  STMT_END

#ifdef _MSC_VER
#define U64_PRINTF_TYPE uint64_t
#define I64_PRINTF_TYPE int64_t
#else
#define U64_PRINTF_TYPE unsigned long long
#define I64_PRINTF_TYPE long long
#endif

#define tt_size_op(a,op,b)                                              \
  tt_assert_test_fmt_type(a,b,#a" "#op" "#b,size_t,(val1_ op val2_),    \
    U64_PRINTF_TYPE, U64_FORMAT,                                        \
    {print_ = (U64_PRINTF_TYPE) value_;}, {}, TT_EXIT_TEST_FUNCTION)

#define tt_u64_op(a,op,b)                                              \
  tt_assert_test_fmt_type(a,b,#a" "#op" "#b,uint64_t,(val1_ op val2_), \
    U64_PRINTF_TYPE, U64_FORMAT,                                       \
    {print_ = (U64_PRINTF_TYPE) value_;}, {}, TT_EXIT_TEST_FUNCTION)

#define tt_i64_op(a,op,b)                                              \
  tt_assert_test_fmt_type(a,b,#a" "#op" "#b,int64_t,(val1_ op val2_), \
    I64_PRINTF_TYPE, I64_FORMAT,                                       \
    {print_ = (I64_PRINTF_TYPE) value_;}, {}, TT_EXIT_TEST_FUNCTION)

const char *get_fname(const char *name);
const char *get_fname_rnd(const char *name);
struct crypto_pk_t *pk_generate(int idx);

#define US2_CONCAT_2__(a, b) a ## __ ## b
#define US_CONCAT_2__(a, b) a ## _ ## b
#define US_CONCAT_3__(a, b, c) a ## _ ## b ## _ ## c
#define US_CONCAT_2_(a, b) US_CONCAT_2__(a, b)
#define US_CONCAT_3_(a, b, c) US_CONCAT_3__(a, b, c)

/*
 * These macros are helpful for streamlining the authorship of several test
 * cases that use mocks.
 *
 * The pattern is as follows.
 * * Declare a top level namespace:
 *       #define NS_MODULE foo
 *
 * * For each test case you want to write, create a new submodule in the
 *   namespace. All mocks and other information should belong to a single
 *   submodule to avoid interference with other test cases.
 *   You can simply name the submodule after the function in the module you
 *   are testing:
 *       #define NS_SUBMODULE some_function
 *   or, if you're wanting to write several tests against the same function,
 *   ie., you are testing an aspect of that function, you can use:
 *       #define NS_SUBMODULE ASPECT(some_function, behavior)
 *
 * * Declare all the mocks you will use. The NS_DECL macro serves to declare
 *   the mock in the current namespace (defined by NS_MODULE and NS_SUBMODULE).
 *   It behaves like MOCK_DECL:
 *       NS_DECL(int, dependent_function, (void *));
 *   Here, dependent_function must be declared and implemented with the
 *   MOCK_DECL and MOCK_IMPL macros. The NS_DECL macro also defines an integer
 *   global for use for tracking how many times a mock was called, and can be
 *   accessed by CALLED(mock_name). For example, you might put
 *       CALLED(dependent_function)++;
 *   in your mock body.
 *
 * * Define a function called NS(main) that will contain the body of the
 *   test case. The NS macro can be used to reference a name in the current
 *   namespace.
 *
 * * In NS(main), indicate that a mock function in the current namespace,
 *   declared with NS_DECL is to override that in the global namespace,
 *   with the NS_MOCK macro:
 *       NS_MOCK(dependent_function)
 *   Unmock with:
 *       NS_UNMOCK(dependent_function)
 *
 * * Define the mocks with the NS macro, eg.,
 *       int
 *       NS(dependent_function)(void *)
 *       {
 *           CALLED(dependent_function)++;
 *       }
 *
 * * In the struct testcase_t array, you can use the TEST_CASE and
 *   TEST_CASE_ASPECT macros to define the cases without having to do so
 *   explicitly nor without having to reset NS_SUBMODULE, eg.,
 *       struct testcase_t foo_tests[] = {
 *         TEST_CASE_ASPECT(some_function, behavior),
 *         ...
 *         END_OF_TESTCASES
 *   which will define a test case named "some_function__behavior".
 */

#define NAME_TEST_(name) #name
#define NAME_TEST(name) NAME_TEST_(name)
#define ASPECT(test_module, test_name) US2_CONCAT_2__(test_module, test_name)
#define TEST_CASE(function)  \
  {  \
      NAME_TEST(function),  \
      NS_FULL(NS_MODULE, function, test_main),  \
      TT_FORK,  \
      NULL,  \
      NULL,  \
  }
#define TEST_CASE_ASPECT(function, aspect)  \
  {  \
      NAME_TEST(ASPECT(function, aspect)),  \
      NS_FULL(NS_MODULE, ASPECT(function, aspect), test_main),  \
      TT_FORK,  \
      NULL,  \
      NULL,  \
  }

#define NS(name) US_CONCAT_3_(NS_MODULE, NS_SUBMODULE, name)
#define NS_FULL(module, submodule, name) US_CONCAT_3_(module, submodule, name)

#define CALLED(mock_name) US_CONCAT_2_(NS(mock_name), called)
#define NS_DECL(retval, mock_fn, args) \
    extern int CALLED(mock_fn);        \
    static retval NS(mock_fn) args; int CALLED(mock_fn) = 0
#define NS_MOCK(name) MOCK(name, NS(name))
#define NS_UNMOCK(name) UNMOCK(name)

extern const struct testcase_setup_t passthrough_setup;
extern const struct testcase_setup_t ed25519_test_setup;

extern struct testcase_t accounting_tests[];
extern struct testcase_t addr_tests[];
extern struct testcase_t address_tests[];
extern struct testcase_t buffer_tests[];
extern struct testcase_t cell_format_tests[];
extern struct testcase_t cell_queue_tests[];
extern struct testcase_t channel_tests[];
extern struct testcase_t channeltls_tests[];
extern struct testcase_t checkdir_tests[];
extern struct testcase_t circuitlist_tests[];
extern struct testcase_t circuitmux_tests[];
extern struct testcase_t compat_libevent_tests[];
extern struct testcase_t config_tests[];
extern struct testcase_t connection_tests[];
extern struct testcase_t container_tests[];
extern struct testcase_t controller_tests[];
extern struct testcase_t controller_event_tests[];
extern struct testcase_t crypto_tests[];
extern struct testcase_t dir_tests[];
extern struct testcase_t dir_handle_get_tests[];
extern struct testcase_t entryconn_tests[];
extern struct testcase_t entrynodes_tests[];
extern struct testcase_t guardfraction_tests[];
extern struct testcase_t extorport_tests[];
extern struct testcase_t hs_tests[];
extern struct testcase_t introduce_tests[];
extern struct testcase_t keypin_tests[];
extern struct testcase_t link_handshake_tests[];
extern struct testcase_t logging_tests[];
extern struct testcase_t microdesc_tests[];
extern struct testcase_t nodelist_tests[];
extern struct testcase_t oom_tests[];
extern struct testcase_t oos_tests[];
extern struct testcase_t options_tests[];
extern struct testcase_t policy_tests[];
extern struct testcase_t procmon_tests[];
extern struct testcase_t protover_tests[];
extern struct testcase_t pubsub_tests[];
extern struct testcase_t pt_tests[];
extern struct testcase_t relay_tests[];
extern struct testcase_t relaycell_tests[];
extern struct testcase_t rend_cache_tests[];
extern struct testcase_t replaycache_tests[];
extern struct testcase_t router_tests[];
extern struct testcase_t routerkeys_tests[];
extern struct testcase_t routerlist_tests[];
extern struct testcase_t routerset_tests[];
extern struct testcase_t scheduler_tests[];
extern struct testcase_t socks_tests[];
extern struct testcase_t status_tests[];
extern struct testcase_t thread_tests[];
extern struct testcase_t tortls_tests[];
extern struct testcase_t util_tests[];
extern struct testcase_t util_format_tests[];
extern struct testcase_t util_process_tests[];
extern struct testcase_t dns_tests[];
extern struct testcase_t handle_tests[];
extern struct testcase_t sr_tests[];

extern struct testcase_t slow_crypto_tests[];
extern struct testcase_t slow_util_tests[];

extern struct testgroup_t testgroups[];

extern const char AUTHORITY_CERT_1[];
extern const char AUTHORITY_SIGNKEY_1[];
extern const char AUTHORITY_SIGNKEY_A_DIGEST[];
extern const char AUTHORITY_SIGNKEY_A_DIGEST256[];
extern const char AUTHORITY_CERT_2[];
extern const char AUTHORITY_SIGNKEY_2[];
extern const char AUTHORITY_SIGNKEY_B_DIGEST[];
extern const char AUTHORITY_SIGNKEY_B_DIGEST256[];
extern const char AUTHORITY_CERT_3[];
extern const char AUTHORITY_SIGNKEY_3[];
extern const char AUTHORITY_SIGNKEY_C_DIGEST[];
extern const char AUTHORITY_SIGNKEY_C_DIGEST256[];

#endif

