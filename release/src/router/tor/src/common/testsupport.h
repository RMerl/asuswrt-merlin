/* Copyright (c) 2013-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TOR_TESTSUPPORT_H
#define TOR_TESTSUPPORT_H

#ifdef TOR_UNIT_TESTS
#define STATIC
#else
#define STATIC static
#endif

/** Quick and dirty macros to implement test mocking.
 *
 * To use them, suppose that you have a function you'd like to mock
 * with the signature "void writebuf(size_t n, char *buf)".  You can then
 * declare the function as:
 *
 *     MOCK_DECL(void, writebuf, (size_t n, char *buf));
 *
 * and implement it as:
 *
 *     MOCK_IMPL(void,
 *     writebuf,(size_t n, char *buf))
 *     {
 *          ...
 *     }
 *
 * For the non-testing build, this will expand simply into:
 *
 *     void writebuf(size_t n, char *buf);
 *     void
 *     writebuf(size_t n, char *buf)
 *     {
 *         ...
 *     }
 *
 * But for the testing case, it will expand into:
 *
 *     void writebuf__real(size_t n, char *buf);
 *     extern void (*writebuf)(size_t n, char *buf);
 *
 *     void (*writebuf)(size_t n, char *buf) = writebuf__real;
 *     void
 *     writebuf__real(size_t n, char *buf)
 *     {
 *         ...
 *     }
 *
 * This is not a great mocking system!  It is deliberately "the simplest
 * thing that could work", and pays for its simplicity in its lack of
 * features, and in its uglification of the Tor code.  Replacing it with
 * something clever would be a fine thing.
 *
 * @{ */
#ifdef TOR_UNIT_TESTS
#define MOCK_DECL(rv, funcname, arglist)     \
  rv funcname ##__real arglist;              \
  extern rv(*funcname) arglist
#define MOCK_IMPL(rv, funcname, arglist)     \
  rv(*funcname) arglist = funcname ##__real; \
  rv funcname ##__real arglist
#define MOCK(func, replacement)                 \
  do {                                          \
    (func) = (replacement);                     \
  } while (0)
#define UNMOCK(func)                            \
  do {                                          \
    func = func ##__real;                       \
  } while (0)
#else
#define MOCK_DECL(rv, funcname, arglist) \
  rv funcname arglist
#define MOCK_IMPL(rv, funcname, arglist) \
  rv funcname arglist
#endif
/** @} */

#endif

