dnl Checks for IPv6 support
dnl
AC_DEFUN([AC_IPV6], [

  AC_CHECK_DECL([AI_ADDRCONFIG],
                [AC_DEFINE([HAVE_DECL_AI_ADDRCONFIG], 1,
                           [Define this to 1 if AI_ADDRCONFIG macro is defined])], ,
                [ #include <netdb.h> ])

  if test "$enable_ipv6" = yes; then

    dnl TI-RPC required for IPv6
    if test "$enable_tirpc" = no; then
      AC_MSG_ERROR(['--enable-ipv6' requires '--enable-tirpc'.])
    fi

    dnl IPv6-enabled networking functions required for IPv6
    AC_CHECK_FUNCS([getnameinfo bindresvport_sa], ,
                   [AC_MSG_ERROR([Missing functions needed for IPv6.])])

    dnl Need to detect presence of IPv6 networking at run time via
    dnl getaddrinfo(3); old versions of glibc do not support ADDRCONFIG
    AC_CHECK_DECL([AI_ADDRCONFIG], ,
                  [AC_MSG_ERROR([full getaddrinfo(3) implementation needed for IPv6 support])],
                  [ #include <netdb.h> ])

  fi

])dnl
