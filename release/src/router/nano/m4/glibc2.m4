# glibc2.m4 serial 3
dnl Copyright (C) 2000-2002, 2004, 2008, 2010-2013 Free Software Foundation,
dnl Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

# Test for the GNU C Library, version 2.0 or newer.
# From Bruno Haible.

AC_DEFUN([gt_GLIBC2],
  [
    AC_CACHE_CHECK([whether we are using the GNU C Library 2 or newer],
      [ac_cv_gnu_library_2],
      [AC_EGREP_CPP([Lucky GNU user],
        [
#include <features.h>
#ifdef __GNU_LIBRARY__
 #if (__GLIBC__ >= 2) && !defined __UCLIBC__
  Lucky GNU user
 #endif
#endif
        ],
        [ac_cv_gnu_library_2=yes],
        [ac_cv_gnu_library_2=no])
      ]
    )
    AC_SUBST([GLIBC2])
    GLIBC2="$ac_cv_gnu_library_2"
  ]
)
