# utime_h.m4 serial 1
dnl Copyright (C) 2017 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.

AC_DEFUN([gl_UTIME_H],
[
  AC_REQUIRE([AC_CANONICAL_HOST])
  AC_REQUIRE([gl_UTIME_H_DEFAULTS])
  AC_CHECK_HEADERS_ONCE([utime.h])
  gl_CHECK_NEXT_HEADERS([utime.h])

  if test $ac_cv_header_utime_h = yes; then
    HAVE_UTIME_H=1
  else
    HAVE_UTIME_H=0
  fi
  AC_SUBST([HAVE_UTIME_H])

  UTIME_H=''
  if test $ac_cv_header_utime_h != yes; then
    dnl Provide a substitute <utime.h> file.
    UTIME_H=utime.h
  else
    case "$host_os" in
      mingw*) dnl Need special handling of 'struct utimbuf'.
        UTIME_H=utime.h
        ;;
    esac
  fi
  AC_SUBST([UTIME_H])
  AM_CONDITIONAL([GL_GENERATE_UTIME_H], [test -n "$UTIME_H"])

  dnl Check for declarations of anything we want to poison if the
  dnl corresponding gnulib module is not in use.
  gl_WARN_ON_USE_PREPARE([[#include <utime.h>
    ]],
    [utime])
])

AC_DEFUN([gl_UTIME_MODULE_INDICATOR],
[
  dnl Use AC_REQUIRE here, so that the default settings are expanded once only.
  AC_REQUIRE([gl_UTIME_H_DEFAULTS])
  gl_MODULE_INDICATOR_SET_VARIABLE([$1])
  dnl Define it also as a C macro, for the benefit of the unit tests.
  gl_MODULE_INDICATOR_FOR_TESTS([$1])
])

AC_DEFUN([gl_UTIME_H_DEFAULTS],
[
  GNULIB_UTIME=0;            AC_SUBST([GNULIB_UTIME])
  dnl Assume POSIX behavior unless another module says otherwise.
  HAVE_UTIME=1;              AC_SUBST([HAVE_UTIME])
  REPLACE_UTIME=0;           AC_SUBST([REPLACE_UTIME])
])
