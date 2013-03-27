dnl Autoconf configure script for GDB, the GNU debugger.
dnl Copyright (C) 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
dnl 2005, 2006
dnl Free Software Foundation, Inc.
dnl
dnl This file is part of GDB.
dnl 
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 3 of the License, or
dnl (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program.  If not, see <http://www.gnu.org/licenses/>.

dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)dnl
AC_INIT(main.c)
AC_CONFIG_HEADER(config.h:config.in)
AM_MAINTAINER_MODE

AC_PROG_CC
AC_GNU_SOURCE
AC_AIX
AC_ISC_POSIX
AM_PROG_CC_STDC

AC_CONFIG_AUX_DIR(`cd $srcdir;pwd`/..)
AC_CANONICAL_SYSTEM

dnl List of object files and targets accumulated by configure.

CONFIG_OBS=
CONFIG_DEPS=
CONFIG_SRCS=
ENABLE_CFLAGS=

CONFIG_ALL=
CONFIG_CLEAN=
CONFIG_INSTALL=
CONFIG_UNINSTALL=

dnl Set up for gettext.
ZW_GNU_GETTEXT_SISTER_DIR

localedir='${datadir}/locale'
AC_SUBST(localedir)

if test x"$USE_NLS" = xyes; then
   CONFIG_ALL="$CONFIG_ALL all-po"
   CONFIG_CLEAN="$CONFIG_CLEAN clean-po"		   
   CONFIG_INSTALL="$CONFIG_INSTALL install-po"
   CONFIG_UNINSTALL="$CONFIG_UNINSTALL uninstall-po"
fi

PACKAGE=gdb
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE", [Name of this package. ])
AC_SUBST(PACKAGE)

debugdir=${libdir}/debug
	 
AC_ARG_WITH(separate-debug-dir,
[  --with-separate-debug-dir=path   Look for global separate debug info in this path [LIBDIR/debug]],
[debugdir="${withval}"])
	
AC_DEFINE_DIR(DEBUGDIR, debugdir,
              [Global directory for separate debug files. ])
#AC_DEFINE_UNQUOTED(DEBUGDIR, "$debugdir"),

if test "x$exec_prefix" = xNONE || test "x$exec_prefix" = 'x${prefix}'; then
  if test "x$prefix" = xNONE; then
    test_prefix=/usr/local
  else
    test_prefix=$prefix
  fi
else
  test_prefix=$exec_prefix
fi
case ${debugdir} in
"${test_prefix}"|"${test_prefix}/"*|\
'${exec_prefix}'|'${exec_prefix}/'*)
  AC_DEFINE(DEBUGDIR_RELOCATABLE, 1, [Define if the debug directory should be relocated when GDB is moved.])
  ;;
esac

AC_CONFIG_SUBDIRS(doc testsuite)

# Provide defaults for some variables set by the per-host and per-target
# configuration.
gdb_host_obs=posix-hdep.o

if test "${target}" = "${host}"; then
  gdb_native=yes
else
  gdb_native=no
fi

. $srcdir/configure.host

. $srcdir/configure.tgt

# Fetch the default architecture and default target vector from BFD.
targ=$target; . $srcdir/../bfd/config.bfd

# We only want the first architecture, so strip off the others if
# there is more than one.
targ_archs=`echo $targ_archs | sed 's/ .*//'`

if test "x$targ_archs" != x; then
  AC_DEFINE_UNQUOTED(DEFAULT_BFD_ARCH, $targ_archs,
    [Define to BFD's default architecture. ])
fi
if test "x$targ_defvec" != x; then
  AC_DEFINE_UNQUOTED(DEFAULT_BFD_VEC, $targ_defvec,
    [Define to BFD's default target vector. ])
fi

AC_ARG_PROGRAM

# The CLI cannot be disabled yet, but may be in the future.

# Enable CLI.
AC_ARG_ENABLE(gdbcli,
[  --disable-gdbcli        disable command-line interface (CLI)],
  [case $enableval in
    yes)
      ;;
    no)
      AC_MSG_ERROR([the command-line interface cannot be disabled yet]) ;;
    *)
      AC_MSG_ERROR([bad value $enableval for --enable-gdbcli]) ;;
  esac],
  [enable_gdbcli=yes])
if test x"$enable_gdbcli" = xyes; then
  if test -d $srcdir/cli; then
    CONFIG_OBS="$CONFIG_OBS \$(SUBDIR_CLI_OBS)"
    CONFIG_DEPS="$CONFIG_DEPS \$(SUBDIR_CLI_DEPS)"
    CONFIG_SRCS="$CONFIG_SRCS \$(SUBDIR_CLI_SRCS)"
    ENABLE_CFLAGS="$ENABLE_CFLAGS \$(SUBDIR_CLI_CFLAGS)"
  fi
fi

# Enable MI.
AC_ARG_ENABLE(gdbmi,
[  --disable-gdbmi         disable machine-interface (MI)],
  [case $enableval in
    yes | no)
      ;;
    *)
      AC_MSG_ERROR([bad value $enableval for --enable-gdbmi]) ;;
  esac],
  [enable_gdbmi=yes])
if test x"$enable_gdbmi" = xyes; then
  if test -d $srcdir/mi; then
    CONFIG_OBS="$CONFIG_OBS \$(SUBDIR_MI_OBS)"
    CONFIG_DEPS="$CONFIG_DEPS \$(SUBDIR_MI_DEPS)"
    CONFIG_SRCS="$CONFIG_SRCS \$(SUBDIR_MI_SRCS)"
    ENABLE_CFLAGS="$ENABLE_CFLAGS \$(SUBDIR_MI_CFLAGS)"
  fi
fi

# Enable TUI.
AC_ARG_ENABLE(tui,
[  --enable-tui            enable full-screen terminal user interface (TUI)],
  [case $enableval in
    yes | no)
      ;;
    *)
      AC_MSG_ERROR([bad value $enableval for --enable-tui]) ;;
  esac],enable_tui=yes)

# Enable gdbtk.
AC_ARG_ENABLE(gdbtk,
[  --enable-gdbtk          enable gdbtk graphical user interface (GUI)],
  [case $enableval in
    yes | no)
      ;;
    *)
      AC_MSG_ERROR([bad value $enableval for --enable-gdbtk]) ;;
  esac],
  [if test -d $srcdir/gdbtk -a -d $srcdir/../itcl; then
    enable_gdbtk=yes
  else
    enable_gdbtk=no
  fi])
# We unconditionally disable gdbtk tests on selected platforms.
case $host_os in
  go32* | windows*)
    AC_MSG_WARN([gdbtk isn't supported on $host; disabling])
    enable_gdbtk=no ;;
esac

# Libunwind support.
AC_ARG_WITH(libunwind,
[  --with-libunwind            Use libunwind frame unwinding support],
[case "${withval}" in
  yes)  enable_libunwind=yes ;;
  no)   enable_libunwind=no ;;
  *)    AC_MSG_ERROR(bad value ${withval} for GDB with-libunwind option) ;;
esac],[
  AC_CHECK_HEADERS(libunwind.h)
  AC_CHECK_HEADERS(libunwind-ia64.h)
  if test x"$ac_cv_header_libunwind_h" = xyes -a x"$ac_cv_header_libunwind_ia64_h" = xyes; then
    enable_libunwind=yes;
  fi
])
   
if test x"$enable_libunwind" = xyes; then
  AC_CHECK_HEADERS(libunwind.h)
  AC_CHECK_HEADERS(libunwind-ia64.h)
  AC_DEFINE(HAVE_LIBUNWIND, 1, [Define if libunwind library is being used.])
  CONFIG_OBS="$CONFIG_OBS libunwind-frame.o"
  CONFIG_DEPS="$CONFIG_DEPS libunwind-frame.o"
  CONFIG_SRCS="$CONFIG_SRCS libunwind-frame.c"
fi

# Profiling support.
AC_ARG_ENABLE(profiling,
[  --enable-profiling      enable profiling of GDB],
  [case $enableval in
    yes | no)
      ;;
    *)
      AC_MSG_ERROR([bad value $enableval for --enable-profile]) ;;
  esac],
 [enable_profiling=no])

AC_CHECK_FUNCS(monstartup _mcleanup)
AC_CACHE_CHECK([for _etext], ac_cv_var__etext,
[AC_TRY_LINK(
[#include <stdlib.h>
extern char _etext;
],
[free (&_etext);], ac_cv_var__etext=yes, ac_cv_var__etext=no)])
if test $ac_cv_var__etext = yes; then
  AC_DEFINE(HAVE__ETEXT, 1,
            [Define to 1 if your system has the _etext variable. ])
fi
if test "$enable_profiling" = yes ; then
  if test $ac_cv_func_monstartup = no || test $ac_cv_func__mcleanup = no; then
    AC_MSG_ERROR(--enable-profiling requires monstartup and _mcleanup)
  fi
  PROFILE_CFLAGS=-pg
  OLD_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS $PROFILE_CFLAGS"

  AC_CACHE_CHECK([whether $CC supports -pg], ac_cv_cc_supports_pg,
    [AC_TRY_COMPILE([], [int x;], ac_cv_cc_supports_pg=yes,
     ac_cv_cc_supports_pg=no)])

  if test $ac_cv_cc_supports_pg = no; then
    AC_MSG_ERROR(--enable-profiling requires a compiler which supports -pg)
  fi

  CFLAGS="$OLD_CFLAGS"
fi

# --------------------- #
# Checks for programs.  #
# --------------------- #

AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_RANLIB
AC_PROG_YACC

AC_CHECK_TOOL(AR, ar)
AC_CHECK_TOOL(DLLTOOL, dlltool)
AC_CHECK_TOOL(WINDRES, windres)

# Needed for GNU/Hurd.
AC_CHECK_TOOL(MIG, mig)

# ---------------------- #
# Checks for libraries.  #
# ---------------------- #

# We might need to link with -lm; most simulators need it.
AC_CHECK_LIB(m, main)

# We need to link with -lw to get `wctype' on Solaris before Solaris
# 2.6.  Solaris 2.6 and beyond have this function in libc, and have a
# libw that some versions of the GNU linker cannot hanle (GNU ld 2.9.1
# is known to have this problem).  Therefore we avoid libw if we can.
AC_CHECK_FUNC(wctype, [],
  [AC_CHECK_LIB(w, wctype)])

# Some systems (e.g. Solaris) have `gethostbyname' in libnsl.
AC_SEARCH_LIBS(gethostbyname, nsl)

# Some systems (e.g. Solaris) have `socketpair' in libsocket.
AC_SEARCH_LIBS(socketpair, socket)

# For the TUI, we need enhanced curses functionality.
#
# FIXME: kettenis/20040905: We prefer ncurses over the vendor-supplied
# curses library because the latter might not provide all the
# functionality we need.  However, this leads to problems on systems
# where the linker searches /usr/local/lib, but the compiler doesn't
# search /usr/local/include, if ncurses is installed in /usr/local.  A
# default installation of ncurses on alpha*-dec-osf* will lead to such
# a situation.
AC_SEARCH_LIBS(waddstr, [ncurses cursesX curses])

# Since GDB uses Readline, we need termcap functionality.  In many
# cases this will be provided by the curses library, but some systems
# have a seperate termcap library, or no curses library at all.

case $host_os in
  cygwin*)
    if test -d $srcdir/libtermcap; then
      LIBS="../libtermcap/libtermcap.a $LIBS"
      ac_cv_search_tgetent="../libtermcap/libtermcap.a"
    fi ;;
  go32* | *djgpp*)
    ac_cv_search_tgetent="none required"
    ;;
  *mingw32*)	 
    ac_cv_search_tgetent="none required"
    CONFIG_OBS="$CONFIG_OBS win32-termcap.o"
    ;;
esac

# These are the libraries checked by Readline.
AC_SEARCH_LIBS(tgetent, [termcap tinfo curses ncurses])

if test "$ac_cv_search_tgetent" = no; then
  AC_MSG_ERROR([no termcap library found])
fi

AC_ARG_WITH([system-readline],
  [AS_HELP_STRING([--with-system-readline],
                  [use installed readline library])])

if test "$with_system_readline" = yes; then
  READLINE=-lreadline
  READLINE_DEPS=
  READLINE_CFLAGS=
else
  READLINE='$(READLINE_DIR)/libreadline.a'
  READLINE_DEPS='$(READLINE)'
  READLINE_CFLAGS='-I$(READLINE_SRC)/..'
fi
AC_SUBST(READLINE)
AC_SUBST(READLINE_DEPS)
AC_SUBST(READLINE_CFLAGS)

AC_ARG_WITH(expat,
  AS_HELP_STRING([--with-expat], [include expat support (auto/yes/no)]),
  [], [with_expat=auto])
AC_MSG_CHECKING([whether to use expat])
AC_MSG_RESULT([$with_expat])

if test "${with_expat}" = no; then
  AC_MSG_WARN([expat support disabled; some features may be unavailable.])
  HAVE_LIBEXPAT=no
else
  AC_LIB_HAVE_LINKFLAGS([expat], [], [#include "expat.h"],
			[XML_Parser p = XML_ParserCreate (0);])
  if test "$HAVE_LIBEXPAT" != yes; then
    if test "$with_expat" = yes; then
      AC_MSG_ERROR([expat is missing or unusable])
    else
      AC_MSG_WARN([expat is missing or unusable; some features may be unavailable.])
    fi
  else
    save_LIBS=$LIBS
    LIBS="$LIBS $LIBEXPAT"
    AC_CHECK_FUNCS(XML_StopParser)
    LIBS=$save_LIBS
  fi
fi

# ------------------------- #
# Checks for header files.  #
# ------------------------- #

AC_HEADER_DIRENT
AC_HEADER_STAT
AC_HEADER_STDC
AC_CHECK_HEADERS(nlist.h)
AC_CHECK_HEADERS(link.h, [], [],
[#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_NLIST_H
# include <nlist.h>
#endif
])
AC_CHECK_HEADERS(machine/reg.h)
AC_CHECK_HEADERS(poll.h sys/poll.h)
AC_CHECK_HEADERS(proc_service.h thread_db.h gnu/libc-version.h)
AC_CHECK_HEADERS(signal.h)
AC_CHECK_HEADERS(stddef.h)
AC_CHECK_HEADERS(stdlib.h)
AC_CHECK_HEADERS(stdint.h)
AC_CHECK_HEADERS(string.h memory.h strings.h)
AC_CHECK_HEADERS(sys/fault.h)
AC_CHECK_HEADERS(sys/file.h)
AC_CHECK_HEADERS(sys/filio.h)
AC_CHECK_HEADERS(sys/ioctl.h)
AC_CHECK_HEADERS(sys/param.h)
AC_CHECK_HEADERS(sys/resource.h)
AC_CHECK_HEADERS(sys/proc.h, [], [],
[#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
])
AC_CHECK_HEADERS(sys/procfs.h)
AC_CHECK_HEADERS(sys/ptrace.h ptrace.h)
AC_CHECK_HEADERS(sys/reg.h sys/debugreg.h)
AC_CHECK_HEADERS(sys/select.h)
AC_CHECK_HEADERS(sys/syscall.h)
AC_CHECK_HEADERS(sys/types.h)
AC_CHECK_HEADERS(sys/user.h, [], [],
[#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
])
AC_CHECK_HEADERS(sys/wait.h wait.h)
AC_CHECK_HEADERS(termios.h termio.h sgtty.h)
AC_CHECK_HEADERS(unistd.h)

# On Solaris 2.[789], we need to define _MSE_INT_H to avoid a clash
# between <widec.h> and <wchar.h> that would cause AC_CHECK_HEADERS to
# think that we don't have <curses.h> if we're using GCC.
case $host_os in
  solaris2.[[789]])
    if test "$GCC" = yes; then
      AC_DEFINE(_MSE_INT_H, 1,
        [Define to 1 to avoid a clash between <widec.h> and <wchar.h> on
   Solaris 2.[789] when using GCC. ])
    fi ;;
esac
AC_CHECK_HEADERS(curses.h cursesX.h ncurses.h ncurses/ncurses.h)
AC_CHECK_HEADERS(ncurses/term.h)
AC_CHECK_HEADERS(term.h, [], [],
[#if HAVE_CURSES_H
# include <curses.h>
#endif
])

# FIXME: kettenis/20030102: In most cases we include these
# unconditionally, so what's the point in checking these?
AC_CHECK_HEADERS(ctype.h time.h)

# Create a header we can use portably to get the standard integer types.
GCC_HEADER_STDINT(gdb_stdint.h)

# ------------------------- #
# Checks for declarations.  #
# ------------------------- #

AC_CHECK_DECLS([free, malloc, realloc])
AC_CHECK_DECLS([strerror, strstr])
AC_CHECK_DECLS([getopt, snprintf, vsnprintf])

# ----------------------- #
# Checks for structures.  #
# ----------------------- #

AC_CHECK_MEMBERS([struct stat.st_blocks])
AC_CHECK_MEMBERS([struct stat.st_blksize])

# ------------------ #
# Checks for types.  #
# ------------------ #

AC_TYPE_SIGNAL
AC_CHECK_TYPES(socklen_t, [], [],
[#include <sys/types.h>
#include <sys/socket.h>
])
AC_CHECK_TYPES(uintptr_t, [], [], [#include <stdint.h>])

# ------------------------------------- #
# Checks for compiler characteristics.  #
# ------------------------------------- #

AC_C_CONST
AC_C_INLINE

# ------------------------------ #
# Checks for library functions.  #
# ------------------------------ #

AC_FUNC_ALLOCA
AC_FUNC_MMAP
AC_FUNC_VFORK
AC_CHECK_FUNCS(canonicalize_file_name realpath)
AC_CHECK_FUNCS(getrusage)
AC_CHECK_FUNCS(getuid getgid)
AC_CHECK_FUNCS(poll)
AC_CHECK_FUNCS(pread64)
AC_CHECK_FUNCS(sbrk)
AC_CHECK_FUNCS(setpgid setpgrp)
AC_CHECK_FUNCS(sigaction sigprocmask sigsetmask)
AC_CHECK_FUNCS(socketpair)
AC_CHECK_FUNCS(syscall)
AC_CHECK_FUNCS(ttrace)
AC_CHECK_FUNCS(wborder)

# Check the return and argument types of ptrace.  No canned test for
# this, so roll our own.
gdb_ptrace_headers='
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_SYS_PTRACE_H
# include <sys/ptrace.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
'
# There is no point in checking if we don't have a prototype.
AC_CHECK_DECLS(ptrace, [], [
  : ${gdb_cv_func_ptrace_ret='int'}
  : ${gdb_cv_func_ptrace_args='int,int,long,long'}
], $gdb_ptrace_headers)
# Check return type.
AC_CACHE_CHECK([return type of ptrace], gdb_cv_func_ptrace_ret,
  AC_TRY_COMPILE($gdb_ptrace_headers,
    [extern int ptrace ();],
    gdb_cv_func_ptrace_ret='int',
    gdb_cv_func_ptrace_ret='long'))
AC_DEFINE_UNQUOTED(PTRACE_TYPE_RET, $gdb_cv_func_ptrace_ret,
  [Define as the return type of ptrace.])
# Check argument types.
AC_CACHE_CHECK([types of arguments for ptrace], gdb_cv_func_ptrace_args, [
for gdb_arg1 in 'int' 'long'; do
 for gdb_arg2 in 'pid_t' 'int' 'long'; do
  for gdb_arg3 in 'int *' 'caddr_t' 'int' 'long'; do
   for gdb_arg4 in 'int' 'long'; do
     AC_TRY_COMPILE($gdb_ptrace_headers, [
extern $gdb_cv_func_ptrace_ret
  ptrace ($gdb_arg1, $gdb_arg2, $gdb_arg3, $gdb_arg4);
], [gdb_cv_func_ptrace_args="$gdb_arg1,$gdb_arg2,$gdb_arg3,$gdb_arg4";
    break 4;])
    for gdb_arg5 in 'int *' 'int' 'long'; do
     AC_TRY_COMPILE($gdb_ptrace_headers, [
extern $gdb_cv_func_ptrace_ret
  ptrace ($gdb_arg1, $gdb_arg2, $gdb_arg3, $gdb_arg4, $gdb_arg5);
], [
gdb_cv_func_ptrace_args="$gdb_arg1,$gdb_arg2,$gdb_arg3,$gdb_arg4,$gdb_arg5";
    break 5;])
    done
   done
  done
 done
done
# Provide a safe default value.
: ${gdb_cv_func_ptrace_args='int,int,long,long'}
])
ac_save_IFS=$IFS; IFS=','
set dummy `echo "$gdb_cv_func_ptrace_args" | sed 's/\*/\*/g'`
IFS=$ac_save_IFS
shift
AC_DEFINE_UNQUOTED(PTRACE_TYPE_ARG3, $[3],
  [Define to the type of arg 3 for ptrace.])
if test -n "$[5]"; then
  AC_DEFINE_UNQUOTED(PTRACE_TYPE_ARG5, $[5],
    [Define to the type of arg 5 for ptrace.])
fi

dnl AC_FUNC_SETPGRP does not work when cross compiling
dnl Instead, assume we will have a prototype for setpgrp if cross compiling.
if test "$cross_compiling" = no; then
  AC_FUNC_SETPGRP
else
  AC_CACHE_CHECK([whether setpgrp takes no argument], ac_cv_func_setpgrp_void,
    [AC_TRY_COMPILE([
#include <unistd.h>
], [
  if (setpgrp(1,1) == -1)
    exit (0);
  else
    exit (1);
], ac_cv_func_setpgrp_void=no, ac_cv_func_setpgrp_void=yes)])
if test $ac_cv_func_setpgrp_void = yes; then
  AC_DEFINE(SETPGRP_VOID, 1)
fi
fi

# Check if sigsetjmp is available.  Using AC_CHECK_FUNCS won't do
# since sigsetjmp might only be defined as a macro.
AC_CACHE_CHECK([for sigsetjmp], gdb_cv_func_sigsetjmp,
[AC_TRY_COMPILE([
#include <setjmp.h>
], [sigjmp_buf env; while (! sigsetjmp (env, 1)) siglongjmp (env, 1);],
gdb_cv_func_sigsetjmp=yes, gdb_cv_func_sigsetjmp=no)])
if test $gdb_cv_func_sigsetjmp = yes; then
  AC_DEFINE(HAVE_SIGSETJMP, 1, [Define if sigsetjmp is available. ])
fi

# Assume we'll default to using the included libiberty regex.
gdb_use_included_regex=yes

# However, if the system regex is GNU regex, then default to *not*
# using the included regex.
AC_CACHE_CHECK(
  [for GNU regex],
  [gdb_cv_have_gnu_regex],
  [AC_TRY_COMPILE(
    [#include <gnu-versions.h>],
    [#define REGEX_INTERFACE_VERSION 1
#if _GNU_REGEX_INTERFACE_VERSION != REGEX_INTERFACE_VERSION
# error "Version mismatch"
#endif],
    gdb_cv_have_gnu_regex=yes,
    gdb_cv_have_gnu_regex=no)])
if test $gdb_cv_have_gnu_regex = yes; then
  gdb_use_included_regex=no
fi

AC_ARG_WITH(included-regex,
  [  --without-included-regex don't use included regex; this is the default
                          on systems with version 2 of the GNU C library
                          (use with caution on other system)],
  gdb_with_regex=$withval,
  gdb_with_regex=$gdb_use_included_regex)
if test "$gdb_with_regex" = yes; then
  AC_DEFINE(USE_INCLUDED_REGEX, 1,
    [Define to 1 if the regex included in libiberty should be used.])
fi

# Check if <sys/proc.h> defines `struct thread' with a td_pcb member.
AC_CHECK_MEMBERS([struct thread.td_pcb], [], [],
[#include <sys/param.h>
#include <sys/proc.h>
])

# See if <sys/lwp.h> defines `struct lwp`.
AC_CACHE_CHECK([for struct lwp], gdb_cv_struct_lwp,
[AC_TRY_COMPILE([#include <sys/param.h>
#include <sys/lwp.h>], [struct lwp l;],
gdb_cv_struct_lwp=yes, gdb_cv_struct_lwp=no)])
if test $gdb_cv_struct_lwp = yes; then
  AC_DEFINE(HAVE_STRUCT_LWP, 1,
            [Define to 1 if your system has struct lwp.])
fi

# See if <machine/reg.h> degines `struct reg'.
AC_CACHE_CHECK([for struct reg in machine/reg.h], gdb_cv_struct_reg,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <machine/reg.h>], [struct reg r;],
gdb_cv_struct_reg=yes, gdb_cv_struct_reg=no)])
if test $gdb_cv_struct_reg = yes; then
  AC_DEFINE(HAVE_STRUCT_REG, 1,
            [Define to 1 if your system has struct reg in <machine/reg.h>.])
fi

# See if <machine/reg.h> supports the %fs and %gs i386 segment registers.
# Older i386 BSD's don't have the r_fs and r_gs members of `struct reg'.
AC_CHECK_MEMBERS([struct reg.r_fs, struct reg.r_gs], [], [],
                 [#include <machine/reg.h>])

# See if <sys/ptrace.h> provides the PTRACE_GETREGS request.
AC_MSG_CHECKING(for PTRACE_GETREGS)
AC_CACHE_VAL(gdb_cv_have_ptrace_getregs,
[AC_TRY_COMPILE([#include <sys/ptrace.h>],
		[PTRACE_GETREGS;],
		[gdb_cv_have_ptrace_getregs=yes],
		[gdb_cv_have_ptrace_getregs=no])])
AC_MSG_RESULT($gdb_cv_have_ptrace_getregs)
if test $gdb_cv_have_ptrace_getregs = yes; then
  AC_DEFINE(HAVE_PTRACE_GETREGS, 1, 
  [Define if sys/ptrace.h defines the PTRACE_GETREGS request.])
fi

# See if <sys/ptrace.h> provides the PTRACE_GETFPXREGS request.
AC_MSG_CHECKING(for PTRACE_GETFPXREGS)
AC_CACHE_VAL(gdb_cv_have_ptrace_getfpxregs,
[AC_TRY_COMPILE([#include <sys/ptrace.h>],
		[PTRACE_GETFPXREGS;],
		[gdb_cv_have_ptrace_getfpxregs=yes],
		[gdb_cv_have_ptrace_getfpxregs=no])])
AC_MSG_RESULT($gdb_cv_have_ptrace_getfpxregs)
if test $gdb_cv_have_ptrace_getfpxregs = yes; then
  AC_DEFINE(HAVE_PTRACE_GETFPXREGS, 1,
  [Define if sys/ptrace.h defines the PTRACE_GETFPXREGS request.])
fi

# See if <sys/ptrace.h> provides the PT_GETDBREGS request.
AC_MSG_CHECKING(for PT_GETDBREGS)
AC_CACHE_VAL(gdb_cv_have_pt_getdbregs,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/ptrace.h>],
		[PT_GETDBREGS;],
		[gdb_cv_have_pt_getdbregs=yes],
		[gdb_cv_have_pt_getdbregs=no])])
AC_MSG_RESULT($gdb_cv_have_pt_getdbregs)
if test $gdb_cv_have_pt_getdbregs = yes; then
  AC_DEFINE(HAVE_PT_GETDBREGS, 1,
  [Define if sys/ptrace.h defines the PT_GETDBREGS request.])
fi

# See if <sys/ptrace.h> provides the PT_GETXMMREGS request.
AC_MSG_CHECKING(for PT_GETXMMREGS)
AC_CACHE_VAL(gdb_cv_have_pt_getxmmregs,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/ptrace.h>],
		[PT_GETXMMREGS;],
		[gdb_cv_have_pt_getxmmregs=yes],
		[gdb_cv_have_pt_getxmmregs=no])])
AC_MSG_RESULT($gdb_cv_have_pt_getxmmregs)
if test $gdb_cv_have_pt_getxmmregs = yes; then
  AC_DEFINE(HAVE_PT_GETXMMREGS, 1,
  [Define if sys/ptrace.h defines the PT_GETXMMREGS request.])
fi

# Detect which type of /proc is in use, such as for Unixware or Solaris.

if test "${target}" = "${host}"; then
  case "${host}" in
  *-*-unixware* | *-*-sysv4.2* | *-*-sysv5* | *-*-interix* )
      AC_DEFINE(NEW_PROC_API, 1,
      [Define if you want to use new multi-fd /proc interface
       (replaces HAVE_MULTIPLE_PROC_FDS as well as other macros).])
      ;;
  *-*-solaris2.[[6789]] | *-*-solaris2.1[[0-9]])
      AC_DEFINE(NEW_PROC_API, 1,
      [Define if you want to use new multi-fd /proc interface
       (replaces HAVE_MULTIPLE_PROC_FDS as well as other macros).])
      ;;
  mips-sgi-irix5*)
      # Work around <sys/proc.h> needing _KMEMUSER problem on IRIX 5.
      AC_DEFINE([_KMEMUSER], 1,
      [Define to 1 so <sys/proc.h> gets a definition of anon_hdl.  Works
       around a <sys/proc.h> problem on IRIX 5.])
      ;;
  esac
fi

if test "$ac_cv_header_sys_procfs_h" = yes; then
  BFD_HAVE_SYS_PROCFS_TYPE(pstatus_t)
  BFD_HAVE_SYS_PROCFS_TYPE(prrun_t)
  BFD_HAVE_SYS_PROCFS_TYPE(gregset_t)
  BFD_HAVE_SYS_PROCFS_TYPE(fpregset_t)
  BFD_HAVE_SYS_PROCFS_TYPE(prgregset_t)
  BFD_HAVE_SYS_PROCFS_TYPE(prfpregset_t)
  BFD_HAVE_SYS_PROCFS_TYPE(prgregset32_t)
  BFD_HAVE_SYS_PROCFS_TYPE(prfpregset32_t)
  BFD_HAVE_SYS_PROCFS_TYPE(lwpid_t)
  BFD_HAVE_SYS_PROCFS_TYPE(psaddr_t)
  BFD_HAVE_SYS_PROCFS_TYPE(prsysent_t)
  BFD_HAVE_SYS_PROCFS_TYPE(pr_sigset_t)
  BFD_HAVE_SYS_PROCFS_TYPE(pr_sigaction64_t)
  BFD_HAVE_SYS_PROCFS_TYPE(pr_siginfo64_t)


  dnl Check for broken prfpregset_t type

  dnl For Linux/i386, glibc 2.1.3 was released with a bogus
  dnl prfpregset_t type (it's a typedef for the pointer to a struct
  dnl instead of the struct itself).  We detect this here, and work
  dnl around it in gdb_proc_service.h.

  if test $bfd_cv_have_sys_procfs_type_prfpregset_t = yes; then
    AC_MSG_CHECKING(whether prfpregset_t type is broken)
    AC_CACHE_VAL(gdb_cv_prfpregset_t_broken,
      [AC_TRY_RUN([#include <sys/procfs.h>
       int main ()
       {
         if (sizeof (prfpregset_t) == sizeof (void *))
           return 1;
         return 0;
       }],
       gdb_cv_prfpregset_t_broken=no,
       gdb_cv_prfpregset_t_broken=yes,
       gdb_cv_prfpregset_t_broken=yes)])
    AC_MSG_RESULT($gdb_cv_prfpregset_t_broken)
    if test $gdb_cv_prfpregset_t_broken = yes; then
      AC_DEFINE(PRFPREGSET_T_BROKEN, 1,
      [Define if the prfpregset_t type is broken.])
    fi
  fi

  dnl Check for PIOCSET ioctl entry 

  AC_MSG_CHECKING(for PIOCSET ioctl entry in sys/procfs.h)
  AC_CACHE_VAL(gdb_cv_have_procfs_piocset,
  [AC_TRY_COMPILE([#include <unistd.h>
#include <sys/types.h>
#include <sys/procfs.h>
], [
    int dummy;;
    dummy = ioctl(0, PIOCSET, &dummy);
  ],
  gdb_cv_have_procfs_piocset=yes, gdb_cv_have_procfs_piocset=no)])
  AC_MSG_RESULT($gdb_cv_have_procfs_piocset)
  if test $gdb_cv_have_procfs_piocset = yes; then
    AC_DEFINE(HAVE_PROCFS_PIOCSET, 1,
    [Define if ioctl argument PIOCSET is available.])
  fi
fi

dnl For native ports (host == target), check to see what kind of
dnl legacy link.h support is needed.  (See solib-legacy.c.)
if test ${host} = ${target} ; then
  dnl Check for struct link_map with l_ members which are indicative
  dnl of SVR4-like shared libraries

  AC_MSG_CHECKING(for member l_addr in struct link_map)
  AC_CACHE_VAL(gdb_cv_have_struct_link_map_with_l_members,
    [AC_TRY_COMPILE([#include <link.h>],
                    [struct link_map lm; (void) lm.l_addr;],
                    gdb_cv_have_struct_link_map_with_l_members=yes,
                    gdb_cv_have_struct_link_map_with_l_members=no)])
  AC_MSG_RESULT($gdb_cv_have_struct_link_map_with_l_members)
  if test $gdb_cv_have_struct_link_map_with_l_members = yes; then
    AC_DEFINE(HAVE_STRUCT_LINK_MAP_WITH_L_MEMBERS,1,
    [Define if <link.h> exists and defines struct link_map which has
     members with an ``l_'' prefix.  (For Solaris, SVR4, and
     SVR4-like systems.)])
  fi

  dnl Check for struct link_map with lm_ members which are indicative
  dnl of SunOS-like shared libraries

  AC_MSG_CHECKING(for member lm_addr in struct link_map)
  AC_CACHE_VAL(gdb_cv_have_struct_link_map_with_lm_members,
    [AC_TRY_COMPILE([#include <sys/types.h>
#include <link.h>],
                    [struct link_map lm; (void) lm.lm_addr;],
                    gdb_cv_have_struct_link_map_with_lm_members=yes,
                    gdb_cv_have_struct_link_map_with_lm_members=no)])
  AC_MSG_RESULT($gdb_cv_have_struct_link_map_with_lm_members)
  if test $gdb_cv_have_struct_link_map_with_lm_members = yes; then
    AC_DEFINE(HAVE_STRUCT_LINK_MAP_WITH_LM_MEMBERS, 1,
    [Define if <link.h> exists and defines struct link_map which has
     members with an ``lm_'' prefix.  (For SunOS.)])
  fi

  dnl Check for struct so_map with som_ members which are found on 
  dnl some *BSD systems.

  AC_MSG_CHECKING(for member som_addr in struct so_map)
  AC_CACHE_VAL(gdb_cv_have_struct_so_map_with_som_members,
    [AC_TRY_COMPILE([#include <sys/types.h>
#ifdef HAVE_NLIST_H
#include <nlist.h>
#endif
#include <link.h>],
                    [struct so_map lm; (void) lm.som_addr;],
                    gdb_cv_have_struct_so_map_with_som_members=yes,
                    gdb_cv_have_struct_so_map_with_som_members=no)])
  AC_MSG_RESULT($gdb_cv_have_struct_so_map_with_som_members)
  if test $gdb_cv_have_struct_so_map_with_som_members = yes; then
    AC_DEFINE(HAVE_STRUCT_SO_MAP_WITH_SOM_MEMBERS, 1,
    [Define if <link.h> exists and defines a struct so_map which has
     members with an ``som_'' prefix.  (Found on older *BSD systems.)])
  fi

  dnl Check for struct link_map32 type, which allows a 64-bit Solaris
  dnl debugger to debug a 32-bit Solaris app with 32-bit shared libraries.

  AC_MSG_CHECKING(for struct link_map32 in sys/link.h)
  AC_CACHE_VAL(gdb_cv_have_struct_link_map32, 
    [AC_TRY_COMPILE([#define _SYSCALL32
#include <sys/link.h>], [struct link_map32 l;],
     gdb_cv_have_struct_link_map32=yes,
     gdb_cv_have_struct_link_map32=no)])
  AC_MSG_RESULT($gdb_cv_have_struct_link_map32)
  if test $gdb_cv_have_struct_link_map32 = yes; then
    AC_DEFINE(HAVE_STRUCT_LINK_MAP32, 1,
    [Define if <sys/link.h> has struct link_map32])
    AC_DEFINE(_SYSCALL32, 1,
    [Define if <sys/link.h> has link_map32 (solaris sparc-64 target)])
  fi
fi

# Check if the compiler supports the `long long' type.

AC_CACHE_CHECK([for long long support in compiler], gdb_cv_c_long_long,
               [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[[extern long long foo;]],
[[switch (foo & 2) { case 0: return 1; }]])],
                                  gdb_cv_c_long_long=yes,
                                  gdb_cv_c_long_long=no)])
if test $gdb_cv_c_long_long = yes; then
  AC_DEFINE(CC_HAS_LONG_LONG, 1,
            [Define to 1 if the compiler supports long long.])
fi

# Check if the compiler and runtime support printing long longs.

AC_CACHE_CHECK([for long long support in printf],
               gdb_cv_printf_has_long_long,
               [AC_RUN_IFELSE([AC_LANG_PROGRAM([AC_INCLUDES_DEFAULT],
[[char buf[32];
  long long l = 0;
  l = (l << 16) + 0x0123;
  l = (l << 16) + 0x4567;
  l = (l << 16) + 0x89ab;
  l = (l << 16) + 0xcdef;
  sprintf (buf, "0x%016llx", l);
  return (strcmp ("0x0123456789abcdef", buf));]])],
                              gdb_cv_printf_has_long_long=yes,
                              gdb_cv_printf_has_long_long=no,
                              gdb_cv_printf_has_long_long=no)])
if test $gdb_cv_printf_has_long_long = yes; then
  AC_DEFINE(PRINTF_HAS_LONG_LONG, 1,
            [Define to 1 if the "%ll" format works to print long longs.])
fi

# Check if the compiler supports the `long double' type.  We can't use
# AC_C_LONG_DOUBLE because that one does additional checks on the
# constants defined in <float.h> that fail on some systems,
# e.g. FreeBSD/i386 4.7 and OpenBSD/i386 3.6.

AC_CACHE_CHECK([for long double support in compiler], gdb_cv_c_long_double,
               [AC_COMPILE_IFELSE([AC_LANG_SOURCE([[long double foo;]])],
                                  gdb_cv_c_long_double=yes,
                                  gdb_cv_c_long_double=no)])
if test $gdb_cv_c_long_double = yes; then
  AC_DEFINE(HAVE_LONG_DOUBLE, 1,
           [Define to 1 if the compiler supports long double.])
fi

# Check if the compiler and runtime support printing long doubles.

AC_CACHE_CHECK([for long double support in printf],
               gdb_cv_printf_has_long_double,
               [AC_RUN_IFELSE([AC_LANG_PROGRAM([AC_INCLUDES_DEFAULT],
[[char buf[16];
  long double f = 3.141592653;
  sprintf (buf, "%Lg", f);
  return (strncmp ("3.14159", buf, 7));]])],
                              gdb_cv_printf_has_long_double=yes,
                              gdb_cv_printf_has_long_double=no,
                              gdb_cv_printf_has_long_double=no)])
if test $gdb_cv_printf_has_long_double = yes; then
  AC_DEFINE(PRINTF_HAS_LONG_DOUBLE, 1,
            [Define to 1 if the "%Lg" format works to print long doubles.])
fi

# Check if the compiler and runtime support scanning long doubles.

AC_CACHE_CHECK([for long double support in scanf], 
               gdb_cv_scanf_has_long_double,
               [AC_RUN_IFELSE([AC_LANG_PROGRAM(
[[#include <stdio.h>]],
[[char *buf = "3.141592653";
  long double f = 0;
  sscanf (buf, "%Lg", &f);
  return !(f > 3.14159 && f < 3.14160);]])],
                              gdb_cv_scanf_has_long_double=yes,
                              gdb_cv_scanf_has_long_double=no,
                              gdb_cv_scanf_has_long_double=no)])
if test $gdb_cv_scanf_has_long_double = yes; then
  AC_DEFINE(SCANF_HAS_LONG_DOUBLE, 1,
            [Define to 1 if the "%Lg" format works to scan long doubles.])
fi

case ${host_os} in
aix*)
  AC_CACHE_CHECK([for -bbigtoc option], [gdb_cv_bigtoc], [
    SAVE_LDFLAGS=$LDFLAGS

    case $GCC in
    yes) gdb_cv_bigtoc=-Wl,-bbigtoc ;;
    *) gdb_cv_bigtoc=-bbigtoc ;;
    esac

    LDFLAGS=$LDFLAGS\ $gdb_cv_bigtoc
    AC_TRY_LINK([], [int i;], [], [gdb_cv_bigtoc=])
    LDFLAGS="${SAVE_LDFLAGS}"
  ])
  CONFIG_LDFLAGS="${CONFIG_LDFLAGS} ${gdb_cv_bigtoc}"
  ;;
esac


dnl For certain native configurations, we need to check whether thread
dnl support can be built in or not.
dnl
dnl Note that we only want this if we are both native (host == target),
dnl and not doing a canadian cross build (build == host).

if test ${build} = ${host} -a ${host} = ${target} ; then
   case ${host_os} in
   hpux*)
      AC_MSG_CHECKING(for HPUX/OSF thread support)
      if test -f /usr/include/dce/cma_config.h ; then
         if test "$GCC" = "yes" ; then
            AC_MSG_RESULT(yes)
            AC_DEFINE(HAVE_HPUX_THREAD_SUPPORT, 1,
	    [Define if you have HPUX threads])
            CONFIG_OBS="${CONFIG_OBS} hpux-thread.o"
            CONFIG_SRCS="${CONFIG_SRCS} hpux-thread.c"
         else
            AC_MSG_RESULT(no (suppressed because you are not using GCC))
         fi
      else
         AC_MSG_RESULT(no)
      fi
      ;;
   solaris*)
      # See if thread_db library is around for Solaris thread debugging.
      # Note that we must explicitly test for version 1 of the library
      # because version 0 (present on Solaris 2.4 or earlier) doesn't have
      # the same API.
      AC_MSG_CHECKING(for Solaris thread debugging library)
      if test -f /usr/lib/libthread_db.so.1 ; then
         AC_MSG_RESULT(yes)
         AC_DEFINE(HAVE_THREAD_DB_LIB, 1,
         [Define if using Solaris thread debugging.])
         CONFIG_OBS="${CONFIG_OBS} sol-thread.o"
         CONFIG_SRCS="${CONFIG_SRCS} sol-thread.c"
         AC_CHECK_LIB(dl, dlopen)
         if test "$GCC" = "yes" ; then
            # The GNU linker requires the -export-dynamic option to make
            # all symbols visible in the dynamic symbol table.
            hold_ldflags=$LDFLAGS
            AC_MSG_CHECKING(for the ld -export-dynamic flag)
            LDFLAGS="${LDFLAGS} -Wl,-export-dynamic"
            AC_TRY_LINK(, [int i;], found=yes, found=no)
            LDFLAGS=$hold_ldflags
            AC_MSG_RESULT($found)
            if test $found = yes; then
               CONFIG_LDFLAGS="${CONFIG_LDFLAGS} -Wl,-export-dynamic"
            fi
         fi
	 # Sun randomly tweaked the prototypes in <proc_service.h>
	 # at one point.
	 AC_MSG_CHECKING(if <proc_service.h> is old)
	 AC_CACHE_VAL(gdb_cv_proc_service_is_old,[
	    AC_TRY_COMPILE([
		#include <proc_service.h>
		ps_err_e ps_pdwrite
		    (struct ps_prochandle*, psaddr_t, const void*, size_t);
	    ],, gdb_cv_proc_service_is_old=no,
	        gdb_cv_proc_service_is_old=yes)
	 ])
	 AC_MSG_RESULT($gdb_cv_proc_service_is_old)
	 if test $gdb_cv_proc_service_is_old = yes; then
	    AC_DEFINE(PROC_SERVICE_IS_OLD, 1,
            [Define if <proc_service.h> on solaris uses int instead of
             size_t, and assorted other type changes.])
	 fi
      else
         AC_MSG_RESULT(no)
      fi
      ;;
   aix*)
      AC_MSG_CHECKING(for AiX thread debugging library)
      AC_CACHE_VAL(gdb_cv_have_aix_thread_debug,
                   [AC_TRY_COMPILE([#include <sys/pthdebug.h>],
                                   [#ifndef PTHDB_VERSION_3
                                    #error
                                    #endif],
                                   gdb_cv_have_aix_thread_debug=yes,
                                   gdb_cv_have_aix_thread_debug=no)])
      AC_MSG_RESULT($gdb_cv_have_aix_thread_debug)
      if test $gdb_cv_have_aix_thread_debug = yes; then
         CONFIG_SRCS="${CONFIG_SRCS} aix-thread.c"
         CONFIG_OBS="${CONFIG_OBS} aix-thread.o"
         CONFIG_LDFLAGS="${CONFIG_LDFLAGS} -lpthdebug"
      fi
      ;;
   esac
   AC_SUBST(CONFIG_LDFLAGS)
fi

dnl See if we have a thread_db header file that has TD_NOTALLOC and
dnl other error codes.
if test "x$ac_cv_header_thread_db_h" = "xyes"; then
   AC_CACHE_CHECK([whether <thread_db.h> has TD_NOTALLOC],
                  gdb_cv_thread_db_h_has_td_notalloc,
     AC_TRY_COMPILE(
       [#include <thread_db.h>],
       [int i = TD_NOTALLOC;],
       gdb_cv_thread_db_h_has_td_notalloc=yes,
       gdb_cv_thread_db_h_has_td_notalloc=no
     )
   )
   AC_CACHE_CHECK([whether <thread_db.h> has TD_VERSION],
                  gdb_cv_thread_db_h_has_td_version,
     AC_TRY_COMPILE(
       [#include <thread_db.h>],
       [int i = TD_VERSION;],
       gdb_cv_thread_db_h_has_td_version=yes,
       gdb_cv_thread_db_h_has_td_version=no
     )
   )
   AC_CACHE_CHECK([whether <thread_db.h> has TD_NOTLS],
                  gdb_cv_thread_db_h_has_td_notls,
     AC_TRY_COMPILE(
       [#include <thread_db.h>],
       [int i = TD_NOTLS;],
       gdb_cv_thread_db_h_has_td_notls=yes,
       gdb_cv_thread_db_h_has_td_notls=no
     )
   )
fi
if test "x$gdb_cv_thread_db_h_has_td_notalloc" = "xyes"; then
  AC_DEFINE(THREAD_DB_HAS_TD_NOTALLOC, 1,
            [Define if <thread_db.h> has the TD_NOTALLOC error code.])
fi
if test "x$gdb_cv_thread_db_h_has_td_version" = "xyes"; then
  AC_DEFINE(THREAD_DB_HAS_TD_VERSION, 1,
            [Define if <thread_db.h> has the TD_VERSION error code.])
fi
if test "x$gdb_cv_thread_db_h_has_td_notls" = "xyes"; then
  AC_DEFINE(THREAD_DB_HAS_TD_NOTLS, 1,
            [Define if <thread_db.h> has the TD_NOTLS error code.])
fi

dnl See if we have a sys/syscall header file that has __NR_tkill.
if test "x$ac_cv_header_sys_syscall_h" = "xyes"; then
   AC_CACHE_CHECK([whether <sys/syscall.h> has __NR_tkill],
                  gdb_cv_sys_syscall_h_has_tkill,
     AC_TRY_COMPILE(
       [#include <sys/syscall.h>],
       [int i = __NR_tkill;],
       gdb_cv_sys_syscall_h_has_tkill=yes,
       gdb_cv_sys_syscall_h_has_tkill=no
     )
   )
fi
dnl See if we can issue tkill syscall.
if test "x$gdb_cv_sys_syscall_h_has_tkill" = "xyes" && test "x$ac_cv_func_syscall" = "xyes"; then
  AC_DEFINE(HAVE_TKILL_SYSCALL, 1, [Define if you support the tkill syscall.])
fi

dnl Handle optional features that can be enabled.

AC_ARG_WITH(sysroot,
[  --with-sysroot[=DIR] Search for usr/lib et al within DIR.],
[
 case ${with_sysroot} in
 yes) TARGET_SYSTEM_ROOT='${exec_prefix}/${target_alias}/sys-root' ;;
 *) TARGET_SYSTEM_ROOT=$with_sysroot ;;
 esac

 TARGET_SYSTEM_ROOT_DEFINE='-DTARGET_SYSTEM_ROOT=\"$(TARGET_SYSTEM_ROOT)\"'

 if test "x$prefix" = xNONE; then
  test_prefix=/usr/local
 else
  test_prefix=$prefix
 fi
 if test "x$exec_prefix" = xNONE || test "x$exec_prefix" = 'x${prefix}'; then
  test_exec_prefix=$test_prefix
 else
  test_exec_prefix=$exec_prefix
 fi
 case ${TARGET_SYSTEM_ROOT} in
 "${test_prefix}"|"${test_prefix}/"*|\
 "${test_exec_prefix}"|"${test_exec_prefix}/"*|\
 '${prefix}'|'${prefix}/'*|\
 '${exec_prefix}'|'${exec_prefix}/'*)
   t="$TARGET_SYSTEM_ROOT_DEFINE -DTARGET_SYSTEM_ROOT_RELOCATABLE"
   TARGET_SYSTEM_ROOT_DEFINE="$t"
   ;;
 esac
], [
 TARGET_SYSTEM_ROOT=
 TARGET_SYSTEM_ROOT_DEFINE='-DTARGET_SYSTEM_ROOT=\"\"'
])
AC_SUBST(TARGET_SYSTEM_ROOT)
AC_SUBST(TARGET_SYSTEM_ROOT_DEFINE)

AC_ARG_ENABLE(werror,
  [  --enable-werror    treat compile warnings as errors],
  [case "${enableval}" in
     yes | y) ERROR_ON_WARNING="yes" ;;
     no | n)  ERROR_ON_WARNING="no" ;;
     *) AC_MSG_ERROR(bad value ${enableval} for --enable-werror) ;;
   esac])

# Enable -Werror by default when using gcc
if test "${GCC}" = yes -a -z "${ERROR_ON_WARNING}" ; then
    ERROR_ON_WARNING=yes
fi

WERROR_CFLAGS=""
if test "${ERROR_ON_WARNING}" = yes ; then
    WERROR_CFLAGS="-Werror"
fi

# The entries after -Wno-pointer-sign are disabled warnings which may
# be enabled in the future, which can not currently be used to build
# GDB.
# NOTE: If you change this list, remember to update
# gdb/doc/gdbint.texinfo.
build_warnings="-Wall -Wdeclaration-after-statement -Wpointer-arith \
-Wformat-nonliteral -Wno-pointer-sign \
-Wno-unused -Wno-switch -Wno-char-subscripts"

AC_ARG_ENABLE(build-warnings,
[  --enable-build-warnings Enable build-time compiler warnings if gcc is used],
[case "${enableval}" in
  yes)	;;
  no)	build_warnings="-w";;
  ,*)   t=`echo "${enableval}" | sed -e "s/,/ /g"`
        build_warnings="${build_warnings} ${t}";;
  *,)   t=`echo "${enableval}" | sed -e "s/,/ /g"`
        build_warnings="${t} ${build_warnings}";;
  *)    build_warnings=`echo "${enableval}" | sed -e "s/,/ /g"`;;
esac
if test x"$silent" != x"yes" && test x"$build_warnings" != x""; then
  echo "Setting compiler warning flags = $build_warnings" 6>&1
fi])dnl
AC_ARG_ENABLE(gdb-build-warnings,
[  --enable-gdb-build-warnings Enable GDB specific build-time compiler warnings if gcc is used],
[case "${enableval}" in
  yes)	;;
  no)	build_warnings="-w";;
  ,*)   t=`echo "${enableval}" | sed -e "s/,/ /g"`
        build_warnings="${build_warnings} ${t}";;
  *,)   t=`echo "${enableval}" | sed -e "s/,/ /g"`
        build_warnings="${t} ${build_warnings}";;
  *)    build_warnings=`echo "${enableval}" | sed -e "s/,/ /g"`;;
esac
if test x"$silent" != x"yes" && test x"$build_warnings" != x""; then
  echo "Setting GDB specific compiler warning flags = $build_warnings" 6>&1
fi])dnl
WARN_CFLAGS=""
if test "x${build_warnings}" != x -a "x$GCC" = xyes
then
    AC_MSG_CHECKING(compiler warning flags)
    # Separate out the -Werror flag as some files just cannot be
    # compiled with it enabled.
    for w in ${build_warnings}; do
	case $w in
	-Werr*) WERROR_CFLAGS=-Werror ;;
	*) # Check that GCC accepts it
	    saved_CFLAGS="$CFLAGS"
	    CFLAGS="$CFLAGS $w"
	    AC_TRY_COMPILE([],[],WARN_CFLAGS="${WARN_CFLAGS} $w",)
	    CFLAGS="$saved_CFLAGS"
	esac
    done
    AC_MSG_RESULT(${WARN_CFLAGS} ${WERROR_CFLAGS})
fi
AC_SUBST(WARN_CFLAGS)
AC_SUBST(WERROR_CFLAGS)

# In the Cygwin environment, we need some additional flags.
AC_CACHE_CHECK([for cygwin], gdb_cv_os_cygwin,
[AC_EGREP_CPP(lose, [
#if defined (__CYGWIN__) || defined (__CYGWIN32__)
lose
#endif],[gdb_cv_os_cygwin=yes],[gdb_cv_os_cygwin=no])])


dnl Figure out which of the many generic ser-*.c files the _host_ supports.
SER_HARDWIRE="ser-base.o ser-unix.o ser-pipe.o ser-tcp.o"
case ${host} in
  *go32* ) SER_HARDWIRE=ser-go32.o ;;
  *djgpp* ) SER_HARDWIRE=ser-go32.o ;;
  *mingw32*) SER_HARDWIRE="ser-base.o ser-tcp.o ser-mingw.o" ;;
esac
AC_SUBST(SER_HARDWIRE)

# libreadline needs libuser32.a in a cygwin environment
WIN32LIBS=
if test x$gdb_cv_os_cygwin = xyes; then
    WIN32LIBS="-luser32"
    case "${target}" in
	*cygwin*) WIN32LIBS="$WIN32LIBS -limagehlp"
	;;
    esac
fi

# The ser-tcp.c module requires sockets.
case ${host} in
  *mingw32*)
    AC_DEFINE(USE_WIN32API, 1,
              [Define if we should use the Windows API, instead of the 
	       POSIX API.  On Windows, we use the Windows API when 
	       building for MinGW, but the POSIX API when building 
	       for Cygwin.])
    WIN32LIBS="$WIN32LIBS -lws2_32"
    ;;
esac	    
AC_SUBST(WIN32LIBS)

# Add ELF support to GDB, but only if BFD includes ELF support.
OLD_CFLAGS=$CFLAGS
OLD_LDFLAGS=$LDFLAGS
OLD_LIBS=$LIBS
CFLAGS="$CFLAGS -I${srcdir}/../include -I../bfd -I${srcdir}/../bfd"
LDFLAGS="$LDFLAGS -L../bfd -L../libiberty"
intl=`echo $LIBINTL | sed 's,${top_builddir}/,,g'`
LIBS="$LIBS -lbfd -liberty $intl"
AC_CACHE_CHECK([for ELF support in BFD], gdb_cv_var_elf,
[AC_TRY_LINK(
[#include <stdlib.h>
#include "bfd.h"
#include "elf-bfd.h"
],
[bfd *abfd = NULL; bfd_get_elf_phdr_upper_bound (abfd); ],
gdb_cv_var_elf=yes, gdb_cv_var_elf=no)])
if test $gdb_cv_var_elf = yes; then
  CONFIG_OBS="$CONFIG_OBS elfread.o"
  AC_DEFINE(HAVE_ELF, 1,
	    [Define if ELF support should be included.])
fi
CFLAGS=$OLD_CFLAGS
LDFLAGS=$OLD_LDFLAGS
LIBS=$OLD_LIBS

# Add any host-specific objects to GDB.
CONFIG_OBS="${CONFIG_OBS} ${gdb_host_obs}"

LIBGUI="../libgui/src/libgui.a"
GUI_CFLAGS_X="-I${srcdir}/../libgui/src"
AC_SUBST(LIBGUI)
AC_SUBST(GUI_CFLAGS_X)

WIN32LDAPP=
AC_SUBST(WIN32LIBS)
AC_SUBST(WIN32LDAPP)

case "${host}" in
*-*-cygwin*)
    configdir="win"
    ;;
*)
    configdir="unix"
    ;;
esac

GDBTKLIBS=
if test "${enable_gdbtk}" = "yes"; then

    # Gdbtk must have an absolute path to srcdir in order to run
    # properly when not installed.
    here=`pwd`
    cd ${srcdir}
    GDBTK_SRC_DIR=`pwd`
    cd $here

    CY_AC_PATH_TCLCONFIG
    if test -z "${no_tcl}"; then
	CY_AC_LOAD_TCLCONFIG
	CY_AC_PATH_TKCONFIG

	# now look for Tcl library stuff

	tcldir="../tcl/${configdir}/"

	TCL_DEPS="${tcldir}${TCL_LIB_FILE}"

	# If $no_tk is nonempty, then we can't do Tk, and there is no
	# point to doing Tcl.
	if test -z "${no_tk}"; then
	   CY_AC_LOAD_TKCONFIG
	   CY_AC_PATH_TCLH
	   CY_AC_PATH_TKH
	   CY_AC_PATH_ITCLH
	   CY_AC_PATH_ITKH


	   # now look for Tk library stuff

	   tkdir="../tk/${configdir}/"

	   TK_DEPS="${tkdir}${TK_LIB_FILE}"

	   # now look for Itcl library stuff

	   CY_AC_PATH_ITCLCONFIG
           if test -z "${no_itcl}"; then
             CY_AC_LOAD_ITCLCONFIG 

	     ITCLLIB="${ITCL_BUILD_LIB_SPEC}"
	     ITCL_DEPS="${ITCL_LIB_FULL_PATH}"
           fi

	   
	   # now look for Itk library stuff
	   CY_AC_PATH_ITKCONFIG
           if test -z "${no_itcl}"; then
             CY_AC_LOAD_ITKCONFIG

	     ITKLIB="${ITK_BUILD_LIB_SPEC}"
	     ITK_DEPS="${ITK_LIB_FULL_PATH}"
           fi

           ENABLE_CFLAGS="${ENABLE_CFLAGS} \$(SUBDIR_GDBTK_CFLAGS)"

	   # Include some libraries that Tcl and Tk want.
	   TCL_LIBS='$(LIBGUI) $(ITCL) $(ITK) $(TK) $(TCL) $(X11_LDFLAGS) $(X11_LIBS)'
	   # Yes, the ordering seems wrong here.  But it isn't.
	   # TK_LIBS is the list of libraries that need to be linked
	   # after Tcl/Tk.  Note that this isn't put into LIBS.  If it
	   # were in LIBS then any link tests after this point would
	   # try to include things like `$(LIBGUI)', which wouldn't work.
	   GDBTKLIBS="${TCL_LIBS} ${TK_LIBS}"

           CONFIG_OBS="${CONFIG_OBS} \$(SUBDIR_GDBTK_OBS)"
           CONFIG_DEPS="${CONFIG_DEPS} \$(SUBDIR_GDBTK_DEPS)"
           CONFIG_SRCS="${CONFIG_SRCS} \$(SUBDIR_GDBTK_SRCS)"
	   CONFIG_ALL="${CONFIG_ALL} all-gdbtk"
	   CONFIG_CLEAN="${CONFIG_CLEAN} clean-gdbtk"
	   CONFIG_INSTALL="${CONFIG_INSTALL} install-gdbtk"
	   CONFIG_UNINSTALL="${CONFIG_UNINSTALL} uninstall-gdbtk"

	   if test x$gdb_cv_os_cygwin = xyes; then
	      WIN32LIBS="${WIN32LIBS} -lshell32 -lgdi32 -lcomdlg32 -ladvapi32"
	      WIN32LDAPP="-Wl,--subsystem,console"
	      CONFIG_OBS="${CONFIG_OBS} gdbres.o"
	   fi
	fi
    fi

    AC_CONFIG_SUBDIRS(gdbtk)
fi

AC_SUBST(X_CFLAGS)
AC_SUBST(X_LDFLAGS)
AC_SUBST(X_LIBS)
AC_SUBST(TCL_DEPS)
AC_SUBST(TK_DEPS)
AC_SUBST(ITCLLIB)
AC_SUBST(ITCL_DEPS)
AC_SUBST(ITKLIB)
AC_SUBST(ITK_DEPS)
AC_SUBST(GDBTKLIBS)
AC_SUBST(GDBTK_CFLAGS)
AC_SUBST(GDBTK_SRC_DIR)

AC_PATH_X

# Check whether we should enable the TUI, but only do so if we really
# can.
if test x"$enable_tui" = xyes; then
  if test -d $srcdir/tui; then
    if test "$ac_cv_search_waddstr" != no; then
      CONFIG_OBS="$CONFIG_OBS \$(SUBDIR_TUI_OBS)"
      CONFIG_DEPS="$CONFIG_DEPS \$(SUBDIR_TUI_DEPS)"
      CONFIG_SRCS="$CONFIG_SRCS \$(SUBDIR_TUI_SRCS)"
      CONFIG_INITS="$CONFIG_INITS \$(SUBDIR_TUI_INITS)"
      ENABLE_CFLAGS="$ENABLE_CFLAGS \$(SUBDIR_TUI_CFLAGS)"
      CONFIG_ALL="${CONFIG_ALL} all-tui"
      CONFIG_CLEAN="${CONFIG_CLEAN} clean-tui"
      CONFIG_INSTALL="${CONFIG_INSTALL} install-tui"
      CONFIG_UNINSTALL="${CONFIG_UNINSTALL} uninstall-tui"
    else
      AC_MSG_WARN([no enhanced curses library found; disabling TUI])
    fi
  fi
fi

# Unlike the sim directory, whether a simulator is linked is controlled by 
# presence of a SIM= and a SIM_OBS= definition in the target '.mt' file.  
# This code just checks for a few cases where we'd like to ignore those
# definitions, even when they're present in the '.mt' file.  These cases
# are when --disable-sim is specified, or if the simulator directory is
# not part of the source tree.
#
AC_ARG_ENABLE(sim,
[  --enable-sim            Link gdb with simulator],
[echo "enable_sim = $enable_sim";
 echo "enableval = ${enableval}";
 case "${enableval}" in
  yes) ignore_sim=false ;;
  no)  ignore_sim=true ;;
  *)   ignore_sim=false ;;
 esac],
[ignore_sim=false])

if test ! -d "${srcdir}/../sim"; then
  ignore_sim=true
fi

if test "${ignore_sim}" = "true"; then
    IGNORE_SIM="SIM="
    IGNORE_SIM_OBS="SIM_OBS="
else
    IGNORE_SIM=""
    IGNORE_SIM_OBS=""
    AC_DEFINE(WITH_SIM, 1, [Define if the simulator is being linked in.])
fi
AC_SUBST(IGNORE_SIM)
AC_SUBST(IGNORE_SIM_OBS)

AC_SUBST(ENABLE_CFLAGS)
AC_SUBST(PROFILE_CFLAGS)

AC_SUBST(CONFIG_OBS)
AC_SUBST(CONFIG_DEPS)
AC_SUBST(CONFIG_SRCS)
AC_SUBST(CONFIG_ALL)
AC_SUBST(CONFIG_CLEAN)
AC_SUBST(CONFIG_INSTALL)
AC_SUBST(CONFIG_UNINSTALL)

# List of host floatformats.
AC_DEFINE_UNQUOTED(GDB_HOST_FLOAT_FORMAT,$gdb_host_float_format,[Host float floatformat])
AC_DEFINE_UNQUOTED(GDB_HOST_DOUBLE_FORMAT,$gdb_host_double_format,[Host double floatformat])
AC_DEFINE_UNQUOTED(GDB_HOST_LONG_DOUBLE_FORMAT,$gdb_host_long_double_format,[Host long double floatformat])

# target_subdir is used by the testsuite to find the target libraries.
target_subdir=
if test "${host}" != "${target}"; then
    target_subdir="${target_alias}/"
fi
AC_SUBST(target_subdir)

frags=
if test "${gdb_native}" = "yes"; then
  host_makefile_frag=${srcdir}/config/${gdb_host_cpu}/${gdb_host}.mh
  if test ! -f ${host_makefile_frag}; then
    AC_MSG_ERROR("*** Gdb does not support native target ${host}")
  fi
  frags="$frags $host_makefile_frag"
else
  host_makefile_frag=/dev/null
fi

target_makefile_frag=${srcdir}/config/${gdb_target_cpu}/${gdb_target}.mt
if test ! -f ${target_makefile_frag}; then
  AC_MSG_ERROR("*** Gdb does not support target ${target}")
fi
frags="$frags $target_makefile_frag"

AC_SUBST_FILE(host_makefile_frag)
AC_SUBST_FILE(target_makefile_frag)
AC_SUBST(frags)

changequote(,)dnl
hostfile=`sed -n '
s/XM_FILE[ 	]*=[ 	]*\([^ 	]*\)/\1/p
' ${host_makefile_frag}`

targetfile=`sed -n '
s/^[ 	]*DEPRECATED_TM_FILE[ 	]*=[ 	]*\([^ 	]*\)/\1/p
' ${target_makefile_frag}`

if test "${gdb_native}" = "yes"; then
# We pick this up from the host configuration file (.mh) because we
# do not have a native configuration Makefile fragment.
nativefile=`sed -n '
s/NAT_FILE[ 	]*=[ 	]*\([^ 	]*\)/\1/p
' ${host_makefile_frag}`
fi
changequote([,])

if test x"${gdb_osabi}" != x ; then
    AC_DEFINE_UNQUOTED(GDB_OSABI_DEFAULT, $gdb_osabi,
		       [Define to the default OS ABI for this configuration.])
fi

# Enable multi-ice-gdb-server.
AC_ARG_ENABLE(multi-ice,
[  --enable-multi-ice      build the multi-ice-gdb-server],
  [case $enableval in
    yes | no)
      ;;
    *) AC_MSG_ERROR([bad value $enableval for --enable-multi-ice]) ;;
  esac])
if test "x$enable_multi_ice" = xyes; then
   AC_CONFIG_SUBDIRS(multi-ice)
fi

# We only build gdbserver automatically in a native configuration. 
if test "$gdb_native" = "yes"; then
  AC_MSG_CHECKING(whether gdbserver is supported on this host)
  if test "x$build_gdbserver" = xyes; then
    AC_MSG_RESULT(yes)
    AC_CONFIG_SUBDIRS(gdbserver)
  else
    AC_MSG_RESULT(no)
  fi
fi

# If hostfile (XM_FILE) and/or targetfile (DEPRECATED_TM_FILE) and/or
# nativefile (NAT_FILE) is not set in config/*/*.m[ht] files, we link
# to an empty version.

files=
links=

rm -f xm.h
xm_h=""
if test "${hostfile}" != ""; then
    xm_h=xm.h
    case "${hostfile}" in
      xm-*.h ) GDB_XM_FILE="config/${gdb_host_cpu}/${hostfile}" ;;
      * ) GDB_XM_FILE="${hostfile}"
    esac
    files="${files} ${GDB_XM_FILE}"
    links="${links} xm.h"
    AC_DEFINE_UNQUOTED(GDB_XM_FILE, "${GDB_XM_FILE}", [hostfile])
fi
AC_SUBST(xm_h)

rm -f tm.h
tm_h=""
if test "${targetfile}" != ""; then
    tm_h=tm.h
    case "${targetfile}" in
      tm-*.h ) GDB_TM_FILE="config/${gdb_target_cpu}/${targetfile}" ;;
      * ) GDB_TM_FILE="${targetfile}"
    esac
    files="${files} ${GDB_TM_FILE}"
    links="${links} tm.h"
    AC_DEFINE_UNQUOTED(GDB_TM_FILE, "${GDB_TM_FILE}", [targetfile])
fi
AC_SUBST(tm_h)

rm -f nm.h
nm_h=""
if test "${nativefile}" != ""; then
    nm_h=nm.h
    case "${nativefile}" in
      nm-*.h ) GDB_NM_FILE="config/${gdb_host_cpu}/${nativefile}" ;;
      * ) GDB_NM_FILE="${nativefile}"
    esac
    files="${files} ${GDB_NM_FILE}"
    links="${links} nm.h"
    AC_DEFINE_UNQUOTED(GDB_NM_FILE, "${GDB_NM_FILE}", [nativefile])
fi
AC_SUBST(nm_h)

AC_LINK_FILES($files, $links)

dnl Check for exe extension set on certain hosts (e.g. Win32)
AC_EXEEXT

dnl  Detect the character set used by this host.

dnl  At the moment, we just assume it's ISO-8859-1 (which is a
dnl  superset of ASCII containing the characters needed for French,
dnl  German, Spanish, Italian, and possibly others), but if were
dnl  *were* to support any host character sets other than ISO-8859-1,
dnl  here's where we'd detect it.
AC_DEFINE(GDB_DEFAULT_HOST_CHARSET, "ISO-8859-1",
          [Define to be a string naming the default host character set.])

AM_ICONV

AC_OUTPUT(Makefile .gdbinit:gdbinit.in,
[
dnl Autoconf doesn't provide a mechanism for modifying definitions 
dnl provided by makefile fragments.
dnl

changequote(,)dnl
sed -e '/^DEPRECATED_TM_FILE[ 	]*=/s,^DEPRECATED_TM_FILE[ 	]*=[ 	]*,&config/'"${gdb_target_cpu}"'/,
/^XM_FILE[ 	]*=/s,^XM_FILE[ 	]*=[ 	]*,&config/'"${gdb_host_cpu}"'/,
/^NAT_FILE[ 	]*=/s,^NAT_FILE[ 	]*=[ 	]*,&config/'"${gdb_host_cpu}"'/,' <Makefile >Makefile.tmp
mv -f Makefile.tmp Makefile
changequote([,])dnl


case x$CONFIG_HEADERS in
xconfig.h:config.in)
echo > stamp-h ;;
esac
],
[
gdb_host_cpu=$gdb_host_cpu
gdb_target_cpu=$gdb_target_cpu
nativefile=$nativefile
])

exit 0
