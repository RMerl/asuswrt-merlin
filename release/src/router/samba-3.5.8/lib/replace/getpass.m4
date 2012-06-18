AC_CHECK_FUNC(getpass, libreplace_cv_HAVE_GETPASS=yes)
AC_CHECK_FUNC(getpassphrase, libreplace_cv_HAVE_GETPASSPHRASE=yes)
if test x"$libreplace_cv_HAVE_GETPASS" = x"yes" -a x"$libreplace_cv_HAVE_GETPASSPHRASE" = x"yes"; then
        AC_DEFINE(REPLACE_GETPASS_BY_GETPASSPHRASE, 1, [getpass returns <9 chars where getpassphrase returns <265 chars])
	AC_DEFINE(REPLACE_GETPASS,1,[Whether getpass should be replaced])
	LIBREPLACEOBJ="${LIBREPLACEOBJ} $libreplacedir/getpass.o"
else

AC_CACHE_CHECK([whether getpass should be replaced],libreplace_cv_REPLACE_GETPASS,[
SAVE_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS -I$libreplacedir/"
AC_TRY_COMPILE([
#include "confdefs.h"
#define NO_CONFIG_H
#include "$libreplacedir/getpass.c"
],[],libreplace_cv_REPLACE_GETPASS=yes,libreplace_cv_REPLACE_GETPASS=no)
CPPFLAGS="$SAVE_CPPFLAGS"
])
if test x"$libreplace_cv_REPLACE_GETPASS" = x"yes"; then
	AC_DEFINE(REPLACE_GETPASS,1,[Whether getpass should be replaced])
	LIBREPLACEOBJ="${LIBREPLACEOBJ} $libreplacedir/getpass.o"
fi

fi
