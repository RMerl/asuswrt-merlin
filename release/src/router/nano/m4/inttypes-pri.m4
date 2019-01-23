# inttypes-pri.m4 serial 7 (gettext-0.18.2)
dnl Copyright (C) 1997-2002, 2006, 2008-2013 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.

AC_PREREQ([2.53])

# Define PRI_MACROS_BROKEN if <inttypes.h> exists and defines the PRI*
# macros to non-string values.  This is the case on AIX 4.3.3.

AC_DEFUN([gt_INTTYPES_PRI],
[
  AC_CHECK_HEADERS([inttypes.h])
  if test $ac_cv_header_inttypes_h = yes; then
    AC_CACHE_CHECK([whether the inttypes.h PRIxNN macros are broken],
      [gt_cv_inttypes_pri_broken],
      [
        AC_COMPILE_IFELSE(
          [AC_LANG_PROGRAM(
             [[
#include <inttypes.h>
#ifdef PRId32
char *p = PRId32;
#endif
             ]],
             [[]])],
          [gt_cv_inttypes_pri_broken=no],
          [gt_cv_inttypes_pri_broken=yes])
      ])
  fi
  if test "$gt_cv_inttypes_pri_broken" = yes; then
    AC_DEFINE_UNQUOTED([PRI_MACROS_BROKEN], [1],
      [Define if <inttypes.h> exists and defines unusable PRI* macros.])
    PRI_MACROS_BROKEN=1
  else
    PRI_MACROS_BROKEN=0
  fi
  AC_SUBST([PRI_MACROS_BROKEN])
])
