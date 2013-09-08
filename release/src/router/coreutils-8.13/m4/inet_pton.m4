# inet_pton.m4 serial 13
dnl Copyright (C) 2006, 2008-2011 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_FUNC_INET_PTON],
[
  dnl Persuade Solaris <arpa/inet.h> to declare inet_pton.
  AC_REQUIRE([gl_USE_SYSTEM_EXTENSIONS])

  dnl Most platforms that provide inet_pton define it in libc.
  dnl Solaris 8..10 provide inet_pton in libnsl instead.
  HAVE_INET_PTON=1
  gl_save_LIBS=$LIBS
  AC_SEARCH_LIBS([inet_pton], [nsl], [],
    [AC_CHECK_FUNCS([inet_pton])
     if test $ac_cv_func_inet_pton = no; then
       HAVE_INET_PTON=0
     fi
    ])
  LIBS=$gl_save_LIBS

  INET_PTON_LIB=
  if test "$ac_cv_search_inet_pton" != "no" &&
     test "$ac_cv_search_inet_pton" != "none required"; then
    INET_PTON_LIB="$ac_cv_search_inet_pton"
  fi
  AC_SUBST([INET_PTON_LIB])

  AC_CHECK_HEADERS_ONCE([netdb.h])
  AC_CHECK_DECLS([inet_pton],,,
    [#include <arpa/inet.h>
     #if HAVE_NETDB_H
     # include <netdb.h>
     #endif
    ])
  if test $ac_cv_have_decl_inet_pton = no; then
    HAVE_DECL_INET_PTON=0
    AC_REQUIRE([AC_C_RESTRICT])
  fi
])

# Prerequisites of lib/inet_pton.c.
AC_DEFUN([gl_PREREQ_INET_PTON], [
  AC_REQUIRE([gl_SOCKET_FAMILIES])
])
