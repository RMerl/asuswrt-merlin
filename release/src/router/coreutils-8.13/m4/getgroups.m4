# serial 16

dnl From Jim Meyering.
dnl A wrapper around AC_FUNC_GETGROUPS.

# Copyright (C) 1996-1997, 1999-2004, 2008-2011 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_FUNC_GETGROUPS],
[
  AC_REQUIRE([AC_FUNC_GETGROUPS])
  AC_REQUIRE([AC_TYPE_GETGROUPS])
  AC_REQUIRE([gl_UNISTD_H_DEFAULTS])
  if test "$ac_cv_func_getgroups" != yes; then
    HAVE_GETGROUPS=0
  elif test "$ac_cv_func_getgroups_works.$ac_cv_type_getgroups" != yes.gid_t
  then
    REPLACE_GETGROUPS=1
    AC_DEFINE([GETGROUPS_ZERO_BUG], [1], [Define this to 1 if
      getgroups(0,NULL) does not return the number of groups.])
  else
    dnl Detect FreeBSD bug; POSIX requires getgroups(-1,ptr) to fail.
    AC_CACHE_CHECK([whether getgroups handles negative values],
      [gl_cv_func_getgroups_works],
      [AC_RUN_IFELSE([AC_LANG_PROGRAM([AC_INCLUDES_DEFAULT],
        [[int size = getgroups (0, 0);
          gid_t *list = malloc (size * sizeof *list);
          return getgroups (-1, list) != -1;]])],
        [gl_cv_func_getgroups_works=yes],
        [gl_cv_func_getgroups_works=no],
        [gl_cv_func_getgroups_works="guessing no"])])
    if test "$gl_cv_func_getgroups_works" != yes; then
      REPLACE_GETGROUPS=1
    fi
  fi
  test -n "$GETGROUPS_LIB" && LIBS="$GETGROUPS_LIB $LIBS"
])
