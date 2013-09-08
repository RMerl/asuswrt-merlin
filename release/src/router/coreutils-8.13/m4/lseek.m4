# lseek.m4 serial 7
dnl Copyright (C) 2007, 2009-2011 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_FUNC_LSEEK],
[
  AC_REQUIRE([gl_UNISTD_H_DEFAULTS])
  AC_REQUIRE([AC_PROG_CC])
  AC_CACHE_CHECK([whether lseek detects pipes], [gl_cv_func_lseek_pipe],
    [if test $cross_compiling = no; then
       AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h> /* for off_t */
#include <stdio.h> /* for SEEK_CUR */
#include <unistd.h>]], [[
  /* Exit with success only if stdin is seekable.  */
  return lseek (0, (off_t)0, SEEK_CUR) < 0;
]])],
         [if test -s conftest$ac_exeext \
             && ./conftest$ac_exeext < conftest.$ac_ext \
             && test 1 = "`echo hi \
               | { ./conftest$ac_exeext; echo $?; cat >/dev/null; }`"; then
            gl_cv_func_lseek_pipe=yes
          else
            gl_cv_func_lseek_pipe=no
          fi],
         [gl_cv_func_lseek_pipe=no])
     else
       AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#if ((defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__) || defined __BEOS__
/* mingw and BeOS mistakenly return 0 when trying to seek on pipes.  */
  Choke me.
#endif]])],
         [gl_cv_func_lseek_pipe=yes], [gl_cv_func_lseek_pipe=no])
     fi])
  if test $gl_cv_func_lseek_pipe = no; then
    REPLACE_LSEEK=1
    AC_DEFINE([LSEEK_PIPE_BROKEN], [1],
      [Define to 1 if lseek does not detect pipes.])
  fi
])
