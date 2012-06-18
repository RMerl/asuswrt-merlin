dnl SMB Build System
dnl ----------------
dnl Copyright (C) 2004 Stefan Metzmacher
dnl Copyright (C) 2004-2005 Jelmer Vernooij
dnl Published under the GPL
dnl
dnl SMB_EXT_LIB_FROM_PKGCONFIG(name,pkg-config name,[ACTION-IF-FOUND],[ACTION-IF-NOT-FOUND])
dnl
dnl SMB_INCLUDED_LIB_PKGCONFIG(name,pkg-config name,[ACTION-IF-FOUND],[ACTION-IF-NOT-FOUND])
dnl
dnl SMB_EXT_LIB(name,libs,cflags,cppflags,ldflags)
dnl
dnl SMB_ENABLE(name,default_build)
dnl
dnl SMB_INCLUDE_MK(file)
dnl
dnl SMB_WRITE_MAKEVARS(file)
dnl
dnl SMB_WRITE_PERLVARS(file)
dnl
dnl #######################################################
dnl ### And now the implementation			###
dnl #######################################################

dnl SMB_SUBSYSTEM(name,obj_files,required_subsystems,cflags)
AC_DEFUN([SMB_SUBSYSTEM],
[
MAKE_SETTINGS="$MAKE_SETTINGS
$1_CFLAGS = $4
$1_ENABLE = YES
$1_OBJ_FILES = $2
"

SMB_INFO_SUBSYSTEMS="$SMB_INFO_SUBSYSTEMS
###################################
# Start Subsystem $1
@<:@SUBSYSTEM::$1@:>@
PRIVATE_DEPENDENCIES = $3
CFLAGS = \$($1_CFLAGS)
ENABLE = YES
# End Subsystem $1
###################################
"
])

dnl SMB_LIBRARY(name,obj_files,required_subsystems,cflags,ldflags)
AC_DEFUN([SMB_LIBRARY],
[
MAKE_SETTINGS="$MAKE_SETTINGS
$1_CFLAGS = $6
$1_LDFLAGS = $7
n1_ENABLE = YES
$1_OBJ_FILES = $2
"

SMB_INFO_LIBRARIES="$SMB_INFO_LIBRARIES
###################################
# Start Library $1
@<:@LIBRARY::$1@:>@
PRIVATE_DEPENDENCIES = $3
CFLAGS = \$($1_CFLAGS)
LDFLAGS = \$($1_LDFLAGS)
ENABLE = YES
# End Library $1
###################################
"
])

dnl SMB_EXT_LIB_FROM_PKGCONFIG(name,pkg-config name,[ACTION-IF-FOUND],[ACTION-IF-NOT-FOUND])
AC_DEFUN([SMB_EXT_LIB_FROM_PKGCONFIG], 
[
	dnl Figure out the correct variables and call SMB_EXT_LIB()

	if test -z "$PKG_CONFIG"; then
		AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
	fi

	if test "$PKG_CONFIG" = "no" ; then
		echo "*** The pkg-config script could not be found. Make sure it is"
		echo "*** in your path, or set the PKG_CONFIG environment variable"
		echo "*** to the full path to pkg-config."
		echo "*** Or see http://pkg-config.freedesktop.org/ to get pkg-config."
			ac_cv_$1_found=no
	else
		SAMBA_PKG_CONFIG_MIN_VERSION="0.9.0"
		if $PKG_CONFIG --atleast-pkgconfig-version $SAMBA_PKG_CONFIG_MIN_VERSION; then
			AC_MSG_CHECKING(for $2)

			if $PKG_CONFIG --exists '$2' ; then
				AC_MSG_RESULT(yes)

				$1_CFLAGS="`$PKG_CONFIG --cflags '$2'`"
				OLD_CFLAGS="$CFLAGS"
				CFLAGS="$CFLAGS $$1_CFLAGS"
				AC_MSG_CHECKING([that the C compiler can use the $1_CFLAGS])
				AC_TRY_RUN([#include "${srcdir-.}/../tests/trivial.c"],
					SMB_ENABLE($1, YES)
					AC_MSG_RESULT(yes),
					AC_MSG_RESULT(no),
					AC_MSG_WARN([cannot run when cross-compiling]))
				CFLAGS="$OLD_CFLAGS"

				ac_cv_$1_libs_only_other="`$PKG_CONFIG --libs-only-other '$2'` `$PKG_CONFIG --libs-only-L '$2'`"
				LIB_REMOVE_USR_LIB(ac_cv_$1_libs_only_other)
				ac_cv_$1_includedir_only="`$PKG_CONFIG --cflags-only-I '$2'`"
				CFLAGS_REMOVE_USR_INCLUDE(ac_cv_$1_includedir_only)
				SMB_EXT_LIB($1, 
					[`$PKG_CONFIG --libs-only-l '$2'`], 
					[`$PKG_CONFIG --cflags-only-other '$2'`],
					[$ac_cv_$1_includedir_only],
					[$ac_cv_$1_libs_only_other])
				ac_cv_$1_found=yes

			else
				AC_MSG_RESULT(no)
				$PKG_CONFIG --errors-to-stdout --print-errors '$2'
				ac_cv_$1_found=no
			fi
		else
			echo "*** Your version of pkg-config is too old. You need version $SAMBA_PKG_CONFIG_MIN_VERSION or newer."
			echo "*** See http://pkg-config.freedesktop.org/"
			ac_cv_$1_found=no
		fi
	fi
	if test x$ac_cv_$1_found = x"yes"; then
		ifelse([$3], [], [echo -n ""], [$3])
	else
		ifelse([$4], [], [
			  SMB_EXT_LIB($1)
			  SMB_ENABLE($1, NO)
		], [$4])
	fi
])

dnl SMB_INCLUDED_LIB_PKGCONFIG(name,pkg-config name,[ACTION-IF-FOUND],[ACTION-IF-NOT-FOUND])
AC_DEFUN([SMB_INCLUDED_LIB_PKGCONFIG],
[
	AC_ARG_ENABLE([external-]translit($1,`A-Z',`a-z'),
		AS_HELP_STRING([--enable-external-]translit($1,`A-Z',`a-z'), [Use external $1 instead of built-in (default=ifelse([$5],[],auto,$5))]), [], [enableval=ifelse([$5],[],auto,$5)])

	if test $enableval = yes -o $enableval = auto; then
		SMB_EXT_LIB_FROM_PKGCONFIG([$1], [$2], [$3], [
			if test $enableval = yes; then
				AC_MSG_ERROR([Unable to find external $1])
			fi
			enableval=no
		])
	fi
	if test $enableval = no; then
		ifelse([$4], [], [
			  SMB_EXT_LIB($1)
			  SMB_ENABLE($1, NO)
		], [$4])
	fi
])

dnl SMB_INCLUDE_MK(file)
AC_DEFUN([SMB_INCLUDE_MK],
[
SMB_INFO_EXT_LIBS="$SMB_INFO_EXT_LIBS
mkinclude $1
"
])

dnl
dnl SMB_EXT_LIB() just specifies the details of the library.
dnl Note: the library isn't enabled by default.
dnl You need to enable it with SMB_ENABLE(name) if configure
dnl find it should be used. E.g. it should not be enabled
dnl if the library is present, but the header file is missing.
dnl
dnl SMB_EXT_LIB(name,libs,cflags,cppflags,ldflags)
AC_DEFUN([SMB_EXT_LIB],
[
MAKE_SETTINGS="$MAKE_SETTINGS
$1_LIBS = $2
$1_CFLAGS = $3
$1_CPPFLAGS = $4
$1_LDFLAGS = $5
"

])

dnl SMB_ENABLE(name,default_build)
AC_DEFUN([SMB_ENABLE],
[
	MAKE_SETTINGS="$MAKE_SETTINGS
$1_ENABLE = $2
"
SMB_INFO_ENABLES="$SMB_INFO_ENABLES
\$enabled{\"$1\"} = \"$2\";"
])

dnl SMB_MAKE_SETTINGS(text)
AC_DEFUN([SMB_MAKE_SETTINGS],
[
MAKE_SETTINGS="$MAKE_SETTINGS
$1
"
])

dnl SMB_WRITE_MAKEVARS(path, skip_vars)
AC_DEFUN([SMB_WRITE_MAKEVARS],
[
echo "configure: creating $1"
cat >$1<<CEOF
# $1 - Autogenerated by configure, DO NOT EDIT!
$MAKE_SETTINGS
CEOF
skip_vars=" $2 "
for ac_var in $ac_subst_vars
do
    eval ac_val=\$$ac_var
	if echo "$skip_vars" | grep -v " $ac_var " >/dev/null 2>/dev/null; then
		echo "$ac_var = $ac_val" >> $1
	fi
done
])

dnl SMB_WRITE_PERLVARS(path)
AC_DEFUN([SMB_WRITE_PERLVARS],
[
echo "configure: creating $1"
cat >$1<<CEOF
# config.pm - Autogenerate by configure. DO NOT EDIT!

package config;
require Exporter;
@ISA = qw(Exporter);
@EXPORT_OK = qw(%enabled %config);
use strict;

use vars qw(%enabled %config);

%config = (
CEOF

for ac_var in $ac_subst_vars
do
	eval ac_val=\$$ac_var
	# quote ' (\x27) inside '...' and make sure \ isn't eaten by shells, so use perl:
	QAC_VAL=$ac_val QAC_VAR=$ac_var perl -e '$myval="$ENV{QAC_VAL}"; $myval =~ s/\x27/\\\x27/g ; print $ENV{QAC_VAR}." => \x27$myval\x27,\n"' >> $1
done

cat >>$1<<CEOF
);
$SMB_INFO_ENABLES
1;
CEOF
])

dnl SMB_BUILD_RUN(OUTPUT_FILE)
AC_DEFUN([SMB_BUILD_RUN],
[
AC_OUTPUT_COMMANDS(
[
test "x$ac_abs_srcdir" != "x$ac_abs_builddir" && (
	cd $builddir;
	# NOTE: We *must* use -R so we don't follow symlinks (at least on BSD
	# systems).
	test -d heimdal || cp -R $srcdir/heimdal $builddir/
	test -d heimdal_build || cp -R $srcdir/heimdal_build $builddir/
	test -d build || builddir="$builddir" \
			srcdir="$srcdir" \
			$PERL ${srcdir}/script/buildtree.pl
 )

$PERL -I${builddir} -I${builddir}/build \
    -I${srcdir} -I${srcdir}/build \
    ${srcdir}/build/smb_build/main.pl --output=$1 main.mk || exit $?
],
[
srcdir="$srcdir"
builddir="$builddir"
PERL="$PERL"

export PERL
export srcdir
export builddir
])
])
