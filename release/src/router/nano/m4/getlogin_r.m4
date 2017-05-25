#serial 12

# Copyright (C) 2005-2007, 2009-2017 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

dnl From Derek Price
dnl
dnl Provide getlogin_r when the system lacks it.
dnl

AC_DEFUN([gl_FUNC_GETLOGIN_R],
[
  AC_REQUIRE([gl_UNISTD_H_DEFAULTS])

  dnl Persuade glibc <unistd.h> to declare getlogin_r().
  dnl Persuade Solaris <unistd.h> to provide the POSIX compliant declaration of
  dnl getlogin_r().
  AC_REQUIRE([gl_USE_SYSTEM_EXTENSIONS])

  AC_CHECK_DECLS_ONCE([getlogin_r])
  if test $ac_cv_have_decl_getlogin_r = no; then
    HAVE_DECL_GETLOGIN_R=0
  fi

  AC_CHECK_FUNCS_ONCE([getlogin_r])
  if test $ac_cv_func_getlogin_r = no; then
    HAVE_GETLOGIN_R=0
  else
    HAVE_GETLOGIN_R=1
    dnl On Mac OS X 10.12 and OSF/1 5.1, getlogin_r returns a truncated result
    dnl if the buffer is not large enough.
    AC_REQUIRE([AC_CANONICAL_HOST])
    AC_CACHE_CHECK([whether getlogin_r works with small buffers],
      [gl_cv_func_getlogin_r_works],
      [
        dnl Initial guess, used when cross-compiling.
changequote(,)dnl
        case "$host_os" in
                          # Guess no on Mac OS X, OSF/1.
          darwin* | osf*) gl_cv_func_getlogin_r_works="guessing no" ;;
                          # Guess yes otherwise.
          *)              gl_cv_func_getlogin_r_works="guessing yes" ;;
        esac
changequote([,])dnl
        AC_RUN_IFELSE(
          [AC_LANG_SOURCE([[
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#if !HAVE_DECL_GETLOGIN_R
extern
# ifdef __cplusplus
"C"
# endif
int getlogin_r (char *, size_t);
#endif
int
main (void)
{
  int result = 0;
  char buf[100];

  if (getlogin_r (buf, 0) == 0)
    result |= 1;
  if (getlogin_r (buf, 1) == 0)
    result |= 2;
  if (getlogin_r (buf, 100) == 0)
    {
      size_t n = strlen (buf);
      if (getlogin_r (buf, n) == 0)
        result |= 4;
    }
  return result;
}]])],
          [gl_cv_func_getlogin_r_works=yes],
          [gl_cv_func_getlogin_r_works=no],
          [:])
      ])
    case "$gl_cv_func_getlogin_r_works" in
      *yes) ;;
      *) REPLACE_GETLOGIN_R=1 ;;
    esac
  fi
])

AC_DEFUN([gl_PREREQ_GETLOGIN_R],
[
  AC_CHECK_DECLS_ONCE([getlogin])
])
