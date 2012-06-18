dnl Process this file with autoconf to produce a configure script.
dnl LIC: GPL
AC_INIT(l2tp.h)

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_CHECK_TOOL([AR],[ar])

dnl Checks for libraries.
dnl Replace `main' with a function in -ldl:
AC_CHECK_LIB(dl, dlopen)

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h sys/ioctl.h sys/time.h syslog.h unistd.h)

dnl Check for libevent
AC_MSG_CHECKING([for libevent directory])
if test -d ../libevent ; then
   LIBEVENT=../libevent
elif test -d libevent ; then
   LIBEVENT=libevent
else
   AC_MSG_RESULT([not found])
   echo "Cannot find libevent library; exiting"
   exit 1
fi

AC_MSG_RESULT($LIBEVENT)

AC_SUBST(LIBEVENT)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME

dnl Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MEMCMP
AC_TYPE_SIGNAL
AC_FUNC_VPRINTF
AC_CHECK_FUNCS(gethostname gettimeofday socket strdup strerror strtol)

dnl If we own this copy of libevent, do its makefile; otherwise, not

if test -d libevent ; then
   if test -L libevent ; then
      OUTPUT="Makefile handlers/Makefile"
   else
      OUTPUT="Makefile libevent/Makefile handlers/Makefile"
   fi
else
   OUTPUT="Makefile handlers/Makefile"
fi

AC_OUTPUT($OUTPUT)
