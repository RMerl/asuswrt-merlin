AC_CACHE_CHECK([whether getpass should be replaced],samba_cv_REPLACE_GETPASS,[
SAVE_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS -I$libreplacedir/"
AC_TRY_COMPILE([
#include "confdefs.h"
#define _LIBREPLACE_REPLACE_H
#define REPLACE_GETPASS 1
#define main dont_declare_main
#include "$libreplacedir/getpass.c"
#undef main
],[],samba_cv_REPLACE_GETPASS=yes,samba_cv_REPLACE_GETPASS=no)
CPPFLAGS="$SAVE_CPPFLAGS"
])
if test x"$samba_cv_REPLACE_GETPASS" = x"yes"; then
	AC_DEFINE(REPLACE_GETPASS,1,[Whether getpass should be replaced])
	LIBREPLACEOBJ="${LIBREPLACEOBJ} getpass.o"
fi
