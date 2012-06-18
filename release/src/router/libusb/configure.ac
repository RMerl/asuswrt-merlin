AC_INIT([libusb-compat], [0.1.3])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([libusb/core.c])
AM_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_PREREQ([2.50])
AC_PROG_CC
AC_PROG_LIBTOOL
AC_C_INLINE
AM_PROG_CC_C_O

# Library versioning
LT_MAJOR="8"
LT_REVISION="4"
LT_AGE="4"
AC_SUBST(LT_MAJOR)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

# The libusb-0.1 version we pretend to be
LIBUSB01_VERSION="0.1.12"
AC_SUBST(LIBUSB01_VERSION)

PKG_CHECK_MODULES([LIBUSB_1_0], libusb-1.0 >= 0.9.1)
AC_SUBST(LIBUSB_1_0_CFLAGS)
AC_SUBST(LIBUSB_1_0_LIBS)

# Message logging
AC_ARG_ENABLE([log], [AS_HELP_STRING([--disable-log], [disable all logging])],
	[log_enabled=$enableval],
	[log_enabled='yes'])
if test "x$log_enabled" != "xno"; then
	AC_DEFINE([ENABLE_LOGGING], 1, [Message logging])
fi

AC_ARG_ENABLE([debug-log], [AS_HELP_STRING([--enable-debug-log],
	[enable debug logging (default n)])],
	[debug_log_enabled=$enableval],
	[debug_log_enabled='no'])
if test "x$debug_log_enabled" != "xno"; then
	AC_DEFINE([ENABLE_DEBUG_LOGGING], 1, [Debug message logging])
fi

# Examples build
AC_ARG_ENABLE([examples-build], [AS_HELP_STRING([--enable-examples-build],
	[build example applications (default n)])],
	[build_examples=$enableval],
	[build_examples='no'])
AM_CONDITIONAL([BUILD_EXAMPLES], [test "x$build_examples" != "xno"])

# Restore gnu89 inline semantics on gcc 4.3 and newer
saved_cflags="$CFLAGS"
CFLAGS="$CFLAGS -fgnu89-inline"
AC_COMPILE_IFELSE(AC_LANG_PROGRAM([]), inline_cflags="-fgnu89-inline", inline_cflags="")
CFLAGS="$saved_cflags"

AC_DEFINE([API_EXPORTED], [__attribute__((visibility("default")))], [Default visibility])
AM_CFLAGS="-std=gnu99 $inline_cflags -Wall -Wundef -Wunused -Wstrict-prototypes -Werror-implicit-function-declaration -Wshadow"
AC_SUBST(AM_CFLAGS)

AC_CONFIG_FILES([libusb.pc] [libusb-config] [Makefile] [libusb/Makefile] [examples/Makefile])
AC_CONFIG_COMMANDS([default],[[chmod +x libusb-config]],[[]])

AC_OUTPUT

