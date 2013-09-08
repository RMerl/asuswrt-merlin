# localename.m4 serial 2
dnl Copyright (C) 2007, 2009-2011 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_LOCALENAME],
[
  AC_REQUIRE([gt_LC_MESSAGES])
  AC_REQUIRE([gt_INTL_MACOSX])
  AC_CHECK_FUNCS([setlocale uselocale])
])
