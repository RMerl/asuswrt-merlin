AC_PREREQ(2.50)
AC_INIT(replace.c)
AC_CONFIG_SRCDIR([replace.c])
AC_CONFIG_HEADER(config.h)

CFLAGS="$CFLAGS -I$srcdir"

AC_LIBREPLACE_ALL_CHECKS
AC_LIBREPLACE_NETWORK_CHECKS

if test "$ac_cv_prog_gcc" = yes; then
   CFLAGS="$CFLAGS -Wall"
   CFLAGS="$CFLAGS -W"
   CFLAGS="$CFLAGS -Wshadow"
   CFLAGS="$CFLAGS -Wstrict-prototypes"
   CFLAGS="$CFLAGS -Wpointer-arith"
   CFLAGS="$CFLAGS -Wcast-qual"
   CFLAGS="$CFLAGS -Wcast-align"
   CFLAGS="$CFLAGS -Wwrite-strings"
   CFLAGS="$CFLAGS -Werror-implicit-function-declaration"
   CFLAGS="$CFLAGS -Wformat=2"
   CFLAGS="$CFLAGS -Wno-format-y2k"
fi

m4_include(build_macros.m4)
BUILD_WITH_SHARED_BUILD_DIR

LIBS="${LIBREPLACE_NETWORK_LIBS}"
AC_SUBST(LIBS)

AC_SUBST(LDFLAGS)

AC_OUTPUT(Makefile)
