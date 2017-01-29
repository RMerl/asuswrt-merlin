dnl Process this file with autoconf to produce a configure script.
AC_INIT(ethtool, 4.8, netdev@vger.kernel.org)
AC_PREREQ(2.52)
AC_CONFIG_SRCDIR([ethtool.c])
AM_INIT_AUTOMAKE([gnu])
AC_CONFIG_HEADERS([ethtool-config.h])

AM_MAINTAINER_MODE

dnl Checks for programs.
AC_PROG_CC
AC_PROG_GCC_TRADITIONAL
AM_PROG_CC_C_O

dnl Checks for libraries.

dnl Checks for header files.

dnl Checks for typedefs, structures, and compiler characteristics.
AC_MSG_CHECKING([whether <linux/types.h> defines big-endian types])
AC_TRY_COMPILE([#include <linux/types.h>],
	       [__be16 foo;__be32 bar;],
	       [AC_MSG_RESULT(yes)
		AC_DEFINE([HAVE_BE_TYPES], [1],
			  [Define to 1 if <linux/types.h> defines big-endian types])],
	       [AC_MSG_RESULT(no)])

dnl Checks for library functions.
AC_HEADER_STDC
AC_CHECK_FUNCS(socket strtol)

dnl Check for options
AC_ARG_ENABLE(pretty-dump,
	      [  --enable-pretty-dump	  enable registers, EEPROM and SFP pretty dumps (enabled by default)],
	      ,
	      enable_pretty_dump=yes)
if test x$enable_pretty_dump = xyes; then
    AC_DEFINE(ETHTOOL_ENABLE_PRETTY_DUMP, 1,
	      [Define this to enable register, EEPROM and SFP pretty dumps.])
fi
AM_CONDITIONAL([ETHTOOL_ENABLE_PRETTY_DUMP], [test x$enable_pretty_dump = xyes])

AC_CONFIG_FILES([Makefile ethtool.spec ethtool.8])
AC_OUTPUT
