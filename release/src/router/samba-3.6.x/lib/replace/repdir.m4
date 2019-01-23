AC_CACHE_CHECK([for broken readdir],libreplace_cv_READDIR_NEEDED,[
	AC_TRY_RUN([
#define test_readdir_os2_delete main
#include "$libreplacedir/test/os2_delete.c"],
	[libreplace_cv_READDIR_NEEDED=no],
	[libreplace_cv_READDIR_NEEDED=yes],
	[libreplace_cv_READDIR_NEEDED="assuming not"])
])

AC_CHECK_FUNCS(dirfd)
AC_HAVE_DECL(dirfd, [#include <dirent.h>])

#
# try to replace with getdirentries() if needed
#
if test x"$libreplace_cv_READDIR_NEEDED" = x"yes"; then
AC_CHECK_FUNCS(getdirentries)
AC_VERIFY_C_PROTOTYPE([long telldir(const DIR *dir)],
	[
	return 0;
	],[
	AC_DEFINE(TELLDIR_TAKES_CONST_DIR, 1, [Whether telldir takes a const pointer])
	],[],[
	#include <dirent.h>
	])

AC_VERIFY_C_PROTOTYPE([int seekdir(DIR *dir, long ofs)],
	[
	return 0;
	],[
	AC_DEFINE(SEEKDIR_RETURNS_INT, 1, [Whether seekdir returns an int])
	],[],[
	#include <dirent.h>
	])
AC_CACHE_CHECK([for replacing readdir using getdirentries()],libreplace_cv_READDIR_GETDIRENTRIES,[
	AC_TRY_RUN([
#define _LIBREPLACE_REPLACE_H
#include "$libreplacedir/repdir_getdirentries.c"
#define test_readdir_os2_delete main
#include "$libreplacedir/test/os2_delete.c"],
	[libreplace_cv_READDIR_GETDIRENTRIES=yes],
	[libreplace_cv_READDIR_GETDIRENTRIES=no])
])
fi
if test x"$libreplace_cv_READDIR_GETDIRENTRIES" = x"yes"; then
	AC_DEFINE(REPLACE_READDIR,1,[replace readdir])
	AC_DEFINE(REPLACE_READDIR_GETDIRENTRIES,1,[replace readdir using getdirentries()])
	LIBREPLACEOBJ="${LIBREPLACEOBJ} $libreplacedir/repdir_getdirentries.o"
	libreplace_cv_READDIR_NEEDED=no
fi

#
# try to replace with getdents() if needed
#
if test x"$libreplace_cv_READDIR_NEEDED" = x"yes"; then
AC_CHECK_FUNCS(getdents)
AC_CACHE_CHECK([for replacing readdir using getdents()],libreplace_cv_READDIR_GETDENTS,[
	AC_TRY_RUN([
#define _LIBREPLACE_REPLACE_H
#error _donot_use_getdents_replacement_anymore
#include "$libreplacedir/repdir_getdents.c"
#define test_readdir_os2_delete main
#include "$libreplacedir/test/os2_delete.c"],
	[libreplace_cv_READDIR_GETDENTS=yes],
	[libreplace_cv_READDIR_GETDENTS=no])
])
fi
if test x"$libreplace_cv_READDIR_GETDENTS" = x"yes"; then
	AC_DEFINE(REPLACE_READDIR,1,[replace readdir])
	AC_DEFINE(REPLACE_READDIR_GETDENTS,1,[replace readdir using getdents()])
	LIBREPLACEOBJ="${LIBREPLACEOBJ} $libreplacedir/repdir_getdents.o"
	libreplace_cv_READDIR_NEEDED=no
fi

AC_MSG_CHECKING([a usable readdir()])
if test x"$libreplace_cv_READDIR_NEEDED" = x"yes"; then
	AC_MSG_RESULT(no)
	AC_MSG_WARN([the provided readdir() is broken])
else
	AC_MSG_RESULT(yes)
fi
