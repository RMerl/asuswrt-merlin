#
# configure.ac - Source file to generate "./configure" to prepare package for
#		 compilation.
#
# Copyright (c) 2000-2006 Anton Altaparmakov
# Copyright (c) 2005-2009 Szabolcs Szakacsits
# Copyright (C) 2007-2008 Alon Bar-Lev
#
# This program/include file is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as published
# by the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program/include file is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program (in the main directory of the NTFS-3G
# distribution in the file COPYING); if not, write to the Free Software
# Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# Autoconf
AC_PREREQ(2.59)
AC_INIT([ntfs-3g],[2010.10.2],[ntfs-3g-devel@lists.sf.net])
LIBNTFS_3G_VERSION="80"
AC_CONFIG_SRCDIR([src/ntfs-3g.c])

# Environment
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

# Automake
AM_INIT_AUTOMAKE([${PACKAGE_NAME}], [${PACKAGE_VERSION}])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AM_MAINTAINER_MODE

# Options
AC_ARG_ENABLE(
	[debug],
	[AS_HELP_STRING([--enable-debug],[enable debugging code and output])],
	,
	[enable_debug="no"]
)

AC_ARG_ENABLE(
	[warnings],
	[AS_HELP_STRING([--enable-warnings],[enable lots of compiler warnings])],
	,
	[enable_warnings="no"]
)

AC_ARG_ENABLE(
	[pedantic],
	[AS_HELP_STRING([--enable-pedantic],[enable compile pedantic mode])],
	,
	[enable_pedantic="no"]
)

AC_ARG_ENABLE(
	[really-static],
	[AS_HELP_STRING([--enable-really-static],[create fully static binaries])],
	,
	[enable_really_static="no"]
)

AC_ARG_ENABLE(
	[mount-helper],
	[AS_HELP_STRING([--enable-mount-helper],[install mount helper @<:@default=enabled for linux@:>@])],
	,
	[
		case "${target_os}" in
			linux*) enable_mount_helper="yes" ;;
			*) enable_mount_helper="no" ;;
		esac
	]
)

AC_ARG_ENABLE(
	[ldscript],
	[AS_HELP_STRING([--enable-ldscript],[use ldscript instead of .so symlink])],
	,
	[enable_ldscript="no"]
)

AC_ARG_ENABLE(
	[ldconfig],
	[AS_HELP_STRING([--disable-ldconfig],[do not update dynamic linker cache using ldconfig])],
	,
	[enable_ldconfig="yes"]
)

AC_ARG_ENABLE(
	[library],
	[AS_HELP_STRING([--disable-library],[do not install libntfs-3g but link it into ntfs-3g])],
	,
	[enable_library="yes"]
)

AC_ARG_ENABLE(
	[mtab],
	[AS_HELP_STRING([--disable-mtab],[disable and ignore usage of /etc/mtab])],
	,
	[enable_mtab="yes"]
)

AC_ARG_ENABLE(
	[posix-acls],
	[AS_HELP_STRING([--enable-posix-acls],[enable POSIX ACL support])],
	,
	[enable_posix_acls="no"]
)

AC_ARG_ENABLE(
	[device-default-io-ops],
	[AS_HELP_STRING([--disable-device-default-io-ops],[install default IO ops])],
	,
	[enable_device_default_io_ops="yes"]
)

AC_ARG_ENABLE(
	[nfconv],
	[AS_HELP_STRING([--disable-nfconv],[disable the 'nfconv' patch, which adds support for Unicode normalization form conversion when built on Mac OS X @<:@default=enabled for Mac OS X@:>@])],
	[enable_nfconv="no"],
	[
		case "${target_os}" in
			darwin*) enable_nfconv="yes" ;;
			*) enable_nfconv="no" ;;
		esac
	]
)

# pthread_rwlock_t requires _GNU_SOURCE
AC_GNU_SOURCE

# Programs
AC_PROG_CC(gcc cc)
AC_PROG_LN_S
AM_PROG_CC_C_O

ifdef(
	[LT_INIT],
	[LT_INIT],
	[AC_PROG_LIBTOOL]
)

AC_PATH_PROG([MV], [mv])
AC_PATH_PROG([RM], [rm])
AC_PATH_PROG([SED], [sed])
AC_ARG_VAR([LDCONFIG], [ldconfig utility])
AC_PATH_PROG([LDCONFIG], [ldconfig], [true], [/sbin /usr/sbin $PATH])

# Environment
AC_MSG_CHECKING([Windows OS])
case "${target}" in
*-mingw32*|*-winnt*|*-cygwin*)
	AC_MSG_RESULT([yes])
	WINDOWS="yes"
	AC_DEFINE(
		[WINDOWS],
		[1],
		[Define to 1 if this is a Windows OS]
	)
	;;
*)
	AC_MSG_RESULT([no])
	WINDOWS="no"
	;;
esac

AC_MSG_CHECKING([fuse compatibility])
case "${target_os}" in
linux*)
	AC_ARG_WITH(
		[fuse],
		[AS_HELP_STRING([--with-fuse=<internal|external>],[Select FUSE library: internal or external @<:@default=internal@:>@])],
		,
		[with_fuse="internal"]
	)
	;;
darwin*|netbsd*|solaris*)
	with_fuse="external"
	;;
freebsd*)
	AC_MSG_ERROR([Please see FreeBSD support at http://www.freshports.org/sysutils/fusefs-ntfs])
	;;
*)
	AC_MSG_ERROR([ntfs-3g can be built for Linux, FreeBSD, Mac OS X, NetBSD, and Solaris only.])
	;;
esac
AC_MSG_RESULT([${with_fuse}])

case "${target_os}" in
solaris*)
	if test "x$GCC" != "xyes" ; then
		AC_MSG_ERROR([ntfs-3g can be built only with gcc on Solaris. Install it by 'pkg install gcc-dev' and retry.)])
	fi
	;;
esac

if test "${enable_ldscript}" = "yes"; then
	AC_MSG_CHECKING([Output format])
	OUTPUT_FORMAT="$(${CC} ${CFLAGS} ${LDFLAGS} -Wl,--verbose 2>&1 | ${SED} -n 's/^OUTPUT_FORMAT("\([[^"]]*\)",.*/\1/p')"
	if test -z "${OUTPUT_FORMAT}"; then
		AC_MSG_RESULT([None])
	else
		AC_MSG_RESULT([${OUTPUT_FORMAT}])
		OUTPUT_FORMAT="OUTPUT_FORMAT ( ${OUTPUT_FORMAT} )"
	fi
fi

# Libraries
if test "${with_fuse}" = "internal"; then
	AC_CHECK_LIB(
		[pthread],
		[pthread_create],
		[LIBFUSE_LITE_LIBS="${LIBFUSE_LITE_LIBS} -lpthread"],
		[AC_MSG_ERROR([Cannot find pthread library])]
	)
	AC_DEFINE(
		[_REENTRANT],
		[1],
		[Required define if using POSIX threads]
	)
	# required so that we re-compile anything
	AC_DEFINE(
		[FUSE_INTERNAL],
		[1],
		[Define to 1 if using internal fuse]
	)
else
	if test -z "$PKG_CONFIG"; then
		AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
	fi
	test "x${PKG_CONFIG}" = "xno" && AC_MSG_ERROR([pkg-config wasn't found! Please install from your vendor, or see http://pkg-config.freedesktop.org/wiki/])
	# Libraries often install their metadata .pc files in directories
	# not searched by pkg-config. Let's workaround this. 
	export PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:/lib/pkgconfig:/usr/lib/pkgconfig:/opt/gnome/lib/pkgconfig:/usr/share/pkgconfig:/usr/local/lib/pkgconfig:$prefix/lib/pkgconfig:/opt/gnome/share/pkgconfig:/usr/local/share/pkgconfig
	PKG_CHECK_MODULES(
		[FUSE_MODULE],
		[fuse >= 2.6.0],
		,
		[
			AC_MSG_ERROR([FUSE >= 2.6.0 was not found. Either older FUSE is still present, or FUSE is not fully installed (e.g. fuse, libfuse, libfuse2, libfuse-dev, etc packages). Source code: http://fuse.sf.net])
		]
	)
	FUSE_LIB_PATH=`$PKG_CONFIG --libs-only-L fuse | sed -e 's,/[/]*,/,g' -e 's,[ ]*$,,'`
fi

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([ctype.h fcntl.h libgen.h libintl.h limits.h locale.h \
	mntent.h stddef.h stdint.h stdlib.h stdio.h stdarg.h string.h \
	strings.h errno.h time.h unistd.h utime.h wchar.h getopt.h features.h \
	endian.h byteswap.h sys/byteorder.h sys/endian.h sys/param.h \
	sys/ioctl.h sys/mkdev.h sys/mount.h sys/stat.h sys/types.h sys/vfs.h \
	sys/statvfs.h sys/sysmacros.h linux/major.h linux/fd.h linux/hdreg.h \
	machine/endian.h windows.h syslog.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_BIGENDIAN(
	,
	[
		AC_DEFINE(
			[WORDS_LITTLEENDIAN],
			[1],
			[Define to 1 if your processor stores words with the least significant
			byte first (like Intel and VAX, unlike Motorola and SPARC).]
		)
	]
	,
)
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_STRUCT_ST_BLOCKS
AC_CHECK_MEMBERS([struct stat.st_rdev])
AC_CHECK_MEMBERS([struct stat.st_atim])
AC_CHECK_MEMBERS([struct stat.st_atimespec])
AC_CHECK_MEMBERS([struct stat.st_atimensec])

# For the 'nfconv' patch (Mac OS X only):
case "${target_os}" in
darwin*)
	if test "${enable_nfconv}" = "yes"; then
		AC_CHECK_HEADER(
			[CoreFoundation/CoreFoundation.h],
			[
				LDFLAGS="${LDFLAGS} -framework CoreFoundation"
				AC_DEFINE(
					[ENABLE_NFCONV],
					[1],
					[Define to 1 if the nfconv patch should be enabled]
				)
			],
			AC_MSG_ERROR([[Cannot find CoreFoundation required for 'nfconv' functionality Mac OS X. You may use the --disable-nfconv 'configure' option to avoid this error.]])
		)
	fi
	;;
esac

# Checks for library functions.
AC_FUNC_GETMNTENT
AC_FUNC_MBRTOWC
AC_FUNC_MEMCMP
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_FUNC_UTIME_NULL
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([ \
	atexit basename daemon dup2 fdatasync ffs getopt_long hasmntopt \
	mbsinit memmove memset realpath regcomp setlocale setxattr \
	strcasecmp strchr strdup strerror strnlen strsep strtol strtoul \
	sysconf utime utimensat gettimeofday clock_gettime fork \
])
AC_SYS_LARGEFILE

if test "$GCC" = "yes" ; then
	# We add -Wall to enable some compiler warnings.
	CFLAGS="${CFLAGS} -Wall"
fi

if test "${enable_pedantic}" = "yes"; then
	enable_warnings="yes"
	CFLAGS="${CFLAGS} -pedantic"
fi

if test "${enable_warnings}" = "yes"; then
	CFLAGS="${CFLAGS} -W -Wall -Waggregate-return -Wbad-function-cast -Wcast-align -Wcast-qual -Wdisabled-optimization -Wdiv-by-zero -Wfloat-equal -Winline -Wmissing-declarations -Wmissing-format-attribute -Wmissing-noreturn -Wmissing-prototypes -Wmultichar -Wnested-externs -Wpointer-arith -Wredundant-decls -Wshadow -Wsign-compare -Wstrict-prototypes -Wundef -Wwrite-strings -Wformat -Wformat-security -Wuninitialized"
fi

if test "${enable_debug}" = "yes"; then
	CFLAGS="${CFLAGS} -ggdb3 -DDEBUG"
	AC_DEFINE(
		[ENABLE_DEBUG],
		[1],
		[Define to 1 if debug should be enabled]
	)
fi

test "${enable_device_default_io_ops}" = "no" && AC_DEFINE(
	[NO_NTFS_DEVICE_DEFAULT_IO_OPS],
	[1],
	[Don't use default IO ops]
)

test "${enable_mtab}" = "no" && AC_DEFINE([IGNORE_MTAB], [1], [Don't update /etc/mtab])
test "${enable_posix_acls}" != "no" && AC_DEFINE([POSIXACLS], [1], [POSIX ACL support])

test "${enable_really_static}" = "yes" && enable_library="no"
test "${enable_library}" = "no" && enable_ldconfig="no"

if test "x${DISTCHECK_HACK}" != "x"; then
	enable_mount_helper="no"
	enable_ldconfig="no"
fi

# Settings
pkgconfigdir="\$(libdir)/pkgconfig"
ntfs3gincludedir="\$(includedir)/ntfs-3g"
# Executables should be installed to the root filesystem, otherwise 
# automounting NTFS volumes can fail during boot if the driver binaries 
# and their dependencies are on an unmounted partition. Use --exec-prefix
# to override this.
if test "x${exec_prefix}" = "xNONE"; then
	rootbindir="/bin"
	rootsbindir="/sbin"
	rootlibdir="/lib${libdir##*/lib}"
else
	rootbindir="\$(bindir)"
	rootsbindir="\$(sbindir)"
	rootlibdir="\$(libdir)"
fi
AC_SUBST([pkgconfigdir])
AC_SUBST([ntfs3gincludedir])
AC_SUBST([rootbindir])
AC_SUBST([rootsbindir])
AC_SUBST([rootlibdir])
AC_SUBST([LIBNTFS_3G_VERSION])
AC_SUBST([LIBFUSE_LITE_LIBS])
AC_SUBST([OUTPUT_FORMAT])
AM_CONDITIONAL([FUSE_INTERNAL], [test "${with_fuse}" = "internal"])
AM_CONDITIONAL([GENERATE_LDSCRIPT], [test "${enable_ldscript}" = "yes"])
AM_CONDITIONAL([WINDOWS], [test "${WINDOWS}" = "yes"])
AM_CONDITIONAL([NTFS_DEVICE_DEFAULT_IO_OPS], [test "${enable_device_default_io_ops}" = "yes"])
AM_CONDITIONAL([RUN_LDCONFIG], [test "${enable_ldconfig}" = "yes"])
AM_CONDITIONAL([REALLYSTATIC], [test "${enable_really_static}" = "yes"])
AM_CONDITIONAL([INSTALL_LIBRARY], [test "${enable_library}" = "yes"])
AM_CONDITIONAL([ENABLE_MOUNT_HELPER], [test "${enable_mount_helper}" = "yes"])

# workaround for <autoconf-2.60
if test -z "${docdir}"; then
	docdir="\$(datarootdir)/doc/\$(PACKAGE_NAME)"
	AC_SUBST([docdir])
fi
# workaround for <automake-1.10
if test -z "${MKDIR_P}"; then
	MKDIR_P="\$(mkdir_p)"
	AC_SUBST([MKDIR_P])
fi

# generate files
AC_CONFIG_FILES([
	Makefile
	include/Makefile
	include/fuse-lite/Makefile
	include/ntfs-3g/Makefile
	libfuse-lite/Makefile
	libntfs-3g/Makefile
	libntfs-3g/libntfs-3g.pc
	libntfs-3g/libntfs-3g.script.so
	src/Makefile
	src/ntfs-3g.8
	src/ntfs-3g.probe.8
	src/ntfs-3g.usermap.8
	src/ntfs-3g.secaudit.8
])
AC_OUTPUT

if test "${with_fuse}" = "external"; then
	if ! echo "x$FUSE_LIB_PATH" | grep -- "x-L/lib" > /dev/null; then
		cat <<EOF
****************************************************************************
* WARNING  WARNING  WARNING  WARNING  WARNING  WARNING  WARNING  WARNING   *
* The FUSE user space binaries were NOT installed with root directory      *
* executable prefix. This means that automounting NTFS volumes during boot *
* could fail. This can be fixed the below way by reinstalling FUSE using   *
* the right 'configure' option during FUSE compilation:                    *
* 	./configure --exec-prefix=/                                        *
* 	make && sudo make install                                          *
* WARNING  WARNING  WARNING  WARNING  WARNING  WARNING  WARNING  WARNING   *
****************************************************************************
EOF
	fi
fi

echo "You can type now 'make' to build ntfs-3g."

