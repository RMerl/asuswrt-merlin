dnl SMB Build Environment Checks
dnl -------------------------------------------------------
dnl  Copyright (C) Stefan (metze) Metzmacher 2004
dnl  Copyright (C) Jelmer Vernooij 2005,2008
dnl  Released under the GNU GPL
dnl -------------------------------------------------------
dnl

AC_SUBST(srcdir)
export srcdir;

# we always set builddir to "." as that's nicer than
# having the absolute path of the current work directory
builddir=.
AC_SUBST(builddir)
export builddir;

AC_SUBST(datarootdir)

AC_SUBST(VPATH)
VPATH="\$(builddir):\$(srcdir)"

SMB_VERSION_STRING=`cat ${srcdir}/version.h | grep 'SAMBA_VERSION_OFFICIAL_STRING' | cut -d '"' -f2`
echo "SAMBA VERSION: ${SMB_VERSION_STRING}"

SAMBA_VERSION_GIT_COMMIT_FULLREV=`cat ${srcdir}/version.h | grep 'SAMBA_VERSION_GIT_COMMIT_FULLREV' | cut -d ' ' -f3- | cut -d '"' -f2`
if test -n "${SAMBA_VERSION_GIT_COMMIT_FULLREV}";then
	echo "BUILD COMMIT REVISION: ${SAMBA_VERSION_GIT_COMMIT_FULLREV}"
fi
SAMBA_VERSION_GIT_COMMIT_DATE=`cat ${srcdir}/version.h | grep 'SAMBA_VERSION_GIT_COMMIT_DATE' | cut -d ' ' -f3-`
if test -n "${SAMBA_VERSION_GIT_COMMIT_DATE}";then
	echo "BUILD COMMIT DATE: ${SAMBA_VERSION_GIT_COMMIT_DATE}"
fi
SAMBA_VERSION_GIT_COMMIT_TIME=`cat ${srcdir}/version.h | grep 'SAMBA_VERSION_GIT_COMMIT_TIME' | cut -d ' ' -f3-`
if test -n "${SAMBA_VERSION_GIT_COMMIT_TIME}";then
	echo "BUILD COMMIT TIME: ${SAMBA_VERSION_GIT_COMMIT_TIME}"

	# just to keep the build-farm gui happy for now...
	echo "BUILD REVISION: ${SAMBA_VERSION_GIT_COMMIT_TIME}"
fi

m4_include(build/m4/check_path.m4)
m4_include(../m4/check_perl.m4)

AC_SAMBA_PERL([], [AC_MSG_ERROR([Please install perl from http://www.perl.com/])])

AC_PATH_PROG(YAPP, yapp, false)

m4_include(build/m4/check_cc.m4)
m4_include(build/m4/check_ld.m4)
m4_include(../m4/check_make.m4)

AC_SAMBA_GNU_MAKE([AC_MSG_RESULT(found)], [AC_MSG_ERROR([Unable to find GNU make])])
AC_SAMBA_GNU_MAKE_VERSION()
GNU_MAKE_VERSION=$samba_cv_gnu_make_version
AC_SUBST(GNU_MAKE_VERSION)

new_make=no
AC_MSG_CHECKING([for GNU make >= 3.81])
if $PERL -e " \$_ = '$GNU_MAKE_VERSION'; s/@<:@^\d\.@:>@.*//g; exit (\$_ < 3.81);"; then
	new_make=yes
fi
AC_MSG_RESULT($new_make)
automatic_dependencies=no
AX_CFLAGS_GCC_OPTION([-M -MT conftest.d -MF conftest.o], [], [ automatic_dependencies=$new_make ], [])
AC_MSG_CHECKING([Whether to use automatic dependencies])
AC_ARG_ENABLE(automatic-dependencies,
[  --enable-automatic-dependencies Enable automatic dependencies],
[ automatic_dependencies=$enableval ], 
[ automatic_dependencies=no ])
AC_MSG_RESULT($automatic_dependencies)
AC_SUBST(automatic_dependencies)

m4_include(build/m4/check_doc.m4)

m4_include(../m4/check_python.m4)

AC_SAMBA_PYTHON_DEVEL([
SMB_EXT_LIB(EXT_LIB_PYTHON, [$PYTHON_LDFLAGS], [$PYTHON_CFLAGS])
SMB_ENABLE(EXT_LIB_PYTHON,YES)
SMB_ENABLE(LIBPYTHON,YES)
],[
AC_MSG_ERROR([Python not found. Please install Python 2.x and its development headers/libraries.])
])

AC_MSG_CHECKING(python library directory)
pythondir=`$PYTHON -c "from distutils import sysconfig; print sysconfig.get_python_lib(1, 0, '\\${prefix}')"`
AC_MSG_RESULT($pythondir)

AC_SUBST(pythondir)
