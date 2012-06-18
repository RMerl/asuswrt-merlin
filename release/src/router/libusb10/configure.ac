AC_INIT([libusb], [1.0.8])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([libusb/core.c])
AC_CONFIG_MACRO_DIR([m4])
AM_CONFIG_HEADER([config.h])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_PREREQ([2.50])
AC_PROG_CC
AC_PROG_LIBTOOL
AC_C_INLINE
AM_PROG_CC_C_O
AC_DEFINE([_GNU_SOURCE], [], [Use GNU extensions])

AC_MSG_CHECKING([operating system])
case $host in
*-linux*)
	AC_DEFINE(OS_LINUX, [], [Linux backend])
	AC_SUBST(OS_LINUX)
	AC_MSG_RESULT([Linux])
	backend="linux"
	AC_CHECK_LIB(rt, clock_gettime)
	AM_LDFLAGS=""
	;;
*-darwin*)
	AC_DEFINE(OS_DARWIN, [], [Darwin backend])
	AC_SUBST(OS_DARWIN)
	AC_DEFINE(USBI_OS_HANDLES_TIMEOUT, [], [Backend handles timeout])
	AC_MSG_RESULT([Darwin/MacOS X])
	backend="darwin"
	AM_LDFLAGS="-Wl,-framework -Wl,IOKit -Wl,-framework -Wl,CoreFoundation -Wl,-prebind -no-undefined"
	;;
*)
	AC_MSG_ERROR([unsupported operating system])
esac

AM_CONDITIONAL([OS_LINUX], [test "x$backend" == "xlinux"])
AM_CONDITIONAL([OS_DARWIN], [test "x$backend" == "xdarwin"])

# Library versioning
lt_major="0"
lt_revision="0"
lt_age="0"
AC_SUBST(lt_major)
AC_SUBST(lt_revision)
AC_SUBST(lt_age)

# timerfd
AC_CHECK_HEADER([sys/timerfd.h], [timerfd_h=1], [timerfd_h=0])
AC_ARG_ENABLE([timerfd],
	[AS_HELP_STRING([--enable-timerfd],
		[use timerfd for timing (default auto)])],
	[use_timerfd=$enableval], [use_timerfd='auto'])

if test "x$use_timerfd" = "xyes" -a "x$timerfd_h" = "x0"; then
	AC_MSG_ERROR([timerfd header not available; glibc 2.9+ required])
fi

AC_CHECK_DECL([TFD_NONBLOCK], [tfd_hdr_ok=yes], [tfd_hdr_ok=no], [#include <sys/timerfd.h>])
if test "x$use_timerfd" = "xyes" -a "x$tfd_hdr_ok" = "xno"; then
	AC_MSG_ERROR([timerfd header not usable; glibc 2.9+ required])
fi

AC_MSG_CHECKING([whether to use timerfd for timing])
if test "x$use_timerfd" = "xno"; then
	AC_MSG_RESULT([no (disabled by user)])
else
	if test "x$timerfd_h" = "x1" -a "x$tfd_hdr_ok" = "xyes"; then
		AC_MSG_RESULT([yes])
		AC_DEFINE(USBI_TIMERFD_AVAILABLE, [], [timerfd headers available])
	else
		AC_MSG_RESULT([no (header not available)])
	fi
fi

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

# check for -fvisibility=hidden compiler support (GCC >= 3.4)
saved_cflags="$CFLAGS"
CFLAGS="$CFLAGS -fvisibility=hidden"
AC_COMPILE_IFELSE(AC_LANG_PROGRAM([]),
	[VISIBILITY_CFLAGS="-fvisibility=hidden"
	 AC_DEFINE([API_EXPORTED], [__attribute__((visibility("default")))], [Default visibility]) ],
	[ VISIBILITY_CFLAGS=""
	 AC_DEFINE([API_EXPORTED], [], [Default visibility]) ],
	])
CFLAGS="$saved_cflags"

# check for -Wno-pointer-sign compiler support (GCC >= 4)
saved_cflags="$CFLAGS"
CFLAGS="$CFLAGS -Wno-pointer-sign"
AC_COMPILE_IFELSE(AC_LANG_PROGRAM([]),
	nopointersign_cflags="-Wno-pointer-sign", nopointersign_cflags="")
CFLAGS="$saved_cflags"

AM_CFLAGS="-std=gnu99 $inline_cflags -Wall -Wundef -Wunused -Wstrict-prototypes -Werror-implicit-function-declaration $nopointersign_cflags -Wshadow"

AC_SUBST(VISIBILITY_CFLAGS)
AC_SUBST(AM_CFLAGS)
AC_SUBST(AM_LDFLAGS)

AC_CONFIG_FILES([libusb-1.0.pc] [Makefile] [libusb/Makefile] [examples/Makefile] [doc/Makefile] [doc/doxygen.cfg])
AC_OUTPUT

