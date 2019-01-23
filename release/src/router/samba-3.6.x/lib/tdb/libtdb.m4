dnl find the tdb sources. This is meant to work both for 
dnl tdb standalone builds, and builds of packages using tdb
tdbdir=""
tdbpaths=". lib/tdb tdb ../tdb ../lib/tdb"
for d in $tdbpaths; do
	if test -f "$srcdir/$d/common/tdb.c"; then
		tdbdir="$d"		
		AC_SUBST(tdbdir)
		break;
	fi
done
if test x"$tdbdir" = "x"; then
   AC_MSG_ERROR([cannot find tdb source in $tdbpaths])
fi
TDB_OBJ="common/tdb.o common/dump.o common/transaction.o common/error.o common/traverse.o"
TDB_OBJ="$TDB_OBJ common/freelist.o common/freelistcheck.o common/io.o common/lock.o common/open.o common/check.o common/hash.o common/summary.o"
AC_SUBST(TDB_OBJ)
AC_SUBST(LIBREPLACEOBJ)

TDB_LIBS=""
AC_SUBST(TDB_LIBS)

TDB_DEPS=""
if test x$libreplace_cv_HAVE_FDATASYNC_IN_LIBRT = xyes ; then
	TDB_DEPS="$TDB_DEPS -lrt"
fi
AC_SUBST(TDB_DEPS)

TDB_CFLAGS="-I$tdbdir/include"
AC_SUBST(TDB_CFLAGS)

AC_CHECK_FUNCS(mmap pread pwrite getpagesize utime)
AC_CHECK_HEADERS(getopt.h sys/select.h sys/time.h)

AC_HAVE_DECL(pread, [#include <unistd.h>])
AC_HAVE_DECL(pwrite, [#include <unistd.h>])

if test x"$VERSIONSCRIPT" != "x"; then
    EXPORTSFILE=tdb.exports
    AC_SUBST(EXPORTSFILE)
fi
