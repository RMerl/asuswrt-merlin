# inet_ntop.m4 serial 19
dnl Copyright (C) 2005-2006, 2008-2017 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_FUNC_INET_NTOP],
[
  AC_REQUIRE([gl_ARPA_INET_H_DEFAULTS])

  dnl Persuade Solaris <arpa/inet.h> to declare inet_ntop.
  AC_REQUIRE([gl_USE_SYSTEM_EXTENSIONS])

  AC_REQUIRE([AC_C_RESTRICT])

  dnl Most platforms that provide inet_ntop define it in libc.
  dnl Solaris 8..10 provide inet_ntop in libnsl instead.
  dnl Solaris 2.6..7 provide inet_ntop in libresolv instead.
  dnl Native Windows provides it in -lws2_32 instead, with a declaration in
  dnl <ws2tcpip.h>, and it uses stdcall calling convention, not cdecl
  dnl (hence we cannot use AC_CHECK_FUNCS, AC_SEARCH_LIBS to find it).
  HAVE_INET_NTOP=1
  INET_NTOP_LIB=
  gl_PREREQ_SYS_H_WINSOCK2
  if test $HAVE_WINSOCK2_H = 1; then
    AC_CHECK_DECLS([inet_ntop],,, [[#include <ws2tcpip.h>]])
    if test $ac_cv_have_decl_inet_ntop = yes; then
      dnl It needs to be overridden, because the stdcall calling convention
      dnl is not compliant with POSIX.
      REPLACE_INET_NTOP=1
      INET_NTOP_LIB="-lws2_32"
    else
      HAVE_DECL_INET_NTOP=0
      HAVE_INET_NTOP=0
    fi
  else
    gl_save_LIBS=$LIBS
    AC_SEARCH_LIBS([inet_ntop], [nsl resolv], [],
      [AC_CHECK_FUNCS([inet_ntop])
       if test $ac_cv_func_inet_ntop = no; then
         HAVE_INET_NTOP=0
       fi
      ])
    LIBS=$gl_save_LIBS

    if test "$ac_cv_search_inet_ntop" != "no" \
       && test "$ac_cv_search_inet_ntop" != "none required"; then
      INET_NTOP_LIB="$ac_cv_search_inet_ntop"
    fi

    AC_CHECK_HEADERS_ONCE([netdb.h])
    AC_CHECK_DECLS([inet_ntop],,,
      [[#include <arpa/inet.h>
        #if HAVE_NETDB_H
        # include <netdb.h>
        #endif
      ]])
    if test $ac_cv_have_decl_inet_ntop = no; then
      HAVE_DECL_INET_NTOP=0
    fi
  fi
  AC_SUBST([INET_NTOP_LIB])
])

# Prerequisites of lib/inet_ntop.c.
AC_DEFUN([gl_PREREQ_INET_NTOP], [
  AC_REQUIRE([gl_SOCKET_FAMILIES])
])
