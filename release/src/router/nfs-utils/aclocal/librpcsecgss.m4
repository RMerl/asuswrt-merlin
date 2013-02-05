dnl Checks for rpcsecgss library and headers
dnl KRB5LIBS must be set before this function is invoked.
dnl
AC_DEFUN([AC_LIBRPCSECGSS], [

  dnl libtirpc provides an rpcsecgss API
  if test "$enable_tirpc" = no; then

    dnl Check for library, but do not add -lrpcsecgss to LIBS
    AC_CHECK_LIB([rpcsecgss], [authgss_create_default], [librpcsecgss=1],
                 [AC_MSG_ERROR([librpcsecgss not found.])])

    AC_CHECK_LIB([rpcsecgss], [authgss_set_debug_level],
                 [AC_DEFINE([HAVE_AUTHGSS_SET_DEBUG_LEVEL], 1,
                 [Define to 1 if you have the `authgss_set_debug_level' function.])])

  fi

])dnl
