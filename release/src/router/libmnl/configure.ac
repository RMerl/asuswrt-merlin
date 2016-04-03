dnl Process this file with autoconf to create configure.

AC_INIT([libmnl], [1.0.3])
AC_CONFIG_AUX_DIR([build-aux])
AC_CANONICAL_HOST
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign tar-pax no-dist-gzip dist-bzip2 1.6 subdir-objects])

dnl kernel style compile messages
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_PROG_CC
AM_PROG_CC_C_O
AC_EXEEXT
AC_DISABLE_STATIC
LT_INIT
CHECK_GCC_FVISIBILITY
case "$host" in
*-*-linux*) ;;
*) AC_MSG_ERROR([Linux only, dude!]);;
esac

regular_CPPFLAGS="-D_FILE_OFFSET_BITS=64 -D_REENTRANT"
regular_CFLAGS="-Wall -Waggregate-return -Wmissing-declarations \
	-Wmissing-prototypes -Wshadow -Wstrict-prototypes \
	-Wformat=2 -pipe"
AC_SUBST([regular_CPPFLAGS])
AC_SUBST([regular_CFLAGS])
AC_CONFIG_FILES([Makefile src/Makefile include/Makefile include/libmnl/Makefile include/linux/Makefile examples/Makefile examples/genl/Makefile examples/netfilter/Makefile examples/rtnl/Makefile libmnl.pc doxygen.cfg])
AC_OUTPUT
