AC_PREREQ(2.50)
AC_DEFUN([AC_CHECK_LIB_EXT], [
	AC_CHECK_LIB([$1],[$3],[$4],[$5],[$7])
	ac_cv_lib_ext_$1_$3=$ac_cv_lib_$1_$3
])
AC_DEFUN([AC_CHECK_FUNC_EXT], [
	AC_CHECK_FUNC([$1],[$3],[$4])
	ac_cv_func_ext_$1=$ac_cv_func_$1
])
AC_DEFUN([SMB_MODULE_DEFAULT], [echo -n ""])
AC_DEFUN([SMB_LIBRARY_ENABLE], [echo -n ""])
AC_DEFUN([SMB_EXT_LIB], [echo -n ""])
AC_DEFUN([SMB_ENABLE], [echo -n ""])
AC_INIT(ldb, 0.9.7)
AC_CONFIG_SRCDIR([common/ldb.c])

AC_LIBREPLACE_ALL_CHECKS

if test "$ac_cv_prog_gcc" = yes; then
   CFLAGS="$CFLAGS -Wall -Wshadow -Wstrict-prototypes -Wpointer-arith -Wcast-qual -Wcast-align -Wwrite-strings"
fi

WITH_GCOV=0
AC_ARG_ENABLE(gcov,
	AS_HELP_STRING([--enable-gcov],[enable GCOV code coverage tests]),
	[ WITH_GCOV=1])
AC_SUBST(WITH_GCOV)
if test x"$with_gcov_support" = x"yes"; then
   CFLAGS="$CFLAGS -ftest-coverage -fprofile-arcs"
   LIBS="$LIBS -lgcov"
fi

AC_PATH_PROG(XSLTPROC,xsltproc)
AC_PATH_PROG(DOXYGEN,doxygen)
AC_PATH_PROG(GCOV,gcov)
AC_PATH_PROG(SLAPD,slapd)
AC_CHECK_HEADERS(stdint.h dlfcn.h)
AC_CONFIG_HEADER(include/config.h)

LDB_MODULESDIR="${libdir}/ldb"
AC_SUBST(LDB_MODULESDIR)

TESTS=""
EXTRA_OBJ=""

m4_include(build_macros.m4)
BUILD_WITH_SHARED_BUILD_DIR

m4_include(pkg.m4)
m4_include(libpopt.m4)
m4_include(libtalloc.m4)
m4_include(libtdb.m4)
m4_include(libevents.m4)

m4_include(ldap.m4)
if test x"$with_ldap_support" = x"yes"; then
   CFLAGS="$CFLAGS -DHAVE_LDB_LDAP=1"
   EXTRA_OBJ="$EXTRA_OBJ ldb_ldap/ldb_ldap.o"
   LDAP_LIBS="-llber -lldap"
   AC_SUBST(LDAP_LIBS)
   TESTS="$TESTS test-ldap.sh"
fi

m4_include(sqlite3.m4)
if test x"$with_sqlite3_support" = x"yes"; then
   LIBS="$LIBS -lsqlite3"
   CFLAGS="$CFLAGS -DHAVE_LDB_SQLITE3=1"
   EXTRA_OBJ="$EXTRA_OBJ ldb_sqlite3/ldb_sqlite3.o"
   TESTS="$TESTS test-sqlite3.sh"
fi

AC_SUBST(TESTS)
AC_SUBST(EXTRA_OBJ)

AC_LD_EXPORT_DYNAMIC
AC_LD_PICFLAG
AC_LD_SHLIBEXT
AC_LD_SONAMEFLAG
AC_LIBREPLACE_SHLD
AC_LIBREPLACE_SHLD_FLAGS
AC_LIBREPLACE_MDLD
AC_LIBREPLACE_MDLD_FLAGS
AC_LIBREPLACE_RUNTIME_LIB_PATH_VAR

AC_PATH_PROGS([PYTHON_CONFIG], [python2.6-config python2.5-config python2.4-config python-config])
AC_PATH_PROGS([PYTHON], [python2.6 python2.5 python2.4 python])

PYTHON_BUILD_TARGET="build-python"
PYTHON_INSTALL_TARGET="install-python"
PYTHON_CHECK_TARGET="check-python"
AC_SUBST(PYTHON_BUILD_TARGET)
AC_SUBST(PYTHON_INSTALL_TARGET)
AC_SUBST(PYTHON_CHECK_TARGET)

if test -z "$PYTHON_CONFIG"; then
	PYTHON_BUILD_TARGET=""	
	PYTHON_CHECK_TARGET=""
	PYTHON_INSTALL_TARGET=""	
fi

m4_include(libldb.m4)
AC_OUTPUT(Makefile ldb.pc)
