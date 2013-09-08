# freopen.m4 serial 3
dnl Copyright (C) 2007-2011 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_FUNC_FREOPEN],
[
  AC_REQUIRE([gl_STDIO_H_DEFAULTS])
  AC_REQUIRE([AC_CANONICAL_HOST])
  case "$host_os" in
    mingw* | pw*)
      REPLACE_FREOPEN=1
      ;;
  esac
])

# Prerequisites of lib/freopen.c.
AC_DEFUN([gl_PREREQ_FREOPEN],
[
  AC_REQUIRE([AC_C_INLINE])
])
