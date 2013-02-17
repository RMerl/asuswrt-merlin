dnl Checks for TI-RPC library and headers
dnl
AC_DEFUN([AC_LIBTIRPC], [

  AC_ARG_WITH([tirpcinclude],
              [AC_HELP_STRING([--with-tirpcinclude=DIR],
                              [use TI-RPC headers in DIR])],
              [tirpc_header_dir=$withval],
              [tirpc_header_dir=/usr/include/tirpc])

  dnl if --enable-tirpc was specifed, the following components
  dnl must be present, and we set up HAVE_ macros for them.

  if test "$enable_tirpc" = yes; then

    dnl look for the library; add to LIBS if found
    AC_CHECK_LIB([tirpc], [clnt_tli_create], ,
                 [AC_MSG_ERROR([libtirpc not found.])])

    dnl also must have the headers installed where we expect
    dnl look for headers; add -I compiler option if found
    AC_CHECK_HEADERS([${tirpc_header_dir}/netconfig.h], ,
                     [AC_MSG_ERROR([libtirpc headers not found.])])
    AC_SUBST([AM_CPPFLAGS], ["-I${tirpc_header_dir}"])

  fi

])dnl
