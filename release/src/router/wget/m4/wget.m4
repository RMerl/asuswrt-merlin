dnl Wget-specific Autoconf macros.
dnl Copyright (C) 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003,
dnl 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Free Software
dnl Foundation, Inc.

dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 3 of the License, or
dnl (at your option) any later version.

dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.

dnl You should have received a copy of the GNU General Public License
dnl along with this program.  If not, see <http://www.gnu.org/licenses/>.

dnl Additional permission under GNU GPL version 3 section 7

dnl If you modify this program, or any covered work, by linking or
dnl combining it with the OpenSSL project's OpenSSL library (or a
dnl modified version of that library), containing parts covered by the
dnl terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
dnl grants you additional permission to convey the resulting work.
dnl Corresponding Source for a non-source form of such a combination
dnl shall include the source code for the parts of OpenSSL used as well
dnl as that of the covered work.

dnl ************************************************************
dnl START OF IPv6 AUTOCONFIGURATION SUPPORT MACROS
dnl ************************************************************

AC_DEFUN([TYPE_STRUCT_SOCKADDR_IN6],[
  wget_have_sockaddr_in6=
  AC_CHECK_TYPES([struct sockaddr_in6],[
    wget_have_sockaddr_in6=yes
  ],[
    wget_have_sockaddr_in6=no
  ],[
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif
  ])

  if test "X$wget_have_sockaddr_in6" = "Xyes"; then :
    $1
  else :
    $2
  fi
])


AC_DEFUN([MEMBER_SIN6_SCOPE_ID],[
  AC_REQUIRE([TYPE_STRUCT_SOCKADDR_IN6])

  wget_member_sin6_scope_id=
  if test "X$wget_have_sockaddr_in6" = "Xyes"; then
    AC_CHECK_MEMBER([struct sockaddr_in6.sin6_scope_id],[
      wget_member_sin6_scope_id=yes
    ],[
      wget_member_sin6_scope_id=no
    ],[
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif
    ])
  fi

  if test "X$wget_member_sin6_scope_id" = "Xyes"; then
    AC_DEFINE([HAVE_SOCKADDR_IN6_SCOPE_ID], 1,
      [Define if struct sockaddr_in6 has the sin6_scope_id member])
    $1
  else :
    $2
  fi
])


AC_DEFUN([PROTO_INET6],[
  AC_CACHE_CHECK([for INET6 protocol support], [wget_cv_proto_inet6],[
    AC_TRY_CPP([
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif
#ifndef PF_INET6
#error Missing PF_INET6
#endif
#ifndef AF_INET6
#error Missing AF_INET6
#endif
    ],[
      wget_cv_proto_inet6=yes
    ],[
      wget_cv_proto_inet6=no
    ])
  ])

  if test "X$wget_cv_proto_inet6" = "Xyes"; then :
    $1
  else :
    $2
  fi
])


AC_DEFUN([WGET_STRUCT_SOCKADDR_STORAGE],[
  AC_CHECK_TYPES([struct sockaddr_storage],[], [], [
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
])
])

dnl ************************************************************
dnl END OF IPv6 AUTOCONFIGURATION SUPPORT MACROS
dnl ************************************************************

dnl AM_PATH_PROG_WITH_TEST(VARIABLE, PROG-TO-CHECK-FOR,
dnl   TEST-PERFORMED-ON-FOUND_PROGRAM [, VALUE-IF-NOT-FOUND [, PATH]])
AC_DEFUN([AM_PATH_PROG_WITH_TEST],
[# Extract the first word of "$2", so it can be a program name with args.
set dummy $2; ac_word=[$]2
AC_MSG_CHECKING([for $ac_word])
AC_CACHE_VAL(ac_cv_path_$1,
[case "[$]$1" in
  /*)
  ac_cv_path_$1="[$]$1" # Let the user override the test with a path.
  ;;
  *)
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}:"
  for ac_dir in ifelse([$5], , $PATH, [$5]); do
    test -z "$ac_dir" && ac_dir=.
    if test -f $ac_dir/$ac_word; then
      if [$3]; then
	ac_cv_path_$1="$ac_dir/$ac_word"
	break
      fi
    fi
  done
  IFS="$ac_save_ifs"
dnl If no 4th arg is given, leave the cache variable unset,
dnl so AC_PATH_PROGS will keep looking.
ifelse([$4], , , [  test -z "[$]ac_cv_path_$1" && ac_cv_path_$1="$4"
])dnl
  ;;
esac])dnl
$1="$ac_cv_path_$1"
if test -n "[$]$1"; then
  AC_MSG_RESULT([$]$1)
else
  AC_MSG_RESULT(no)
fi
AC_SUBST($1)dnl
])

