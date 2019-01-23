dnl Check whether limits.h has needed features.

dnl Copyright 2016-2017 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Paul Eggert.

AC_DEFUN_ONCE([gl_LIMITS_H],
[
  gl_CHECK_NEXT_HEADERS([limits.h])

  AC_CACHE_CHECK([whether limits.h has ULLONG_WIDTH etc.],
    [gl_cv_header_limits_width],
    [AC_COMPILE_IFELSE(
       [AC_LANG_PROGRAM([[#ifndef __STDC_WANT_IEC_60559_BFP_EXT__
                           #define __STDC_WANT_IEC_60559_BFP_EXT__ 1
                          #endif
                          #include <limits.h>
                          int ullw = ULLONG_WIDTH;]])],
       [gl_cv_header_limits_width=yes],
       [gl_cv_header_limits_width=no])])
  if test "$gl_cv_header_limits_width" = yes; then
    LIMITS_H=
  else
    LIMITS_H=limits.h
  fi
  AC_SUBST([LIMITS_H])
  AM_CONDITIONAL([GL_GENERATE_LIMITS_H], [test -n "$LIMITS_H"])
])
