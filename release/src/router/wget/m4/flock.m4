# flock.m4 serial 3
dnl Copyright (C) 2008-2017 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_FUNC_FLOCK],
[
  AC_REQUIRE([gl_HEADER_SYS_FILE_H_DEFAULTS])
  AC_CHECK_FUNCS_ONCE([flock])
  if test $ac_cv_func_flock = no; then
    HAVE_FLOCK=0
  fi
])

dnl Prerequisites of lib/flock.c.
AC_DEFUN([gl_PREREQ_FLOCK],
[
  AC_CHECK_FUNCS_ONCE([fcntl])
  AC_CHECK_HEADERS_ONCE([unistd.h])

  dnl Do we have a POSIX fcntl lock implementation?
  AC_CHECK_MEMBERS([struct flock.l_type],[],[],[[#include <fcntl.h>]])
])
