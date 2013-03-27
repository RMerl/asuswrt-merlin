#                                                       -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# Copyright 2002, 2003, 2004, 2005
# Free Software Foundation, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

AC_PREREQ(2.59)
AC_INIT(gdb.base)

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

ACX_NONCANONICAL_TARGET

# Add HP-specific tests when appropriate.
case $target in
  hppa*-*-hpux*)
    AC_CONFIG_SUBDIRS(gdb.hp) ;;
esac

# With stabs.
AC_ARG_WITH(stabs,
[  --with-stabs            arrange to use stabs instead of host debug format],,
[# We enable stabs tests by default on selected targets.
case $target in
   powerpc-*-aix* \
 | rs6000-*-aix*  \
 | *-*-*bsd*      \
 | *-*-go32*      \
 | *-*-linux*     \
 | *-*-lynxos*    \
 | *-sun-*        \
 | hppa*-*-*      \
 | *-*-elf*       \
 )
    with_stabs=yes ;;
 *)
    with_stabs=no ;;
esac])

# Add stabs tests when appropriate.
if test $with_stabs = yes; then
   AC_CONFIG_SUBDIRS(gdb.stabs)
fi

# Enable gdbtk.
AC_ARG_ENABLE(gdbtk,
[  --enable-gtk            enable gdbtk graphical user interface (GUI)],,
  [if test -d $srcdir/gdb.gdbtk; then
    enable_gdbtk=yes
  else
    enable_gdbtk=no
  fi])
# We unconditionally disable gdbtk tests on selected platforms.
case $host_os in
  go32* | windows*)
    enable_gdbtk=no ;;
esac

# Add gdbtk tests when appropriate.
if test $enable_gdbtk = yes; then
   AC_CONFIG_SUBDIRS(gdb.gdbtk)
fi

# Enable shared libraries.
AC_ARG_ENABLE(shared,
[  --enable-shared         build shared libraries [deault=yes]],,
  enable_shared=yes)

# If we have shared libraries, try to set RPATH_ENVVAR reasonably,
# such that we can find the shared libraries in the build tree.
if test $enable_shared = no; then
  # The variable `RPATH_ENVVAR' itself is not likely to be used on any
  # platform.
  RPATH_ENVVAR=RPATH_ENVVAR
else
  # The variable `LD_LIBRARY_PATH' is used on most platforms.
  RPATH_ENVVAR=LD_LIBRARY_PATH
  # The following exceptions are taken from Libtool 1.4.3.
  case $host_os in
    aix*)
      if test $host_cpu != ia64; then
        RPATH_ENVVAR=LIBPATH
      fi ;;
    darwin* | rhapsody*)
      RPATH_ENVVAR=DYLD_LIBRARY_PATH ;;
    hpux*)
      RPATH_ENVVAR=SHLIB_PATH ;;
  esac
fi
AC_SUBST(RPATH_ENVVAR)

AC_CHECK_HEADERS(pthread.h)

AC_EXEEXT

AC_OUTPUT([Makefile \
  gdb.ada/Makefile \
  gdb.arch/Makefile gdb.asm/Makefile gdb.base/Makefile \
  gdb.cp/Makefile gdb.disasm/Makefile gdb.dwarf2/Makefile \
  gdb.fortran/Makefile gdb.server/Makefile \
  gdb.java/Makefile gdb.mi/Makefile \
  gdb.objc/Makefile gdb.threads/Makefile gdb.trace/Makefile \
  gdb.xml/Makefile])
