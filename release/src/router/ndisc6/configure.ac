dnl configure.ac - Configure script for ndisc6
dnl based on earlier configure.ac from tcpreen by the same author

dnl Process this file with GNU Autoconf to produce a configure script

dnl *************************************************************************
dnl *  Copyright © 2002-2011 Rémi Denis-Courmont.                           *
dnl *  This program is free software: you can redistribute and/or modify    *
dnl *  it under the terms of the GNU General Public License as published by *
dnl *  the Free Software Foundation, version 2 or 3 of the license.         *
dnl *                                                                       *
dnl *  This program is distributed in the hope that it will be useful,      *
dnl *  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
dnl *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
dnl *  GNU General Public License for more details.                         *
dnl *                                                                       *
dnl *  You should have received a copy of the GNU General Public License    *
dnl *  along with this program. If not, see <http://www.gnu.org/licenses/>. *
dnl *************************************************************************

AC_COPYRIGHT([Copyright (C) 2005-2011 Remi Denis-Courmont])
AC_INIT(ndisc6, 1.0.2, remi_no_bulk_mail@remlab.net)
AC_PREREQ(2.62)
INVOCATION="`echo "$0 $ac_configure_args" | sed -e 's/\\\/\\\\\\\/g'`"

AS_MESSAGE(checking system...)
AC_CONFIG_SRCDIR(configure.ac)
AC_CONFIG_AUX_DIR(admin)
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_HEADERS(config.h)

AC_DEFINE_UNQUOTED(PACKAGE_CONFIGURE_INVOCATION, "$INVOCATION",
                   [Define to the command line used to invoke the configure script.])
RDC_BUILD_HOSTNAME


# Checks for programs.
AS_MESSAGE([checking required programs...])
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_USE_SYSTEM_EXTENSIONS

AC_PROG_RANLIB

AC_PATH_PROG(PERL, perl, :)
AC_ARG_VAR(PERL, [Perl interpreter])

AS_IF([test "x${enable_silent_rules}" = "x"], [
  enable_silent_rules="yes"
])
AM_INIT_AUTOMAKE
AM_SILENT_RULES

# Checks for libraries.
AS_MESSAGE([checking required libraries...])

LIBRT=""
AC_CHECK_LIB([rt], clock_gettime, [LIBRT="-lrt"])
AC_SUBST(LIBRT)

AM_GNU_GETTEXT_VERSION([0.17])
AM_GNU_GETTEXT([external], [need-ngettext])

dnl AC_DEFINE_UNQUOTED(PACKAGE_BUILD, "$build",
dnl                    [Define to the canonical build-system name])
dnl AC_DEFINE_UNQUOTED(PACKAGE_HOST, "$host",
dnl                    [Define to the canonical host-system name])

# Checks for header files.
AS_MESSAGE([checking header files...])
AC_HEADER_ASSERT


# Checks for typedefs, structures, and compiler characteristics.
dnl AS_MESSAGE([checking target characteristics...])
dnl AC_C_BIGENDIAN
dnl RDC_STRUCT_SOCKADDR_LEN


# Checks for library functions.
AS_MESSAGE([checking library functions...])
RDC_REPLACE_FUNC_GETOPT_LONG
LIBS_save="$LIBS"
LIBS="$LIBRT $LIBS"
AC_CHECK_FUNCS([clock_nanosleep])
LIBS="$LIBS_save"
AC_REPLACE_FUNCS([fdatasync inet6_rth_add ppoll])

# Network stuff
RDC_FUNC_SOCKET
AC_SEARCH_LIBS(inet_ntop, [nsl])

# Checks for optionnal features
AS_MESSAGE([checking optional features...])
AC_MSG_CHECKING([if installed programs should be setuid])
AC_ARG_ENABLE(suid-install,
              [AS_HELP_STRING(--disable-suid-install,
                              [do not set the SetUID bit on installed programs])])
AS_IF([test "x${enable_suid_install}" != "xno"],
	[enable_suid_install=yes])
AM_CONDITIONAL(NOSUID, [test "${enable_suid_install}" = "no"])
AC_MSG_RESULT([${enable_suid_install}])


# Defines for <config.h>
AH_BOTTOM([
#include "compat/fixups.h"

#ifndef HAVE_CLOCK_NANOSLEEP
# define clock_nanosleep( c, f, d, r ) nanosleep( d, r )
#endif

#define _( str )		gettext (str)
#define N_( str )		gettext_noop (str)
])


# END
AS_MESSAGE(writing results...)
AC_CONFIG_FILES([Makefile
	m4/Makefile
	po/Makefile.in
	doc/Makefile
	compat/Makefile
	src/Makefile
	rdnssd/Makefile
])
AC_OUTPUT
