# localtime-buffer.m4 serial 1
dnl Copyright (C) 2017 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_LOCALTIME_BUFFER_DEFAULTS],
[
  NEED_LOCALTIME_BUFFER=0
])

dnl Macro invoked from other modules, to signal that the compilation of
dnl module 'localtime-buffer' is needed.
AC_DEFUN([gl_LOCALTIME_BUFFER_NEEDED],
[
  AC_REQUIRE([gl_LOCALTIME_BUFFER_DEFAULTS])
  AC_REQUIRE([gl_HEADER_TIME_H_DEFAULTS])
  NEED_LOCALTIME_BUFFER=1
  REPLACE_GMTIME=1
  REPLACE_LOCALTIME=1
])
