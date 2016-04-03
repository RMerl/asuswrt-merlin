dnl Process this file with autoconf to create configure.

AC_INIT(libnfnetlink, 1.0.1)
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([-Wall foreign subdir-objects
	tar-pax no-dist-gzip dist-bzip2 1.6])

dnl kernel style compile messages
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_PROG_CC
AM_PROG_CC_C_O
AC_DISABLE_STATIC
AM_PROG_LIBTOOL

case "$host" in 
*-*-linux*) ;;
*) AC_MSG_ERROR([Linux only, dude!]);;
esac


dnl--------------------------------
dnl--------------------------------


dnl Output the makefile
AC_CONFIG_FILES([Makefile src/Makefile include/Makefile
	include/libnfnetlink/Makefile utils/Makefile libnfnetlink.pc])
AC_OUTPUT
