/* Copyright (c) 2001-2003, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
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

#define test_fail_msg(msg) TT_DIE((msg))

#define test_fail() test_fail_msg("Assertion failed.")

#define test_assert(expr) tt_assert(expr)

#define test_eq(expr1, expr2) tt_int_op((expr1), ==, (expr2))
#define test_eq_ptr(expr1, expr2) tt_ptr_op((expr1), ==, (expr2))
#define test_neq(expr1, expr2) tt_int_op((expr1), !=, (expr2))
#define test_neq_ptr(expr1, expr2) tt_ptr_op((expr1), !=, (expr2))
#define test_streq(expr1, expr2) tt_str_op((expr1), ==, (expr2))
#define test_strneq(expr1, expr2) tt_str_op((expr1), !=, (expr2))

#define test_mem_op(expr1, op, expr2, len)                              \
  tt_mem_op((expr1), op, (expr2), (len))

#define test_memeq(expr1, expr2, len) test_mem_op((expr1), ==, (expr2), len)
#define test_memneq(expr1, expr2, len) test_mem_op((expr1), !=, (expr2), len)

/* As test_mem_op, but decodes 'hex' before comparing.  There must be a
 * local char* variable called mem_op_hex_tmp for this to work. */
#define test_mem_op_hex(expr1, op, hex)                                 \
  STMT_BEGIN                                                            \
  size_t length = strlen(hex);                                          \
  tor_free(mem_op_hex_tmp);                                             \
  mem_op_hex_tmp = tor_malloc(length/2);                                \
  tor_assert((length&1)==0);                                            \
  base16_decode(mem_op_hex_tmp, length/2, hex, length);                 \
  test_mem_op(expr1, op, mem_op_hex_tmp, length/2);                     \
  STMT_END

#define test_memeq_hex(expr1, hex) test_mem_op_hex(expr1, ==, hex)

#define tt_double_op(a,op,b)                                            \
  tt_assert_test_type(a,b,#a" "#op" "#b,double,(val1_ op val2_),"%f",   \
                      TT_EXIT_TEST_FUNCTION)

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
crypto_pk_t *pk_generate(int idx);

void legacy_test_helper(void *data);
extern const struct testcase_setup_t legacy_setup;

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
    static retval NS(mock_fn) args; int CALLED(mock_fn) = 0
#define NS_MOCK(name) MOCK(name, NS(name))
#define NS_UNMOCK(name) UNMOCK(name)

#endif

