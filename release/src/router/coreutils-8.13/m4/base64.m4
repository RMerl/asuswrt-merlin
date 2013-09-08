# base64.m4 serial 3
dnl Copyright (C) 2004, 2006, 2009-2011 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_FUNC_BASE64],
[
  gl_PREREQ_BASE64
])

# Prerequisites of lib/base64.c.
AC_DEFUN([gl_PREREQ_BASE64], [
  AC_REQUIRE([AC_C_INLINE])
  AC_REQUIRE([AC_C_RESTRICT])
])
