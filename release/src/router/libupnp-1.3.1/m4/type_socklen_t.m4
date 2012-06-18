dnl @synopsis TYPE_SOCKLEN_T
dnl
dnl Check whether sys/socket.h defines type socklen_t. Please note that
dnl some systems require sys/types.h to be included before sys/socket.h
dnl can be compiled.
dnl
dnl @category Misc
dnl @author Lars Brinkhoff <lars@nocrew.org>
dnl @version 2005-01-11
dnl @license GPLWithACException

AC_DEFUN([TYPE_SOCKLEN_T],
[AC_CACHE_CHECK([for socklen_t], ac_cv_type_socklen_t,
[
  AC_TRY_COMPILE(
  [#include <sys/types.h>
   #include <sys/socket.h>],
  [socklen_t len = 42; return 0;],
  ac_cv_type_socklen_t=yes,
  ac_cv_type_socklen_t=no)
])
  if test $ac_cv_type_socklen_t != yes; then
    AC_DEFINE(socklen_t, int, [Substitute for socklen_t])
  fi
])
