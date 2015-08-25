AC_PREREQ(2.52)

# Process this file with autoconf to produce a configure script.
AC_INIT([json-c], 0.12, [json-c@googlegroups.com])

AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE

AC_PROG_MAKE_SET

AC_ARG_ENABLE(rdrand,
 AS_HELP_STRING([--enable-rdrand],
   [Enable RDRAND Hardware RNG Hash Seed generation on supported x86/x64 platforms.]),
[if test x$enableval = xyes; then
  enable_rdrand=yes
  AC_DEFINE(ENABLE_RDRAND, 1, [Enable RDRANR Hardware RNG Hash Seed])
fi])

if test "x$enable_rdrand" = "xyes"; then
  AC_MSG_RESULT([RDRAND Hardware RNG Hash Seed enabled on supported x86/x64 platforms])
else
  AC_MSG_RESULT([RDRAND Hardware RNG Hash Seed disabled. Use --enable-rdrand to enable])
fi

# Checks for programs.

# Checks for libraries.

# Checks for header files.
AM_PROG_CC_C_O
AC_CONFIG_HEADER(config.h)
AC_CONFIG_HEADER(json_config.h)
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h limits.h strings.h syslog.h unistd.h [sys/cdefs.h] [sys/param.h] stdarg.h locale.h endian.h)
AC_CHECK_HEADER(inttypes.h,[AC_DEFINE([JSON_C_HAVE_INTTYPES_H],[1],[Public define for json_inttypes.h])])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_VPRINTF
AC_FUNC_MEMCMP
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS(strcasecmp strdup strerror snprintf vsnprintf vasprintf open vsyslog strncasecmp setlocale)
AC_CHECK_DECLS([INFINITY], [], [], [[#include <math.h>]])
AC_CHECK_DECLS([nan], [], [], [[#include <math.h>]])
AC_CHECK_DECLS([isnan], [], [], [[#include <math.h>]])
AC_CHECK_DECLS([isinf], [], [], [[#include <math.h>]])
AC_CHECK_DECLS([_isnan], [], [], [[#include <float.h>]])
AC_CHECK_DECLS([_finite], [], [], [[#include <float.h>]])

#check if .section.gnu.warning accepts long strings (for __warn_references)
AC_LANG_PUSH([C])

AC_MSG_CHECKING([if .gnu.warning accepts long strings])
AC_LINK_IFELSE([AC_LANG_SOURCE([[
extern void json_object_get();
__asm__(".section .gnu.json_object_get,\n\t.ascii \"Please link against libjson-c instead of libjson\"\n\t.text");

int main(int c,char* v) {return 0;}
]])], [
    AC_DEFINE(HAS_GNU_WARNING_LONG, 1, [Define if .gnu.warning accepts long strings.])
    AC_MSG_RESULT(yes)
], [
   AC_MSG_RESULT(no)
])

AC_LANG_POP([C])

AM_PROG_LIBTOOL

# Check for the -Bsymbolic-functions linker flag
AC_ARG_ENABLE([Bsymbolic],
              [AS_HELP_STRING([--disable-Bsymbolic], [Avoid linking with -Bsymbolic-function])],
              [],
              [enable_Bsymbolic=check])

AS_IF([test "x$enable_Bsymbolic" = "xcheck"],
      [
        saved_LDFLAGS="${LDFLAGS}"
        AC_MSG_CHECKING([for -Bsymbolic-functions linker flag])
        LDFLAGS=-Wl,-Bsymbolic-functions
        AC_TRY_LINK([], [int main (void) { return 0; }],
                    [
                      AC_MSG_RESULT([yes])
                      enable_Bsymbolic=yes
                    ],
                    [
                      AC_MSG_RESULT([no])
                      enable_Bsymbolic=no
                    ])
        LDFLAGS="${saved_LDFLAGS}"
      ])

AS_IF([test "x$enable_Bsymbolic" = "xyes"], [JSON_BSYMBOLIC_LDFLAGS=-Wl[,]-Bsymbolic-functions])
AC_SUBST(JSON_BSYMBOLIC_LDFLAGS)

AC_CONFIG_FILES([
Makefile
json-c.pc
tests/Makefile
json-c-uninstalled.pc
])

AC_OUTPUT

