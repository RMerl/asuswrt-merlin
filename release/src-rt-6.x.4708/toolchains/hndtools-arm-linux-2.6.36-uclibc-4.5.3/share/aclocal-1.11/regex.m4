## --------------------------------- ##                     -*- Autoconf -*-
## Check if --with-regex was given.  ##
## --------------------------------- ##
# Copyright (C) 1996, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005
# Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# serial 7

# AM_WITH_REGEX
# -------------
#
# The idea is to distribute rx.[hc] and regex.[hc] together, for a
# while.  The WITH_REGEX symbol is used to decide which of regex.h or
# rx.h should be included in the application.  If `./configure
# --with-regex' is given (the default), the package will use gawk's
# regex.  If `./configure --without-regex', a check is made to see if
# rx is already installed, as with newer Linux'es.  If not found, the
# package will use the rx from the distribution.  If found, the
# package will use the system's rx which, on Linux at least, will
# result in a smaller executable file.
#
# FIXME: This macro seems quite obsolete now since rx doesn't seem to
# be maintained, while regex is.
AC_DEFUN([AM_WITH_REGEX],
[AC_PREREQ(2.50)dnl
AC_LIBSOURCES([rx.h, rx.c, regex.c, regex.h])dnl
AC_MSG_CHECKING([which of GNU rx or gawk's regex is wanted])
AC_ARG_WITH([regex],
[  --without-regex         use GNU rx in lieu of gawk's regex for matching],
	    [test "$withval" = yes && am_with_regex=1],
	    [am_with_regex=1])
if test -n "$am_with_regex"; then
  AC_MSG_RESULT([regex])
  AC_DEFINE([WITH_REGEX], 1, [Define if using GNU regex])
  AC_CACHE_CHECK([for GNU regex in libc], [am_cv_gnu_regex],
    [AC_TRY_LINK([],
		 [extern int re_max_failures; re_max_failures = 1],
		 [am_cv_gnu_regex=yes],
		 [am_cv_gnu_regex=no])])
  if test $am_cv_gnu_regex = no; then
    AC_LIBOBJ([regex])
  fi
else
  AC_MSG_RESULT([rx])
  AC_CHECK_FUNC([re_rx_search], , [AC_LIBOBJ([rx])])
fi[]dnl
])

AU_DEFUN([fp_WITH_REGEX], [AM_WITH_REGEX])
