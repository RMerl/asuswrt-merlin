/* 
   Stupidly simple test framework
   Copyright (C) 2001-2004, Joe Orton <joe@manyfish.co.uk>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA

*/

#ifndef TESTS_H
#define TESTS_H 1

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <stdio.h>

#define OK 0
#define FAIL 1
#define FAILHARD 2 /* fail and skip all succeeding tests in this suite. */
#define SKIP 3 /* test was skipped because precondition was not met */
#define SKIPREST 4 /* skipped, and skip all succeeding tests in suite */

/* A test function. Must return any of OK, FAIL, FAILHARD, SKIP, or
 * SKIPREST.  May call t_warning() any number of times.  If not
 * returning OK, optionally call t_context to provide an error
 * message. */
typedef int (*test_func)(void);

typedef struct {
    test_func fn; /* the function to test. */
    const char *name; /* the name of the test. */
    int flags;
} ne_test;

/* possible values for flags: */
#define T_CHECK_LEAKS (1) /* check for memory leaks */
#define T_EXPECT_FAIL (2) /* expect failure */
#define T_EXPECT_LEAKS (4) /* expect memory leak failures */

/* array of tests to run: must be defined by each test suite. */
extern ne_test tests[];

/* define a test function which has the same name as the function,
 * and does check for memory leaks. */
#define T(fn) { fn, #fn, T_CHECK_LEAKS }
/* define a test function which is expected to return FAIL. */
#define T_XFAIL(fn) { fn, #fn, T_EXPECT_FAIL | T_CHECK_LEAKS }
/* define a test function which isn't checked for memory leaks. */
#define T_LEAKY(fn) { fn, #fn, 0 }
/* define a test function which is expected to fail memory leak checks */
#define T_XLEAKY(fn) { fn, #fn, T_EXPECT_LEAKS }

/* current test number */
extern int test_num;

/* name of test suite */
extern const char *test_suite;

/* Provide result context message. */
void t_context(const char *ctx, ...)
#ifdef __GNUC__
                __attribute__ ((format (printf, 1, 2)))
#endif /* __GNUC__ */
    ;

extern char test_context[];

/* the command-line arguments passed in to the test suite: */
extern char **test_argv;
extern int test_argc;

/* child process should call this. */
void in_child(void);

/* issue a warning. */
void t_warning(const char *str, ...)
#ifdef __GNUC__
                __attribute__ ((format (printf, 1, 2)))
#endif /* __GNUC__ */
;

/* Macros for easily writing is-not-zero comparison tests; the ON*
 * macros fail the function if a comparison is not zero.
 *
 * ONV(x,vs) takes a comparison X, and a printf varargs list for
 * the failure message.
 *  e.g.   ONV(strcmp(bar, "foo"), ("bar was %s not 'foo'", bar))
 *
 * ON(x) takes a comparison X, and uses the line number for the failure
 * message.   e.g.  ONV(strcmp(bar, "foo"))
 *
 * ONN(n, x) takes a comparison X, and a flat string failure message.
 *  e.g.  ONN("foo was wrong", strcmp(bar, "foo")) */

#define ONV(x,vs) do { if ((x)) { t_context vs; return FAIL; } } while (0)
#define ON(x) ONV((x), ("line %d", __LINE__ ))
#define ONN(n,x) ONV((x), (n))

/* ONCMP(exp, act, name): 'exp' is the expected string, 'act' is the
 * actual string for some field 'name'.  Succeeds if strcmp(exp,act)
 * == 0 or both are NULL. */
#define ONCMP(exp, act, ctx, name) do { \
        ONV(exp && !act, ("%s: " name " was NULL, expected '%s'", ctx, exp)); \
        ONV(!exp && act, ("%s: " name " was '%s', expected NULL", ctx, act)); \
        ONV(exp && strcmp(exp, act), ("%s: " name " was '%s' not '%s'", ctx, act, exp)); \
} while (0)

/* return immediately with result of test 'x' if it fails. */
#define CALL(x) do { int t_ret = (x); if (t_ret != OK) return t_ret; } while (0)

/* PRECOND: skip current test if condition 'x' is not true. */
#define PRECOND(x) do { if (!(x)) { return SKIP; } } while (0)

#endif /* TESTS_H */
