# serial 1
dnl Copyright (C) 2005, 2009-2011 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_EUIDACCESS_STAT],
[
  AC_LIBSOURCES([euidaccess-stat.c, euidaccess-stat.h])
  AC_LIBOBJ([euidaccess-stat])
])
