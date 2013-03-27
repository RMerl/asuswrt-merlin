dnl Autoconf configure script for GDB server.
dnl Copyright (C) 2000, 2002, 2003, 2004, 2005, 2006
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

AC_INIT(server.c)
AC_CONFIG_HEADER(config.h:config.in)

AC_PROG_CC

AC_CANONICAL_SYSTEM

AC_PROG_INSTALL

AC_ARG_PROGRAM

AC_HEADER_STDC

AC_CHECK_HEADERS(sgtty.h termio.h termios.h sys/reg.h string.h dnl
		 proc_service.h sys/procfs.h thread_db.h linux/elf.h dnl
		 stdlib.h unistd.h dnl
 		 errno.h fcntl.h signal.h sys/file.h malloc.h dnl
		 sys/ioctl.h netinet/in.h sys/socket.h netdb.h dnl
		 netinet/tcp.h arpa/inet.h sys/wait.h)

have_errno=no
AC_MSG_CHECKING(for errno)
AC_TRY_LINK([
#if HAVE_ERRNO_H
#include <errno.h>
#endif], [static int x; x = errno;],
  [AC_MSG_RESULT(yes - in errno.h); AC_DEFINE(HAVE_ERRNO, 1, [Define if errno is available]) have_errno=yes])
if test $have_errno = no; then
AC_TRY_LINK([
#if HAVE_ERRNO_H
#include <errno.h>
#endif], [extern int errno; static int x; x = errno;],
  [AC_MSG_RESULT(yes - must define); AC_DEFINE(HAVE_ERRNO, 1, [Define if errno is available]) AC_DEFINE(MUST_DEFINE_ERRNO, 1, [Checking if errno must be defined])],
  [AC_MSG_RESULT(no)])
fi

AC_CHECK_DECLS([strerror, perror])

AC_CHECK_TYPES(socklen_t, [], [],
[#include <sys/types.h>
#include <sys/socket.h>
])

# Check for various supplementary target information (beyond the
# triplet) which might affect the choices in configure.srv.
case "${target}" in
  arm*-*-linux*)
    AC_CACHE_CHECK([if iWMMXt is selected], [gdb_cv_arm_iwmmxt],
		   [save_CPPFLAGS="$CPPFLAGS"
		   CPPFLAGS="$CPPFLAGS $CFLAGS"
		   AC_EGREP_CPP([got it], [
#ifdef __IWMMXT__
got it
#endif
				], [gdb_cv_arm_iwmmxt=yes],
				[gdb_cv_arm_iwmmxt=no])
		   CPPFLAGS="$save_CPPFLAGS"])
    ;;
esac

. ${srcdir}/configure.srv

if test "${srv_mingwce}" = "yes"; then
  LIBS="$LIBS -lws2"
elif test "${srv_mingw}" = "yes"; then
  LIBS="$LIBS -lwsock32"
fi

if test "${srv_mingw}" = "yes"; then
  AC_DEFINE(USE_WIN32API, 1,
	    [Define if we should use the Windows API, instead of the
	     POSIX API.  On Windows, we use the Windows API when
	     building for MinGW, but the POSIX API when building
	     for Cygwin.])
fi

if test "${srv_linux_usrregs}" = "yes"; then
  AC_DEFINE(HAVE_LINUX_USRREGS, 1,
	    [Define if the target supports PTRACE_PEEKUSR for register ]
	    [access.])
fi

if test "${srv_linux_regsets}" = "yes"; then
  AC_DEFINE(HAVE_LINUX_REGSETS, 1,
	    [Define if the target supports register sets.])

  AC_MSG_CHECKING(for PTRACE_GETREGS)
  AC_CACHE_VAL(gdbsrv_cv_have_ptrace_getregs,
  [AC_TRY_COMPILE([#include <sys/ptrace.h>],
		  [PTRACE_GETREGS;],
		  [gdbsrv_cv_have_ptrace_getregs=yes],
		  [gdbsrv_cv_have_ptrace_getregs=no])])
  AC_MSG_RESULT($gdbsrv_cv_have_ptrace_getregs)
  if test "${gdbsrv_cv_have_ptrace_getregs}" = "yes"; then
    AC_DEFINE(HAVE_PTRACE_GETREGS, 1,
	      [Define if the target supports PTRACE_GETREGS for register ]
	      [access.])
  fi

  AC_MSG_CHECKING(for PTRACE_GETFPXREGS)
  AC_CACHE_VAL(gdbsrv_cv_have_ptrace_getfpxregs,
  [AC_TRY_COMPILE([#include <sys/ptrace.h>],
		  [PTRACE_GETFPXREGS;],
		  [gdbsrv_cv_have_ptrace_getfpxregs=yes],
		  [gdbsrv_cv_have_ptrace_getfpxregs=no])])
  AC_MSG_RESULT($gdbsrv_cv_have_ptrace_getfpxregs)
  if test "${gdbsrv_cv_have_ptrace_getfpxregs}" = "yes"; then
    AC_DEFINE(HAVE_PTRACE_GETFPXREGS, 1,
	      [Define if the target supports PTRACE_GETFPXREGS for extended ]
	      [register access.])
  fi
fi

if test "$ac_cv_header_sys_procfs_h" = yes; then
  BFD_HAVE_SYS_PROCFS_TYPE(lwpid_t)
  BFD_HAVE_SYS_PROCFS_TYPE(psaddr_t)
  BFD_HAVE_SYS_PROCFS_TYPE(prgregset_t)
  BFD_HAVE_SYS_PROCFS_TYPE(elf_fpregset_t)
fi

srv_thread_depfiles=
srv_libs=
USE_THREAD_DB=

if test "$srv_linux_thread_db" = "yes"; then
  SRV_CHECK_THREAD_DB
  if test "$srv_cv_thread_db" = no; then
    AC_WARN([Could not find libthread_db.])
    AC_WARN([Disabling thread support in gdbserver.])
    srv_linux_thread_db=no
  else
    srv_libs="$srv_cv_thread_db"
    SRV_CHECK_TLS_GET_ADDR
  fi
  old_LDFLAGS="$LDFLAGS"
  LDFLAGS="$LDFLAGS -rdynamic"
  AC_TRY_LINK([], [], [RDYNAMIC=-rdynamic], [RDYNAMIC=])
  AC_SUBST(RDYNAMIC)
  LDFLAGS="$old_LDFLAGS"
fi

if test "$srv_linux_thread_db" = "yes"; then
  srv_thread_depfiles="thread-db.o proc-service.o"
  USE_THREAD_DB="-DUSE_THREAD_DB"
  AC_CACHE_CHECK([for TD_VERSION], gdbsrv_cv_have_td_version,
  [AC_TRY_COMPILE([#include <thread_db.h>], [TD_VERSION;],
                  [gdbsrv_cv_have_td_version=yes],
                  [gdbsrv_cv_have_td_version=no])])
  if test $gdbsrv_cv_have_td_version = yes; then
    AC_DEFINE(HAVE_TD_VERSION, 1, [Define if TD_VERSION is available.])
  fi

  if test "$srv_cv_tls_get_addr" = yes; then
    AC_DEFINE(HAVE_TD_THR_TLS_GET_ADDR, 1, [Define if td_thr_tls_get_addr is available.])
  fi
fi

if test "$srv_xmltarget" != ""; then
  srv_xmltarget="\$(XML_DIR)/$srv_xmltarget"
  srv_xmlbuiltin="xml-builtin.o"
  AC_DEFINE(USE_XML, 1, [Define if an XML target description is available.])

  tmp_xmlfiles=$srv_xmlfiles
  srv_xmlfiles="target.xml"
  for f in $tmp_xmlfiles; do
    srv_xmlfiles="$srv_xmlfiles \$(XML_DIR)/$f"
  done
fi

GDBSERVER_DEPFILES="$srv_regobj $srv_tgtobj $srv_thread_depfiles"
GDBSERVER_LIBS="$srv_libs"

AC_SUBST(GDBSERVER_DEPFILES)
AC_SUBST(GDBSERVER_LIBS)
AC_SUBST(USE_THREAD_DB)
AC_SUBST(srv_xmlbuiltin)
AC_SUBST(srv_xmlfiles)
AC_SUBST(srv_xmltarget)

AC_OUTPUT(Makefile,
[case x$CONFIG_HEADERS in
xconfig.h:config.in)
echo > stamp-h ;;
esac
])
