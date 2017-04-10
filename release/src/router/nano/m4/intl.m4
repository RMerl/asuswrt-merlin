# intl.m4 serial 25 (gettext-0.18.3)
dnl Copyright (C) 1995-2013 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.
dnl
dnl This file can can be used in projects which are not available under
dnl the GNU General Public License or the GNU Library General Public
dnl License but which still want to provide support for the GNU gettext
dnl functionality.
dnl Please note that the actual code of the GNU gettext library is covered
dnl by the GNU Library General Public License, and the rest of the GNU
dnl gettext package package is covered by the GNU General Public License.
dnl They are *not* in the public domain.

dnl Authors:
dnl   Ulrich Drepper <drepper@cygnus.com>, 1995-2000.
dnl   Bruno Haible <haible@clisp.cons.org>, 2000-2009.

AC_PREREQ([2.60])

dnl Checks for all prerequisites of the intl subdirectory,
dnl except for INTL_LIBTOOL_SUFFIX_PREFIX (and possibly LIBTOOL), INTLOBJS,
dnl            USE_INCLUDED_LIBINTL, BUILD_INCLUDED_LIBINTL.
AC_DEFUN([AM_INTL_SUBDIR],
[
  AC_REQUIRE([AC_PROG_INSTALL])dnl
  AC_REQUIRE([AC_PROG_MKDIR_P])dnl
  AC_REQUIRE([AC_PROG_CC])dnl
  AC_REQUIRE([AC_CANONICAL_HOST])dnl
  AC_REQUIRE([gt_GLIBC2])dnl
  AC_REQUIRE([AC_PROG_RANLIB])dnl
  AC_REQUIRE([gl_VISIBILITY])dnl
  AC_REQUIRE([gt_INTL_SUBDIR_CORE])dnl
  AC_REQUIRE([AC_TYPE_LONG_LONG_INT])dnl
  AC_REQUIRE([gt_TYPE_WCHAR_T])dnl
  AC_REQUIRE([gt_TYPE_WINT_T])dnl
  AC_REQUIRE([gl_AC_HEADER_INTTYPES_H])
  AC_REQUIRE([gt_TYPE_INTMAX_T])
  AC_REQUIRE([gt_PRINTF_POSIX])
  AC_REQUIRE([gl_GLIBC21])dnl
  AC_REQUIRE([gl_XSIZE])dnl
  AC_REQUIRE([gl_FCNTL_O_FLAGS])dnl
  AC_REQUIRE([gt_INTL_MACOSX])dnl
  AC_REQUIRE([gl_EXTERN_INLINE])dnl

  dnl Support for automake's --enable-silent-rules.
  case "$enable_silent_rules" in
    yes) INTL_DEFAULT_VERBOSITY=0;;
    no)  INTL_DEFAULT_VERBOSITY=1;;
    *)   INTL_DEFAULT_VERBOSITY=1;;
  esac
  AC_SUBST([INTL_DEFAULT_VERBOSITY])

  AC_CHECK_TYPE([ptrdiff_t], ,
    [AC_DEFINE([ptrdiff_t], [long],
       [Define as the type of the result of subtracting two pointers, if the system doesn't define it.])
    ])
  AC_CHECK_HEADERS([features.h stddef.h stdlib.h string.h])
  AC_CHECK_FUNCS([asprintf fwprintf newlocale putenv setenv setlocale \
    snprintf strnlen wcslen wcsnlen mbrtowc wcrtomb])

  dnl Use the _snprintf function only if it is declared (because on NetBSD it
  dnl is defined as a weak alias of snprintf; we prefer to use the latter).
  AC_CHECK_DECLS([_snprintf, _snwprintf], , , [#include <stdio.h>])

  dnl Use the *_unlocked functions only if they are declared.
  dnl (because some of them were defined without being declared in Solaris
  dnl 2.5.1 but were removed in Solaris 2.6, whereas we want binaries built
  dnl on Solaris 2.5.1 to run on Solaris 2.6).
  AC_CHECK_DECLS([getc_unlocked], , , [#include <stdio.h>])

  case $gt_cv_func_printf_posix in
    *yes) HAVE_POSIX_PRINTF=1 ;;
    *) HAVE_POSIX_PRINTF=0 ;;
  esac
  AC_SUBST([HAVE_POSIX_PRINTF])
  if test "$ac_cv_func_asprintf" = yes; then
    HAVE_ASPRINTF=1
  else
    HAVE_ASPRINTF=0
  fi
  AC_SUBST([HAVE_ASPRINTF])
  if test "$ac_cv_func_snprintf" = yes; then
    HAVE_SNPRINTF=1
  else
    HAVE_SNPRINTF=0
  fi
  AC_SUBST([HAVE_SNPRINTF])
  if test "$ac_cv_func_newlocale" = yes; then
    HAVE_NEWLOCALE=1
  else
    HAVE_NEWLOCALE=0
  fi
  AC_SUBST([HAVE_NEWLOCALE])
  if test "$ac_cv_func_wprintf" = yes; then
    HAVE_WPRINTF=1
  else
    HAVE_WPRINTF=0
  fi
  AC_SUBST([HAVE_WPRINTF])

  AM_LANGINFO_CODESET
  gt_LC_MESSAGES

  dnl Compilation on mingw and Cygwin needs special Makefile rules, because
  dnl 1. when we install a shared library, we must arrange to export
  dnl    auxiliary pointer variables for every exported variable,
  dnl 2. when we install a shared library and a static library simultaneously,
  dnl    the include file specifies __declspec(dllimport) and therefore we
  dnl    must arrange to define the auxiliary pointer variables for the
  dnl    exported variables _also_ in the static library.
  if test "$enable_shared" = yes; then
    case "$host_os" in
      mingw* | cygwin*) is_woe32dll=yes ;;
      *) is_woe32dll=no ;;
    esac
  else
    is_woe32dll=no
  fi
  WOE32DLL=$is_woe32dll
  AC_SUBST([WOE32DLL])

  dnl On mingw and Cygwin, we can activate special Makefile rules which add
  dnl version information to the shared libraries and executables.
  case "$host_os" in
    mingw* | cygwin*) is_woe32=yes ;;
    *) is_woe32=no ;;
  esac
  WOE32=$is_woe32
  AC_SUBST([WOE32])
  if test $WOE32 = yes; then
    dnl Check for a program that compiles Windows resource files.
    AC_CHECK_TOOL([WINDRES], [windres])
  fi

  dnl Determine whether when creating a library, "-lc" should be passed to
  dnl libtool or not. On many platforms, it is required for the libtool option
  dnl -no-undefined to work. On HP-UX, however, the -lc - stored by libtool
  dnl in the *.la files - makes it impossible to create multithreaded programs,
  dnl because libtool also reorders the -lc to come before the -pthread, and
  dnl this disables pthread_create() <http://docs.hp.com/en/1896/pthreads.html>.
  case "$host_os" in
    hpux*) LTLIBC="" ;;
    *)     LTLIBC="-lc" ;;
  esac
  AC_SUBST([LTLIBC])

  dnl Rename some macros and functions used for locking.
  AH_BOTTOM([
#define __libc_lock_t                   gl_lock_t
#define __libc_lock_define              gl_lock_define
#define __libc_lock_define_initialized  gl_lock_define_initialized
#define __libc_lock_init                gl_lock_init
#define __libc_lock_lock                gl_lock_lock
#define __libc_lock_unlock              gl_lock_unlock
#define __libc_lock_recursive_t                   gl_recursive_lock_t
#define __libc_lock_define_recursive              gl_recursive_lock_define
#define __libc_lock_define_initialized_recursive  gl_recursive_lock_define_initialized
#define __libc_lock_init_recursive                gl_recursive_lock_init
#define __libc_lock_lock_recursive                gl_recursive_lock_lock
#define __libc_lock_unlock_recursive              gl_recursive_lock_unlock
#define glthread_in_use  libintl_thread_in_use
#define glthread_lock_init_func     libintl_lock_init_func
#define glthread_lock_lock_func     libintl_lock_lock_func
#define glthread_lock_unlock_func   libintl_lock_unlock_func
#define glthread_lock_destroy_func  libintl_lock_destroy_func
#define glthread_rwlock_init_multithreaded     libintl_rwlock_init_multithreaded
#define glthread_rwlock_init_func              libintl_rwlock_init_func
#define glthread_rwlock_rdlock_multithreaded   libintl_rwlock_rdlock_multithreaded
#define glthread_rwlock_rdlock_func            libintl_rwlock_rdlock_func
#define glthread_rwlock_wrlock_multithreaded   libintl_rwlock_wrlock_multithreaded
#define glthread_rwlock_wrlock_func            libintl_rwlock_wrlock_func
#define glthread_rwlock_unlock_multithreaded   libintl_rwlock_unlock_multithreaded
#define glthread_rwlock_unlock_func            libintl_rwlock_unlock_func
#define glthread_rwlock_destroy_multithreaded  libintl_rwlock_destroy_multithreaded
#define glthread_rwlock_destroy_func           libintl_rwlock_destroy_func
#define glthread_recursive_lock_init_multithreaded     libintl_recursive_lock_init_multithreaded
#define glthread_recursive_lock_init_func              libintl_recursive_lock_init_func
#define glthread_recursive_lock_lock_multithreaded     libintl_recursive_lock_lock_multithreaded
#define glthread_recursive_lock_lock_func              libintl_recursive_lock_lock_func
#define glthread_recursive_lock_unlock_multithreaded   libintl_recursive_lock_unlock_multithreaded
#define glthread_recursive_lock_unlock_func            libintl_recursive_lock_unlock_func
#define glthread_recursive_lock_destroy_multithreaded  libintl_recursive_lock_destroy_multithreaded
#define glthread_recursive_lock_destroy_func           libintl_recursive_lock_destroy_func
#define glthread_once_func            libintl_once_func
#define glthread_once_singlethreaded  libintl_once_singlethreaded
#define glthread_once_multithreaded   libintl_once_multithreaded
])
])


dnl Checks for the core files of the intl subdirectory:
dnl   dcigettext.c
dnl   eval-plural.h
dnl   explodename.c
dnl   finddomain.c
dnl   gettextP.h
dnl   gmo.h
dnl   hash-string.h hash-string.c
dnl   l10nflist.c
dnl   libgnuintl.h.in (except the *printf stuff)
dnl   loadinfo.h
dnl   loadmsgcat.c
dnl   localealias.c
dnl   log.c
dnl   plural-exp.h plural-exp.c
dnl   plural.y
dnl Used by libglocale.
AC_DEFUN([gt_INTL_SUBDIR_CORE],
[
  AC_REQUIRE([AC_C_INLINE])dnl
  AC_REQUIRE([AC_TYPE_SIZE_T])dnl
  AC_REQUIRE([gl_AC_HEADER_STDINT_H])
  AC_REQUIRE([AC_FUNC_ALLOCA])dnl
  AC_REQUIRE([AC_FUNC_MMAP])dnl
  AC_REQUIRE([gt_INTDIV0])dnl
  AC_REQUIRE([gl_AC_TYPE_UINTMAX_T])dnl
  AC_REQUIRE([gt_INTTYPES_PRI])dnl
  AC_REQUIRE([gl_LOCK])dnl

  AC_LINK_IFELSE(
    [AC_LANG_PROGRAM(
       [[int foo (int a) { a = __builtin_expect (a, 10); return a == 10 ? 0 : 1; }]],
       [[]])],
    [AC_DEFINE([HAVE_BUILTIN_EXPECT], [1],
       [Define to 1 if the compiler understands __builtin_expect.])])

  AC_CHECK_HEADERS([argz.h inttypes.h limits.h unistd.h sys/param.h])
  AC_CHECK_FUNCS([getcwd getegid geteuid getgid getuid mempcpy munmap \
    stpcpy strcasecmp strdup strtoul tsearch uselocale argz_count \
    argz_stringify argz_next __fsetlocking])

  dnl Use the *_unlocked functions only if they are declared.
  dnl (because some of them were defined without being declared in Solaris
  dnl 2.5.1 but were removed in Solaris 2.6, whereas we want binaries built
  dnl on Solaris 2.5.1 to run on Solaris 2.6).
  AC_CHECK_DECLS([feof_unlocked, fgets_unlocked], , , [#include <stdio.h>])

  AM_ICONV

  dnl intl/plural.c is generated from intl/plural.y. It requires bison,
  dnl because plural.y uses bison specific features. It requires at least
  dnl bison-1.26 because earlier versions generate a plural.c that doesn't
  dnl compile.
  dnl bison is only needed for the maintainer (who touches plural.y). But in
  dnl order to avoid separate Makefiles or --enable-maintainer-mode, we put
  dnl the rule in general Makefile. Now, some people carelessly touch the
  dnl files or have a broken "make" program, hence the plural.c rule will
  dnl sometimes fire. To avoid an error, defines BISON to ":" if it is not
  dnl present or too old.
  AC_CHECK_PROGS([INTLBISON], [bison])
  if test -z "$INTLBISON"; then
    ac_verc_fail=yes
  else
    dnl Found it, now check the version.
    AC_MSG_CHECKING([version of bison])
changequote(<<,>>)dnl
    ac_prog_version=`$INTLBISON --version 2>&1 | sed -n 's/^.*GNU Bison.* \([0-9]*\.[0-9.]*\).*$/\1/p'`
    case $ac_prog_version in
      '') ac_prog_version="v. ?.??, bad"; ac_verc_fail=yes;;
      1.2[6-9]* | 1.[3-9][0-9]* | [2-9].*)
changequote([,])dnl
         ac_prog_version="$ac_prog_version, ok"; ac_verc_fail=no;;
      *) ac_prog_version="$ac_prog_version, bad"; ac_verc_fail=yes;;
    esac
    AC_MSG_RESULT([$ac_prog_version])
  fi
  if test $ac_verc_fail = yes; then
    INTLBISON=:
  fi
])
