dnl @synopsis GP_PKG_CONFIG
dnl
dnl If you want to set the PKG_CONFIG_PATH, best do so before
dnl calling GP_PKG_CONFIG
AC_DEFUN([GP_PKG_CONFIG],[
#
# [GP_PKG_CONFIG]
#
AC_ARG_VAR([PKG_CONFIG],[pkg-config package config utility])
export PKG_CONFIG
AC_ARG_VAR([PKG_CONFIG_PATH],[directory where pkg-config looks for *.pc files])
export PKG_CONFIG_PATH

AC_MSG_CHECKING([PKG_CONFIG_PATH])
if test "x${PKG_CONFIG_PATH}" = "x"; then
	AC_MSG_RESULT([empty])
else
	AC_MSG_RESULT([${PKG_CONFIG_PATH}])
fi

dnl AC_REQUIRE([PKG_CHECK_MODULES])
AC_PATH_PROG([PKG_CONFIG],[pkg-config],[false])
if test "$PKG_CONFIG" = "false"; then
AC_MSG_ERROR([
*** Build requires pkg-config
***
*** Possible solutions:
***   - set PKG_CONFIG to where your pkg-config is located
***   - set PATH to include the directory where pkg-config is installed
***   - get it from http://freedesktop.org/software/pkgconfig/ and install it
])
fi
])dnl

dnl Please do not remove this:
dnl filetype: d87b877b-80ec-447c-b042-21ec4a27c6f0
dnl I use this to find all the different instances of this file which 
dnl are supposed to be synchronized.

dnl Local Variables:
dnl mode: autoconf
dnl End:
