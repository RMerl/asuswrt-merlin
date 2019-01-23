# utime.m4 serial 1
dnl Copyright (C) 2017 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_FUNC_UTIME],
[
  AC_REQUIRE([gl_UTIME_H_DEFAULTS])
  AC_REQUIRE([AC_CANONICAL_HOST])
  AC_CHECK_FUNCS_ONCE([utime])
  if test $ac_cv_func_utime = no; then
    HAVE_UTIME=0
  else
    case "$host_os" in
      mingw*)
        dnl On this platform, the original utime() or _utime() produces
        dnl timestamps that are affected by the time zone.
        REPLACE_UTIME=1
        ;;
    esac
  fi
])

# Prerequisites of lib/utime.c.
AC_DEFUN([gl_PREREQ_UTIME], [:])
