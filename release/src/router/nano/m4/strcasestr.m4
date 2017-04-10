# strcasestr.m4 serial 21
dnl Copyright (C) 2005, 2007-2017 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl Check that strcasestr is present and works.
AC_DEFUN([gl_FUNC_STRCASESTR_SIMPLE],
[
  AC_REQUIRE([gl_HEADER_STRING_H_DEFAULTS])

  dnl Persuade glibc <string.h> to declare strcasestr().
  AC_REQUIRE([AC_USE_SYSTEM_EXTENSIONS])

  AC_REQUIRE([gl_FUNC_MEMCHR])
  AC_CHECK_FUNCS([strcasestr])
  if test $ac_cv_func_strcasestr = no; then
    HAVE_STRCASESTR=0
  else
    if test "$gl_cv_func_memchr_works" != yes; then
      REPLACE_STRCASESTR=1
    else
      dnl Detect http://sourceware.org/bugzilla/show_bug.cgi?id=12092.
      AC_CACHE_CHECK([whether strcasestr works],
        [gl_cv_func_strcasestr_works_always],
        [AC_RUN_IFELSE([AC_LANG_PROGRAM([[
#include <string.h> /* for strcasestr */
#define P "_EF_BF_BD"
#define HAYSTACK "F_BD_CE_BD" P P P P "_C3_88_20" P P P "_C3_A7_20" P
#define NEEDLE P P P P P
]], [[return !!strcasestr (HAYSTACK, NEEDLE);
      ]])],
          [gl_cv_func_strcasestr_works_always=yes],
          [gl_cv_func_strcasestr_works_always=no],
          [dnl glibc 2.12 and cygwin 1.7.7 have a known bug.  uClibc is not
           dnl affected, since it uses different source code for strcasestr
           dnl than glibc.
           dnl Assume that it works on all other platforms, even if it is not
           dnl linear.
           AC_EGREP_CPP([Lucky user],
             [
#ifdef __GNU_LIBRARY__
 #include <features.h>
 #if ((__GLIBC__ == 2 && __GLIBC_MINOR__ > 12) || (__GLIBC__ > 2)) \
     || defined __UCLIBC__
  Lucky user
 #endif
#elif defined __CYGWIN__
 #include <cygwin/version.h>
 #if CYGWIN_VERSION_DLL_COMBINED > CYGWIN_VERSION_DLL_MAKE_COMBINED (1007, 7)
  Lucky user
 #endif
#else
  Lucky user
#endif
             ],
             [gl_cv_func_strcasestr_works_always="guessing yes"],
             [gl_cv_func_strcasestr_works_always="guessing no"])
          ])
        ])
      case "$gl_cv_func_strcasestr_works_always" in
        *yes) ;;
        *)
          REPLACE_STRCASESTR=1
          ;;
      esac
    fi
  fi
]) # gl_FUNC_STRCASESTR_SIMPLE

dnl Additionally, check that strcasestr is efficient.
AC_DEFUN([gl_FUNC_STRCASESTR],
[
  AC_REQUIRE([gl_FUNC_STRCASESTR_SIMPLE])
  if test $HAVE_STRCASESTR = 1 && test $REPLACE_STRCASESTR = 0; then
    AC_CACHE_CHECK([whether strcasestr works in linear time],
      [gl_cv_func_strcasestr_linear],
      [AC_RUN_IFELSE([AC_LANG_PROGRAM([[
#include <signal.h> /* for signal */
#include <string.h> /* for strcasestr */
#include <stdlib.h> /* for malloc */
#include <unistd.h> /* for alarm */
static void quit (int sig) { _exit (sig + 128); }
]], [[
    int result = 0;
    size_t m = 1000000;
    char *haystack = (char *) malloc (2 * m + 2);
    char *needle = (char *) malloc (m + 2);
    /* Failure to compile this test due to missing alarm is okay,
       since all such platforms (mingw) also lack strcasestr.  */
    signal (SIGALRM, quit);
    alarm (5);
    /* Check for quadratic performance.  */
    if (haystack && needle)
      {
        memset (haystack, 'A', 2 * m);
        haystack[2 * m] = 'B';
        haystack[2 * m + 1] = 0;
        memset (needle, 'A', m);
        needle[m] = 'B';
        needle[m + 1] = 0;
        if (!strcasestr (haystack, needle))
          result |= 1;
      }
    return result;
    ]])],
        [gl_cv_func_strcasestr_linear=yes], [gl_cv_func_strcasestr_linear=no],
        [dnl Only glibc > 2.12 and cygwin > 1.7.7 are known to have a
         dnl strcasestr that works in linear time.
         AC_EGREP_CPP([Lucky user],
           [
#include <features.h>
#ifdef __GNU_LIBRARY__
 #if ((__GLIBC__ == 2 && __GLIBC_MINOR__ > 12) || (__GLIBC__ > 2)) \
     && !defined __UCLIBC__
  Lucky user
 #endif
#endif
#ifdef __CYGWIN__
 #include <cygwin/version.h>
 #if CYGWIN_VERSION_DLL_COMBINED > CYGWIN_VERSION_DLL_MAKE_COMBINED (1007, 7)
  Lucky user
 #endif
#endif
           ],
           [gl_cv_func_strcasestr_linear="guessing yes"],
           [gl_cv_func_strcasestr_linear="guessing no"])
        ])
      ])
    case "$gl_cv_func_strcasestr_linear" in
      *yes) ;;
      *)
        REPLACE_STRCASESTR=1
        ;;
    esac
  fi
]) # gl_FUNC_STRCASESTR

# Prerequisites of lib/strcasestr.c.
AC_DEFUN([gl_PREREQ_STRCASESTR], [
  :
])
