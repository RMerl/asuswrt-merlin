m4_define([SUBUNIT_MAJOR_VERSION], [0])
m4_define([SUBUNIT_MINOR_VERSION], [0])
m4_define([SUBUNIT_MICRO_VERSION], [6])
m4_define([SUBUNIT_VERSION],
m4_defn([SUBUNIT_MAJOR_VERSION]).m4_defn([SUBUNIT_MINOR_VERSION]).m4_defn([SUBUNIT_MICRO_VERSION]))
AC_PREREQ([2.59])
AC_INIT([subunit], [SUBUNIT_VERSION], [subunit-dev@lists.launchpad.net])
AC_CONFIG_SRCDIR([c/lib/child.c])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_CONFIG_MACRO_DIR([m4])
[SUBUNIT_MAJOR_VERSION]=SUBUNIT_MAJOR_VERSION
[SUBUNIT_MINOR_VERSION]=SUBUNIT_MINOR_VERSION
[SUBUNIT_MICRO_VERSION]=SUBUNIT_MICRO_VERSION
[SUBUNIT_VERSION]=SUBUNIT_VERSION
AC_SUBST([SUBUNIT_MAJOR_VERSION])
AC_SUBST([SUBUNIT_MINOR_VERSION])
AC_SUBST([SUBUNIT_MICRO_VERSION])
AC_SUBST([SUBUNIT_VERSION])
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_LIBTOOL
AM_PATH_PYTHON

AS_IF([test "$GCC" = "yes"],
      [
  SUBUNIT_CFLAGS="-Wall -Werror -Wextra -Wstrict-prototypes "
  SUBUNIT_CFLAGS="$SUBUNIT_CFLAGS -Wmissing-prototypes -Wwrite-strings "
  SUBUNIT_CFLAGS="$SUBUNIT_CFLAGS -Wno-variadic-macros "
  SUBUNIT_CXXFLAGS="-Wall -Werror -Wextra -Wwrite-strings -Wno-variadic-macros"
      ])

AM_CFLAGS="$SUBUNIT_CFLAGS -I\$(top_srcdir)/c/include"
AM_CXXFLAGS="$SUBUNIT_CXXFLAGS -I\$(top_srcdir)/c/include"
AC_SUBST(AM_CFLAGS)
AC_SUBST(AM_CXXFLAGS)

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

AC_CHECK_SIZEOF(int, 4)
AC_CHECK_SIZEOF(short, 2)
AC_CHECK_SIZEOF(long, 4)

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC

# Easier memory management.
# C unit testing.
PKG_CHECK_MODULES([CHECK], [check >= 0.9.4])
# C++ unit testing.
PKG_CHECK_MODULES([CPPUNIT], [cppunit])

# Output files
AC_CONFIG_HEADERS([config.h])

AC_CONFIG_FILES([libsubunit.pc
		 libcppunit_subunit.pc
                 Makefile
		 perl/Makefile.PL
                 ])
AC_OUTPUT
