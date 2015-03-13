# mkostemp.m4 serial 2
dnl Copyright (C) 2009-2014 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_FUNC_MKOSTEMP],
[
  AC_REQUIRE([gl_STDLIB_H_DEFAULTS])

  dnl Persuade glibc <stdlib.h> to declare mkostemp().
  AC_REQUIRE([AC_USE_SYSTEM_EXTENSIONS])

  AC_CHECK_FUNCS_ONCE([mkostemp])
  if test $ac_cv_func_mkostemp != yes; then
    HAVE_MKOSTEMP=0
  fi
])

# Prerequisites of lib/mkostemp.c.
AC_DEFUN([gl_PREREQ_MKOSTEMP],
[
])
