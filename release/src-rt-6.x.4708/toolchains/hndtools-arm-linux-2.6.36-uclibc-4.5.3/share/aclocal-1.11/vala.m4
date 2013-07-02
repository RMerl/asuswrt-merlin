# Autoconf support for the Vala compiler

# Copyright (C) 2008, 2009 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# serial 4

# Check whether the Vala compiler exists in `PATH'. If it is found, the
# variable VALAC is set. Optionally a minimum release number of the
# compiler can be requested.
#
# AM_PROG_VALAC([MINIMUM-VERSION])
# --------------------------------
AC_DEFUN([AM_PROG_VALAC],
[AC_PATH_PROG([VALAC], [valac], [])
 AS_IF([test -z "$VALAC"],
   [AC_MSG_WARN([No Vala compiler found.  You will not be able to compile .vala source files.])],
   [AS_IF([test -n "$1"],
      [AC_MSG_CHECKING([$VALAC is at least version $1])
       am__vala_version=`$VALAC --version | sed 's/Vala  *//'`
       AS_VERSION_COMPARE([$1], ["$am__vala_version"],
         [AC_MSG_RESULT([yes])],
         [AC_MSG_RESULT([yes])],
         [AC_MSG_RESULT([no])
          AC_MSG_ERROR([Vala $1 not found.])])])])
])
