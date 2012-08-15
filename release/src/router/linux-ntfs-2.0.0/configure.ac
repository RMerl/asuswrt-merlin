#
# configure.ac - Source file to generate "./configure" to prepare package for
#		 compilation.
#
# Copyright (c) 2000-2006 Anton Altaparmakov
# Copyright (c) 2003 Jan Kratochvil
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
# along with this program (in the main directory of the Linux-NTFS
# distribution in the file COPYING); if not, write to the Free Software
# Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

AC_PREREQ(2.59)
AC_INIT([ntfsprogs],[2.0.0],[linux-ntfs-dev@lists.sourceforge.net])

#
# Before making a release, the LTVERSION string should be modified.
# The string is of the form CURRENT:REVISION:AGE.
#
# CURRENT (C)
#	The most recent interface number that this library implements.
#
# REVISION (R)
#	The implementation number that this library implements.
#
# AGE (A)
#	The difference between the newest and oldest interfaces that this
#	library implements. In other works, the library implements all the
#	interface numbers in the range from number 'CURRENT - AGE' to
#	'CURRENT'.
#
# This means that:
#
#   - If interfaces have been changed or added, but binary compatibility has
#     been preserved, change to C+1:0:A+1
#
#   - If binary compatibility has been broken (eg removed or changed
#     interfaces) change to C+1:0:0
#
#   - If the interface is the same as the previous version, change to C:R+1:A
#
LTVERSION_LIBNTFS="10:0:0"
AC_SUBST(LTVERSION_LIBNTFS)

AC_LANG([C])
AC_CANONICAL_HOST([])
AC_CANONICAL_TARGET([])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AM_ENABLE_SHARED
AM_ENABLE_STATIC

# This is required to get past a stupid configure bug when making the rpm.
# Basically it is broken to specify the host as a command line argument to
# configure on its own, i.e. without giving --host=. It is supposed to work
# but doesn't. So this sets host and erases nonopt effectively moving the
# standalone command line option into the --host= form.
if test "x$nonopt" != "xNONE"; then
	host="$nonopt"
	nonopt="NONE"
fi

AC_PREFIX_DEFAULT(/usr/local)
if test "x$prefix" = "xNONE"; then
	prefix=$ac_default_prefix
	ac_configure_args="$ac_configure_args --prefix $prefix"
fi

# Command-line options.
AC_ARG_ENABLE(debug,
	AS_HELP_STRING(--enable-debug,enable additional debugging code and
		       output), ,
	enable_debug=no
)

AC_ARG_ENABLE(sparse,
	AS_HELP_STRING(--enable-sparse,enable sparse code checking), ,
	enable_sparse=no
)

# AH_TEMPLATE is needed here because we want to have a template in config.h
# regardless of whether the option is given or not.
AH_TEMPLATE([NO_NTFS_DEVICE_DEFAULT_IO_OPS],
	    [Define this if you do not want the NTFS library to provide default
	     device io operations.  This means that you cannot use ntfs_mount()
	     but have to use ntfs_device_mount() and provide your own device
	     operations.])
AC_ARG_ENABLE(default-device-io-ops,
	AS_HELP_STRING(--disable-default-device-io-ops,do not provide default
		       device io operations),
	if test "$enable_default_device_io_ops" == "no"; then
		AC_DEFINE(NO_NTFS_DEVICE_DEFAULT_IO_OPS)
	fi,
)

AC_ARG_ENABLE(gnome-vfs,
	AS_HELP_STRING(--disable-gnome-vfs,omit Gnome-VFS-2.0 'libntfs'
		       interface (default=detect)), ,
	enable_gnome_vfs=auto
)

AC_ARG_ENABLE(ntfsmount,
	AS_HELP_STRING(--disable-ntfsmount,omit userspace NTFS driver
		       (default=detect)), ,
	enable_ntfsmount=auto
)

AC_ARG_ENABLE(crypto,
	AS_HELP_STRING(--enable-crypto,enable crypto related code and utilities
		       (default=detect)), ,
	enable_crypto=auto
)

AC_ARG_ENABLE(really-static,
	AS_HELP_STRING(--enable-really-static,create completely static binaries
		       for the utilities), ,
	enable_really_static=no
)
AM_CONDITIONAL(REALLYSTATIC, test "$enable_really_static" = yes)

AC_ARG_ENABLE(warnings,
	AS_HELP_STRING(--enable-warnings,enable additional compiler warnings), ,
	enable_warnings=no
)

AC_ARG_ENABLE(test,
	AS_HELP_STRING(--enable-test,compile in test code for libntfs), ,
	enable_test=no
)
AM_CONDITIONAL(ENABLE_TEST, test "$enable_test" = yes)
if test "$enable_test" = "yes"; then
	CFLAGS="${CFLAGS} -DNTFS_TEST"
fi

# Use GNU extensions if available.
AC_GNU_SOURCE

# For djgpp support.  (Note: This needs to be before AC_PROG_CC.)
case "$target" in
*-*-*djgpp)
	if test -z "$CC"; then CC=gcc; fi
	if test -z "$AR"; then AR=ar; fi
	if test -z "$LD"; then LD=ld; fi
;;
esac

# Checks for programs.
AC_PROG_CC
AC_PROG_GCC_TRADITIONAL
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PATH_PROG(RM, rm, rm)
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
PKG_PROG_PKG_CONFIG

# Libraries often install their metadata .pc files in directories not searched
# by pkg-config. Let's workaround this. 
export PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:/usr/lib/pkgconfig:/opt/gnome/lib/pkgconfig:/usr/share/pkgconfig:/usr/local/lib/pkgconfig:$prefix/lib/pkgconfig:/opt/gnome/share/pkgconfig:/usr/local/share/pkgconfig

# Enable large file support.
AC_SYS_LARGEFILE

# Autodetect whether to build Gnome VFS or not.
compile_gnome_vfs=false
if test "$enable_gnome_vfs" != "no"; then
	PKG_CHECK_MODULES(LIBNTFS_GNOMEVFS, [glib-2.0 gthread-2.0 gnome-vfs-module-2.0],
	[ compile_gnome_vfs=true ],
		if test "$enable_gnome_vfs" = "yes"; then
			AC_MSG_ERROR([Linux-NTFS Gnome VFS module requires glib-2.0 and gnome-vfs-module-2.0 libraries.])
		else
			AC_MSG_WARN([Linux-NTFS Gnome VFS module requires glib-2.0 and gnome-vfs-module-2.0 libraries.])
		fi
	)
fi
AM_CONDITIONAL(ENABLE_GNOME_VFS, $compile_gnome_vfs)

# Autodetect whether to build FUSE module or not.
compile_ntfsmount=false
if test "$enable_ntfsmount" != "no"; then
	PKG_CHECK_MODULES(FUSE, fuse >= 2.6.1, [ compile_ntfsmount=true ],
		if test "$enable_ntfsmount" = "yes"; then
			AC_MSG_ERROR([ntfsmount requires FUSE version >= 2.6.1.])
		else
			AC_MSG_WARN([ntfsmount requires FUSE version >= 2.6.1.])
		fi
	)
fi
AM_CONDITIONAL(ENABLE_FUSE, $compile_ntfsmount)

# Autodetect whether we can build crypto stuff or not.
compile_crypto=false
if test "$enable_crypto" != "no"; then
	have_libgcrypt=false
	AM_PATH_LIBGCRYPT(1.2.2, [ have_libgcrypt=true ],
	[
		if test "$enable_crypto" = "yes"; then
			AC_MSG_ERROR([Linux-NTFS crypto code requires the gcrypt library.])
		else
			AC_MSG_WARN([Linux-NTFS crypto code requires the gcrypt library.])
		fi
	])
	have_libgnutls=false
	PKG_CHECK_MODULES(GNUTLS, gnutls >= 1.4.4, [ have_libgnutls=true ],
		if test "$enable_crypto" = "yes"; then
			AC_MSG_ERROR([Linux-NTFS crypto code requires the gnutls library.])
		else
			AC_MSG_WARN([Linux-NTFS crypto code requires the gnutls library.])
		fi
	)
	have_libconfig=false
	PKG_CHECK_MODULES(libconfig, libconfig >= 1.0.1, [ have_libconfig=true ],
		if test "$enable_crypto" = "yes"; then
			AC_MSG_ERROR([Linux-NTFS crypto code requires the libconfig.])
		else
			AC_MSG_WARN([Linux-NTFS crypto code requires the libconfig.])
		fi
	)
	if test "$have_libgcrypt" = "true"; then
		if test "$have_libgnutls" = "true"; then
			if test "$have_libconfig" = "true"; then
				compile_crypto=true
				AC_DEFINE([ENABLE_CRYPTO], 1,
				[Define this to 1 if you want to enable support of
				 encrypted files in libntfs and utilities.])
			fi
		fi
	fi
fi
AM_CONDITIONAL(ENABLE_CRYPTO, $compile_crypto)

# add --with-extra-includes and --with-extra-libs switch to ./configure
all_libraries="$all_libraries $USER_LDFLAGS"
all_includes="$all_includes $USER_INCLUDES"
AC_SUBST(all_includes)
AC_SUBST(all_libraries)

# Specify support for generating DCE compliant UUIDs (aka GUIDs).  We check if
# uuid/uuid.h header is present and the uuid library is present that goes with
# it and then check if uuid_generate() is present and usable.
#
# DCE UUIDs are enabled by default and can be disabled with the --disable-uuid
# option to the configure script.
AC_ARG_WITH(uuid, [
  --with-uuid@<:@=PFX@:>@       generate DCE compliant UUIDs, with optional prefix
                          to uuid library and headers @<:@default=detect@:>@
  --without-uuid          do not generate DCE compliant UUIDs],
	if test "$with_uuid" = "yes"; then
		extrapath=default
	elif test "$with_uuid" = "no"; then
		extrapath=
	else
		extrapath=$with_uuid
	fi,
	extrapath=default
)
if test "x$extrapath" != "x"; then
	if test "x$extrapath" != "xdefault"; then
		MKNTFS_CPPFLAGS="$MKNTFS_CPPFLAGS -I$extrapath/include"
		MKNTFS_LIBS="$MKNTFS_LIBS -L$extrapath/lib"
	fi
	AC_CHECK_HEADER([uuid/uuid.h],
		AC_CHECK_LIB([uuid], [uuid_generate],
			AC_DEFINE([ENABLE_UUID], 1,
			[Define this to 1 if you want to enable generation of
			DCE compliant UUIDs.])
			MKNTFS_LIBS="$MKNTFS_LIBS -luuid",
			AC_MSG_WARN([Linux-NTFS DCE compliant UUID generation code requires the uuid library.]),
		),
		AC_MSG_WARN([Linux-NTFS DCE compliant UUID generation code requires the uuid library.]),
	)
fi

# Get compiler name
if test ! -z "$CC"; then
	_cc="$CC"
else
	_cc="gcc"
fi

# Check for gcc version being >= 2.96.
AC_MSG_CHECKING(version of $_cc)
cc_version=`$srcdir/getgccver $_cc version`
cc_major=`$srcdir/getgccver $_cc major`
cc_minor=`$srcdir/getgccver $_cc minor`
if test -z "$cc_version"; then
	cc_version="v. ?.??"
	cc_major=1
	cc_minor=1
fi
if test $cc_major -lt 2 -o \( $cc_major -eq 2 -a $cc_minor -lt 96 \); then
cc_version="$cc_version, bad"
AC_MSG_RESULT($cc_version)
AC_MSG_ERROR(Please upgrade your gcc compiler to gcc-2.96+ or gcc-3+ version!\
 Earlier compiler versions will NOT work as these do not support \
unnamed/annonymous structures and unions which are used heavily in linux-ntfs.)
fi
cc_version="$cc_version, ok"
AC_MSG_RESULT($cc_version)

# Add our compiler switches not discarding 'CFLAGS' as they may have been
# passed to us by rpmbuild(8).
# We add -Wall to enable some compiler warnings.
CFLAGS="$CFLAGS -Wall"

# Add lots of extra warnings if --enable-warnings was specified.
if test "$enable_warnings" = "yes"; then
	CFLAGS="$CFLAGS -W -Wall -Waggregate-return -Wbad-function-cast -Wcast-align -Wcast-qual -Wdisabled-optimization -Wdiv-by-zero -Wfloat-equal -Winline -Wmissing-declarations -Wmissing-format-attribute -Wmissing-noreturn -Wmissing-prototypes -Wmultichar -Wnested-externs -Wpointer-arith -Wredundant-decls -Wshadow -Wsign-compare -Wstrict-prototypes -Wundef -Wwrite-strings"
fi

# Add debugging switches if --enable-debug was specified.
if test "$enable_debug" = "yes"; then
	CFLAGS="$CFLAGS -ggdb3 -DDEBUG -O0"
fi

# Set cgcc as compiler and add sparse flags if --enable-sparse was specified.
if test "$enable_sparse" = "yes"; then
	CC="cgcc"
	CFLAGS="$CFLAGS -Wbitwise -Wno-transparent-union"
fi

AC_SUBST(CFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(LIBS)

AC_SUBST(LIBNTFS_CFLAGS)
AC_SUBST(LIBNTFS_GNOMEVFS_CFLAGS)
AC_SUBST(LIBNTFS_GNOMEVFS_LIBS)

AC_SUBST(MKNTFS_CPPFLAGS)
AC_SUBST(MKNTFS_LIBS)

AC_SUBST(AUTODIRS)

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([ctype.h fcntl.h libgen.h libintl.h limits.h locale.h \
	mntent.h stddef.h stdint.h stdlib.h stdio.h stdarg.h string.h \
	strings.h errno.h time.h unistd.h utime.h wchar.h getopt.h features.h \
	endian.h byteswap.h sys/byteorder.h sys/endian.h sys/param.h \
	sys/ioctl.h sys/mount.h sys/stat.h sys/types.h sys/vfs.h \
	sys/statvfs.h sys/sysmacros.h linux/major.h linux/fd.h linux/hdreg.h \
	machine/endian.h windows.h syslog.h pwd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_BIGENDIAN(,
	[AC_DEFINE([WORDS_LITTLEENDIAN], 1,
	[Define to 1 if your processor stores words with the least significant
	byte first (like Intel and VAX, unlike Motorola and SPARC).])]
	,)
AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_STRUCT_ST_BLOCKS
AC_CHECK_MEMBERS([struct stat.st_rdev])

# Checks for library functions.
AC_FUNC_GETMNTENT
AC_FUNC_MBRTOWC
AC_FUNC_MEMCMP
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_FUNC_UTIME_NULL
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([atexit basename dup2 fdatasync getopt_long hasmntopt mbsinit \
	memmove memset realpath regcomp setlocale setxattr strcasecmp strchr \
	strdup strerror strnlen strtol strtoul sysconf utime])

# Makefiles to be created by configure.
AC_CONFIG_FILES([
	Makefile
	doc/Makefile
	include/Makefile
	include/ntfs/Makefile
	libntfs/Makefile
	libntfs/libntfs.conf
	libntfs/libntfs.8
	libntfs/libntfs-gnomevfs.8
	ntfsprogs/Makefile
	ntfsprogs/mkntfs.8
	ntfsprogs/ntfscat.8
	ntfsprogs/ntfsclone.8
	ntfsprogs/ntfscluster.8
	ntfsprogs/ntfscmp.8
	ntfsprogs/ntfscp.8
	ntfsprogs/ntfsfix.8
	ntfsprogs/ntfsinfo.8
	ntfsprogs/ntfslabel.8
	ntfsprogs/ntfsls.8
	ntfsprogs/ntfsmount.8
	ntfsprogs/ntfsprogs.8
	ntfsprogs/ntfsresize.8
	ntfsprogs/ntfsundelete.8
	ntfsprogs.spec
	test/Makefile
])
AC_OUTPUT
