# -*- Autoconf -*-
# $Id$
#
# Top-level configure.ac file for libupnp
#
# Process this file with autoconf to produce a configure script.
#
# (C) Copyright 2005-2006 Rémi Turboult <r3mi@users.sourceforge.net>
#

AC_PREREQ(2.59)

AC_INIT([libupnp], [1.3.1], [r3mi@users.sourceforge.net])
# *Independently* of the above libupnp package version, the libtool version
# of the 3 libraries need to be updated whenever there is a change :
# "current:revision:age" (this is NOT the same as the package version), where:
# 	- library code modified:		revision++
# 	- interfaces changed/added/removed:	current++ and revision=0
# 	- interfaces added: 			age++
# 	- interfaces removed:			age=0
AC_SUBST([LT_VERSION_IXML],       [2:0:0])
AC_SUBST([LT_VERSION_THREADUTIL], [2:0:0])
AC_SUBST([LT_VERSION_UPNP],       [2:1:0])


AC_CONFIG_AUX_DIR(config.aux)
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_SRCDIR(upnp/inc/upnp.h)

AM_INIT_AUTOMAKE([1.9 -Wall foreign subdir-objects])

#
# There are 3 configuration files :
# 1) "./autoconfig.h" is auto-generated and used only internally during build
#    (usually named "config.h" but conflicts with the file below)
# 2) "./upnp/src/inc/config.h" is static and contains some compile-time 
#    parameters. This file was previously in "./upnp/inc" but is no longer 
#    installed (contains internal definitions only).
# 3) "./upnp/inc/upnpconfig.h" is auto-generated and installed with the 
#    libraries : it contains information on the configuration of the
#    installed libraries.
#
AC_CONFIG_HEADERS([autoconfig.h upnp/inc/upnpconfig.h])

AC_REVISION([$Revision: 1.8 $])

upnpmaj=`echo "$PACKAGE_VERSION" | sed 's/\..*//' `
upnpmin=[`echo "$PACKAGE_VERSION" | sed 's/^[^.]\.// ; s/[^0-9].*$//' `]
upnppatch=[`echo "$PACKAGE_VERSION" | sed 's/^[^.]\.[^.]\.// ; s/[^0-9].*$//' `]
AC_DEFINE_UNQUOTED([UPNP_VERSION_STRING], "$PACKAGE_VERSION",
	[see upnpconfig.h])
AC_DEFINE_UNQUOTED([UPNP_VERSION_MAJOR], $upnpmaj, [see upnpconfig.h])
AC_DEFINE_UNQUOTED([UPNP_VERSION_MINOR], $upnpmin, [see upnpconfig.h])
AC_DEFINE_UNQUOTED([UPNP_VERSION_PATCH], $upnppatch, [see upnpconfig.h])


#
# Check for DEBUG flag
#
RT_BOOL_ARG_ENABLE([debug], [no], [extra debugging code])
if test "x$enable_debug" = xyes ; then
	AC_DEFINE(UPNP_HAVE_DEBUG, 1, [see upnpconfig.h])
	AC_DEFINE(DEBUG, 1, [Define to 1 to compile debug code])
else
	AC_DEFINE(NO_DEBUG, 1, [Define to 1 to prevent some debug code])
	AC_DEFINE(NDEBUG, 1, [Define to 1 to prevent compilation of assert()])
fi


#
# Check for libupnp subsets
#

RT_BOOL_ARG_ENABLE([client], [yes], [control point code (client)])
if test "x$enable_client" = xyes ; then
	AC_DEFINE(UPNP_HAVE_CLIENT, 1, [see upnpconfig.h])
fi


RT_BOOL_ARG_ENABLE([device], [yes],
     [device specific code (implies --disable-webserver if disabled)])
if test "x$enable_device" = xyes ; then
	AC_DEFINE(UPNP_HAVE_DEVICE, 1, [see upnpconfig.h])
fi


if test "x$enable_device" = xno ; then 
	enable_webserver=no
else 
	RT_BOOL_ARG_ENABLE([webserver], [yes], [integrated web server])
fi
AM_CONDITIONAL(ENABLE_WEBSERVER, test x"$enable_webserver" = xyes)
if test "x$enable_webserver" = xyes ; then
	AC_DEFINE(UPNP_HAVE_WEBSERVER, 1, [see upnpconfig.h])
fi


RT_BOOL_ARG_ENABLE([tools], [yes], [helper APIs in upnptools.h])
if test "x$enable_tools" = xyes ; then
	AC_DEFINE(UPNP_HAVE_TOOLS, 1, [see upnpconfig.h])
fi



#
# Checks for programs
#
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LIBTOOL
AC_PROG_INSTALL
AC_PROG_MAKE_SET


#
# Default compilation flags
#
if test x"$enable_debug" = xyes; then
	# AC_PROG_CC already sets CFLAGS to "-g -O2" by default
	:
else
	# add optimise for size
	AX_CFLAGS_GCC_OPTION([-Os])
fi
AX_CFLAGS_WARN_ALL

# Arrange for large-file support (can be disabled with --disable-largefile).
# Define _FILE_OFFSET_BITS and _LARGE_FILES if necessary
AC_SYS_LARGEFILE


#
# Checks for header files
#
AC_HEADER_STDC
# libupnp code doesn't use autoconf variables yet,
# so just abort if a header file is not found.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h malloc.h netdb.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h sys/timeb.h syslog.h unistd.h],[],[AC_MSG_ERROR([required header file missing])])


#
# Checks for typedefs, structures, and compiler characteristics
#	
AC_C_CONST
AC_TYPE_SIZE_T
TYPE_SOCKLEN_T


#
# Checks for library functions
#
AC_FUNC_VPRINTF
AC_FUNC_FSEEKO


#
# Checks for POSIX Threads
#
ACX_PTHREAD([],[AC_MSG_ERROR([POSIX threads are required to build this program])])




AC_CONFIG_FILES([Makefile
		 ixml/Makefile
		 ixml/doc/Makefile
                 threadutil/Makefile
		 upnp/Makefile
		 upnp/doc/Makefile
		 libupnp.pc
                 ])

AC_OUTPUT

