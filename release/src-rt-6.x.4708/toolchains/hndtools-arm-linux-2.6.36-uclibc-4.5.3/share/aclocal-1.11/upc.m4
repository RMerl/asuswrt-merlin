# Find a compiler for Unified Parallel C.	            -*- Autoconf -*-

# Copyright (C) 2006  Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

AC_DEFUN([AM_PROG_UPC],
[dnl We need OBJEXT and EXEEXT, but Autoconf doesn't offer any public
dnl macro to compute them.  Use AC_PROG_CC instead.
AC_REQUIRE([AC_PROG_CC])dnl
AC_ARG_VAR([UPC], [Unified Parallel C compiler command])dnl
AC_ARG_VAR([UPCFLAGS], [Unified Parallel C compiler flags])dnl
AC_CHECK_TOOLS([UPC], [m4_default([$1], [upcc upc])], [:])
if test "$UPC" = :; then
  AC_MSG_ERROR([no Unified Parallel C compiler was found], [77])
fi
_AM_IF_OPTION([no-dependencies],, [_AM_DEPENDENCIES([UPC])])dnl
])
