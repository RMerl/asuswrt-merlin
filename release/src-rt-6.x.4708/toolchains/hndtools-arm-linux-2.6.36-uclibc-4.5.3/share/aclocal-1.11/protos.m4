## ------------------------------- ##                       -*- Autoconf -*-
## Check for function prototypes.  ##
## From Franc,ois Pinard           ##
## ------------------------------- ##
# Copyright (C) 1996, 1997, 1998, 2000, 2001, 2002, 2003, 2005, 2006
# Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# serial 5

AC_DEFUN([AM_C_PROTOTYPES],
[AC_REQUIRE([AC_C_PROTOTYPES])
if test "$ac_cv_prog_cc_stdc" != no; then
  U= ANSI2KNR=
else
  U=_ ANSI2KNR=./ansi2knr
fi
# Ensure some checks needed by ansi2knr itself.
AC_REQUIRE([AC_HEADER_STDC])
AC_CHECK_HEADERS([string.h])
AC_SUBST([U])dnl
AC_SUBST([ANSI2KNR])dnl
_AM_SUBST_NOTMAKE([ANSI2KNR])dnl
])

AU_DEFUN([fp_C_PROTOTYPES], [AM_C_PROTOTYPES])
