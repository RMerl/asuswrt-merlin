dnl This function is (C) 1997,98,99 Stephan Kulow (coolo@kde.org)
dnl Modifications (C) Joe Orton 1999,2000

AC_DEFUN([SOCKLEN_ARG_TYPE],[

dnl Check for the type of the third argument of getsockname
AC_MSG_CHECKING(for the third argument of getsockname)  
AC_CACHE_VAL(ac_cv_ksize_t,
[AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/socket.h>
],[
socklen_t a=0; 
getsockname(0,(struct sockaddr*)0, &a);
],
ac_cv_ksize_t=socklen_t,
ac_cv_ksize_t=)
if test -z "$ac_cv_ksize_t"; then
ac_safe_cflags="$CFLAGS"
if test "$GCC" = "yes"; then
  CFLAGS="-Werror $CFLAGS"
fi
AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/socket.h>
],[
int a=0; 
getsockname(0,(struct sockaddr*)0, &a);
],
ac_cv_ksize_t=int,
ac_cv_ksize_t=size_t)
CFLAGS="$ac_safe_cflags"
fi
])

if test -z "$ac_cv_ksize_t"; then
  ac_cv_ksize_t=int
fi

AC_MSG_RESULT($ac_cv_ksize_t)
AC_DEFINE_UNQUOTED(ksize_t, $ac_cv_ksize_t, [Define to be the type of the third argument to getsockname])

])