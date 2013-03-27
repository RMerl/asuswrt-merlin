dnl Process this file with autoconf to produce a configure script.
sinclude(../common/aclocal.m4)
AC_PREREQ(2.59)dnl
AC_INIT(table.h)

AC_PROG_INSTALL
AC_PROG_CC

SIM_AC_OPTION_WARNINGS

# Put a plausible default for CC_FOR_BUILD in Makefile.
if test "x$cross_compiling" = "xno" -a "x$host" != "xi386-windows"; then
  AR_FOR_BUILD='$(AR)'
  AR_FLAGS_FOR_BUILD='$(AR_FLAGS)'
  CC_FOR_BUILD='$(CC)'
  CFLAGS_FOR_BUILD='$(CFLAGS)'
  RANLIB_FOR_BUILD='$(RANLIB)'
  LIBIBERTY_LIB=../../libiberty/libiberty.a
else
  AR_FOR_BUILD=${AR_FOR_BUILD-ar}
  AR_FLAGS_FOR_BUILD=${AR_FLAGS_FOR_BUILD-rc}
  CC_FOR_BUILD=${CC_FOR_BUILD-gcc}
  CFLAGS_FOR_BUILD=${CFLAGS_FOR_BUILD-"-g"}
  RANLIB_FOR_BUILD=${RANLIB_FOR_BUILD-ranlib}
  LIBIBERTY_LIB=
fi


AC_CANONICAL_SYSTEM
AC_ARG_PROGRAM

. ${srcdir}/../../bfd/configure.host

AC_CONFIG_HEADER(config.h:config.in)

AC_CHECK_HEADERS(stdlib.h string.h strings.h sys/stat.h sys/types.h unistd.h)
AC_HEADER_DIRENT

AC_SUBST(AR_FOR_BUILD)
AC_SUBST(AR_FLAGS_FOR_BUILD)
AC_SUBST(CC_FOR_BUILD)
AC_SUBST(CFLAGS_FOR_BUILD)
AC_SUBST(RANLIB_FOR_BUILD)
AC_SUBST(LIBIBERTY_LIB)

AC_SUBST(AR)
AC_SUBST(CFLAGS)
AC_PROG_RANLIB

AC_OUTPUT(Makefile,
[case x$CONFIG_HEADERS in xconfig.h:config.in) echo > stamp-h ;; esac])
