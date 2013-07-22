AC_INIT(util-linux,
	m4_esyscmd([tools/git-version-gen .tarball-version]),
	kzak@redhat.com)

AC_PREREQ(2.60)

AC_CONFIG_AUX_DIR(config)
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([-Wall foreign 1.10 dist-bzip2 -Wno-portability])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])],
                            [AC_SUBST([AM_DEFAULT_VERBOSITY], [1])])

AC_CONFIG_SRCDIR(mount/mount.c)

AC_PREFIX_DEFAULT([/usr])

dnl version details from <major>.<minor>[-<suffix>]
PACKAGE_VERSION_MAJOR=$(echo $PACKAGE_VERSION | awk -F. '{print $1}')
PACKAGE_VERSION_MINOR=$(echo $PACKAGE_VERSION | awk -F. '{print $2}' \
                                              | awk -F- '{print $1}')
PACKAGE_VERSION_RELEASE=0

dnl libblkid version
LIBBLKID_VERSION="$PACKAGE_VERSION_MAJOR.$PACKAGE_VERSION_MINOR.$PACKAGE_VERSION_RELEASE"
LIBBLKID_DATE="29-Aug-2011"
LIBBLKID_LT_MAJOR=1
LIBBLKID_LT_MINOR=1
LIBBLKID_LT_MICRO=0
LIBBLKID_VERSION_INFO=`expr $LIBBLKID_LT_MAJOR + $LIBBLKID_LT_MINOR`:$LIBBLKID_LT_MICRO:$LIBBLKID_LT_MINOR

dnl libuuid version
LIBUUID_VERSION="$PACKAGE_VERSION_MAJOR.$PACKAGE_VERSION_MINOR.$PACKAGE_VERSION_RELEASE"
LIBUUID_LT_MAJOR=1
LIBUUID_LT_MINOR=3
LIBUUID_LT_MICRO=0
LIBUUID_VERSION_INFO=`expr $LIBUUID_LT_MAJOR + $LIBUUID_LT_MINOR`:$LIBUUID_LT_MICRO:$LIBUUID_LT_MINOR

dnl libmount version
LIBMOUNT_VERSION="$PACKAGE_VERSION_MAJOR.$PACKAGE_VERSION_MINOR.$PACKAGE_VERSION_RELEASE"
LIBMOUNT_LT_MAJOR=1
LIBMOUNT_LT_MINOR=1
LIBMOUNT_LT_MICRO=0
LIBMOUNT_VERSION_INFO=`expr $LIBMOUNT_LT_MAJOR + $LIBMOUNT_LT_MINOR`:$LIBMOUNT_LT_MICRO:$LIBMOUNT_LT_MINOR

# Check whether exec_prefix=/usr:
case $exec_prefix:$prefix in
NONE:NONE | NONE:/usr | /usr:*)
  AC_MSG_NOTICE([Default --exec-prefix detected.])
  case $bindir in
  '${exec_prefix}/bin') bindir=/bin
    AC_MSG_NOTICE([  --bindir defaults to /bin]) ;;
  esac
  case $sbindir in
  '${exec_prefix}/sbin') sbindir=/sbin
    AC_MSG_NOTICE([  --sbindir defaults to /sbin]) ;;
  esac
  case $libdir in
  '${exec_prefix}/lib') libdir=/lib
    AC_MSG_NOTICE([  --libdir defaults to /lib]) ;;
  esac ;;
esac

libdirname=`basename "$libdir"`
AC_SUBST([libdirname])

# The original default values of {bin,sbin,lib}dir
usrbin_execdir='${exec_prefix}/bin'
AC_SUBST([usrbin_execdir])

usrsbin_execdir='${exec_prefix}/sbin'
AC_SUBST([usrsbin_execdir])

usrlib_execdir='${exec_prefix}/'$libdirname
AC_SUBST([usrlib_execdir])

AM_PROG_CC_C_O
AC_PROG_CC_STDC
AC_GNU_SOURCE
AC_CANONICAL_HOST
AC_C_CONST
AC_C_VOLATILE
AC_C_BIGENDIAN

dnl libtool-2
LT_INIT

m4_ifndef([PKG_PROG_PKG_CONFIG],
  [m4_fatal([Could not locate the pkg-config autoconf
    macros. These are usually located in /usr/share/aclocal/pkg.m4.
    If your macros are in a different location, try setting the
    environment variable AL_OPTS="-I/other/macro/dir" before running
    ./autogen.sh or autoreconf again.])])
PKG_PROG_PKG_CONFIG

GTK_DOC_CHECK([1.10])
AC_PATH_PROG([XSLTPROC], [xsltproc])

linux_os=no
case ${host_os} in
  *linux*)
    linux_os=yes
    ;;
esac
AM_CONDITIONAL([LINUX], test "x$linux_os" = xyes)

AC_PATH_PROG(PERL, perl)

AC_SYS_LARGEFILE

AC_CHECK_HEADERS([linux/compiler.h linux/blkpg.h linux/major.h], [], [], [
#ifdef HAVE_LINUX_COMPILER_H
#include <linux/compiler.h>
#endif
])
AC_CHECK_HEADERS(
	[err.h \
	errno.h \
	getopt.h \
	linux/fd.h \
	linux/tiocl.h \
	linux/version.h \
	linux/falloc.h \
	linux/cdrom.h \
	fcntl.h \
	locale.h \
	stdint.h \
	inttypes.h \
	paths.h \
	pty.h \
	mntent.h \
        net/if.h \
	net/if_dl.h \
        netinet/in.h \
	rpcsvc/nfs_prot.h \
	stdlib.h \
	asm/io.h \
	sys/disk.h \
	sys/disklabel.h \
	sys/ioctl.h \
	sys/ioccom.h \
	sys/io.h \
	sys/time.h \
	sys/mkdev.h \
	sys/prctl.h \
	sys/queue.h \
	sys/sockio.h \
	sys/socket.h \
	sys/syscall.h \
	sys/file.h \
	sys/ioctl.h \
	sys/stat.h \
	sys/types.h \
	sys/un.h \
	sys/user.h \
	sys/resource.h \
	unistd.h ])

AC_CHECK_HEADERS([linux/raw.h],
		 [AM_CONDITIONAL([HAVE_RAW], [true])],
		 [AM_CONDITIONAL([HAVE_RAW], [false])])

AC_CHECK_HEADERS([langinfo.h],
		[AM_CONDITIONAL([HAVE_LANGINFO], [true])],
		[AM_CONDITIONAL([HAVE_LANGINFO], [false])])


AC_CHECK_DECLS([_NL_TIME_WEEK_1STDAY],[],[],[[#include <langinfo.h>]])

AC_CHECK_DECL([llseek],
	[AC_DEFINE(HAVE_LLSEEK_PROTOTYPE, 1,
			[Define to 1 if have llseek prototype])],
	[],
	[#include <unistd.h>])

AC_CHECK_DECL([lseek64],
	[AC_DEFINE(HAVE_LSEEK64_PROTOTYPE, 1,
			[Define to 1 if have lseek64 prototype])],
	[],
	[#define _LARGEFILE_SOURCE
	 #define _LARGEFILE64_SOURCE
	 #include <unistd.h>])

AC_CHECK_FUNCS(
	[inet_aton \
	err \
	errx \
	futimens \
	fstat64 \
	fsync \
	getdomainname \
	get_current_dir_name \
	usleep \
	nanosleep \
	personality \
	updwtmp \
	jrand48 \
	lchown \
	llseek \
	lseek64 \
	strtoull \
	sysconf \
	getdtablesize \
	getexecname \
	getrlimit \
	sigqueue \
	srandom \
	setresgid \
	setresuid \
	strndup \
	strnlen \
	strnchr \
	inotify_init \
	prctl \
	posix_fadvise \
	getmntinfo \
	__secure_getenv \
	warn \
	warnx \
	rpmatch])
AC_FUNC_FSEEKO

AC_CHECK_FUNCS([openat fstatat unlinkat], [have_openat=yes], [have_openat=no])

AC_CHECK_FUNCS([ioperm iopl], [have_io=yes])
AM_CONDITIONAL([BUILD_HWCLOCK], test "x$have_io" = xyes -o "x$linux_os" = xyes)

AC_CHECK_MEMBER(struct sockaddr.sa_len,
		AC_DEFINE_UNQUOTED(HAVE_SA_LEN,1,[Define if struct sockaddr contains sa_len]),,
	[#include <sys/types.h>
	 #include <sys/socket.h>])

SOCKET_LIBS=
AC_SEARCH_LIBS([gethostbyname], [nsl],
	[if test x"$ac_cv_search_gethostbyname" != x"none required"; then
		SOCKET_LIBS="$SOCKET_LIBS -lnsl";
	 fi])
AC_SEARCH_LIBS([socket], [socket],
	[if test x"$ac_cv_search_socket" != x"none required"; then
		SOCKET_LIBS="$SOCKET_LIBS -lsocket";
	fi])
AC_SUBST([SOCKET_LIBS])


have_dirfd=no
AC_CHECK_FUNCS([dirfd], [have_dirfd=yes], [have_dirfd=no])
if test x"$have_dirfd" = xno ; then
	AC_CHECK_DECLS([dirfd],
		[have_dirfd=yes], [have_dirfd=no],
		[#include <sys/types.h>
		 #include <dirent.h>])
fi

have_ddfd=no
if test x"$have_dirfd" = xno ; then
	AC_CHECK_MEMBERS([DIR.dd_fd],
		[have_ddfd=yes], [have_ddfd=no],
		[#include <sys/types.h>
		 #include <dirent.h>])
fi

case "$have_dirfd:$have_ddfd" in
no:no)
  AC_MSG_ERROR([cannot find a method to get filedescriptor of directory]) ;;
esac


AC_MSG_CHECKING(whether program_invocation_short_name is defined)
AC_TRY_COMPILE([#include <argp.h>],
               [program_invocation_short_name = "test";],
               AC_DEFINE(HAVE_PROGRAM_INVOCATION_SHORT_NAME, 1,
                         [Define if program_invocation_short_name is defined])
               AC_MSG_RESULT(yes),
               AC_MSG_RESULT(no))

AC_MSG_CHECKING([whether __progname is defined])
AC_LINK_IFELSE([AC_LANG_PROGRAM([extern char *__progname;],
           [if (*__progname == 0) return;])],
           AC_DEFINE(HAVE___PROGNAME, 1, [Define if __progname is defined])
           AC_MSG_RESULT(yes),
           AC_MSG_RESULT(no))

dnl Static compilation
m4_define([UTIL_STATIC_PROGRAMS], [losetup, mount, umount, fdisk, sfdisk, blkid])

AC_ARG_ENABLE([static-programs],
  [AS_HELP_STRING([--enable-static-programs=LIST],
      [link static the programs in LIST (comma-separated,
      supported for ]m4_defn([UTIL_STATIC_PROGRAMS])[)])])

case $enable_static_programs in
yes) enable_static_programs=m4_quote(UTIL_STATIC_PROGRAMS) ;;
no) enable_static_programs= ;;
esac

dnl Set all the individual AM_CONDITIONALs
m4_foreach([UTIL_PRG], m4_defn([UTIL_STATIC_PROGRAMS]), [
  case ,$enable_static_programs, in
   *,UTIL_PRG,*) static_[]UTIL_PRG=yes ;;
  esac
  AM_CONDITIONAL([HAVE_STATIC_]m4_toupper(UTIL_PRG),
    [test "x$static_[]UTIL_PRG" = xyes])
])

dnl UTIL_PKG_STATIC(VARIABLE, MODULES)
dnl ----------------------------------
AC_DEFUN([UTIL_PKG_STATIC], [
  if AC_RUN_LOG([pkg-config --exists --print-errors "$2"]); then
    $1=`pkg-config --libs --static "$2"`
  else
    AC_MSG_ERROR([pkg-config description of $2, needed for static build, is not available])
  fi
])

dnl UTIL_CHECK_LIB(LIBRARY, FUNCTION, [VARSUFFIX = $1]))
dnl The VARSUFFIX is optional and overrides the default behaviour. For example:
dnl     UTIL_CHECK_LIB(yyy, func, xxx) generates have_xxx and HAVE_LIBXXX
dnl     UTIL_CHECK_LIB(yyy, func)      generates have_yyy and HAVE_LIBYYY
dnl ---------------------------------
AC_DEFUN([UTIL_CHECK_LIB], [
  m4_define([suffix], m4_default([$3],$1))
  [have_]suffix=yes
  m4_ifdef([$3],
    [AC_CHECK_LIB([$1], [$2], [AC_DEFINE(AS_TR_CPP([HAVE_LIB]suffix), 1)], [[have_]suffix=no])],
    [AC_CHECK_LIB([$1], [$2], [], [[have_]suffix=no])])
  AM_CONDITIONAL(AS_TR_CPP([HAVE_]suffix), [test [$have_]suffix = yes])
])

dnl UTIL_SET_FLAGS(CFLAGS, CPPFLAGS, LDFLAGS)
AC_DEFUN([UTIL_SET_FLAGS], [
  old_CFLAGS="$CFLAGS"
  old_CPPFLAGS="$CPPFLAGS"
  old_LDFLAGS="$LDFLAGS"
  CFLAGS="$CFLAGS $1"
  CPPFLAGS="$CPPFLAGS $2"
  LDFLAGS="$LDFLAGS $3"
])

dnl UTIL_RESTORE_FLAGS()
AC_DEFUN([UTIL_RESTORE_FLAGS], [
  CFLAGS="$old_CFLAGS"
  CPPFLAGS="$old_CPPFLAGS"
  LDFLAGS="$old_LDFLAGS"
])


AX_CHECK_TLS

AC_ARG_ENABLE([mount],
  AS_HELP_STRING([--disable-mount], [do not build mount utilities]),
  [], enable_mount=check
)
build_mount=yes
if test "x$enable_mount" = xcheck; then
  if test "x$linux_os" = xno; then
    AC_MSG_WARN([non-linux system; do not build mount utilities])
    build_mount=no
  fi
elif test "x$enable_mount" = xno; then
    build_mount=no
fi
AM_CONDITIONAL(BUILD_MOUNT, test "x$build_mount" = xyes)


AC_ARG_ENABLE([fsck],
  AS_HELP_STRING([--disable-fsck], [do not build fsck]),
  [], enable_fsck=yes
)
AM_CONDITIONAL(BUILD_FSCK, test "x$enable_fsck" = xyes)


AC_ARG_ENABLE([partx],
  AS_HELP_STRING([--disable-partx], [do not build addpart, delpart, partx]),
  [], enable_partx=check
)
build_partx=yes
if test "x$enable_partx" = xcheck; then
  if test "x$linux_os" = xno; then
    AC_MSG_WARN([non-linux system; do not build partx utilities])
    build_partx=no
  fi
elif test "x$enable_partx" = xno; then
    build_partx=no
fi
AM_CONDITIONAL(BUILD_PARTX, test "x$build_partx" = xyes)


AC_ARG_ENABLE([libuuid],
  AS_HELP_STRING([--disable-libuuid], [do not build libuuid and uuid utilities]),
  [], enable_libuuid=yes
)
AC_SUBST([LIBUUID_VERSION])
AC_SUBST([LIBUUID_VERSION_INFO])
AM_CONDITIONAL(BUILD_LIBUUID, test "x$enable_libuuid" = xyes)

if test "x$enable_libuuid" = xyes; then
  AC_DEFINE(HAVE_LIBUUID, 1, [Define to 1 if you have the -luuid.])
else
  AC_MSG_WARN([uuid library is not found; mkswap(8) will not generate UUIDs])
fi


AC_ARG_ENABLE([uuidd],
  AS_HELP_STRING([--disable-uuidd], [do not build the uuid daemon]),
  [], enable_uuidd=auto
)

case "$enable_uuidd:$enable_libuuid" in
yes:no)
  AC_MSG_ERROR([cannot enable uuidd when libuuid is disabled]) ;;
auto:*)
  enable_uuidd=$enable_libuuid ;;
esac
if test "x$enable_uuidd" = xyes; then
  AC_DEFINE(HAVE_UUIDD, 1, [Define to 1 if you want to use uuid daemon.])
fi
AM_CONDITIONAL(BUILD_UUIDD, test "x$enable_uuidd" = xyes)


AC_ARG_ENABLE([libblkid],
  AS_HELP_STRING([--disable-libblkid], [do not build libblkid and blkid utilities]),
  [], enable_libblkid=yes
)
AC_SUBST([LIBBLKID_DATE])
AC_SUBST([LIBBLKID_VERSION])
AC_SUBST([LIBBLKID_VERSION_INFO])
AM_CONDITIONAL(BUILD_LIBBLKID, test "x$enable_libblkid" = xyes)
AC_DEFINE_UNQUOTED(LIBBLKID_VERSION, "$LIBBLKID_VERSION", [libblkid version string])
AC_DEFINE_UNQUOTED(LIBBLKID_DATE, "$LIBBLKID_DATE", [libblkid date string])

if test "x$enable_libblkid" = xyes; then
  AC_DEFINE(HAVE_LIBBLKID, 1, [Define to 1 if you have the -lblkid.])
else
  if test "x$build_mount" = xyes; then
    AC_MSG_ERROR([libblkid is needed to build util-linux mount])
  fi
  if test "x$enable_fsck" = xyes; then
    AC_MSG_ERROR([libblkid is needed to build util-linux fsck])
  fi
  if test "x$build_partx" = xyes; then
    AC_MSG_ERROR([libblkid is needed to build util-linux partx])
  fi
fi


AC_ARG_ENABLE([libmount],
  AS_HELP_STRING([--disable-libmount], [do not build libmount]),
  [], enable_libmount=check
)
build_libmount=yes
if test "x$enable_libmount" = xcheck; then
  if test "x$linux_os" = xno; then
    AC_MSG_WARN([non-linux system; do not build libmount])
    build_libmount=no
  fi
elif test "x$enable_libmount" = xno; then
    build_libmount=no
fi

case "$enable_libblkid:$build_libmount" in
no:yes)
  AC_MSG_ERROR([cannot enable libmount when libblkid is disabled]) ;;
esac

AC_SUBST([LIBMOUNT_VERSION])
AC_SUBST([LIBMOUNT_VERSION_INFO])
AC_DEFINE_UNQUOTED(LIBMOUNT_VERSION, "$LIBMOUNT_VERSION", [libmount version string])
AM_CONDITIONAL(BUILD_LIBMOUNT, test "x$build_libmount" = xyes)


AC_ARG_ENABLE([libmount-mount],
  AS_HELP_STRING([--enable-libmount-mount], [link mount(8) with libmount (EXPERIMENTAL)]),
  [], enable_libmount_mount=no
)

case "$build_libmount:$enable_libmount_mount" in
no:yes)
  AC_MSG_ERROR([cannot link mount(8) with libmount when libmount is disabled]) ;;
yes:yes)
  AC_DEFINE(HAVE_LIBMOUNT_MOUNT, 1, [use libmount for mount(8)])
esac
AM_CONDITIONAL(BUILD_LIBMOUNT_MOUNT, test "x$enable_libmount_mount" = xyes)


AC_ARG_ENABLE([mountpoint],
  AS_HELP_STRING([--disable-mountpoint], [do not build mountpoint]),
  [], enable_mountpoint=check
)

build_mountpoint=yes
if test "x$enable_mountpoint" = xcheck; then
  if test "x$build_libmount" = xno; then
    AC_MSG_WARN([libmount disabled; do not build mountpoint])
    build_mountpoint=no
  fi
elif test "x$enable_mountpoint" = xno; then
    build_mountpoint=no
fi

case "$build_libmount:$build_mountpoint" in
no:yes)
  AC_MSG_ERROR([cannot enable mountpoint when libmount is disabled]) ;;
esac
AM_CONDITIONAL(BUILD_MOUNTPOINT, test "x$build_mountpoint" = xyes)


UTIL_CHECK_LIB(util, openpty)
UTIL_CHECK_LIB(termcap, tgetnum)

AM_GNU_GETTEXT_VERSION([0.14.1])
AM_GNU_GETTEXT([external])
if test -d "$srcdir/po"
then
    ALL_LINGUAS=`cd $srcdir/po > /dev/null && echo *.po | sed 's/\.po//g'`
else
    ALL_LINGUAS="af am ar as be bg bn_IN bn ca cs cy da de el en_GB es et eu_ES fa fi fr gl gu he hi hr hu hy id is it ja ka kn ko ku lo lt lv mk ml mr ms my nb nl nn no nso or pa pl pt_BR pt ro ru si sk sl sq sr@Latn sr sv ta te th tr uk ur vi zh_CN zh_TW zu"
fi

AC_ARG_WITH([ncurses],
  AS_HELP_STRING([--with-ncurses], [build with non-wide ncurses, default is wide version
                                    (--without-ncurses disables all ncurses(w) support)]),
  [], with_ncurses=auto
)
AM_CONDITIONAL(HAVE_NCURSES, false)

if test "x$with_ncurses" != xno; then
  have_ncurses=no
  AC_CHECK_HEADERS([ncurses.h ncurses/ncurses.h], [
    if test "x$with_ncurses" = xauto; then
      UTIL_CHECK_LIB(ncursesw, initscr, ncurses)
      if test "x$have_ncurses" = xyes; then
        AC_CHECK_HEADERS([ncursesw/ncurses.h])
        NCURSES_LIBS="-lncursesw"
      fi
    fi
    if test "x$have_ncurses" = xno; then
      UTIL_CHECK_LIB(ncurses, initscr)
      if test "x$have_ncurses" = xyes; then
        NCURSES_LIBS="-lncurses"
      fi
    fi
  ])
  if test "x$have_ncurses" = xno; then
    AC_MSG_ERROR([ncurses or ncursesw selected, but library not found (--without-ncurses to disable)])
  fi
fi
AC_SUBST([NCURSES_LIBS])


AC_ARG_WITH([slang],
  AS_HELP_STRING([--with-slang], [compile cfdisk with slang]),
  [], with_slang=no
)

have_tinfo=no
AC_CHECK_LIB(tinfo, tgetent, [have_tinfo=yes])
AM_CONDITIONAL(HAVE_TINFO, test "x$have_tinfo" = xyes)

use_slang=no
if test "x$with_slang" = xyes; then
  AC_CHECK_HEADERS([slang.h slang/slang.h])
  AC_CHECK_HEADERS([slcurses.h slang/slcurses.h],
                   [use_slang=yes], [], [
#ifdef HAVE_SLANG_H
#include <slang.h>
#elif defined(HAVE_SLANG_SLANG_H)
#include <slang/slang.h>
#endif
])
  if test "x$use_slang" = xno; then
    AC_MSG_ERROR([slang selected but slcurses.h not found])
  fi
fi
AM_CONDITIONAL(USE_SLANG, test "x$use_slang" = xyes)


AC_ARG_WITH([utempter],
  AS_HELP_STRING([--with-utempter], [compile script(1) with libutempter]),
  [], with_utempter=no
)

if test "x$with_utempter" = xyes; then
  UTIL_CHECK_LIB(utempter, utempter_add_record)
  if test "x$have_utempter" = xno; then
    AC_MSG_ERROR([utempter selected but libutempter not found])
  fi
else
  AM_CONDITIONAL(HAVE_UTEMPTER, false)
fi

# on Solaris, you can't mix and match standards, since we use c99
# aparently at this stage, XOPEN_SOURCE will conflict.  As workaround,
# check for crypt.h and use that without XOPEN_SOURCE.
AC_CHECK_HEADERS([crypt.h])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#else
#define _XOPEN_SOURCE
#include <unistd.h>
#endif
]], [[
char *c = crypt("abc","pw");
]])],[],[
  LIBS="$LIBS -lcrypt"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
  #ifdef HAVE_CRYPT_H
  #include <crypt.h>
  #else
  #define _XOPEN_SOURCE
  #include <unistd.h>
  #endif
  ]], [[
  char *c = crypt("abc","pw");
  ]])],[
  AC_DEFINE(NEED_LIBCRYPT, 1, [Do we need -lcrypt?])
  need_libcrypt=yes
  ],[
  AC_MSG_ERROR([crypt() is not available])
  ])
])

AM_CONDITIONAL(NEED_LIBCRYPT, test "x$need_libcrypt" = xyes)

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <stdio.h>
]], [[
printf(__progname);
]])],
[AC_DEFINE(HAVE___PROGNAME, 1, Do we have __progname?)
])


AC_CHECK_TYPES([union semun], [], [], [[
#include <sys/sem.h>
]])
AC_CHECK_TYPES(loff_t)


AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <wchar.h>
#include <wctype.h>
#include <stdio.h>
]], [[
  wchar_t wc;
  wint_t w;
  w = fgetwc(stdin);
  if (w == WEOF) exit(1);
  wc = w;
  fputwc(wc,stdout);
]])],
[AC_DEFINE(HAVE_WIDECHAR,1,Do we have wide character support?)
])


dnl UTIL_CHECK_SYSCALL(SYSCALL, FALLBACK, ...)
dnl Only specify FALLBACK if the SYSCALL
dnl you're checking for is a "newish" one
dnl -------------------------------------
AC_DEFUN([UTIL_CHECK_SYSCALL], [
  dnl This macro uses host_cpu.
  AC_REQUIRE([AC_CANONICAL_HOST])
  AC_CACHE_CHECK([for syscall $1],
    [util_cv_syscall_$1],
    [_UTIL_SYSCALL_CHECK_DECL([SYS_$1],
      [syscall=SYS_$1],
      [dnl Our libc failed use, so see if we can get the kernel
      dnl headers to play ball ...
      _UTIL_SYSCALL_CHECK_DECL([_NR_$1],
	[syscall=_NR_$1],
	[
	  syscall=no
	  if test "x$linux_os" = xyes; then
	    case $host_cpu in
	      _UTIL_CHECK_SYSCALL_FALLBACK(m4_shift($@))
	    esac
	  fi
        ])
      ])
    util_cv_syscall_$1=$syscall
    ])
  AM_CONDITIONAL([HAVE_]m4_toupper($1), [test "x$util_cv_syscall_$1" != xno])
  case $util_cv_syscall_$1 in #(
  no) AC_MSG_WARN([Unable to detect syscall $1.]) ;;
  SYS_*) ;;
  *) AC_DEFINE_UNQUOTED([SYS_$1], [$util_cv_syscall_$1],
	[Fallback syscall number for $1]) ;;
  esac
])

dnl _UTIL_SYSCALL_CHECK_DECL(SYMBOL, ACTION-IF-FOUND, ACTION-IF-NOT-FOUND)
dnl Check if SYMBOL is declared, using the headers needed for syscall checks.
dnl -------------------------------------
m4_define([_UTIL_SYSCALL_CHECK_DECL],
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/syscall.h>
#include <unistd.h>
]], [[int test = $1;]])],
[$2], [$3])
])

dnl _UTIL_CHECK_SYSCALL_FALLBACK(PATTERN, VALUE, ...)
dnl Helper macro to create the body for the above `case'.
dnl -------------------------------------
m4_define([_UTIL_CHECK_SYSCALL_FALLBACK],
[m4_ifval([$1],
  [#(
  $1) syscall="$2" ;;dnl
  _UTIL_CHECK_SYSCALL_FALLBACK(m4_shiftn(2, $@))])dnl
])


UTIL_CHECK_SYSCALL([pivot_root])
UTIL_CHECK_SYSCALL([sched_getaffinity])
UTIL_CHECK_SYSCALL([ioprio_set],
  [alpha],    [442],
  [i*86],     [289],
  [ia64*],    [1274],
  [powerpc*], [273],
  [s390*],    [282],
  [sparc*],   [196],
  [sh*],      [288],
  [x86_64*],  [251])
UTIL_CHECK_SYSCALL([ioprio_get],
  [alpha],    [443],
  [i*86],     [290],
  [ia64*],    [1275],
  [powerpc*], [274],
  [s390*],    [283],
  [sparc*],   [218],
  [sh*],      [289],
  [x86_64*],  [252])

dnl fallocate could be available as libc function or as syscall only
UTIL_CHECK_SYSCALL([fallocate])

dnl check for valid fallocate() function
dnl with 32 bits glibc 2.10, fallocate() exists but not fallocate64()
dnl when _FILE_OFFSET_BITS==64, fallocate() is redirect to fallocate64()
dnl and program can't be linked.
dnl AC_CHECK_FUNC can't catch such errors since it's redefining
dnl function prototype.
AC_MSG_CHECKING([for valid fallocate() function])
AC_LINK_IFELSE([
AC_LANG_PROGRAM([[
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_LINUX_FALLOC_H
# include <linux/falloc.h>
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
]],[[
   long ret;

   ret = fallocate(0, FALLOC_FL_KEEP_SIZE, 0xfffffffful, 0xfffffffful);

   if (ret != 0) {
      return 1;
   }
   ]])],[
AC_MSG_RESULT([yes])
AC_DEFINE(HAVE_FALLOCATE,1,[Have valid fallocate() function])],[
AC_MSG_RESULT([no])])

dnl unshare could be available as libc function or as syscall only
UTIL_CHECK_SYSCALL([unshare])
AC_CHECK_FUNCS([unshare])

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <time.h>
#include <unistd.h>
]], [[
    int a = 0;
    struct tm *tm = localtime(0);
    if (a == -1)  /* false */
        sleep(tm->tm_gmtoff);
]])],
[AC_DEFINE(HAVE_TM_GMTOFF,1,[Does struct tm have a field tm_gmtoff?])
])

AC_CHECK_MEMBERS([struct termios.c_line],,,
    [[#include <termios.h>]])

AC_CHECK_MEMBERS([struct stat.st_mtim.tv_nsec],,,
	[#include <sys/stat.h>])

AC_CHECK_DECLS([
 UNAME26,
 ADDR_NO_RANDOMIZE,
 FDPIC_FUNCPTRS,
 MMAP_PAGE_ZERO,
 ADDR_COMPAT_LAYOUT,
 READ_IMPLIES_EXEC,
 ADDR_LIMIT_32BIT,
 WHOLE_SECONDS,
 STICKY_TIMEOUTS,
 ADDR_LIMIT_3GB], [], [], [#include <linux/personality.h>])

AC_CHECK_HEADERS([sys/swap.h])

AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
[[
#ifdef HAVE_SYS_SWAP_H
# include <sys/swap.h>
#endif
#include <unistd.h>
]],
[[swapon("/dev/null", 0);]])],
[AC_DEFINE(SWAPON_HAS_TWO_ARGS, 1, [Is swapon() declared with two parameters?])
],
[AC_MSG_NOTICE([Your libc thinks that swapon has 1 arg only.])
])


AC_CHECK_TYPES([cpu_set_t], [have_cpu_set_t=yes], [], [[
#include <sched.h>
]])

AM_CONDITIONAL(HAVE_CPU_SET_T, [test "x$have_cpu_set_t" = xyes])

AC_CHECK_DECLS([CPU_ALLOC], [], [], [[
#include <sched.h>
]])


dnl UTIL_SET_ARCH(ARCHNAME, PATTERN)
dnl ---------------------------------
AC_DEFUN([UTIL_SET_ARCH], [
  cpu_$1=false
  case "$host" in
   $2) cpu_$1=true ;;
  esac
  AM_CONDITIONAL(AS_TR_CPP(ARCH_$1), [test "x$cpu_$1" = xtrue])
])

UTIL_SET_ARCH(I86, i?86-*)
UTIL_SET_ARCH(86_64, x86_64*)
UTIL_SET_ARCH(IA64, ia64*)
UTIL_SET_ARCH(S390, s390*)
UTIL_SET_ARCH(SPARC, sparc*)
UTIL_SET_ARCH(PPC, ppc*|powerpc*)
UTIL_SET_ARCH(M68K, m68*)
UTIL_SET_ARCH(MIPS, mips*)
UTIL_SET_ARCH(HPPA, hppa*)

AC_ARG_ENABLE([arch],
  AS_HELP_STRING([--enable-arch], [do build arch]),
  [], enable_arch=no
)
AM_CONDITIONAL(BUILD_ARCH, test "x$enable_arch" = xyes)

AC_ARG_ENABLE([ddate],
  AS_HELP_STRING([--enable-ddate], [do build ddate]),
  [], enable_date=no
)
AM_CONDITIONAL(BUILD_DDATE, test "x$enable_ddate" = xyes)

AC_ARG_ENABLE([agetty],
  AS_HELP_STRING([--disable-agetty], [do not build agetty]),
  [], enable_agetty=yes
)
AM_CONDITIONAL(BUILD_AGETTY, test "x$enable_agetty" = xyes)

AC_ARG_ENABLE([cramfs],
  AS_HELP_STRING([--disable-cramfs], [do not build fsck.cramfs, mkfs.cramfs]),
  [], enable_cramfs=check
)

if test "x$enable_cramfs" = xno; then
  build_cramfs=no
else
  build_cramfs=yes
  dnl Trick: leave the third parameter empty to get the default action.
  AC_CHECK_LIB(z, crc32, [], build_cramfs=no)
  case $enable_cramfs:$build_cramfs in
  yes:no) AC_MSG_ERROR([cramfs selected but libz not found]);;
  esac
fi
AM_CONDITIONAL(BUILD_CRAMFS, test "x$build_cramfs" = xyes)

build_lsblk=yes
if test "x$have_openat" = xno; then
  AC_MSG_WARN([openat() function not found; do not build lsblk])
  build_lsblk=no
elif test "x$linux_os" = xno; then
  AC_MSG_WARN([non-linux system; do not build lsblk])
  build_lsblk=no
fi
AM_CONDITIONAL(BUILD_LSBLK, test "x$build_lsblk" = xyes)

AC_ARG_ENABLE([switch_root],
  AS_HELP_STRING([--disable-switch_root], [do not build switch_root]),
  [], enable_switch_root=check
)
if test "x$enable_switch_root" = xno; then
  build_switch_root=no
else
  build_switch_root=yes
  case $enable_switch_root:$linux_os in
  yes:no) AC_MSG_ERROR([switch_root selected for non-linux system]);;
  check:no) AC_MSG_WARN([non-linux system; do not build switch_root])
            build_switch_root=no;;
  esac
  if test "x$build_switch_root" = xyes; then
    case $enable_switch_root:$have_openat in
    yes:no) AC_MSG_ERROR([switch_root selected but openat() function not found]);;
    check:no) AC_MSG_WARN([openat() function not found; do not build switch_root])
              build_switch_root=no;;
    esac
  fi
fi
AM_CONDITIONAL(BUILD_SWITCH_ROOT, test "x$build_switch_root" = xyes)


AC_ARG_ENABLE([pivot_root],
  AS_HELP_STRING([--disable-pivot_root], [do not build pivot_root]),
  [], enable_pivot_root=check
)
if test "x$enable_pivot_root" = xno; then
  build_pivot_root=no
else
  build_pivot_root=yes
  case $enable_pivot_root:$linux_os in
  yes:no) AC_MSG_ERROR([pivot_root selected for non-linux system]);;
  check:no) AC_MSG_WARN([non-linux system; do not build pivot_root])
            build_pivot_root=no;;
  esac
  if test "x$build_pivot_root" = xyes; then
    case $enable_pivot_root:$util_cv_syscall_pivot_root in
    yes:no) AC_MSG_ERROR([pivot_root selected but pivot_root syscall not found]);;
    check:no) AC_MSG_WARN([pivot_root syscall not found; do not build pivot_root])
              build_pivot_root=no;;
    esac
  fi
fi
AM_CONDITIONAL(BUILD_PIVOT_ROOT, test "x$build_pivot_root" = xyes)


AC_ARG_ENABLE([fallocate],
  AS_HELP_STRING([--disable-fallocate], [do not build fallocate]),
  [], enable_fallocate=check
)
if test "x$enable_fallocate" = xno; then
  build_fallocate=no
else
  build_fallocate=yes
  case $enable_fallocate:$linux_os in
  yes:no) AC_MSG_ERROR([fallocate selected for non-linux system]);;
  check:no) AC_MSG_WARN([non-linux system; do not build fallocate])
            build_fallocate=no;;
  esac
  if test "x$build_fallocate" = xyes; then
    case $enable_fallocate:$util_cv_syscall_fallocate in
    yes:no) AC_MSG_ERROR([fallocate selected but fallocate syscall not found]);;
    check:no) AC_MSG_WARN([fallocate syscall not found; do not build fallocate])
              build_fallocate=no;;
    esac
  fi
fi
AM_CONDITIONAL(BUILD_FALLOCATE, test "x$build_fallocate" = xyes)


AC_ARG_ENABLE([unshare],
  AS_HELP_STRING([--disable-unshare], [do not build unshare]),
  [], enable_unshare=check
)
if test "x$enable_unshare" = xno; then
  build_unshare=no
else
  build_unshare=yes
  case $enable_unshare:$linux_os in
  yes:no) AC_MSG_ERROR([unshare selected for non-linux system]);;
  check:no) AC_MSG_WARN([non-linux system; do not build unshare])
            build_unshare=no;;
  esac
  if test "x$build_unshare" = xyes; then
    case $enable_unshare:$util_cv_syscall_unshare in
    yes:no) AC_MSG_ERROR([unshare selected but unshare syscall not found]);;
    check:no) AC_MSG_WARN([unshare syscall not found; do not build unshare])
              build_unshare=no;;
    esac
  fi
fi
AM_CONDITIONAL(BUILD_UNSHARE, test "x$build_unshare" = xyes)



AC_ARG_ENABLE([elvtune],
  AS_HELP_STRING([--enable-elvtune], [build elvtune (only works with 2.2 and 2.4 kernels)]),
  [], enable_elvtune=no
)
AM_CONDITIONAL(BUILD_ELVTUNE, test "x$enable_elvtune" = xyes)

AC_ARG_ENABLE([kill],
  AS_HELP_STRING([--enable-kill], [build kill]),
  [], enable_kill=no
)
AM_CONDITIONAL(BUILD_KILL, test "x$enable_kill" = xyes)


AC_ARG_ENABLE([last],
  AS_HELP_STRING([--enable-last], [build last]),
  [], enable_last=no
)
AM_CONDITIONAL(BUILD_LAST, test "x$enable_last" = xyes)

AC_ARG_ENABLE([line],
  AS_HELP_STRING([--enable-line], [build line]),
  [], enable_line=no
)
AM_CONDITIONAL(BUILD_LINE, test "x$enable_line" = xyes)

AC_ARG_ENABLE([mesg],
  AS_HELP_STRING([--enable-mesg], [build mesg]),
  [], enable_mesg=no
)
AM_CONDITIONAL(BUILD_MESG, test "x$enable_mesg" = xyes)


AC_ARG_ENABLE([raw],
  AS_HELP_STRING([--enable-raw], [build raw]),
  [], enable_raw=no
)
AM_CONDITIONAL(BUILD_RAW, test "x$enable_raw" = xyes)


AC_ARG_ENABLE([rename],
  AS_HELP_STRING([--disable-rename], [do not build rename]),
  [], enable_rename=yes
)
AM_CONDITIONAL(BUILD_RENAME, test "x$enable_rename" = xyes)


AC_ARG_ENABLE([reset],
  AS_HELP_STRING([--enable-reset], [build reset]),
  [], enable_reset=no
)
AM_CONDITIONAL(BUILD_RESET, test "x$enable_reset" = xyes)


AC_ARG_ENABLE([login-utils],
  AS_HELP_STRING([--enable-login-utils], [build chfn, chsh, login, newgrp, vipw]),
  [], enable_login_utils=no
)
AM_CONDITIONAL(BUILD_LOGIN_UTILS, test "x$enable_login_utils" = xyes)

AC_ARG_WITH([pam],
  [AS_HELP_STRING([--without-pam], [compile login-utils without PAM support])])

AM_CONDITIONAL(HAVE_PAM, false)
if test "x$enable_login_utils" = xyes && test "x$with_pam" != xno; then
  AC_CHECK_HEADERS([security/pam_misc.h],
    [AM_CONDITIONAL(HAVE_PAM, true)],
    [if test "x$with_pam" = xyes; then
	AC_MSG_ERROR([PAM selected but security/pam_misc.h not found])
     fi
    ])
fi

AC_ARG_WITH([selinux],
  AS_HELP_STRING([--with-selinux], [compile with SELinux support]),
  [], with_selinux=no
)

if test "x$with_selinux" = xno; then
  AM_CONDITIONAL(HAVE_SELINUX, false)
else
  UTIL_CHECK_LIB(selinux, getprevcon)
  case "$with_selinux:$have_selinux" in
  yes:no) AC_MSG_ERROR([SELinux selected but libselinux not found]);;
  esac
fi

if test "x$have_selinux" = xyes; then
  SELINUX_LIBS="-lselinux -lsepol"
  SELINUX_LIBS_STATIC="-lselinux -lsepol"
  old_LDFLAGS="$LDFLAGS"
  LDFLAGS="$LDFLAGS $SELINUX_LIBS"
  # This function is missing in old libselinux 1.xx versions
  AC_CHECK_FUNCS([security_get_initial_context])
  LDFLAGS="$old_LDFLAGS"
fi
AC_SUBST([SELINUX_LIBS])
AC_SUBST([SELINUX_LIBS_STATIC])

AC_ARG_WITH([audit],
  AS_HELP_STRING([--with-audit], [compile with audit support]),
  [], with_audit=no
)

if test "x$with_audit" = xno; then
  AM_CONDITIONAL(HAVE_AUDIT, false)
else
  UTIL_CHECK_LIB(audit, audit_log_user_message)
  case "$with_audit:$have_audit" in
  yes:no)
   AC_MSG_ERROR([Audit selected but libaudit not found (or doesn't support audit_log_user_message())])
   ;;
  esac
fi

AC_ARG_ENABLE([schedutils],
  AS_HELP_STRING([--disable-schedutils], [do not build chrt, ionice, teskset]),
  [], enable_schedutils=yes
)
AM_CONDITIONAL(BUILD_SCHEDUTILS, test "x$enable_schedutils" = xyes)


AC_ARG_ENABLE([wall],
  AS_HELP_STRING([--disable-wall], [do not build wall]),
  [], enable_wall=yes
)
AM_CONDITIONAL(BUILD_WALL, test "x$enable_wall" = xyes)


AC_ARG_ENABLE([write],
  AS_HELP_STRING([--enable-write], [build write]),
  [], enable_write=no
)
AM_CONDITIONAL(BUILD_WRITE, test "x$enable_write" = xyes)


AC_ARG_ENABLE([chsh-only-listed],
  AS_HELP_STRING([--disable-chsh-only-listed], [chsh: allow shells not in /etc/shells]),
  [], enable_chsh_only_listed=yes
)

if test "x$enable_chsh_only_listed" = xyes; then
  AC_DEFINE(ONLY_LISTED_SHELLS, 1, [Should chsh allow only shells in /etc/shells?])
fi


AC_ARG_ENABLE([login-chown-vcs],
  AS_HELP_STRING([--enable-login-chown-vcs], [let login chown /dev/vcsN]),
  [], enable_login_chown_vcs=no
)

if test "x$enable_login_chown_vcs" = xyes; then
  AC_DEFINE(LOGIN_CHOWN_VCS, 1, [Should login chown /dev/vcsN?])
fi


AC_ARG_ENABLE([login-stat-mail],
  AS_HELP_STRING([--enable-login-stat-mail], [let login stat() the mailbox]),
  [], enable_login_stat_mail=no
)

if test "x$enable_login_stat_mail" = xyes; then
  AC_DEFINE(LOGIN_STAT_MAIL, 1, [Should login stat() the mailbox?])
fi


AC_ARG_ENABLE([pg-bell],
  AS_HELP_STRING([--disable-pg-bell], [let pg not ring the bell on invalid keys]),
  [], enable_pg_bell=yes
)

if test "x$enable_pg_bell" = xyes; then
  AC_DEFINE(PG_BELL, 1, [Should pg ring the bell on invalid keys?])
fi


AC_ARG_ENABLE([require-password],
  AS_HELP_STRING([--disable-require-password], [do not require the user to enter the password in chfn and chsh]),
  [], enable_require_password=yes
)

if test "x$enable_require_password" = xyes; then
  AC_DEFINE(REQUIRE_PASSWORD, 1, [Should chfn and chsh require the user to enter the password?])
fi


AC_DEFUN([FS_PATHS_DEFAULT], [/sbin:/sbin/fs.d:/sbin/fs])
AC_ARG_ENABLE([fs-paths-default],
  AS_HELP_STRING([--enable-fs-paths-default=paths], [default search path for fs helpers @<:@FS_PATHS_DEFAULT@:>@]),
  [case "$enableval" in
     yes) fs_paths_defaults="FS_PATHS_DEFAULT" ;;
      no) fs_paths_defaults="" ;;
       *) fs_paths_defaults="$enableval" ;;
   esac],
  [fs_paths_defaults="FS_PATHS_DEFAULT"]
)
AC_ARG_ENABLE([fs-paths-extra],
  AS_HELP_STRING([--enable-fs-paths-extra=paths], [additional search paths for fs helpers]),
  [case "$enableval" in
     yes|no) fs_paths_extra="" ;;
          *) fs_paths_extra="$enableval" ;;
    esac],
  [fs_paths_extra=""]
)
fs_paths="$fs_paths_defaults"
if test "x$fs_paths_extra" != "x"; then
  if test "x$fs_paths" != "x"; then
    fs_paths="${fs_paths}:"
  fi
  fs_paths="${fs_paths}${fs_paths_extra}"
fi
AC_DEFINE_UNQUOTED([FS_SEARCH_PATH], "$fs_paths", [search path for fs helpers])


AC_ARG_ENABLE([use-tty-group],
  AS_HELP_STRING([--disable-use-tty-group], [do not install wall and write setgid tty]),
  [], enable_use_tty_group=yes
)
AM_CONDITIONAL(USE_TTY_GROUP, test "x$enable_use_tty_group" = xyes)

if test "x$enable_use_tty_group" = xyes; then
  AC_DEFINE(USE_TTY_GROUP, 1, [Should wall and write be installed setgid tty?])
fi

AC_ARG_ENABLE([makeinstall-chown],
  AS_HELP_STRING([--disable-makeinstall-chown], [do not do chown-like operations during "make install"]),
  [], enable_makeinstall_chown=yes
)
AM_CONDITIONAL(MAKEINSTALL_DO_CHOWN, test "x$enable_makeinstall_chown" = xyes)

AC_ARG_ENABLE([makeinstall-setuid],
  AS_HELP_STRING([--disable-makeinstall-setuid], [do not do setuid chmod operations during "make install"]),
  [], enable_makeinstall_setuid=yes
)
AM_CONDITIONAL(MAKEINSTALL_DO_SETUID, test "x$enable_makeinstall_setuid" = xyes)


AC_ARG_VAR([SUID_CFLAGS],
	   [CFLAGS used for binaries which are usually with the suid bit])
AC_ARG_VAR([SUID_LDFLAGS],
	   [LDFLAGS used for binaries which are usually with the suid bit])

LIBS=""


AC_CONFIG_HEADERS(config.h)

AC_CONFIG_FILES([
disk-utils/Makefile
fdisk/Makefile
fsck/Makefile
getopt/Makefile
hwclock/Makefile
include/Makefile
lib/Makefile
login-utils/Makefile
Makefile
man/ru/Makefile
misc-utils/chkdupexe:misc-utils/chkdupexe.pl
misc-utils/Makefile
mount/Makefile
partx/Makefile
po/Makefile.in
schedutils/Makefile
libblkid/blkid.pc
libblkid/Makefile
libblkid/docs/Makefile
libblkid/docs/version.xml
libblkid/src/Makefile
libblkid/src/blkid.h
libblkid/src/superblocks/Makefile
libblkid/src/topology/Makefile
libblkid/src/partitions/Makefile
libblkid/samples/Makefile
libmount/mount.pc
libmount/Makefile
libmount/src/Makefile
libmount/src/libmount.h
libmount/docs/Makefile
libmount/docs/version.xml
libmount/samples/Makefile
libuuid/uuid.pc
libuuid/Makefile
libuuid/man/Makefile
libuuid/src/Makefile
sys-utils/Makefile
term-utils/Makefile
tests/commands.sh
tests/helpers/Makefile
tests/Makefile
text-utils/Makefile
])


AC_OUTPUT
