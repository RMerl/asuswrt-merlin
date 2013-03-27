dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.59)dnl
AC_INIT(Makefile.in)
AC_CONFIG_HEADER(config.h:config.in)

sinclude(../common/aclocal.m4)

# Bugs in autoconf 2.59 break the call to SIM_AC_COMMON, hack around
# it by inlining the macro's contents.
sinclude(../common/common.m4)

SIM_AC_OPTION_ENDIAN(LITTLE_ENDIAN)
SIM_AC_OPTION_ALIGNMENT(,NONSTRICT_ALIGNMENT)
SIM_AC_OPTION_HOSTENDIAN
SIM_AC_OPTION_WARNINGS
SIM_AC_OPTION_RESERVED_BITS
SIM_AC_OPTION_BITSIZE(32,31)

AC_CHECK_FUNCS(time chmod utime fork execve execv chown)
AC_CHECK_HEADERS(unistd.h stdlib.h string.h strings.h utime.h time.h)

SIM_AC_OUTPUT
