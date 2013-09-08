# locale_h.m4 serial 14
dnl Copyright (C) 2007, 2009-2011 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_LOCALE_H],
[
  dnl Use AC_REQUIRE here, so that the default behavior below is expanded
  dnl once only, before all statements that occur in other macros.
  AC_REQUIRE([gl_LOCALE_H_DEFAULTS])

  dnl Persuade glibc <locale.h> to define locale_t.
  AC_REQUIRE([gl_USE_SYSTEM_EXTENSIONS])

  dnl If <stddef.h> is replaced, then <locale.h> must also be replaced.
  AC_REQUIRE([gl_STDDEF_H])

  AC_CACHE_CHECK([whether locale.h conforms to POSIX:2001],
    [gl_cv_header_locale_h_posix2001],
    [AC_COMPILE_IFELSE(
       [AC_LANG_PROGRAM(
          [[#include <locale.h>
            int x = LC_MESSAGES;]],
          [[]])],
       [gl_cv_header_locale_h_posix2001=yes],
       [gl_cv_header_locale_h_posix2001=no])])

  dnl Check for <xlocale.h>.
  AC_CHECK_HEADERS_ONCE([xlocale.h])
  if test $ac_cv_header_xlocale_h = yes; then
    HAVE_XLOCALE_H=1
    dnl Check whether use of locale_t requires inclusion of <xlocale.h>,
    dnl e.g. on MacOS X 10.5. If <locale.h> does not define locale_t by
    dnl itself, we assume that <xlocale.h> will do so.
    AC_CACHE_CHECK([whether locale.h defines locale_t],
      [gl_cv_header_locale_has_locale_t],
      [AC_COMPILE_IFELSE(
         [AC_LANG_PROGRAM(
            [[#include <locale.h>
              locale_t x;]],
            [[]])],
         [gl_cv_header_locale_has_locale_t=yes],
         [gl_cv_header_locale_has_locale_t=no])
      ])
    if test $gl_cv_header_locale_has_locale_t = yes; then
      gl_cv_header_locale_h_needs_xlocale_h=no
    else
      gl_cv_header_locale_h_needs_xlocale_h=yes
    fi
  else
    HAVE_XLOCALE_H=0
    gl_cv_header_locale_h_needs_xlocale_h=no
  fi
  AC_SUBST([HAVE_XLOCALE_H])

  dnl <locale.h> is always overridden, because of GNULIB_POSIXCHECK.
  gl_NEXT_HEADERS([locale.h])

  dnl Check for declarations of anything we want to poison if the
  dnl corresponding gnulib module is not in use.
  gl_WARN_ON_USE_PREPARE([[#include <locale.h>
/* Some systems provide declarations in a non-standard header.  */
#if HAVE_XLOCALE_H
# include <xlocale.h>
#endif
    ]],
    [setlocale duplocale])
])

AC_DEFUN([gl_LOCALE_MODULE_INDICATOR],
[
  dnl Use AC_REQUIRE here, so that the default settings are expanded once only.
  AC_REQUIRE([gl_LOCALE_H_DEFAULTS])
  gl_MODULE_INDICATOR_SET_VARIABLE([$1])
  dnl Define it also as a C macro, for the benefit of the unit tests.
  gl_MODULE_INDICATOR_FOR_TESTS([$1])
])

AC_DEFUN([gl_LOCALE_H_DEFAULTS],
[
  GNULIB_SETLOCALE=0;  AC_SUBST([GNULIB_SETLOCALE])
  GNULIB_DUPLOCALE=0;  AC_SUBST([GNULIB_DUPLOCALE])
  dnl Assume proper GNU behavior unless another module says otherwise.
  HAVE_DUPLOCALE=1;    AC_SUBST([HAVE_DUPLOCALE])
  REPLACE_SETLOCALE=0; AC_SUBST([REPLACE_SETLOCALE])
  REPLACE_DUPLOCALE=0; AC_SUBST([REPLACE_DUPLOCALE])
])
