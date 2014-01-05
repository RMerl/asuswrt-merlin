# sockaddr.m4
# Copyright (C) 2003-2004 Remi Denis-Courmont
# <remi (at) remlab (dot) net>.
# This file (sockaddr.m4) is free software; unlimited permission to
# copy and/or distribute it , with or without modifications, as long
# as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

dnl SHOULD check <sys/socket.h>, <winsock2.h> before that
AC_DEFUN([RDC_STRUCT_SOCKADDR_LEN],
[AC_LANG_ASSERT(C)
AH_TEMPLATE(HAVE_SA_LEN, [Define to 1 if `struct sockaddr' has a `sa_len' member.])
AC_CACHE_CHECK([if struct sockaddr has a sa_len member],
rdc_cv_struct_sockaddr_len,
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
[#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#elif HAVE_WINSOCK2_H
# include <winsock2.h>
#endif]], [[struct sockaddr addr; addr.sa_len = 0;]])],
rdc_cv_struct_sockaddr_len=yes,
rdc_cv_struct_sockaddr_len=no)])
AS_IF([test $rdc_cv_struct_sockaddr_len = yes],
 [AC_DEFINE(HAVE_SA_LEN)])
]) 

dnl SHOULD check <sys/socket.h>, <winsock2.h> before that
AC_DEFUN([RDC_STRUCT_SOCKADDR_STORAGE],
[AC_LANG_ASSERT(C)
AH_TEMPLATE(sockaddr_storage, [Define to `sockaddr' if <sys/socket.h> does not define.])
AH_TEMPLATE(ss_family, [Define to `sa_family' if <sys/socket.h> does not define.])
AC_CACHE_CHECK([for struct sockaddr_storage in sys/socket.h],
rdc_cv_struct_sockaddr_storage,
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
[#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#elif HAVE_WINSOCK2_H
# include <winsock2.h>
#endif]], [[struct sockaddr_storage addr; addr.ss_family = 0;]])],
rdc_cv_struct_sockaddr_storage=yes,
rdc_cv_struct_sockaddr_storage=no)])
AS_IF([test $rdc_cv_struct_sockaddr_storage = no],
 [AC_DEFINE(sockaddr_storage, sockaddr)
  AC_DEFINE(ss_family, sa_family)])
])

dnl SHOULD check <sys/socket.h>, <winsock2.h> before that
AC_DEFUN([RDC_TYPE_SOCKLEN_T],
[AC_LANG_ASSERT(C)
AH_TEMPLATE(socklen_t, [Define to `int' if <sys/socket.h> does not define.])
AC_CACHE_CHECK([for socklen_t in sys/socket.h],
rdc_cv_type_socklen_t,
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
[#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#elif HAVE_WINSOCK2_H
# include <winsock2.h>
#endif]], [[socklen_t len; len = 0;]])],
rdc_cv_type_socklen_t=yes,
rdc_cv_type_socklen_t=no)])
AS_IF([test "$rdc_cv_type_socklen_t" = no],
 [AC_DEFINE(socklen_t, int)])
])

dnl SHOULD check <winsock2.h> before that
AC_DEFUN([RDC_FUNC_SOCKET],
[AC_LANG_ASSERT(C)
AC_SEARCH_LIBS(socket, [socket], rdc_cv_func_socket=yes,
dnl AC_SEARCH_LIBS(socket, [ws2_32]) does not work with Mingw32
[AC_CACHE_CHECK([for socket in -lws2_32], rdc_cv_func_socket_ws2_32,
[rdc_func_socket_save_LIBS=$LIBS
LIBS="-lws2_32 $LIBS"
AC_LINK_IFELSE([AC_LANG_PROGRAM([
[#if HAVE_WINSOCK2_H
# include <winsock2.h>
#endif]], [[socket(0, 0, 0);]])],
rdc_cv_func_socket_ws2_32=yes,
rdc_cv_func_socket_ws2_32=no)
LIBS=$rdv_func_socket_save_LIBS])
AS_IF([test "$rdc_cv_func_socket_ws2_32" = yes],
 [rdc_cv_func_socket=yes; LIBS="-lws2_32 $LIBS"])
])
AS_IF([test "$rdc_cv_func_socket" = yes], [$1], [$2])
])

dnl SHOULD check <sys/socket.h>, <netdb.h>, <winsock2.h> before that
dnl NOTE: does not check required library
dnl (-lresolv is NOT needed on Solaris, but -lws2_32 is required on Windows)
AC_DEFUN([RDC_FUNC_GETADDRINFO],
[AC_LANG_ASSERT(C)
AH_TEMPLATE(HAVE_GETADDRINFO,
  [Define to 1 if you have the `getaddrinfo' function.])
AH_TEMPLATE(HAVE_GETNAMEINFO,
  [Define to 1 if you have the `getnameinfo' function.])
AH_TEMPLATE(HAVE_GAI_STRERROR,
  [Define to 1 if you have the `gai_strerror' function.])

gai_support=yes

AC_CACHE_CHECK([for getaddrinfo], rdc_cv_func_getaddrinfo,
[AC_LINK_IFELSE([AC_LANG_PROGRAM([
[#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#elif HAVE_WINSOCK2_H
# include <winsock2.h>
# include <ws2tcpip.h>
#endif
#if HAVE_NETDB_H
# include <netdb.h>
#endif]], [[getaddrinfo(0,0,0,0);]])],
rdc_cv_func_getaddrinfo=yes,
rdc_cv_func_getaddrinfo=no)])
AS_IF([test $rdc_cv_func_getaddrinfo = yes],
  [AC_DEFINE(HAVE_GETADDRINFO)

dnl Cannot have getnameinfo if not getaddrinfo
AC_CACHE_CHECK([for getnameinfo], rdc_cv_func_getnameinfo,
[AC_LINK_IFELSE([AC_LANG_PROGRAM([
[#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#elif HAVE_WINSOCK2_H
# include <winsock2.h>
# include <ws2tcpip.h>
#endif
#if HAVE_NETDB_H
# include <netdb.h>
#endif]], [[getnameinfo(0,0,0,0,0,0,0);]])],
rdc_cv_func_getnameinfo=yes,
rdc_cv_func_getnameinfo=no)])
AS_IF([test $rdc_cv_func_getnameinfo = yes],
  [AC_DEFINE(HAVE_GETNAMEINFO)],
  [gai_support=no])

dnl Cannot have gai_strerror if not getaddrinfo
AC_CACHE_CHECK([for gai_strerror], rdc_cv_func_gai_strerror,
[AC_LINK_IFELSE([AC_LANG_PROGRAM([
[#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#elif HAVE_WINSOCK2_H
# include <winsock2.h>
# include <ws2tcpip.h>
#endif
#if HAVE_NETDB_H
# include <netdb.h>
#endif]], [[gai_strerror(0);]])],
rdc_cv_func_gai_strerror=yes,
rdc_cv_func_gai_strerror=no)])
AS_IF([test $rdc_cv_func_gai_strerror = yes],
  [AC_DEFINE(HAVE_GAI_STRERROR)],
  [gai_support=no])
])

AC_LIBSOURCES([getaddrinfo.h, getaddrinfo.c])dnl
AS_IF([test $gai_support = no],
 [AC_CHECK_HEADERS([arpa/inet.h netinet/in.h])
  AC_SEARCH_LIBS(gethostbyaddr, [resolv])
  AC_LIBOBJ(getaddrinfo)])
])

